#include <vector>
#include <iostream>
#include <cstring>
#include <ctime>
#include <optional>

#ifdef _WIN32
#include <corecrt.h>
#endif

#include "../../include/Network/Session.h"
#include "../../include/Utils/Parser.h"
#include "../../include/Enums/ExtrasEnums.h"
#include "../../../MainServer/include/Structures/AccountInfo/MainAccountUniqueId.h"
#include <include/Utils/SetupParser.h>

#include <cryptopp/dh.h>
#include <cryptopp/osrng.h>
#include <cryptopp/secblock.h>
#include <cryptopp/pem.h>
#include <cryptopp/hex.h>
#include <cryptopp/cryptlib.h>
#include <cryptopp/eccrypto.h>
#include <cryptopp/osrng.h>
#include <cryptopp/secblock.h>
#include <cryptopp/oids.h> 
#include <cstring>


namespace Common
{
	namespace Network
	{
		void Session::sendMessage(const std::string& message, std::uint32_t extra)
		{
			std::string completeMessage;
			completeMessage.reserve(16 + message.size());
			completeMessage.append(16, '0');
			completeMessage.append(message);

			Common::Network::Packet response;
			response.setTcpHeader(getId(), Common::Enums::NO_ENCRYPTION);
			response.setCommand(316, 0, extra, completeMessage.size());
			response.setData(reinterpret_cast<std::uint8_t*>(completeMessage.data()), completeMessage.size());
			asyncWrite(response);
		}

		bool Session::asyncWrite(const Common::Network::Packet& message)
		{
			return asyncWriteImpl<PacketType::ENCRYPTED>(message);
		}

		bool Session::asyncWrite(const Common::Network::UnecryptedPacket& message)
		{
			return asyncWriteImpl<PacketType::UNECRYPTED>(message);
		}
	
		void Session::asyncRead()
		{
			if (m_checkValidSession && m_isFirstRead)
			{
				m_isFirstRead = false;
				if (s_loggedIps.contains(m_ip))
				{
					m_isValidSession = true;
					s_loggedIps.erase(m_ip);
				}
			}
			if (m_checkValidSession && !m_isValidSession)
			{
				//::Utils::Logger::log("Invalid session with IP: " + m_ip, ::Utils::LogType::Warning, "Session::onRead");
				closeSocket();
				return;
			}
			if (!m_socket.is_open())
			{
				return;
			}
			m_socket.async_read_some(asio::buffer(m_buffer.data(), m_buffer.size()),
				[this, self = this->shared_from_this()](const asio::error_code& error, std::size_t bytes_transferred)
				{
					this->onRead(error, bytes_transferred);
					asyncRead();
				});
		}

		void Session::onRead(asio::error_code error, std::size_t bytes_transferred)
		{
			if (!error)
			{
				const constexpr int headerSize = sizeof(Common::Protocol::TcpHeader);
				m_reader.insert(m_reader.end(), m_buffer.begin(), m_buffer.begin() + bytes_transferred);

				if (!m_crypt.isUsed) Common::Parser::parse_cast(m_reader.data(), m_reader.size(), 13000, "client", "server");
				//if (m_crypt.isUsed) Common::Parser::parse(m_reader.data(), m_reader.size(), 13000, "client", "server", m_crypt.UserKey);

				Common::Protocol::TcpHeader header;
				Common::Cryptography::Crypt cryptography;
				cryptography.KeySetup(0);

				while (m_reader.size() >= headerSize)
				{
					if (m_crypt.isUsed) 
					{
						cryptography.RC5Decrypt32(reinterpret_cast<int32_t*>(m_reader.data()), &header, headerSize);
					}
					else
					{
						std::memcpy(&header, m_reader.data(), headerSize);
					}

					if (header.getSize() >= 1450)
					{
						std::printf("Session::onRead() - Invalid packet size: %d\n", header.getSize());
						closeSocket();
						return;
					}
					if (m_reader.size() >= static_cast<std::size_t>(header.getSize()))
					{
						std::vector<std::uint8_t> data(m_reader.begin(), m_reader.begin() + header.getSize());
						onPacket(data);
						if (m_reader.empty()) break;

						const auto newSize = m_reader.size() - header.getSize();
						std::memmove(m_reader.data(), m_reader.data() + header.getSize(), newSize);
						m_reader.resize(newSize);
					}
					else
					{
						// avoid infinite loop
						break;
					}
				}
			}
			else
			{
				closeSocket();
			}
		}

