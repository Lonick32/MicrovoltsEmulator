
#include "../../include/Classes/Player.h"
#include <unordered_map>
#include <vector>
#include <algorithm>
#include "../../include/Network/MainSession.h"
#include "../../include/Structures/AccountInfo/MuteInfo.h"
#include "ConstantDatabase/Structures/SetItemInfo.h"
#include "Utils/Utils.h"
#include "Utils/Constants.h"
#include <ranges>
#include <cstring> 

namespace Main
{
	namespace Classes
	{
		using Item = Main::Structures::Item;
		using EquippedItem = Main::Structures::EquippedItem;
		using DetailedEquippedItem = Main::Structures::DetailedEquippedItem;
		using BoughtItem = Main::Structures::BoughtItem;
		using AccountInfo = Main::Structures::AccountInfo;
		using Session = Main::Network::Session;
		using BlockedPlayer = Main::Structures::BlockedPlayer;
		using Friend = Main::Structures::Friend;
		using Mailbox = Main::Structures::Mailbox;
		using Session = Main::Network::Session;
		using TradedItem = Main::Structures::TradeBasicItem;

		void Player::setAccountInfo(const AccountInfo& accountInfo)
		{
			m_accountInfo = accountInfo;
		}

		std::uint32_t Player::addBattery(std::uint32_t battery)
		{
			m_accountInfo.battery = std::min(static_cast<std::uint32_t>(m_accountInfo.battery + battery), m_accountInfo.maxBattery);
			return m_accountInfo.battery;
		}


		void Player::addBatteryObtainedInMatch(std::uint32_t newBattery)
		{
			m_batteryObtainedInMatch += newBattery;
		}

		void Player::storeBatteryObtainedInMatch()
		{
			if (m_accountInfo.battery + m_batteryObtainedInMatch >= m_accountInfo.maxBattery)
			{
				m_accountInfo.battery = m_accountInfo.maxBattery;
			}
			else
			{
				m_accountInfo.battery += m_batteryObtainedInMatch;
			}
			m_batteryObtainedInMatch = 0;
		}

		const AccountInfo& Player::getAccountInfo() const
		{
			return m_accountInfo;
		}

		void Player::setPing(std::uint16_t ping)
		{
			m_ping = ping;
		}

		std::uint16_t Player::getPing() const
		{
			return m_ping;
		}

		std::uint32_t Player::getAccountID() const
		{
			return m_accountInfo.accountID;
		}

		const char* const Player::getPlayerName() const
		{
			return m_accountInfo.nickname;
		}

		bool Player::setAccountRockTotens(std::uint32_t rt)
		{
			if (rt > 0x3FFFFFFF) return false;
			m_accountInfo.rockTotens = rt;
			return true;
		}

		bool Player::setAccountMicroPoints(std::uint32_t mp)
		{
			if (mp > 0x7FFFFFFF) return false;
			m_accountInfo.microPoints = mp;
			return true;
		}

		bool Player::setAccountCoins(std::uint16_t coins)
		{
			if (coins > 0x7F) return false;
			m_accountInfo.coins = coins;
			return true;
		}

		void Player::setAccountLatestCharacterSelected(std::uint16_t latestCharacterSelected)
		{
			m_accountInfo.latestSelectedCharacter = latestCharacterSelected;
		}

		void Player::setLevel(std::uint16_t level)
		{
			m_accountInfo.playerLevel = level + 1;
		}

		void Player::setExperience(std::uint32_t exp)
		{
			m_accountInfo.experience = exp;
		}

		void Player::resetKillDeath()
		{
			m_accountInfo.totalKills = m_accountInfo.deaths = 0;
		}

		void Player::resetRecord()
		{
			m_accountInfo.wins = m_accountInfo.losses = m_accountInfo.draws = 0;
		}

		bool Player::expandBattery()
		{
			if (m_accountInfo.maxBattery > 4000) return false;
			m_accountInfo.maxBattery += 1000;
			return true;
		}

		bool Player::expandInventory(std::uint32_t spaceToAdd)
		{
			if (m_accountInfo.inventorySpace + spaceToAdd > 1000) return false;
			m_accountInfo.inventorySpace += spaceToAdd;
			return true;
		}

		void Player::setPlayerName(const char* playerName)
		{
			strncpy(m_accountInfo.nickname, playerName, sizeof(m_accountInfo.nickname) - 1);
			m_accountInfo.nickname[sizeof(m_accountInfo.nickname) - 1] = '\0';
		}

		bool Player::hasEnoughInventorySpace(std::uint16_t totalNewItems) const
		{
			return (static_cast<std::int32_t>(m_accountInfo.inventorySpace) - m_totalEquippedItems + m_couponItems.size()
				- m_itemsByItemNumber.size()) >= totalNewItems;
		}

		Main::ClientData::CouponItemUseRet Player::tryRemoveCoupons(std::uint32_t totalCouponsNeeded)
		{
			auto it = std::find_if(m_itemsByItemNumber.begin(), m_itemsByItemNumber.end(),
				[](const auto& pair) { return pair.second.itemId.itemId == 1000000; });

			if (it != m_itemsByItemNumber.end()) 
			{
				Item& coupon = it->second;
				if (coupon.itemId.stock == 0)
				{
					return { Common::Enums::CouponItemAction::COUPON_ITEM_STOCK_IS_ZERO, 0, coupon.serialInfo };
				}
				else if (coupon.itemId.stock > totalCouponsNeeded)
				{
					coupon.itemId.stock -= totalCouponsNeeded;
					return { Common::Enums::CouponItemAction::COUPON_ITEM_STOCKS_REDUCED_SUCCESS, coupon.itemId.stock, coupon.serialInfo };
				}
				else if (coupon.itemId.stock == totalCouponsNeeded)
				{
					coupon.itemId.stock -= totalCouponsNeeded;
					return { Common::Enums::CouponItemAction::COUPON_ITEM_DELETE, 0, coupon.serialInfo };
				}
			}
			return { Common::Enums::CouponItemAction::COUPON_ITEM_DO_NOTHING, 0, {} };
		}

