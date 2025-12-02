#ifndef MAIN_DATABASE_MANAGER_H
#define MAIN_DATABASE_MANAGER_H

#include <string>
#include <thread>
#include "../MainEnums.h"
#include "Utils/SetupParser.h"
#include "Utils/Logger.h"
#include "Utils/Constants.h"

#include <mariadb/conncpp.hpp>
#include <mariadb/conncpp/Connection.hpp>
#include "../Structures/AccountInfo/MuteInfo.h"

namespace Main 
{ 
	namespace Structures 
	{ 
		struct Item; struct BoughtItem; struct EquippedItem; struct DetailedEquippedItem; struct AccountInfo; struct MuteInfo;
		struct BoxItem; struct BoughtItemToProlong; struct Friend; struct BlockedPlayer; struct SingleMapEvent; struct SingleModeEvent;
		struct Giftbox; struct Mailbox;
	} 
}

namespace Main
{
	namespace Persistence
	{
		class PersistentDatabase
		{
		private:
			sql::Connection* m_con;

			using Item = Main::Structures::Item;
			using BoughtItem = Main::Structures::BoughtItem;
			using EquippedItem = Main::Structures::EquippedItem;
			using DetailedEquippedItem = Main::Structures::DetailedEquippedItem;

		public:

			PersistentDatabase();
			void pingDatabase();
			void reconnect();
			void updatePlayerCurrencyByType(std::uint32_t accountID, std::uint32_t newAmount, Main::Enums::ItemCurrencyType currencyType);
			void addPlayerAchievement(std::uint32_t accountID, std::uint32_t achievementIndex);
			void logMessage(std::uint32_t accountID, const std::string& message);
			std::vector<std::pair<std::uint32_t, std::uint32_t>> getPlayerAchievements(std::uint32_t accountID);
			std::unordered_map<std::uint32_t, std::uint32_t> getPlayerMissions(std::uint32_t accountID);
			bool updatePlayerMissionProgress(std::uint32_t accountID, std::uint32_t missionID, std::uint32_t newProgress);
			void updateLatestSelectedCharacter(std::uint32_t accountID, std::uint16_t characterId);
			bool savePlayerMissions(std::uint32_t accountID, const std::unordered_map<std::uint32_t, std::uint32_t>& activeMissions);
			std::optional<std::pair<Main::Structures::AccountInfo, std::string>> getPlayerInfoByNickname(const std::string& nickname);
			std::optional<Main::Structures::AccountInfo> getPlayerInfo(std::uint32_t playerID);
			Main::Structures::MuteInfo isMuted(std::uint32_t playerID);
			std::optional<std::string> getRoomCreationDisabledUntil(const std::string& nickname);
			std::optional<Main::Structures::BanInfo> getBanInfoByNickname(const std::string& nickname);
			std::optional<Main::Structures::MuteInfo> getMuteInfoByNickname(const std::string& nickname);
			bool isRoomCreationDisabled(std::uint32_t playerID);
			bool isVotekickDisabled(std::uint32_t playerID);
			std::optional<std::string> getVotekickDisabledUntil(const std::string& nickname);
			bool updateVotekickDisabledUntil(const std::string& nickname, const std::string& until);
			bool resetVotekickDisabledUntil(const std::string& nickname);
			bool unbanPlayer(const std::string& nickname);
			bool addPlayer(const std::string& username, const std::string& password, const std::string& nickname);
			auto getPlayerItems(std::uint32_t playerID) -> std::pair<std::vector<Item>, std::unordered_map<std::uint16_t, std::vector<EquippedItem>>>;
			bool addPlayerItems(std::uint32_t accountID, const std::vector<Item>& items, std::uint32_t latestCharacterSelected = -1);
			bool replaceItem(std::uint32_t accountID, std::uint64_t itemNumber, std::uint32_t newItemId);
			bool updateItemStock(std::uint32_t accountID, std::uint64_t itemNumber, std::uint32_t newStock);
			bool replaceItemResetEnergy(std::uint32_t accountID, std::uint64_t itemNumber, std::uint32_t newItemId);
			bool addPlayerCouponItem(std::uint32_t accountID, const Item& item);
			void addPlayerBoughtItems(std::uint32_t accountID, const std::vector<BoughtItem>& boughtItems, std::uint32_t latestCharacterSelected = -1)
			{
				addPlayerItems(accountID, std::vector<Item>(boughtItems.begin(), boughtItems.end()), latestCharacterSelected);
			}
			void addPlayerBoxItems(std::uint32_t accountID, const std::vector<Main::Structures::BoxItem>& boxItems, std::uint32_t latestCharacterSelected = -1)
			{
				addPlayerItems(accountID, std::vector<Item>(boxItems.begin(), boxItems.end()), latestCharacterSelected);
			}
			void addPlayerTradedItems(std::uint32_t accountID, const std::vector<Main::Structures::TradeBasicItem>& tradedItems, std::uint32_t latestCharacterSelected = -1)
			{
				addPlayerItems(accountID, std::vector<Item>(tradedItems.begin(), tradedItems.end()), latestCharacterSelected);
			}
			void addPlayerTradedItem(std::uint32_t accountID, const Main::Structures::TradeBasicItem& tradedItem, std::uint32_t latestCharacterSelected = -1)
			{
				addPlayerTradedItems(accountID, std::vector<Main::Structures::TradeBasicItem>{ tradedItem }, latestCharacterSelected);
			}
			void prolongItems(std::uint32_t accountID, const std::vector<Main::Structures::BoughtItemToProlong>& toProlongItems,
				const std::vector<std::uint64_t>& itemDurations, std::uint32_t timeNow);

