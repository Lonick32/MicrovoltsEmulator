import argparse
import configparser
import os
import shutil
import xml.etree.ElementTree as ET
import zlib
from pathlib import Path
import subprocess

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
    prefix, version_str = ver.split('_', 1)
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

def get_relative_structure(file_path, game_root):
    file_path = Path(file_path).resolve()
    game_root = Path(game_root).resolve()
    try:
        relative = file_path.relative_to(game_root)
        return str(relative).replace("\\", "/")
    except ValueError:
        return str(file_path.name)

def adler32_checksum(file_path):
    with open(file_path, 'rb') as f:
        return format(zlib.adler32(f.read()) & 0xffffffff, '08x')

def generate_xml(file_paths, game_root, output_xml_path, new_version):
    root = ET.Element('DeltaInfo', Name='microvolts', Version=new_version)
    updated = ET.SubElement(root, 'UpdatedFiles')

    dir_map = {} 

    for patch_file in file_paths:
        patch_filename = os.path.basename(patch_file)

        matched_path = None
        for root_dir, _, files in os.walk(game_root):
            if os.path.basename(patch_file).replace('.new', '') in files:
                matched_path = os.path.join(root_dir, os.path.basename(patch_file).replace('.new', ''))
                break

        if not matched_path:
            print(f"Warning: {os.path.basename(patch_file).replace('.new', '')} not found in game directory")
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

def generate_cab(file_paths, cab_path, game_root):
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
                print(f"Warning: {filename} not found in game directory")
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
    
    subprocess.run(['makecab', '/F', str(ddf_path)], check=True)
    os.remove(ddf_path)
    
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--patch-ini', required=True)
    parser.add_argument('--new-files', required=True)
    parser.add_argument('--game-root', required=True)
    parser.add_argument('--output', required=True)
    args = parser.parse_args()

    try:
        print("[*] Starting patch generation...")
        
        patch_ini = Path(args.patch_ini)
        new_files_dir = Path(args.new_files)
        game_root = Path(args.game_root)
        output_dir = Path(args.output)

        if not patch_ini.exists():
            raise FileNotFoundError(f"Patch.ini not found at {patch_ini}")
        if not new_files_dir.exists():
            raise FileNotFoundError(f"New files directory not found at {new_files_dir}")
        if not game_root.exists():
            raise FileNotFoundError(f"Game root directory not found at {game_root}")

        print("[*] Reading versions...")
        versions = read_patch_ini(patch_ini)
        current_version = versions[0] if versions else 'UNKNOWN'
        next_version = increment_version(current_version)
        print(f"[*] Current version: {current_version}, Next version: {next_version}")

        version_folder = output_dir / next_version
        version_folder.mkdir(parents=True, exist_ok=True)

        output_patch_ini = output_dir / 'patch.ini'
        shutil.copy(patch_ini, output_patch_ini)
        update_patch_ini(output_patch_ini, versions, next_version) 
        print(f"[*] Created and updated patch.ini at {output_patch_ini}")

        temp_dir = version_folder / 'temp'
        temp_dir.mkdir(exist_ok=True)

        all_files = [str(f) for f in new_files_dir.rglob('*') if f.is_file()]

        cab_path = version_folder / f'microvolts-{current_version}-{next_version}.cab'
        xml_path = version_folder / f'microvolts-{current_version}-{next_version}.xml'

        renamed_files = copy_and_rename_files(all_files, temp_dir, new_files_dir)
        
        patch_ini_temp_path = temp_dir / 'patch.ini.new'
        shutil.copy(output_patch_ini, patch_ini_temp_path) 
        renamed_files.append(str(patch_ini_temp_path))
        all_files.append(str(output_patch_ini))  

        print("[*] Generating XML manifest...")
        generate_xml(all_files, game_root, xml_path, next_version)

        print("[*] Generating CAB archive...")
        generate_cab(renamed_files, cab_path, game_root)

        shutil.rmtree(temp_dir)

        print('\n[✓✓✓] PATCH GENERATION COMPLETE!')
        print(f"    Includes updated patch.ini with version: {next_version}")

    except Exception as e:
        print(f"\n[!!!] ERROR: {str(e)}")
        print("Patch generation failed!")
        if 'temp_dir' in locals() and temp_dir.exists():
            print("Note: Temp files remain at:", temp_dir)
        sys.exit(1)

if __name__ == '__main__':
    import sys
    main()
