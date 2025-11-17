from PySide6.QtWidgets import (
    QVBoxLayout, QPushButton, QFileDialog, QDialog, QDialogButtonBox, QLineEdit, QLabel
)

class SettingsDialog(QDialog):
    def __init__(self, parent, current_path):
        super().__init__(parent)
        self.setWindowTitle("Settings")
        layout = QVBoxLayout(self)

        self.pathBox = QLineEdit()
        self.pathBox.setText(current_path)
        layout.addWidget(QLabel("Unpacked UI/Icon Folder"))
        layout.addWidget(self.pathBox)

        browseBtn = QPushButton("Browse")
        browseBtn.clicked.connect(self.browse)
        layout.addWidget(browseBtn)

        btns = QDialogButtonBox(QDialogButtonBox.Ok | QDialogButtonBox.Cancel)
        btns.accepted.connect(self.accept)
        btns.rejected.connect(self.reject)
        layout.addWidget(btns)

    def browse(self):
        d = QFileDialog.getExistingDirectory(self, "Select Icon Folder")
        if d:
            self.pathBox.setText(d)

    def getValue(self):
        return self.pathBox.text().strip()