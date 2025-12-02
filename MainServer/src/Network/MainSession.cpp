
#include "Network/Session.h"
#include "../../include/Structures/AccountInfo/MainAccountInfo.h"
#include "../../include/Structures/Item/MainItem.h"
#include "../../include/Structures/Item/MainEquippedItem.h"
#include "../../include/Persistence/MainScheduler.h"
#include "../../include/Persistence/MainDatabaseManager.h"
#include "../../include/Structures/PlayerLists/BlockedPlayer.h"
#include "../../include/Structures/PlayerLists/Friend.h"
#include "../../include/Structures/Mailbox.h"
#include "../../include/Structures/AccountInfo/MuteInfo.h"
#include "Enums/GameEnums.h"

#include "../../include/Classes/Player.h"
#include "../../include/Network/MainSession.h"
#include <functional>
#include <asio.hpp>
#include <array>
#include <Utils/Utils.h>
#include "Utils/Constants.h"
#include "../../include/Detail/Utilities.h"
#include "../../include/Structures/PlayerLists/SingleLobbyList.h"
#include "../../include/Structures/PlayerLists/SingleLobbyClanList.h"

#include <source_location>
#include <ConstantDatabase/Structures/CdbCollectionInfo.h>
#include "Macros.h"
#include <cstring> 

namespace Main
{
	namespace Network
	{
		Session::Session(Main::Persistence::MainScheduler& scheduler, tcp::socket&& socket, std::function<void(std::size_t)> fnct,
			Ac::AntiCheatManager& ac)
			: Common::Network::Session{ std::move(socket), fnct }
			, m_scheduler{ scheduler }
			, m_acManager{ ac }
		{
			m_packet.setTcpHeader(m_id, Common::Enums::USER_ENCRYPTION);
		}

		std::size_t Session::getSessionId() const
		{
			return m_id;
		}

		void Session::onPacket(std::vector<std::uint8_t>& data)
		{
			Common::Network::Packet incomingPacket;
			incomingPacket.processIncomingPacket(data.data(), static_cast<std::uint16_t>(data.size()), m_crypt.UserKey);
			const std::uint16_t callbackNum = incomingPacket.getOrder();

			if (!Common::Network::Session::callbacks<Common::Network::PacketType::ENCRYPTED, Main::Network::Session>.contains(callbackNum))
			{
				Utils::Logger::log("[SEID:" + std::to_string(m_id) + "] No callback for order: " + std::to_string(callbackNum), 
					Utils::LogType::Error, "MainSession::onPacket");
				return;
			}
			
			Common::Network::Session::callbacks<Common::Network::PacketType::ENCRYPTED, Main::Network::Session>[callbackNum](incomingPacket, 
				std::static_pointer_cast<Main::Network::Session>(shared_from_this()));

			Common::Protocol::TcpHeader header;
			Common::Cryptography::Crypt cryptography;
			cryptography.KeySetup(0);
			cryptography.RC5Decrypt32(reinterpret_cast<int32_t*>(m_reader.data()), &header, sizeof(Common::Protocol::TcpHeader));

			if (header.getCrypt() && header.getSize() >= 12 && m_packetReplicaWhitelist.count(callbackNum) == 0)
			{
				m_acManager.submitEvent(std::make_unique<Ac::PacketReplicationEvent>(
					shared_from_this(), callbackNum, m_reader));
			}
		}

		// For many packets, the clients assumes certain extras and logic:
		// - (1) If the packet is empty => extra 6, no data (this function performs this step)
		// - (2) If the packet is non-empty, and its size in bytes is < 1440, then send the single packet with extra = 37
		// - (3) Otherwise, split the packet into sub-packets, where the first sub-packet's extra = 37, and all other sub-packet extra = 0 (this function performs this step)
	    void Session::sendEmptyPacket(std::size_t order, std::uint32_t mission)
		{
			m_packet.setOrder(order);
			m_packet.setMission(mission);
			m_packet.setExtra(6);
			m_packet.setData(nullptr, 0);
			asyncWrite(m_packet);
		}

		bool Session::sendOfflineMailbox(Main::Structures::Mailbox mailbox)
		{
			m_packet.setTcpHeader(m_id, Common::Enums::NO_ENCRYPTION);
			m_packet.setOrder(104);
			m_packet.setData(nullptr, 0);

			if (m_player.getMailboxSent().size() > Common::Constants::maxMailbox)
			{
				m_packet.setExtra(Main::Enums::MailboxExtra::SENDER_NO_SPACE_LEFT);
				asyncWrite(m_packet);
				return false;
			}

			// store the mailbox for the target (receiver)
			const Main::Enums::MailboxExtra res =
				m_scheduler.immediatePersist(std::source_location::current(), &Main::Persistence::PersistentDatabase::storeOfflineMailbox, 
					mailbox, m_player.getAccountInfo().nickname);
			if (res == Main::Enums::MailboxExtra::MAILBOX_SENT)
			{
				mailbox.accountId = m_player.getAccountInfo().accountID;
				std::memcpy(mailbox.nickname, mailbox.nickname, Common::Constants::maxNicknameSize);
				addMailboxSent(mailbox);
			}
			else if (res == Main::Enums::MailboxExtra::MAILBOX_DB_ERROR)
			{
				sendMessage("[Session::sendOfflineMailbox] error: database error");
			}
			else 
			{
				m_packet.setExtra(res);
				asyncWrite(m_packet);
				return false;
			}
		}

		bool Session::sendOnlineMailbox(std::shared_ptr<Session> target, Main::Structures::Mailbox mailbox)
		{
			m_packet.setTcpHeader(m_id, Common::Enums::NO_ENCRYPTION);
			m_packet.setOrder(104);
			m_packet.setData(nullptr, 0);
			const auto& selfAccountInfo = m_player.getAccountInfo();

			if (m_player.getMailboxSent().size() > Common::Constants::maxMailbox)
			{
				m_packet.setExtra(Main::Enums::MailboxExtra::SENDER_NO_SPACE_LEFT);
				asyncWrite(m_packet);
				return false;
			}
			else if (target->getPlayer().getMailboxReceived().size() > Common::Constants::maxMailbox)
			{
				m_packet.setExtra(Main::Enums::MailboxExtra::RECEIVER_NO_SPACE_LEFT);
				asyncWrite(m_packet);
				return false;
			}
			else if (target->getPlayer().hasBlocked(selfAccountInfo.accountID))
			{
				m_packet.setExtra(Main::Enums::MailboxExtra::MAILBOX_RECEIVER_BLOCKED_SENDER);
				asyncWrite(m_packet);
				return false;
			}
			else // send the mailbox to the targetm
			{
				m_packet.setExtra(Main::Enums::MailboxExtra::MAILBOX_RECEIVED);
				m_packet.setData(nullptr, 0);
				target->asyncWrite(m_packet);
				target->addMailboxReceived(mailbox);

				mailbox.accountId = selfAccountInfo.accountID;
				std::memcpy(mailbox.nickname, target->getAccountInfo().nickname, Common::Constants::maxNicknameSize);
				addMailboxSent(mailbox);
				return true;
			}
		}

		void Session::addMailboxReceived(const Main::Structures::Mailbox& mailbox)
		{
			m_player.addMailboxReceived(mailbox);
			m_scheduler.addRepetitiveCallback(std::source_location::current(), m_player.getAccountID(), &Main::Persistence::PersistentDatabase::storeMailbox,
				mailbox, m_player.getAccountID(), false);
		}

		void Session::addGiftboxReceived(const Main::Structures::Giftbox& giftbox)
		{
			m_player.addGiftboxReceived(giftbox);
			m_scheduler.addRepetitiveCallback(std::source_location::current(), m_player.getAccountID(), LIFT_MEMBER(storeGiftbox),
				giftbox, m_player.getAccountID());
		}

		void Session::addMailboxSent(const Main::Structures::Mailbox& mailbox)
		{
			m_player.addMailboxSent(mailbox);
			m_scheduler.addRepetitiveCallback(std::source_location::current(), m_player.getAccountID(), &Main::Persistence::PersistentDatabase::storeMailbox,
				mailbox, m_player.getAccountID(), true);
		}

		bool Session::deleteSentMailbox(std::uint32_t timestamp)
		{
			if (m_player.deleteSentMailbox(timestamp))
			{
				m_scheduler.addRepetitiveCallback(std::source_location::current(), m_player.getAccountID(), &Main::Persistence::PersistentDatabase::deleteMailbox,
					timestamp, m_player.getAccountID(), true);
				return true;
			}
			sendMessage("[Session::deleteSentMailbox] Error while deleting sent mailbox");
			return false;
		}

		bool Session::deleteReceivedMailbox(std::uint32_t timestamp)
		{
			if (m_player.deleteReceivedMailbox(timestamp))
			{
				m_scheduler.addRepetitiveCallback(std::source_location::current(), m_player.getAccountID(), &Main::Persistence::PersistentDatabase::deleteMailbox,
					timestamp, m_player.getAccountID(), false);
				return true;
			}
			sendMessage("[Session::deleteReceivedMailbox] Error while deleting received mailbox");
			return false;
		}

		bool Session::deleteMailbox(Main::Enums::MailboxMission type, std::uint32_t timestamp)
		{
			return type == Main::Enums::MailboxMission::MISSION_MAILBOX_RECEIVED 
				? deleteReceivedMailbox(timestamp) 
				: deleteSentMailbox(timestamp);
		}

		void Session::setMailbox(const std::vector<Main::Structures::Mailbox>& mailbox, bool sent)
		{
			m_player.setMailbox(mailbox, sent);
		}

		void Session::setReceivedGiftboxes(const std::vector<Main::Structures::Giftbox>& giftboxes)
		{
			m_player.setReceivedGiftboxes(giftboxes);
			if (!giftboxes.empty())
			{
				m_packet.setTcpHeader(m_id, Common::Enums::NO_ENCRYPTION);
				m_packet.setCommand(64, 0, Main::Enums::GiftSystemExtra::GIFT_RECEIVED_NOTICE, 0);
				m_packet.setData(reinterpret_cast<const std::uint8_t*>(Common::Constants::teamString.c_str()), Common::Constants::maxNicknameSize);
				asyncWrite(m_packet);
			}
		}

		void Session::sendUnreadMailboxes()
		{
			const auto& newMailboxes = m_player.getMailboxReceived();
			std::vector<Main::Structures::Mailbox> unreadMailboxes;
			std::copy_if(newMailboxes.begin(), newMailboxes.end(), std::back_inserter(unreadMailboxes), [](const Main::Structures::Mailbox& mailbox) {
				return !mailbox.hasBeenRead;
				});

			m_packet.setTcpHeader(m_id, Common::Enums::NO_ENCRYPTION);
			m_packet.setCommand(104, 0, Main::Enums::MailboxExtra::MAILBOX_RECEIVED, 0);

			for (const auto& mailbox : unreadMailboxes)
			{
				m_packet.setData(reinterpret_cast<const std::uint8_t*>(&mailbox), sizeof(mailbox));
				asyncWrite(m_packet);
			}
		}

		void Session::sendAccountInfoConfirmation()
		{
			static const std::array<std::uint8_t, 28> unused{}; 
			m_packet.setTcpHeader(m_id, Common::Enums::NO_ENCRYPTION);
			m_packet.setCommand(413, 0, 59, 0);
			m_packet.setData(unused.data(), unused.size());
			asyncWrite(m_packet);
		}


		void Session::setAccountInfo(const AccountInfo& accountInfo)
		{
			this->setAccountId(accountInfo.accountID);
			m_player.setAccountInfo(accountInfo);

			m_packet.setTcpHeader(m_id, Common::Enums::USER_LARGE_ENCRYPTION);
			m_packet.setCommand(413, 0, 1, 0);
			m_packet.setData(reinterpret_cast<const std::uint8_t*>(&accountInfo), sizeof(accountInfo));
			asyncWrite(m_packet);
		}