		Main::ClientData::CouponItemAddRet Player::addCoupon(std::uint32_t stockToAdd)
		{
			auto it = std::find_if(m_itemsByItemNumber.begin(), m_itemsByItemNumber.end(),
				[](const auto& pair) { return pair.second.itemId.itemId == 1000000; });

			if (it != m_itemsByItemNumber.end())
			{
				Item& coupon = it->second;
				if (coupon.itemId.stock == 0 || coupon.itemId.stock + stockToAdd > 250)
				{
					return { Common::Enums::AddCouponAction::UNKNOWN_COUPON_ADD_ERROR, 0, {} };
				}
				else
				{
					coupon.itemId.stock += stockToAdd;
					return { Common::Enums::AddCouponAction::EXISTING_COUPON_STOCK_UPDATED, coupon.itemId.stock, coupon.serialInfo };
				}
			}
			return { Common::Enums::AddCouponAction::MUST_CREATE_NEW_COUPON, 0, {} };
		}

		void Player::setPlayerState(Common::Enums::PlayerState playerState)
		{
			m_playerState = playerState;
		}

		Common::Enums::PlayerState Player::getPlayerState() const
		{
			return m_playerState;
		}

		bool Player::isInLobby() const
		{
			return m_roomNumber == 0;
		}

		Main::Structures::MuteInfo Player::getMuteInfo() const
		{
			return Main::Structures::MuteInfo{ m_isMuted, m_muteReason, m_mutedBy, m_mutedUntil };
		}

		void Player::mute(const std::string& reason, const std::string& mutedBy, const std::string& mutedUntil)
		{
			m_isMuted = true;
			m_muteReason = reason;
			m_mutedBy = mutedBy;
			m_mutedUntil = mutedUntil;
		}

		void Player::disableRoomCreation()
		{
			m_isRoomCreationEnabled = false;
		}

		void Player::enableRoomCreation()
		{
			m_isRoomCreationEnabled = true;
		}

		bool Player::isRoomCreationEnabled() const noexcept
		{
			return m_isRoomCreationEnabled;
		}

		void Player::disableVotekick()
		{
			m_isVotekickEnabled = false;
		}

		void Player::enableVotekick()
		{
			m_isVotekickEnabled = true;
		}

		bool Player::isVotekickEnabled() const noexcept
		{
			return m_isVotekickEnabled;
		}


		void Player::unmute()
		{
			m_isMuted = false;
		}

		bool Player::isMuted() const
		{
			return m_isMuted;
		}

		void Player::addLuckyPoints(std::uint32_t points)
		{
			m_accountInfo.luckyPoints += points;
		}

		void Player::setLuckyPoints(std::uint32_t points)
		{
			m_accountInfo.luckyPoints = points;
		}

		std::uint32_t Player::getLuckyPoints() const
		{
			return static_cast<std::uint32_t>(m_accountInfo.luckyPoints);
		}

		const std::vector<Friend> Player::getFriendlist() const
		{
			std::vector<Friend> ret;
			for (const auto& [ffriend, unused] : m_friends)
			{
				ret.push_back(ffriend);
			}
			return ret;
		}

		std::unordered_map<Friend, std::weak_ptr<Session>>& Player::getFriendSessions()
		{
			return m_friends;
		}

		void Player::setFriendList(const std::vector<Friend>& friendlist)
		{
			for (const auto& currentFriend : friendlist)
			{
				m_friends[currentFriend] = std::weak_ptr<Session>{};
			}
		}

		void Player::updateFriend(const Friend& targetFriend, std::shared_ptr<Main::Network::Session> targetSession, bool remove = false)
		{
			if (m_friends.contains(targetFriend)) m_friends.erase(targetFriend);
			m_friends[targetFriend] = remove ? std::weak_ptr<Session>{} : std::weak_ptr<Session>{ targetSession };
		}

		// call once with default "persist", since removeFriend removes the friend for both players
		bool Player::deleteFriend(std::uint32_t targetAccountId)
		{
			Main::Structures::Friend targetFriend;
			targetFriend.targetAccountId = targetAccountId;
			auto it = m_friends.find(targetFriend);
			if (it != m_friends.end())
			{
				m_friends.erase(it);
				return true;
			}
			return false;
		}


		void Player::addOfflineFriend(const Main::Structures::Friend& ffriend)
		{
			m_friends[ffriend] = std::weak_ptr<Session>{};
		}

		std::optional<Main::Structures::ItemSerialInfo> Player::getBossBattleTicket() const
		{
			for (const auto& [itemNumber, item] : m_itemsByItemNumber)
			{
				const auto id = item.itemId.itemId;
				if (id >= 4811300 && id <= 4811600)
				{
					return item.serialInfo; 
				}
			}
			return std::nullopt; 
		}


		std::optional<Main::Structures::Friend> Player::addOnlineFriend(std::shared_ptr<Main::Network::Session> session)
		{
			if (session)
			{
				const auto& accountInfo = session->getAccountInfo();
				Main::Structures::Friend ffriend{ accountInfo.uniqueId, accountInfo.accountID };
				std::memcpy(ffriend.targetNickname, accountInfo.nickname, 16);
				m_friends[ffriend] = session;
				return ffriend;
			}
			return std::nullopt;
		}

