import os
from functools import partial
from CgdParser import CgdManager
from CgdManager import CgdEditor

from PySide6.QtWidgets import (
    QApplication,
    QComboBox,
    QLineEdit,
    QVBoxLayout,
    QWidget,
    QMainWindow,
    QPushButton,
    QFileDialog,
    QHBoxLayout,
    QTextEdit,
    QMessageBox,
)
from PySide6.QtCore import QObject, QThread, Signal, Slot

class ParserWorker(QObject):
    logSignal = Signal(str)
    finishedSignal = Signal(dict)

    def __init__(self, manager, path, password="", mode="cgd"):
        super().__init__()
        self.manager = manager
        self.path = path
        self.password = password
        self.mode = mode 

    def run(self):
        if self.mode == "cgd":
            result = self.manager.parseCgdArchive(
                self.path, self.password, log_callback=self.logSignal.emit
            )
        elif self.mode == "existing":
            result = self.manager.parseJsonFiles(self.path, log_callback=self.logSignal.emit)
        else:
            result = {"success": False, "error": f"Unknown import mode: {self.mode}"}
        self.finishedSignal.emit(result)


class ImportCgdDialog(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Importer")
        self.index = 0
        self.cgdManager = any
        self.iconFolder = ""

        main_layout = QVBoxLayout()

        self.comboBox = QComboBox()
        self.comboBox.addItems(["cgd.dip archive", "select existing folder (must contain cdbs and jsons subfolders)"])
        self.comboBox.currentIndexChanged.connect(self.index_changed)
        main_layout.addWidget(self.comboBox)

        self.passwordTextbox = QLineEdit()
        self.passwordTextbox.setPlaceholderText("Enter archive password")
        main_layout.addWidget(self.passwordTextbox)

        path_layout = QHBoxLayout()
        self.pathTextbox = QLineEdit()
        self.pathTextbox.setPlaceholderText("Enter full path")
        path_layout.addWidget(self.pathTextbox)
        btnBrowseInput = QPushButton("...")
        btnBrowseInput.setToolTip("Select input directory")
        btnBrowseInput.clicked.connect(partial(self.selectInputPath, self.pathTextbox))
        path_layout.addWidget(btnBrowseInput)
        main_layout.addLayout(path_layout)  

        icon_layout = QHBoxLayout()
        self.iconTextbox = QLineEdit()
        self.iconTextbox.setPlaceholderText("Enter path to unpacked UI/icon folder")
        icon_layout.addWidget(self.iconTextbox)
        btnBrowseIcon = QPushButton("...")
        btnBrowseIcon.setToolTip("Select UI/Icon folder")
        btnBrowseIcon.clicked.connect(partial(self.selectDirectory, self.iconTextbox))
        icon_layout.addWidget(btnBrowseIcon)
        main_layout.addLayout(icon_layout)

        outputLayout = QHBoxLayout()
        self.outputPathbox = QLineEdit()
        self.outputPathbox.setPlaceholderText("Enter output path")
        outputLayout.addWidget(self.outputPathbox)
        self.btnBrowseOutput = QPushButton("...")
        self.btnBrowseOutput.setToolTip("Select output directory")
        self.btnBrowseOutput.clicked.connect(partial(self.selectDirectory, self.outputPathbox))
        outputLayout.addWidget(self.btnBrowseOutput)
        main_layout.addLayout(outputLayout)  

        importButton = QPushButton("START IMPORTING")
        importButton.clicked.connect(self.startImport)
        main_layout.addWidget(importButton)  

        self.importLoadInfo = QTextEdit()
        main_layout.addWidget(self.importLoadInfo)

        central_widget = QWidget()
        central_widget.setLayout(main_layout)
        self.setCentralWidget(central_widget)
        self.setMinimumHeight(550)
        self.setMinimumWidth(600)

    def selectInputPath(self, targetLineEdit):
        file_path = ""
        if self.index == 0:
            file_path, _ = QFileDialog.getOpenFileName(self,
                "Select CGD archive", "",
                "All Files (*)"
            )
        else:
             file_path = QFileDialog.getExistingDirectory(self, "Select Directory")
        if file_path:
            targetLineEdit.setText(file_path)

    def index_changed(self, index):
        self.passwordTextbox.setVisible(index == 0)
        self.outputPathbox.setVisible(False)
        self.btnBrowseOutput.setVisible(False)
        self.index = index

    def selectDirectory(self, targetLineEdit):
        path = QFileDialog.getExistingDirectory(self, "Select Directory")
        if path:
            targetLineEdit.setText(path)

    def startImport(self):
        idx = self.comboBox.currentIndex()

        if idx == 0 and not self.passwordTextbox.text().strip():
            QMessageBox.warning(self, "Missing Password", "Enter the cgd.dip archive password before importing!")
            return
        elif idx == 1 and not self.pathTextbox.text().strip():
            QMessageBox.warning(self, "Missing Required Field", "Select the existing project folder before importing!")
            return
        elif not self.iconTextbox.text().strip():
            QMessageBox.warning(self, "Missing Required Field", "Specify the path to the unpacked UI/icon folder!")
            return

        self.iconFolder = self.iconTextbox.text().strip()

        mode = {0: "cgd", 1: "existing"}[self.comboBox.currentIndex()]
        path = self.pathTextbox.text().strip()
        password = self.passwordTextbox.text().strip() if mode == "cgd" else ""

        if mode == "existing":
            output_dir = path
            temp_dir = os.path.join(path, "temp")
        else:
            output_dir = self.outputPathbox.text().strip() if self.outputPathbox.text().strip() else path
            temp_dir = os.path.join(output_dir, "temp")

        self.cgdManager = CgdManager(temp_dir)
        self.cgdManager.iconFolder = self.iconFolder
        if mode == "existing":
            self.cgdManager.json_dir = os.path.join(path, "jsons")
            self.cgdManager.cdb_dir = os.path.join(path, "cdbs")

        self.worker = ParserWorker(self.cgdManager, path, password, mode)
        self.thread = QThread()
        self.worker.moveToThread(self.thread)
        self.worker.logSignal.connect(self.appendLog)
        self.worker.finishedSignal.connect(self.importFinished)
        self.thread.started.connect(self.worker.run)
        self.thread.start()


    def importFinished(self, result):
        if result["success"]:
            self.appendLog(f'<span style="color: green;"><b>Import Complete:</b> {result.get("message","")}</span>')
            self.cgdManager.deleteTempFolder()
            self.thread.quit()
            self.thread.wait()
            self.editorWindow = CgdEditor(self.cgdManager)
            self.editorWindow.show()
            self.close()
        else:
            errors_text = "<br>".join(result.get("errors", []))
            self.appendLog(f'<span style="color: red;"><b>Import Errors:</b><br>{errors_text}</span>')
            self.thread.quit()
            self.thread.wait()

    @Slot(str)
    def appendLog(self, msg):
        self.importLoadInfo.append(msg)
        self.importLoadInfo.verticalScrollBar().setValue(
            self.importLoadInfo.verticalScrollBar().maximum()
        )

app = QApplication([])
window = ImportCgdDialog()
window.show()
app.exec()