		const Main::Structures::AccountInfo& Session::getAccountInfo() const
		{
			return m_player.getAccountInfo();
		}

		void Session::addBatteryObtainedInMatch(std::uint32_t newBattery)
		{
			m_player.addBatteryObtainedInMatch(newBattery);
		}

		std::unordered_map<Main::Structures::Friend, std::weak_ptr<Session>>& Session::getFriendSessions()
		{
			return m_player.getFriendSessions();
		}

		void Session::sendFriendRequest(std::shared_ptr<Main::Network::Session> targetSession, const char* nickname)
		{
			m_packet.setTcpHeader(m_id, Common::Enums::NO_ENCRYPTION);
			m_packet.setCommand(61, 0, 0, 0);
			m_packet.setData(nullptr, 0);

			if (m_player.getFriendlist().size() >= Common::Constants::maxFriends)
			{
				m_packet.setMission(Main::Enums::AddFriendServerMission::SENDER_FRIENDLIST_FULL);
				m_packet.setExtra(Main::Enums::AddFriendServerExtra::TARGET_OR_SENDER_FRIEND_LIST_FULL);
				asyncWrite(m_packet);
				return;
			}
			const AccountInfo& accountInfo = m_player.getAccountInfo();
			if (!targetSession)
			{
				handleOfflineFriendRequest(accountInfo, nickname);
			}
			else
			{
				handleOnlineFriendRequest(targetSession, accountInfo);
			}
		}

		void Session::handleOfflineFriendRequest(const AccountInfo& accountInfo, const char* nickname)
		{
			const Main::Enums::AddFriendServerExtra result = m_scheduler.immediatePersist(
				std::source_location::current(), &Main::Persistence::PersistentDatabase::addPendingFriendRequest, accountInfo.accountID, nickname);

			if (result != Main::Enums::AddFriendServerExtra::DB_ERROR && result != Main::Enums::AddFriendServerExtra::REQUEST_SENT)
			{
				if (result == Main::Enums::AddFriendServerExtra::TARGET_OR_SENDER_FRIEND_LIST_FULL)
				{
					m_packet.setMission(Main::Enums::AddFriendServerMission::RECEIVER_FRIENDLIST_FULL);
				}
				m_packet.setExtra(result);
				asyncWrite(m_packet);
			}
		}

		void Session::handleOnlineFriendRequest(std::shared_ptr<Main::Network::Session> targetSession, const AccountInfo& accountInfo)
		{
			if (!targetSession) return;
			if (targetSession->getAccountInfo().accountID == accountInfo.accountID)
			{ // disable self-friend request
				sendMessage("You cannot send a friend request to yourself!");
				return;
			}
			if (targetSession->getPlayer().getFriendlist().size() >= Common::Constants::maxFriends)
			{
				m_packet.setExtra(Main::Enums::AddFriendServerExtra::TARGET_OR_SENDER_FRIEND_LIST_FULL);
				m_packet.setMission(Main::Enums::AddFriendServerMission::RECEIVER_FRIENDLIST_FULL);
				asyncWrite(m_packet);
				return;
			}
			if (targetSession->getPlayer().hasBlocked(accountInfo.accountID))
			{
				m_packet.setExtra(Main::Enums::AddFriendServerExtra::RECEIVER_BLOCKED_SENDER);
				asyncWrite(m_packet);
				return;
			}
			m_packet.setExtra(Main::Enums::AddFriendServerExtra::SEND_REQUEST_TO_TARGET);
			Main::Structures::Friend friendStruct{ accountInfo.uniqueId, accountInfo.accountID };
			std::memcpy(friendStruct.targetNickname, m_player.getPlayerName(), 16);
			m_packet.setData(reinterpret_cast<std::uint8_t*>(&friendStruct), sizeof(friendStruct));
			targetSession->asyncWrite(m_packet);
		}

		bool Session::removeBossBattleTicket()
		{
			if (auto foundBossBattleSerialInfo = m_player.getBossBattleTicket())
			{
				return deleteItem(foundBossBattleSerialInfo.value(), "Boss Battle ticket removed automatically after starting Boss Battle match");
			}
			return false;
		}

		void Session::acceptFriendRequest(std::shared_ptr<Main::Network::Session> senderSession, const Main::Structures::Friend& target, const std::uint8_t* const data)
		{
			const auto& accountInfo = m_player.getAccountInfo();
			m_packet.setTcpHeader(m_id, Common::Enums::NO_ENCRYPTION);
			m_packet.setCommand(61, 0, Main::Enums::AddFriendServerExtra::REQUEST_ACCEPTED, 0);
			m_packet.setData(data, sizeof(accountInfo.uniqueId) + sizeof(accountInfo.accountID) + sizeof(accountInfo.nickname));

			if (!senderSession)
			{ // only the receiver is online
				addFriend(target);
				asyncWrite(m_packet);
			}
			else
			{ // sender and receiver are both online
				m_player.addOnlineFriend(senderSession);
				senderSession->addOnlineFriend(std::static_pointer_cast<Main::Network::Session>(shared_from_this()));
				m_scheduler.immediatePersist(std::source_location::current(), &Main::Persistence::PersistentDatabase::addFriend, 
					m_player.getAccountID(), senderSession->getAccountInfo().accountID);
				asyncWrite(m_packet);
			}
		}

		void Session::sendFriendList(std::vector<Main::Structures::Friend>& pendingRequests, std::uint32_t serverId)
		{
			m_packet.setTcpHeader(m_id, Common::Enums::NO_ENCRYPTION);
			if (!pendingRequests.empty())
			{
				m_packet.setCommand(61, 0, Main::Enums::AddFriendServerExtra::SEND_REQUEST_TO_TARGET, 2);
				for (auto& currentPendingFriend : pendingRequests)
				{
					// ignored later, but needed because if we send an empty UniqueId the client won't respond with packet 61
					currentPendingFriend.targetUniqueId = Main::Structures::UniqueId{ 0, serverId }; 
					m_packet.setData(reinterpret_cast<const std::uint8_t*>(&currentPendingFriend), sizeof(currentPendingFriend));
					asyncWrite(m_packet);
				}
			}
			const std::vector<Main::Structures::Friend> friendlist = m_player.getFriendlist();
			m_packet.setCommand(63, 0, 37, friendlist.size());
			m_packet.setData(reinterpret_cast<const std::uint8_t*>(friendlist.data()), friendlist.size() * sizeof(Main::Structures::Friend));
			asyncWrite(m_packet);
		}

		void Session::sendAccountInfo(Common::Network::Packet& response)
		{
			auto accountInfo = m_player.getAccountInfo();
			response.setData(reinterpret_cast<std::uint8_t*>(&accountInfo), sizeof(accountInfo));
			asyncWrite(response);
		}

		void Session::setPing(std::uint16_t ping)
		{
			m_player.setPing(ping);
		}

		void Session::setFriendList(const std::vector<Main::Structures::Friend>& friendlist)
		{
			m_player.setFriendList(friendlist);
		}

		void Session::logFriend(Main::Enums::FriendLogType logType, std::uint32_t targetAccountId)
		{ 
			m_packet.setTcpHeader(m_id, Common::Enums::NO_ENCRYPTION);
			m_packet.setCommand(85, 0, 53, static_cast<std::uint8_t>(logType));
			m_packet.setData(reinterpret_cast<std::uint8_t*>(&targetAccountId), sizeof(targetAccountId));
			asyncWrite(m_packet);
		}

		void Session::updateFriendSession(std::shared_ptr<Session> targetSession, bool remove)
		{
			if (targetSession)
			{
				Main::Structures::Friend targetFriend;
				const auto& targetAccountInfo = targetSession->getAccountInfo();
				targetFriend.targetAccountId = targetAccountInfo.accountID;
				targetFriend.targetUniqueId = remove ? Main::Structures::UniqueId{} : targetAccountInfo.uniqueId;
				std::memcpy(targetFriend.targetNickname, targetAccountInfo.nickname, 16);
				m_player.updateFriend(targetFriend, targetSession, remove);
			}
		}

		bool Session::blockAccount(std::uint32_t accountId, const char* nickname)
		{
			if (m_player.blockAccount(accountId, nickname))
			{
				m_packet.setTcpHeader(m_id, Common::Enums::NO_ENCRYPTION);
				m_packet.setCommand(52, 0, 1, 0);
				m_packet.setData(nullptr, 0);
				asyncWrite(m_packet);
				m_scheduler.addRepetitiveCallback(std::source_location::current(), m_player.getAccountID(), &Main::Persistence::PersistentDatabase::blockPlayer,
					m_player.getAccountID(), accountId);
				return true;
			}
			else
			{ // max blocked players (30) reached
				sendMessage("Maximum block list reached (30)", Main::Enums::INFO);
				return false;
			}
		}

		void Session::addAchievement(std::uint32_t idx)
		{
			using CollectionInfo = Common::ConstantDatabase::CdbSingleton<Common::ConstantDatabase::CdbCollectionInfo>;
			if (auto entryOpt = CollectionInfo::getInstance().getEntry(idx))
			{
				m_player.addAchievementTier1(idx);
				setAccountMicroPoints(m_player.getAccountInfo().microPoints + entryOpt->ci_reward_point);
				sendCurrency();
				m_scheduler.addRepetitiveCallback(std::source_location::current(), m_player.getAccountID(), &Main::Persistence::PersistentDatabase::addPlayerAchievement,
					m_player.getAccountID(), idx);
			}
			else
			{
				::Utils::Logger::log("Achievement EntryOpt was nullopt AID: " + std::to_string(m_player.getAccountID()),
					::Utils::LogType::Error, "Session::addAchievement");
			}
		}

		void Session::unblockAccount(std::uint32_t accountId)
		{
			START_BENCHMARK

			bool unblockSuccess = false;
			if (m_player.unblockAccount(accountId))
			{
				m_scheduler.addRepetitiveCallback(std::source_location::current(), m_player.getAccountID(), &Main::Persistence::PersistentDatabase::unblockPlayer,
					m_player.getAccountID(), accountId);
				unblockSuccess = true;
			}
			m_packet.setTcpHeader(m_id, Common::Enums::NO_ENCRYPTION);
			m_packet.setCommand(53, 0, unblockSuccess, 0);
			m_packet.setData(nullptr, 0);
			asyncWrite(m_packet);

			END_BENCHMARK(Session::unblockAccount, (*this))
		}

		void Session::sendBlockedPlayers()
		{
			START_BENCHMARK
			const std::vector<Main::Structures::BlockedPlayer>& blockedPlayers = m_player.getBlockedPlayers();
			m_packet.setTcpHeader(m_id, Common::Enums::NO_ENCRYPTION);
			m_packet.setCommand(54, 0, 37, blockedPlayers.size());  // max blocked players is 30, should never exceed 1440 bytes in total
			m_packet.setData(reinterpret_cast<const std::uint8_t*>(blockedPlayers.data()), blockedPlayers.size() * sizeof(Main::Structures::BlockedPlayer));
			asyncWrite(m_packet);
			END_BENCHMARK(Session::sendBlockedPlayers, (*this))
		}

		void Session::setIsInvisible(bool value)
		{
			m_isInvisible = value;
		}

		bool Session::isInvisible() const noexcept
		{
			return m_isInvisible;
		}

		void Session::setHasBeenMatchBanned(bool v)
		{
			m_hasBeenMatchBanned = v;
		}

		bool Session::hasBeenMatchBanned() const noexcept
		{
			return m_hasBeenMatchBanned;
		}

		void Session::setBlockedPlayers(const std::vector<Main::Structures::BlockedPlayer>& blockedPlayers)
		{
			m_player.setBlockedPlayers(blockedPlayers);
		}