		bool Player::isFriend(std::uint32_t accountId) const
		{
			for (const auto& [key, val] : m_friends)
			{
				if (key.targetAccountId == accountId)
				{
					return true;
				}
			}
			return false;
		}

		void Player::setUnequippedItems(const std::vector<Item>& items)
		{
			for (const auto& currentItem : items)
			{
				//m_latestItemNumber = std::max(m_latestItemNumber, currentItem.serialInfo.itemNumber);
				m_itemsByItemNumber.insert_or_assign(currentItem.serialInfo.itemNumber, currentItem);
				if (currentItem.itemId.itemId == 1000000) //if (currentItem.unknown)
				{
					m_couponItems.push_back(currentItem);
				}
			}
		}

		std::optional<std::uint32_t> Player::findItemIdBySerialInfo(const Main::Structures::ItemSerialInfo& itemSerialInfo) const
		{
			if (auto it = m_itemsByItemNumber.find(itemSerialInfo.itemNumber); it != m_itemsByItemNumber.end())
			{
				return it->second.itemId.itemId;
			}
			auto it = std::find_if(m_equippedItemByCharacter.begin(), m_equippedItemByCharacter.end(), [&itemSerialInfo](const auto& equippedItem)
				{
					return equippedItem.serialInfo.itemNumber == itemSerialInfo.itemNumber;
				});
			return it != m_equippedItemByCharacter.end() ? std::make_optional(it->id) : std::nullopt;
		}

		std::optional<std::pair<std::uint32_t, std::uint32_t>>
			Player::findItemIdAndDurabilityBySerialInfo(const Main::Structures::ItemSerialInfo& itemSerialInfo) const
		{
			if (auto it = m_itemsByItemNumber.find(itemSerialInfo.itemNumber);
				it != m_itemsByItemNumber.end())
			{
				return std::make_pair(it->second.itemId.itemId, it->second.durability);
			}

			auto it = std::find_if(
				m_equippedItemByCharacter.begin(),
				m_equippedItemByCharacter.end(),
				[&itemSerialInfo](const auto& equippedItem)
				{
					return equippedItem.serialInfo.itemNumber == itemSerialInfo.itemNumber;
				}
			);

			if (it != m_equippedItemByCharacter.end())
			{
				return std::make_pair(it->id, it->durability);
			}

			return std::nullopt;
		}

		bool Player::isItemTradeable(const Main::Structures::ItemSerialInfo& itemSerialInfo) const
		{
			if (auto it = m_itemsByItemNumber.find(itemSerialInfo.itemNumber);
				it != m_itemsByItemNumber.end())
			{
				const auto& item = it->second;
				return Main::CdbUtils::isTradeable(item.itemId.itemId).value_or(false)
					&& item.itemId.itemId != 1000000
					&& item.expirationDate == 0;
			}

			auto it = std::find_if(m_equippedItemByCharacter.begin(), m_equippedItemByCharacter.end(),
				[&itemSerialInfo](const auto& equippedItem)
				{
					return equippedItem.serialInfo.itemNumber == itemSerialInfo.itemNumber;
				});

			if (it != m_equippedItemByCharacter.end())
			{
				return Main::CdbUtils::isTradeable(it->id).value_or(false)
					&& it->id != 1000000
					&& it->expirationDate == 0;
			}

			return false;
		}

		std::optional<std::uint64_t> Player::findMaxItemNumber() const
		{
			std::optional<std::uint64_t> maxItemNumber = std::nullopt;

			if (!m_itemsByItemNumber.empty())
			{
				maxItemNumber = std::max_element(m_itemsByItemNumber.begin(), m_itemsByItemNumber.end(),
					[](const auto& a, const auto& b) {
						return a.first < b.first; 
					})->first;
			}

			if (!m_equippedItemByCharacter.empty())
			{
				auto maxEquipped = std::max_element(m_equippedItemByCharacter.begin(), m_equippedItemByCharacter.end(),
					[](const auto& a, const auto& b) {
						return a.serialInfo.itemNumber < b.serialInfo.itemNumber;
					})->serialInfo.itemNumber;

				if (!maxItemNumber || maxEquipped > *maxItemNumber)
				{
					maxItemNumber = maxEquipped;
				}
			}

			return maxItemNumber;
		}


		bool Player::prolongItem(const Main::Structures::ItemSerialInfo& newItemSerialInfo)
		{
			if (auto it = m_itemsByItemNumber.find(newItemSerialInfo.itemNumber); it != m_itemsByItemNumber.end())
			{
				it->second.serialInfo = newItemSerialInfo;
				return true;
			}
			auto it = std::ranges::find_if(m_equippedItemByCharacter, [&newItemSerialInfo](auto& equippedItem)
				{
					return equippedItem.serialInfo.itemNumber == newItemSerialInfo.itemNumber;
				});
			if (it != m_equippedItemByCharacter.end())
			{
				it->serialInfo = newItemSerialInfo;
				return true;
			}
			return false;
		}

		std::vector<EquippedItem> Player::getEquippedItemsFor(std::uint16_t characterID) const
		{
			std::vector<EquippedItem> equippedItems;

			if (characterID >= Common::Enums::MAX_CHARACTERS)
			{
				return equippedItems;
			}

			std::size_t startIndex = characterID * Common::Enums::MAX_ITEMTYPE;
			std::size_t endIndex = startIndex + Common::Enums::MAX_ITEMTYPE;

			for (std::size_t i = startIndex; i < endIndex && i < m_equippedItemByCharacter.size(); ++i)
			{
				const auto& item = m_equippedItemByCharacter[i];
				if (item.id != 0) 
					equippedItems.push_back(item);
			}

			return equippedItems;
		}

