from io import BytesIO
import os
from PySide6.QtGui import QPixmap
from PySide6.QtCore import Qt, QThread, Signal
from PIL import Image

class IconLoaderThread(QThread):
    iconLoaded = Signal(int, QPixmap) 

    def __init__(self, items_to_load, item_icon_path):
        super().__init__()
        self.items_to_load = items_to_load 
        self.item_icon_path = item_icon_path

    def run(self):
        for icon_id, icon_entry in self.items_to_load:
            pixmap = self.loadPixmap(icon_entry)
            self.iconLoaded.emit(icon_id, pixmap)

    def loadPixmap(self, icon_entry):
        filename = icon_entry.get("filename")
        offset = icon_entry.get("offset", 0)
        width = icon_entry.get("width", 64)
        height = icon_entry.get("height", 64)
        path = os.path.join(self.item_icon_path, filename)

        if not os.path.exists(path):
            pixmap = QPixmap(width, height)
            pixmap.fill(Qt.darkGray)
            return pixmap

        if path.lower().endswith(".dds"):
            try:
                img = Image.open(path)
                buffer = BytesIO()
                img.save(buffer, format="PNG")
                pixmap = QPixmap()
                pixmap.loadFromData(buffer.getvalue(), "PNG")
            except Exception:
                pixmap = QPixmap(width, height)
                pixmap.fill(Qt.darkGray)
                return pixmap
        else:
            pixmap = QPixmap(path)

        if pixmap.isNull():
            pixmap = QPixmap(width, height)
            pixmap.fill(Qt.darkGray)
            return pixmap

        icons_per_row = pixmap.width() // width
        col = offset % icons_per_row
        row = offset // icons_per_row
        x = col * width
        y = row * height
        return pixmap.copy(x, y, width, height)
    
def defaultPixmap():
    pixmap = QPixmap(64, 64)
    pixmap.fill(Qt.darkGray)
    return pixmap
