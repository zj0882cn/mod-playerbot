#include "BotCommandScript.h"
#include "PlayerBotMgr.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Chat.h"
#include "Log.h"

#define PLAYERBOT_VERSION "v2.1.0.2"

BotCommandScript::BotCommandScript() : CommandScript("BotCommandScript") { }

std::vector<Acore::ChatCommands::ChatCommandBuilder> BotCommandScript::GetCommands() const
{
    static std::vector<Acore::ChatCommands::ChatCommandBuilder> botCommandTable =
    {
        { "bot", HandleBotCommand, SEC_PLAYER, Acore::ChatCommands::Console::Yes },
    };
    return botCommandTable;
}

void AddBotCommandScripts()
{
    new BotCommandScript();
}

bool BotCommandScript::HandleBotCommand(ChatHandler* handler, char const* args)
{
    bool isConsole = !handler->GetSession();

    if (!args)
    {
        if (isConsole)
        {
            printf("[PlayerBot v%s] Usage:\n", PLAYERBOT_VERSION);
            printf("  bot set <playername>     - Mark a player as Bot\n");
            printf("  bot remove <playername>  - Remove a player from Bot list\n");
            printf("  bot list                 - List all Bot players\n");
        }
        else
        {
            handler->PSendSysMessage("|cff00ff00=== PlayerBot Module {} ===|r", PLAYERBOT_VERSION);
            handler->PSendSysMessage("|cffff0000GM Commands:|r");
            handler->PSendSysMessage("|cff00ff/bot set $name|r          - Mark a player as Bot");
            handler->PSendSysMessage("|cff00ff/bot remove $name|r       - Remove a player from Bot list");
            handler->PSendSysMessage("|cff00ff/bot list|r               - List all Bot players");
            handler->PSendSysMessage("|cff00ff/bot clearmaster $name|r  - Clear bot's master");
            handler->PSendSysMessage("|cff00ff/bot master $name|r       - Show bot's master");
            handler->PSendSysMessage("|cffff0000Player Commands:|r");
            handler->PSendSysMessage("|cff00ff/bot stance [defensive|passive|aggressive]|r - Set stance for ALL your bots");
            handler->PSendSysMessage("|cff00ff/bot stance $name [defensive|passive|aggressive]|r - Set stance for one bot");
            handler->PSendSysMessage("|cff00ff00==================================================|r");
            handler->PSendSysMessage("Total bots: |cffff00ff%u|r", sPlayerBotMgr->GetCount());
        }
        return true;
    }

    if (isConsole)
    {
        char* action = strtok(const_cast<char*>(args), " ");
        if (!action)
            return false;

        std::string cmd(action);
        std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

        if (cmd == "set")
        {
            char* playerName = strtok(nullptr, " ");
            if (!playerName)
            {
                printf("[PlayerBot] Usage: bot set <playername>\n");
                return true;
            }

            ObjectGuid guid = sPlayerBotMgr->FindPlayerByName(playerName);
            if (guid.IsEmpty())
            {
                printf("[PlayerBot] Player '%s' not found.\n", playerName);
                return true;
            }

            if (sPlayerBotMgr->IsBot(guid))
            {
                printf("[PlayerBot] Player '%s' is already a Bot.\n", playerName);
                return true;
            }

            sPlayerBotMgr->AddBot(guid);
            printf("[PlayerBot] Player '%s' marked as Bot. Total: %zu\n", 
                playerName, sPlayerBotMgr->GetCount());
            return true;
        }

        if (cmd == "remove")
        {
            char* playerName = strtok(nullptr, " ");
            if (!playerName)
            {
                printf("[PlayerBot] Usage: bot remove <playername>\n");
                return true;
            }

            ObjectGuid guid = sPlayerBotMgr->FindPlayerByName(playerName);
            if (guid.IsEmpty())
            {
                printf("[PlayerBot] Player '%s' not found.\n", playerName);
                return true;
            }

            if (!sPlayerBotMgr->IsBot(guid))
            {
                printf("[PlayerBot] Player '%s' is not a Bot.\n", playerName);
                return true;
            }

            sPlayerBotMgr->RemoveBot(guid);
            printf("[PlayerBot] Player '%s' removed from Bot list. Total: %zu\n", 
                playerName, sPlayerBotMgr->GetCount());
            return true;
        }

        if (cmd == "list")
        {
            auto bots = sPlayerBotMgr->GetAllBots();
            printf("[PlayerBot] === Bot Players List (%zu total) ===\n", bots.size());
            uint32 index = 1;
            for (auto const& guid : bots)
            {
                Player* player = ObjectAccessor::FindConnectedPlayer(guid);
                if (player)
                    printf("  %u. %s - Online\n", index++, player->GetName().c_str());
                else
                    printf("  %u. %s - Offline\n", index++, guid.ToString().c_str());
            }
            return true;
        }

        printf("[PlayerBot] Unknown command. Available: set, remove, list\n");
        return true;
    }

    if (!handler->GetSession())
        return true;

    Player* currentPlayer = handler->GetSession()->GetPlayer();
    if (!currentPlayer)
        return true;

    bool isGM3 = handler->GetSession()->GetSecurity() >= SEC_GAMEMASTER;

    char* action = strtok(const_cast<char*>(args), " ");
    if (!action)
        return false;

    std::string cmd(action);
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

    if (cmd == "set")
    {
        if (!isGM3)
        {
            handler->PSendSysMessage("|cffff0000[Bot]|r Requires GM3 permission.");
            return true;
        }

        char* playerName = strtok(nullptr, " ");
        if (!playerName)
        {
            handler->PSendSysMessage("|cffff0000Usage: /bot set $name|r");
            return true;
        }

        ObjectGuid guid = sPlayerBotMgr->FindPlayerByName(playerName);
        if (guid.IsEmpty())
        {
            handler->PSendSysMessage("|cffff0000Player '{}' not found.|r", playerName);
            return true;
        }

        if (sPlayerBotMgr->IsBot(guid))
        {
            handler->PSendSysMessage("|cffff0000Player '{}' is already a Bot.|r", playerName);
            return true;
        }

        sPlayerBotMgr->AddBot(guid);
        handler->PSendSysMessage("|cff00ff00Player '{}' marked as Bot.|r", playerName);
        handler->PSendSysMessage("Total bots: |cffff00ff%u|r", sPlayerBotMgr->GetCount());
        return true;
    }

    if (cmd == "remove")
    {
        if (!isGM3)
        {
            handler->PSendSysMessage("|cffff0000[Bot]|r Requires GM3 permission.");
            return true;
        }

        char* playerName = strtok(nullptr, " ");
        if (!playerName)
        {
            handler->PSendSysMessage("|cffff0000Usage: /bot remove $name|r");
            return true;
        }

        ObjectGuid guid = sPlayerBotMgr->FindPlayerByName(playerName);
        if (guid.IsEmpty())
        {
            handler->PSendSysMessage("|cffff0000Player '{}' not found.|r", playerName);
            return true;
        }

        if (!sPlayerBotMgr->IsBot(guid))
        {
            handler->PSendSysMessage("|cffff0000Player '{}' is not a Bot.|r", playerName);
            return true;
        }

        sPlayerBotMgr->RemoveBot(guid);
        handler->PSendSysMessage("|cff00ff00Player '{}' removed.|r", playerName);
        handler->PSendSysMessage("Total bots: |cffff00ff%u|r", sPlayerBotMgr->GetCount());
        return true;
    }

    if (cmd == "list")
    {
        if (!isGM3)
        {
            handler->PSendSysMessage("|cffff0000[Bot]|r Requires GM3 permission.");
            return true;
        }

        // Machine-readable output for the BotCommander client addon.
        // Format (pipe-separated, no color codes):
        //   BOTLIST|<total>
        //   BOT|<guid>|<name>|<online 1/0>|<master|NONE>|<stance>
        auto bots = sPlayerBotMgr->GetAllBots();
        handler->PSendSysMessage("BOTLIST|{}", bots.size());
        for (auto const& guid : bots)
        {
            Player* player = ObjectAccessor::FindConnectedPlayer(guid);
            ObjectGuid masterGuid = sPlayerBotMgr->GetMaster(guid);
            Player* master = !masterGuid.IsEmpty() ? ObjectAccessor::FindConnectedPlayer(masterGuid) : nullptr;

            const char* stanceName = "defensive";
            switch (sPlayerBotMgr->GetBotStance(guid))
            {
                case PlayerBotMgr::STANCE_PASSIVE:    stanceName = "passive"; break;
                case PlayerBotMgr::STANCE_AGGRESSIVE: stanceName = "aggressive"; break;
                default: break;
            }

            handler->PSendSysMessage("BOT|{}|{}|{}|{}|{}",
                guid.ToString(),
                player ? player->GetName() : guid.ToString(),
                player ? "1" : "0",
                master ? master->GetName() : "NONE",
                stanceName);
        }
        return true;
    }

    if (cmd == "stance")
    {
        char* arg1 = strtok(nullptr, " ");
        if (!arg1)
        {
            handler->PSendSysMessage("|cffff0000Usage: /bot stance [defensive|passive|aggressive]|r  - set all your bots");
            handler->PSendSysMessage("|cffff0000Usage: /bot stance $name [defensive|passive|aggressive]|r  - set one bot");
            handler->PSendSysMessage("|cff00ff00Short: d, p, a|r");
            return true;
        }

        char* arg2 = strtok(nullptr, " ");

        auto parseStance = [](std::string const& s) -> PlayerBotMgr::BotStance
        {
            if (s == "passive" || s == "p")
                return PlayerBotMgr::STANCE_PASSIVE;
            if (s == "aggressive" || s == "a")
                return PlayerBotMgr::STANCE_AGGRESSIVE;
            return PlayerBotMgr::STANCE_DEFENSIVE;
        };

        const char* stanceNames[] = { "Defensive", "Passive", "Aggressive" };

        std::string arg1Lower(arg1);
        std::transform(arg1Lower.begin(), arg1Lower.end(), arg1Lower.begin(), ::tolower);

        bool arg1IsStance = (arg1Lower == "defensive" || arg1Lower == "passive" || arg1Lower == "aggressive" ||
                             arg1Lower == "d" || arg1Lower == "p" || arg1Lower == "a");

        // No bot name given -> apply to all bots owned by the executor.
        if (arg1IsStance && !arg2)
        {
            PlayerBotMgr::BotStance newStance = parseStance(arg1Lower);
            uint32 count = 0;
            for (auto const& botGuid : sPlayerBotMgr->GetAllBots())
            {
                if (sPlayerBotMgr->GetMaster(botGuid) == currentPlayer->GetGUID())
                {
                    sPlayerBotMgr->SetBotStance(botGuid, newStance);
                    count++;
                }
            }
            handler->PSendSysMessage("|cff00ff00Stance set to |cffff00ff{}|r for |cffff00ff{}|r bot(s).",
                stanceNames[newStance], count);
            return true;
        }

        // Named bot: /bot stance <name> <stance>
        if (!arg2)
        {
            handler->PSendSysMessage("|cffff0000Usage: /bot stance $name [defensive|passive|aggressive]|r");
            handler->PSendSysMessage("|cff00ff00Short: d, p, a|r");
            return true;
        }

        char* playerName = arg1;
        char* stanceName = arg2;

        ObjectGuid guid = sPlayerBotMgr->FindPlayerByName(playerName);
        if (guid.IsEmpty())
        {
            handler->PSendSysMessage("|cffff0000Player '{}' not found.|r", playerName);
            return true;
        }

        if (!sPlayerBotMgr->IsBot(guid))
        {
            handler->PSendSysMessage("|cffff0000Player '{}' is not a Bot.|r", playerName);
            return true;
        }

        ObjectGuid masterGuid = sPlayerBotMgr->GetMaster(guid);
        if (!isGM3 && masterGuid != currentPlayer->GetGUID())
        {
            handler->PSendSysMessage("|cffff0000Only the Bot's Master can change its stance!|r");
            Player* master = ObjectAccessor::FindConnectedPlayer(masterGuid);
            if (master)
                handler->PSendSysMessage("|cffffff00Master: |cffff00ff%s|r", master->GetName().c_str());
            return true;
        }

        std::string stanceLower(stanceName);
        std::transform(stanceLower.begin(), stanceLower.end(), stanceLower.begin(), ::tolower);
        PlayerBotMgr::BotStance newStance = parseStance(stanceLower);

        sPlayerBotMgr->SetBotStance(guid, newStance);

        handler->PSendSysMessage("|cff00ff00Bot '{}' stance set to |cffff00ff{}|r.", 
            playerName, stanceNames[newStance]);

        Player* bot = ObjectAccessor::FindConnectedPlayer(guid);
        if (bot)
        {
            ChatHandler h(bot->GetSession());
            h.PSendSysMessage("|cff00ff00[Bot]|r Stance changed to |cffff00ff{}|r.", 
                stanceNames[newStance]);
        }
        return true;
    }

    if (cmd == "clearmaster")
    {
        if (!isGM3)
        {
            handler->PSendSysMessage("|cffff0000[Bot]|r Requires GM3 permission.");
            return true;
        }

        char* playerName = strtok(nullptr, " ");
        if (!playerName)
        {
            handler->PSendSysMessage("|cffff0000Usage: /bot clearmaster $name|r");
            return true;
        }

        ObjectGuid guid = sPlayerBotMgr->FindPlayerByName(playerName);
        if (guid.IsEmpty())
        {
            handler->PSendSysMessage("|cffff0000Player '{}' not found.|r", playerName);
            return true;
        }

        if (!sPlayerBotMgr->IsBot(guid))
        {
            handler->PSendSysMessage("|cffff0000Player '{}' is not a Bot.|r", playerName);
            return true;
        }

        sPlayerBotMgr->ClearMaster(guid);
        handler->PSendSysMessage("|cff00ff00Master cleared for bot '{}'.|r", playerName);
        return true;
    }

    if (cmd == "master")
    {
        if (!isGM3)
        {
            handler->PSendSysMessage("|cffff0000[Bot]|r Requires GM3 permission.");
            return true;
        }

        char* playerName = strtok(nullptr, " ");
        if (!playerName)
        {
            handler->PSendSysMessage("|cffff0000Usage: /bot master $name|r");
            return true;
        }

        ObjectGuid guid = sPlayerBotMgr->FindPlayerByName(playerName);
        if (guid.IsEmpty())
        {
            handler->PSendSysMessage("|cffff0000Player '{}' not found.|r", playerName);
            return true;
        }

        if (!sPlayerBotMgr->IsBot(guid))
        {
            handler->PSendSysMessage("|cffff0000Player '{}' is not a Bot.|r", playerName);
            return true;
        }

        ObjectGuid masterGuid = sPlayerBotMgr->GetMaster(guid);
        if (!masterGuid.IsEmpty())
        {
            Player* master = ObjectAccessor::FindConnectedPlayer(masterGuid);
            handler->PSendSysMessage("|cff00ff00Bot '{}' Master: |cffff00ff{}|r", 
                playerName, master ? master->GetName().c_str() : "Offline");
        }
        else
        {
            handler->PSendSysMessage("|cffffff00Bot '{}' has no Master.|r", playerName);
        }
        return true;
    }

    handler->PSendSysMessage("|cffff0000Unknown bot command: '{}'|r", action);
    return true;
}