		// call once with default "persist", since removeFriend removes the friend for both players
		void Session::deleteFriend(std::uint32_t targetAccountId, bool persist)
		{
			const bool deletedFriend = m_player.deleteFriend(targetAccountId);
			if (persist)
			{
				m_scheduler.immediatePersist(std::source_location::current(), &Main::Persistence::PersistentDatabase::removeFriend, m_player.getAccountID(), targetAccountId);
			}
		}

		bool Session::prolongItems(const std::vector<Main::Structures::BoughtItemToProlong>& toProlongItems, const std::vector<std::uint64_t>& itemDurations)
		{
			if (toProlongItems.empty() || toProlongItems.size() != itemDurations.size())
			{
				return false;
			}
			m_packet.setCommand(87, 1, 0, toProlongItems.size());
			m_packet.setData(reinterpret_cast<const std::uint8_t*>(toProlongItems.data()), toProlongItems.size() * sizeof(Main::Structures::BoughtItemToProlong));
			asyncWrite(m_packet);

			m_scheduler.addRepetitiveCallback(std::source_location::current(), m_player.getAccountID(), &Main::Persistence::PersistentDatabase::prolongItems, m_player.getAccountID(),
					toProlongItems, itemDurations, static_cast<__time32_t>(std::time(0)));
			return true;
		}

		// Assumptions: serialInfo is correct
		bool Session::upgradeWeapon(std::uint32_t itemId, const Main::Structures::ItemSerialInfo& serialInfo, bool hasParent,
			std::uint8_t mission, std::uint8_t option, bool useEnergyRefund, bool useGlue)
		{
			const std::uint32_t toAdd = hasParent ? 1 : (mission * 10 + 1);
			const std::uint32_t newItemId = itemId + toAdd;
			const auto newItemUpgradeInfo = Common::ConstantDatabase::CdbSingleton<Common::ConstantDatabase::CdbUpgradeInfo>::getInstance().getEntry(newItemId);
			if (!newItemUpgradeInfo)
			{
				sendMessage("[Session::upgradeWeapon] newItemUpgradeInfo is std::nullopt - report this issue if it's an error");
				return false;
			}

			static constexpr std::array<std::uint8_t, 72> unused{};
			const Main::Structures::AccountInfo& accountInfo = m_player.getAccountInfo();
			m_packet.setTcpHeader(m_id, Common::Enums::NO_ENCRYPTION);
			m_packet.setCommand(101, mission, Enums::UPGRADE_SUCCESS, option);
			m_packet.setData(unused.data(), unused.size());

			if (accountInfo.microPoints < newItemUpgradeInfo->ui_buy_point)
			{
				m_packet.setExtra(Enums::NOT_ENOUGH_MP_FOR_UPGRADE);
				asyncWrite(m_packet);
				return false;
			}
			auto itemEnergy = m_player.getItemEnergy(serialInfo);
			if (!itemEnergy)
			{
				m_packet.setExtra(Enums::UPGRADE_FAIL);
				asyncWrite(m_packet);
				sendMessage("[Session::upgradeWeapon] could not retrieve the energy for this item, please report this issue");
				return false;
			}
			// check that itemEnergy >= totalRequiredEnergy (from weapon CgdUtils)
			if (itemEnergy.value() < newItemUpgradeInfo->ui_use_exp)
			{
				m_packet.setExtra(Enums::UPGRADE_FAIL);
				asyncWrite(m_packet);
				sendMessage("[Session::upgradeWeapon] error: selected item does not have enough energy for an upgrade!");
				return false;
			}
			setAccountMicroPoints(accountInfo.microPoints - newItemUpgradeInfo->ui_buy_point);

			if (!useGlue && m_dist(m_gen) <= Common::Constants::upgradeFailRate)
			{
				m_packet.setExtra(Enums::UPGRADE_FAIL);
				asyncWrite(m_packet);
				if (!useEnergyRefund)
				{ // respawn a new identical item with 0 energy
					replaceItem(serialInfo, itemId, "Item upgrade attempt failed for itemID: " + std::to_string(itemId));
				} // if energy refund used, the item remains identical
			}
			else
			{
				asyncWrite(m_packet);
				replaceItem(serialInfo, newItemId, "Item upgraded successfully from ItemID: " + std::to_string(itemId) + " to ItemID: " + std::to_string(itemId + toAdd));
			}
			sendCurrency();
			return true;
		}

		void Session::resetUpgrade(const Main::ClientData::UpgradeReset& upgradeReset)
		{
			if (const auto itemIdOpt = m_player.findItemIdBySerialInfo(upgradeReset.weaponToResetSerialInfo); itemIdOpt && deleteItem(upgradeReset.upgradeResetItemSerialInfo,
				"Item deleted automatically while attempting to use upgrade reset on it"))
			{
				if (auto baseItemOpt = CdbUtils::getBaseItemId(*itemIdOpt); baseItemOpt)
				{
					replaceItem(upgradeReset.weaponToResetSerialInfo, *baseItemOpt, "Item upgrade reset successfully from ItemID: " + std::to_string(*itemIdOpt)
					+ " to ItemID: " + std::to_string(*baseItemOpt));
				}
				else sendMessage("[Session::resetUpgrade] error: No base item opt found");
			}
			else sendMessage("[Session::resetUpgrade] itemopt is nullopt or reset upgrade item not found");
		}

		void Session::refundItem(const Main::ClientData::ItemRefund& itemRefund)
		{
			auto originalItemId = m_player.findItemIdBySerialInfo(itemRefund.serialInfo);
			const bool removed = deleteItemBasic(itemRefund.serialInfo, "Session::refundItem");

			m_packet.setTcpHeader(m_id, Common::Enums::NO_ENCRYPTION);
			m_packet.setCommand(100, 0, removed ? Main::Enums::SellItemExtra::SELL_SUCCESS : Main::Enums::SellItemExtra::SELL_ERROR, 0);
			m_packet.setData(reinterpret_cast<const std::uint8_t*>(&itemRefund), sizeof(itemRefund));
			asyncWrite(m_packet);

			if (removed)
			{
				setAccountMicroPoints(getAccountInfo().microPoints + itemRefund.mpToAdd);
				if (originalItemId)
				{
					logItemInfo(itemRefund.serialInfo.itemNumber, *originalItemId, 0, "This item was sold");
				}
			}
		}

		bool Session::deleteItemBasic(const Main::Structures::ItemSerialInfo& itemSerialInfo, const std::string& caller)
		{
			//::Utils::Logger::log("Delete/Sold Item Number: " + std::to_string(itemSerialInfo.itemNumber) + 
			//	" from User: " + m_player.getAccountInfo().nickname + ", ip: " + m_ip, ::Utils::LogType::Warning);

			if (m_player.deleteItemBasic(itemSerialInfo))
			{
				m_scheduler.addRepetitiveCallback(std::source_location::current(), m_player.getAccountID(), 
					&Main::Persistence::PersistentDatabase::removePlayerItem,
					m_player.getAccountID(), static_cast<std::uint64_t>(itemSerialInfo.itemNumber), "Session::deletetemBasic");
				return true;
			}

			::Utils::Logger::log("Failed to delete/sell this item", ::Utils::LogType::Error);
			return false;
		}

		void Session::addItems(const std::vector<Item>& items)
		{
			m_player.addItems(items);
			m_scheduler.addRepetitiveCallback(std::source_location::current(), m_player.getAccountID(),
				&Main::Persistence::PersistentDatabase::addPlayerItems, m_player.getAccountID(), items, -1);
		}

		// Important notes: before using this function:
		// - make sure to call "setLatestItemNumber(getLatestItemNumber() + 1)"!
		// - make sure that Item.itemSerialNumber is already set appropriately
		// ^ These aren't performed inside this function because this function is used for multiple purposes (e.g. replacing an already existing item that has the correct
		//   Item.itemSerialNumber, in which case setLatestItemNumber must not be called)
		bool Session::addItem(const Item& item, bool isCouponItem)
		{
			if (!isCouponItem && !m_player.hasEnoughInventorySpace(1))
			{
				sendMessage("[error] Not enough inventory space!");
				return false;
			}
			else
			{
				m_scheduler.addRepetitiveCallback(std::source_location::current(), m_player.getAccountID(),
					&Main::Persistence::PersistentDatabase::addPlayerItem, item, m_player.getAccountID(), -1);
			}
			m_player.addItem(item);
			return true;
		}

		bool Session::addItem(const Main::Structures::Giftbox& item)
		{
			if (!m_player.hasEnoughInventorySpace(1))
			{
				sendMessage("[error] Not enough inventory space!");
				return false;
			}
			
			Item convertedItem{item.id};
			convertedItem.serialInfo.itemOrigin = 0;
			auto latestItemNumber = m_player.getLatestItemNumber();
			convertedItem.serialInfo.itemNumber = ++latestItemNumber;
			m_player.setLatestItemNumber(latestItemNumber);
			convertedItem.serialInfo.itemCreationDate = static_cast<__time32_t>(std::time(0));
			convertedItem.durability = Main::CdbUtils::getItemDurability(item.id).value_or(0);
			const std::uint32_t duration = Main::CdbUtils::getItemDuration(item.id);
			convertedItem.expirationDate = duration <= 3 ? duration : convertedItem.serialInfo.itemCreationDate + duration;

			m_scheduler.addRepetitiveCallback(std::source_location::current(), m_player.getAccountID(),
				&Main::Persistence::PersistentDatabase::addPlayerItem, convertedItem, m_player.getAccountID(), -1);
			m_player.addItem(convertedItem);
			return true;
		}

		void Session::addItems(const std::vector<Main::Structures::BoxItem>& boxItems, const Main::ClientData::BoxOpen& boxData, std::uint32_t extra)
		{
			if (boxItems.empty() || !m_player.hasEnoughInventorySpace(boxItems.size()) || !deleteItem(boxData.serialInfo, "The item was deleted after being used (Example: boxes)"))
			{
				return;
			}
			if (extra == 26 && !deleteItem(boxData.serialInfo2, "The item was deleted after being used (Example: mystery capsules"))
			{ // hammers
				sendMessage("[Session::addItems] fail: failed to delete used capsule item");
				return;
			}
			m_packet.setCommand(102, 1, extra, boxItems.size());
			m_packet.setData(reinterpret_cast<const std::uint8_t*>(boxItems.data()), sizeof(Main::Structures::BoxItem) * boxItems.size());
			asyncWrite(m_packet);

			setLatestItemNumber(boxItems.back().serialInfo.itemNumber);
			m_player.addItems(boxItems);
			m_scheduler.addRepetitiveCallback(std::source_location::current(), m_player.getAccountID(),
				&Main::Persistence::PersistentDatabase::addPlayerBoxItems, m_player.getAccountID(),
				boxItems, -1);

			m_scheduler.addRepetitiveCallback(std::source_location::current(), m_player.getAccountID(), &Main::Persistence::PersistentDatabase::logBoxItems,
				m_player.getAccountID(), boxItems);
		}

		bool Session::useInstantRespawn(const Main::Structures::ItemSerialInfo& serialInfo, const Common::Network::Packet& request)
		{
			auto response = request;
			response.setCommand(102, 1, 1, 0);

			const auto result = m_player.useInstantRespawn(serialInfo.itemNumber);
			if (result.first == Common::Enums::MATCHITEM_DELETE)
			{
				asyncWrite(response);
				return deleteItem(serialInfo, "Instant Respawn item deleted automatically after being used and total remaining usages == 0");
			}
			else if (result.first == Common::Enums::MATCHITEM_STOCKS_REDUCED_SUCCESS)
			{
				asyncWrite(response);
				m_scheduler.addRepetitiveCallback(std::source_location::current(), m_player.getAccountID(),
					&Main::Persistence::PersistentDatabase::updateItemStock, m_player.getAccountID(),
					serialInfo.itemNumber, result.second);
				return true;
			}
			else if (result.first == Common::Enums::MATCHITEM_STOCK_ZERO)
			{
				sendMessage("[Session::useMatchItem] server error - item stock is zero, please report this issue");
			}
			else
			{
				sendMessage("[Session::useMatchItem] server error - Item not found through ItemNumber, please report this issue");
			}
			return false;
		}