			void addPlayerItem(const Item& item, std::uint32_t accountID, std::uint32_t latestCharacterSelected = -1);
			bool removePlayerItem(std::uint32_t accountId, std::uint64_t itemNumber, const std::string& caller);
			void updatePlayerLevel(std::uint32_t accountID, std::uint16_t level);
			void updatePlayerExperience(std::uint32_t accountID, std::uint32_t exp);
			bool updateHwid(std::uint32_t accountId, const std::string& hwid);
			std::optional<bool> hasBeenMatchBanned(std::uint32_t accountId);
			std::optional<bool> hasBeenMatchBannedByNick(const std::string& nickname);
			bool updatePlayerName(std::uint32_t accountID, const char* name);
			bool updateSuspension(const std::string& nickname, const std::string& until, const std::string& reason, std::uint32_t executorGrade);
			void updateLatestRewardDay(const std::string& columnName, std::uint32_t accountId, const std::string& rewardDay);
			std::string getLatestRewardDayFor(const std::string& columnName, std::uint32_t accountId);
			bool mustRewardsBeUpdated(const std::string& tableName, std::uint32_t daysToCheck);
			
			bool mustMonthlyRewardsBeUpdated()
			{
				std::chrono::year_month_day ymd = std::chrono::year_month_day(std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now()));
				std::chrono::year_month_day next_month_first_day = std::chrono::year_month_day{ ymd.year(), ymd.month() + std::chrono::months{1}, std::chrono::day{1} };
				std::chrono::sys_days last_day_of_current_month = std::chrono::sys_days(next_month_first_day) - std::chrono::days{ 1 };
				std::chrono::year_month_day last_ymd = std::chrono::year_month_day(last_day_of_current_month);
				return mustRewardsBeUpdated("MonthlyRewards", static_cast<std::uint32_t>(last_ymd.day()));
			}
			bool mustWeeklyRewardsBeUpdated()
			{
				return mustRewardsBeUpdated("WeeklyRewards", 7);
			}

			void updateRewards(const std::string& tableName, const std::vector<std::uint32_t>& items);

			void updateWeeklyRewards(const std::array<std::uint32_t, 7>& weeklyItems)
			{
				updateRewards("WeeklyRewards", { weeklyItems.begin(), weeklyItems.end() });
			}

			void updateMonthlyRewards(const std::array<std::uint32_t, 32>& monthlyItems)
			{
				updateRewards("MonthlyRewards", { monthlyItems.begin(), monthlyItems.end() });
			}

