

#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include "Crypt.h"
#include <algorithm>
#include <optional>
#include <iomanip>
#include <cassert>
#include <cstring> 

const std::string GREEN = "\033[32m";
const std::string ORANGE = "\033[33m";
const std::string RESET = "\033[0m";

std::optional<std::vector<unsigned char>> readCdbPassword(const std::string& exePath,
    std::streamoff offset = 0x00bd0c70,
    std::size_t passwordSize = 127) {
    std::ifstream inputFile(exePath, std::ios::binary | std::ios::ate);
    if (!inputFile) {
        std::cerr << ORANGE << "Error: exe file not found!" << RESET << "\n";
        return std::nullopt;
    }

    std::streamoff fileSize = inputFile.tellg();
    if (fileSize <= 0) {
        std::cerr << ORANGE << "Error: Wrong file size or file is empty." << RESET << "\n";
        return std::nullopt;
    }

    if (offset < 0 || offset + passwordSize > fileSize) {
        std::cerr << ORANGE << "Error: Invalid offset or size exceeds file bounds." << RESET << "\n";
        return std::nullopt;
    }

    inputFile.seekg(offset, std::ios::beg);
    if (!inputFile.good()) {
        std::cerr << ORANGE << "Error: Failed to seek to the specified offset." << RESET << "\n";
        return std::nullopt;
    }

    std::vector<unsigned char> passwordBytes(passwordSize);
    inputFile.read(reinterpret_cast<char*>(passwordBytes.data()), passwordSize);
    if (!inputFile.good() || inputFile.gcount() != static_cast<std::streamsize>(passwordSize)) {
        std::cerr << ORANGE << "Error: Could not read the specified number of bytes." << RESET << "\n";
        return std::nullopt;
    }

    return passwordBytes;
}


std::optional<std::streamoff> findEncryptedPasswordOffset(const std::string& exePath, const std::string& password) {
    if (password.empty()) {
        std::cerr << ORANGE << "Error: Password cannot be empty!\n";
        return std::nullopt;
    }

    std::string truncatedPassword = password.substr(0, 16);
    std::transform(truncatedPassword.begin(), truncatedPassword.end(), truncatedPassword.begin(), ::tolower);

    std::ifstream inputFile(exePath, std::ios::binary);
    if (!inputFile) {
        std::cerr << ORANGE << "Error: exe file not found!\n";
        return std::nullopt;
    }

    std::vector<unsigned char> buffer(1024);
    while (inputFile.read(reinterpret_cast<char*>(buffer.data()), buffer.size())) {
        auto pos = std::search(
            buffer.begin(), buffer.end(),
            truncatedPassword.begin(), truncatedPassword.end(),
            [](unsigned char c1, unsigned char c2) {
                return tolower(c1) == c2;
            }
        );

        if (pos != buffer.end()) {
            return static_cast<std::streamoff>(inputFile.tellg()) - buffer.size() + (pos - buffer.begin());
        }
    }

    std::cerr << ORANGE << "Error: Password '" << truncatedPassword << "' not found in the file.\n";
    return std::nullopt;
}


std::vector<unsigned char> hexStringToBytes(const std::string& hexString) {
    std::vector<unsigned char> result;
    for (size_t i = 0; i < hexString.length(); i += 2) {
        unsigned char byte = static_cast<unsigned char>(std::stoi(hexString.substr(i, 2), nullptr, 16));
        result.push_back(byte);
    }
    return result;
}

std::optional<std::streamoff> findDecryptedPasswordOffset(const std::string& exePath, const std::string& unencryptedPassword, CCrypt& crypt) {
    if (unencryptedPassword.empty()) {
        std::cerr << ORANGE << "Error: Password cannot be empty\n";
        return std::nullopt;
    }

    const std::string truncatedPassword = unencryptedPassword.substr(0, 16);
    std::vector<unsigned char> encrypted(truncatedPassword.begin(), truncatedPassword.end());
    crypt.RC6Encrypt128(encrypted.data(), encrypted.data(), encrypted.size());

    std::string encryptedHexStr;
    for (unsigned char byte : encrypted) {
        char hex[3];
        snprintf(hex, sizeof(hex), "%02X", byte);
        encryptedHexStr += hex;
    }
    return findEncryptedPasswordOffset(exePath, encryptedHexStr);
}

