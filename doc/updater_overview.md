# 7.1 The Updater: How the launcher retrieves updates

## 7.1.1 Update System Overview

Starting with version 2.0, the Microvolts updater tool is available for creating client update packages. This document explains:

1. How the update system works
2. Required file structure
3. How to use the Python update utility
4. How to deploy updates

## Update Types

The system supports two approaches:
- **Full updates** (complete file replacements) - *This is what our tool creates*
- **Partial updates** (not supported by our tool)

## Configuration Files 
The following files can be found in the root directory of the game folder.
### `updateinfo.ini`
Specifies download locations:
```ini
[update]
addr = http://raw.githubusercontent.com/SoWeBegin/MVPtchTest/main

[FullFile]
addr = http://dls3.microvolts.com/update/ENG/Full/
```
Only the `[update]` section is relevant for our purposes.

### `patch.ini`
Tracks all client versions (newest to oldest):
```
[patch]
version = ENG_7.0.0.2
version1 = ENG_7.0.0.1
version2 = ENG_7.0.0.0
exe = bin/Microvolts.exe
```

**Important Notes:**
- Versions are listed newest to oldest
- The first version entry has no number suffix
- Older versions have sequential numbers (version1, version2, etc.)
- The game executable path is always last


## 7.1.2 Creating Update Packages
### Required folder structure
Your update server (or website, or whenever you put all your update packages) needs this exact structure:
```cpp
microvolts/
│
├── patch.ini                # Current version manifest
│
├── Patcher/                 # Launcher update files (unused in this system)
│   └── patchLauncher.ini    # Launcher version info
│
└── ENG_[NEW_VERSION]/       # Version-specific update package
    │
    ├── microvolts-[OLD_VERSION]-[NEW_VERSION].cab    # Compressed update files
    └── microvolts-[OLD_VERSION]-[NEW_VERSION].xml    # Update manifest
```

You can take a look at my public example [here](https://github.com/SoWeBegin/MVPtchTest/tree/main)

### Update Package Components
Each version folder contains two critical files:
1. **.cab** File:
 - Contains all updated files
 - Files must have .new extension (e.g., MicroVolts.exe.new)
 - These will replace existing files during update

2. **.xml** File:
 - Lists all updated files and their checksums
 - Uses ADLER32 checksum algorithm
 - Specifies file locations

Example:
```xml
<?xml version="1.0" encoding="utf-8" ?>
<DeltaInfo Name="microvolts" Version="ENG_7.0.0.1">
    <UpdatedFiles>
        <File Name="patch.ini" CheckSum="5e931ee8" />
        <Dir Name="Bin">
            <File Name="Microvolts.exe" CheckSum="6ec5e804" />
        </Dir>
    </UpdatedFiles>
</DeltaInfo>
```

## 7.1.3 Using the updater
Everything mentioned above is almost automatically done through the `updater.py` utility, that you can find inside `Tools/updater.py`.
You will need to install all the necessary packages for Python. 

The only **Requirement** is having Python >= 3.12 installed on your machine.

### Usage / Command line arguments
Just open a terminal, `cd <yourPathToUpdater>`, and then `python updater.py`. This will open a visual tool. 

To know what each field requires, just hover over it with your mouse and a tooltip should appear.

### What the tool does
1. Reads the current version from patch.ini
2. Generates the next version number (increments last digit)
3. Creates a new version folder in the output directory
4. Generates:
  - Updated patch.ini with new version
  - .cab file containing all changed files
  - .xml manifest with file checksums


### Deployment instructions
After running the updater:
1. Locate these generated files:
```py
update_output/
├── patch.ini
└── ENG_NEW_VERSION/
    ├── microvolts-OLD-NEW.cab
    └── microvolts-OLD-NEW.xml
```
2. Upload to your update server:
- Place the updated patch.ini in your microvolts/ root
- Place the ENG_NEW_VERSION folder in your microvolts/ directory

3. Final server structure should look like:
```py
microvolts/
│
├── patch.ini                # Contains all versions up to ENG_7.0.0.2
│
├── Patcher/
│   └── patchLauncher.ini
│
├── ENG_7.0.0.1/             # Update from 7.0.0.0 → 7.0.0.1
│   │
│   ├── microvolts-ENG_7.0.0.0-ENG_7.0.0.1.cab
│   └── microvolts-ENG_7.0.0.0-ENG_7.0.0.1.xml
│
└── ENG_7.0.0.2/             # Update from 7.0.0.1 → 7.0.0.2
    │
    ├── microvolts-ENG_7.0.0.1-ENG_7.0.0.2.cab
    └── microvolts-ENG_7.0.0.1-ENG_7.0.0.2.xml
```

### Verification checklist
Before deploying:
- Test update locally
- Verify all versions appear in patch.ini
- Confirm .cab contains all changed files
- Check XML checksums match the files
- Ensure previous update packages remain available in the server
- Ensure that your cgd.dip's `patchversion.cdb` inside the `ENG` folder has the latest version, identically as the last one inside your `patch.ini` file. Otherwise your launcher will complain!




## Next
[7.2 Changing the client version (using the client version tool)](https://github.com/SoWeBegin/MicrovoltsEmulator/blob/mv1.1_2.0/doc/client_version_tool.md)
