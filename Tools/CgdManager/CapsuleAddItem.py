from PySide6.QtWidgets import QDialog, QFormLayout, QLineEdit, QComboBox, QDialogButtonBox

class AddItemDialog(QDialog):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Add new capsule item")
        layout = QFormLayout(self)

        self.item_id_input = QLineEdit()
        self.item_id_input.setPlaceholderText("Enter ItemID")
        layout.addRow("ItemID:", self.item_id_input)

        self.type_input = QComboBox()
        self.type_input.addItems(["Normal (0)", "Rare (1)"])
        layout.addRow("Item Type:", self.type_input)

        buttons = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

    def getValues(self):
        try:
            gi_itemid = int(self.item_id_input.text())
        except ValueError:
            gi_itemid = None
        gi_type = self.type_input.currentIndex()  
        return gi_itemid, gi_type

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