		bool Session::tryRemoveCoupons(std::uint32_t totalCouponsNeeded)
		{
			const auto result = m_player.tryRemoveCoupons(totalCouponsNeeded);
			if (result.action == Common::Enums::COUPON_ITEM_DELETE)
			{
				return deleteItem(result.serialInfo, "Coupon item deleted automatically after using the item");
			}
			else if (result.action == Common::Enums::MATCHITEM_STOCKS_REDUCED_SUCCESS)
			{
				m_scheduler.addRepetitiveCallback(std::source_location::current(), m_player.getAccountID(),
					&Main::Persistence::PersistentDatabase::updateItemStock, m_player.getAccountID(),
					result.serialInfo.itemNumber, result.newStock);
				return true;
			}
			else if (result.action == Common::Enums::MATCHITEM_STOCK_ZERO)
			{
				sendMessage("[Session::useMatchItem] server error - item stock is zero, please report this issue");
			}
			else
			{
				sendMessage("[Session::useMatchItem] server error - Item not found through ItemNumber, please report this issue");
			}

			return false;
		}

		bool Session::useNoPenalty(const Main::Structures::ItemSerialInfo& serialInfo, const Common::Network::Packet& request)
		{
			auto response = request;
			response.setCommand(102, 1, 1, 0);
			response.setData(reinterpret_cast<const std::uint8_t*>(&serialInfo), sizeof(serialInfo));

			const auto result = m_player.useInstantRespawn(serialInfo.itemNumber);
			if (result.first == Common::Enums::MATCHITEM_DELETE)
			{
				asyncWrite(response);
				return deleteItem(serialInfo, "In-match item deleted automatically after it was used and total usages left == 0 (Example: No Penalty item");
			}
			else if (result.first == Common::Enums::MATCHITEM_STOCKS_REDUCED_SUCCESS)
			{
				asyncWrite(response);
				m_scheduler.addRepetitiveCallback(std::source_location::current(), m_player.getAccountID(),
					&Main::Persistence::PersistentDatabase::updateItemStock, m_player.getAccountID(),
					serialInfo.itemNumber, result.second);
				return true;
			}
			else if (result.first == Common::Enums::MATCHITEM_STOCK_ZERO)
			{
				//sendMessage("[Session::useMatchItem] server error - item stock is zero, please report this issue");
			}
			else
			{
				//sendMessage("[Session::useMatchItem] server error - Item not found through ItemNumber, please report this issue");
			}
			return false;
		}

		// Assumptions:
		//  - The caller has validated all itemIDs (i.e. check whether the items given by the client really exist in vendorinfo.cdb)
		//  - BoughtItems are valid, i.e. their item numbers are already set
		//  - The latest item in BoughtItems has the greatest item number (which should be the last one that is set)
		bool Session::addItems(const std::vector<BoughtItem>& boughtItems, bool areCouponItems)
		{
			if (boughtItems.empty() || !m_player.hasEnoughInventorySpace(boughtItems.size()))
			{
				sendMessage("[Session::addItems] fail: boughtItems.empty() || !hasEnoughInventorySpace(boughtItems.size())");
				return false;
			}
			m_packet.setCommand(areCouponItems ? 91 : 87, 0, areCouponItems ? 1 : 0, boughtItems.size());
			m_packet.setData(reinterpret_cast<std::uint8_t*>(const_cast<BoughtItem*>(boughtItems.data())), boughtItems.size() * sizeof(Main::Structures::BoughtItem));
			asyncWrite(m_packet);

			m_player.setLatestItemNumber(boughtItems.back().serialInfo.itemNumber);
			m_player.addItems(boughtItems);

			m_scheduler.addRepetitiveCallback(std::source_location::current(), m_player.getAccountID(), &Main::Persistence::PersistentDatabase::addPlayerBoughtItems, 
				m_player.getAccountID(), boughtItems, -1);

			m_scheduler.addRepetitiveCallback(std::source_location::current(), m_player.getAccountID(), &Main::Persistence::PersistentDatabase::logBoughtItems,
				m_player.getAccountID(), boughtItems, areCouponItems);

			return true;
		}

		bool Session::setAccountRockTotens(std::uint32_t rt)
		{
			if (!m_player.setAccountRockTotens(rt)) return false;
			m_scheduler.addCallback(std::source_location::current(), m_player.getAccountID(), 1, &Main::Persistence::PersistentDatabase::updatePlayerCurrencyByType,
				m_player.getAccountID(), rt, Main::Enums::ItemCurrencyType::ITEM_RT);
			return true;
		}

		bool Session::setAccountMicroPoints(std::uint32_t mp)
		{
			if (!m_player.setAccountMicroPoints(mp)) return false;
			m_scheduler.addCallback(std::source_location::current(), m_player.getAccountID(), 2, &Main::Persistence::PersistentDatabase::updatePlayerCurrencyByType,
				m_player.getAccountID(), mp, Main::Enums::ItemCurrencyType::ITEM_MP);
			return true;
		}

		bool Session::setAccountCoins(std::uint16_t coins)
		{
			if (!m_player.setAccountCoins(coins)) return false;
			m_scheduler.addCallback(std::source_location::current(), m_player.getAccountID(), 3, &Main::Persistence::PersistentDatabase::updatePlayerCurrencyByType,
				m_player.getAccountID(), coins, Main::Enums::ItemCurrencyType::ITEM_COIN);
			return true;
		}

		void Session::setAccountLatestCharacterSelected(std::uint16_t latestCharacterSelected)
		{
			m_player.setAccountLatestCharacterSelected(latestCharacterSelected);
			m_scheduler.addCallback(std::source_location::current(), m_player.getAccountID(), 5, &Main::Persistence::PersistentDatabase::updateLatestSelectedCharacter,
				m_player.getAccountID(), latestCharacterSelected);
		}

		bool Session::setLevel(std::uint16_t level)
		{
			if (level < 0 || level > 105)
			{
				return false;
			}
			m_player.setLevel(level);
			m_scheduler.addCallback(std::source_location::current(), 
				m_player.getAccountID(), 6, &Main::Persistence::PersistentDatabase::updatePlayerLevel, m_player.getAccountID(), level);
			return true;
		}

		bool Session::setExperience(std::uint32_t experience)
		{
			m_player.setExperience(experience);
			m_scheduler.addCallback(std::source_location::current(),
				m_player.getAccountID(), 7, &Main::Persistence::PersistentDatabase::updatePlayerExperience, m_player.getAccountID(), experience);
			return true;
		}

		// This function is exclusively used for /setnickname cmd
		bool Session::setPlayerName(const std::string& playerName)
		{
		    if (playerName.size() >= 16)
		    {
		        sendMessage("error: the nickname cannot have more than 16 characters");
		        return false;
		    }
		
		    const bool changed = m_scheduler.immediatePersist(
		        std::source_location::current(),
		        &Main::Persistence::PersistentDatabase::updatePlayerName,
		        m_player.getAccountID(),
		        playerName.c_str()
		    );
		
		    if (!changed) 
		    {
		        sendMessage("error: there's already a player with this nickname");
		        return false;
		    }
		
		    m_player.setPlayerName(playerName.c_str());
		    return true;
		}

		// option and mission are probably related to the upgrade type (power, firing rate, etc)
		void Session::addEnergyToItem(const Main::ClientData::ItemAddEnergy& itemAddEnergy, std::uint16_t weaponType, std::uint16_t mission)
		{
			const auto result = m_player.addEnergyToItem(itemAddEnergy.serialInfo, itemAddEnergy.usedEnergy);
			if (result)
			{
				m_packet.setTcpHeader(m_id, Common::Enums::NO_ENCRYPTION);
				m_packet.setCommand(101, mission, Enums::ItemUpgradeExtra::ENERGY_ADD, weaponType);
				m_packet.setData(reinterpret_cast<const std::uint8_t*>(&itemAddEnergy), sizeof(itemAddEnergy));
				asyncWrite(m_packet);

				m_scheduler.addRepetitiveCallback(std::source_location::current(), 
					m_player.getAccountID(), &Main::Persistence::PersistentDatabase::insertEnergyToItem,
					m_player.getAccountID(), static_cast<std::uint32_t>(itemAddEnergy.serialInfo.itemNumber), result->first, static_cast<std::uint32_t>(result->second));
			}
		}

		void Session::addFriend(const Main::Structures::Friend& ffriend)
		{
			m_player.addOfflineFriend(ffriend);
			m_scheduler.immediatePersist(std::source_location::current(), 
				&Main::Persistence::PersistentDatabase::addFriend, m_player.getAccountID(), ffriend.targetAccountId);
		}

		std::optional<Main::Structures::Friend> Session::addOnlineFriend(std::shared_ptr<Main::Network::Session> session)
		{
			if (!session) return std::nullopt;
			return m_player.addOnlineFriend(session);
		}

		void Session::updateHwid(const std::string& hwid)
		{
			m_hwid = hwid;
		}

		void Session::storeHwid()
		{
			m_scheduler.immediatePersist(std::source_location::current(),
				&Main::Persistence::PersistentDatabase::updateHwid, m_player.getAccountID(), m_hwid);
		}

		void Session::equipItem(const std::uint16_t itemNumber)
		{
			m_player.equipItem(itemNumber, m_scheduler);
		}

		bool Session::replaceItem(const Main::Structures::ItemSerialInfo& serialInfo, std::uint32_t newItemId, const std::string& action)
		{
			// Logs
			auto itemIdOpt = m_player.findItemIdBySerialInfo(serialInfo);

			if (!CdbUtils::itemExists(newItemId))
			{
				sendMessage("[Session::replaceItem] error: itemID not found");
				return false;
			}
			if (!m_player.hasEnoughInventorySpace(1))
			{
				sendMessage("[Session::replaceItem] not enough inventory space!");
				return false;
			}
			if (!sendDeletePacket(serialInfo))
			{
				sendMessage("[Session::replaceItem] error: failed to delete original item");
				return false;
			}
			Main::Structures::SpawnedItem spawnedItem{ newItemId };
			spawnedItem.serialInfo = serialInfo;
			auto item = Item{ spawnedItem };
			m_player.addItem(item);

			m_packet.setCommand(66, 0, 51, 2);
			m_packet.setData(reinterpret_cast<const std::uint8_t*>(&spawnedItem), sizeof(spawnedItem));
			setLatestItemNumber(spawnedItem.serialInfo.itemNumber);
			asyncWrite(m_packet);

			m_scheduler.addRepetitiveCallback(std::source_location::current(), m_player.getAccountID(),
				&Main::Persistence::PersistentDatabase::replaceItemResetEnergy, m_player.getAccountID(), static_cast<std::uint64_t>(spawnedItem.serialInfo.itemNumber), newItemId);

			Main::Structures::ItemLogInfo log{ serialInfo.itemNumber, *itemIdOpt, 0, action};
			m_scheduler.addRepetitiveCallback(std::source_location::current(),
				m_player.getAccountID(), &Main::Persistence::PersistentDatabase::insertItemLog, m_player.getAccountID(), log);

			return true;
		}

		void  Session::logItemInfo(std::uint64_t itemNumber, std::uint32_t itemId, std::uint32_t expiration, const std::string& action)
		{
			Main::Structures::ItemLogInfo log{ itemNumber, itemId, expiration, action };
			m_scheduler.addRepetitiveCallback(std::source_location::current(),
				m_player.getAccountID(), &Main::Persistence::PersistentDatabase::insertItemLog, m_player.getAccountID(), log);
		}

