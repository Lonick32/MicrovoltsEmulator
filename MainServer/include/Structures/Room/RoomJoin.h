#ifndef ROOM_JOIN_STRUCTURE_H
#define ROOM_JOIN_STRUCTURE_H

#include <cstdint>
#include "Utils/Constants.h"
#include "Macros.h"
#include <cstring> 

// This structure is sent from server => client when a room is entered
// // MISSING: specific room setting [eli => num of rounds, TDM => num of total kills, etc.] 

namespace Main
{
	namespace Structures
	{
PACK_PUSH(1)
		struct RoomJoin
		{
			std::uint32_t map : 7 = 0;
			std::uint32_t mode : 5 = 0; 
			std::uint32_t maxPlayers : 5 = 0; 
			std::uint32_t hasMatchStarted : 1 = 0;
			std::uint32_t hasPassword : 1 = 0;
			std::uint32_t isOpen : 1 = 0;  
			std::uint32_t weaponRestriction : 4 = 0; 
			std::uint32_t isClanMatch : 1 = 0;  // if 1 then client crashes?!  
			std::uint32_t isTeamBalanceOn : 1 = 0; 
			std::uint32_t isTeamBalanceOn2 : 1 = 0; // ? keep this as 0
			std::uint32_t isObserverOn : 1 = 0;
			std::uint32_t hidePassword : 1 = 0;
			std::uint32_t unknown1 : 1 = 0;
			std::uint32_t unknown2 : 1 = 0;
			std::uint32_t unknown3 : 1 = 0;
			char password[14]{};
			std::uint8_t unknown4{};

			explicit RoomJoin(std::uint32_t map, std::uint32_t mode, std::uint32_t maxPlayers, std::uint32_t hasMatchStarted,
				std::uint32_t hasPassword, std::uint32_t isOpen, std::uint32_t weaponRestriction, std::uint32_t isTeamBalanceOn,
				std::uint32_t isObserverOn, std::uint32_t hidePassword, const std::string& password)
				: map(map),
				mode(mode),
				maxPlayers(maxPlayers),
				hasMatchStarted(hasMatchStarted),
				hasPassword(hasPassword),
				isOpen(isOpen),
				weaponRestriction(weaponRestriction),
				isTeamBalanceOn(isTeamBalanceOn),
				isObserverOn(isObserverOn),
				hidePassword(hidePassword)
			{
				std::memcpy(this->password, password.data(), Common::Constants::maxPassword);
			}
		};
PACK_POP()

PACK_PUSH(1)
		struct RoomInviteFollow
		{
			std::uint16_t serverId{};
			std::uint16_t channelId{};
			char sourceNickname[Common::Constants::maxNicknameSize]{};
			std::uint16_t roomNumber{};
			std::uint16_t unknown{};
			char roomTitle[32]{};
			char password[14]{};
		};
PACK_POP()
	}
}

#endif