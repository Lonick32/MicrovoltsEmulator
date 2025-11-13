#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <sstream>
#include <string>
#include <stdexcept>

using DWORD = uint32_t;

struct PatchLocation {
    DWORD fileOffset;
    DWORD newValue;
};

bool isValidVersion(int major, int minor, int patch) {
    if (major < 0 || major > 2) return false;
    if (minor < 0 || minor > 9) return false;
    if (patch < 0 || patch > 9) return false;
    if (major == 0 && minor == 0 && patch == 0) return false;

    return true;
}

bool parseVersion(const std::string& versionStr, int& major, int& minor, int& patch) {
    std::stringstream ss(versionStr);
    char dot;

    if (!(ss >> major >> dot >> minor >> dot >> patch) || dot != '.') {
        std::cerr << "Invalid version format. Expected X.X.X (e.g., 1.2.3)" << std::endl;
        return false;
    }

    return isValidVersion(major, minor, patch);
}

bool PatchExecutable(const char* filePath, int major, int minor, int patch) {
    std::fstream file(filePath, std::ios::in | std::ios::out | std::ios::binary);
    if (!file) {
        std::cerr << "Error opening file: " << filePath << std::endl;
        return false;
    }

    PatchLocation patches[] = {
        {0x0070A94D, major},
        {0x0070A954, minor},
        {0x0070A95B, patch}
    };

    for (const auto& patch : patches) {
        file.seekg(patch.fileOffset);
        std::vector<char> buffer(4);
        file.read(buffer.data(), 4);

        if (!file) {
            std::cerr << "Read failed at offset 0x" << std::hex << patch.fileOffset << std::endl;
            return false;
        }

        std::cout << "Original bytes at offset 0x" << std::hex << patch.fileOffset << ": ";
        for (auto byte : buffer) {
            std::cout << std::hex << (0xFF & (uint8_t)byte) << " ";
        }
        std::cout << std::endl;

        file.seekp(patch.fileOffset);
        if (!file) {
            std::cerr << "Seek failed at offset 0x" << std::hex << patch.fileOffset << std::endl;
            return false;
        }

        file.write(reinterpret_cast<const char*>(&patch.newValue), sizeof(DWORD));
        if (!file) {
            std::cerr << "Write failed at offset 0x" << std::hex << patch.fileOffset << std::endl;
            return false;
        }

        file.seekg(patch.fileOffset);
        file.read(buffer.data(), 4);
        std::cout << "Patched bytes at offset 0x" << std::hex << patch.fileOffset << ": ";
        for (auto byte : buffer) {
            std::cout << std::hex << (0xFF & (uint8_t)byte) << " ";
        }
        std::cout << std::endl;
    }

    return true;
}

int main() {
    std::cout << "This program patches the specified MV exe by modifying the (hardcoded) client version\n";
    std::cout << "You will need to provide the full path to the executable and the new version in this format: X.X.X\n";
    std::cout << "The program will create a backup of the executable before patching it.\n\n";


    std::string exePath;
    std::cout << "Enter the full path to the executable: ";
    std::getline(std::cin, exePath);

    const std::string backupExe = exePath + "_backup.exe";

    try {
        std::ifstream src(exePath, std::ios::binary);
        std::ofstream dst(backupExe, std::ios::binary);
        dst << src.rdbuf();
    }
    catch (...) {
        std::cerr << "Backup creation failed!" << std::endl;
        return 1;
    }

    std::string versionStr;
    int major, minor, patch;

    while (true) {
        std::cout << "Enter the new client version (X.X.X): ";
        std::getline(std::cin, versionStr);

        if (parseVersion(versionStr, major, minor, patch)) {
            break;
        }
        else {
            std::cout << "Please enter a valid version (0.0.1 <= version <= 2.9.9)." << std::endl;
        }
    }

    if (PatchExecutable(exePath.c_str(), major, minor, patch)) {
        std::cout << "Successfully patched executable!" << std::endl;
    }
    else {
        std::cerr << "Patching failed!" << std::endl;
    }

    std::cout << "Press Enter to exit...";
    std::cin.get(); 

    return 0;
}
