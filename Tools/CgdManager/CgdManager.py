from PySide6.QtWidgets import (
    QMainWindow, QWidget, QVBoxLayout, QInputDialog, QComboBox,
    QPushButton, QMessageBox,
    QMenuBar, QDialog, QLineEdit, QAbstractItemView, QHBoxLayout,
    QTableView
)
from PySide6.QtGui import QAction
from PySide6.QtCore import Qt
from CapsuleEditor import CapsuleManager
from ShopManager import ShopManager
import os
from AddNewDialog import NewEntryDialog
from Utils import EntryDataType, decodeValue, showToast
from EditSettingsDialog import SettingsDialog
from CgdTableModel import CdbTableModel

class CgdEditor(QMainWindow):
    def __init__(self, cgdManager):
        super().__init__()
        self.cgdManager = cgdManager
        self.setWindowTitle("CGD Editor - ToyBattlesHQ")
        self.resize(1200, 700)
        self.menuBar = QMenuBar()
        self.setMenuBar(self.menuBar)

        fileMenu = self.menuBar.addMenu("File")
        exportAction = QAction("Export cgd.dip", self)
        exportAction.triggered.connect(self.exportCgdDip)
        fileMenu.addAction(exportAction)

        settingsAction = QAction("Settings", self)
        settingsAction.triggered.connect(self.openSettings)
        fileMenu.addAction(settingsAction)

        managersMenu = self.menuBar.addMenu("Managers")
        capsuleManagerAction = QAction("Capsule Manager", self)
        capsuleManagerAction.triggered.connect(self.openCapsuleManager)
        managersMenu.addAction(capsuleManagerAction)

        shopManagerAction = QAction("Shop Manager", self)
        shopManagerAction.triggered.connect(self.openShopManager)
        managersMenu.addAction(shopManagerAction)

        self.centralWidget = QWidget()
        self.setCentralWidget(self.centralWidget)
        self.layout = QVBoxLayout(self.centralWidget)

        self.comboBox = QComboBox()
        self.comboBox.addItems(sorted(self.cgdManager.cdbs.keys()))
        self.comboBox.currentTextChanged.connect(self.switchCdb)
        self.layout.addWidget(self.comboBox)

        self.tableWidget = QTableView()
        self.layout.addWidget(self.tableWidget)

        self.addEntryBtn = QPushButton("Add New Entry")
        self.addEntryBtn.clicked.connect(self.addEntry)
        self.layout.addWidget(self.addEntryBtn)

        search_layout = QHBoxLayout()
        self.searchKeyTextbox = QLineEdit()
        self.searchKeyTextbox.setPlaceholderText("Enter key to search (column name)")
        search_layout.addWidget(self.searchKeyTextbox)
        self.searchValueTextbox = QLineEdit()
        self.searchValueTextbox.setPlaceholderText("Enter value to search")
        search_layout.addWidget(self.searchValueTextbox)
        searchBtn = QPushButton("Search")
        searchBtn.clicked.connect(self.searchValue)
        search_layout.addWidget(searchBtn)
        self.layout.addLayout(search_layout)

        self.currentCdbName = self.comboBox.currentText()
        self.loadCdbTable(self.currentCdbName)

    def loadCdbTable(self, cdbName):
        cdb = self.cgdManager.cdbs[cdbName]
        self.model = CdbTableModel(cdb, self.cgdManager, parentWidget=self)
        self.tableWidget.setModel(self.model)
        self.tableWidget.setSelectionBehavior(QAbstractItemView.SelectItems)
        self.tableWidget.setSelectionMode(QAbstractItemView.SingleSelection)

        try:
            self.tableWidget.clicked.disconnect()
        except TypeError:
            pass 
        self.tableWidget.clicked.connect(self.onCellClicked)

    def onCellClicked(self, index):
        if index.column() == len(self.cgdManager.cdbs[self.currentCdbName].keys):
            self.deleteEntry(index.row())

    def switchCdb(self, cdbName):
        self.currentCdbName = cdbName
        self.loadCdbTable(cdbName)

    def openSettings(self):
        dlg = SettingsDialog(self, self.cgdManager.iconFolder)
        if dlg.exec() == QDialog.Accepted:
            new_path = dlg.getValue()
            if new_path:
                self.cgdManager.iconFolder = new_path

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
                showToast(self, "New entry added successfully")
                self.loadCdbTable(cdbName)
            else:
                QMessageBox.warning(self, "Error", res["error"])

    def deleteEntry(self, row_idx):
        cdbName = self.currentCdbName
        res = self.cgdManager.removeEntry(cdbName, row_idx)
        if res["success"]:
            showToast(self, "Entry deleted successfully")
            self.loadCdbTable(cdbName)
        else:
            QMessageBox.warning(self, "Error", res["error"])

    def onCapsuleManagerDestroyed(self):
        self.capsuleWindow = None

    def openCapsuleManager(self):
        if not hasattr(self, "capsuleWindow") or self.capsuleWindow is None:
            self.capsuleWindow = CapsuleManager(self.cgdManager,
                os.path.join(self.cgdManager.iconFolder, "ENG"), self.cgdManager.iconFolder)
            self.capsuleWindow.destroyed.connect(self.onCapsuleManagerDestroyed)
        self.capsuleWindow.setAttribute(Qt.WidgetAttribute.WA_DeleteOnClose)
        self.capsuleWindow.show()
        self.capsuleWindow.raise_()
        self.capsuleWindow.activateWindow()

    def openShopManager(self):
        if not hasattr(self, "shopWindow") or self.shopWindow is None:
            self.shopWindow = ShopManager(
                self.cgdManager,
                os.path.join(self.cgdManager.iconFolder, "ENG"),
                self.cgdManager.iconFolder
            )
            self.shopWindow.destroyed.connect(self.onShopManagerDestroyed)
        self.shopWindow.setAttribute(Qt.WidgetAttribute.WA_DeleteOnClose)
        self.shopWindow.show()
        self.shopWindow.raise_()
        self.shopWindow.activateWindow()

    def onShopManagerDestroyed(self):
        self.shopWindow = None

    def exportCgdDip(self):
        if not hasattr(self, "cgdManager"):
            QMessageBox.warning(self, "Error", "CGD Manager not loaded!")
            return
        password, ok = QInputDialog.getText(self, "Archive Password", "Enter password for the DIP archive:")
        if not ok:
            return
        result = self.cgdManager.createDipFromCdbs(password)
        if result.get("success"):
            showToast(self, "cgd.dip archive exported successfully")
        else:
            QMessageBox.critical(self, "Error", result.get("error"))

    def searchValue(self):
        key_to_search = self.searchKeyTextbox.text().strip()
        value_to_search = self.searchValueTextbox.text().strip()
        if not key_to_search or not value_to_search:
            QMessageBox.warning(self, "Search Error", "Enter both key and value to start searching")
            return
        if key_to_search not in self.cgdManager.cdbs[self.currentCdbName].keys:
            QMessageBox.warning(self, "Search Error", f"Key '{key_to_search}' not found in current CDB file!")
            return

        cdb = self.cgdManager.cdbs[self.currentCdbName]
        col_index = cdb.keys.index(key_to_search)
        found = False

        for row, entry in enumerate(cdb.entries):
            field = entry[col_index]
            val = str(decodeValue(field.value, field.typeSize))
            if val == value_to_search:
                self.tableWidget.selectRow(row)
                index = self.tableWidget.model().index(row, col_index)
                self.tableWidget.scrollTo(index, QAbstractItemView.PositionAtCenter)
                found = True
                break

        if not found:
            QMessageBox.information(self, "Not Found", f"Value '{value_to_search}' for key '{key_to_search}' not found!")
