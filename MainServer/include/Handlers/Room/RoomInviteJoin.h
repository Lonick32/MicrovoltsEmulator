#ifndef ROOM_INVITE_JOIN_HANDLER_H
#define ROOM_INVITE_JOIN_HANDLER_H

#include "../../Network/MainSession.h"
#include "../../../include/Structures/AccountInfo/MainAccountInfo.h"
#include "../../../include/Structures/Room/RoomSettingsUpdate.h"
#include "Network/Packet.h"
#include <span>
#include "../Room/RoomJoinHandler.h"
#include "../Room/RoomLeaveHandler.h"
#include "../../Detail/Utilities.h"
#include "../../Structures/ClientData/Structures.h"


namespace Main
{
	namespace Handlers
	{
		enum RoomInviteJoinExtra
		{
			USER_INVITING_FRIEND = 0,
			USER_READY = 5,
			USER_OFFLINE = 0xD,
			ROOM_FULL = 0xE,
			ROOM_SENDINVITE_TOTARGET = 44,
			USER_JOINING_FRIEND = 28,
		};

		inline void handleJoinAndInvites(const Common::Network::Packet& request, std::shared_ptr<Main::Network::Session> session, Main::Classes::RoomsManager& roomsManager,
			Main::Network::SessionsManager& sessionsManager, std::uint32_t serverId)
		{
			SEND_DEBUG_MESSAGE("[handleJoinAndInvites] Extra: " + std::to_string((uint32_t)request.getExtra())
				+ ", Mission: " + std::to_string((uint32_t)request.getMission())
				+ ", Option: " + std::to_string((uint32_t)request.getOption()), session);

			
			auto response = request;
			const auto& selfPlayer = session->getPlayer();

			if (request.getExtra() == RoomInviteJoinExtra::USER_JOINING_FRIEND)
			{
				const std::uint32_t targetAccountId = Main::Details::parseData<std::uint32_t>(request); 
				if (auto targetSession = sessionsManager.getSessionByAccountId(targetAccountId))
				{
					const auto& targetPlayer = targetSession->getPlayer();
					const std::uint32_t targetRoomNum = targetPlayer.getRoomNumber();
					const std::uint32_t selfRoomNum = selfPlayer.getRoomNumber();
					if (targetRoomNum)
					{
						if (!selfPlayer.isInLobby())
						{ 
							session->sendMessage("You can only join a friend's room while being in the lobby!");
							return;
						}
						else if (targetRoomNum == selfRoomNum)
						{ // User attempting to join the same room, disallow this
							session->sendMessage("You cannot join this friend as you're already in their room.");
							return;
						}
						else if (targetRoomNum >= Common::Constants::clanRoomNumberStart)
						{
							session->sendMessage("The target user is in a clan room");
							return;
						}

						Main::ClientData::RoomInfo joinInfo{ targetRoomNum - 1 };
						response.setCommand(140, 0, 0, 0);
						response.setData(reinterpret_cast<std::uint8_t*>(&joinInfo), sizeof(joinInfo));

						if (Main::Classes::Room* room = roomsManager.getRoomByNumber(targetRoomNum))
						{
							Main::Handlers::handleRoomJoin(response, session, roomsManager, !room->getPassword().empty(), joinInfo);
						}
						else
						{
							session->sendMessage("Room not found.");
						}
					}
					else
					{
						session->sendMessage("This friend is currently not inside a room.");
					}
				}
				else
				{
					session->sendMessage("The friend you're trying to join went offline.");
				}
			}
			else if (request.getExtra() == RoomInviteJoinExtra::USER_INVITING_FRIEND && request.getOption() == 2) 
			{
				char targetNickname[Common::Constants::maxNicknameSize];
				std::ranges::copy(std::span(reinterpret_cast<const char*>(request.getData()), sizeof(targetNickname)), targetNickname);

				auto targetSession = sessionsManager.findSessionByName(targetNickname);
				if (targetSession)
				{
					if (session->getPlayer().getRoomNumber() == 0 || session->getPlayer().getRoomNumber() >= Common::Constants::clanRoomNumberStart)
					{
						session->sendMessage("You must be in a normal room to invite someone!");
						return;
					}
					const auto& targetPlayer = targetSession->getPlayer();
					if (!targetPlayer.isInLobby())
					{
						session->sendMessage("This player is currently not in the lobby.");
						return;
					}
					else if (targetPlayer.getRoomNumber() == selfPlayer.getRoomNumber())
					{
						session->sendMessage("The player you are trying to invite is already in your room.");
						return;
					}
					else if (targetPlayer.hasBlocked(session->getAccountInfo().accountID))
					{
						session->sendMessage("The player you are trying to invite has blocked you.");
						return;
					}
					if (Main::Classes::Room* room = roomsManager.getRoomByNumber(selfPlayer.getRoomNumber()))
					{
						response.setCommand(319, 0, 0, 0); 
						response.setData(nullptr, 0);

						Main::Structures::RoomInviteFollow roomFollow; 
						roomFollow.channelId = 1; // if 0 then beginner channel?
						roomFollow.serverId = serverId; // This is correct (checked)
						roomFollow.unknown = 2; // Apparently can be anything except for 0?
						roomFollow.roomNumber = selfPlayer.getRoomNumber() - 1;
						std::ranges::copy(std::span(room->getRoomTitle().c_str(), sizeof(roomFollow.roomTitle) - 1), roomFollow.roomTitle);
						roomFollow.roomTitle[sizeof(roomFollow.roomTitle) - 1] = '\0';
						std::ranges::copy(std::span(session->getAccountInfo().nickname, sizeof(roomFollow.sourceNickname) - 1), roomFollow.sourceNickname);
						roomFollow.sourceNickname[sizeof(roomFollow.sourceNickname) - 1] = '\0';
						if (room->getRoomSettings().hasPassword)
						{
							const std::string& pwd = room->getPassword();
							std::size_t copyLength = std::min(pwd.size(), sizeof(roomFollow.password) - 1);
							std::copy_n(pwd.data(), copyLength, roomFollow.password);
							roomFollow.password[copyLength] = '\0';
						}
						response.setData(reinterpret_cast<std::uint8_t*>(&roomFollow), sizeof(roomFollow));
						targetSession->asyncWrite(response);
					}
					else
					{
						session->sendMessage("You must be in a room to invite a player.");
					}
				}
				else
				{
					session->sendMessage("The player you invited is offline.");
				}
			}
		}
	}
}

#endif
