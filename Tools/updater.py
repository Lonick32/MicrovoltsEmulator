
import sys
import os
import shutil
import configparser
import zlib
import subprocess
from pathlib import Path
import xml.etree.ElementTree as ET

from PySide6.QtWidgets import (
    QApplication, QWidget, QLabel, QLineEdit, QPushButton,
    QHBoxLayout, QVBoxLayout, QFileDialog, QPlainTextEdit, QMessageBox
)
from PySide6.QtCore import Qt, QThread, Signal


def read_patch_ini(path):
    config = configparser.ConfigParser()
    config.read(path)
    versions = []
    if 'patch' in config:
        for key in sorted(config['patch'].keys()):
            if key.startswith('version') or key == 'version':
                versions.append(config['patch'][key])
    return versions


def increment_version(ver):
    try:
        prefix, version_str = ver.split('_', 1)
    except ValueError:
        return ver + '_1.0.0'
    parts = list(map(int, version_str.split('.')))
    parts[-1] += 1
    for i in reversed(range(1, len(parts))):
        if parts[i] > 9:
            parts[i] = 0
            parts[i-1] += 1
    return f"{prefix}_{'.'.join(map(str, parts))}"


def update_patch_ini(path, versions, new_version):
    config = configparser.ConfigParser()
    config.read(path)

    exe_path = None
    if 'patch' in config and 'exe' in config['patch']:
        exe_path = config['patch']['exe']

    if 'patch' not in config:
        config['patch'] = {}
    else:
        config['patch'].clear()

    config['patch']['version'] = new_version
    for i, ver in enumerate(versions):
        config['patch'][f'version{i+1}'] = ver

    if exe_path:
        config['patch']['exe'] = exe_path

    with open(path, 'w') as f:
        config.write(f)


def adler32_checksum(file_path):
    with open(file_path, 'rb') as f:
        return format(zlib.adler32(f.read()) & 0xffffffff, '08x')


def generate_xml(file_paths, game_root, output_xml_path, new_version, log_fn=print):
    root = ET.Element('DeltaInfo', Name='microvolts', Version=new_version)
    updated = ET.SubElement(root, 'UpdatedFiles')

    dir_map = {}

    for patch_file in file_paths:

        matched_path = None
        for root_dir, _, files in os.walk(game_root):
            if os.path.basename(patch_file).replace('.new', '') in files:
                matched_path = os.path.join(root_dir, os.path.basename(patch_file).replace('.new', ''))
                break

        if not matched_path:
            log_fn(f"Warning: {os.path.basename(patch_file).replace('.new', '')} not found in game directory")
            continue

        checksum = adler32_checksum(patch_file)
        rel_path = os.path.relpath(matched_path, game_root).replace("\\", "/")
        dir_part = os.path.dirname(rel_path)
        file_part = os.path.basename(rel_path)

        if dir_part == "":
            ET.SubElement(updated, 'File', Name=file_part, CheckSum=checksum)
        else:
            if dir_part not in dir_map:
                dir_elem = ET.SubElement(updated, 'Dir', Name=dir_part)
                dir_map[dir_part] = dir_elem
            ET.SubElement(dir_map[dir_part], 'File', Name=file_part, CheckSum=checksum)

    tree = ET.ElementTree(root)
    tree.write(output_xml_path, encoding='utf-8', xml_declaration=True)


def copy_and_rename_files(file_paths, temp_dir, new_files_dir):
    renamed_files = []
    for file_path in file_paths:
        rel_path = Path(file_path).relative_to(new_files_dir)
        target_path = temp_dir / rel_path.with_suffix(rel_path.suffix + '.new')
        target_path.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy(file_path, target_path)
        renamed_files.append(str(target_path))
    return renamed_files

