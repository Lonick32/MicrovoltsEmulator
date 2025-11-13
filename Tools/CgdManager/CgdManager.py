from PySide6.QtWidgets import (
    QMainWindow, QWidget, QVBoxLayout, QInputDialog, QComboBox,
    QPushButton, QLabel, QLineEdit, QMessageBox, QScrollArea,
    QMenuBar, QDialog, QDialogButtonBox, QFormLayout, QTableWidget, QTableWidgetItem
)
from PySide6.QtGui import QAction
from PySide6.QtCore import Qt
from dataclasses import dataclass
from typing import Any
from CapsuleEditor import CapsuleManager
import os
from functools import partial


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

class CgdEditor(QMainWindow):
    def __init__(self, cgdManager):
        super().__init__()
        self.cgdManager = cgdManager
        self.setWindowTitle("CGD Editor - ToyBattlesHQ")
        self.resize(1200, 700)

        self.menuBar = QMenuBar()

        fileMenu = self.menuBar.addMenu("More options")
        exportAction = QAction("Export cgd.dip", self)
        exportAction.triggered.connect(self.exportCgdDip)
        fileMenu.addAction(exportAction)
        capsuleManagerAction = QAction("Capsule Manager", self)
        capsuleManagerAction.triggered.connect(self.openCapsuleManager)
        fileMenu.addAction(capsuleManagerAction)
        self.setMenuBar(self.menuBar)

        self.centralWidget = QWidget()
        self.setCentralWidget(self.centralWidget)
        self.layout = QVBoxLayout(self.centralWidget)

        self.comboBox = QComboBox()
        self.comboBox.addItems(sorted(self.cgdManager.cdbs.keys()))
        self.comboBox.currentTextChanged.connect(self.switchCdb)
        self.layout.addWidget(self.comboBox)

        self.tableWidget = QTableWidget()
        self.layout.addWidget(self.tableWidget)

        self.addEntryBtn = QPushButton("Add New Entry")
        self.addEntryBtn.clicked.connect(self.addEntry)
        self.layout.addWidget(self.addEntryBtn)

        self.currentCdbName = self.comboBox.currentText()
        self.loadCdbTable(self.currentCdbName)

    def loadCdbTable(self, cdbName):
        cdb = self.cgdManager.cdbs[cdbName]
        keys = cdb.keys
        try:
            self.tableWidget.itemChanged.disconnect()
        except TypeError:
            pass

        self.tableWidget.clear()
        self.tableWidget.setRowCount(0)
        self.tableWidget.setColumnCount(len(keys) + 1)
        self.tableWidget.setHorizontalHeaderLabels(keys + ["Delete"])
        self.tableWidget.setRowCount(len(cdb.entries))

        for row_idx, entry in enumerate(cdb.entries):
            for col_idx, field in enumerate(entry):
                val = field.value
                ts = field.typeSize
                if ts == 1:
                    val = bool(val[0]) if isinstance(val, (bytes, bytearray)) else bool(val)
                elif ts in (2,3,4):
                    val = int.from_bytes(val, byteorder="little") if isinstance(val, (bytes, bytearray)) else int(val)
                else:
                    val = val.decode("utf-8").rstrip("\x00") if isinstance(val, (bytes, bytearray)) else str(val)
                item = QTableWidgetItem(str(val))
                self.tableWidget.setItem(row_idx, col_idx, item)

            btn = QPushButton("Delete")
            btn.clicked.connect(lambda _, r=row_idx: self.deleteEntry(r))
            self.tableWidget.setCellWidget(row_idx, len(keys), btn)

        self.tableWidget.itemChanged.connect(self.onItemChanged)

    def deleteEntry(self, row_idx):
            cdbName = self.currentCdbName
            res = self.cgdManager.removeEntry(cdbName, row_idx)
            if res["success"]:
                self.loadCdbTable(cdbName)
            else:
                QMessageBox.warning(self, "Error", res["error"])
                
    def onItemChanged(self, item: QTableWidgetItem):
        row = item.row()
        col = item.column()
        cdb = self.cgdManager.cdbs[self.currentCdbName]
        key = cdb.keys[col]
        ts = cdb.typeSizes[col]
        text = item.text().strip()
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
            self.cgdManager.updateCdbEntry(self.currentCdbName, row, key, val)
        except (TypeError, ValueError) as e:
            QMessageBox.warning(self, "Invalid Value", str(e))
            old_value = cdb.entries[row][col].value
            if ts == 1:
                old_value = bool(old_value[0]) if isinstance(old_value,(bytes,bytearray)) else bool(old_value)
            elif ts in (2,3,4):
                old_value = int.from_bytes(old_value, byteorder="little") if isinstance(old_value,(bytes,bytearray)) else int(old_value)
            else:
                old_value = old_value.decode("utf-8").rstrip("\x00") if isinstance(old_value,(bytes,bytearray)) else str(old_value)
            item.setText(str(old_value))

    def switchCdb(self, cdbName):
        try:
            self.tableWidget.itemChanged.disconnect()
        except TypeError:
            pass
        self.currentCdbName = cdbName
        self.loadCdbTable(cdbName)

    def addEntry(self):
        cdbName = self.currentCdbName
        cdb = self.cgdManager.cdbs[cdbName]
        if not cdb.keys or not cdb.typeSizes:
            QMessageBox.warning(self, "Error", "Cannot add entry: schema unknown")
            return
        schema_fields = [EntryDataType(k, ts, "") for k, ts in zip(cdb.keys, cdb.typeSizes)]
        dialog = NewEntryDialog(schema_fields, self)
        if dialog.exec() == QDialog.DialogCode.Accepted:
            new_fields = dialog.getValues()
            if new_fields is None:
                return
            res = self.cgdManager.addEntryTo(cdbName, new_fields)
            if res["success"]:
                self.loadCdbTable(cdbName)
            else:
                QMessageBox.warning(self, "Error", res["error"])

    def openCapsuleManager(self):
        if not hasattr(self, "capsuleWindow") or self.capsuleWindow is None:
            self.capsuleWindow = CapsuleManager(
                self.cgdManager,
                os.path.join(self.cgdManager.iconFolder, "ENG"),
                self.cgdManager.iconFolder
            )
            self.capsuleWindow.destroyed.connect(self.onCapsuleWindowDestroyed)

        self.capsuleWindow.setAttribute(Qt.WidgetAttribute.WA_DeleteOnClose)
        self.capsuleWindow.show()
        self.capsuleWindow.raise_()
        self.capsuleWindow.activateWindow()

    def onCapsuleWindowDestroyed(self):
        self.capsuleWindow = None

    def exportCgdDip(self):
        if not hasattr(self, "cgdManager"):
            QMessageBox.warning(self, "Error", "CGD Manager not loaded!")
            return

        password, ok = QInputDialog.getText(self, "Archive Password", "Enter password for the DIP archive:")
        if not ok:
            return

        result = self.cgdManager.createDipFromCdbs(password)
        if result.get("success"):
            QMessageBox.information(self, "Success", result.get("message"))
        else:
            QMessageBox.critical(self, "Error", result.get("error"))

