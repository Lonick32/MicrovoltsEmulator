# 7.3 Changing the CGD password (using the password tool)

## Overview
This utility modifies the encrypted password stored in the Microvolts client executable that's used to protect the cgd.dip archive. The tool can:
1. Locate the current password in the executable
2. Decrypt and display the current password
3. Patch the executable with a new password

### Features
1. RC6 Encryption/Decryption - Uses the same crypto as the original client
2. Offset Searching - Can find password location automatically
3. Non-destructive - Creates patched copies rather than modifying originals
4. Interactive CLI - User-friendly command interface


## Usage guide
### Basic Commands
```bash
# Find and show current password
/try_find_password

# Find password at specific offset (hex)
/find_password_by_offset 0x00bd0c70

# Search for password location (encrypted)
/find_offset_encrypted [ENCRYPTED_HEX]

# Search for password location (decrypted)
/find_offset_decrypted [CURRENT_PASSWORD]

# Apply new password
/patch_password_by_offset [OFFSET] [NEW_PASSWORD]

# Change executable path
/chpath [NEW_PATH]

# Show help
/help

# Exit program
/exit
```

### Password Requirements
- Must be exactly **26 characters** long
- The password will be RC6 encrypted automatically, you just need to provide a plain-text one.

### Example Usage
1. Locate current password: `/try_find_password`
2. Find password offset (if default fails): `/find_offset_decrypted MyCurrentPassword123456`
3. Patch new password: `/patch_password_by_offset 00bd0c70 MyNewPassword12345678901234`
4. Verify patched file:
```
/chpath Microvolts_patched.exe
/try_find_password
```

## Building
Requires:
- C++17 compiler
- Windows SDK (for binary manipulation)
- `Crypt.h` (can be found inside the `Tools/Client` folder)
- The utility can be found inside `Tools/Client/CgdPasswordUpdater.cpp`


## Next
[8.1 How to create bug reports via GitHub Issues](https://github.com/SoWeBegin/MicrovoltsEmulator/blob/mv1.1_2.0/doc/reporting_issues.md)
