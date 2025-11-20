from collections import OrderedDict
import os
import shutil
import zipfile
from CdbParser import Cdb, EntryDataType
import subprocess
import sys

class CgdManager:
    def __init__(self, tempPath):
        self.cdbs: OrderedDict[str, Cdb] = OrderedDict()
        self.tempPath = tempPath
        self.outputRoot = os.path.join(os.path.dirname(tempPath))
        self.iconFolder = ""

        self.json_dir = os.path.join(self.outputRoot, "jsons")
        self.cdb_dir = os.path.join(self.outputRoot, "cdbs")

        os.makedirs(self.tempPath, exist_ok=True)
        os.makedirs(self.outputRoot, exist_ok=True)
        os.makedirs(self.json_dir, exist_ok=True)
        os.makedirs(self.cdb_dir, exist_ok=True)

    def deleteTempFolder(self, log_callback=None):
        try:
            shutil.rmtree(self.tempPath)
        except Exception as e:
            if log_callback:
                log_callback(f"Exception: Failed to delete temp folder {self.tempPath}: {e}")
            else:
                print(f"Exception: Failed to delete temp folder {self.tempPath}: {e}")

    def parseCgdArchive(self, filePath, archivePassword, log_callback=None):
        errors = []

        try:
            with zipfile.ZipFile(filePath, 'r') as zip_ref:
                all_names = [
                    os.path.normpath(zi.filename)
                    for zi in zip_ref.infolist()
                    if not zi.filename.endswith("/")
                ]

                common_root = os.path.commonpath(all_names) if all_names else ""
                if os.path.isfile(common_root):
                    common_root = os.path.dirname(common_root)
                for zip_info in zip_ref.infolist():
                    normalized = os.path.normpath(zip_info.filename)
                    rel_path = os.path.relpath(normalized, common_root)

                    if rel_path.startswith(".."):
                        rel_path = os.path.basename(normalized)

                    zip_info.filename = rel_path

                    try:
                        zip_ref.extract(zip_info, path=self.tempPath, pwd=archivePassword.encode())
                        if log_callback:
                            log_callback(f"Extracted: {rel_path}")
                    except Exception as e:
                        errors.append(f"Error extracting {normalized}: {e}")

        except Exception as e:
            return {
                "success": False,
                "errors": [f"Archive read error: {e}"]
            }

        for root, _, files in os.walk(self.tempPath):
            rel_path = os.path.relpath(root, self.tempPath)
            json_output_dir = os.path.join(self.json_dir, rel_path)
            cdb_output_dir = os.path.join(self.cdb_dir, rel_path)

            os.makedirs(json_output_dir, exist_ok=True)
            os.makedirs(cdb_output_dir, exist_ok=True)

            for filename in files:
                full_path = os.path.join(root, filename)

                if filename.lower().endswith(".cdb"):
                    if log_callback:
                        log_callback(f"Parsing: {full_path}")

                    try:
                        cdb = Cdb(full_path)
                        cdb.relativePath = rel_path
                        cdb.parseCdb()
                        self.cdbs[cdb.fileName] = cdb

                        cdb.toJson(json_output_dir)
                        cdb.toCdb(cdb_output_dir)

                    except Exception as e:
                        errors.append(f"Error parsing {full_path}: {e}")

                else:
                    if log_callback:
                        log_callback(f"Copying non-CDB file: {filename}")

                    try:
                        shutil.copy2(full_path, os.path.join(json_output_dir, filename))
                        shutil.copy2(full_path, os.path.join(cdb_output_dir, filename))
                    except Exception as e:
                        errors.append(f"Error copying {full_path}: {e}")

        if errors:
            return {
                "success": False,
                "errorCount": len(errors),
                "errors": errors
            }

        return {"success": True, "message": "All CDB files processed and exported successfully"}

    def fromImpl(self, path: str, extension: str, log_callback=None):
        errors = []

        extension = extension.lower()
        if extension not in (".json", ".cdb"):
            return {"success": False, "error": f"Wrong file extension: {extension}"}

        for root, _, files in os.walk(path):
            for filename in files:
                full_path = os.path.join(root, filename)

                if filename.lower().endswith(extension):
                    log_callback(f"Parsing: {full_path}")
                    try:
                        cdb = Cdb(full_path)
                        cdb.relativePath = os.path.relpath(os.path.dirname(full_path), path)

                        if extension == ".json":
                            cdb.parseJson()
                        elif extension == ".cdb":
                            cdb.parseCdb()

                        self.cdbs[cdb.fileName] = cdb

                    except Exception as e:
                        errors.append(f"Error parsing {full_path}: {e}")

        if errors:
            return {"success": False, "errorCount": len(errors), "errors": errors}
        else:
            return {"success": True, "message": f"All '{extension}' files loaded successfully"}

    def parseJsonFiles(self, path: str, log_callback=None):
        return self.fromImpl(path, ".json", log_callback)

    def parseCdbFiles(self, path: str, log_callback=None):
        return self.fromImpl(path, ".cdb", log_callback)

    def saveCdbOutputs(self, cdb: Cdb, log_callback=None):
        rel_path = getattr(cdb, "relativePath", "").replace("\\", "/")
        rel_path = rel_path.lstrip("/")

        if rel_path.startswith("jsons/"):
            rel_path = rel_path[6:]  
        elif rel_path.startswith("cdbs/"):
            rel_path = rel_path[5:]

        json_output_dir = os.path.join(self.json_dir, rel_path)
        cdb_output_dir = os.path.join(self.cdb_dir, rel_path)
        os.makedirs(json_output_dir, exist_ok=True)
        os.makedirs(cdb_output_dir, exist_ok=True)

        if log_callback:
            log_callback(f"Exporting '{cdb.fileName}' to JSON...")
        cdb.toJson(json_output_dir)

        if log_callback:
            log_callback(f"Exporting '{cdb.fileName}' to CDB...")
        cdb.toCdb(cdb_output_dir)

    def updateCdbEntry(self, cdbFileName: str, entryNumber, entryKey: str, newValue):
        cdb = self.cdbs.get(cdbFileName)
        if cdb is None:
            return {"success": False, "error": f"No CDB file named '{cdbFileName}' found"}

        try:
            cdb.updateValue(entryNumber, entryKey, newValue)
            self.saveCdbOutputs(cdb)
            return {
                "success": True,
                "message": f"Updated {cdbFileName} → entry {entryNumber}, key '{entryKey}' = {newValue}"
            }

        except Exception as e:
            return {"success": False, "error": f"Unexpected error while updating {cdbFileName}: {e}"}

    def addEntryTo(self, cdbFileName: str, entry: list[EntryDataType]):
        cdb = self.cdbs.get(cdbFileName)
        if cdb is None:
            return {"success": False, "error": f"No CDB file named '{cdbFileName}' found"}
        
        try:
            cdb.addEntry(entry)
            self.saveCdbOutputs(cdb)
            return {
                "success": True,
                "message": f"Added entry to {cdbFileName}"
            }

        except Exception as e:
            return {"success": False, "error": f"Error while adding new entry to {cdbFileName}: {e}"}

    def removeEntry(self, cdbFileName:str, entryNumber):
        cdb = self.cdbs.get(cdbFileName)
        if cdb is None:
            return {"success": False, "error": f"No CDB file named '{cdbFileName}' found"}
        
        try:
            cdb.deleteEntry(entryNumber)
            self.saveCdbOutputs(cdb)
            return {
                "success": True,
                "message": f"Removed entry from {cdbFileName}"
            }

        except Exception as e:
            return {"success": False, "error": f"Error while removing entry from {cdbFileName}: {e}"}
    
    def get_dipmaker_path(self):
        if getattr(sys, 'frozen', False):
            base_path = sys._MEIPASS
        else:
            base_path = os.path.dirname(os.path.abspath(__file__))
        return os.path.join(base_path, "DipMaker.exe")

    def createDipFromCdbs(self, password_file=None):
        if not hasattr(self, "cdb_dir") or not os.path.isdir(self.cdb_dir):
            return {"success": False, "error": f"CDB folder '{getattr(self, 'cdb_dir', None)}' doesn't exist"}

        dipmaker_path = self.get_dipmaker_path()

        cmd = [dipmaker_path, self.cdb_dir]
        if password_file:
            cmd.append(password_file)

        try:
            result = subprocess.run(cmd, capture_output=True, text=True, check=False)
            if result.returncode == 0:
                return {"success": True, "message": result.stdout.strip()}
            else:
                err = result.stderr.strip() or result.stdout.strip()
                return {"success": False, "error": f"DipMaker failed: {err}"}
        except Exception as e:
            return {"success": False, "error": f"Failed to launch DipMaker.exe: {e}"}

    def addEntriesTo(self, cdbFileName: str, entries: list[list[EntryDataType]]):
        cdb = self.cdbs.get(cdbFileName)
        if cdb is None:
            return {"success": False, "error": f"No CDB file named '{cdbFileName}' found"}

        try:
            for entry in entries:
                cdb.addEntry(entry) 
            self.saveCdbOutputs(cdb)  
            return {
                "success": True,
                "message": f"Added {len(entries)} entries to {cdbFileName}"
            }
        except Exception as e:
            return {"success": False, "error": f"Error while adding entries to {cdbFileName}: {e}"}

    def removeEntriesFrom(self, cdbFileName: str, entryIndices: list[int]):
        cdb = self.cdbs.get(cdbFileName)
        if cdb is None:
            return {"success": False, "error": f"No CDB file named '{cdbFileName}' found"}

        try:
            for index in sorted(entryIndices, reverse=True):
                cdb.deleteEntry(index)
            self.saveCdbOutputs(cdb)  
            return {
                "success": True,
                "message": f"Removed {len(entryIndices)} entries from {cdbFileName}"
            }
        except Exception as e:
            return {"success": False, "error": f"Error while removing entries from {cdbFileName}: {e}"}
