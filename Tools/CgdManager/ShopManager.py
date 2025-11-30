from functools import partial
from PySide6.QtWidgets import (
    QWidget, QLabel, QHBoxLayout, QVBoxLayout, QScrollArea, QComboBox,
    QTreeWidget, QTreeWidgetItem, QPushButton, QDialog, QMessageBox
)
from PySide6.QtCore import Qt
from Utils import getFieldValue, defaultPixmap, loadPixmap, showToast
from ModifyShopItemDialog import ModifyItemDialog
from AddNewVendorItemDialog import AddNewVendorItemDialog

class ShopManager(QWidget):
    CHARACTER_FLAGS = {
        "Naomi": "ii_class_a",
        "Kai": "ii_class_b",
        "Pandora": "ii_class_c",
        "CHIP": "ii_class_d",
        "Knox": "ii_class_e",
        "Simon": "ii_class_f",
        "Amelia": "ii_class_g",
        "Sharkill": "ii_class_h",
        "Sophitia": "ii_class_i"
    }

    CATEGORY_MAP = {
        "Weapons": {"Package": 0, "Melee": 1, "Rifle": 2, "Shotgun": 3, "Sniper": 4, "MicroGun": 5, "Bazooka": 6, "Grenade": 7},
        "Set": {"Package": 8, "Original": 9},
        "Parts": {"Package": 10, "Hair": 11, "Face": 12, "Top": 13, "Bottom": 14, "Legs": 15, "Hands": 16, "Shoes": 17},
        "Accessories": {"Package": 18, "Head": 19, "Back": 20, "Waist": 21},
        "Items": {"Package": 22, "Growth": 23, "Convenience": 24, "Diorama": 25, "For Clan": 26}
    }

    CURRENCY_PRIORITY = ["RT", "Coupon", "MP"]

    def __init__(self, cgdManager, capsule_icon_path, item_icon_path):
        super().__init__()
        self.cgdManager = cgdManager
        self.iconFolder = capsule_icon_path
        self.item_icon_path = item_icon_path

        self.item_lookup = {}
        self.icon_lookup = {}
        self.vendor_lookup = {}
        self.pixmap_cache = {}
        self.buildLookups()

        self.currentMainSelection = None
        self.currentSubSelection = None
        self.relevant_items = []

        self.setWindowTitle("Shop Manager - ToyBattlesHQ")
        self.resize(1000, 600)

        main_layout = QVBoxLayout(self)

        self.categoryDropdown = QComboBox()
        self.categoryDropdown.addItems(list(self.CHARACTER_FLAGS.keys()) + ["Weapons"])
        self.categoryDropdown.currentTextChanged.connect(self.onMainCategoryChanged)
        main_layout.addWidget(self.categoryDropdown)

        self.currencyFilterDropdown = QComboBox()
        self.currencyFilterDropdown.addItems(["All", "RT (Yellow)", "Coupon (Violet)", "MP (Blue)"])
        self.currencyFilterDropdown.currentTextChanged.connect(self.refreshItemDisplay)
        main_layout.addWidget(self.currencyFilterDropdown)

        self.addItemBtn = QPushButton("Add Item")
        self.addItemBtn.clicked.connect(self.openAddItemDialog)
        main_layout.addWidget(self.addItemBtn)
        
        self.deleteItemBtn = QPushButton("Re-sort")
        self.deleteItemBtn.clicked.connect(self.resortVendorInfo)
        main_layout.addWidget(self.deleteItemBtn)


        split_layout = QHBoxLayout()
        main_layout.addLayout(split_layout)

        self.leftTree = QTreeWidget()
        self.leftTree.setHeaderHidden(True)
        self.leftTree.itemClicked.connect(self.onLeftTreeItemSelected)
        split_layout.addWidget(self.leftTree, 1)

        self.itemArea = QScrollArea()
        self.itemArea.setWidgetResizable(True)
        self.itemWidget = QWidget()
        self.itemLayout = QVBoxLayout(self.itemWidget)
        self.itemLayout.setAlignment(Qt.AlignTop)
        self.itemArea.setWidget(self.itemWidget)
        split_layout.addWidget(self.itemArea, 2)

        self.onMainCategoryChanged(self.categoryDropdown.currentText())

    def buildLookups(self):
        for cdb_name, cdb in self.cgdManager.cdbs.items():
            lower = cdb_name.lower()
            if "iteminfo" in lower or "itemweaponsinfo" in lower:
                key = "ii_id"
                for idx, entry in enumerate(cdb.entries):
                    entry_id = getFieldValue(entry, key)
                    if entry_id is not None:
                        self.item_lookup[entry_id] = (entry, cdb.fileName, idx)
            elif "vendorinfo" in lower:
                for idx, entry in enumerate(cdb.entries):
                    vi_id = getFieldValue(entry, "vi_id")
                    if vi_id is not None:
                        self.vendor_lookup[vi_id] = (entry, cdb.fileName, idx)
            elif "iconsinfo" in lower:
                for idx, entry in enumerate(cdb.entries):
                    ii_id = getFieldValue(entry, "ii_id")
                    if ii_id is not None:
                        self.icon_lookup[ii_id] = entry

    def onMainCategoryChanged(self, text):
        self.currentMainSelection = text
        self.leftTree.clear()
        if text == "Weapons":
            for name in self.CATEGORY_MAP["Weapons"].keys():
                QTreeWidgetItem(self.leftTree, [name])
        else:
            for main_cat in ["Set", "Parts", "Accessories", "Items"]:
                parent = QTreeWidgetItem(self.leftTree, [main_cat])
                for sub_name in self.CATEGORY_MAP[main_cat].keys():
                    QTreeWidgetItem(parent, [sub_name])
            self.leftTree.expandAll()

    def onLeftTreeItemSelected(self, item):
        self.currentSubSelection = item.text(0)
        while self.itemLayout.count():
            child = self.itemLayout.takeAt(0)
            widget = child.widget()
            if widget:
                widget.deleteLater()

        if not self.currentMainSelection or not self.currentSubSelection:
            return

        parent = item.parent()
        parent_name = parent.text(0) if parent else self.currentSubSelection
        char_flag = self.CHARACTER_FLAGS.get(self.currentMainSelection)
        cat_id = (self.CATEGORY_MAP["Weapons"].get(self.currentSubSelection)
                  if self.currentMainSelection == "Weapons"
                  else self.CATEGORY_MAP.get(parent_name, {}).get(self.currentSubSelection))
        if cat_id is None:
            return

        self.relevant_items = []
        for v_meta in self.vendor_lookup.values():
            v_entry = v_meta[0]
            vi_id = getFieldValue(v_entry, "vi_id")
            item_meta = self.item_lookup.get(vi_id)
            if item_meta is None:
                continue
            item_entry = item_meta[0]
            if char_flag and not getFieldValue(item_entry, char_flag):
                continue
            if getFieldValue(v_entry, "vi_category") != cat_id:
                continue
            self.relevant_items.append((item_entry, vi_id))

        self.refreshItemDisplay()

    def getItemCurrencyType(self, item_entry):
        cash = getFieldValue(item_entry, "ii_buy_cash", 0)
        coupon = getFieldValue(item_entry, "ii_buy_coupon", 0)
        point = getFieldValue(item_entry, "ii_buy_point", 0)

        if cash and cash != 0:
            return "RT"
        elif coupon and coupon != 0:
            return "Coupon"
        elif point and point != 0:
            return "MP"
        return "None"

    def refreshItemDisplay(self, *_):
        while self.itemLayout.count():
            child = self.itemLayout.takeAt(0)
            widget = child.widget()
            if widget:
                widget.deleteLater()

        filter_text = self.currencyFilterDropdown.currentText()
        filter_map = {"All": None, "RT (Yellow)": "RT", "Coupon (Violet)": "Coupon", "MP (Blue)": "MP"}
        current_filter = filter_map.get(filter_text)

        sorted_items = sorted(
            self.relevant_items,
            key=lambda x: self.CURRENCY_PRIORITY.index(self.getItemCurrencyType(x[0])) if self.getItemCurrencyType(x[0]) in self.CURRENCY_PRIORITY else 99
        )

        for item_entry, item_id in sorted_items:
            currency_type = self.getItemCurrencyType(item_entry)
            if current_filter and currency_type != current_filter:
                continue
            self.displayShopItem(item_entry, item_id)

    def displayShopItem(self, item_entry, item_id):
        container = QWidget()
        layout = QHBoxLayout(container)
        layout.setContentsMargins(5, 5, 5, 5)

        currency_type = self.getItemCurrencyType(item_entry)
        if currency_type == "RT":
            bg_color = "rgb(255, 255, 150)"
        elif currency_type == "Coupon":
            bg_color = "rgb(180, 120, 220)"
        elif currency_type == "MP":
            bg_color = "rgb(120, 200, 255)"
        else:
            bg_color = "rgb(240, 240, 240)"
        container.setStyleSheet(f"background-color: {bg_color};")

        icon_lbl = QLabel()
        icon_lbl.setObjectName("icon")
        icon_lbl.setFixedSize(64, 64)
        layout.addWidget(icon_lbl)

        name = getFieldValue(item_entry, "ii_name")
        text_lbl = QLabel(f"{name} (ID: {item_id})")
        layout.addWidget(text_lbl)

        modify_btn = QPushButton("MODIFY")
        modify_btn.setObjectName("modify")
        layout.addWidget(modify_btn)

        delete_btn = QPushButton("DELETE")
        delete_btn.setObjectName("delete")
        layout.addWidget(delete_btn)

        self.itemLayout.addWidget(container)

        if item_id in self.pixmap_cache:
            pixmap = self.pixmap_cache[item_id]
        else:
            pixmap = defaultPixmap()
            iconsmall_id = getFieldValue(item_entry, "ii_iconsmall")
            if icon_entry_raw := self.icon_lookup.get(iconsmall_id):
                icon_entry = {
                    "filename": getFieldValue(icon_entry_raw, "ii_filename"),
                    "offset": getFieldValue(icon_entry_raw, "ii_offset"),
                    "width": getFieldValue(icon_entry_raw, "ii_width"),
                    "height": getFieldValue(icon_entry_raw, "ii_height"),
                }
                pixmap = loadPixmap(icon_entry, self.item_icon_path)
                self.pixmap_cache[item_id] = pixmap
        icon_lbl.setPixmap(pixmap.scaled(64, 64, Qt.KeepAspectRatio))

        vendor_meta = self.vendor_lookup.get(item_id)
        vendor_entry = vendor_meta[0] if vendor_meta else None
        modify_btn.clicked.connect(partial(self.openModifyDialog, item_id))
        delete_btn.clicked.connect(partial(self.deleteVendorItem, item_id))


    def refreshRelevantItems(self):
        if not self.currentMainSelection or not self.currentSubSelection:
            return
            
        parent_name = None
        for i in range(self.leftTree.topLevelItemCount()):
            top_item = self.leftTree.topLevelItem(i)
            for j in range(top_item.childCount()):
                child = top_item.child(j)
                if child.text(0) == self.currentSubSelection:
                    parent_name = top_item.text(0)
                    break
            if parent_name:
                break
        
        char_flag = self.CHARACTER_FLAGS.get(self.currentMainSelection)
        cat_id = (self.CATEGORY_MAP["Weapons"].get(self.currentSubSelection)
                if self.currentMainSelection == "Weapons"
                else self.CATEGORY_MAP.get(parent_name, {}).get(self.currentSubSelection))
        if cat_id is None:
            return

        self.relevant_items = []
        for v_meta in self.vendor_lookup.values():
            v_entry = v_meta[0]
            vi_id = getFieldValue(v_entry, "vi_id")
            item_meta = self.item_lookup.get(vi_id)
            if item_meta is None:
                continue
            item_entry = item_meta[0]
            if char_flag and not getFieldValue(item_entry, char_flag):
                continue
            if getFieldValue(v_entry, "vi_category") != cat_id:
                continue
            self.relevant_items.append((item_entry, vi_id))
        
        self.refreshItemDisplay()
    
    def openModifyDialog(self, item_id):
        vendor_meta = self.vendor_lookup.get(item_id)
        if not vendor_meta:
            QMessageBox.critical(self, "Error", "Vendor entry not found for this item.")
            return
        vendor_entry, vendor_cdb_name, vendor_entry_index = vendor_meta[0], vendor_meta[1], vendor_meta[2]
        
        original_vi_id = item_id
        
        dlg = ModifyItemDialog(self, self.cgdManager, vendor_cdb_name, vendor_entry_index, self.item_lookup, self.icon_lookup, self.item_icon_path, self.vendor_lookup)
        if dlg.exec() == QDialog.Accepted:
            updated_vendor_entry = self.cgdManager.cdbs[vendor_cdb_name].entries[vendor_entry_index]
            new_vi_id = getFieldValue(updated_vendor_entry, "vi_id")
            
            if new_vi_id != original_vi_id and original_vi_id in self.vendor_lookup:
                del self.vendor_lookup[original_vi_id]
            
            self.buildLookups()
            self.refreshRelevantItems()
            
            showToast(self, "Changes saved in CDB")

    def openAddItemDialog(self):
        dlg = AddNewVendorItemDialog(self, self)
        if dlg.exec() == QDialog.Accepted:
            self.buildLookups()
            self.refreshRelevantItems()

    def deleteVendorItem(self, item_id):
        vendor_meta = self.vendor_lookup.get(item_id)
        if not vendor_meta:
            QMessageBox.critical(self, "Error", f"No vendor entry found for item ID {item_id}.")
            return

        v_entry, cdbFileName, entryNumber = vendor_meta

        reply = QMessageBox.question(
            self,
            "Confirm Delete",
            f"Are you sure you want to delete vendor entry for item ID {item_id}?",
            QMessageBox.Yes | QMessageBox.No
        )

        if reply != QMessageBox.Yes:
            return

        result = self.cgdManager.removeEntry(cdbFileName, entryNumber)
        if not result.get("success"):
            QMessageBox.critical(self, "Error", result.get("error", "Unknown error"))
            return

        if item_id in self.vendor_lookup:
            del self.vendor_lookup[item_id]

        self.refreshRelevantItems()
        showToast(self, "Vendor entry removed.")


    def resortVendorInfo(self):
        vendor_cdb = self.cgdManager.cdbs["vendorinfo"]
        entries = vendor_cdb.entries

        def detect_type(item_id):
            meta = self.item_lookup.get(item_id)
            if not meta:
                print(f"[SORT] Item {item_id} not found in item_lookup → treat as coupon.")
                return "coupon"

            entry = meta[0] if isinstance(meta, tuple) else meta
            if not entry:
                print(f"[SORT] Item entry missing for {item_id} → treat as coupon.")
                return "coupon"

            try:
                cash = int(getFieldValue(entry, "ii_buy_cash") or 0)
            except Exception:
                cash = 0
            try:
                point = int(getFieldValue(entry, "ii_buy_point") or 0)
            except Exception:
                point = 0
            try:
                coupon = int(getFieldValue(entry, "ii_buy_coupon") or 0)
            except Exception:
                coupon = 0

            if cash > 0:
                return "rt"
            if point > 0:
                return "mp"
            if coupon > 0:
                return "coupon"

            return "coupon"

        rt_list = []
        mp_list = []
        coupon_list = []

        for idx, entry in enumerate(entries):
            raw_vi = getFieldValue(entry, "vi_list_01")
            if not raw_vi:
                continue

            try:
                vi_id = int(raw_vi)
            except Exception:
                print(f"[SORT] Skipping entry at entries[{idx}] because vi_list_01 is not integer: {raw_vi!r}")
                continue

            t = detect_type(vi_id)

            if t == "rt":
                rt_list.append(entry)
            elif t == "mp":
                mp_list.append(entry)
            else:
                coupon_list.append(entry)

        print(f"[SORT] Counts → RT: {len(rt_list)}, MP: {len(mp_list)}, Coupon: {len(coupon_list)}")

        sorted_entries = rt_list + mp_list + coupon_list

        for new_index, entry in enumerate(sorted_entries, start=1):
            original_idx = None
            try:
                original_idx = next(i for i, e in enumerate(entries) if e is entry)
            except StopIteration:
                try:
                    original_idx = entries.index(entry)
                except ValueError:
                    original_idx = None

            if original_idx is None:
                print(f"[SORT][WARN] Could not find numeric index for entry (vi_list_01={getFieldValue(entry,'vi_list_01')}). Skipping update for this entry.")
                continue

            print(f"[SORT] Updating vendor entry numeric index={original_idx} -> vi_array_* = {new_index}")

            vendor_cdb.updateValue(original_idx, "vi_array_none", new_index)
            vendor_cdb.updateValue(original_idx, "vi_array_new", new_index)
            vendor_cdb.updateValue(original_idx, "vi_array_hit", new_index)

        try:
            self.cgdManager.saveCdbOutputs(vendor_cdb)
        except Exception as e:
            print(f"[SORT][ERROR] Failed to save vendorinfo CDB: {e}")
            QMessageBox.critical(self, "Save Error", f"Failed to save vendorinfo CDB:\n{e}")
            return
        
        showToast("VendorInfo resorted successfully")
        print("[SORT] VendorInfo resorted by RT > MP > Coupon priority.")