		std::vector<EquippedItem> Player::getUnlimitedEquippedWeaponsFor(std::uint16_t characterID) const
		{
			std::vector<EquippedItem> unlimitedItems;

			if (characterID >= Common::Enums::MAX_CHARACTERS)
				return unlimitedItems;

			const std::size_t startIndex = characterID * Common::Enums::MAX_ITEMTYPE;
			const std::size_t endIndex = startIndex + Common::Enums::MAX_ITEMTYPE;

			for (std::size_t i = startIndex; i < endIndex && i < m_equippedItemByCharacter.size(); ++i)
			{
				const auto& item = m_equippedItemByCharacter[i];
				if (item.id != 0 && item.expirationDate == 0 && Common::Enums::isWeapon(static_cast<Common::Enums::ItemType>(item.type)))
					unlimitedItems.push_back(item);
			}

			return unlimitedItems;
		}

		const std::array<EquippedItem, Common::Enums::MAX_CHARACTERS * Common::Enums::MAX_ITEMTYPE>& Player::getEquippedItems() const
		{
			return m_equippedItemByCharacter;
		}

		const std::unordered_map<std::uint64_t, Item>& Player::getItems() const
		{
			return m_itemsByItemNumber;
		}

		const std::vector<Item> Player::getItemsAsVec() const
		{
			std::vector<Item> items;
			items.reserve(m_itemsByItemNumber.size());
			for (const auto& [unused, item] : m_itemsByItemNumber)
			{
				items.push_back(item);
			}
			return items;
		}

		bool Player::deleteItemBasic(const Main::Structures::ItemSerialInfo& itemSerialInfo)
		{
			if (auto it = m_itemsByItemNumber.find(itemSerialInfo.itemNumber); it != m_itemsByItemNumber.end())
			{
				m_itemsByItemNumber.erase(it);
				return true;
			}
			auto it = std::ranges::find_if(m_equippedItemByCharacter, [&](auto& equippedItem) {
				return equippedItem.serialInfo.itemNumber == itemSerialInfo.itemNumber;
				});
			if (it != m_equippedItemByCharacter.end())
			{
				it->serialInfo.itemNumber = 0;
				return true;
			}
			return false;
		}

		void Player::addItems(const std::vector<Item>& items)
		{
			m_itemsByItemNumber.reserve(m_itemsByItemNumber.size() + items.size());
			for (const auto& currentItem : items)
			{
				m_itemsByItemNumber.insert_or_assign(currentItem.serialInfo.itemNumber, currentItem);
			}
		}

		std::size_t Player::getTotalCoupons() const noexcept 
		{ 
			std::size_t total = 0;
			for (const auto& current : m_couponItems)
			{
				total += current.itemId.stock;
			}
			return total;
		}


		void Player::addTotalCouponItems(const Item& item)
		{
			m_couponItems.push_back(item);
		}

		void Player::addItem(const Item& item)
		{
			m_itemsByItemNumber.insert_or_assign(item.serialInfo.itemNumber, item);
		}

		void Player::addItems(const std::vector<Main::Structures::BoxItem>& boxItems)
		{
			addItems(std::vector<Item>(boxItems.begin(), boxItems.end()));
		}

		void Player::addItems(const std::vector<BoughtItem>& boughtItems)
		{
			addItems(std::vector<Item>(boughtItems.begin(), boughtItems.end()));
		}

		void Player::setEquippedItems(const std::unordered_map<std::uint16_t, std::vector<EquippedItem>>& equippedItems)
		{
			for (const auto& [characterID, items] : equippedItems)
			{
				for (const auto& currentItem : items)
				{
					std::size_t index = characterID * Common::Enums::MAX_ITEMTYPE + currentItem.type;

					if (index >= m_equippedItemByCharacter.size())
					{
						continue;
					}

					m_equippedItemByCharacter[index] = currentItem;
					++m_totalEquippedItems;
				}
			}
		}

		std::pair<std::vector<Main::ClientData::SingleWeaponDurabilityDamage>,
			std::vector<std::pair<std::uint32_t, std::uint64_t>>> Player::reduceEquippedItemsDurabilities(
			std::size_t characterID, std::uint32_t weaponRestrictionValue)
		{
			using namespace Common::Enums;

			WeaponRestriction weaponRestriction = static_cast<WeaponRestriction>(weaponRestrictionValue);
			std::vector<Main::ClientData::SingleWeaponDurabilityDamage> damages;
			std::vector<std::pair<std::uint32_t, std::uint64_t>> idsAndItemNumbers;
			const std::size_t startIndex = characterID * MAX_ITEMTYPE;
			const std::size_t endIndex = startIndex + MAX_ITEMTYPE;

			for (std::size_t i = startIndex; i < endIndex && i < m_equippedItemByCharacter.size(); ++i)
			{
				auto& item = m_equippedItemByCharacter[i];

				if (item.id == 0 || item.expirationDate != 0 || !isWeapon(static_cast<ItemType>(item.type)))
					continue;

				if (weaponRestriction != All && weaponRestriction != WeaponSelect)
				{
					ItemType restrictedType;
					switch (weaponRestriction)
					{
					case MeleeOnly:  restrictedType = MELEE; break;
					case RifleOnly:  restrictedType = RIFLE; break;
					case ShotgunOnly: restrictedType = SHOTGUN; break;
					case SniperOnly: restrictedType = SNIPER; break;
					case GatlingOnly: restrictedType = MG; break;
					case BazookaOnly: restrictedType = BAZOOKA; break;
					case GrenadeOnly: restrictedType = GRENADE; break;
					default: continue;
					}

					if (item.type != static_cast<std::uint32_t>(restrictedType))
						continue;
				}

				const auto baseDurability = Main::CdbUtils::getItemDurability(item.id);
				if (!baseDurability || *baseDurability == 0)
					continue;

				const std::uint32_t reduction = (*baseDurability / 100) * 1;
				const std::uint32_t newDurability = (*baseDurability > reduction) ? (*baseDurability - reduction) : 0;

				item.durability = newDurability;
				damages.push_back(Main::ClientData::SingleWeaponDurabilityDamage{ item.serialInfo, reduction });
				idsAndItemNumbers.push_back(std::pair{ static_cast<std::uint32_t>(item.id), item.serialInfo.itemNumber });
			}

			return std::pair{ damages, idsAndItemNumbers };
		}