		bool Session::spawnItemCommand(const std::uint32_t itemId, const std::string& action)
		{
			if (!CdbUtils::itemExists(itemId))
			{
				sendMessage("[Session::spawnItemCommand] error: itemID not found");
				return false;
			}

			Main::Structures::SpawnedItem spawnedItem{itemId};
			spawnedItem.serialInfo.itemNumber = m_player.getLatestItemNumber() + 1;

			const auto duration = CdbUtils::getItemDuration(itemId);
			spawnedItem.expirationDate = duration <= 3 ? static_cast<__time32_t>(duration) : static_cast<time_t>(std::time(0)) + duration;
			if (addItem(Item{ spawnedItem }))
			{
				m_packet.setCommand(66, 0, 51, 2);
				m_packet.setData(reinterpret_cast<const std::uint8_t*>(&spawnedItem), sizeof(spawnedItem));
				setLatestItemNumber(spawnedItem.serialInfo.itemNumber);
				asyncWrite(m_packet);

				logItemInfo(spawnedItem.serialInfo.itemNumber, itemId, spawnedItem.expirationDate, action);
				return true;
			}
			return false;
		}

		// use this for coupon items
		bool Session::spawnCoupon(const std::uint32_t total)
		{
			return spawnCouponCommon(total, false);
		}

		// use this to spawn coupons directly to account currency
		bool Session::spawnCouponImmediate(const std::uint32_t total)
		{
			return spawnCouponCommon(total, true);
		}

		bool Session::spawnCouponCommon(const std::uint32_t total, bool immediateToAccount)
		{
			const auto res = m_player.addCoupon(total);

			Main::Structures::SpawnedItem spawnedItem{ 1000000 };
			spawnedItem.itemId.stock = total;
			spawnedItem.serialInfo.itemNumber = m_player.getLatestItemNumber() + 1;
			spawnedItem.expirationDate = 0;

			if (res.action == Common::Enums::AddCouponAction::MUST_CREATE_NEW_COUPON)
			{
				Item convertedItem{ spawnedItem };
				convertedItem.unknown = true;

				if (!addItem(convertedItem, true))
					return false;

				if (immediateToAccount)
				{
					m_packet.setCommand(66, 0, 51, 2);
					m_packet.setData(reinterpret_cast<const std::uint8_t*>(&spawnedItem), sizeof(spawnedItem));
					setLatestItemNumber(spawnedItem.serialInfo.itemNumber);
					asyncWrite(m_packet);

					logItemInfo(spawnedItem.serialInfo.itemNumber, spawnedItem.itemId.itemId, spawnedItem.expirationDate,
						"Coupon item spawned automatically in Session::spawnCouponCommon");
				}
				else
				{
					m_player.addTotalCouponItems(convertedItem);
				}

				return true;
			}
			else if (res.action == Common::Enums::EXISTING_COUPON_STOCK_UPDATED)
			{
				if (immediateToAccount)
				{
					// trick the client in thinking it has more coupon items, since deleting coupon items doesn't work unless we use the coupon shop packets
					// this way, the client will keep the updated coupon total, consistent without relogging
					spawnedItem.serialInfo.itemNumber = 0; // on purpose, this is just for the client anyway
					m_packet.setCommand(66, 0, 51, 2);
					m_packet.setData(reinterpret_cast<const std::uint8_t*>(&spawnedItem), sizeof(spawnedItem));
					setLatestItemNumber(spawnedItem.serialInfo.itemNumber);
					asyncWrite(m_packet);

					logItemInfo(spawnedItem.serialInfo.itemNumber, spawnedItem.itemId.itemId, spawnedItem.expirationDate,
						"Coupon item spawned automatically in Session::spawnCouponCommon");
				}

				m_scheduler.addRepetitiveCallback(std::source_location::current(), m_player.getAccountID(), &Main::Persistence::PersistentDatabase::updateItemStock,
					m_player.getAccountID(), res.serialInfo.itemNumber, res.newStock);
				return true;
			}

			return false;
		}

		// Assumptions: giftDescription is < 256 characters; itemId is valid
		bool Session::receiveGift(std::uint32_t itemId, const std::string& giftDescription)
		{
			if (m_player.getMailboxReceived().size() > Common::Constants::maxMailbox)
			{
				return false;
			}
			else
			{
				m_packet.setCommand(64, 0, Main::Enums::GiftSystemExtra::GIFT_RECEIVED_NOTICE, 0);
				m_packet.setData(nullptr, 0);
				asyncWrite(m_packet);

				
				Main::Structures::Giftbox giftbox{ m_player.getAccountInfo().accountID, static_cast<__time32_t>(std::time(0)), itemId, itemId, itemId };
				std::memcpy(giftbox.nickname, Common::Constants::teamString.c_str(), Common::Constants::teamString.size());
				std::memcpy(giftbox.message, giftDescription.c_str(), giftDescription.size());
				addGiftboxReceived(giftbox);
				return true;
			}
		}

		void Session::deleteGiftbox(std::uint32_t timestamp)
		{
			m_player.deleteGiftbox(timestamp);
			m_scheduler.addRepetitiveCallback(std::source_location::current(), 
				m_player.getAccountID(), &Main::Persistence::PersistentDatabase::deleteReceivedGiftbox, m_player.getAccountID(), timestamp);
		}

		void Session::displayGiftboxes(Main::Enums::MailboxMission type)
		{
			m_packet.setTcpHeader(m_id, Common::Enums::NO_ENCRYPTION);
			if (type == Main::Enums::MailboxMission::MISSION_MAILBOX_SENT)
			{
				sendEmptyPacket(67, type);
				return; // currently, sending / sent gifts remains unimplemented
			}

			const auto& receivedGiftboxes = m_player.getGiftboxReceived();
			const std::uint32_t totalBytes = receivedGiftboxes.size() * sizeof(Main::Structures::Giftbox);
			m_packet.setCommand(67, type, 0, receivedGiftboxes.size());

			if (receivedGiftboxes.empty())
			{
				sendEmptyPacket(67);
			}
			else if (totalBytes < Common::Constants::maxPacketBytes)
			{
				m_packet.setExtra(37);
				m_packet.setData(reinterpret_cast<const std::uint8_t*>(receivedGiftboxes.data()), totalBytes);
				asyncWrite(m_packet);
				m_packet.setExtra(51); // this is needed as a "confirmation"
				m_packet.setData(nullptr, 0);
				asyncWrite(m_packet);
			}
			else
			{
				sendSeparatePackets(receivedGiftboxes);
				m_packet.setExtra(51);
				m_packet.setData(nullptr, 0);
				asyncWrite(m_packet);
			}
		}

		void Session::displayMailboxes(Main::Enums::MailboxMission mailboxType)
		{
			START_BENCHMARK

			const auto& actualMailbox = mailboxType == Main::Enums::MISSION_MAILBOX_RECEIVED ? m_player.getMailboxReceived() : m_player.getMailboxSent();
			m_packet.setTcpHeader(m_id, Common::Enums::NO_ENCRYPTION);
			m_packet.setCommand(106, mailboxType, 0, actualMailbox.size());
			const std::size_t totalBytes = actualMailbox.size() * sizeof(Main::Structures::Mailbox);

			if (actualMailbox.empty())
			{
				sendEmptyPacket(106, mailboxType);
			}
			else if (totalBytes < Common::Constants::maxPacketBytes)
			{
				m_packet.setExtra(37);
				m_packet.setData(reinterpret_cast<const std::uint8_t*>(actualMailbox.data()), totalBytes);
				asyncWrite(m_packet);
				m_packet.setExtra(51); // This is needed as a "confirmation"
				m_packet.setData(nullptr, 0);
				asyncWrite(m_packet);
			}
			else
			{
				sendSeparatePackets(actualMailbox);
				m_packet.setExtra(51);
				m_packet.setData(nullptr, 0);
				asyncWrite(m_packet);
			}

			END_BENCHMARK(Session::displayMailboxes, (*this))
		}


		bool Session::deleteItem(const Main::Structures::ItemSerialInfo& itemSerialInfoToDelete, const std::string& action)
		{
			if (auto itemIdOpt = m_player.findItemIdBySerialInfo(itemSerialInfoToDelete))
			{
				Main::Structures::ItemLogInfo itemLog{itemSerialInfoToDelete.itemNumber, *itemIdOpt, 0, action};
				m_scheduler.addRepetitiveCallback(std::source_location::current(),
					m_player.getAccountID(), &Main::Persistence::PersistentDatabase::insertItemLog, m_player.getAccountID(), itemLog);
			}

			const bool removed = deleteItemBasic(itemSerialInfoToDelete, "Session::deleteItem");
			const std::pair<std::uint32_t, Main::Structures::ItemSerialInfo> itemToDelete{ 1, itemSerialInfoToDelete };
			m_packet.setTcpHeader(m_id, Common::Enums::NO_ENCRYPTION);
			m_packet.setCommand(89, 0, removed, 0);
			m_packet.setData(reinterpret_cast<const std::uint8_t*>(&itemToDelete), sizeof(itemToDelete));
			asyncWrite(m_packet);

			if (!removed)
			{
				sendMessage("[Session::deleteItem] ERROR: Item deletion failed - either a DB error or a server logic error!");
			}

			return removed;
		}

		bool Session::sendDeletePacket(const Main::Structures::ItemSerialInfo& itemSerialInfoToDelete)
		{
			const bool removed = m_player.deleteItemBasic(itemSerialInfoToDelete);
			const std::pair<std::uint32_t, Main::Structures::ItemSerialInfo> itemToDelete{ 1, itemSerialInfoToDelete };
			m_packet.setTcpHeader(m_id, Common::Enums::NO_ENCRYPTION);
			m_packet.setCommand(89, 0, removed, 0);
			m_packet.setData(reinterpret_cast<const std::uint8_t*>(&itemToDelete), sizeof(itemToDelete));
			asyncWrite(m_packet);
			return removed;
		}

		void Session::sendMp(std::uint32_t mpToAdd)
		{
			const auto& accountInfo = m_player.getAccountInfo();
			std::pair<std::uint32_t, std::uint32_t> rtAndMpMessage{ accountInfo.rockTotens, accountInfo.microPoints + mpToAdd };
			m_packet.setCommand(307, 0, 0, 0);
			m_packet.setData(reinterpret_cast<std::uint8_t*>(&rtAndMpMessage), sizeof(std::uint32_t) * 2);
			asyncWrite(m_packet);
			setAccountMicroPoints(accountInfo.microPoints + mpToAdd);
		}

		void Session::sendRt(std::uint32_t rtToAdd)
		{
			const auto& accountInfo = m_player.getAccountInfo();
			std::pair<std::uint32_t, std::uint32_t> rtAndMpMessage{ accountInfo.rockTotens + rtToAdd , accountInfo.microPoints };
			m_packet.setCommand(307, 0, 0, 0);
			m_packet.setData(reinterpret_cast<std::uint8_t*>(&rtAndMpMessage), sizeof(std::uint32_t) * 2);
			asyncWrite(m_packet);
			setAccountRockTotens(accountInfo.rockTotens + rtToAdd);
		}

		void Session::reduceEquippedItemsDurability(std::uint32_t weaponRestriction)
		{
			const auto characterID = m_player.getAccountInfo().latestSelectedCharacter;
			auto cont = m_player.reduceEquippedItemsDurabilities(characterID, weaponRestriction);
			auto weaponDurabilityDamages = cont.first;
			auto weaponsToDamageIds = cont.second;

			m_scheduler.addRepetitiveCallback(std::source_location::current(), m_player.getAccountID(), &Main::Persistence::PersistentDatabase::reduceDurability,
				m_player.getAccountID(), weaponsToDamageIds);

			m_packet.setOrder(93);
			m_packet.setOption(weaponDurabilityDamages.size());
			m_packet.setData(reinterpret_cast<const std::uint8_t*>(weaponDurabilityDamages.data()),
				weaponDurabilityDamages.size() * sizeof(Main::ClientData::SingleWeaponDurabilityDamage));
			asyncWrite(m_packet);
		}