def generate_cab(file_paths, cab_path, game_root, log_fn=print):
    ddf_path = Path(cab_path).with_suffix('.ddf')
    with open(ddf_path, 'w') as ddf:
        ddf.write('.OPTION EXPLICIT\n')
        ddf.write(f'.Set CabinetNameTemplate={Path(cab_path).name}\n')
        ddf.write(f'.Set DiskDirectory1={Path(cab_path).parent}\n')
        ddf.write('.Set CompressionType=MSZIP\n')
        ddf.write('.Set Cabinet=ON\n')
        ddf.write('.Set MaxDiskSize=0\n')
        ddf.write('.Set MaxDiskFileCount=0\n')
        ddf.write('.Set FolderSizeThreshold=0\n')

        dir_structure = {}

        for new_file in file_paths:
            filename = Path(new_file).stem
            matched_path = None
            for root, _, files in os.walk(game_root):
                if filename in files:
                    matched_path = Path(root) / filename
                    break

            if not matched_path:
                log_fn(f"Warning: {filename} not found in game directory")
                continue

            rel_path = matched_path.relative_to(game_root)
            dir_part = str(rel_path.parent) if rel_path.parent != Path('.') else ''
            file_name = rel_path.name + '.new'

            if dir_part not in dir_structure:
                dir_structure[dir_part] = []
            dir_structure[dir_part].append((new_file, file_name))

        if '' in dir_structure:
            for src_path, dest_name in dir_structure['']:
                ddf.write(f'"{src_path}" {dest_name}\n')

        for dir_part in sorted([d for d in dir_structure.keys() if d]):
            ddf.write(f'.Set DestinationDir={dir_part}\n')
            for src_path, dest_name in dir_structure[dir_part]:
                ddf.write(f'"{src_path}" {dest_name}\n')

    try:
        process = subprocess.Popen(
            ['makecab', '/F', str(ddf_path)],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
            universal_newlines=True
        )

        for line in process.stdout:
            log_fn(line.strip())

        process.wait()

        if process.returncode != 0:
            log_fn(f"[Error] makecab failed with code {process.returncode}")

    finally:
        try:
            os.remove(ddf_path)
        except Exception:
            pass


class WorkerThread(QThread):
    log = Signal(str)
    finished_success = Signal(str)
    finished_failure = Signal(str)

    def __init__(self, patch_ini, new_files_dir, game_root, output_dir):
        super().__init__()
        self.patch_ini = Path(patch_ini)
        self.new_files_dir = Path(new_files_dir)
        self.game_root = Path(game_root)
        self.output_dir = Path(output_dir)

    def run(self):
        try:
            self.log.emit('(*) Starting patch generation...')

            if not self.patch_ini.exists():
                raise FileNotFoundError(f"Patch.ini not found at {self.patch_ini}")
            if not self.new_files_dir.exists():
                raise FileNotFoundError(f"New files directory not found at {self.new_files_dir}")
            if not self.game_root.exists():
                raise FileNotFoundError(f"Game root directory not found at {self.game_root}")

            self.log.emit('(*) Reading versions...')
            versions = read_patch_ini(self.patch_ini)
            current_version = versions[0] if versions else 'UNKNOWN'
            next_version = increment_version(current_version)
            self.log.emit(f"(*) Current version: {current_version}, Next version: {next_version}")

            version_folder = self.output_dir / next_version
            version_folder.mkdir(parents=True, exist_ok=True)

            output_patch_ini = self.output_dir / 'patch.ini'
            shutil.copy(self.patch_ini, output_patch_ini)
            update_patch_ini(output_patch_ini, versions, next_version)
            self.log.emit(f"(*) Created and updated patch.ini at {output_patch_ini}")

            temp_dir = version_folder / 'temp'
            temp_dir.mkdir(exist_ok=True)

            all_files = [str(f) for f in self.new_files_dir.rglob('*') if f.is_file()]

            cab_path = version_folder / f'microvolts-{current_version}-{next_version}.cab'
            xml_path = version_folder / f'microvolts-{current_version}-{next_version}.xml'

            self.log.emit('(*) Copying and renaming files...')
            renamed_files = copy_and_rename_files(all_files, temp_dir, self.new_files_dir)

            patch_ini_temp_path = temp_dir / 'patch.ini.new'
            shutil.copy(output_patch_ini, patch_ini_temp_path)
            renamed_files.append(str(patch_ini_temp_path))
            all_files.append(str(output_patch_ini))

            self.log.emit('(*) Generating XML manifest...')
            generate_xml(all_files, str(self.game_root), str(xml_path), next_version, log_fn=lambda s: self.log.emit(s))

            self.log.emit('(*) Generating CAB archive...')
            generate_cab(renamed_files, str(cab_path), str(self.game_root), log_fn=lambda s: self.log.emit(s))

            try:
                shutil.rmtree(temp_dir)
            except Exception:
                pass

            msg = f"PATCH GENERATION COMPLETE! Includes updated patch.ini with version: {next_version}"
            self.log.emit('(OK) ' + msg)
            self.finished_success.emit(msg)

        except Exception as e:
            self.log.emit('(!!!) ERROR: ' + str(e))
            try:
                if 'temp_dir' in locals() and temp_dir.exists():
                    self.log.emit('Note: Temp files remain at: ' + str(temp_dir))
            except Exception:
                pass
            self.finished_failure.emit(str(e))