		bool Player::updateItemDurabilityByNumber(std::uint32_t itemNumber, std::uint32_t newDurability)
		{
			auto updateDurability = [&](auto& item) {
				item.durability = newDurability;
				return true;
				};
			if (auto it = m_itemsByItemNumber.find(itemNumber); it != m_itemsByItemNumber.end())
			{
				return updateDurability(it->second);
			}

			const std::size_t offset = m_accountInfo.latestSelectedCharacter * Common::Enums::MAX_ITEMTYPE;
			if (offset + Common::Enums::MAX_ITEMTYPE > m_equippedItemByCharacter.size())
			{
				return false;
			}

			for (std::size_t i = 0; i < Common::Enums::MAX_ITEMTYPE; ++i)
			{
				auto& equippedItem = m_equippedItemByCharacter[offset + i];
				if (equippedItem.serialInfo.itemNumber == itemNumber)
				{
					return updateDurability(equippedItem);
				}
			}
			return false;
		}

		std::optional<std::pair<std::uint16_t, std::uint64_t>> Player::addEnergyToItem(const Main::Structures::ItemSerialInfo& itemSerialInfo, std::uint32_t energyAdded)
		{
			auto updateEnergyAndBattery = [&](auto& item) {
				item.energy += energyAdded;
				m_accountInfo.battery -= energyAdded;
				return std::pair{ item.energy, static_cast<std::uint64_t>(m_accountInfo.battery) };
				};

			if (auto it = m_itemsByItemNumber.find(itemSerialInfo.itemNumber); it != m_itemsByItemNumber.end())
			{
				return updateEnergyAndBattery(it->second);
			}

			const std::size_t offset = m_accountInfo.latestSelectedCharacter * Common::Enums::MAX_ITEMTYPE;
			for (std::size_t i = 0; i < Common::Enums::MAX_ITEMTYPE; ++i)
			{
				if (offset + i >= m_equippedItemByCharacter.size()) continue;

				auto& equippedItem = m_equippedItemByCharacter[offset + i];
				if (equippedItem.serialInfo.itemNumber == itemSerialInfo.itemNumber)
				{
					return updateEnergyAndBattery(equippedItem);
				}
			}

			return std::nullopt;
		}

		std::optional<std::uint16_t> Player::getItemEnergy(const Main::Structures::ItemSerialInfo& itemSerialInfo) const
		{
			if (auto it = m_itemsByItemNumber.find(itemSerialInfo.itemNumber); it != m_itemsByItemNumber.end())
			{
				return it->second.energy;
			}

			const std::size_t offset = m_accountInfo.latestSelectedCharacter * Common::Enums::MAX_ITEMTYPE;
			for (std::size_t i = 0; i < Common::Enums::MAX_ITEMTYPE; ++i)
			{
				if (offset + i >= m_equippedItemByCharacter.size()) continue;

				const auto& equippedItem = m_equippedItemByCharacter[offset + i];
				if (equippedItem.serialInfo.itemNumber == itemSerialInfo.itemNumber)
				{
					return equippedItem.energy;
				}
			}

			return std::nullopt;
		}

		void Player::unequipItemImpl(std::uint64_t itemType, Main::Persistence::MainScheduler& scheduler, std::uint32_t character)
		{
			const std::uint32_t characterIndex = character == -1 ? m_accountInfo.latestSelectedCharacter : character;
			const std::size_t index = characterIndex * Common::Enums::MAX_ITEMTYPE + itemType;
			if (index >= m_equippedItemByCharacter.size() || (index < m_equippedItemByCharacter.size() && !m_equippedItemByCharacter[index].serialInfo.itemNumber))
				return;
			const std::uint64_t itemNumber = m_equippedItemByCharacter[index].serialInfo.itemNumber;

			// unequip the item
			m_itemsByItemNumber.insert_or_assign(itemNumber, m_equippedItemByCharacter[index]);
			m_equippedItemByCharacter[index].serialInfo.itemNumber = 0;
			--m_totalEquippedItems;
			scheduler.addRepetitiveCallback(std::source_location::current(), m_accountInfo.accountID, &Main::Persistence::PersistentDatabase::unequipItem,
				m_accountInfo.accountID, static_cast<std::uint64_t>(itemNumber));
		}

