from PySide6.QtWidgets import QDialog, QFormLayout, QLineEdit, QComboBox, QDialogButtonBox, QLabel, QMessageBox
from Utils import defaultPixmap, getFieldValue, loadPixmap, showMessage
from PySide6.QtCore import Qt

class AddItemDialog(QDialog):
    def __init__(self, item_lookup, icon_lookup, pixmap_cache, item_icon_path, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Add new capsule item")
        self.item_lookup = item_lookup   
        self.icon_lookup = icon_lookup
        self.pixmap_cache = pixmap_cache
        self.item_icon_path = item_icon_path

        layout = QFormLayout(self)

        self.item_id_input = QLineEdit()
        self.item_id_input.setPlaceholderText("Enter ItemID")
        layout.addRow("ItemID:", self.item_id_input)

        self.icon_preview = QLabel()
        self.icon_preview.setFixedSize(64, 64)
        self.icon_preview.setPixmap(defaultPixmap().scaled(64, 64, Qt.KeepAspectRatio))
        layout.addRow("Preview:", self.icon_preview)

        self.type_input = QComboBox()
        self.type_input.addItems(["Normal (0)", "Rare (1)"])
        layout.addRow("Item Type:", self.type_input)

        buttons = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        buttons.accepted.connect(self.validateAndAccept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

        self.item_id_input.textChanged.connect(self.updateIconPreview)

    def updateIconPreview(self):
        text = self.item_id_input.text().strip()
        if not text.isdigit():
            self.icon_preview.setPixmap(defaultPixmap().scaled(64, 64, Qt.KeepAspectRatio))
            return

        item_id = int(text)
        item_entry = self.item_lookup.get(item_id)
        if not item_entry:
            self.icon_preview.setPixmap(defaultPixmap().scaled(64, 64, Qt.KeepAspectRatio))
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
                except Exception as e:
                    pixmap = defaultPixmap()

            self.pixmap_cache[icon_id] = pixmap
        self.icon_preview.setPixmap(pixmap.scaled(64, 64, Qt.KeepAspectRatio))
        self.icon_preview.repaint()

    def validateAndAccept(self):
        text = self.item_id_input.text().strip()
        if not text.isdigit():
            showMessage(QMessageBox.Warning, "Invalid ItemID", "ItemID must be a number.", self)
            return

        item_id = int(text)
        if item_id not in self.item_lookup:
            showMessage(QMessageBox.Warning, "Unknown ItemID", f"ItemID {item_id} does not exist.", self)
            return

        self.values = (item_id, self.type_input.currentIndex())
        self.accept()

    def getValues(self):
        return getattr(self, "values", (None, None))

class AddFixedItems(QDialog):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Select capsule type")

        self.type_input = QComboBox()
        self.type_input.addItems(["Melee", "Rifle", "Shotgun", "Sniper", "MicroGun", "Bazooka", "Grenade",
                                  "Naomi", "Pandora", "CHIP", "Knox", "Kai", "Simon", "Amelia", "Sharkill", "Sophitia"])
        layout = QFormLayout(self)
        layout.addRow("Capsule Type:", self.type_input)

        buttons = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)
    
    def getItemIds(self):
        capsule_type = self.type_input.currentIndex()  
        common_items = {2613500, 2613600, 2610300, 4600030, 4600020, 4600010, 4305005}
        if capsule_type == 0: # melee
            return {3011600, 3011700, 3011800, *common_items}
        elif capsule_type == 1: 
            return {3021800, 3021900, 3022000, *common_items}
        elif capsule_type == 2:
            return {3031100, 3031200, 3031300, *common_items}
        elif capsule_type == 3:
            return {3041000, 3041100, 3041200, *common_items}
        elif capsule_type == 4:
            return {3050800, 3050900, 3051000, *common_items}
        elif capsule_type == 5: 
            return {306550, 3062650, 3062750, *common_items}
        elif capsule_type == 6:
            return {3071750, 3071850, 3071950, *common_items}
        elif capsule_type == 7: # naomi
            return {6200000, 6200001, 6202400, 6202401, 6202600, 6202601, 6202800, 6202801, *common_items}
        elif capsule_type == 8:
            return {5220500, 5220501, 6220800, 6220801, 6221000, 6221001, 6221100, 6221101, *common_items}
        elif capsule_type == 9:
            return {6230200, 6230201, 6231000, 6231001, 6230400, 6230401, 6231300, 6231301, *common_items}
        elif capsule_type == 10:
            return {6210600, 6210601, 6211200, 6211201, 6210400, 6210401, 6210800, 6210801, *common_items}
        elif capsule_type == 11:
            return {1153400, 1353200, 1353300, 1553000, 1553100, 1752301, 1752400, *common_items}
        else:
            return common_items
