#include "BotCommandScript.h"
#include "BotCommon.h"
#include "BotPlayerScript.h"
#include "PlayerBotMgr.h"
#include "ObjectAccessor.h"
#include "Language.h"
#include "Player.h"
#include "Chat.h"
#include "Log.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "World.h"
#include "Item.h"
#include "ItemTemplate.h"

// Console usage：完全对齐 AZ 核心命令输出（acore_string 195/8/191/192）
//   ### USAGE: .bot ...
//   Possible subcommands:
//   |- bot attack ...
//   |- bot list
//   ...
static void BotPrintConsoleUsage(ChatHandler* handler)
{
    handler->PSendSysMessage(LANG_CMD_HELP_GENERIC, "bot");
    handler->SendSysMessage(LANG_SUBCMDS_LIST);
    handler->PSendSysMessage(LANG_SUBCMDS_LIST_ENTRY, "bot attack ...");
    handler->PSendSysMessage(LANG_SUBCMDS_LIST_ENTRY, "bot autospell ...");
    handler->PSendSysMessage(LANG_SUBCMDS_LIST_ENTRY, "bot clearmaster <name>");
    handler->PSendSysMessage(LANG_SUBCMDS_LIST_ENTRY, "bot follow ...");
    handler->PSendSysMessage(LANG_SUBCMDS_LIST_ENTRY, "bot list");
    handler->PSendSysMessage(LANG_SUBCMDS_LIST_ENTRY, "bot master <name>");
    handler->PSendSysMessage(LANG_SUBCMDS_LIST_ENTRY, "bot remove <name>");
    handler->PSendSysMessage(LANG_SUBCMDS_LIST_ENTRY, "bot return ...");
    handler->PSendSysMessage(LANG_SUBCMDS_LIST_ENTRY, "bot set <name>");
    handler->PSendSysMessage(LANG_SUBCMDS_LIST_ENTRY, "bot skill ...");
    handler->PSendSysMessage(LANG_SUBCMDS_LIST_ENTRY, "bot spell ...");
    handler->PSendSysMessage(LANG_SUBCMDS_LIST_ENTRY, "bot stance ...");
    handler->PSendSysMessage(LANG_SUBCMDS_LIST_ENTRY, "bot stay ...");
}