		// ugly design but easier to write, ideally we shouldn't pass the scheduler to this function though
		void Player::equipItem(const std::uint16_t itemNumber, Main::Persistence::MainScheduler& scheduler, std::uint32_t character)
		{
			using setItems = Common::ConstantDatabase::CdbSingleton<Common::ConstantDatabase::SetItemInfo>;

			auto it = m_itemsByItemNumber.find(itemNumber);
			if (it == m_itemsByItemNumber.end()) return;

			EquippedItem equippedItem = EquippedItem{ it->second };

			if (const auto entry = setItems::getInstance().getEntry(it->second.itemId.itemId);
				entry && equippedItem.type == Common::Enums::ItemType::SET)
			{ // Case 1: the user is equipping a set. We need to unequip item types that are already present in such set first
				for (auto currentTypeNotNull : Common::Utils::getPartTypesWhereSetItemInfoTypeNotNull(*entry, m_accountInfo.latestSelectedCharacter))
				{
					unequipItemImpl(currentTypeNotNull, scheduler, character);
				}
			}

			const std::size_t charIndex = character == -1 ? m_accountInfo.latestSelectedCharacter : character;
			const std::size_t setIndex = charIndex * Common::Enums::MAX_ITEMTYPE + Common::Enums::ItemType::SET;

			if (setIndex >= m_equippedItemByCharacter.size())
				return; 

			const auto& setItem = m_equippedItemByCharacter[setIndex];

			if (const auto entry = setItems::getInstance().getEntry(setItem.id);
				entry && setItem.serialInfo.itemNumber)
			{ // Case 2: the user has an already equipped set. We need to unequip it if the user is now equipping a part type that is already present in said set
				for (auto currentTypeNotNull : Common::Utils::getPartTypesWhereSetItemInfoTypeNotNull(*entry, m_accountInfo.latestSelectedCharacter))
				{
					if (equippedItem.type == currentTypeNotNull)
					{
						unequipItemImpl(Common::Enums::SET, scheduler, character);
						break;
					}
				}
			}

			// Delete this item from the Non-Equipped items, since we'll add it to the Equipped items.
			m_itemsByItemNumber.erase(itemNumber);

			// Now proceed normally: check if equippedItems already has such a type
			const std::size_t itemIndex = charIndex * Common::Enums::MAX_ITEMTYPE + equippedItem.type;
			if (itemIndex >= m_equippedItemByCharacter.size()) return;
			if (m_equippedItemByCharacter[itemIndex].serialInfo.itemNumber)
			{
				const std::uint64_t toUnequipItemNumber = m_equippedItemByCharacter[itemIndex].serialInfo.itemNumber;
				m_itemsByItemNumber.insert_or_assign(toUnequipItemNumber, Item{ m_equippedItemByCharacter[itemIndex] });
				m_equippedItemByCharacter[itemIndex] = equippedItem;

				scheduler.addRepetitiveCallback(std::source_location::current(), m_accountInfo.accountID, &Main::Persistence::PersistentDatabase::swapItems,
					m_accountInfo.accountID, toUnequipItemNumber, static_cast<std::uint64_t>(equippedItem.serialInfo.itemNumber),
					static_cast<std::uint16_t>(character == -1 ? m_accountInfo.latestSelectedCharacter : character));
				return;
			}

			// Otherwise just add the to-be-added item to the equipped items.
			m_equippedItemByCharacter[itemIndex] = equippedItem;
			++m_totalEquippedItems;

			scheduler.addRepetitiveCallback(std::source_location::current(), m_accountInfo.accountID, &Main::Persistence::PersistentDatabase::equipItem,
				m_accountInfo.accountID, static_cast<std::uint64_t>(equippedItem.serialInfo.itemNumber),
				static_cast<std::uint16_t>(character == -1 ? m_accountInfo.latestSelectedCharacter : character));
		}


		std::optional<std::uint64_t> Player::unequipItem(std::uint64_t itemType, Main::Persistence::MainScheduler& scheduler)
		{
			const std::size_t itemIndex = m_accountInfo.latestSelectedCharacter * Common::Enums::MAX_ITEMTYPE + itemType;
			if (itemIndex >= m_equippedItemByCharacter.size()) return std::nullopt;
			if (!m_equippedItemByCharacter[itemIndex].serialInfo.itemNumber)
			{
				return std::nullopt;
			}
			const std::uint64_t itemNumber = m_equippedItemByCharacter[itemIndex].serialInfo.itemNumber;
			m_itemsByItemNumber.insert_or_assign(itemNumber, Item{ m_equippedItemByCharacter[itemIndex] });
			m_equippedItemByCharacter[itemIndex].serialInfo.itemNumber = 0;
			--m_totalEquippedItems;
			return itemNumber;
		}

		std::uint64_t Player::getTotalEquippedItems() const
		{
			return m_totalEquippedItems;
		}

		std::uint64_t Player::getLatestItemNumber() const
		{
			return findMaxItemNumber().value_or(0); // on purpose, we're sure this always find the latest item number (greatest)
		}

		void Player::setLatestItemNumber(std::uint64_t itemNum)
		{
			//m_latestItemNumber = itemNum;
		}

		std::pair<Common::Enums::MatchItemAction, std::uint32_t> Player::useInstantRespawn(std::uint64_t itemNum)
		{
			if (auto it = m_itemsByItemNumber.find(itemNum); it != m_itemsByItemNumber.end())
			{
				if (it->second.itemId.stock > 0)
				{
					if (it->second.itemId.stock == 1)
					{
						return { Common::Enums::MATCHITEM_DELETE, 0 };
					}
					else
					{
						--it->second.itemId.stock;
						return { Common::Enums::MATCHITEM_STOCKS_REDUCED_SUCCESS, static_cast<std::uint32_t>(it->second.itemId.stock) };
					}
				}
				else
				{
					return { Common::Enums::MATCHITEM_STOCK_ZERO, 0 };
				}
			}
			return { Common::Enums::MATCHITEM_DO_NOTHING, 0 };
		}

		bool Player::unequipItemIfEquipped(std::uint64_t itemNumber, std::uint32_t characterId, Main::Persistence::MainScheduler& scheduler)
		{
			if (characterId >= Common::Enums::MAX_CHARACTERS) return false;

			const std::size_t startIndex = characterId * Common::Enums::MAX_ITEMTYPE;
			for (std::size_t itemIndex = 0; itemIndex < Common::Enums::MAX_ITEMTYPE; ++itemIndex)
			{
				if (startIndex + itemIndex >= m_equippedItemByCharacter.size())
					break;

				const auto& equippedItem = m_equippedItemByCharacter[startIndex + itemIndex];
				if (equippedItem.serialInfo.itemNumber == itemNumber)
				{
					unequipItemImpl(itemIndex, scheduler, characterId);
					return true;
				}
			}
			return false;
		}

