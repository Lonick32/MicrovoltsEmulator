from PySide6.QtCore import Qt, QAbstractTableModel, QModelIndex


class CdbTableModel(QAbstractTableModel):
    def __init__(self, cdb, cgdManager):
        super().__init__()
        self.cdb = cdb
        self.cgdManager = cgdManager

    def rowCount(self, parent=QModelIndex()):
        return len(self.cdb.entries)

    def columnCount(self, parent=QModelIndex()):
        return len(self.cdb.keys) + 1

    def data(self, index, role=Qt.DisplayRole):
        if not index.isValid():
            return None
        if role == Qt.DisplayRole:
            if index.column() == len(self.cdb.keys):
                return "Delete"
            field = self.cdb.entries[index.row()][index.column()]
            val = field.value
            ts = field.typeSize
            if ts == 1:
                return str(bool(val[0]) if isinstance(val, (bytes, bytearray)) else bool(val))
            elif ts in (2,3,4):
                return str(int.from_bytes(val, "little") if isinstance(val,(bytes,bytearray)) else int(val))
            else:
                return val.decode("utf-8").rstrip("\x00") if isinstance(val,(bytes,bytearray)) else str(val)
        return None

    def setData(self, index, value, role=Qt.EditRole):
        if not index.isValid() or index.column() == len(self.cdb.keys):
            return False
        row = index.row()
        col = index.column()
        ts = self.cdb.typeSizes[col]
        key = self.cdb.keys[col]
        try:
            if ts == 1:
                if str(value).lower() in ("1","true","yes"):
                    val = True
                elif str(value).lower() in ("0","false","no"):
                    val = False
                else:
                    raise TypeError(f"Key '{key}' expects boolean value")
            elif ts in (2,3,4):
                val = int(value)
            else:
                if str(value).isdigit():
                    raise TypeError(f"Key '{key}' expects string value")
                val = str(value)
            self.cdb.entries[row][col].value = val
            self.cgdManager.saveCdbOutputs(self.cdb)
            self.dataChanged.emit(index, index, [Qt.DisplayRole])
            return True
        except (TypeError, ValueError):
            return False

    def flags(self, index):
        if not index.isValid():
            return Qt.ItemIsEnabled
        if index.column() == len(self.cdb.keys):
            return Qt.ItemIsEnabled | Qt.ItemIsSelectable
        return Qt.ItemIsEnabled | Qt.ItemIsSelectable | Qt.ItemIsEditable

    def headerData(self, section, orientation, role=Qt.DisplayRole):
        if role == Qt.DisplayRole and orientation == Qt.Horizontal:
            if section == len(self.cdb.keys):
                return "Delete"
            return self.cdb.keys[section]
        return None