from functools import partial
from dataclasses import dataclass
from typing import Any
from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QLabel, QLineEdit, QMessageBox, QScrollArea, QDialog, QDialogButtonBox, QFormLayout
)

@dataclass
class EntryDataType:
    key: str
    typeSize: int
    value: Any

class NewEntryDialog(QDialog):
    def __init__(self, schema_fields, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Add New Entry")
        self.resize(500, 600)
        self.setModal(True)

        main_layout = QVBoxLayout(self)

        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        main_layout.addWidget(scroll)

        scroll_content = QWidget()
        scroll_layout = QFormLayout(scroll_content)
        scroll.setWidget(scroll_content)

        self.editors = {}
        self.schema_fields = schema_fields
        for field in schema_fields:
            editor = QLineEdit()
            scroll_layout.addRow(QLabel(field.key), editor)
            self.editors[field.key] = editor

        buttons = QDialogButtonBox(QDialogButtonBox.StandardButton.Ok | QDialogButtonBox.StandardButton.Cancel)
        buttons.accepted.connect(self.validateAndAccept)
        buttons.rejected.connect(self.reject)
        main_layout.addWidget(buttons)

        self.values = None

    def validateAndAccept(self):
        validated = []
        for field in self.schema_fields:
            key = field.key
            ts = field.typeSize
            text = self.editors[key].text().strip()
            try:
                if ts == 1:
                    if text.lower() in ("1","true","yes"):
                        val = True
                    elif text.lower() in ("0","false","no"):
                        val = False
                    else:
                        raise TypeError(f"Key '{key}' expects boolean value")
                elif ts in (2,3,4):
                    val = int(text)
                else:
                    if text.isdigit():
                        raise TypeError(f"Key '{key}' expects string value")
                    val = text
                validated.append(EntryDataType(key, ts, val))
            except Exception as e:
                QMessageBox.warning(self, "Invalid value", f"{e}")
                return
        self.values = validated
        self.accept()

    def getValues(self):
        return self.values