		void Player::equipItemIfNotEquipped(std::uint64_t itemNumber, std::uint32_t characterId, Main::Persistence::MainScheduler& scheduler)
		{
			if (characterId >= Common::Enums::MAX_CHARACTERS) return;

			const std::size_t startIndex = characterId * Common::Enums::MAX_ITEMTYPE;
			for (std::size_t itemIndex = 0; itemIndex < Common::Enums::MAX_ITEMTYPE; ++itemIndex)
			{
				if (startIndex + itemIndex >= m_equippedItemByCharacter.size()) continue;

				const auto& equippedItem = m_equippedItemByCharacter[startIndex + itemIndex];
				if (equippedItem.serialInfo.itemNumber == itemNumber) return;
			}

			equipItem(static_cast<std::uint16_t>(itemNumber), scheduler, characterId);
		}

		std::pair<std::array<std::uint32_t, 10>, std::array<std::uint32_t, 7>> Player::getEquippedItemsSeparated() const
		{
			std::array<std::uint32_t, 10> equippedPlayerItems{};
			std::array<std::uint32_t, 7> equippedPlayerWeapons{};
			const std::size_t startIndex = m_accountInfo.latestSelectedCharacter * Common::Enums::MAX_ITEMTYPE;

			// Set is a special case
			const std::size_t setIndex = startIndex + Common::Enums::ItemType::SET;
			if (setIndex >= m_equippedItemByCharacter.size()) return std::pair{ equippedPlayerItems, equippedPlayerWeapons };
			if (m_equippedItemByCharacter[setIndex].serialInfo.itemNumber)
			{
				using setItems = Common::ConstantDatabase::CdbSingleton<Common::ConstantDatabase::SetItemInfo>;
				const std::uint32_t setItemId = m_equippedItemByCharacter[setIndex].id;
				if (auto entry = setItems::getInstance().getEntry(setItemId); entry)
				{
					for (const auto& currentTypeNotNull : Common::Utils::getPartTypesWhereSetItemInfoTypeNotNull(*entry, m_accountInfo.latestSelectedCharacter))
						equippedPlayerItems[currentTypeNotNull] = setItemId;
				}
			}
			for (std::size_t i = 0; i < Common::Enums::MAX_ITEMTYPE; ++i)
			{
				const auto& equippedItem = m_equippedItemByCharacter[startIndex + i];
				if (equippedItem.serialInfo.itemNumber)
				{
					if (i < Common::Enums::MELEE)
						equippedPlayerItems[i] = equippedItem.id;
					else if (i < Common::Enums::MAX_ITEMTYPE && (i - 10 < equippedPlayerWeapons.size()))
						equippedPlayerWeapons[i - 10] = equippedItem.id;
				}
			}
			return { equippedPlayerItems, equippedPlayerWeapons };
		}

		bool Player::blockAccount(std::uint32_t accountId, const char* nickname)
		{
			if (m_blockedAccounts.size() >= Common::Constants::maxFriends)
				return false;

			Main::Structures::BlockedPlayer blocked{ accountId };
			std::memcpy(blocked.targetNickname, nickname, sizeof(blocked.targetNickname));
			m_blockedAccounts.push_back(blocked);
			return true;
		}


		bool Player::unblockAccount(std::uint32_t accountId)
		{
			auto it = std::remove_if(m_blockedAccounts.begin(), m_blockedAccounts.end(),
				[accountId](const auto& account) { return account.targetAccountId == accountId; });

			if (it != m_blockedAccounts.end())
			{
				m_blockedAccounts.erase(it, m_blockedAccounts.end());
				return true;
			}
			return false; // Account not found
		}

		void Player::addAchievementTier1(std::uint32_t achievementId)
		{
			m_accountInfo.achievements.setAchievementTier1(achievementId);
		}

		bool Player::hasBlocked(std::uint32_t accountId) const
		{
			for (const auto& currentBlocked : m_blockedAccounts)
			{
				if (currentBlocked.targetAccountId == accountId)
				{
					return true;
				}
			}
			return false;
		}

		const std::vector<Main::Structures::BlockedPlayer>& Player::getBlockedPlayers() const
		{
			return m_blockedAccounts;
		}

		void Player::setBlockedPlayers(const std::vector<Main::Structures::BlockedPlayer>& blockedPlayers)
		{
			m_blockedAccounts = blockedPlayers;
		}

		// Mailbox
		void Player::addMailboxReceived(const Main::Structures::Mailbox& mailbox)
		{
			m_mailboxReceived.push_back(mailbox);
		}

		void Player::addGiftboxReceived(const Main::Structures::Giftbox& giftbox)
		{
			m_giftboxReceived.push_back(giftbox);
		}

		void Player::addMailboxSent(const Main::Structures::Mailbox& mailbox)
		{
			m_mailboxSent.push_back(mailbox);
		}

		bool Player::deleteSentMailbox(std::uint32_t timestamp)
		{
			auto it = std::find_if(m_mailboxSent.begin(), m_mailboxSent.end(), [timestamp](const auto& mailbox) {
				return mailbox.timestamp == timestamp;
				});
			if (it != m_mailboxSent.end())
			{
				m_mailboxSent.erase(it);
				return true;
			}
			return false;
		}

		bool Player::deleteReceivedMailbox(std::uint32_t timestamp)
		{
			auto it = std::find_if(m_mailboxReceived.begin(), m_mailboxReceived.end(), [timestamp](const auto& mailbox) {
				return mailbox.timestamp == timestamp;
				});
			if (it != m_mailboxReceived.end())
			{
				m_mailboxReceived.erase(it);
				return true;
			}
			return false;
		}

