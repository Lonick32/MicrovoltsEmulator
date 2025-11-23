from functools import partial
from PySide6.QtWidgets import (
    QWidget, QLabel, QHBoxLayout, QVBoxLayout, QScrollArea, QComboBox, QApplication,
    QTreeWidget, QTreeWidgetItem, QPushButton, QDialog, QFormLayout, QLineEdit,
    QDialogButtonBox, QMessageBox
)
from PySide6.QtCore import Qt
from Utils import getFieldValue, defaultPixmap, loadPixmap, showToast

class ModifyItemDialog(QDialog):
    def __init__(self, parent, vendor_entry, item_lookup, icon_lookup, item_icon_path):
        super().__init__(parent)
        self.vendor_entry = vendor_entry
        self.item_lookup = item_lookup
        self.icon_lookup = icon_lookup
        self.item_icon_path = item_icon_path
        self.setWindowTitle("Modify Item Durations & Prices")
        self.resize(520, 320)

        self.duration_map = [] 
        self._build_duration_map()

        self.layout = QVBoxLayout(self)

        self.durationDropdown = QComboBox()
        for label, fk, iid in self.duration_map:
            self.durationDropdown.addItem(label, (fk, iid))
        self.durationDropdown.currentIndexChanged.connect(self.onDurationChanged)
        self.layout.addWidget(self.durationDropdown)

        self.form = QFormLayout()
        self.input_cash = QLineEdit()
        self.input_coupon = QLineEdit()
        self.input_point = QLineEdit()
        self.form.addRow("RT (ii_buy_cash):", self.input_cash)
        self.form.addRow("Coupon (ii_buy_coupon):", self.input_coupon)
        self.form.addRow("MP (ii_buy_point):", self.input_point)
        self.layout.addLayout(self.form)

        self.buttons = QDialogButtonBox(QDialogButtonBox.Save | QDialogButtonBox.Cancel)
        self.buttons.accepted.connect(self.onSave)
        self.buttons.rejected.connect(self.reject)
        self.layout.addWidget(self.buttons)

        if self.duration_map:
            self.onDurationChanged(0)
        else:
            QMessageBox.information(self, "No durations", "This vendor entry has no duration variants to modify.")
            self.buttons.button(QDialogButtonBox.Save).setEnabled(False)

    def _build_duration_map(self):
        for num in range(1, 5):
            base = f"vi_list_{num:02d}"
            for suffix, label_suffix in [("", ""), ("_a", " (Speed)"), ("_b", " (Tank)")]:
                key = base + suffix
                val = getFieldValue(self.vendor_entry, key)
                if val is None:
                    continue
                if val == 0:
                    continue
                item_entry = self.item_lookup.get(val)
                time_label = ""
                if item_entry:
                    time_label = getFieldValue(item_entry, "ii_name_time") or ""
                display_label = f"{num} - {time_label}{label_suffix} (ItemID: {val})"
                self.duration_map.append((display_label, key, val))

    def onDurationChanged(self, idx):
        if idx < 0 or idx >= len(self.duration_map):
            return
        _, _, item_id = self.duration_map[idx]
        item_entry = self.item_lookup.get(item_id)
        if not item_entry:
            self.input_cash.setText("")
            self.input_coupon.setText("")
            self.input_point.setText("")
            return
        cash = getFieldValue(item_entry, "ii_buy_cash", 0) or 0
        coupon = getFieldValue(item_entry, "ii_buy_coupon", 0) or 0
        point = getFieldValue(item_entry, "ii_buy_point", 0) or 0
        self.input_cash.setText(str(cash))
        self.input_coupon.setText(str(coupon))
        self.input_point.setText(str(point))

    def _setFieldValue(self, entry, key, newval):
        for f in entry:
            if f.key == key:
                f.value = newval
                return True
        return False

    def onSave(self):
        idx = self.durationDropdown.currentIndex()
        if idx < 0:
            return
        _, _, item_id = self.duration_map[idx]
        item_entry = self.item_lookup.get(item_id)
        if not item_entry:
            QMessageBox.critical(self, "Error", "Target item entry not found in memory.")
            return

        try:
            new_cash = int(self.input_cash.text().strip() or 0)
            new_coupon = int(self.input_coupon.text().strip() or 0)
            new_point = int(self.input_point.text().strip() or 0)
        except ValueError:
            QMessageBox.warning(self, "Input error", "Please enter integer values for prices.")
            return

        ok_cash = self._setFieldValue(item_entry, "ii_buy_cash", new_cash)
        ok_coupon = self._setFieldValue(item_entry, "ii_buy_coupon", new_coupon)
        ok_point = self._setFieldValue(item_entry, "ii_buy_point", new_point)

        if not (ok_cash or ok_coupon or ok_point):
            QMessageBox.warning(self, "Warning", "No price fields were found to update in the item entry.")
        else:
            showToast(self, "Prices updated in memory.")
        self.accept()


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
                for entry in cdb.entries:
                    entry_id = getFieldValue(entry, key)
                    if entry_id is not None:
                        self.item_lookup[entry_id] = entry
            elif "vendorinfo" in lower:
                for entry in cdb.entries:
                    vi_id = getFieldValue(entry, "vi_id")
                    if vi_id is not None:
                        self.vendor_lookup[vi_id] = entry
            elif "iconsinfo" in lower:
                for entry in cdb.entries:
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
        for v in self.vendor_lookup.values():
            vi_id = getFieldValue(v, "vi_id")
            item_entry = self.item_lookup.get(vi_id)
            if item_entry is None:
                continue
            if char_flag and not getFieldValue(item_entry, char_flag):
                continue
            if getFieldValue(v, "vi_category") != cat_id:
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

        vendor_entry = self.vendor_lookup.get(item_id)
        modify_btn.clicked.connect(partial(self.openModifyDialog, vendor_entry))

    def openModifyDialog(self, vendor_entry):
        if vendor_entry is None:
            QMessageBox.critical(self, "Error", "Vendor entry not found for this item.")
            return
        dlg = ModifyItemDialog(self, vendor_entry, self.item_lookup, self.icon_lookup, self.item_icon_path)
        if dlg.exec() == QDialog.Accepted:
            showToast(self, "Changes saved in memory. Call CGD save functions to persist.")
