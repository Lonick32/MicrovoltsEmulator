from functools import partial
from PySide6.QtWidgets import (
    QWidget, QListWidget, QListWidgetItem, QLabel, QMessageBox,
    QHBoxLayout, QVBoxLayout, QScrollArea, QPushButton, QInputDialog
)
from PySide6.QtGui import QPixmap
from PySide6.QtCore import Qt
from IconLoader import IconLoaderThread, defaultPixmap
from CapsuleAddItem import AddItemDialog, AddFixedItems
from CdbParser import EntryDataType
from NewCapsuleDialog import CreateCapsuleDialog
import random

class CapsuleManager(QWidget):
    def __init__(self, cgdManager, capsule_icon_path, item_icon_path):
        super().__init__()
        self.cgdManager = cgdManager
        self.iconFolder = capsule_icon_path
        self.item_icon_path = item_icon_path
        self.pixmap_cache = {} 
        self.threads = []
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
        self.updateCapsuleButton = QPushButton("Update Capsule")
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
                    self.item_lookup[self.getFieldValue(entry, "ii_id")] = entry
            elif "setiteminfo" in lower:
                for entry in cdb.entries:
                    self.item_lookup[self.getFieldValue(entry, "si_id")] = entry
            elif "iconsinfo" in lower:
                for entry in cdb.entries:
                    self.icon_lookup[self.getFieldValue(entry, "ii_id")] = entry

    def decodeValue(self, val, ts):
        if isinstance(val, (bytes, bytearray)):
            try:
                if ts in (1, 2, 3, 4):
                    return int.from_bytes(val, byteorder="little")
                return val.decode("utf-8").rstrip("\x00")
            except Exception:
                return str(val)
        if isinstance(val, str) and val.startswith("b/0x"):
            try:
                bytes_list = [int(x, 16) for x in val.split("/")[1:]]
                return int.from_bytes(bytes(bytes_list), "little")
            except Exception:
                return val
        return val

    def getFieldValue(self, entry, key, default=None):
        field = next((f for f in entry if f.key == key), None)
        if field:
            return self.decodeValue(field.value, field.typeSize)
        return default

    def loadCapsules(self):
        self.capsuleList.clear()
        for cdb_name, cdb in self.cgdManager.cdbs.items():
            if "gachaponinfo" in cdb_name.lower():
                for entry in cdb.entries:
                    gi_name = self.getFieldValue(entry, "gi_name")
                    gi_price = self.getFieldValue(entry, "gi_price")
                    gi_type = self.getFieldValue(entry, "gi_type")
                    typeStr = "Coins" if gi_type == 0 else "RockTokens" if gi_type == 1 else "MicroPoints"
                    item = QListWidgetItem(f"{gi_name} ({gi_price} {typeStr})")
                    item.setData(Qt.UserRole, entry)
                    self.capsuleList.addItem(item)

    def showCapsuleItems(self, capsule_item):
        entry = capsule_item.data(Qt.UserRole)
        gi_infoid = self.getFieldValue(entry, "gi_infoid")

        for i in reversed(range(self.itemLayout.count())): # otherwise previous items stack up with new ones
            widget = self.itemLayout.itemAt(i).widget()
            if widget:
                widget.setParent(None)

        items_to_display = []
        for cdb_name, cdb in self.cgdManager.cdbs.items():
            if "gachaponpackageinfo" in cdb_name.lower():
                for pkg_entry in cdb.entries:
                    if self.getFieldValue(pkg_entry, "gi_infoid") == gi_infoid:
                        item_id = self.getFieldValue(pkg_entry, "gi_itemid")
                        item_entry = self.item_lookup.get(item_id)
                        if item_entry:
                            item_name = self.getFieldValue(item_entry, "ii_name") or self.getFieldValue(item_entry, "si_name")
                            icon_id = self.getFieldValue(item_entry, "ii_iconsmall") or self.getFieldValue(item_entry, "si_iconsmall")
                            items_to_display.append((item_name, icon_id, pkg_entry, cdb_name, item_id))

        if items_to_display:
            self.displayCapsuleItems(items_to_display)

    def displayCapsuleItems(self, items_to_display):
        items_to_load = []

        for item_name, icon_id, pkg_entry, cdb_name, item_id in items_to_display:
            container = QWidget()
            layout = QHBoxLayout(container)
            layout.setContentsMargins(5, 5, 5, 5)

            # rare items
            gi_type = self.getFieldValue(pkg_entry, "gi_type")
            if gi_type == 1:
                container.setStyleSheet("background-color: rgb(255, 255, 200);") 
            else:
                container.setStyleSheet("") 

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
                icon_lbl.setPixmap(self.pixmap_cache[icon_id].scaled(64, 64, Qt.KeepAspectRatio))
            else:
                icon_lbl.setPixmap(defaultPixmap().scaled(64, 64, Qt.KeepAspectRatio))
                if icon_entry_raw := self.icon_lookup.get(icon_id):
                    items_to_load.append((
                        icon_id,
                        {
                            "filename": self.getFieldValue(icon_entry_raw, "ii_filename"),
                            "offset": self.getFieldValue(icon_entry_raw, "ii_offset"),
                            "width": self.getFieldValue(icon_entry_raw, "ii_width"),
                            "height": self.getFieldValue(icon_entry_raw, "ii_height"),
                        }
                    ))

            delete_btn.clicked.connect(partial(self.deleteItem, pkg_entry, cdb_name, container))
            update_btn.clicked.connect(partial(self.updateItem, pkg_entry, cdb_name, text_lbl))

        if items_to_load:
            thread = IconLoaderThread(items_to_load, self.item_icon_path)
            thread.iconLoaded.connect(self.onIconLoaded)
            thread.start()
            self.threads.append(thread)

    def deleteItem(self, pkg_entry, cdb_name, widget):
        entry_num = self.cgdManager.cdbs[cdb_name].entries.index(pkg_entry)
        result = self.cgdManager.removeEntry(cdb_name, entry_num)

        if result.get("success"):
            widget.setParent(None)
        else:
            msg = QMessageBox(self)
            msg.setIcon(QMessageBox.Critical)
            msg.setWindowTitle("Error Deleting Item")
            msg.setText(result.get("error", "Failed to delete the item"))
            msg.exec()

    def updateItem(self, pkg_entry, cdb_name, text_label):
        entry_num = self.cgdManager.cdbs[cdb_name].entries.index(pkg_entry)
        new_val, ok = QInputDialog.getInt(self, "Update ItemID", "Enter new ItemID:")
        
        if not ok:
            return 

        try:
            self.cgdManager.updateCdbEntry(cdb_name, entry_num, "gi_itemid", new_val)
            #text_label.setText(f"{text_label.text().split('(ItemID:')[0]}(ItemID: {new_val})")
            self.showCapsuleItems(self.capsuleList.currentItem())
        except Exception as e:
            msg = QMessageBox(self)
            msg.setIcon(QMessageBox.Critical)
            msg.setWindowTitle("Error updating item")
            msg.setText(f"Error while trying to update the item:\n{str(e)}")
            msg.exec()

    def sortGachaPackageInfo(self, cdb_name: str):
        cdb = self.cgdManager.cdbs.get(cdb_name)
        if not cdb:
            return
        cdb.entries.sort(key=lambda entry: (-self.getFieldValue(entry, "gi_type", 0), self.getFieldValue(entry, "gi_id", 0)))
        self.cgdManager.saveCdbOutputs(cdb)

    def addItemToCapsuleImpl(self, capsule_entry, new_item_id, gi_type):
        if not capsule_entry:
            raise ValueError("capsule_entry NOT provided")
        
        gi_infoid = self.getFieldValue(capsule_entry, "gi_infoid")
        
        if new_item_id is None or gi_type is None:
            raise ValueError("Both new_item_id and gi_type NOT provided")
        
        max_gi_id = 0
        for cdb_name, cdb in self.cgdManager.cdbs.items():
            if "gachaponpackageinfo" in cdb_name.lower():
                for entry in cdb.entries:
                    entry_gi_id = self.getFieldValue(entry, "gi_id", 0)
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

        gi_infoid = self.getFieldValue(capsule_entry, "gi_infoid")
        errors = []
        new_entries = []

        max_gi_id = 0
        for cdb_name, cdb in self.cgdManager.cdbs.items():
            if "gachaponpackageinfo" in cdb_name.lower():
                for entry in cdb.entries:
                    entry_gi_id = self.getFieldValue(entry, "gi_id", 0)
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
            self.sortGachaPackageInfo("gachaponpackageinfo")
            if not result.get("success"):
                errors.append(result.get("error"))

        if errors:
            return {"success": False, "errors": errors}
        return {"success": True, "message": f"Added {len(new_entries)} items successfully"}


    def addNewItem(self):
        current_capsule = self.capsuleList.currentItem()
        errorMessage = QMessageBox(self)
        errorMessage.setWindowTitle("Error")
        errorMessage.setIcon(QMessageBox.Warning)

        if not current_capsule:
            errorMessage.setText("You need to select a capsule before trying to add a new item")
            errorMessage.exec()
            return

        capsule_entry = current_capsule.data(Qt.UserRole)

        dialog = AddItemDialog(self)
        if not dialog.exec():
            return

        new_item_id, gi_type = dialog.getValues()
        if new_item_id is None or gi_type is None:
            errorMessage.setText("Both ItemID and CapsuleType must be provided!")
            errorMessage.exec()
            return

        result = self.addItemToCapsuleImpl(capsule_entry, new_item_id, gi_type)
        if result.get("success"):
            success = QMessageBox(self)
            success.setIcon(QMessageBox.Information)
            success.setWindowTitle("Success")
            success.setText(result.get("message", "Entry added successfully!"))
            success.exec()
            self.showCapsuleItems(current_capsule)
        else:
            errorMessage.setIcon(QMessageBox.Critical)
            errorMessage.setText(result.get("error", "Failed to add entry."))
            errorMessage.exec()

    def onIconLoaded(self, icon_id, pixmap):
        self.pixmap_cache[icon_id] = pixmap
        for i in range(self.itemLayout.count()):
            container = self.itemLayout.itemAt(i).widget()
            if container:
                icon_lbl = container.findChild(QLabel, "icon")
                if icon_lbl.property("icon_id") == icon_id:
                    icon_lbl.setPixmap(pixmap.scaled(64, 64, Qt.KeepAspectRatio))

    def addNewCapsule(self):
        dialog = CreateCapsuleDialog(self)
        if not dialog.exec():
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
            QMessageBox.warning(
                self,
                "Validation Error",
                "These fields are missing or have wrong values:\n\n" + "\n".join(missing)
            )
            return

        if values["gi_listicon"] not in self.icon_lookup:
            QMessageBox.critical(self, "Error", f"List icon ID {values['gi_listicon']} not found in iconsinfo")
            return
        if values["gi_titleicon"] not in self.icon_lookup:
            QMessageBox.critical(self, "Error", f"Title icon ID {values['gi_titleicon']} not found in iconsinfo")
            return

        max_gi_id = 0
        last_entry_index = -1
        existing_infoids = set()
        found_gachaponinfo = False
        for cdb_name, cdb in self.cgdManager.cdbs.items():
            if "gachaponinfo" in cdb_name.lower():
                found_gachaponinfo = True
                for i, entry in enumerate(cdb.entries):
                    entry_gi_id = self.getFieldValue(entry, "gi_id", 0)
                    if entry_gi_id > max_gi_id:
                        max_gi_id = entry_gi_id
                        last_entry_index = i
                    entry_infoid = self.getFieldValue(entry, "gi_infoid", None)
                    if entry_infoid is not None:
                        existing_infoids.add(entry_infoid)

        if not found_gachaponinfo:
            QMessageBox.critical(self, "Error", "No 'gachaponinfo' CDB found in loaded files")
            return

        new_gi_id = max_gi_id
        old_last_gi_id = new_gi_id + 1

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
                try:
                    cdb.entries[-2], cdb.entries[-1] = cdb.entries[-1], cdb.entries[-2]
                    self.cgdManager.saveCdbOutputs(cdb)
                except Exception as e:
                    QMessageBox.information(self, "Warning", "New capsule added, but couldn't swap last two entries (check manually)")
            QMessageBox.information(self, "Success", "New capsule added successfully")
            self.loadCapsules()
        else:
            QMessageBox.critical(self, "Error", result.get("error", "Failed to add new capsule."))


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
            "gi_name": self.getFieldValue(capsule_entry, "gi_name"),
            "gi_type": self.getFieldValue(capsule_entry, "gi_type"),
            "gi_limited_grade": self.getFieldValue(capsule_entry, "gi_limited_grade"),
            "gi_price": self.getFieldValue(capsule_entry, "gi_price"),
            "gi_luckypoint": self.getFieldValue(capsule_entry, "gi_luckypoint"),
            "gi_listicon": self.getFieldValue(capsule_entry, "gi_listicon"),
            "gi_titleicon": self.getFieldValue(capsule_entry, "gi_titleicon"),
            "gi_desc": self.getFieldValue(capsule_entry, "gi_desc"),
        }

        dialog = CreateCapsuleDialog(self, prefill=current_values)
        if not dialog.exec():
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
            QMessageBox.warning(
                self, "Validation Error",
                "The following fields are missing or invalid:\n\n" + "\n".join(missing)
            )
            return

        if values["gi_listicon"] not in self.icon_lookup:
            QMessageBox.critical(self, "Error", f"List icon ID {values['gi_listicon']} not found in iconsinfo.")
            return
        if values["gi_titleicon"] not in self.icon_lookup:
            QMessageBox.critical(self, "Error", f"Title icon ID {values['gi_titleicon']} not found in iconsinfo.")
            return

        cdb_name = next(
            (name for name in self.cgdManager.cdbs if "gachaponinfo" in name.lower()), None
        )
        if not cdb_name:
            QMessageBox.critical(self, "Error", "gachaponinfo CDB not loaded.")
            return

        entry_index = self.cgdManager.cdbs[cdb_name].entries.index(capsule_entry)

        try:
            for key, new_val in values.items():
                self.cgdManager.updateCdbEntry(cdb_name, entry_index, key, new_val)

            QMessageBox.information(self, "Success", "Capsule updated successfully.")
            self.loadCapsules()
        except Exception as e:
            QMessageBox.critical(self, "Error", f"Failed to update capsule:\n{e}")


    def deleteCapsule(self):
        current_capsule = self.capsuleList.currentItem()
        if not current_capsule:
            QMessageBox.warning(self, "Error", "Select a capsule to delete.")
            return

        capsule_entry = current_capsule.data(Qt.UserRole)
        gi_infoid = self.getFieldValue(capsule_entry, "gi_infoid")
        gi_name = self.getFieldValue(capsule_entry, "gi_name")

        confirm = QMessageBox.question(
            self, "Confirm Delete",
            f"Are you sure you want to delete capsule '{gi_name}' and all its items?",
            QMessageBox.Yes | QMessageBox.No
        )
        if confirm != QMessageBox.Yes:
            return

        gachaponinfo_cdb = next(
            (n for n in self.cgdManager.cdbs if "gachaponinfo" in n.lower()), None
        )
        if not gachaponinfo_cdb:
            QMessageBox.critical(self, "Error", "gachaponinfo CDB not found.")
            return

        gi_index = self.cgdManager.cdbs[gachaponinfo_cdb].entries.index(capsule_entry)
        result_info = self.cgdManager.removeEntry(gachaponinfo_cdb, gi_index)

        deleted_count = 0
        for cdb_name, cdb in self.cgdManager.cdbs.items():
            if "gachaponpackageinfo" in cdb_name.lower():
                indices_to_delete = [
                    i for i, entry in enumerate(cdb.entries)
                    if self.getFieldValue(entry, "gi_infoid") == gi_infoid
                ]
                if indices_to_delete:
                    result_items = self.cgdManager.removeEntriesFrom(cdb_name, indices_to_delete)
                    deleted_count += len(indices_to_delete)
                    if not result_items.get("success"):
                        QMessageBox.critical(
                            self, "Error",
                            f"Failed to delete some items from {cdb_name}: {result_items.get('error')}"
                        )

        if result_info.get("success"):
            QMessageBox.information(
                self, "Deleted",
                f"Capsule '{gi_name}' deleted successfully.\nRemoved {deleted_count} related items."
            )
            self.loadCapsules()
            self.addItemButton.setEnabled(False)
            self.deleteCapsuleButton.setEnabled(False)
            self.addFixedItemsButton.setEnabled(False)
            self.updateCapsuleButton.setEnabled(False)
        else:
            QMessageBox.critical(self, "Error", result_info.get("error", "Failed to delete capsule."))

    def addFixedItems(self):
        current_capsule = self.capsuleList.currentItem()
        errorMessage = QMessageBox(self)
        errorMessage.setWindowTitle("Error")
        errorMessage.setIcon(QMessageBox.Warning)

        if not current_capsule:
            errorMessage.setText("You need to select a capsule before trying to add new items")
            errorMessage.exec()
            return

        capsule_entry = current_capsule.data(Qt.UserRole)
        dialog = AddFixedItems(self)
        if not dialog.exec():
            return

        items_to_add = [(itemId, 0) for itemId in dialog.getItemIds()]
        result = self.addItemsToCapsule(capsule_entry, items_to_add)
        self.showCapsuleItems(current_capsule)

        if not result.get("success"):
            errorMessage.setText(f"Failed to add some items:\n{result.get('errors')}")
            errorMessage.exec()
        else:
            success = QMessageBox(self)
            success.setIcon(QMessageBox.Information)
            success.setWindowTitle("Success")
            success.setText(result.get("message", "All items were added successfully!"))
            success.exec()
