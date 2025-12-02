#ifndef PLAYER_CLASS_H
#define PLAYER_CLASS_H

#include "../Structures/Item/MainEquippedItem.h"
#include "../Structures/Item/MainBoughtItem.h"
#include "../Structures/AccountInfo/MainAccountInfo.h"
#include "../Structures/PlayerLists/Friend.h"
#include "../Structures/Mailbox.h"
#include "../Structures/PlayerLists/BlockedPlayer.h"
#include "../Structures/Item/MainItem.h"
#include "../Persistence/MainScheduler.h"
#include "../Structures/TradeSystem/TradeSystemItem.h"
#include "../Structures/ClientData/Structures.h"

#include <unordered_map>
#include <vector>
#include <array>
#include <optional>

namespace Common { namespace Network { class Session; } }
namespace Main { namespace Network { class Session; } }
namespace Main
{
	namespace Classes
	{
		class Player
		{
		private:
			using Item = Main::Structures::Item;
			using EquippedItem = Main::Structures::EquippedItem;
			using DetailedEquippedItem = Main::Structures::DetailedEquippedItem;
			using BoughtItem = Main::Structures::BoughtItem;
			using AccountInfo = Main::Structures::AccountInfo;
			using Session = Main::Network::Session;
			using BlockedPlayer = Main::Structures::BlockedPlayer;
			using Friend = Main::Structures::Friend;
			using Mailbox = Main::Structures::Mailbox;
			using Giftbox = Main::Structures::Giftbox;
			using TradedItem = Main::Structures::TradeBasicItem;

			AccountInfo m_accountInfo{};
			std::unordered_map<std::uint64_t, Item> m_itemsByItemNumber{};
			std::array<EquippedItem, Common::Enums::MAX_ITEMTYPE* Common::Enums::MAX_CHARACTERS> m_equippedItemByCharacter{}; // originally 2D, then flattened
			std::uint64_t m_totalEquippedItems{};
			std::unordered_map<Friend, std::weak_ptr<Session>> m_friends;
			std::vector<BlockedPlayer> m_blockedAccounts{};
			Common::Enums::PlayerState m_playerState{};
			std::uint16_t m_ping{};
			bool m_isMuted{ false };
			bool m_isRoomCreationEnabled{ true };
			bool m_isVotekickEnabled{true};
			std::string m_mutedBy{};
			std::string m_muteReason{};
			std::string m_mutedUntil{};
			std::string m_latestWeeklyRewardDay{};
			std::string m_latestMonthlyRewardDay{};

			// Mailbox/Giftbox specific
			std::vector<Mailbox> m_mailboxReceived{};
			std::vector<Mailbox> m_mailboxSent{};
			std::vector<Giftbox> m_giftboxReceived{};

			// Other
			std::uint16_t m_roomNumber{};
			std::uint16_t m_clanRoomNumber{};
			bool m_isInMatch{};
			std::uint32_t m_batteryObtainedInMatch{};
			std::vector<Item> m_couponItems;

			// Trade system
			std::uint32_t m_currentlyTradingWithAccountId{};
			std::vector<TradedItem> m_tradedItems{};
			bool m_hasPlayerLocked{};

		public:
			// Account info
			void setAccountInfo(const AccountInfo& accountInfo);
			void addBatteryObtainedInMatch(std::uint32_t newBattery);
			void storeBatteryObtainedInMatch();
			const AccountInfo& getAccountInfo() const;
			std::uint32_t getAccountID() const;
			const char* const getPlayerName() const;
			bool setAccountRockTotens(std::uint32_t rt);
			bool setAccountMicroPoints(std::uint32_t mp);
			bool setAccountCoins(std::uint16_t coins);
			void setAccountLatestCharacterSelected(std::uint16_t latestCharacterSelected);
			void setLevel(std::uint16_t level);
			void setExperience(std::uint32_t exp);
			void setPlayerName(const char* playerName);
			bool hasEnoughInventorySpace(std::uint16_t totalNewItems) const;
			Main::ClientData::CouponItemAddRet addCoupon(std::uint32_t stockToAdd);
			void addTotalCouponItems(const Item& item);
			Main::ClientData::CouponItemUseRet tryRemoveCoupons(std::uint32_t totalCouponsNeeded);
			std::size_t getTotalCoupons() const noexcept;
			void setPlayerState(Common::Enums::PlayerState playerState);
			Common::Enums::PlayerState getPlayerState() const;
			void addLuckyPoints(std::uint32_t points);
			void setLuckyPoints(std::uint32_t points);
			std::uint32_t getLuckyPoints() const;
			void setPing(std::uint16_t ping);
			std::uint16_t getPing() const;
			bool isInLobby() const;
			void mute(const std::string& reason, const std::string& mutedBy, const std::string& mutedUntil);
			void unmute();
			void disableRoomCreation();
			void enableRoomCreation();
			bool isRoomCreationEnabled() const noexcept;
			void disableVotekick();
			void enableVotekick();
			bool isVotekickEnabled() const noexcept;
			Main::Structures::MuteInfo getMuteInfo() const;
			bool isMuted() const;
			void resetKillDeath();
			void resetRecord();
			bool expandBattery();
			bool expandInventory(std::uint32_t spaceToAdd);