class MainWindow(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowTitle('Patch Generator GUI')
        self.resize(800, 500)

        layout = QVBoxLayout()

        self.patch_edit = QLineEdit()
        self.patch_edit.setToolTip(
            "This is the current (latest) patch.ini file. "
            "The tool will generate an updated version in the output directory."
        )
        b1 = QPushButton('Browse')
        b1.clicked.connect(self.browse_patch)
        row1 = QHBoxLayout()
        row1.addWidget(QLabel('patch.ini:'))
        row1.addWidget(self.patch_edit)
        row1.addWidget(b1)
        layout.addLayout(row1)

        self.new_files_edit = QLineEdit()
        self.new_files_edit.setToolTip(
            "Path to the folder containing all updated game files. "
            "Place all files directly inside this folder (no subfolders)."
        )
        b2 = QPushButton('Browse')
        b2.clicked.connect(self.browse_new_files)
        row2 = QHBoxLayout()
        row2.addWidget(QLabel('New files dir:'))
        row2.addWidget(self.new_files_edit)
        row2.addWidget(b2)
        layout.addLayout(row2)

        self.game_root_edit = QLineEdit()
        self.game_root_edit.setToolTip(
            "Path to the main game installation folder (e.g., Microvolts or ToyBattles)."
        )
        b3 = QPushButton('Browse')
        b3.clicked.connect(self.browse_game_root)
        row3 = QHBoxLayout()
        row3.addWidget(QLabel('Game root:'))
        row3.addWidget(self.game_root_edit)
        row3.addWidget(b3)
        layout.addLayout(row3)

        self.output_edit = QLineEdit()
        self.output_edit.setToolTip(
            "Path to the folder where the tool will save the generated update files."
        )
        b4 = QPushButton('Browse')
        b4.clicked.connect(self.browse_output)
        row4 = QHBoxLayout()
        row4.addWidget(QLabel('Output dir:'))
        row4.addWidget(self.output_edit)
        row4.addWidget(b4)
        layout.addLayout(row4)

        self.run_button = QPushButton('Create update files')
        self.run_button.clicked.connect(self.start_generation)
        layout.addWidget(self.run_button)

        self.log_widget = QPlainTextEdit()
        self.log_widget.setReadOnly(True)
        layout.addWidget(self.log_widget)

        self.setLayout(layout)
        self.worker = None

    def append_log(self, text):
        self.log_widget.appendPlainText(text)

    def browse_patch(self):
        fn, _ = QFileDialog.getOpenFileName(self, 'Select patch.ini', os.getcwd(), 'INI Files (*.ini);;All Files (*)')
        if fn:
            self.patch_edit.setText(fn)

    def browse_new_files(self):
        d = QFileDialog.getExistingDirectory(self, 'Select Directory containing New Updated Files', os.getcwd())
        if d:
            self.new_files_edit.setText(d)

    def browse_game_root(self):
        d = QFileDialog.getExistingDirectory(self, 'Select MicroVolts/ToyBattles Root directory', os.getcwd())
        if d:
            self.game_root_edit.setText(d)

    def browse_output(self):
        d = QFileDialog.getExistingDirectory(self, 'Select Output directory', os.getcwd())
        if d:
            self.output_edit.setText(d)

    def start_generation(self):
        patch = self.patch_edit.text().strip()
        new_files = self.new_files_edit.text().strip()
        game_root = self.game_root_edit.text().strip()
        output = self.output_edit.text().strip()

        if not patch or not new_files or not game_root or not output:
            QMessageBox.warning(self, 'Missing values', 'Fill all paths before starting.')
            return

        confirm_msg = (
            "Before proceeding please confirm (otherwise the client update will fail!):\n\n"
            "1. You have placed an updated 'cgd.dip' file inside the NEW FILES folder.\n"
            "2. The file 'patchversioninfo.cdb' inside 'cgd.dip' from step 1. has been modified "
            "to include the new client version (e.g from ENG_2.8.1.7 --> ENG_2.8.1.8).\n\n"
            "Have you completed these steps?"
        )
        reply = QMessageBox.question(
            self,
            'Confirm cgd.dip update',
            confirm_msg,
            QMessageBox.Yes | QMessageBox.No,
            QMessageBox.No
        )

        if reply == QMessageBox.No:
            self.append_log('(x) Patch generation cancelled: user did not confirm cgd.dip update.')
            return

        self.run_button.setEnabled(False)
        self.append_log('(*) Start patch generation requested...')

        self.worker = WorkerThread(patch, new_files, game_root, output)
        self.worker.log.connect(self.append_log)
        self.worker.finished_success.connect(self.on_finished)
        self.worker.finished_failure.connect(self.on_failure)
        self.worker.start()


    def on_finished(self, msg):
        QMessageBox.information(self, 'Done', msg)
        self.run_button.setEnabled(True)

    def on_failure(self, error):
        QMessageBox.critical(self, 'Error', 'Patch generation failed: ' + error)
        self.run_button.setEnabled(True)


if __name__ == '__main__':
    app = QApplication(sys.argv)
    w = MainWindow()
    w.show()
    sys.exit(app.exec())
