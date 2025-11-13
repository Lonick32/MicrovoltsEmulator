import os
from dataclasses import dataclass
import json
from typing import Any

@dataclass
class EntryDataType:
    key: str
    typeSize: int
    value: Any

class Cdb:
    def __init__(self, filePath):
        self.filePath = filePath
        self.fileName = os.path.splitext(os.path.basename(filePath))[0]
        self.entries: list[list[EntryDataType]] = []
        self.keys: list[str] = []
        self.typeSizes: list[int] = []

    def parseCdb(self):
        entrySize = 0
        fileSize = os.path.getsize(self.filePath)
        with open(self.filePath, "rb") as f:
            totalKeys = int.from_bytes(f.read(4), byteorder="little")
            keys = []
            for _ in range(totalKeys):
                keyBytes = f.read(30)
                key_str = keyBytes.rstrip(b'\x00').decode("utf-8", errors="ignore")
                keys.append(key_str)
            self.keys = keys
            typeSizes = []
            for _ in range(totalKeys):
                size = int.from_bytes(f.read(4), byteorder="little")
                typeSizes.append(size)
                entrySize += size
            self.typeSizes = typeSizes

            if entrySize > 0:
                num_entries = (fileSize - 4 - (totalKeys * 34)) // entrySize
                for _ in range(num_entries):
                    entry_fields = []
                    for key, size in zip(keys, typeSizes):
                        raw = f.read(size)
                        if size == 1:
                            value = bool(raw[0])
                        elif size in (2,3,4):
                            value = int.from_bytes(raw, byteorder="little", signed=False)
                        else:
                            value = raw.rstrip(b'\x00').decode("utf-8", errors="ignore")
                        entry_fields.append(EntryDataType(key=key, typeSize=size, value=value))
                    self.entries.append(entry_fields)

    def parseJson(self):
        with open(self.filePath, "r", encoding="utf-8") as f:
            json_data = json.load(f)
        header = json_data.get("_header")
        if header:
            self.keys = header.get("keys", [])
            self.typeSizes = header.get("typeSizes", [])
        entries_list = json_data.get("entries", [])
        for entry_obj in entries_list:
            entry_fields = []
            for key, field_data in entry_obj.items():
                ts = field_data["typeSize"]
                value = field_data["value"]
                if ts == 1:
                    py_value = bool(value)
                elif ts in (2,3,4):
                    py_value = int(value)
                else:
                    py_value = str(value)
                entry_fields.append(EntryDataType(key=key, typeSize=ts, value=py_value))
            self.entries.append(entry_fields)

    def toJson(self, outputPath):
        json_data = {
            "_header": {"keys": self.keys, "typeSizes": self.typeSizes},
            "entries": []
        }
        for entry_fields in self.entries:
            entry_obj = {}
            for field in entry_fields:
                val = field.value
                ts = field.typeSize
                entry_obj[field.key] = {"typeSize": ts, "value": val}
            json_data["entries"].append(entry_obj)
        output_file = os.path.join(outputPath, f"{self.fileName}.json")
        with open(output_file, "w", encoding="utf-8") as f:
            json.dump(json_data, f, indent=4, ensure_ascii=False)

    def toCdb(self, outputPath):
        if not self.keys or not self.typeSizes:
            raise ValueError("Can't write CDB: header info missing")
        totalKeys = len(self.keys)
        output_file = os.path.join(outputPath, f"{self.fileName}.cdb")
        with open(output_file, "wb") as f:
            f.write(totalKeys.to_bytes(4, byteorder="little"))
            for key in self.keys:
                f.write(key.encode("utf-8")[:30].ljust(30, b'\x00'))
            for size in self.typeSizes:
                f.write(size.to_bytes(4, byteorder="little"))
            for entry_fields in self.entries:
                for field in entry_fields:
                    ts = field.typeSize
                    val = field.value
                    if ts == 1:
                        val_bytes = bytes([int(val)])
                    elif ts in (2,3,4):
                        val_bytes = int(val).to_bytes(ts, byteorder="little", signed=False)
                    else:
                        val_bytes = str(val).encode("utf-8").ljust(ts, b'\x00')
                    f.write(val_bytes[:ts])

    def updateValue(self, entry_index, key, newValue):
        if entry_index >= len(self.entries):
            raise IndexError("Entry index out of range")
        fields = self.entries[entry_index]
        for field in fields:
            if field.key == key:
                ts = field.typeSize
                if ts == 1:
                    if not isinstance(newValue, bool):
                        raise TypeError(f"Expected bool for key '{key}' but got {type(newValue).__name__}")
                    field.value = newValue
                elif ts in (2,3,4):
                    if not isinstance(newValue, int):
                        raise TypeError(f"Expected int for key '{key}' but got {type(newValue).__name__}")
                    field.value = newValue
                else:
                    if not isinstance(newValue, str):
                        raise TypeError(f"Expected str for key '{key}' but got {type(newValue).__name__}")
                    field.value = newValue
                return
        raise ValueError(f"Key '{key}' doesn't exist in entry {entry_index}")

    def addEntries(self, entries: list[list[EntryDataType]]):
        for entry in entries:
            self.addEntry(entry)  

    def deleteEntries(self, indices: list[int]):
        for index in sorted(indices, reverse=True):
            self.deleteEntry(index)
            
    def deleteEntry(self, entry_index):
        if entry_index >= len(self.entries):
            raise IndexError("Entry index out of range")
        self.entries.pop(entry_index)

    def addEntry(self, entry: list[EntryDataType]):
        if not hasattr(self, "keys") or not self.keys:
            raise ValueError("CDB schema not known")

        schema_fields = [EntryDataType(k, ts, None) for k, ts in zip(self.keys, self.typeSizes)]
        if len(entry) != len(schema_fields):
            raise ValueError("Entry has wrong number of fields")

        for new_field, schema_field in zip(entry, schema_fields):
            if new_field.key != schema_field.key:
                raise ValueError(f"Key mismatch: expects '{schema_field.key}' but got '{new_field.key}'")
            if new_field.typeSize != schema_field.typeSize:
                raise ValueError(f"TypeSize mismatch for key '{new_field.key}': expects {schema_field.typeSize} but got {new_field.typeSize}")

            ts = new_field.typeSize
            val = new_field.value
            if ts == 1:
                if not isinstance(val, bool):
                    raise TypeError(f"Key '{new_field.key}' expects a bool value")
            elif ts in (2,3,4):
                if not isinstance(val, int):
                    raise TypeError(f"Key '{new_field.key}' expects an int value")
            else:
                if not isinstance(val, str):
                    raise TypeError(f"Key '{new_field.key}' expects a str value")

        self.entries.append(entry)

    def getEntries(self):
        return self.entries
