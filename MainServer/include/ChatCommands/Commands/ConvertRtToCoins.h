#ifndef CONVERT_RT_TO_COINS
#define CONVERT_RT_TO_COINS


#include "../ICommand.h"
#include "../ChatCommands.h"
#include "../../MainServer.h"

namespace Main
{
    namespace Command
    {
        class Rt2Coins final : public ICommand
        {
        private:
            std::uint32_t m_rtToConvert{};

            bool parseCommand(const std::string& providedCommand) override
            {
                std::smatch match;
                if (std::regex_match(providedCommand, match, m_pattern))
                {
                    const std::string& matched_str = match[1].str();
                    const std::from_chars_result result = std::from_chars(matched_str.data(), matched_str.data() + matched_str.size(), m_rtToConvert);
                    if (result.ec == std::errc())
                    {
                        return true;
                    }
                }
                return false;
            }

        public:
            explicit Rt2Coins(const Common::Enums::PlayerGrade requiredGrade)
                : ICommand{ requiredGrade, "/rt2coins <total rocktokens> (1 coin = 3000 RT)", R"(^\S+\s(\d+)$)" }
            {
            }

            void execute(const std::string& command, std::shared_ptr<Main::Network::Session> session, MN::SessionsManager& sessionsManager,
                MC::RoomsManager& roomsManager, MP::MainScheduler&, std::uint32_t roomNumber,
                Main::MainServer& mainSv) override
            {
                if (!parseCommand(command))
                {
                    session->sendMessage("parsing error");
                    return;
                }

                auto& accountInfo = session->getAccountInfo();

                constexpr std::uint32_t coinCost = 3'000;
                constexpr std::uint32_t maxCoins = 100;

                if (accountInfo.rockTotens < coinCost)
                {
                    session->sendMessage("Error: you must have at least 3'000 RockTokens");
                    return;
                }
                if (m_rtToConvert > accountInfo.rockTotens)
                {
                    session->sendMessage("Error: you must specify an amount of RT that you currently have!");
                    return;
                }

                const std::uint32_t currentCoins = accountInfo.coins;
                if (currentCoins >= maxCoins)
                {
                    session->sendMessage("Error: You already have the maximum coins possible.");
                    return;
                }

                std::uint32_t coinsToSpawn = m_rtToConvert / coinCost;
                if (coinsToSpawn == 0)
                {
                    session->sendMessage("Error: you must convert at least 3'000 RockTokens (1 coin).");
                    return;
                }

                if (currentCoins + coinsToSpawn > maxCoins)
                {
                    coinsToSpawn = maxCoins - currentCoins;
                    session->sendMessage("Note: You can only hold " + std::to_string(maxCoins) + " coins. Conversion limited to " + std::to_string(coinsToSpawn) + ".");
                }

                const std::uint32_t rtSpent = coinsToSpawn * coinCost;
                if (session->setAccountRockTotens(accountInfo.rockTotens - rtSpent) && session->setAccountCoins(accountInfo.coins + coinsToSpawn))
                {
                    session->sendCurrency();
                    session->sendMessage("Success: converted " + std::to_string(rtSpent) + " RT into " + std::to_string(coinsToSpawn) + " coin(s).");
                }
                else
                {
                    session->sendMessage("Error while converting RT to Coins, please report this issue through a ticket.");
                }
            }
        };

        REGISTER_CMD(Rt2Coins, Common::Enums::PlayerGrade::GRADE_NORMAL)
    }
}

#endif