			// Friends
			const std::vector<Friend> getFriendlist() const;
			std::unordered_map<Friend, std::weak_ptr<Session>>& getFriendSessions();
			void setFriendList(const std::vector<Friend>& friendlist);
			void updateFriend(const Friend& targetFriend, std::shared_ptr<Main::Network::Session> targetSession, bool remove);
			// call once with default "persist", since removeFriend removes the friend for both players
			bool deleteFriend(std::uint32_t targetAccountId);
			void addOfflineFriend(const Main::Structures::Friend& ffriend);
			std::optional<Main::Structures::Friend> addOnlineFriend(std::shared_ptr<Main::Network::Session> session);
			bool isFriend(std::uint32_t accountId) const;

			/********* Player items related /*********/
			void setUnequippedItems(const std::vector<Item>& items);
			bool isItemTradeable(const Main::Structures::ItemSerialInfo& itemSerialInfo) const;
			std::optional<std::uint32_t> findItemIdBySerialInfo(const Main::Structures::ItemSerialInfo& itemSerialInfo) const;
			std::optional<Main::Structures::ItemSerialInfo> getBossBattleTicket() const;
			std::optional<std::pair<std::uint32_t, std::uint32_t>>
			findItemIdAndDurabilityBySerialInfo(const Main::Structures::ItemSerialInfo& itemSerialInfo) const;
			std::optional<std::uint64_t> findMaxItemNumber() const;
			bool prolongItem(const Main::Structures::ItemSerialInfo& newItemSerialInfo);
			const std::array<EquippedItem, Common::Enums::MAX_CHARACTERS* Common::Enums::MAX_ITEMTYPE>& getEquippedItems() const;
			std::vector<EquippedItem> getEquippedItemsFor(std::uint16_t characterID) const;
			std::vector<EquippedItem> getUnlimitedEquippedWeaponsFor(std::uint16_t characterID) const;
			const std::unordered_map<std::uint64_t, Item>& getItems() const;
			const std::vector<Item> getItemsAsVec() const;
			bool deleteItemBasic(const Main::Structures::ItemSerialInfo& itemSerialInfo);
			void addItems(const std::vector<Item>& items);
			void addItem(const Item& item);
			void addItems(const std::vector<BoughtItem>& boughtItems);
			void addItems(const std::vector<Main::Structures::BoxItem>& boxItems);
			void setEquippedItems(const std::unordered_map<std::uint16_t, std::vector<EquippedItem>>& equippedItems);
			std::optional<std::pair<std::uint16_t, std::uint64_t>>
				addEnergyToItem(const Main::Structures::ItemSerialInfo& itemSerialInfo, std::uint32_t energyAdded);
			std::pair<std::vector<Main::ClientData::SingleWeaponDurabilityDamage>,
				std::vector<std::pair<std::uint32_t, std::uint64_t>>> reduceEquippedItemsDurabilities(std::size_t characterID, std::uint32_t weaponRestriction);
			bool updateItemDurabilityByNumber(std::uint32_t itemNumber, std::uint32_t newDurability);
			std::optional<std::uint16_t> getItemEnergy(const Main::Structures::ItemSerialInfo& itemSerialInfo) const;

			// ugly design but easier to write, ideally we shouldn't pass the scheduler to this function...
			void equipItem(const std::uint16_t itemNumber, Main::Persistence::MainScheduler& scheduler, std::uint32_t character = -1);
			std::optional<std::uint64_t> unequipItem(uint64_t itemType, Main::Persistence::MainScheduler& scheduler);
			std::uint64_t getTotalEquippedItems() const;
			std::uint32_t addBattery(std::uint32_t battery);