			template<typename RewardsGenerator, typename UpdateFunction, typename RequiresUpdateF, std::size_t N>
			std::array<std::uint32_t, N> getRewards(const std::string& tableName, const std::string& limit, RewardsGenerator rewardsGenerator,
				UpdateFunction updateFunction, RequiresUpdateF requiresUpdate)
			{
				if (requiresUpdate())
				{
					auto generatedRewards = rewardsGenerator();
					updateFunction(generatedRewards);
					return generatedRewards;
				}
				else
				{
					std::array<std::uint32_t, N> rewards{};
					try
					{
						const std::string queryStr = "SELECT ItemID FROM " + tableName + " LIMIT " + limit;
						std::unique_ptr<sql::PreparedStatement> stmt(m_con->prepareStatement(queryStr));

						std::unique_ptr<sql::ResultSet> res(stmt->executeQuery());
						std::size_t index = 0;
						while (res->next() && index < rewards.size())
						{
							rewards[index] = res->getInt("ItemID");
							++index;
						}
						if (index < rewards.size())
						{
							Utils::Logger::log("[PersistentDatabase::getRewards] WARNING: " + tableName + " table contains < " 
								+ limit + " itemIDs!", Utils::LogType::Warning, "PersistentDatabase::getRewards");
						}
					}
					catch (const sql::SQLException& e)
					{
						Utils::Logger::log("[Error] Exception during rewards retrieval: " + std::string(e.what()), Utils::LogType::Error, 
							"PersistentDatabase::getRewards");
					}
					return rewards;
				}
			}


			template<typename RewardsGenerator>
			std::array<std::uint32_t, 7> getWeeklyRewards(RewardsGenerator rewardsGenerator)
			{
				auto l = [this](const std::array<std::uint32_t, 7>& rewards) { updateWeeklyRewards(rewards); };
				auto l2 = [this]() { return mustWeeklyRewardsBeUpdated(); };
				return getRewards<RewardsGenerator, decltype(l), decltype(l2), 7>("WeeklyRewards", "7", rewardsGenerator, l, l2);
			}

			template<typename RewardsGenerator>
			std::array<std::uint32_t, 32> getMonthlyRewards(RewardsGenerator rewardsGenerator)
			{
				auto l = [this](const std::array<std::uint32_t, 32>& rewards) { updateMonthlyRewards(rewards); };
				auto l2 = [this]() { return mustMonthlyRewardsBeUpdated(); };
				return getRewards<RewardsGenerator, decltype(l), decltype(l2), 32>("MonthlyRewards", "32", rewardsGenerator, l, l2);
			}