		const std::vector<Main::Structures::Mailbox>& Player::getMailboxReceived() const
		{
			return m_mailboxReceived;
		}

		const std::vector<Main::Structures::Mailbox>& Player::getMailboxSent() const
		{
			return m_mailboxSent;
		}

		const std::vector<Main::Structures::Giftbox>& Player::getGiftboxReceived() const
		{
			return m_giftboxReceived;
		}

		void Player::deleteGiftbox(std::uint32_t timestamp)
		{
			auto it = std::remove_if(m_giftboxReceived.begin(), m_giftboxReceived.end(), [timestamp](const auto& giftbox) {
				return giftbox.timestamp == timestamp;
				});

			if (it != m_giftboxReceived.end())
			{
				m_giftboxReceived.erase(it, m_giftboxReceived.end());
			}
		}

		void Player::setMailbox(const std::vector<Main::Structures::Mailbox>& mailbox, bool sent)
		{
			if (sent) m_mailboxSent = mailbox;
			else m_mailboxReceived = mailbox;
		}

		std::optional<std::uint32_t> Player::getItemIdFromGiftbox(std::uint32_t timestamp) const
		{
			for (const auto& currentGiftbox : m_giftboxReceived)
			{
				if (currentGiftbox.timestamp == timestamp)
				{
					return currentGiftbox.id;
				}
			}
			return std::nullopt;
		}

		std::optional<Main::Structures::Giftbox> Player::getGiftbox(std::uint32_t timestamp) const
		{
			for (const auto& currentGiftbox : m_giftboxReceived)
			{
				if (currentGiftbox.timestamp == timestamp)
				{
					return currentGiftbox;
				}
			}
			return std::nullopt;
		}


		void Player::setReceivedGiftboxes(const std::vector<Main::Structures::Giftbox>& giftbox)
		{
			m_giftboxReceived = giftbox;
		}

		// Room info
		void Player::setRoomNumber(std::uint16_t roomNumber)
		{
			m_roomNumber = roomNumber;
		}

		void Player::setClanRoomNumber(std::uint16_t clanRoomNumber)
		{
			m_clanRoomNumber = clanRoomNumber;
		}

		std::uint16_t Player::getRoomNumber() const
		{
			return m_roomNumber;
		}

		std::uint16_t Player::getClanRoomNumber() const noexcept
		{
			return m_clanRoomNumber;
		}

		void Player::setIsInMatch(bool val)
		{
			m_isInMatch = val;
		}

		bool Player::isInMatch() const
		{
			return (m_playerState == Common::Enums::STATE_NORMAL || m_playerState == Common::Enums::STATE_DYING);
		}

		void Player::leaveRoom()
		{
			setRoomNumber(0);
			setIsInMatch(false);
			m_batteryObtainedInMatch = 0;
		}

		void Player::decreaseRoomNumber()
		{
			if (m_roomNumber > 0)
			{
				--m_roomNumber;
			}
		}

		// Trade system
		std::vector<Main::Structures::Item> Player::addItems(const std::vector<Main::Structures::TradeBasicItem>& tradedItems)
		{
			std::vector<Item> items;
			for (auto& currentItem : tradedItems)
			{
				m_itemsByItemNumber.insert_or_assign(currentItem.itemSerialInfo.itemNumber, currentItem);
			}
			return items;
		}

		Item Player::addItemFromTrade(TradedItem tradeItem)
		{
			auto latestItemNum = getLatestItemNumber();
			tradeItem.itemSerialInfo.itemNumber = ++latestItemNum;
			Main::Structures::Item item{ tradeItem };
			m_itemsByItemNumber.insert_or_assign(tradeItem.itemSerialInfo.itemNumber, item);
			return item;
		}

		void Player::lockTrade()
		{
			m_hasPlayerLocked = true;
		}

		bool Player::hasPlayerLocked() const
		{
			return m_hasPlayerLocked;
		}

		void Player::resetTradeInfo()
		{
			m_hasPlayerLocked = false;
			m_tradedItems.clear();
			m_currentlyTradingWithAccountId = 0;
			setPlayerState(Common::Enums::PlayerState::STATE_INVENTORY);
		}

		void Player::setCurrentlyTradingWithAccountId(std::uint32_t targetAccountId)
		{
			m_currentlyTradingWithAccountId = targetAccountId;
		}

		std::uint32_t Player::getCurrentlyTradingWithAccountId() const
		{
			return m_currentlyTradingWithAccountId;
		}

		bool Player::addTradedItem(std::uint32_t itemId, const Main::Structures::ItemSerialInfo& serialInfo)
		{
			auto it = std::find_if(m_tradedItems.begin(), m_tradedItems.end(),
				[&serialInfo](const Main::Structures::TradeBasicItem& item)
				{
					return item.itemSerialInfo.itemNumber == serialInfo.itemNumber;
				});

			if (it != m_tradedItems.end()) 
			{
				return false;
			}

			m_tradedItems.push_back(Main::Structures::TradeBasicItem{ itemId, serialInfo });
			return true;
		}


		void Player::removeTradedItem(const Main::Structures::ItemSerialInfo& serialInfo)
		{
			for (auto it = m_tradedItems.begin(); it != m_tradedItems.end(); ++it)
			{
				if (it->itemSerialInfo == serialInfo)
				{
					m_tradedItems.erase(it);
					return;
				}
			}
		}

		void Player::resetTradedItems()
		{
			m_tradedItems.clear();
		}

		const std::vector<TradedItem>& Player::getTradedItems() const
		{
			return m_tradedItems;
		}

	}
}
