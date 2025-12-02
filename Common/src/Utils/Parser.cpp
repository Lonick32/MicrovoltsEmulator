
#include <iostream>
#include <cstring>


#include "../../include/Utils/Parser.h"

#ifdef _WIN32
#include <Windows.h>
#include <corecrt.h>
#endif


#include <fstream>

namespace Common
{
	namespace Parser
	{
#ifdef _WIN32
		HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
		void setConsoleColor(int color) 
		{
		    SetConsoleTextAttribute(hConsole, color);
		}
#else
		void setConsoleColor(int color) {
		
		    switch(color) {
		        case 2: std::cout << "\033[32m"; break; // green
		        case 3: std::cout << "\033[36m"; break; // cyan
		        case 5: std::cout << "\033[35m"; break; // magenta
		        case 7: std::cout << "\033[0m";  break; // reset
		        default: std::cout << "\033[0m"; break;
		    }
		}
#endif

		std::ofstream logFile("log.txt", std::ios::app);
		void log(const std::string& message)
		{
		    std::cout << message;
		    /*
		    if (logFile.is_open()) {
		        logFile << message << std::endl;
		    }
		    */
		}

		void printTcpHeader(Common::Protocol::TcpHeader header)
		{
			log("[Actual Size:" + std::to_string(header.getSize()) + "] ");
			log("[Bogus/Padding:" + std::to_string(header.getBogus()) + "] ");
			log("[SessionID:" + std::to_string(header.getSessionId()) + "] ");
			log("[Crypt:" + std::to_string(header.getCrypt()) + "] ");
		}

		void printCommandHeader(Common::Protocol::CommandHeader command)
		{
			log("[Order:" + std::to_string((uint32_t)command.getOrder()) + "] ");
			log("[Mission:" + std::to_string((uint32_t)command.getMission()) + "] ");
			log("[Extra:" + std::to_string((uint32_t)command.getExtra()) + "] ");
			log("[Option:" + std::to_string((uint32_t)command.getOption()) + "] ");
			log("[Padding:" + std::to_string((uint32_t)command.getBogus()) + "] ");
		}

		void parseCommandHeader(std::uint8_t* data)
		{
			log("Command Header:");
			std::uint32_t actualCommand; memcpy(&actualCommand, data + 4, sizeof(uint32_t));
			Common::Protocol::CommandHeader command(actualCommand);
			log("[Mission:" + std::to_string(command.getMission()) + "] ");
			log("[Order:" + std::to_string(command.getOrder()) + "] ");
			log("[Extra:" + std::to_string(command.getExtra()) + "] ");
			log("[Option:" + std::to_string(command.getOption()) + "] ");
			log("[Padding:" + std::to_string(command.getBogus()) + "] ");
		}

		std::pair<std::size_t, uint32_t> parseTcpHeader(std::uint8_t* data)
		{
			log("\nTcp Header:");
			std::uint32_t header;
			memcpy(&header, data, sizeof(std::uint32_t));
			Common::Protocol::TcpHeader parsedHeader(header);
			log("[Actual Size:" + std::to_string(parsedHeader.getSize()) + "] ");
			log("[Bogus/Padding:" + std::to_string(parsedHeader.getBogus()) + "] ");
			log("[SessionID:" + std::to_string(parsedHeader.getSessionId()) + "] ");
			log("[Crypt:" + std::to_string(parsedHeader.getCrypt()) + "] ");
			std::size_t sizeRetrievedFromTcpHeader = parsedHeader.getSize();
			std::uint32_t toCrypt = parsedHeader.getCrypt();
			return std::pair{ sizeRetrievedFromTcpHeader, toCrypt };
		}

		void parseDecryptedPacket(std::size_t len, std::uint8_t* data)
		{
			parseCommandHeader(data);
			log("Decrypted Packet:");
			std::string packetData;
			for (std::size_t i = 0; i < len; ++i)
			{
				char buffer[4];
				snprintf(buffer, sizeof(buffer), "%02X ", static_cast<uint8_t>(data[i]));
				packetData += buffer;
			}
			log(packetData);
		}
	
