# 7.4  Cgd Manager Tool
This tool is useful to modify the `cgd.dip` file of MicroVolts/ToyBattles, which contains very important information like:
- Server IPs, channel IDs
- Items, Shop Items, Capsules
- Ingame information: Zombie info, Modes info, Maps info, etc.

These are just a few examples. `cgd.dip` is a super important archive for the game.

Inside of it, there are multiple `cdb` files (a custom format) that contain data. The archive is protected by a ZipCrypto password, and you can find the password for your archive by using the [Password Tool](https://github.com/SoWeBegin/ToyBattlesHQ/blob/toybattles_mvsurge/doc/password_updater.md).

Of course modifying cdb files through tools like HxD is possible, but it's very time consuming. This tool makes the whole process much easier.

## Current Capabilities
- The tool is able to import the archive and extract its data. You can also import your previous work.
- The tool is able to generate two output folders, that are updated immediately after you apply any changes:
  - `cdbs` folder: this contains all updated cdb files, in the same folder structure as in `cgd.dip`. When you update your `cgd.dip` archive, you can simply take the `cdbs` folder and use that for the server-side `original_cgd` inside `ExternalLibraries`.
  - `jsons` folder: same as above, but all the files will be jsons, so you can easily edit them through a text editor.
- It's also able to generate an updated `cgd.dip` archive and protect it with your custom password.
- You can modify any file inside the original `cgd.dip` archive easily.
- There are specific utilities like the Capsule Manager. Since changing capsule stuff requires editing multiple cdb files, the tool automates this process and provides an easy visual way to update the capsule.

## Future Planned Additions
- Item Manager
- Shop Manager
- Coupon Shop Manager
- Server Manager (to setup IPs easily)
- Search Features

## Requirements
- Python 3.12
- .NET SDK (>= 10.0)

Open a terminal and move onto your CgdManager folder:
1) Clone this repository.
2) Open a terminal and `cd <ClonedRepositoryRootPath>`, next `cd Tools/CgdManager`

Then:

### Compile and generate DipMaker.exe
1) `dotnet new console -n temp_build --output temp_build`
2) `del temp_build\Program.cs`
3) `copy DipMaker.cs temp_build\`
4) `dotnet add temp_build package DotNetZip`
5) `dotnet publish temp_build -c Release -r win-x64 --self-contained true ^
    -p:PublishSingleFile=true -p:PublishTrimmed=true -p:IncludeAllContentForSelfExtract=true ^
    -p:AssemblyName=DipMaker`
6) `copy temp_build\bin\Release\net6.0\win-x64\publish\DipMaker.exe .`  (**Important:** instead of `new6.0`, use whatever NET version you installed)
7) `rmdir /s /q temp_build`

### Install requirements for python and generate executable
1) `python -m pip install -r requirements.txt`
2) `pyinstaller --onefile --windowed --name "CgdManager" ImportCgdDialog.py`
The final executable will be inside the new `dist` folder. This is a standalone exe that you can run directly.

## Overview

<img width="606" height="590" alt="image" src="https://github.com/user-attachments/assets/61f74213-9979-4cbf-b2ca-271e5599a81b" />

When you launch the exe this is the first window. You have two import options:
1) Import a `cgd.dip` archive. You will need to know the password, and you can find it through the [Password Tool](https://github.com/SoWeBegin/ToyBattlesHQ/blob/toybattles_mvsurge/doc/password_updater.md).
   - Enter Full Path: here you must add the path to the cgd.dip archive. Example: `C:\MyFiles\cgd.dip`
   - Path to unpacked UI/icon folder: this requires you to unpack Microvolts/ToyBattles `ui.dat` archive (you can follow [this](https://www.youtube.com/watch?v=xd5s4XigkAE) tutorial). Once unpacked, the resulting UI folder will contain an `icon` folder. The path to it needs to be inserted here.
   - Output path: This is where the tool will generate `cdbs` and `jsons` folders, containing updated data every time you modify anything. This output can later be imported to continue previous work.
2) Import previous folder work:
   - When you first start, you'll need to do point 1). When that's done and you modified your archive's data, the tool will have generated `cdbs` and `jsons` folders.
   - Just copy the path to the folder that contains these two subfolders and put it in the first field.
   - The second field is identical to point 1) for the unpacked UI/icon folder.

<img width="1199" height="737" alt="image" src="https://github.com/user-attachments/assets/b8688458-9b4b-4b28-9b2d-764f23b1ef29" />

In the main window, you can simply select the file you want to edit, and then edit the files normally.

<img width="1002" height="634" alt="image" src="https://github.com/user-attachments/assets/e41a9278-19ac-4532-a68d-a50275d6219f" />

The CapsuleManager makes it really easy to edit existing capsules, remove them, add new ones, add new items, remove existing items, and so on.
  

## Next
[8.1 How to create bug reports via GitHub Issues](https://github.com/SoWeBegin/MicrovoltsEmulator/blob/mv1.1_2.0/doc/reporting_issues.md)

