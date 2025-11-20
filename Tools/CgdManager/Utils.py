from PySide6.QtWidgets import QMessageBox, QLabel, QWidget, QGraphicsDropShadowEffect
from PySide6.QtGui import QPixmap, QFont, QColor
from PySide6.QtCore import Qt, QEasingCurve, QPropertyAnimation, QTimer

from io import BytesIO
import os 
from PIL import Image
from dataclasses import dataclass
from typing import Any

@dataclass
class EntryDataType:
    key: str
    typeSize: int
    value: Any

def showMessage(icon_type: QMessageBox.Icon, title: str, text: str, parent):
    msg = QMessageBox(parent)
    msg.setIcon(icon_type)
    msg.setWindowTitle(title)
    msg.setText(text)
    msg.exec()

def decodeValue(val, ts):
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

def getFieldValue(entry, key, default=None):
    field = next((f for f in entry if f.key == key), None)
    if field:
        return decodeValue(field.value, field.typeSize)
    return default

def parseFieldValue(key: str, ts: int, text: str):
    text = text.strip()
    if ts == 1:
        if text.lower() in ("1", "true", "yes"):
            return True
        elif text.lower() in ("0", "false", "no"):
            return False
        else:
            raise TypeError(f"Key '{key}' expects boolean value")
    elif ts in (2, 3, 4):
        try:
            return int(text)
        except ValueError:
            raise TypeError(f"Key '{key}' expects integer value")
    else:
        if text.isdigit():
            raise TypeError(f"Key '{key}' expects string value")
        return text

def defaultPixmap(): 
    pixmap = QPixmap(64, 64) 
    pixmap.fill(Qt.darkGray) 
    return pixmap

def loadPixmap(icon_entry, item_icon_path):
    filename = icon_entry.get("filename")
    offset = icon_entry.get("offset", 0)
    width = icon_entry.get("width", 64)
    height = icon_entry.get("height", 64)
    path = os.path.join(item_icon_path, filename)

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

def findEntryIndexById(cgdManager, cdb_name: str, entry) -> int | None:
    cdb = cgdManager.cdbs.get(cdb_name)
    if not cdb:
        return None
    entry_gi_id = getFieldValue(entry, "gi_id")
    return next((i for i, e in enumerate(cdb.entries) if getFieldValue(e, "gi_id") == entry_gi_id), None)

def showToast(instance, message, duration=2000):
    toast = QLabel(message)
    toast.setWindowFlags(Qt.ToolTip | Qt.WindowStaysOnTopHint)
    toast.setAttribute(Qt.WA_TranslucentBackground)

    toast.setStyleSheet("""
        background-color: rgba(50, 180, 50, 220);
        color: white;
        padding: 10px 16px;
        border-radius: 8px;
    """)
    toast.setFont(QFont("Segoe UI", 10, QFont.Weight.Bold))

    shadow = QGraphicsDropShadowEffect()
    shadow.setBlurRadius(12)
    shadow.setColor(QColor(0, 0, 0, 160))
    shadow.setOffset(0, 0)
    toast.setGraphicsEffect(shadow)

    toast.adjustSize()

    win_geo = instance.frameGeometry() 
    screen_top_right = win_geo.topRight()
    x = screen_top_right.x() - toast.width() - 20
    y = screen_top_right.y() + 20
    toast.move(x, y)

    toast.setWindowOpacity(0.0)
    toast.show()

    fade_in = QPropertyAnimation(toast, b"windowOpacity")
    fade_in.setDuration(300)
    fade_in.setStartValue(0.0)
    fade_in.setEndValue(1.0)
    fade_in.setEasingCurve(QEasingCurve.InOutQuad)
    toast.anim_in = fade_in
    fade_in.start()

    def start_fade_out():
        fade_out = QPropertyAnimation(toast, b"windowOpacity")
        fade_out.setDuration(800)
        fade_out.setStartValue(1.0)
        fade_out.setEndValue(0.0)
        fade_out.setEasingCurve(QEasingCurve.InOutQuad)
        toast.anim_out = fade_out

        def cleanup():
            toast.deleteLater()

        fade_out.finished.connect(cleanup)
        fade_out.start()

    QTimer.singleShot(duration, start_fade_out)