#define PLAYERBOT_VERSION "v2.2.0"
// Git commit this source corresponds to. Stored in PlayerbotVersion.h, which
// mod-playerbot.cmake rewrites at (re)configure time (git rev-parse --short
// HEAD). Keeping it in a header means ZIP deployments (no .git) still carry
// the version in the source tree and /bot shows the exact commit built from.
#include "PlayerbotVersion.h"
#ifndef PLAYERBOT_REV
#define PLAYERBOT_REV "unknown"
#endif
// Compile date/time (auto-embedded at build). Shown in the /bot help so the
// master can verify the running worldserver was built from the latest code.
#define PLAYERBOT_BUILD __DATE__ " " __TIME__

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
    PB_LOG(1, "Command '/bot' from {}: '{}'",
        isConsole ? "console"
                  : (handler->GetSession() && handler->GetSession()->GetPlayer()
                      ? handler->GetSession()->GetPlayer()->GetName() : "player"),
        args ? args : "");

    if (!args || !*args)
    {
        if (isConsole)
        {
            BotPrintConsoleUsage(handler);
        }
        else
        {
            BotSendSysMessage(handler, "|cff00ff00=== PlayerBot Module {} (rev {}, built {}) ===|r", PLAYERBOT_VERSION, PLAYERBOT_REV, PLAYERBOT_BUILD);
            BotSendSysMessage(handler, "|cffff0000GM Commands:|r");
            BotSendSysMessage(handler, "|cff00ff/bot set $name|r             - Mark a player as Bot");
            BotSendSysMessage(handler, "|cff00ff/bot remove $name|r          - Remove a player from Bot list");
            BotSendSysMessage(handler, "|cff00ff/bot list|r                  - List bots (yours or all)");
            BotSendSysMessage(handler, "|cff00ff/bot master $name|r          - Show bot's master");
            BotSendSysMessage(handler, "|cff00ff/bot clearmaster $name|r     - Clear bot's master");
            BotSendSysMessage(handler, "|cffff0000Pet Commands (master, [$name]=one bot, omit=all):|r");
            BotSendSysMessage(handler, "|cff00ff/bot attack [$name]|r         - Attack master's target");
            BotSendSysMessage(handler, "|cff00ff/bot follow [$name]|r         - Follow master");
            BotSendSysMessage(handler, "|cff00ff/bot stay [$name]|r           - Stay at current position");
            BotSendSysMessage(handler, "|cff00ff/bot return [$name]|r         - Teleport back to master");
            BotSendSysMessage(handler, "|cff00ff/bot stance [$name] [d|p|a]|r - Set stance (d=def,p=passive,a=agg)");
            BotSendSysMessage(handler, "|cff00ff/bot spell [$name] <1-12>|r    - Cast a skill slot");
            BotSendSysMessage(handler, "|cff00ff/bot autospell [$name] <1-12>|r- Toggle slot autocast");
            BotSendSysMessage(handler, "|cff00ff/bot skill [$name] pool|info|r - View bot skills");
            BotSendSysMessage(handler, "|cff00ff00==================================================|r");
            BotSendSysMessage(handler, "Total bots: |cffff00ff{}|r", sPlayerBotMgr->GetCount());
        }
        return true;
    }

    if (isConsole)
    {
        char* action = strtok(const_cast<char*>(args), " ");
        if (!action)
        {
            // Console with only whitespace: show the console usage instead of
            // returning false (returning false makes the core print
            // "There is no detailed usage information associated with 'bot'").
            BotPrintConsoleUsage(handler);
            return true;
        }

        std::string cmd(action);
        std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

        if (cmd == "set")
        {
            char* playerName = strtok(nullptr, " ");
            if (!playerName)
            {
                handler->PSendSysMessage(LANG_CMD_SYNTAX);
                return false;
            }

            ObjectGuid guid = sPlayerBotMgr->FindPlayerByName(playerName);
            if (guid.IsEmpty())
            {
                handler->PSendSysMessage("Player \"{}\" not found.", playerName);
                return false;
            }

            if (sPlayerBotMgr->IsBot(guid))
            {
                handler->PSendSysMessage("Player \"{}\" is already a Bot.", playerName);
                return true;
            }

            sPlayerBotMgr->AddBot(guid);
            PB_LOG(1, "Console 'bot set': '{}' marked as bot ({})", playerName, guid.ToString());
            handler->PSendSysMessage("Player \"{}\" marked as Bot ({} total).", playerName, sPlayerBotMgr->GetCount());
            return true;
        }

        if (cmd == "remove")
        {
            char* playerName = strtok(nullptr, " ");
            if (!playerName)
            {
                handler->PSendSysMessage(LANG_CMD_SYNTAX);
                return false;
            }

            ObjectGuid guid = sPlayerBotMgr->FindPlayerByName(playerName);
            if (guid.IsEmpty())
            {
                handler->PSendSysMessage("Player \"{}\" not found.", playerName);
                return false;
            }

            if (!sPlayerBotMgr->IsBot(guid))
            {
                handler->PSendSysMessage("Player \"{}\" is not a Bot.", playerName);
                return false;
            }

            sPlayerBotMgr->RemoveBot(guid);
            PB_LOG(1, "Console 'bot remove': '{}' removed from bots ({})", playerName, guid.ToString());
            handler->PSendSysMessage("Player \"{}\" removed from Bot list ({} total).", playerName, sPlayerBotMgr->GetCount());
            return true;
        }

        if (cmd == "list")
        {
            auto bots = sPlayerBotMgr->GetAllBots();
            // 对齐 AZ 表格输出 (acore_string onlinelist 格式: -前后缀 + [字段]固定宽)
            BotSendSysMessage(handler, "-================ PlayerBot 在线列表 ({}) ================-", bots.size());
            BotSendSysMessage(handler, "-[{:<14}][{:<4}][{:<14}][{:<12}]-", "角色名", "在线", "主人", "姿态");
            BotSendSysMessage(handler, "-==================================================================-");
            for (auto const& guid : bots)
            {
                Player* player = ObjectAccessor::FindConnectedPlayer(guid);
                const char* stanceName = "defensive";
                switch (sPlayerBotMgr->GetBotStance(guid))
                {
                    case PlayerBotMgr::STANCE_PASSIVE:    stanceName = "passive"; break;
                    case PlayerBotMgr::STANCE_AGGRESSIVE: stanceName = "aggressive"; break;
                    default: break;
                }
                BotSendSysMessage(handler, "-[{:<14}][{:<4}][{:<14}][{:<12}]-",
                    sPlayerBotMgr->GetCharacterName(guid),
                    player ? "1" : "0",
                    sPlayerBotMgr->GetMasterName(guid).c_str(),
                    stanceName);
            }
            BotSendSysMessage(handler, "-==================================================================-");
            return true;
        }

        handler->PSendSysMessage("Unknown subcommand '{}'.", cmd);
        return false;
    }

    if (!handler->GetSession())
        return true;

    Player* currentPlayer = handler->GetSession()->GetPlayer();
    if (!currentPlayer)
        return true;

    bool isGM3 = handler->GetSession()->GetSecurity() >= SEC_GAMEMASTER;

    char* action = strtok(const_cast<char*>(args), " ");
    if (!action)
    {
        // args was empty/whitespace: show usage instead of letting the core
        // print "no detailed usage" (which happens when we return false here).
        BotSendSysMessage(handler, "|cff00ff00=== PlayerBot Module {} (rev {}, built {}) ===|r", PLAYERBOT_VERSION, PLAYERBOT_REV, PLAYERBOT_BUILD);
        BotSendSysMessage(handler, "|cffff0000GM Commands:|r");
        BotSendSysMessage(handler, "|cff00ff00/bot set $name|r             - Mark a player as Bot");
        BotSendSysMessage(handler, "|cff00ff00/bot remove $name|r          - Remove a player from Bot list");
        BotSendSysMessage(handler, "|cff00ff00/bot list|r                  - List bots (yours or all)");
        BotSendSysMessage(handler, "|cff00ff00/bot master $name|r          - Show bot's master");
        BotSendSysMessage(handler, "|cff00ff00/bot clearmaster $name|r     - Clear bot's master");
        BotSendSysMessage(handler, "|cffff0000Player Commands:|r");
        BotSendSysMessage(handler, "|cff00ff00/bot attack [$name]|r        - Attack master's target");
        BotSendSysMessage(handler, "|cff00ff00/bot follow [$name]|r        - Follow master");
        BotSendSysMessage(handler, "|cff00ff00/bot stay [$name]|r          - Stay at current position");
        BotSendSysMessage(handler, "|cff00ff00/bot return [$name]|r        - Teleport back to master");
        BotSendSysMessage(handler, "|cff00ff00/bot stance [$name] [d|p|a]|r - Set stance (d=def,p=passive,a=agg)");
        BotSendSysMessage(handler, "|cff00ff00/bot spell [$name] <1-12>|r   - Cast a skill slot");
        BotSendSysMessage(handler, "|cff00ff00/bot autospell [$name] <1-12>|r- Toggle slot autocast");
        BotSendSysMessage(handler, "|cff00ff00/bot skill [$name] pool|info|r - View bot skills");
        BotSendSysMessage(handler, "|cff00ff00==================================================|r");
        BotSendSysMessage(handler, "Total bots: |cffff00ff{}|r", sPlayerBotMgr->GetCount());
        return true;
    }

    std::string cmd(action);
    std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);

    // ---- Pet-style commands (only for the bot's master) ----
    // Optional single-bot target: /bot <cmd> [$botName]  (no name = all your bots)
    if (cmd == "follow" || cmd == "stay" || cmd == "attack" || cmd == "return")
    {
        PlayerBotMgr::BotCommand newCmd;
        if (cmd == "follow")
            newCmd = PlayerBotMgr::BOT_COMMAND_FOLLOW;
        else if (cmd == "stay")
            newCmd = PlayerBotMgr::BOT_COMMAND_STAY;
        else
            newCmd = PlayerBotMgr::BOT_COMMAND_ATTACK;

        // Optional single-bot target.
        char* nameArg = strtok(nullptr, " ");
        std::set<ObjectGuid> targets;
        if (nameArg)
        {
            ObjectGuid tg = sPlayerBotMgr->FindPlayerByName(nameArg);
            if (tg.IsEmpty())
            {
                BotSendSysMessage(handler, "|cffff0000Bot '{}' not found.|r", nameArg);
                return true;
            }
            if (!sPlayerBotMgr->IsMasterOf(currentPlayer->GetGUID(), tg))
            {
                BotSendSysMessage(handler, "|cffff0000You don't own bot '{}'.|r", nameArg);
                return true;
            }
            targets.insert(tg);
        }
        else
            targets = sPlayerBotMgr->GetBotsByMaster(currentPlayer->GetGUID());

        uint32 count = 0;
        for (auto const& botGuid : targets)
        {
            Player* bot = ObjectAccessor::FindConnectedPlayer(botGuid);
            if (!bot || !bot->IsInWorld())
                continue;

            if (cmd == "attack")
            {
                // Pet-style attack: immediately abandon whatever the bot was
                // doing and move/attack the ordered target.
                Unit* masterTarget = currentPlayer->GetVictim();
                if (!masterTarget)
                {
                    ObjectGuid targetGuid = currentPlayer->GetTarget();
                    if (!targetGuid.IsEmpty())
                        masterTarget = ObjectAccessor::GetUnit(*currentPlayer, targetGuid);
                }
                if (!masterTarget || !masterTarget->IsAlive() ||
                    !bot->IsValidAttackTarget(masterTarget))
                {
                    BotSendSysMessage(handler, "|cffff0000No valid attack target.|r");
                    return true;
                }
                sPlayerBotMgr->SetBotAttackTarget(botGuid, masterTarget->GetGUID());
                sPlayerBotMgr->SetBotCommand(botGuid, newCmd);

                // Immediate execution mirroring pet AttackStart: stop current
                // action and attack/chase the target (handles melee/ranged).
                BotPlayerScript::ExecuteBotAttack(bot, masterTarget);
                BotSendSysMessage(handler, "|cff00ff00{}: attack.|r", bot->GetName());
            }
            else if (cmd == "return")
            {
                // Teleport back to the master's side, then follow.
                sPlayerBotMgr->ClearBotCommand(botGuid);
                sPlayerBotMgr->SetBotAttackTarget(botGuid, ObjectGuid::Empty);
                bot->InterruptNonMeleeSpells(false);
                bot->GetMotionMaster()->Clear();
                bot->TeleportTo(currentPlayer->GetMapId(),
                    currentPlayer->GetPositionX(), currentPlayer->GetPositionY(),
                    currentPlayer->GetPositionZ(), currentPlayer->GetOrientation());
                BotSendSysMessage(handler, "|cff00ff00{}: return.|r", bot->GetName());
                count++;
                continue;
            }
            else if (cmd == "follow")
            {
                // Immediately stop combat and follow the master (spread out
                // across an arc like a pet, instead of stacking on one spot).
                sPlayerBotMgr->SetBotCommand(botGuid, newCmd);
                sPlayerBotMgr->SetBotAttackTarget(botGuid, ObjectGuid::Empty);
                sPlayerBotMgr->ClearBotStayPosition(botGuid);
                sPlayerBotMgr->SetBotReturnMode(botGuid, false); // follow when idle
                BotPlayerScript::ExecuteBotFollow(bot, currentPlayer);
                BotSendSysMessage(handler, "|cff00ff00{}: follow.|r", bot->GetName());
            }
            else // stay
            {
                // Immediately stop everything and remember the stay position.
                sPlayerBotMgr->SetBotCommand(botGuid, newCmd);
                sPlayerBotMgr->SetBotAttackTarget(botGuid, ObjectGuid::Empty);
                sPlayerBotMgr->SetBotStayPosition(botGuid,
                    bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ());
                sPlayerBotMgr->SetBotReturnMode(botGuid, true); // hold position when idle
                if (bot->IsInCombat())
                    bot->CombatStop(true);
                bot->InterruptNonMeleeSpells(false);
                bot->GetMotionMaster()->Clear();
                bot->GetMotionMaster()->MoveIdle();
                BotSendSysMessage(handler, "|cff00ff00{}: stay.|r", bot->GetName());
            }
            count++;
        }

        BotSendSysMessage(handler, "|cff00ff00{}: {} bot(s).|r", cmd, count);
        PB_LOG(1, "Command '{}' by '{}' executed on {} bot(s)",
            cmd, currentPlayer->GetName(), count);
        return true;
    }

    // ---- 技能条（pet-style skill bar, 12 slots = player's first action bar）----
    // The 12 slots ARE the bot character's first action bar (button N-1) - no
    // manual spell assignment; the master only toggles autocast per slot.
    // Commands:
    //   /bot skillpool                    -> list the bot's skill pool
    //   /bot skill info <1-12>            -> show what's in a slot
    //   /bot spell <1-12>                 -> cast the skill in a slot
    //   /bot autospell <1-12>             -> toggle autocast for a slot
    if (cmd == "skill" || cmd == "spell" || cmd == "autospell")
    {
        char* arg1 = strtok(nullptr, " ");

        // Optional single-bot target: /bot <cmd> [$botName] ...
        std::set<ObjectGuid> targets;
        if (arg1)
        {
            ObjectGuid tg = sPlayerBotMgr->FindPlayerByName(arg1);
            if (!tg.IsEmpty() && sPlayerBotMgr->IsMasterOf(currentPlayer->GetGUID(), tg))
            {
                targets.insert(tg);
                arg1 = strtok(nullptr, " "); // consume the bot-name token
            }
        }
        if (targets.empty())
            targets = sPlayerBotMgr->GetBotsByMaster(currentPlayer->GetGUID());

        if (cmd == "skill")
        {
            // First sub-command token.
            std::string sub = arg1 ? arg1 : "";
            std::transform(sub.begin(), sub.end(), sub.begin(), ::tolower);

            // Find the first (or only) bot of this master for pool/info ops.
            auto botGuids = targets;
            if (botGuids.empty())
            {
                BotSendSysMessage(handler, "|cffff0000No bots.|r");
                return true;
            }
            ObjectGuid firstBot = *botGuids.begin();
            Player* firstBotPlayer = ObjectAccessor::FindConnectedPlayer(firstBot);
            std::string firstBotName = sPlayerBotMgr->GetCharacterName(firstBot);

            if (sub == "pool")
            {
                if (!firstBotPlayer)
                {
                    BotSendSysMessage(handler, "|cffff0000'{}' is offline.|r", firstBotName);
                    return true;
                }
                auto pool = ScanBotSkillPool(firstBotPlayer);
                BotSendSysMessage(handler, "|cff00ff00'{}' skill pool ({}):|r", firstBotName, pool.size());
                uint32 idx = 0;
                for (uint32 sid : pool)
                {
                    SpellInfo const* info = sSpellMgr->GetSpellInfo(sid);
                    if (!info) continue;
                    BotSendSysMessage(handler, "  {}: [{}] {}",
                        ++idx, sid, info->SpellName[sWorld->GetDefaultDbcLocale()]);
                }
                return true;
            }

            if (sub == "info")
            {
                char* arg2 = strtok(nullptr, " ");
                if (!arg2)
                {
                    BotSendSysMessage(handler, "|cffff0000Usage: /bot skill info <1-{}>.|r", PlayerBotMgr::MAX_SKILL_SLOTS);
                    return true;
                }
                int slot = atoi(arg2);
                if (slot < 1 || slot > PlayerBotMgr::MAX_SKILL_SLOTS)
                {
                    BotSendSysMessage(handler, "|cffff0000Slot must be 1-{}.|r", PlayerBotMgr::MAX_SKILL_SLOTS);
                    return true;
                }
                if (!firstBotPlayer)
                {
                    BotSendSysMessage(handler, "|cffff0000'{}' is offline.|r", firstBotName);
                    return true;
                }
                // Slot N is read from the bot character's first action bar.
                uint32 sid = sPlayerBotMgr->GetBotSkillSlot(firstBotPlayer, slot);
                if (!sid)
                    BotSendSysMessage(handler, "|cffffffff'{}' slot {}: empty.|r", firstBotName, slot);
                else if (SpellInfo const* info = sSpellMgr->GetSpellInfo(sid))
                    BotSendSysMessage(handler, "|cff00ff00'{}' slot {}: [{}] {}.|r",
                        firstBotName, slot, sid,
                        info->SpellName[sWorld->GetDefaultDbcLocale()]);
                return true;
            }

            BotSendSysMessage(handler, "|cffff0000Usage: /bot skill pool|info <1-{}>.|r", PlayerBotMgr::MAX_SKILL_SLOTS);
            return true;
        }

        // spell <N> / autospell <N>
        if (!arg1)
        {
            BotSendSysMessage(handler, "|cffff0000Usage: /bot {} <1-{}>.|r", cmd, PlayerBotMgr::MAX_SKILL_SLOTS);
            return true;
        }
        int slot = atoi(arg1);
        if (slot < 1 || slot > PlayerBotMgr::MAX_SKILL_SLOTS)
        {
            BotSendSysMessage(handler, "|cffff0000Slot must be 1-{}.|r", PlayerBotMgr::MAX_SKILL_SLOTS);
            return true;
        }

        uint32 count = 0;
        for (auto const& botGuid : targets)
        {
            Player* bot = ObjectAccessor::FindConnectedPlayer(botGuid);
            if (!bot || !bot->IsInWorld())
                continue;

            if (cmd == "autospell")
            {
                sPlayerBotMgr->SetBotSlotAutocast(botGuid, slot,
                    !sPlayerBotMgr->IsBotSlotAutocast(botGuid, slot));
                bool on = sPlayerBotMgr->IsBotSlotAutocast(botGuid, slot);
                BotSendSysMessage(handler, "|cff00ff00'{}' slot {} autocast: {}.|r", bot->GetName(), slot, on ? "ON" : "OFF");
                count++;
            }
            else // spell: cast the slot's skill on the bot's current target
            {
                // Slot N is read from the bot's own first action bar.
                uint32 sid = sPlayerBotMgr->GetBotSkillSlot(bot, slot);
                if (sid)
                {
                    if (Unit* t = bot->GetVictim())
                    {
                        if (t->IsAlive() && bot->IsValidAttackTarget(t))
                            bot->CastSpell(t, sid, false);
                    }
                    else if (Unit* t = ObjectAccessor::GetUnit(*bot, bot->GetTarget()))
                    {
                        if (t && t->IsAlive() && bot->IsValidAttackTarget(t))
                            bot->CastSpell(t, sid, false);
                    }
                    BotSendSysMessage(handler, "|cff00ff00'{}' cast slot {} [{}].|r", bot->GetName(), slot, sid);
                }
                else
                    BotSendSysMessage(handler, "|cffff0000'{}' slot {} is empty.|r", bot->GetName(), slot);
                count++;
            }
        }
        BotSendSysMessage(handler, "|cff00ff00{}: {} bot(s).|r", cmd, count);
        PB_LOG(1, "Command '{}' by '{}': processed {} bot(s)", cmd, currentPlayer->GetName(), count);
        return true;
    }

    if (cmd == "set")
    {
        if (!isGM3)
        {
            BotSendSysMessage(handler, "|cffff0000Permission denied.|r");
            return true;
        }

        char* playerName = strtok(nullptr, " ");
        if (!playerName)
        {
            BotSendSysMessage(handler, "|cffff0000Incorrect syntax.|r");
            return true;
        }

        ObjectGuid guid = sPlayerBotMgr->FindPlayerByName(playerName);
        if (guid.IsEmpty())
        {
            BotSendSysMessage(handler, "|cffff0000Player '{}' not found.|r", playerName);
            return true;
        }

        if (sPlayerBotMgr->IsBot(guid))
        {
            BotSendSysMessage(handler, "|cffff0000Player '{}' is already a Bot.|r", playerName);
            return true;
        }

        sPlayerBotMgr->AddBot(guid);
        PB_LOG(1, "Command 'set' by '{}': '{}' marked as bot ({})",
            currentPlayer->GetName(), playerName, guid.ToString());
        BotSendSysMessage(handler, "|cff00ff00Player '{}' marked as Bot ({} total).|r", playerName, sPlayerBotMgr->GetCount());
        return true;
    }

    if (cmd == "remove")
    {
        if (!isGM3)
        {
            BotSendSysMessage(handler, "|cffff0000Permission denied.|r");
            return true;
        }

        char* playerName = strtok(nullptr, " ");
        if (!playerName)
        {
            BotSendSysMessage(handler, "|cffff0000Incorrect syntax.|r");
            return true;
        }

        ObjectGuid guid = sPlayerBotMgr->FindPlayerByName(playerName);
        if (guid.IsEmpty())
        {
            BotSendSysMessage(handler, "|cffff0000Player '{}' not found.|r", playerName);
            return true;
        }

        if (!sPlayerBotMgr->IsBot(guid))
        {
            BotSendSysMessage(handler, "|cffff0000Player '{}' is not a Bot.|r", playerName);
            return true;
        }

        sPlayerBotMgr->RemoveBot(guid);
        PB_LOG(1, "Command 'remove' by '{}': '{}' removed from bots ({})",
            currentPlayer->GetName(), playerName, guid.ToString());
        BotSendSysMessage(handler, "|cff00ff00Player '{}' removed from Bot list ({} total).|r", playerName, sPlayerBotMgr->GetCount());
        return true;
    }

    if (cmd == "list")
    {
        // Masters can list their own bots; GM3 can list every bot.
        std::set<ObjectGuid> bots;
        if (isGM3)
            bots = sPlayerBotMgr->GetAllBots();
        else
            bots = sPlayerBotMgr->GetBotsByMaster(currentPlayer->GetGUID());

        // 对齐 AZ onlinelist 表格格式 (acore_string 1015/1010/1012/1013)。
        // 玩家端也输出同样的 -[...]- 表格，BotCommander 插件直接解析文本即可。
        BotSendSysMessage(handler, "-================ PlayerBot 在线列表 ({}) ================-", bots.size());
        BotSendSysMessage(handler, "-[{:<14}][{:<4}][{:<14}][{:<12}]-", "角色名", "在线", "主人", "姿态");
        BotSendSysMessage(handler, "-==================================================================-");
        for (auto const& guid : bots)
        {
            Player* player = ObjectAccessor::FindConnectedPlayer(guid);

            const char* stanceName = "defensive";
            switch (sPlayerBotMgr->GetBotStance(guid))
            {
                case PlayerBotMgr::STANCE_PASSIVE:    stanceName = "passive"; break;
                case PlayerBotMgr::STANCE_AGGRESSIVE: stanceName = "aggressive"; break;
                default: break;
            }

            BotSendSysMessage(handler, "-[{:<14}][{:<4}][{:<14}][{:<12}]-",
                sPlayerBotMgr->GetCharacterName(guid),
                player ? "1" : "0",
                sPlayerBotMgr->GetMasterName(guid).c_str(), // "None"/"Offline Master"/master name
                stanceName);

            // DebugDump (PlayerBot.DebugLevel >= 3): 每 bot 附加一行状态调试信息。
            if (sPlayerBotMgr->DebugDump())
            {
                Unit* victim = player ? player->GetVictim() : nullptr;
                BotSendSysMessage(handler, "|cffffffff  State: cmd={}, ret={}, tgt={}, slot={}, lv={}, hp={}%, map={}, combat={}, victim={}|r",
                    sPlayerBotMgr->CommandName(sPlayerBotMgr->GetBotCommand(guid)),
                    sPlayerBotMgr->GetBotReturnMode(guid) ? "stay" : "follow",
                    sPlayerBotMgr->GetBotAttackTarget(guid).GetRawValue() ? sPlayerBotMgr->GetBotAttackTarget(guid).ToString() : "none",
                    sPlayerBotMgr->GetBotSkillCursor(guid),
                    player ? int(player->GetLevel()) : -1,
                    player ? int(player->GetHealthPct()) : -1,
                    player ? int(player->GetMapId()) : -1,
                    player ? (player->IsInCombat() ? "1" : "0") : "-",
                    player ? (victim ? victim->GetName() : "none") : "-");
            }
        }
        BotSendSysMessage(handler, "-==================================================================-");
        return true;
    }

    if (cmd == "stance")
    {
        char* arg1 = strtok(nullptr, " ");
        if (!arg1)
        {
            BotSendSysMessage(handler, "|cffff0000Usage: /bot stance [$name] [d|p|a].|r");
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
        const char* stanceLowerNames[] = { "defensive", "passive", "aggressive" };

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
                    if (newStance == PlayerBotMgr::STANCE_PASSIVE)
                    {
                        // Mirror pet REACT_PASSIVE: immediately stop fighting
                        // and drop combat when switched to passive.
                        if (Player* bot = ObjectAccessor::FindConnectedPlayer(botGuid))
                        {
                            if (bot->IsInCombat())
                                bot->CombatStop(true);
                            bot->InterruptNonMeleeSpells(false);
                        }
                    }
                    count++;
                }
            }
            BotSendSysMessage(handler, "|cff00ff00Stance {} set for {} bot(s).|r", stanceLowerNames[newStance], count);
            PB_LOG(1, "Command 'stance' by '{}': set {} for {} bot(s)",
                currentPlayer->GetName(), stanceNames[newStance], count);
            return true;
        }

        // Named bot: /bot stance <name> <stance>
        if (!arg2)
        {
            BotSendSysMessage(handler, "|cffff0000Usage: /bot stance <name> <d|p|a>.|r");
            return true;
        }

        char* playerName = arg1;
        char* stanceName = arg2;

        ObjectGuid guid = sPlayerBotMgr->FindPlayerByName(playerName);
        if (guid.IsEmpty())
        {
            BotSendSysMessage(handler, "|cffff0000Bot '{}' not found.|r", playerName);
            return true;
        }

        if (!sPlayerBotMgr->IsBot(guid))
        {
            BotSendSysMessage(handler, "|cffff0000'{}' is not a Bot.|r", playerName);
            return true;
        }

        ObjectGuid masterGuid = sPlayerBotMgr->GetMaster(guid);
        if (!isGM3 && masterGuid != currentPlayer->GetGUID())
        {
            BotSendSysMessage(handler, "|cffff0000You don't own bot '{}'.|r", playerName);
            return true;
        }

        std::string stanceArgLower(stanceName);
        std::transform(stanceArgLower.begin(), stanceArgLower.end(), stanceArgLower.begin(), ::tolower);
        PlayerBotMgr::BotStance newStance = parseStance(stanceArgLower);

        sPlayerBotMgr->SetBotStance(guid, newStance);

        // Mirror pet REACT_PASSIVE: immediately stop fighting when passive.
        Player* bot = ObjectAccessor::FindConnectedPlayer(guid);
        if (newStance == PlayerBotMgr::STANCE_PASSIVE && bot)
        {
            if (bot->IsInCombat())
                bot->CombatStop(true);
            bot->InterruptNonMeleeSpells(false);
        }

        BotSendSysMessage(handler, "|cff00ff00'{}' stance: {}.|r", playerName, stanceNames[newStance]);
        PB_LOG(1, "Command 'stance' by '{}': bot '{}' set to {}",
            currentPlayer->GetName(), playerName, stanceNames[newStance]);

        if (bot)
        {
            ChatHandler h(bot->GetSession());
            BotSendSysMessage(&h, "|cff00ff00[Bot]|r Stance changed to |cffff00ff{}|r.", 
                stanceNames[newStance]);
        }
        return true;
    }

    if (cmd == "clearmaster")
    {
        if (!isGM3)
        {
            BotSendSysMessage(handler, "|cffff0000Permission denied.|r");
            return true;
        }

        char* playerName = strtok(nullptr, " ");
        if (!playerName)
        {
            BotSendSysMessage(handler, "|cffff0000Usage: /bot clearmaster <name>.|r");
            return true;
        }

        ObjectGuid guid = sPlayerBotMgr->FindPlayerByName(playerName);
        if (guid.IsEmpty())
        {
            BotSendSysMessage(handler, "|cffff0000Bot '{}' not found.|r", playerName);
            return true;
        }

        if (!sPlayerBotMgr->IsBot(guid))
        {
            BotSendSysMessage(handler, "|cffff0000'{}' is not a Bot.|r", playerName);
            return true;
        }

        sPlayerBotMgr->ClearMaster(guid);
        PB_LOG(1, "Command 'clearmaster' by '{}': bot '{}' master cleared ({})",
            currentPlayer->GetName(), playerName, guid.ToString());
        BotSendSysMessage(handler, "|cff00ff00'{}' master cleared.|r", playerName);
        return true;
    }

    if (cmd == "master")
    {
        if (!isGM3)
        {
            BotSendSysMessage(handler, "|cffff0000Permission denied.|r");
            return true;
        }

        char* playerName = strtok(nullptr, " ");
        if (!playerName)
        {
            BotSendSysMessage(handler, "|cffff0000Usage: /bot master <name>.|r");
            return true;
        }

        ObjectGuid guid = sPlayerBotMgr->FindPlayerByName(playerName);
        if (guid.IsEmpty())
        {
            BotSendSysMessage(handler, "|cffff0000Bot '{}' not found.|r", playerName);
            return true;
        }

        if (!sPlayerBotMgr->IsBot(guid))
        {
            BotSendSysMessage(handler, "|cffff0000'{}' is not a Bot.|r", playerName);
            return true;
        }

        ObjectGuid masterGuid = sPlayerBotMgr->GetMaster(guid);
        if (!masterGuid.IsEmpty())
        {
            Player* master = ObjectAccessor::FindConnectedPlayer(masterGuid);
            BotSendSysMessage(handler, "|cff00ff00'{}' master: {}|r", playerName,
                master ? master->GetName().c_str() : "Offline Master");
            PB_LOG(1, "Command 'master' by '{}': bot '{}' master is '{}'",
                currentPlayer->GetName(), playerName,
                master ? master->GetName().c_str() : "Offline");
        }
        else
        {
            BotSendSysMessage(handler, "|cff00ff00'{}' master: None|r", playerName);
            PB_LOG(1, "Command 'master' by '{}': bot '{}' has no master",
                currentPlayer->GetName(), playerName);
        }
        return true;
    }

    BotSendSysMessage(handler, "|cffff0000Unknown command '{}'.|r", action);
    return true;
}