		void Session::updateItemDurability(std::uint32_t itemNumber, std::uint32_t newDurability)
		{
			m_scheduler.addRepetitiveCallback(std::source_location::current(),
				m_player.getAccountID(), &Main::Persistence::PersistentDatabase::updateItemDurability, m_player.getAccountID(), itemNumber, newDurability);
			m_player.updateItemDurabilityByNumber(itemNumber, newDurability);
		}

		void Session::sendCurrency()
		{ // NB: Set the currency before using this function

			m_packet.setCommand(307, 0, 0, 0);
			const auto& accountInfo = m_player.getAccountInfo();
			struct CurrencyData { std::uint32_t rt; std::uint32_t mp; std::uint32_t coins; };
			CurrencyData message{ accountInfo.rockTotens, accountInfo.microPoints, accountInfo.coins };
			m_packet.setData(reinterpret_cast<std::uint8_t*>(&message), sizeof(std::uint32_t) * 3);
			asyncWrite(m_packet);
		}

		void Session::switchItemEquip(std::uint32_t characterId, std::uint64_t itemNumber)
		{
			if (characterId != -1 && characterId >= Common::Enums::MAX_CHARACTERS) return;

			if (!m_player.unequipItemIfEquipped(itemNumber, characterId, m_scheduler))
			{
				m_player.equipItemIfNotEquipped(itemNumber, characterId, m_scheduler);
			}
		}

		void Session::unequipItem(std::uint64_t itemType)
		{
			if (itemType >= Common::Constants::maxItemType) return;
			m_player.unequipItemImpl(itemType, m_scheduler);
		}

		void Session::updateSingleWaveScore(std::uint32_t score, std::uint32_t stage)
		{
			auto ainfo = m_player.getAccountInfo();
			if (ainfo.highestSingleWaveScore < score || ainfo.highestSinglewaveStage < stage)
			{
				if (ainfo.highestSingleWaveScore < score)
				{
					ainfo.highestSingleWaveScore = score;
					m_player.setAccountInfo(ainfo);
				}
				if (ainfo.highestSinglewaveStage < stage)
				{
					ainfo.highestSinglewaveStage = stage;
					m_player.setAccountInfo(ainfo);
				}
				m_scheduler.addRepetitiveCallback(std::source_location::current(), 
					ainfo.accountID, &Main::Persistence::PersistentDatabase::updatePlayerStats, ainfo.accountID, ainfo);
			}
		}

		void Session::completeTutorial()
		{
			auto ainfo = m_player.getAccountInfo();
			if (ainfo.isTutorialDone) return;

			ainfo.isTutorialDone = true;
			m_player.setAccountInfo(ainfo);

			m_scheduler.addRepetitiveCallback(std::source_location::current(), 
				ainfo.accountID, &Main::Persistence::PersistentDatabase::updatePlayerStats, ainfo.accountID, ainfo);

			setAccountMicroPoints(ainfo.microPoints + 3000);
			sendCurrency();
			spawnItemCommand(Common::Constants::tutorialBox, "Item was automatically spawned after completing the tutorial");
		}

		void Session::setLatestItemNumber(std::uint64_t itemNum)
		{
			m_player.setLatestItemNumber(itemNum);
		}

		bool Session::banAccount(std::uint64_t daysDuration, const std::string& reason, bool isMatchBan)
		{
			if (isMatchBan)
			{
				m_hasBeenMatchBanned = true;
			}

			using namespace std::chrono;
			using namespace std::literals;
			zoned_time zt{ "UTC", local_seconds{duration_cast<seconds>(system_clock::now().time_since_epoch()) + seconds(daysDuration * 24 * 60 * 60)}};
			const std::string bannedUntil = std::format("{:%Y-%m-%d %H:%M:%S}", zt.get_sys_time());

			if (m_scheduler.immediatePersist(std::source_location::current(), 
				&Main::Persistence::PersistentDatabase::updateSuspension, m_player.getAccountInfo().nickname,
				bannedUntil, reason, Main::Enums::GRADE_MOD))
			{
				if (!isMatchBan)
				{
					m_packet.setCommand(73, 0, 1, 0);
					m_packet.setData(nullptr, 0);
					asyncWrite(m_packet);
				}
				return true;
			}
			return false;
		}

		bool Session::muteAccount(std::uint64_t daysDuration, const std::string& reason, const std::string& mutedBy)
		{
			using namespace std::chrono;
			using namespace std::literals;
			zoned_time zt{ "UTC", local_seconds{duration_cast<seconds>(system_clock::now().time_since_epoch()) + seconds(daysDuration * 24 * 60 * 60)}};
			const std::string mutedUntil = std::format("{:%Y-%m-%d %H:%M:%S}", zt.get_sys_time());
			if (m_scheduler.immediatePersist(std::source_location::current(), 
				&Main::Persistence::PersistentDatabase::updateMute, m_player.getAccountInfo().nickname,
				mutedUntil, reason, mutedBy, Main::Enums::GRADE_MOD))
			{
				m_player.mute(reason, mutedBy, mutedUntil);
				return true;
			}
			return false;
		}

		void Session::setRoomCreationDisabled()
		{
			m_player.disableRoomCreation();
		}

		void Session::setVotekickDisabled()
		{
			m_player.disableVotekick();
		}

		bool Session::disableRoomCreation(std::uint64_t daysDuration)
		{
			using namespace std::chrono;
			using namespace std::literals;
			zoned_time zt{ "UTC", local_seconds{duration_cast<seconds>(system_clock::now().time_since_epoch()) + seconds(daysDuration * 24 * 60 * 60)}};
			const std::string disabledUntil = std::format("{:%Y-%m-%d %H:%M:%S}", zt.get_sys_time());
			if (m_scheduler.immediatePersist(std::source_location::current(),
				&Main::Persistence::PersistentDatabase::updateRoomCreationDisabledUntil, m_player.getAccountInfo().nickname,
				disabledUntil))
			{
				m_player.disableRoomCreation();
				return true;
			}
			return false;
		}

		bool Session::disableVotekick(std::uint64_t daysDuration)
		{
			using namespace std::chrono;
			using namespace std::literals;
			zoned_time zt{ "UTC",
				local_seconds{duration_cast<seconds>(system_clock::now().time_since_epoch()) + seconds(daysDuration * 24 * 60 * 60)}
			};
			const std::string disabledUntil = std::format("{:%Y-%m-%d %H:%M:%S}", zt.get_sys_time());

			if (m_scheduler.immediatePersist(std::source_location::current(),
				&Main::Persistence::PersistentDatabase::updateVotekickDisabledUntil,
				m_player.getAccountInfo().nickname,
				disabledUntil))
			{
				m_player.disableVotekick();
				return true;
			}
			return false;
		}


		void Session::sendLobbyList(const std::vector<std::shared_ptr<Session>>& allSessions)
		{
			m_packet.setTcpHeader(m_id, Common::Enums::NO_ENCRYPTION);
			m_packet.setCommand(84, 0, 0, allSessions.size());

			if (allSessions.empty() || allSessions.size() == 1)
			{
				sendEmptyPacket(84);
				return;
			}
			std::vector<Main::Structures::SinglePlayerInfoList> playerList;
			for (const auto& currentSession : allSessions)
			{
				if (!currentSession) continue;
				if (currentSession->getId() == m_id || !currentSession->getPlayer().isInLobby()) continue; // skip self
				const auto& partialAccountData = currentSession->getAccountInfo();
				Main::Structures::SinglePlayerInfoList singlePlayerList;
				singlePlayerList.clanLogoBackId = partialAccountData.clanLogoBackId;
				singlePlayerList.clanLogoFrontId = partialAccountData.clanLogoFrontId;
				singlePlayerList.level = partialAccountData.playerLevel;
				strncpy(singlePlayerList.name, partialAccountData.nickname, sizeof(singlePlayerList.name) - 1);
				singlePlayerList.name[sizeof(singlePlayerList.name) - 1] = '\0'; 
				singlePlayerList.uniqueId.server = partialAccountData.uniqueId.server;
				singlePlayerList.uniqueId.session = partialAccountData.uniqueId.session;
				singlePlayerList.uniqueId.unknown = partialAccountData.uniqueId.unknown;
				playerList.push_back(singlePlayerList);
			}

			const std::size_t totalBytes = playerList.size() * sizeof(Main::Structures::SinglePlayerInfoList);
			if (totalBytes < Common::Constants::maxPacketBytes)
			{
				m_packet.setExtra(37);
				m_packet.setData(reinterpret_cast<std::uint8_t*>(playerList.data()), totalBytes);
				asyncWrite(m_packet);
			}
			else
			{
				sendSeparatePackets(playerList);
			}
		}

		void Session::sendClanList(const std::vector<std::shared_ptr<Session>>& allSessions)
		{
			m_packet.setTcpHeader(m_id, Common::Enums::NO_ENCRYPTION);
			if (allSessions.empty() || allSessions.size() == 1)
			{
				sendEmptyPacket(57);
			}
			else
			{
				const auto& selfAccountInfo = m_player.getAccountInfo();
				std::vector<Main::Structures::SingleLobbyClanList> playerList;
				for (const auto& currentSession : allSessions)
				{
					if (!currentSession) continue;
					if (currentSession->getId() == m_id) continue; // Skip self user
					const auto& partialAccountData = currentSession->getAccountInfo();
					if (partialAccountData.clanId != selfAccountInfo.clanId) continue; // Skip non clan members
					Main::Structures::SingleLobbyClanList singlePlayerList;
					singlePlayerList.level = partialAccountData.playerLevel;
					strncpy(singlePlayerList.name, partialAccountData.nickname, sizeof(singlePlayerList.name) - 1);
					singlePlayerList.name[sizeof(singlePlayerList.name) - 1] = '\0';
					singlePlayerList.uniqueId.server = partialAccountData.uniqueId.server;
					singlePlayerList.uniqueId.session = partialAccountData.uniqueId.session;
					singlePlayerList.uniqueId.unknown = partialAccountData.uniqueId.unknown;
					playerList.push_back(singlePlayerList);
				}
				m_packet.setCommand(57, 0, 37, playerList.size());
				m_packet.setData(reinterpret_cast<std::uint8_t*>(playerList.data()), playerList.size() * sizeof(Main::Structures::SingleLobbyClanList));
				asyncWrite(m_packet);
			}
		}

		bool Session::unmuteAccount()
		{
			m_player.unmute();
			return m_scheduler.immediatePersist(std::source_location::current(), 
				&Main::Persistence::PersistentDatabase::unmuteAccount, m_player.getAccountInfo().nickname);
		}

		bool Session::enableRoomCreation()
		{
			m_player.enableRoomCreation();
			return m_scheduler.immediatePersist(std::source_location::current(),
				&Main::Persistence::PersistentDatabase::resetRoomCreationDisabledUntil, m_player.getAccountInfo().nickname);
		}

		bool Session::enableVotekick()
		{
			m_player.enableVotekick();
			return m_scheduler.immediatePersist(std::source_location::current(), &Main::Persistence::PersistentDatabase::resetVotekickDisabledUntil,
				m_player.getAccountInfo().nickname);
		}

		void Session::setMute(Main::Structures::MuteInfo val)
		{
			val.isMuted ? m_player.mute(val.reason, val.mutedBy, val.mutedUntil) : m_player.unmute();
		}

		void Session::clear()
		{
			m_scheduler.persistFor(m_player.getAccountID());
		}

		void Session::setPlayerState(Common::Enums::PlayerState playerState)
		{
			m_player.setPlayerState(playerState);
			if (playerState == Common::Enums::STATE_NORMAL)
			{
				setIsInMatch(true);
			}
			else if (playerState == Common::Enums::STATE_WAITING)
			{
				setIsInMatch(false);
			}
		}

		void Session::addLuckyPoints(std::uint32_t points)
		{
			m_player.addLuckyPoints(points);
			m_scheduler.immediatePersist(std::source_location::current(), 
				&Persistence::PersistentDatabase::updatePlayerLuckyPoints, m_player.getAccountID(), m_player.getLuckyPoints());
		}

