from PySide6.QtWidgets import QDialog, QLineEdit, QSpinBox, QComboBox, QTextEdit, QFormLayout, QDialogButtonBox

class CreateCapsuleDialog(QDialog):
    def __init__(self, parent=None, prefill=None):
        super().__init__(parent)
        self.setWindowTitle("Create New Capsule" if prefill is None else "Update Capsule")
        self.setMinimumWidth(420)

        layout = QFormLayout(self)

        self.name_input = QLineEdit()
        layout.addRow("Name:", self.name_input)

        self.type_input = QComboBox()
        self.type_input.addItems(["Coins (0)", "RockTokens (1)", "MicroPoints (2)"])
        layout.addRow("Type:", self.type_input)

        self.grade_input = QSpinBox()
        self.grade_input.setRange(0, 999)
        layout.addRow("Level required:", self.grade_input)

        self.price_input = QSpinBox()
        self.price_input.setRange(0, 9999999)
        layout.addRow("Price:", self.price_input)

        self.lucky_input = QSpinBox()
        self.lucky_input.setRange(0, 9999)
        layout.addRow("Lucky points per spin:", self.lucky_input)

        self.listicon_input = QSpinBox()
        self.listicon_input.setRange(0, 999999999)
        layout.addRow("List iconID (ii_id from iconsinfo):", self.listicon_input)

        self.titleicon_input = QSpinBox()
        self.titleicon_input.setRange(0, 999999999)
        layout.addRow("Title iconID (ii_id from iconsinfo):", self.titleicon_input)

        self.listicon_input.setValue(91301126)
        self.titleicon_input.setValue(91301132)

        self.desc_input = QTextEdit()
        self.desc_input.setMaximumHeight(120)
        layout.addRow("Description:", self.desc_input)

        buttons = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addRow(buttons)

        self.type_input.currentIndexChanged.connect(self._apply_default_price_for_type)
        self._apply_default_price_for_type(self.type_input.currentIndex())

        if prefill:
            self._load_prefill(prefill)
        else:
            self.grade_input.setValue(1)
            self.lucky_input.setValue(60)

    def _apply_default_price_for_type(self, idx):
        if idx == 0:
            self.price_input.setValue(1)
            self.lucky_input.setValue(40)
        elif idx == 1:
            self.price_input.setValue(990)
            self.lucky_input.setValue(60)
        elif idx == 2:
            self.price_input.setValue(3990)
            self.lucky_input.setValue(15)

    def _load_prefill(self, prefill):
        self.name_input.setText(str(prefill.get("gi_name", "")))
        self.type_input.setCurrentIndex(int(prefill.get("gi_type", 0)))
        self.grade_input.setValue(int(prefill.get("gi_limited_grade", 1)))
        self.price_input.setValue(int(prefill.get("gi_price", 1)))
        self.lucky_input.setValue(int(prefill.get("gi_luckypoint", 60)))
        self.listicon_input.setValue(int(prefill.get("gi_listicon", 0)))
        self.titleicon_input.setValue(int(prefill.get("gi_titleicon", 0)))
        self.desc_input.setPlainText(str(prefill.get("gi_desc", "")))

    def getValues(self):
        desc = self.desc_input.toPlainText().strip()
        return {
            "gi_name": self.name_input.text().strip(),
            "gi_type": self.type_input.currentIndex(),
            "gi_limited_grade": self.grade_input.value(),
            "gi_price": self.price_input.value(),
            "gi_luckypoint": self.lucky_input.value(),
            "gi_listicon": self.listicon_input.value(),
            "gi_titleicon": self.titleicon_input.value(),
            "gi_desc": desc if desc else "Default description"
        }