void patchStringInBinary(const std::string& exePath, const std::string& outputPath,
    const std::string& newString, std::streamoff offset, std::size_t originalPwSize = 26) {

    if (newString.size() != originalPwSize) {
        std::cerr << ORANGE << "Error: Password size must be " << originalPwSize << " characters\n";
        return;
    }

    std::ifstream inputFile(exePath, std::ios::binary);
    if (!inputFile) {
        std::cerr << ORANGE << "Error: Cannot open input file.\n";
        return;
    }

    std::vector<char> binaryContent((std::istreambuf_iterator<char>(inputFile)), std::istreambuf_iterator<char>());
    inputFile.close();

    if (offset + originalPwSize > binaryContent.size()) {
        std::cerr << ORANGE << "Error: Offset out of bounds.\n";
        return;
    }

    std::string password = newString;

    std::string part1 = password.substr(0, 16);
    std::string part2 = password.substr(16, 10);
    part2.resize(16, '\0');

    std::vector<unsigned char> encryptedPart1(part1.begin(), part1.end());
    std::vector<unsigned char> encryptedPart2(part2.begin(), part2.end());

    CCrypt crypt(0);
    crypt.RC6Encrypt128(encryptedPart1.data(), encryptedPart1.data(), encryptedPart1.size());
    crypt.RC6Encrypt128(encryptedPart2.data(), encryptedPart2.data(), encryptedPart2.size());

    std::cout << "\n[Debug] New Password Bytes: ";
    std::string encryptedHexStr;
    for (unsigned char byte : encryptedPart1) {
        char hex[3];
        snprintf(hex, sizeof(hex), "%02X", byte);
        encryptedHexStr += hex;
    }
    for (unsigned char byte : encryptedPart2) {
        char hex[3];
        snprintf(hex, sizeof(hex), "%02X", byte);
        encryptedHexStr += hex;
    }

    std::cout << encryptedHexStr << '\n';

    std::memcpy(&binaryContent[offset], encryptedHexStr.c_str(), encryptedHexStr.size());
    std::memcpy(&binaryContent[0xc6e658], encryptedHexStr.c_str(), encryptedHexStr.size()); // for shotgun.exe

    std::ofstream outputFile(outputPath, std::ios::binary);
    if (!outputFile) {
        std::cerr << ORANGE << "Error: Could not open output file.\n";
        return;
    }
    outputFile.write(binaryContent.data(), binaryContent.size());
    outputFile.close();

    std::cout << GREEN << "Patch applied. Output saved to: " << outputPath << "\n";
}



void printHelp() {
    std::cout << "\nAvailable commands:\n";
    std::cout << "/try_find_password - Attempts to find the password at the default offset and shows both encrypted and decrypted versions.\n";
    std::cout << "/find_password_by_offset <offset> - Finds the password at the specified offset (in hexadecimal format).\n";
    std::cout << "/find_offset_encrypted <current_encrypted_password> - Searches for the offset of the password where the current password (encrypted) is located.\n";
    std::cout << "/find_offset_decrypted <current_decrypted_password> - Searches for the offset of the password where the current password (decrypted) is located.\n";
    std::cout << "/patch_password_by_offset <offset> <new_password> - Patches the executable at the specified offset with a new password.\n";
    std::cout << "/chpath <new_path> - Changes the path\n";
    std::cout << "/help - Displays this help message.\n";
    std::cout << "/exit - Exits the program.\n";
}


