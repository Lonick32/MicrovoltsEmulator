from functools import partial
from PySide6.QtWidgets import (
    QWidget, QListWidget, QListWidgetItem, QLabel, QMessageBox,
    QHBoxLayout, QVBoxLayout, QScrollArea, QPushButton, QDialog, QFormLayout,
    QLineEdit, QDialogButtonBox, QComboBox
)
from PySide6.QtCore import Qt
from CapsuleAddItem import AddItemDialog, AddFixedItems
from CdbParser import EntryDataType
from NewCapsuleDialog import CreateCapsuleDialog
import random
from copy import deepcopy
from Utils import showMessage, getFieldValue, defaultPixmap, loadPixmap, findEntryIndexById, showToast

class CapsuleManager(QWidget):
    def __init__(self, cgdManager, capsule_icon_path, item_icon_path):
        super().__init__()
        self.cgdManager = cgdManager
        self.iconFolder = capsule_icon_path
        self.item_icon_path = item_icon_path
        self.pixmap_cache = {} 
        self.item_lookup = {}  
        self.icon_lookup = {} 
        self.buildLookups()
        self.setWindowTitle("Capsule Manager - ToyBattlesHQ")
        self.resize(1000, 600)

        main_layout = QHBoxLayout(self)

        left_widget = QWidget()
        left_layout = QVBoxLayout(left_widget)
        left_layout.setContentsMargins(0, 0, 0, 0)
        self.capsuleList = QListWidget()
        #self.capsuleList.itemClicked.connect(self.showCapsuleItems)
        left_layout.addWidget(self.capsuleList)

        buttons_container = QVBoxLayout()

        buttons_layout = QHBoxLayout()
        self.newCapsuleButton = QPushButton("New Capsule")
        self.newCapsuleButton.clicked.connect(self.addNewCapsule)
        buttons_layout.addWidget(self.newCapsuleButton)
        self.addItemButton = QPushButton("Add Item")
        self.addItemButton.setEnabled(False)
        self.addItemButton.clicked.connect(self.addNewItem)
        buttons_layout.addWidget(self.addItemButton)
        self.deleteCapsuleButton = QPushButton("Delete Capsule")
        self.deleteCapsuleButton.setEnabled(False)
        self.deleteCapsuleButton.clicked.connect(self.deleteCapsule)
        buttons_layout.addWidget(self.deleteCapsuleButton)
        self.capsuleList.itemClicked.connect(self.onCapsuleSelected)
        self.updateCapsuleButton = QPushButton("Edit Capsule")
        buttons_layout.addWidget(self.updateCapsuleButton)
        self.updateCapsuleButton.setEnabled(False)
        self.updateCapsuleButton.clicked.connect(self.updateCapsule)
        buttons_layout.addStretch()

        buttons_layout_row2 = QHBoxLayout()
        self.addFixedItemsButton = QPushButton("Add 15 items")
        self.addFixedItemsButton.clicked.connect(self.addFixedItems)
        self.addFixedItemsButton.setEnabled(False)
        buttons_layout_row2.addWidget(self.addFixedItemsButton)

        buttons_container.addLayout(buttons_layout)
        buttons_container.addLayout(buttons_layout_row2)

        left_layout.addLayout(buttons_container)
        main_layout.addWidget(left_widget, 1)

        self.itemArea = QScrollArea()
        self.itemArea.setWidgetResizable(True)
        self.itemWidget = QWidget()
        self.itemLayout = QVBoxLayout(self.itemWidget)
        self.itemLayout.setAlignment(Qt.AlignTop)
        self.itemArea.setWidget(self.itemWidget)
        main_layout.addWidget(self.itemArea, 2)

        self.loadCapsules()

    def buildLookups(self):
        for cdb_name, cdb in self.cgdManager.cdbs.items():
            lower = cdb_name.lower()
            if "iteminfo" in lower or "itemweaponsinfo" in lower:
                for entry in cdb.entries:
                    self.item_lookup[getFieldValue(entry, "ii_id")] = entry
            elif "setiteminfo" in lower:
                for entry in cdb.entries:
                    self.item_lookup[getFieldValue(entry, "si_id")] = entry
            elif "iconsinfo" in lower:
                for entry in cdb.entries:
                    self.icon_lookup[getFieldValue(entry, "ii_id")] = entry

    def setEntryValueByType(self, entry, ts_index, value, type_size):
        if type_size in (2, 3, 4):
            entry[ts_index].value = value
        elif type_size == 1:
            entry[ts_index].value = bool(value)
        else:
            entry[ts_index].value = str(value)

    def loadCapsules(self, restore_index=None, restore_scroll=None):
        if restore_scroll is None:
            restore_scroll = self.capsuleList.verticalScrollBar().value()

        self.capsuleList.clear()

        for cdb_name, cdb in self.cgdManager.cdbs.items():
            if "gachaponinfo" in cdb_name.lower():
                for entry in cdb.entries:
                    gi_name = getFieldValue(entry, "gi_name")
                    gi_price = getFieldValue(entry, "gi_price")
                    gi_type = getFieldValue(entry, "gi_type")
                    typeStr = "Coins" if gi_type == 0 else "RockTokens" if gi_type == 1 else "MicroPoints"
                    item = QListWidgetItem(f"{gi_name} ({gi_price} {typeStr})")
                    item.setData(Qt.UserRole, entry)
                    self.capsuleList.addItem(item)

        if restore_index is not None:
            self.capsuleList.setCurrentRow(restore_index)

        self.capsuleList.verticalScrollBar().setValue(restore_scroll)

    def showCapsuleItems(self, capsule_item):
        entry = capsule_item.data(Qt.UserRole)
        gi_infoid = getFieldValue(entry, "gi_infoid")

        for i in reversed(range(self.itemLayout.count())): # otherwise previous items stack up with new ones
            widget = self.itemLayout.itemAt(i).widget()
            if widget:
                widget.setParent(None)

        items_to_display = []
        for cdb_name, cdb in self.cgdManager.cdbs.items():
            if "gachaponpackageinfo" in cdb_name.lower():
                for pkg_entry in cdb.entries:
                    if getFieldValue(pkg_entry, "gi_infoid") == gi_infoid:
                        item_id = getFieldValue(pkg_entry, "gi_itemid")
                        item_entry = self.item_lookup.get(item_id)
                        if item_entry:
                            item_name = getFieldValue(item_entry, "ii_name") or getFieldValue(item_entry, "si_name")
                            icon_id = getFieldValue(item_entry, "ii_iconsmall") or getFieldValue(item_entry, "si_iconsmall")
                            items_to_display.append((item_name, icon_id, pkg_entry, cdb_name, item_id))

        if items_to_display:
            self.displayCapsuleItems(items_to_display)

    def displayCapsuleItems(self, items_to_display):
        for item_name, icon_id, pkg_entry, cdb_name, item_id in items_to_display:
            container = QWidget()
            layout = QHBoxLayout(container)
            layout.setContentsMargins(5, 5, 5, 5)

            gi_type = getFieldValue(pkg_entry, "gi_type")
            container.setStyleSheet("background-color: rgb(255, 255, 200); color: black;" if gi_type == 1 else "")

            icon_lbl = QLabel()
            icon_lbl.setObjectName("icon")
            icon_lbl.setFixedSize(64, 64)
            icon_lbl.setProperty("icon_id", icon_id)
            layout.addWidget(icon_lbl)

            text_lbl = QLabel(f"{item_name} (ItemID: {item_id})")
            text_lbl.setObjectName("text")
            layout.addWidget(text_lbl)

            delete_btn = QPushButton("DELETE")
            delete_btn.setStyleSheet("background-color: rgb(220, 50, 50); color: white;")
            delete_btn.setObjectName("delete")
            layout.addWidget(delete_btn)

            update_btn = QPushButton("UPDATE")
            update_btn.setStyleSheet("background-color: rgb(120, 200, 120);")
            update_btn.setObjectName("update")
            layout.addWidget(update_btn)

            self.itemLayout.addWidget(container)

            if icon_id in self.pixmap_cache:
                pixmap = self.pixmap_cache[icon_id]
            else:
                pixmap = defaultPixmap()
                if icon_entry_raw := self.icon_lookup.get(icon_id):
                    icon_entry = {
                        "filename": getFieldValue(icon_entry_raw, "ii_filename"),
                        "offset": getFieldValue(icon_entry_raw, "ii_offset"),
                        "width": getFieldValue(icon_entry_raw, "ii_width"),
                        "height": getFieldValue(icon_entry_raw, "ii_height"),
                    }
                    pixmap = loadPixmap(icon_entry, self.item_icon_path)  
                    self.pixmap_cache[icon_id] = pixmap

            icon_lbl.setPixmap(pixmap.scaled(64, 64, Qt.KeepAspectRatio))

            delete_btn.clicked.connect(partial(self.deleteItem, pkg_entry, cdb_name, container))
            update_btn.clicked.connect(partial(self.updateItem, pkg_entry, cdb_name, text_lbl))
    
    def deleteItem(self, pkg_entry, cdb_name, widget):
        entry_num = findEntryIndexById(self.cgdManager, cdb_name, pkg_entry)
        if entry_num is None:
            QMessageBox.critical(self, "Error", "Entry not found in CDB.")
            return

        result = self.cgdManager.removeEntry(cdb_name, entry_num)
        if result.get("success"):
            showToast(self, "Item deleted successfully")
            widget.setParent(None)
        else:
            showMessage(QMessageBox.Critical,"Error Deleting Item",result.get("error", "Failed to delete the item"), self)

    def updateItem(self, pkg_entry, cdb_name, text_label):
        entry_num = findEntryIndexById(self.cgdManager, cdb_name, pkg_entry)
        if entry_num is None:
            showMessage(QMessageBox.Critical, "Error", "Entry not found in CDB.", self)
            return

        dialog = QDialog(self)
        dialog.setWindowTitle("Update Item")
        layout = QFormLayout(dialog)

        current_itemid = getFieldValue(pkg_entry, "gi_itemid")
        current_gi_type = getFieldValue(pkg_entry, "gi_type")

        input_box = QLineEdit()
        input_box.setText(str(current_itemid))
        layout.addRow("New ItemID:", input_box)

        type_input = QComboBox()
        type_input.addItems(["Normal (0)", "Rare (1)"])
        type_input.setCurrentIndex(current_gi_type if current_gi_type in (0, 1) else 0)
        layout.addRow("Item Type:", type_input)

        preview = QLabel()
        preview.setFixedSize(64, 64)
        preview.setPixmap(defaultPixmap().scaled(64, 64, Qt.KeepAspectRatio))
        layout.addRow("Preview:", preview)

        def refreshPreview():
            text = input_box.text().strip()
            if not text.isdigit():
                preview.setPixmap(defaultPixmap().scaled(64, 64, Qt.KeepAspectRatio))
                return

            item_id = int(text)
            item_entry = self.item_lookup.get(item_id)
            if not item_entry:
                preview.setPixmap(defaultPixmap().scaled(64, 64, Qt.KeepAspectRatio))
                return

            icon_id = getFieldValue(item_entry, "ii_iconsmall") or getFieldValue(item_entry, "si_iconsmall")
            if icon_id in self.pixmap_cache:
                pixmap = self.pixmap_cache[icon_id]
            else:
                icon_entry_raw = self.icon_lookup.get(icon_id)
                if not icon_entry_raw:
                    pixmap = defaultPixmap()
                else:
                    entry = {
                        "filename": getFieldValue(icon_entry_raw, "ii_filename"),
                        "offset": getFieldValue(icon_entry_raw, "ii_offset"),
                        "width": getFieldValue(icon_entry_raw, "ii_width"),
                        "height": getFieldValue(icon_entry_raw, "ii_height"),
                    }
                    try:
                        pixmap = loadPixmap(entry, self.item_icon_path)
                        if pixmap.isNull():
                            pixmap = defaultPixmap()
                    except Exception:
                        pixmap = defaultPixmap()
                self.pixmap_cache[icon_id] = pixmap

            preview.setPixmap(pixmap.scaled(64, 64, Qt.KeepAspectRatio))

        input_box.textChanged.connect(refreshPreview)
        refreshPreview()

        buttons = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        layout.addWidget(buttons)

        def acceptDialog():
            text = input_box.text().strip()
            if not text.isdigit():
                showMessage(QMessageBox.Critical, "Error", "Invalid ItemID value.", self)
                return

            item_id = int(text)
            if item_id not in self.item_lookup:
                showMessage(QMessageBox.Critical, "Error", f"ItemID {item_id} does not exist.", self)
                return

            dialog.accept()

        buttons.accepted.connect(acceptDialog)
        buttons.rejected.connect(dialog.reject)

        if dialog.exec() != QDialog.DialogCode.Accepted:
            return

        new_itemid = int(input_box.text().strip())
        new_gi_type = type_input.currentIndex()
        try:
            self.cgdManager.updateCdbEntry(cdb_name, entry_num, "gi_itemid", new_itemid)
            self.cgdManager.updateCdbEntry(cdb_name, entry_num, "gi_type", new_gi_type)
            showToast(self, "Item updated successfully")
            self.sortGachaPackageInfo("gachaponpackageinfo")
            self.showCapsuleItems(self.capsuleList.currentItem())
        except Exception as e:
            showMessage(QMessageBox.Critical, "Error updating item", f"Error while trying to update the item:\n{e}", self)

    def sortGachaPackageInfo(self, cdb_name: str):
        cdb = self.cgdManager.cdbs.get(cdb_name)
        if not cdb:
            return

        cdb_copy = deepcopy(cdb)
        cdb_copy.entries.sort(key=lambda entry: (-getFieldValue(entry, "gi_type", 0), getFieldValue(entry, "gi_id", 0)))
        ts_index = cdb_copy.keys.index("gi_id")
        ts = cdb_copy.typeSizes[ts_index]
        new_id = 0
        for entry in cdb_copy.entries:
            self.setEntryValueByType(entry, ts_index, new_id, ts)
            new_id += 1

        try:
            self.cgdManager.saveCdbOutputs(cdb_copy)
        except Exception as e:
            showMessage(QMessageBox.Critical,"Error saving sorted CDB",f"Error while saving sorted CDB '{cdb_name}':\n{str(e)}",
                self)
            return

        cdb.entries = cdb_copy.entries

    def sortCdbByKey(self, cdb_name: str, key: str, reverse: bool = False, start: int = 0):
        cdb = self.cgdManager.cdbs.get(cdb_name)
        if not cdb:
            return

        cdb_copy = deepcopy(cdb)
        cdb_copy.entries.sort(key=lambda entry: getFieldValue(entry, key, 0),reverse=reverse)
        if key == "gi_id":
            ts_index = cdb_copy.keys.index("gi_id")
            ts = cdb_copy.typeSizes[ts_index]

            for idx, entry in enumerate(cdb_copy.entries, start=start):
                self.setEntryValueByType(entry, ts_index, idx, ts)

        try:
            self.cgdManager.saveCdbOutputs(cdb_copy)
        except Exception as e:
            showMessage(QMessageBox.Critical,"Error saving sorted CDB",
            f"Error while saving sorted CDB '{cdb_name}':\n{str(e)}", self)
            return
        cdb.entries = cdb_copy.entries

    def addItemToCapsuleImpl(self, capsule_entry, new_item_id, gi_type):
        if not capsule_entry:
            raise ValueError("capsule_entry NOT provided")
        
        gi_infoid = getFieldValue(capsule_entry, "gi_infoid")
        if new_item_id is None or gi_type is None:
            raise ValueError("Both new_item_id and gi_type NOT provided")
        
        max_gi_id = 0
        for cdb_name, cdb in self.cgdManager.cdbs.items():
            if "gachaponpackageinfo" in cdb_name.lower():
                for entry in cdb.entries:
                    entry_gi_id = getFieldValue(entry, "gi_id", 0)
                    if entry_gi_id > max_gi_id:
                        max_gi_id = entry_gi_id
        
        new_gi_id = max_gi_id + 1
        new_entry = [
            EntryDataType("gi_id", 4, new_gi_id),
            EntryDataType("gi_infoid", 4, gi_infoid),
            EntryDataType("gi_type", 4, gi_type),
            EntryDataType("gi_luckytype", 4, 0),
            EntryDataType("gi_group", 4, 1),
            EntryDataType("gi_prob", 4, 9166 if gi_type == 1 else 37500),
            EntryDataType("gi_itemid", 4, new_item_id),
            EntryDataType("gi_noticeid", 4, 0),
        ]
        
        result = self.cgdManager.addEntryTo("gachaponpackageinfo", new_entry)
        self.sortGachaPackageInfo("gachaponpackageinfo")
        return result

    def addItemsToCapsule(self, capsule_entry, items: list[tuple[int, int]]):
        if not capsule_entry:
            return {"success": False, "errors": ["capsule_entry not provided"]}

        gi_infoid = getFieldValue(capsule_entry, "gi_infoid")
        errors = []
        new_entries = []
        max_gi_id = 0
        for cdb_name, cdb in self.cgdManager.cdbs.items():
            if "gachaponpackageinfo" in cdb_name.lower():
                for entry in cdb.entries:
                    entry_gi_id = getFieldValue(entry, "gi_id", 0)
                    if entry_gi_id > max_gi_id:
                        max_gi_id = entry_gi_id

        for new_item_id, gi_type in items:
            if new_item_id is None or gi_type is None:
                errors.append(f"Missing new_item_id or gi_type for entry ({new_item_id}, {gi_type})")
                continue

            max_gi_id += 1
            new_entry = [
                EntryDataType("gi_id", 4, max_gi_id),
                EntryDataType("gi_infoid", 4, gi_infoid),
                EntryDataType("gi_type", 4, gi_type),
                EntryDataType("gi_luckytype", 4, 0),
                EntryDataType("gi_group", 4, 1),
                EntryDataType("gi_prob", 4, 9166 if gi_type == 1 else 37500),
                EntryDataType("gi_itemid", 4, new_item_id),
                EntryDataType("gi_noticeid", 4, 0),
            ]
            new_entries.append(new_entry)

        if new_entries:
            result = self.cgdManager.addEntriesTo("gachaponpackageinfo", new_entries)
            if not result.get("success"):
                errors.append(result.get("error"))

        if errors:
            return {"success": False, "errors": errors}
        self.sortGachaPackageInfo("gachaponpackageinfo")
        return {"success": True, "message": f"Added {len(new_entries)} items successfully"}

    def addNewItem(self):
        current_capsule = self.capsuleList.currentItem()

        if not current_capsule:
            showMessage(QMessageBox.Warning,"Error","You need to select a capsule before trying to add a new item", self)
            return

        capsule_entry = current_capsule.data(Qt.UserRole)
        dialog = AddItemDialog(self.item_lookup, self.icon_lookup, self.pixmap_cache, self.item_icon_path, self)
        if not dialog.exec():
            return

        new_item_id, gi_type = dialog.getValues()
        if new_item_id is None or gi_type is None:
            showMessage(QMessageBox.Warning,"Error","Both ItemID and CapsuleType must be provided!", self)
            return

        result = self.addItemToCapsuleImpl(capsule_entry, new_item_id, gi_type)
        if result.get("success"):
            showToast(self, "Item added successfully")
            self.showCapsuleItems(current_capsule)
        else:
            showMessage(QMessageBox.Critical,"Error",result.get("error", "Failed to add entry."), self)

    def addNewCapsule(self):
        dialog = CreateCapsuleDialog(self)
        dialog.setModal(False)  
        dialog.show()

        dialog.finished.connect(lambda result, dlg=dialog: self.onCapsuleDialogFinished(result, dlg))

    def onCapsuleDialogFinished(self, result, dialog):
        if result != QDialog.DialogCode.Accepted:
            dialog.deleteLater()
            return

        values = dialog.getValues()
        missing = []

        if not values["gi_name"]:
            missing.append("Name (gi_name)")
        if values["gi_listicon"] == 0:
            missing.append("List icon ID (gi_listicon)")
        if values["gi_titleicon"] == 0:
            missing.append("Title icon ID (gi_titleicon)")
        if not values["gi_desc"]:
            missing.append("Description (gi_desc)")
        if values["gi_limited_grade"] <= 0:
            missing.append("Level required (gi_limited_grade)")
        if values["gi_price"] <= 0:
            missing.append("Price (gi_price)")
        if values["gi_luckypoint"] <= 0:
            missing.append("Lucky point (gi_luckypoint)")

        if missing:
            QMessageBox.warning(self, "Validation Error","These fields are missing or have wrong values:\n\n" + "\n".join(missing))
            return

        if values["gi_listicon"] not in self.icon_lookup:
            QMessageBox.critical(self, "Error", f"List icon ID {values['gi_listicon']} not found in iconsinfo")
            return
        if values["gi_titleicon"] not in self.icon_lookup:
            QMessageBox.critical(self, "Error", f"Title icon ID {values['gi_titleicon']} not found in iconsinfo")
            return

        max_gi_id = 0
        existing_infoids = set()
        found_gachaponinfo = False

        for cdb_name, cdb in self.cgdManager.cdbs.items():
            if "gachaponinfo" in cdb_name.lower():
                found_gachaponinfo = True
                for entry in cdb.entries:
                    entry_gi_id = getFieldValue(entry, "gi_id", 0)
                    if entry_gi_id > max_gi_id:
                        max_gi_id = entry_gi_id
                    entry_infoid = getFieldValue(entry, "gi_infoid", None)
                    if entry_infoid is not None:
                        existing_infoids.add(entry_infoid)

        if not found_gachaponinfo:
            QMessageBox.critical(self, "Error", "No 'gachaponinfo' CDB found in loaded files")
            return

        new_gi_id = max_gi_id # + 1 unnecessary since original last capsule still remains last
        new_gi_infoid = None
        for _ in range(10000):
            candidate = random.randint(100000, 999999)
            if candidate not in existing_infoids:
                new_gi_infoid = candidate
                break
        if new_gi_infoid is None:
            QMessageBox.critical(self, "Error", "Failed to generate an unique gi_infoid!")
            return

        new_entry = [
            EntryDataType("gi_id", 4, new_gi_id),
            EntryDataType("gi_name", 64, values["gi_name"]),
            EntryDataType("gi_type", 4, values["gi_type"]),
            EntryDataType("gi_statetype", 4, 0),
            EntryDataType("gi_infoid", 4, new_gi_infoid),
            EntryDataType("gi_limited_grade", 4, values["gi_limited_grade"]),
            EntryDataType("gi_price", 4, values["gi_price"]),
            EntryDataType("gi_luckypoint", 4, values["gi_luckypoint"]),
            EntryDataType("gi_listicon", 4, values["gi_listicon"]),
            EntryDataType("gi_titleicon", 4, values["gi_titleicon"]),
            EntryDataType("gi_desc", 255, values["gi_desc"]),
        ]

        result = self.cgdManager.addEntryTo("gachaponinfo", new_entry)
        if result.get("success"):
            cdb = self.cgdManager.cdbs.get("gachaponinfo")
            if cdb and len(cdb.entries) >= 2:
                original_last = cdb.entries[-2]
                updated_last_id = new_gi_id + 1
                for field in original_last:
                    if field.key == "gi_id":
                        field.value = updated_last_id
                        break
                cdb.entries[-2], cdb.entries[-1] = cdb.entries[-1], cdb.entries[-2]
                
                try:
                    self.cgdManager.saveCdbOutputs(cdb)
                except Exception as e:
                    showMessage(
                    QMessageBox.Critical,"Error","There was an error while re-sorting the capsule after adding the item. "
                    "Please manually put the lucky box capsule at the end of gachaponinfo")
                    return
            showToast(self, "New capsule added successfully")
            self.loadCapsules()
        else:
            QMessageBox.critical(self, "Error", result.get("error", "Failed to add new capsule."))
        dialog.deleteLater()

    def onCapsuleSelected(self, capsule_item):
        self.addItemButton.setEnabled(True)
        self.deleteCapsuleButton.setEnabled(True)
        self.updateCapsuleButton.setEnabled(True)
        self.addFixedItemsButton.setEnabled(True)
        self.updateCapsuleButton.setEnabled(True)
        self.showCapsuleItems(capsule_item)

    def updateCapsule(self):
        current_capsule = self.capsuleList.currentItem()
        if not current_capsule:
            QMessageBox.warning(self, "Error", "Select a capsule first.")
            return

        capsule_entry = current_capsule.data(Qt.UserRole)
        current_values = {
            "gi_name": getFieldValue(capsule_entry, "gi_name"),
            "gi_type": getFieldValue(capsule_entry, "gi_type"),
            "gi_limited_grade": getFieldValue(capsule_entry, "gi_limited_grade"),
            "gi_price": getFieldValue(capsule_entry, "gi_price"),
            "gi_luckypoint": getFieldValue(capsule_entry, "gi_luckypoint"),
            "gi_listicon": getFieldValue(capsule_entry, "gi_listicon"),
            "gi_titleicon": getFieldValue(capsule_entry, "gi_titleicon"),
            "gi_desc": getFieldValue(capsule_entry, "gi_desc"),
        }

        dialog = CreateCapsuleDialog(self, prefill=current_values)
        dialog.setModal(False)
        dialog.show()

        dialog.finished.connect(lambda result, dlg=dialog, cap_entry=capsule_entry: 
            self.onCapsuleUpdateFinished(result, dlg, cap_entry))
        
    def onCapsuleUpdateFinished(self, result, dialog, capsule_entry):
        if result != QDialog.DialogCode.Accepted:
            dialog.deleteLater()
            return

        values = dialog.getValues()
        missing = []
        for key, label in {
            "gi_name": "Name",
            "gi_listicon": "List Icon ID",
            "gi_titleicon": "Title Icon ID",
            "gi_desc": "Description",
            "gi_limited_grade": "Level Required",
            "gi_price": "Price",
            "gi_luckypoint": "Lucky Point",
        }.items():
            if not values[key]:
                missing.append(label)
        if missing:
            QMessageBox.warning(self, "Validation Error",
                "The following fields are missing or invalid:\n\n" + "\n".join(missing))
            return

        if values["gi_listicon"] not in self.icon_lookup:
            QMessageBox.critical(self, "Error", f"List icon ID {values['gi_listicon']} not found in iconsinfo.")
            return
        if values["gi_titleicon"] not in self.icon_lookup:
            QMessageBox.critical(self, "Error", f"Title icon ID {values['gi_titleicon']} not found in iconsinfo.")
            return

        cdb_name = next((name for name in self.cgdManager.cdbs if "gachaponinfo" in name.lower()), None)
        if not cdb_name:
            QMessageBox.critical(self, "Error", "gachaponinfo CDB not loaded.")
            return

        entry_index = findEntryIndexById(self.cgdManager, cdb_name, capsule_entry)
        if entry_index is None:
            QMessageBox.critical(self, "Error", "Capsule entry not found in CDB.")
            dialog.deleteLater()
            return

        try:
            current_index = self.capsuleList.currentRow()
            scroll_pos = self.capsuleList.verticalScrollBar().value()
            for key, new_val in values.items():
                self.cgdManager.updateCdbEntry(cdb_name, entry_index, key, new_val)
            showToast(self, "Capsule updated successfully!")
            self.loadCapsules(restore_index=current_index, restore_scroll=scroll_pos)
        except Exception as e:
            QMessageBox.critical(self, "Error", f"Failed to update capsule:\n{e}")
        dialog.deleteLater()

    def deleteCapsule(self):
        current_capsule = self.capsuleList.currentItem()
        if not current_capsule:
            QMessageBox.warning(self, "Error", "Select a capsule to delete.")
            return

        capsule_entry = current_capsule.data(Qt.UserRole)
        gi_infoid = getFieldValue(capsule_entry, "gi_infoid")
        gi_name = getFieldValue(capsule_entry, "gi_name")
        gachaponinfo_cdb = next((n for n in self.cgdManager.cdbs if "gachaponinfo" in n.lower()), None)
        if not gachaponinfo_cdb:
            QMessageBox.critical(self, "Error", "gachaponinfo CDB not found.")
            return

        last_entry = self.cgdManager.cdbs[gachaponinfo_cdb].entries[-1]
        if capsule_entry == last_entry:
            QMessageBox.warning(self, "Cannot Delete",
                "The last special capsule is needed for lucky spins to work.\n"
                "You can't delete this capsule, otherwise lucky spins won't work properly anymore!")
            return

        confirm = QMessageBox.question(self, "Confirm Delete",
            f"Are you sure you want to delete capsule '{gi_name}' and all its items?",
            QMessageBox.Yes | QMessageBox.No)
        if confirm != QMessageBox.Yes:
            return

        gi_index = findEntryIndexById(self.cgdManager, gachaponinfo_cdb, capsule_entry)
        if gi_index is None:
            QMessageBox.critical(self, "Error", "Capsule entry not found in CDB.")
            return
        result_info = self.cgdManager.removeEntry(gachaponinfo_cdb, gi_index)

        deleted_count = 0
        for cdb_name, cdb in self.cgdManager.cdbs.items():
            if "gachaponpackageinfo" in cdb_name.lower():
                indices_to_delete = [
                    i for i, entry in enumerate(cdb.entries)
                    if getFieldValue(entry, "gi_infoid") == gi_infoid
                ]
                if indices_to_delete:
                    result_items = self.cgdManager.removeEntriesFrom(cdb_name, indices_to_delete)
                    deleted_count += len(indices_to_delete)
                    if not result_items.get("success"):
                        QMessageBox.critical(self, "Error",
                            f"Failed to delete some items from {cdb_name}: {result_items.get('error')}")

        if result_info.get("success"):
            self.sortCdbByKey(gachaponinfo_cdb, "gi_id")
            showToast(self, "Capsule deleted successfully")
            self.loadCapsules()
            self.addItemButton.setEnabled(False)
            self.deleteCapsuleButton.setEnabled(False)
            self.addFixedItemsButton.setEnabled(False)
            self.updateCapsuleButton.setEnabled(False)
        else:
            QMessageBox.critical(self, "Error", result_info.get("error", "Failed to delete capsule."))

    def addFixedItems(self):
        current_capsule = self.capsuleList.currentItem()

        if not current_capsule:
            showMessage(QMessageBox.Warning,"Error","You need to select a capsule before trying to add new items", self)
            return

        capsule_entry = current_capsule.data(Qt.UserRole)
        dialog = AddFixedItems(self)
        if not dialog.exec():
            return

        items_to_add = [(itemId, 0) for itemId in dialog.getItemIds()]
        result = self.addItemsToCapsule(capsule_entry, items_to_add)
        self.showCapsuleItems(current_capsule)

        if not result.get("success"):
            showMessage(QMessageBox.Critical,"Error",f"Failed to add some items:\n{result.get('errors')}", self)
        else:
            showToast(self, "All items added successfully")
