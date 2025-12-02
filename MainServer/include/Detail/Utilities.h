#ifndef DETAILS_HEADER_MAIN_H
#define DETAILS_HEADER_MAIN_H

#include <ConstantDatabase/Structures/SetItemInfo.h>
#include "../Network/MainSession.h"
#include "Network/Packet.h"
#include "Utils/Utils.h"
#include "Utils/Constants.h"
#include <chrono>
#include <format>
#include <array>
#include <vector>
#include <source_location>
#include "../Common/include/Enums/RoomEnums.h"
#include <cstring> 
#include <cstdint>

namespace Main
{
	namespace Classes { class RoomsManager; }
	namespace Details
	{
		enum Orders
		{
			PLAYER_STATE_NOTIFICATION = 312,
			PLAYER_ITEMS_BROADCAST = 414,
		};


		template<typename T, typename PacketType, bool Warn = false>
		requires (std::is_same_v<PacketType, Common::Network::Packet> ||
		std::is_same_v<PacketType, Common::Network::UnecryptedPacket>)
		T parseDataImpl(const PacketType& request, std::uint32_t offset = 0, std::source_location location = std::source_location::current())
		{
			static_assert(std::is_trivially_copyable_v<T>, "Details::parseData requires T to be trivially copyable");

			T t{};
			const std::uint32_t dataSize = request.getDataSize();

			if (offset >= dataSize)
			{
				if constexpr (Warn)
				{
					Utils::Logger::log(std::format(
						"Offset out of bounds! [with offset={}, dataSize={}]",
						offset, dataSize),
						Utils::LogType::Warning,
						std::format("Details::parseData - Caller: {}", location.function_name())
					);
				}
				return t; 
			}

			const std::uint32_t dataAvailable = dataSize - offset;

			if (dataAvailable >= sizeof(T))
			{
				if (Warn && dataAvailable > sizeof(T))
				{
					Utils::Logger::log(std::format(
						"Losing some data because dataAvailable > sizeof(T)! [with dataAvailable={}, sizeof(T)={}, offset={}]",
						dataAvailable, sizeof(T), offset),
						Utils::LogType::Warning,
						std::format("Details::parseData - Caller: {}", location.function_name())
					);
				}

				std::memcpy(&t, request.getData() + offset, sizeof(T));
			}
			else
			{
				if constexpr (Warn)
				{
					Utils::Logger::log(std::format(
						"Data is incomplete because dataAvailable < sizeof(T)! [with dataAvailable={}, sizeof(T)={}, offset={}]",
						dataAvailable, sizeof(T), offset),
						Utils::LogType::Warning,
						std::format("Details::parseData - Caller: {}", location.function_name())
					);
				}

				std::memcpy(&t, request.getData() + offset, dataAvailable);
			}

			return t;
		}


		template<typename T, bool Warn = false>
		T parseData(const Common::Network::Packet& request, std::uint32_t offset = 0, std::source_location location = std::source_location::current())
		{
			return parseDataImpl<T, Common::Network::Packet, Warn>(request, offset, location);
		}

		template<typename T, bool Warn = false>
		T parseData(const Common::Network::UnecryptedPacket& request, std::uint32_t offset = 0, std::source_location location = std::source_location::current())
		{
			return parseDataImpl<T, Common::Network::UnecryptedPacket, Warn>(request, offset, location);
		}

		inline std::uint64_t getUtcTimeMs()
		{
			const auto durationSinceEpoch = std::chrono::system_clock::now().time_since_epoch();
			return static_cast<std::uint64_t>(duration_cast<std::chrono::milliseconds>(durationSinceEpoch).count());
		}

		template<std::size_t N>
		inline std::array<std::uint32_t, N> generateRewards()
		{
			std::array<std::uint32_t, N> rewards{};
			static std::vector<std::uint32_t> predefinedItems;

			if (predefinedItems.empty()) 
			{
				std::ifstream file("../RewardItemIDs.txt");
				if (!file.is_open())
				{
					std::cerr << "[Utilities::generateRewards] error while opening file RewardItemIDs.txt\n";
					return rewards;
				}

				std::string line;
				while (std::getline(file, line)) 
				{
					std::uint32_t itemID = 0;
					auto [ptr, ec] = std::from_chars(line.data(), line.data() + line.size(), itemID);
					if (ec == std::errc()) 
						predefinedItems.push_back(itemID); 
				}
				if (predefinedItems.empty())
				{
					std::cerr << "[Utilities::generateRewards] error: predefinedItems empty, nothing has been parsed\n";
					return rewards;
				}
			}

			static std::mt19937 rng(std::random_device{}());
			static std::uniform_int_distribution<std::size_t> dist(0, predefinedItems.size() - 1);
			std::unordered_set<std::size_t> usedIndexes;

			for (std::size_t i = 0; i < rewards.size(); ++i)
			{
				std::size_t index;
				do {
					index = dist(rng);
				} while (usedIndexes.count(index) > 0); 
				rewards[i] = predefinedItems[index];
				usedIndexes.insert(index); 
			}
			return rewards;
		}