		void Session::setLuckyPoints(std::uint32_t points)
		{
			m_player.setLuckyPoints(points);
			m_scheduler.immediatePersist(std::source_location::current(), 
				&Persistence::PersistentDatabase::updatePlayerLuckyPoints, m_player.getAccountID(), m_player.getLuckyPoints());
		}

		void Session::setRoomNumber(std::uint16_t roomNumber)
		{
			m_player.setRoomNumber(roomNumber);
		}

		void Session::setClanRoomNumber(std::uint16_t number)
		{
			m_player.setClanRoomNumber(number);
		}

		void Session::leaveRoom()
		{
			m_isInvisible = false;
			m_player.leaveRoom();
		}

		void Session::decreaseRoomNumber()
		{
			m_player.decreaseRoomNumber();
		}

		void Session::setIsInMatch(bool val)
		{
			m_player.setIsInMatch(val);
		}

		void Session::sendBattery(std::uint32_t battery)
		{
			if (battery != 500 && battery != 1000) return;

			const std::uint32_t oldBatteryQuantity = m_player.getAccountInfo().battery;
			const std::uint32_t totalNewBattery = m_player.addBattery(battery);

			if (totalNewBattery > oldBatteryQuantity)
			{
				m_scheduler.addRepetitiveCallback(std::source_location::current(),
					m_player.getAccountID(), &Main::Persistence::PersistentDatabase::updateBattery, m_player.getAccountID(),
					totalNewBattery);
			}
		}

		void Session::resetKillDeath()
		{
			m_player.resetKillDeath();
			const std::uint32_t accountId = m_player.getAccountInfo().accountID;
			m_scheduler.addRepetitiveCallback(std::source_location::current(), 
				accountId, &Main::Persistence::PersistentDatabase::resetKillDeath, accountId);
		}

		void Session::resetRecord()
		{
			m_player.resetRecord();
			const std::uint32_t accountId = m_player.getAccountInfo().accountID;
			m_scheduler.addRepetitiveCallback(std::source_location::current(), 
				accountId, &Main::Persistence::PersistentDatabase::resetRecord, accountId);
		}

		void Session::expandBattery()
		{
			if (m_player.expandBattery())
			{
				const std::uint32_t accountId = m_player.getAccountInfo().accountID;
				m_scheduler.addRepetitiveCallback(std::source_location::current(),
					accountId, &Main::Persistence::PersistentDatabase::batteryExpansion, accountId);
			}
			else
			{
				sendMessage("Error: Maximum possible battery is 5000");
			}
		}

		void Session::expandInventory(std::uint32_t spaceToAdd)
		{
			if (m_player.expandInventory(spaceToAdd))
			{
				const std::uint32_t accountId = m_player.getAccountInfo().accountID;
				m_scheduler.addRepetitiveCallback(std::source_location::current(), 
					accountId, &Main::Persistence::PersistentDatabase::inventoryExpansion, accountId, spaceToAdd);
				sendMessage("Successfully expanded inventory space, relog", Main::Enums::TIP);
			}
			else
			{
				sendMessage("Error: Maximum possible inventory space is 1000!");
			}
		}

		void Session::sendWeeklyReward()
		{
			START_BENCHMARK
			using std::chrono::system_clock;
			using std::chrono::current_zone;

			const std::string& latestOpenedRewardDate = m_player.getLatestWeeklyRewardDate();
			const std::string todayDate = std::format("{:%F}", system_clock::now());
			if (latestOpenedRewardDate != todayDate)
			{
				if (!m_player.hasEnoughInventorySpace(1))
				{
					sendMessage("The weekly reward cannot be received since your inventory is full. Free some space and re-log to receive the reward", 
						Main::Enums::TIP);
					return;
				}
				auto genWeeklyRewards = [] { return Main::Details::generateRewards<7>(); };
				Main::Structures::WeeklyReward rewards(m_scheduler.immediatePersist(std::source_location::current(),
					&Main::Persistence::PersistentDatabase::getWeeklyRewards<decltype(genWeeklyRewards)>, genWeeklyRewards));

				m_packet.setTcpHeader(m_id, Common::Enums::NO_ENCRYPTION);
				m_packet.setCommand(182,0,28,2);
				m_packet.setData(reinterpret_cast<std::uint8_t*>(&rewards), sizeof(rewards));
				asyncWrite(m_packet);

				const std::uint32_t day = rewards.day - 1;
				if (day < rewards.items.size() && Main::CdbUtils::itemExists(rewards.items[day]))
				{
					m_packet.setCommand(66, 0, 51, 1); // n.b. option0, mission3 => story reward
					Main::Structures::SpawnedItem spawnedItem{ rewards.items[day] };
					spawnedItem.serialInfo.itemNumber = m_player.getLatestItemNumber() + 1;
					addItem(Item{ spawnedItem });
					setLatestItemNumber(spawnedItem.serialInfo.itemNumber);

					m_packet.setData(reinterpret_cast<std::uint8_t*>(&spawnedItem), sizeof(spawnedItem));
					asyncWrite(m_packet);
					m_scheduler.immediatePersist(std::source_location::current(), 
						&Main::Persistence::PersistentDatabase::updateLatestRewardDay, "LatestWeeklyRewardDay", m_player.getAccountID(), todayDate);
				}
			}
			END_BENCHMARK(Session::sendWeeklyReward, (*this))
		}

		void Session::sendMonthlyReward()
		{
			START_BENCHMARK
			using std::chrono::system_clock;
			using std::chrono::current_zone;

			const std::string& latestOpenedRewardDate = m_player.getLatestMonthlyRewardDate();
			const std::string todayDate = std::format("{:%F}", system_clock::now());
			if (latestOpenedRewardDate != todayDate)
			{
				if (!m_player.hasEnoughInventorySpace(1))
				{
					sendMessage("The monthly reward cannot be received since your inventory is full. Free some space and re-log to receive the reward",
						Main::Enums::TIP);
					return;
				}
				auto genMonthlyRewards = [] { return Main::Details::generateRewards<32>(); };
				Main::Structures::MonthlyReward rewards(m_scheduler.immediatePersist(std::source_location::current(),
					&Main::Persistence::PersistentDatabase::getMonthlyRewards<decltype(genMonthlyRewards)>, genMonthlyRewards));				
				m_packet.setTcpHeader(m_id, Common::Enums::NO_ENCRYPTION);
				m_packet.setCommand(172, 0, 28, 1); // option=0 not shown, option=1 shown
				m_packet.setData(reinterpret_cast<std::uint8_t*>(&rewards), sizeof(rewards));
				asyncWrite(m_packet);

				if (rewards.day < rewards.items.size() && Main::CdbUtils::itemExists(rewards.items[rewards.day]))
				{
					m_packet.setCommand(66, 0, 51, 1); // n.b. option0, mission3 => story reward
					Main::Structures::SpawnedItem spawnedItem{ rewards.items[rewards.day] };
					spawnedItem.serialInfo.itemNumber = m_player.getLatestItemNumber() + 1;
					addItem(Item{ spawnedItem });
					setLatestItemNumber(spawnedItem.serialInfo.itemNumber);

					m_packet.setData(reinterpret_cast<std::uint8_t*>(&spawnedItem), sizeof(spawnedItem));
					asyncWrite(m_packet);
					m_scheduler.immediatePersist(std::source_location::current(), 
						&Main::Persistence::PersistentDatabase::updateLatestRewardDay, "LatestMonthlyRewardDay", m_player.getAccountID(), todayDate);
				}
			}
			END_BENCHMARK(Session::sendMonthlyReward, (*this))
		}

		void Session::sendInventory(std::uint32_t accountID)
		{
			START_BENCHMARK
			auto playerItems = m_scheduler.immediatePersist(std::source_location::current(), 
				&Main::Persistence::PersistentDatabase::getPlayerItems, accountID);

			auto& nonEquippedItems = playerItems.first;
			auto& equippedItems = playerItems.second;

			m_packet.setTcpHeader(m_id, Common::Enums::USER_LARGE_ENCRYPTION);

			// non-equipped items
			m_packet.setCommand(77, 0, 0, nonEquippedItems.size());
			const std::size_t totalSize = Common::Constants::headerSize + nonEquippedItems.size() * sizeof(Main::Structures::Item);
			if (totalSize == Common::Constants::headerSize)
			{
				sendEmptyPacket(77);
			}
			else if (totalSize < Common::Constants::maxPacketBytes)
			{
				m_packet.setExtra(37);
				m_packet.setData(reinterpret_cast<std::uint8_t*>(nonEquippedItems.data()), nonEquippedItems.size() * sizeof(Main::Structures::Item));
				asyncWrite(m_packet);
			}
			else
			{
				sendSeparatePackets(nonEquippedItems);
			}
			m_player.setUnequippedItems(nonEquippedItems);

			// equipped items
			m_packet.setOrder(75);
			for (auto& [characterID, items] : equippedItems)
			{
				m_packet.setExtra(characterID);
				m_packet.setOption(items.size());
				m_packet.setData(reinterpret_cast<std::uint8_t*>(items.data()), items.size() * sizeof(Main::Structures::EquippedItem));
				asyncWrite(m_packet);
			}
			m_player.setEquippedItems(equippedItems);

			// Client expects this as confirmation
			m_packet.setOption(0);
			m_packet.setExtra(16);
			m_packet.setData(nullptr, 0);
			asyncWrite(m_packet);	

			END_BENCHMARK(Main::Persistence::PersistentDatabase::getPlayerItems, (*this))
		}

		void Session::setMatchStartTime()
		{
			m_matchStartTime = Main::Details::getUtcTimeMs();
		}

		void Session::setEventMissions(const std::unordered_map<std::uint32_t, std::uint32_t>& activeEventIds)
		{
			if (activeEventIds.size() > Common::Constants::totalEventMissions)
			{
				sendMessage("setEventMissions - error: activeEventIds.size() > 5");
				return;
			}

			std::vector<std::uint32_t> eventIds;
			for (const auto& [eventId, progress] : activeEventIds)
			{
				eventIds.push_back(eventId); 
			}

			if (!eventIds.empty())
			{
				Common::Network::Packet startPacket;
				startPacket.setCommand(167, 1, 0, eventIds.size());
				startPacket.setData(reinterpret_cast<std::uint8_t*>(eventIds.data()), static_cast<std::uint32_t>(eventIds.size() * sizeof(std::uint32_t)));
				asyncWrite(startPacket);
			}

			Common::Network::Packet packet;
			packet.setTcpHeader(getId(), Common::Enums::NO_ENCRYPTION);
			packet.setCommand(168, 1, 2, 0);

			ClientData::EventMissionPoint emissionPoint;
			for (const auto& [eventId, progress] : activeEventIds)
			{
				emissionPoint.eventIndex = eventId;  
				packet.setData(reinterpret_cast<std::uint8_t*>(&emissionPoint), sizeof(emissionPoint));

				for (std::uint32_t i = 0; i < progress; ++i)
				{
					asyncWrite(packet);
				}
			}

			m_eventMissions = activeEventIds;
		}

		void Session::sendEventMission(const ClientData::EventMissionPoint& eventMission)
		{
			const std::uint32_t index = eventMission.eventIndex;
			if (m_eventMissions.find(index) == m_eventMissions.end())
			{
				//sendMessage("[sendEventMission] error while sending event mission point (EventMissionIndex: " + std::to_string(index) + ", ActiveEventsSize: " + 
					//std::to_string(m_eventMissions.size()) + ") - please report this issue");
				return;
			}
			
			auto& currentTotal = m_eventMissions[index];  
			if (currentTotal < Common::Constants::eventMissionTotal)
			{
				sendMessage("Obtained one point for event mission idx: " + std::to_string(index));

				++currentTotal; 

				m_packet.setTcpHeader(getId(), Common::Enums::NO_ENCRYPTION);
				m_packet.setCommand(168, 1, 2, 0);
				m_packet.setData(reinterpret_cast<const std::uint8_t*>(&eventMission), sizeof(eventMission));
				asyncWrite(m_packet);

				m_scheduler.addRepetitiveCallback(std::source_location::current(),
					m_player.getAccountID(), &Main::Persistence::PersistentDatabase::savePlayerMissions,
					m_player.getAccountID(), m_eventMissions);
			}
			else
			{
				sendMessage("[sendEventMission] server error: currentTotal < Common::Constants::eventMissionTotal [with currentTotal: " + std::to_string(currentTotal) + "]"
				+ ", eventMissionIndex: " + std::to_string(index));
			}
		}