		void parse(std::uint8_t* data, std::size_t len, std::size_t port, const std::string& origin, const std::string& to, std::int32_t cryptKey, bool first)
		{
			setConsoleColor(2);
			std::cout << "\n[" << origin << "->" << to << "]";
			setConsoleColor(3);

			setConsoleColor(5);
			std::cout << "[Size:" << len << "] \n";


			Common::Cryptography::Crypt cryptDefault;
			cryptDefault.KeySetup(0);
			Common::Cryptography::Crypt userCrypt;
			userCrypt.KeySetup(cryptKey);

			cryptDefault.RC5Decrypt32(data, data, 4);
			std::uint32_t actualData;
			memcpy(&actualData, data, sizeof(std::uint32_t));
			setConsoleColor(5);

			const Common::Protocol::TcpHeader header(actualData);
			printTcpHeader(header);

			const int toCrypt = header.getCrypt();
			const std::size_t actualSize = header.getSize();
			cryptDefault.RC5Encrypt32(data, data, 4);

			setConsoleColor(7);
			for (std::size_t i = 0; i < actualSize; ++i)
			{
				printf("%02X ", static_cast<std::uint8_t>(data[i]));
			}
			std::cout << '\n';

			Common::Cryptography::Crypt givenCrypt;
			givenCrypt.KeySetup(cryptKey);
			if (first)
			{
				cryptDefault.RC5Decrypt32(data, data, 4);
				std::cout << "Decrypted Packet: " << std::endl;
				std::cout << std::endl;
				for (std::size_t i = 0; i < actualSize; ++i)
				{
					printf("%02X ", static_cast<std::uint8_t>(data[i]));
				}
				givenCrypt.RC5Encrypt32(data, data, 4);
				std::uint32_t actualCommand;
				memcpy(&actualCommand, data + 4, sizeof(std::uint32_t));
				printCommandHeader(Common::Protocol::CommandHeader{ actualCommand });
				printf("\n");
				return;
			}
			
			else {
				switch (toCrypt) {
				case 0: // no crypt
					parseDecryptedPacket(actualSize, data);
					break;

				case 1:
					cryptDefault.RC5Decrypt64(data + 4, data + 4, static_cast<int>(actualSize - 4));
					parseDecryptedPacket(actualSize, data);
					cryptDefault.RC5Encrypt64(data + 4, data + 4, static_cast<int>(actualSize - 4));
					break;

				case 3:
					cryptDefault.RC6Decrypt128(data + 4, data + 4, static_cast<int>(actualSize - 4));
					parseDecryptedPacket(actualSize, data);
					cryptDefault.RC6Encrypt128(data + 4, data + 4, static_cast<int>(actualSize - 4));
					break;

				case 2:
					userCrypt.RC5Decrypt64(data + 4, data + 4, static_cast<int>(actualSize - 4));
					parseDecryptedPacket(actualSize, data);
					userCrypt.RC5Encrypt64(data + 4, data + 4, static_cast<int>(actualSize - 4));
					break;

				case 4:
					userCrypt.RC6Decrypt128(data + 4, data + 4, static_cast<int>(actualSize - 4));
					parseDecryptedPacket(actualSize, data);
					userCrypt.RC6Encrypt128(data + 4, data + 4, static_cast<int>(actualSize - 4));
					break;

				default:
					std::cerr << "Invalid crypt found!\n";
					break;
				}
			}
		}

		void parse_cast(std::uint8_t* data, std::size_t len, std::size_t port, const std::string& origin, const std::string& to)
		{
			std::uint32_t actualData;
			memcpy(&actualData, data, sizeof(std::uint32_t));

			const Common::Protocol::TcpHeader header(actualData);
			std::uint32_t actualCommand;

			memcpy(&actualCommand, data + 4, sizeof(std::uint32_t));
			Common::Protocol::CommandHeader commandHeader{ actualCommand };

			if (commandHeader.getOrder() == 281 || commandHeader.getOrder() == 282) return;

			log("\n[" + origin + "->" + to + "]");
			log("[CastServer:" + std::to_string(port) + "]");
			log("[Size:" + std::to_string(len) + "]");

			printTcpHeader(header);
			std::string hexData;
			for (std::size_t i = 0; i < len; ++i)
			{
				char buffer[4];
				snprintf(buffer, sizeof(buffer), "%02X ", static_cast<std::uint8_t>(data[i]));
				hexData += buffer;
			}
			log(hexData);

			printCommandHeader(commandHeader);
		}
	}
}