int main() {
    std::cout << "For help type /help\n";
    std::string exePath;
    std::cout << "Enter the path to the .exe file: ";
    std::cin >> exePath;
    CCrypt crypt(0);

    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    while (true) {
        std::cout << "\nEnter command: ";
        std::string command;
        std::getline(std::cin, command);

        if (command == "/try_find_password") {
            auto optPasswordBytes = readCdbPassword(exePath);
            std::cout << RESET;
            if (!optPasswordBytes) continue;
            std::vector<unsigned char> passwordBytes = *optPasswordBytes;
            if (!passwordBytes.empty()) {
                std::vector<unsigned char> encryptedPassword = hexStringToBytes(std::string(passwordBytes.begin(), passwordBytes.end()));
                std::cout << GREEN << "Encrypted Password: ";
                for (unsigned char byte : encryptedPassword) {
                    printf("%02X", byte);
                }
                std::cout << RESET;
                crypt.RC6Decrypt128(encryptedPassword.data(), encryptedPassword.data(), encryptedPassword.size());
                std::cout << GREEN << "\nDecrypted password: ";
                std::cout << (std::string{ encryptedPassword.begin(), encryptedPassword.end() }) << RESET << '\n';
            }
            else {
                std::cout << ORANGE << "Password not found at default offset." << RESET << '\n';
            }
            std::cout << RESET;
        }
        else if (command.rfind("/find_password_by_offset", 0) == 0) {
            std::string offsetStr = command.substr(25);
            std::streamoff offset = std::stoll(offsetStr);
            auto optPasswordBytes = readCdbPassword(exePath, offset);
            std::cout << RESET;
            if (!optPasswordBytes) continue;
            std::vector<unsigned char> passwordBytes = *optPasswordBytes;
            if (!passwordBytes.empty()) {
                std::vector<unsigned char> encryptedPassword = hexStringToBytes(std::string(passwordBytes.begin(), passwordBytes.end()));
                crypt.RC6Decrypt128(encryptedPassword.data(), encryptedPassword.data(), encryptedPassword.size());
                std::cout << GREEN << "Decrypted password: ";
                std::cout << (std::string{ encryptedPassword.begin(), encryptedPassword.end() }) << RESET << '\n';
            }
            else {
                std::cout << ORANGE << "Password not found at default offset." << RESET << '\n';
            }
            std::cout << RESET;
        }
        else if (command.rfind("/find_offset_decrypted", 0) == 0) {
            std::string currentPassword = command.substr(23);
            auto foundEncryptedPasswordOffsetOpt = findDecryptedPasswordOffset(exePath, currentPassword, crypt);
            std::cout << RESET;
            if (!foundEncryptedPasswordOffsetOpt) continue;
            std::streamoff passwordOffset = *foundEncryptedPasswordOffsetOpt;
            if (passwordOffset == -1) {
                std::cerr << ORANGE << "Encrypted password not found." << RESET << '\n';
            }
            else {
                std::cout << GREEN << "Password found at offset " << std::dec << passwordOffset << RESET << "\n";
            }
            std::cout << RESET;
        }
        else if (command.rfind("/find_offset_encrypted", 0) == 0) {
            std::string currentPassword = command.substr(23);
            auto foundEncryptedPasswordOffset = findEncryptedPasswordOffset(exePath, currentPassword);
            std::cout << RESET;
            if (!foundEncryptedPasswordOffset) continue;
            std::streamoff passwordOffset = *foundEncryptedPasswordOffset;
            if (passwordOffset == -1) {
                std::cerr << ORANGE << "Encrypted password not found." << RESET << '\n';
            }
            else {
                std::cout << GREEN << "Password found at offset " << std::dec << passwordOffset << RESET << "\n";
            }
        }
        else if (command.rfind("/patch_password_by_offset", 0) == 0) {
            size_t firstSpace = command.find(' ', 26);

            if (firstSpace == std::string::npos) {
                std::cerr << ORANGE << "Error: Invalid command format. Missing space between offset and password." << RESET;
                continue;
            }

            std::string offsetStr = command.substr(26, firstSpace - 26);
            std::streamoff offset = 0;
            try {
                offset = std::stoll(offsetStr);
            }
            catch (const std::invalid_argument& e) {
                std::cerr << ORANGE << "Error: Invalid offset format. Could not convert to number." << RESET;
                continue;
            }

            std::string newPassword = command.substr(firstSpace + 1);

            if (newPassword.size() != 26) {
                std::cerr << ORANGE << "Error: Password size must be exactly 26 characters." << RESET;
                continue;
            }

            patchStringInBinary(exePath, exePath + "_patched", newPassword, offset);
            std::cout << RESET;
        }

        else if (command.rfind("/chpath", 0) == 0) {
            std::string newPath = command.substr(8);
            exePath = newPath;
        }
        else if (command == "/help") {
            printHelp();
        }
        else if (command == "/exit") {
            break;
        }
        else {
            std::cout << ORANGE << "Unknown command: " << command << RESET << "\n";
        }
    }
}