			void unequipItemImpl(std::uint64_t itemType, Main::Persistence::MainScheduler& scheduler, std::uint32_t character = -1);

			std::uint64_t getLatestItemNumber() const;
			std::pair<Common::Enums::MatchItemAction, std::uint32_t> useInstantRespawn(std::uint64_t itemNum);
			void setLatestItemNumber(std::uint64_t itemNum);
			std::pair<std::array<std::uint32_t, 10>, std::array<std::uint32_t, 7>> getEquippedItemsSeparated() const;
			bool unequipItemIfEquipped(std::uint64_t itemNumber, std::uint32_t characterId, Main::Persistence::MainScheduler& scheduler);
			void equipItemIfNotEquipped(std::uint64_t itemNumber, std::uint32_t characterId, Main::Persistence::MainScheduler& scheduler);

			// Blocked players
			bool blockAccount(std::uint32_t accountId, const char* nickname);
			bool unblockAccount(std::uint32_t accountId);
			bool hasBlocked(std::uint32_t accountId) const;
			const std::vector<Main::Structures::BlockedPlayer>& getBlockedPlayers() const;
			void setBlockedPlayers(const std::vector<Main::Structures::BlockedPlayer>& blockedPlayers);

			// Mailbox, giftbox
			void addMailboxReceived(const Main::Structures::Mailbox& mailbox);
			void addGiftboxReceived(const Main::Structures::Giftbox& giftbox);
			void addMailboxSent(const Main::Structures::Mailbox& mailbox);
			bool deleteSentMailbox(std::uint32_t timestamp);
			bool deleteReceivedMailbox(std::uint32_t timestamp);
			const std::vector<Main::Structures::Mailbox>& getMailboxReceived() const;
			const std::vector<Main::Structures::Mailbox>& getMailboxSent() const;
			const std::vector<Main::Structures::Giftbox>& getGiftboxReceived() const;
			std::optional<Main::Structures::Giftbox> getGiftbox(std::uint32_t timestamp) const;
			void setMailbox(const std::vector<Main::Structures::Mailbox>& mailbox, bool sent);
			void setReceivedGiftboxes(const std::vector<Main::Structures::Giftbox>& gifbox);
			std::optional<std::uint32_t> getItemIdFromGiftbox(std::uint32_t timestamp) const;
			void deleteGiftbox(std::uint32_t timestamp);

			// Rewards
			void setLatestWeeklyRewardDate(const std::string& date) { m_latestWeeklyRewardDay = date; }
			const std::string getLatestWeeklyRewardDate() const { return m_latestWeeklyRewardDay; }
			void setLatestMonthlyRewardDate(const std::string& date) { m_latestMonthlyRewardDay = date; }
			const std::string getLatestMonthlyRewardDate() const { return m_latestMonthlyRewardDay; }

			// Room info
			void setRoomNumber(std::uint16_t roomNumber);
			void setClanRoomNumber(std::uint16_t clanRoomNumber);
			std::uint16_t getRoomNumber() const;
			std::uint16_t getClanRoomNumber() const noexcept;
			void decreaseRoomNumber();
			void setIsInMatch(bool val);
			bool isInMatch() const;
			void leaveRoom();

			// Achievements
			void addAchievementTier1(std::uint32_t achievementId);

			// Trade system
			std::vector<Item> addItems(const std::vector<Main::Structures::TradeBasicItem>& tradedItems);
			Item addItemFromTrade(TradedItem tradeItem);
			void lockTrade();
			bool hasPlayerLocked() const;
			void resetTradeInfo();
			void setCurrentlyTradingWithAccountId(std::uint32_t targetAccountId);
			std::uint32_t getCurrentlyTradingWithAccountId() const;
			bool addTradedItem(std::uint32_t itemId, const Main::Structures::ItemSerialInfo& serialInfo);
			void removeTradedItem(const Main::Structures::ItemSerialInfo& serialInfo);
			void resetTradedItems();
			const std::vector<TradedItem>& getTradedItems() const;


			std::string getPlayerInfoAsString() const
			{
				return "(PlayerName: " + std::string(m_accountInfo.nickname) + ", RoomNumber: " + std::to_string(m_roomNumber) + ", IsInMatch : " + std::to_string(m_isInMatch) + "\n";
			}
		};
	}
}

#endif