			void reduceDurability(std::uint32_t accountId, const std::vector<std::pair<std::uint32_t, std::uint64_t>>& equippedItems);
			void updateItemDurability(std::uint32_t accountId, std::uint32_t itemNumber, std::uint32_t newDurability);
			void updateBattery(std::uint32_t accountId, std::uint32_t newBattery);
			void updatePlayerStats(std::uint32_t accountId, const Main::Structures::AccountInfo& updatedAccountInfo);
			void updateClanContribution(std::uint32_t clanId, std::uint32_t contribution);
			void updateClanStats(std::uint32_t clanId, Main::Enums::MatchEnd result);
			bool updateMute(const std::string& nickname, const std::string& until, const std::string& reason, const std::string& mutedBy, std::uint32_t executorGrade);
			bool unmuteAccount(const std::string& nickname);
			bool updateRoomCreationDisabledUntil(const std::string& nickname, const std::string& until);
			bool resetRoomCreationDisabledUntil(const std::string& nickname);
			void switchItemEquip(std::uint32_t accountID, std::uint64_t itemNumber, std::uint32_t characterId);
			void unequipItem(std::uint32_t accountID, std::uint64_t unequipItemNumber);
			void equipItem(std::uint32_t accountID, std::uint64_t equipItemNumber, std::uint16_t characterId);
			void swapItems(std::uint32_t accountID, std::uint64_t toUnequipItemNumber, std::uint64_t toEquipItemNumber, std::uint16_t characterId);
			void addFriend(std::uint32_t accountID, std::uint32_t targetAccountId);
			bool resetKillDeath(std::uint32_t accountID);
			bool resetRecord(std::uint32_t accountID);
			bool batteryRecharge(std::uint32_t accountID, std::uint32_t quantity);
			bool batteryExpansion(std::uint32_t accountID);
			bool inventoryExpansion(std::uint32_t accountID, std::uint32_t spaceToAdd);
			void removeFriend(std::uint32_t accountID, std::uint32_t targetAccountId);
			std::vector<Main::Structures::Friend> loadFriends(std::uint32_t accountID);
			std::vector<Main::Structures::BlockedPlayer> loadBlockedPlayers(std::uint32_t accountID);
			void blockPlayer(std::uint32_t accountID, std::uint32_t targetAccountId);
			std::optional<std::uint32_t> blockPlayerByNickname(std::uint32_t accountID, const std::string& targetNickname);
			void unblockPlayer(std::uint32_t accountID, std::uint32_t targetAccountId);
			Main::Enums::AddFriendServerExtra addPendingFriendRequest(std::uint32_t aid, const char* targetName);
			std::vector<Main::Structures::Friend> loadPendingFriendRequests(std::uint32_t accountID);
			std::vector<Main::Structures::SingleModeEvent> getEventsModeList();
			std::vector<Main::Structures::SingleMapEvent> getEventsMapList();
			void storeMailbox(const Main::Structures::Mailbox& mailbox, std::uint32_t accountId, bool isSent);
			bool storeGiftbox(const Main::Structures::Giftbox& giftbox, std::uint32_t accountId);
			bool storeGiftbox(const std::string& nickname, const std::string& giftDescription, std::uint32_t itemId);
			Main::Enums::MailboxExtra storeOfflineMailbox(const Main::Structures::Mailbox& mailbox, const char* senderNickname);
			std::vector<Main::Structures::Mailbox> getNewMailboxes(std::uint32_t accountID);
			void updateReadMailbox(std::uint32_t accountID, std::uint32_t timestamp);
			void deleteMailbox(std::uint32_t timestamp, std::uint32_t accountId, bool isSent);
			void deleteReceivedGiftbox(std::uint32_t accountId, std::uint32_t timestamp);
			std::pair<std::vector<Main::Structures::Mailbox>, std::vector<Main::Structures::Mailbox>> loadMailboxes(std::uint32_t accountID);
			std::vector<Main::Structures::Giftbox> loadReceivedGiftboxes(std::uint32_t accountID);
			void insertEnergyToItem(std::uint32_t accountID, std::uint64_t itemNumber, std::uint32_t newItemEnergy, std::uint32_t newTotalEnergy);
			void updatePlayerLuckyPoints(std::uint32_t accountID, std::uint32_t luckyPoints);

			// Logs
			bool insertItemLogs(std::uint32_t accountId, const std::vector<Main::Structures::ItemLogInfo>& logs);
			bool insertItemLog(std::uint32_t accountId, const Main::Structures::ItemLogInfo& log);
			bool logBoughtItems(std::uint32_t accountId, const std::vector<Main::Structures::BoughtItem>& boughtItems, bool isCouponItems);
			bool logBoxItems(std::uint32_t accountId, const std::vector<Main::Structures::BoxItem>& boxItems);

			bool logProlongedItems(std::uint32_t accountId, const std::vector<Main::Structures::BoughtItemToProlong>& boughtItems,
				const std::vector<std::uint64_t>& itemDurations);

			// Events
			bool updateExpMpBonusInfo(const Main::Structures::ExpMpBonusInfo& info);
			std::optional<Main::Structures::ExpMpBonusInfo> getExpMpBonusInfo();
			bool updateEventInfo(const std::string& tableName, const Main::Structures::EventMissionInfo& info);
			std::optional<Main::Structures::EventMissionInfo> getEventInfo(const std::string& tableName);
			bool updateCapsuleEvent(const Main::Structures::CapsuleListDatabase& capsule);
			std::optional<Main::Structures::CapsuleListDatabase> getCapsuleEvent();

			std::optional<Main::Structures::EventMissionInfo> getEventMissionsInfo()
			{
				return getEventInfo("EventMissionsInfo");
			}

			bool updateEventMissionsInfo(const Main::Structures::EventMissionInfo& info)
			{
				return updateEventInfo("EventMissionsInfo", info);
			}

			std::optional<Main::Structures::EventMissionInfo> getTradeEventsInfo()
			{
				return getEventInfo("TradeEvents");
			}

			bool updateTradeEventsInfo(const Main::Structures::EventMissionInfo& info)
			{
				return updateEventInfo("TradeEvents", info);
			}

			bool setCommandEventExpirationHours(std::uint32_t hoursFromNow);

			bool isCommandEventExpired();
		};
	}
}

#endif