		inline bool isCsdItem(Common::Enums::ItemType itemType, std::uint32_t itemId)
		{
			if (itemType == Common::Enums::ACC_UPPER) return true;
			if (itemType == Common::Enums::MELEE)
			{
				return (itemId >= 3010000 && itemId <= 3010079) // steel hammers
					|| (itemId >= 3010200 && itemId <= 3010279) // wrench
					|| (itemId >= 3010300 && itemId <= 3010379) // chain saw
					|| (itemId == 3010380 || itemId == 3010390 || itemId == 3010280 || itemId == 3010290
						|| itemId == 3010080 || itemId == 3010090 // black / white versions
						|| itemId == 3010100); // basic melee
			}
			else if (itemType == Common::Enums::RIFLE)
			{
				return (itemId >= 3020100 && itemId <= 3020179) || itemId == 3020180 || itemId == 3020190 // peppers
					|| (itemId >= 3020200 && itemId <= 3020279) || itemId == 3020280 || itemId == 3020290 // hornets
					|| (itemId >= 3020300 && itemId <= 3020379) || itemId == 3020380 || itemId == 3020390 // sherlock
					|| itemId == 3020000; // basic rifle
			}
			else if (itemType == Common::Enums::SHOTGUN)
			{
				return itemId == 3030180 || itemId == 3030190 || (itemId >= 3030100 && itemId <= 3030179) // kw
					|| itemId == 3030280 || itemId == 3030290 || (itemId >= 3030200 && itemId <= 3030279) // bombard
					|| itemId == 3030380 || itemId == 3030390 || (itemId >= 3030300 && itemId <= 3030379) // driver
					|| itemId == 3030000; // basic shotgun
			}
			else if (itemType == Common::Enums::SNIPER)
			{
				return itemId == 3040180 || itemId == 3040190 || (itemId >= 3040100 && itemId <= 3040179) // Sea eagle
					|| itemId == 3040280 || itemId == 3040290 || (itemId >= 3040200 && itemId <= 3040279) // Venom
					|| itemId == 3040301 || itemId == 3040302 || (itemId >= 3040300 && itemId <= 3040379) // Vast
					|| itemId == 3040000; // basic sniper
			}
			else if (itemType == Common::Enums::MG)
			{
				return itemId == 3050180 || itemId == 3050190 || (itemId >= 3050100 && itemId <= 3050179) // daredev
					|| itemId == 3050280 || itemId == 3050290 || (itemId >= 3050200 && itemId <= 3050279) // Firefly
					|| itemId == 3050301 || itemId == 3050302 || (itemId >= 3050300 && itemId <= 3050379) // Crank
					|| itemId == 3050000; // basic MG
			}
			else if (itemType == Common::Enums::BAZOOKA)
			{
				return itemId == 3060000 || itemId == 3060001;
			}
			else if (itemType == Common::Enums::GRENADE)
			{
				return itemId == 3070180 || itemId == 3070190 || (itemId >= 3070100 && itemId <= 3070179) // pulse
					|| itemId == 3070280 || itemId == 3070290 || (itemId >= 3070200 && itemId <= 3070279) // exile
					|| itemId == 3070301 || itemId == 3070302 || (itemId >= 3070300 && itemId <= 3070379) // Thunder
					|| itemId == 3070000; // basic grenade
			}
			else if (itemType == Common::Enums::ACC_BACK || itemType == Common::Enums::ACC_WAIST) return false;
			else if ((itemType >= 0 && itemType <= 6) || itemType == 17)
			{
				return CdbUtils::isNoOptionItem(itemId);
			}
			else return false;
		}

		inline bool isBasicItem(Common::Enums::ItemType itemType, std::uint32_t itemId)
		{
			using ItemType = Common::Enums::ItemType;
			static const std::unordered_map<ItemType, std::unordered_set<std::uint32_t>> basicItems = {
				{ ItemType::MELEE,   { 3010100 } },             
				{ ItemType::RIFLE,   { 3020000 } },              
				{ ItemType::SHOTGUN, { 3030000 } },           
				{ ItemType::SNIPER,  { 3040000 } },             
				{ ItemType::MG,      { 3050000 } },     
				{ ItemType::BAZOOKA, { 3060000, 3060001 } },
				{ ItemType::GRENADE, { 3070000 } }                      
			};

			if (itemType == ItemType::ACC_UPPER)
				return true;

			if (auto it = basicItems.find(itemType); it != basicItems.end())
				return it->second.contains(itemId);

			return false;
		}


		void broadcastPlayerItems(Main::Classes::RoomsManager& roomsManager, std::shared_ptr<Main::Network::Session> session, const Common::Network::Packet& request);
	}
}

#endif