		void Session::sendEventMissionReward(std::uint32_t eventIndex)
		{
			if (eventIndex == 0 || eventIndex > Common::Constants::totalEventMissions)
			{
				return;
			}
			auto it = m_eventMissions.find(eventIndex);
			if (it == m_eventMissions.end() || it->second != Common::Constants::eventMissionTotal - 1)
			{
				sendMessage("[sendEventMissionReward] server error: you don't have exactly 50 points to obtain this reward");
				return;
			}

			if (auto rewardId = Main::CdbUtils::getEventMissionRewardFor(eventIndex))
			{
				if (spawnItemCommand(*rewardId, "Item spawned automatically - event mission rewards"))
				{
					Common::Network::Packet packet;
					packet.setTcpHeader(getId(), Common::Enums::NO_ENCRYPTION);
					packet.setCommand(168, 0, 8, 0);
					packet.setData(reinterpret_cast<const std::uint8_t*>(&eventIndex), sizeof(eventIndex));
					asyncWrite(packet);

					m_scheduler.addRepetitiveCallback(std::source_location::current(),
						m_player.getAccountID(), &Main::Persistence::PersistentDatabase::updatePlayerMissionProgress, m_player.getAccountID(),
						eventIndex, Common::Constants::eventMissionTotal);

					// Also send 5,000 RT for each event mission + 10 coupons + boss battle ticket
					sendRt(5'000);
					spawnCouponImmediate(5);
					spawnItemCommand(4811300, "Item spawned automatically - event mission rewards");
					sendMessage("You obtained 5'000 RT, 5 coupons and a Boss Battle ticket!", Main::Enums::TIP);
				}
				else
				{
					sendMessage("[sendEventMissionReward] server error: failed to spawn item - please report this issue");
				}
			}
			else
			{
				sendMessage("[sendEventMissionReward] server error: reward id not found for the given event id - please report this issue");
			}
		}


		void Session::storeEndMatchStats(std::uint64_t totalPlaytimeSeconds, const Main::Structures::ScoreboardResponse& stats, Main::Enums::MatchEnd matchEnd, 
			bool hasLeveledUp, bool isZombieMode, bool isClanMatch)
		{
			auto ainfo = m_player.getAccountInfo();
			if (isZombieMode)
			{
				ainfo.zombieKills += (stats.totalKills / 3);
				ainfo.infected += stats.meleeKills;
			}
			else
			{
				ainfo.meleeKills += stats.meleeKills;
				ainfo.totalKills += stats.totalKills;
			}
			ainfo.playtime += totalPlaytimeSeconds;
			ainfo.rifleKills += stats.rifleKills;
			ainfo.shotgunKills += stats.shotgunKills;
			ainfo.sniperKills += stats.sniperKills;
			ainfo.microgunKills += stats.mgKills;
			ainfo.bazookaKills += stats.bazookaKills;
			ainfo.grenadeKills += stats.grenadeKills;
			ainfo.killstreak = (ainfo.killstreak < stats.probablyKillStreak ? stats.probablyKillStreak : ainfo.killstreak);
			ainfo.deaths += stats.deaths;
			ainfo.headshots += stats.headshots;
			ainfo.experience = stats.newTotalEXP;
			ainfo.microPoints = stats.newTotalMP;
			if (matchEnd == Main::Enums::MATCH_WON) ainfo.wins += 1;
			else if (matchEnd == Main::Enums::MATCH_LOST) ainfo.losses += 1;
			else if (matchEnd == Main::Enums::MATCH_DRAW) ainfo.draws += 1;
			if (isClanMatch)
			{
				if (matchEnd == Main::Enums::MATCH_WON) ainfo.clanWins += 1;
				else if (matchEnd == Main::Enums::MATCH_LOST) ainfo.clanLosses += 1;
				else if (matchEnd == Main::Enums::MATCH_DRAW) ainfo.clanDraws += 1;
				ainfo.clanKills += stats.totalKills;
				ainfo.clanDeaths += stats.deaths;
				ainfo.clanAssists += stats.assists;

				m_scheduler.addRepetitiveCallback(std::source_location::current(), 
					ainfo.accountID, &Main::Persistence::PersistentDatabase::updateClanContribution, ainfo.clanId,
					stats.newTotalClanContribution - ainfo.clanContribution);


				ainfo.clanContribution = stats.newTotalClanContribution;
			}
			if (hasLeveledUp) ainfo.playerLevel += 1;
			m_player.setAccountInfo(ainfo);
			m_scheduler.addRepetitiveCallback(std::source_location::current(), 
				ainfo.accountID, &Main::Persistence::PersistentDatabase::updatePlayerStats, ainfo.accountID, ainfo);

			m_player.storeBatteryObtainedInMatch();
			m_scheduler.addRepetitiveCallback(std::source_location::current(), 
				m_player.getAccountID(), &Main::Persistence::PersistentDatabase::updateBattery, m_player.getAccountID(),
				m_player.getAccountInfo().battery);
		}


		// Trade system
		void Session::temporarilySealAllItems()
		{
PACK_PUSH(1)
			struct SealInfo
			{
				Main::Structures::ItemSerialInfo serialInfo1;
				std::uint32_t unused{};
				std::uint32_t unused2{};
				Main::Structures::ItemSerialInfo serialInfo2;
			};
PACK_POP()

			Common::Network::Packet response;
			response.setTcpHeader(0, Common::Enums::NO_ENCRYPTION);
			response.setOrder(200);
			response.setExtra(1); // seal success
			std::uint32_t unused = 0;
			std::uint32_t index = 0; 

			// Currently causing a client crash in some special circumstances??
			for (const auto& [itemNum, item] : m_player.getItems())
			{ 
				const bool isTradeable = Main::CdbUtils::isTradeable(item.itemId.itemId).value_or(false); // avoid further issues, don't seal it
				if (isTradeable && item.itemId.itemId != 1000000 /* coupons dont work */ && item.expirationDate == 0 /* unlimited */)
				{
					SealInfo msg{.serialInfo1 = item.serialInfo,.unused = 0,.unused2 = 0,.serialInfo2 = item.serialInfo};
					response.setData(reinterpret_cast<const std::uint8_t*>(&msg), sizeof(msg));
					asyncWrite(response);
				}
			}
		}

		void Session::unsealAllItems()
		{
			Common::Network::Packet response;
			response.setTcpHeader(0, Common::Enums::NO_ENCRYPTION);
			response.setOrder(201);
			response.setExtra(1); // unseal success
			std::vector<std::uint8_t> message(8);

			for (const auto& [itemNum, item] : m_player.getItems())
			{
				std::memcpy(message.data(), &item.serialInfo, sizeof(item.serialInfo));
				response.setData(message.data(), message.size());
				asyncWrite(response);
			}
		}

		void Session::lockTrade()
		{
			m_player.lockTrade();
		}

		bool Session::hasPlayerLocked() const
		{
			return m_player.hasPlayerLocked();
		}

		void Session::resetTradeInfo()
		{
			unsealAllItems();
			m_player.resetTradeInfo();
		}

		bool Session::addTradedItem(std::uint32_t itemId, const Main::Structures::ItemSerialInfo& serialInfo)
		{
			return m_player.addTradedItem(itemId, serialInfo);
		}

		void Session::removeTradedItem(const Main::Structures::ItemSerialInfo& serialInfo)
		{
			m_player.removeTradedItem(serialInfo);
		}

		void Session::resetTradedItems()
		{
			m_player.resetTradedItems();
		}

		const std::vector<Main::Structures::TradeBasicItem>& Session::getTradedItems() const
		{
			return m_player.getTradedItems();
		}

		void Session::spawnItems(const std::vector<Main::Structures::TradeBasicItem>& tradeBasicItems, const std::string& action)
		{
			for (const auto& current : tradeBasicItems)
			{
				spawnItem(current.itemId.itemId, current.itemSerialInfo, action);
			}
		}

		bool Session::deleteItems(const std::vector<Main::Structures::TradeBasicItem>& tradeBasicItems, const std::string& action)
		{
			for (const auto& current : tradeBasicItems)
			{
				if (!deleteItem(current.itemSerialInfo, action)) 
					return false;
			}
		}

		void Session::addItems(const std::vector<Main::Structures::TradeBasicItem>& tradedItems)
		{
			auto items = m_player.addItems(tradedItems);
			m_scheduler.addRepetitiveCallback(std::source_location::current(), m_player.getAccountID(), 
				&Main::Persistence::PersistentDatabase::addPlayerItems, m_player.getAccountID(), items, -1);
		}

		void Session::addItemFromTrade(const Main::Structures::TradeBasicItem& tradeItem)
		{
			auto item = m_player.addItemFromTrade(tradeItem);
			m_scheduler.addRepetitiveCallback(std::source_location::current(), m_player.getAccountID(),
				&Main::Persistence::PersistentDatabase::addPlayerItem, item, m_player.getAccountID(), -1);
		}

		void Session::setCurrentlyTradingWithAccountId(std::uint32_t targetAccountId)
		{
			m_player.setCurrentlyTradingWithAccountId(targetAccountId);
		}

		std::uint32_t Session::getCurrentlyTradingWithAccountId() const
		{
			return m_player.getCurrentlyTradingWithAccountId();
		}

		bool Session::spawnItem(std::uint32_t itemId, const Main::Structures::ItemSerialInfo& itemSerialInfo, const std::string& action)
		{
			Common::Network::Packet response;
			response.setTcpHeader(0, Common::Enums::NO_ENCRYPTION);
			response.setCommand(66, 0, 51, 2);
			Main::Structures::SpawnedItem spawnedItem{ itemId };
			spawnedItem.serialInfo = itemSerialInfo;
			spawnedItem.expirationDate = 0;
			spawnedItem.serialInfo.itemNumber = m_player.getLatestItemNumber() + 1;
			response.setData(reinterpret_cast<std::uint8_t*>(&spawnedItem), sizeof(spawnedItem));
			asyncWrite(response);

			if (addItem(spawnedItem))
			{
				Main::Structures::ItemLogInfo log{ itemSerialInfo.itemNumber, itemId, 0, action };
				m_scheduler.addRepetitiveCallback(std::source_location::current(),
					m_player.getAccountID(), &Main::Persistence::PersistentDatabase::insertItemLog, m_player.getAccountID(), log);
				return true;
			}
			else
			{
				sendMessage("[Session::spawnItem] error while spawning item - please report this issue");
				return false;
			}
		}

		bool Session::hasCsdItems()
		{
			return checkEquippedItems(Details::isCsdItem, "is not CSD");
		}

		bool Session::hasBasicItems()
		{
			return checkEquippedItems(Details::isBasicItem, "is not a basic item (Boss Battle requires basic weapons)");
		}

		void Session::respawnBossBattle()
		{
			if (m_totalBossBattleRespawnsLeft) 
			{
				m_packet.setCommand(329, 0, 1, 0);
				m_packet.setData(reinterpret_cast<const std::uint8_t*>(&m_totalBossBattleRespawnsLeft), sizeof(m_totalBossBattleRespawnsLeft));
				asyncWrite(m_packet);
				--m_totalBossBattleRespawnsLeft;
			}
		}
	};
}