		void Session::closeSocket()
		{
			if (!m_socket.is_open())
			{
				return;
			}

			if ((!m_checkValidSession && m_onCloseSocketCallback) || (m_isValidSession && m_onCloseSocketCallback))
			{
				m_onCloseSocketCallback(m_id);
			}

			asio::error_code errorCode;
			auto endPoint = m_socket.remote_endpoint(errorCode);
			
			m_socket.shutdown(tcp::socket::shutdown_both, errorCode);
			m_socket.close(errorCode);
		}


		// Default implementation is used for IPC
		void Session::onPacket(std::vector<std::uint8_t>& data)
		{
			Common::Network::UnecryptedPacket incomingPacket;
			incomingPacket.processIncomingPacket(data.data(), static_cast<std::uint16_t>(data.size()));

			const std::uint16_t callbackNum = incomingPacket.getOrder();
			if (!Common::Network::Session::callbacks<Common::Network::PacketType::UNECRYPTED, Session>.contains(callbackNum))
			{
				std::cout << "[IPC] No callback for order: " << callbackNum << "\n";
				return;
			}
			Common::Network::Session::callbacks<Common::Network::PacketType::UNECRYPTED, Session>[callbackNum](incomingPacket, shared_from_this());
		}


		std::uint8_t* Session::getBufferData()
		{
			return m_buffer.data();
		}

		std::size_t Session::getBufferSize() const
		{
			return m_buffer.size();
		}
		
		Common::Cryptography::Crypt Session::getUserCrypt() const
		{
			return m_crypt;
		}
		
		Common::Cryptography::Crypt Session::getDefaultCrypt() const
		{
			return m_defaultCrypt;
		}
		
		void Session::sendConnectionACK(Common::Enums::ServerType serverType)
		{
			Packet connectionAck;
			connectionAck.setTcpHeader(m_id, Common::Enums::EncryptionType::NO_ENCRYPTION);

			switch (serverType)
			{
				case Enums::AUTH_SERVER:
				{
					struct AuthAck
					{
						std::int32_t key{ static_cast<std::int32_t>(rand() + 1) };
						std::uint32_t timestamp32 = static_cast<std::uint32_t>(std::time(nullptr));
					} authAck;

					m_crypt.KeySetup(authAck.key);
					connectionAck.setData(reinterpret_cast<std::uint8_t*>(&authAck), sizeof(AuthAck));
					connectionAck.setCommand(401, 0, static_cast<int>(Common::Enums::AUTH_SUCCESS), 0);
					asyncWrite(connectionAck);
					break;
				}

				case Enums::MAIN_SERVER:
				{
					struct UniqueId
					{
						std::uint32_t session : 16 = 0;
						std::uint32_t server : 15 = 4;
						std::uint32_t unknown : 1 = 0;
					};

					struct MainAck
					{
						std::int32_t key;
						UniqueId uniqueId{};
						
						MainAck(const Common::Cryptography::Crypt& crypt)
							: key(static_cast<std::int32_t>(rand() + 1))
						{
						}
					};
					MainAck mainAck{ m_crypt };
					mainAck.uniqueId.session = m_id; 
					mainAck.uniqueId.server = 1;    
					m_crypt.KeySetup(mainAck.key);
					connectionAck.setData(reinterpret_cast<std::uint8_t*>(&mainAck), sizeof(MainAck));
					connectionAck.setCommand(401, 0, static_cast<int>(Common::Enums::MAIN_SUCCESS), 1);
					// nb. option = channel selected by user
					// nb. success = MAIN_SUCCESS
					asyncWrite(connectionAck);

					break;
				}

				case Enums::CAST_SERVER:
				{
					m_crypt.isUsed = false;
					struct CastAck
					{
						std::int32_t key{ static_cast<std::int32_t>(rand() + 1) };
					} castAck;
					connectionAck.setData(reinterpret_cast<std::uint8_t*>(&castAck), sizeof(castAck));
					connectionAck.setCommand(401, 0, Common::Enums::CAST_SUCCESS, 0);
					asyncWrite(connectionAck);
					break;
				}

				case Enums::IPC_SERVER:
				{
					m_crypt.isUsed = false;
				}
			}

			asyncRead();
		}

		std::size_t Session::getId() const
		{
			return m_id;
		}
	}
}
