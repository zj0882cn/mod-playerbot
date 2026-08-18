#ifndef _BOT_COMMON_H_
#define _BOT_COMMON_H_

#include "Chat.h"
#include "Player.h"
#include "PlayerBotMgr.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "SharedDefines.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "fmt/format.h"
#include <utility>
#include <vector>
#include <set>

// Scan a bot's spellbook + equipment for usable active skills (the pool the
// master can assign to the pet-style skill bar). Covers class/talent spells
// and item (trinket/gear) spells.
inline std::vector<uint32> ScanBotSkillPool(Player* bot)
{
    std::vector<uint32> pool;
    if (!bot)
        return pool;

    std::set<uint32> seen;

    for (auto const& [spellId, playerSpell] : bot->GetSpellMap())
    {
        if (!playerSpell || !playerSpell->Active)
            continue;
        SpellInfo const* info = sSpellMgr->GetSpellInfo(spellId);
        if (!info)
            continue;
        if (info->IsPassive())
            continue;
        if (info->HasEffect(SPELL_EFFECT_TELEPORT_UNITS) ||
            info->HasEffect(SPELL_EFFECT_SUMMON_PET) ||
            info->HasEffect(SPELL_EFFECT_SKILL_STEP) ||
            info->HasEffect(SPELL_EFFECT_LEARN_SPELL) ||
            info->HasEffect(SPELL_EFFECT_TRADE_SKILL))
            continue;
        if (seen.insert(spellId).second)
            pool.push_back(spellId);
    }

    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (!item)
            continue;
        ItemTemplate const* tmpl = item->GetTemplate();
        if (!tmpl)
            continue;
        for (uint8 i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
        {
            uint32 spellId = tmpl->Spells[i].SpellId;
            if (!spellId)
                continue;
            if (seen.insert(spellId).second)
                pool.push_back(spellId);
        }
    }

    return pool;
}

// Cross-version PSendSysMessage helper.
//
// Newer AzerothCore cores accept fmt "{}" placeholders in PSendSysMessage,
// but older cores (e.g. v4.0.0) use printf-style and would print "{}"
// literally (never substituted).
//   - new cores: pass through, they handle "{}" natively.
//   - legacy cores: pre-format with fmt::format first (legacy fmt allows
//     runtime format strings), then the printf-style call just prints it.
template <typename... Args>
inline void BotSendSysMessage(ChatHandler* handler, char const* format, Args&&... args)
{
#ifdef PLAYERBOT_NEW_PLAYERSCRIPT
    handler->PSendSysMessage(format, std::forward<Args>(args)...);
#else
    // vformat accepts runtime format strings on every fmt version.
    handler->PSendSysMessage(fmt::vformat(format, fmt::make_format_args(args...)).c_str());
#endif
}

// Send a message to the bot itself AND to its current master (if online),
// so the master can see what the bot is doing.
template <typename... Args>
inline void BotNotify(Player* bot, char const* format, Args&&... args)
{
    if (!bot)
        return;

    // Notify the bot itself.
    if (bot->GetSession())
    {
        ChatHandler handler(bot->GetSession());
        BotSendSysMessage(&handler, format, std::forward<Args>(args)...);
    }

    // Notify the master.
    ObjectGuid masterGuid = sPlayerBotMgr->GetMaster(bot->GetGUID());
    if (!masterGuid.IsEmpty() && masterGuid != bot->GetGUID())
    {
        Player* master = ObjectAccessor::FindConnectedPlayer(masterGuid);
        if (master && master->GetSession())
        {
            ChatHandler handler(master->GetSession());
            std::string msg = fmt::vformat(format, fmt::make_format_args(args...));
            BotSendSysMessage(&handler, "|cff00ff00[Bot:{}]|r {}", bot->GetName(), msg);
        }
    }
}

// ---------------------------------------------------------------------------
// Debug logging (AZ-style: reuses the core "playerbots" log filter + LOG_INFO,
// but gated by the module's own PlayerBot.DebugLevel config so production stays
// quiet at the default 0 and debugging turns on without touching the core's
// global Log.Level).
//   DebugLevel 1 = events      : every hook entry, bot state change, command
//   DebugLevel 2 = behaviour   : + per-tick AI decisions (targets, casts, moves)
//   DebugLevel 3 = dump        : bot list prints full in-memory state + live
//                                Player info for every bot
// Use PB_LOG(1, ...) for events, PB_LOG(2, ...) for behaviour, PB_LOG(3, ...)
// for dumps. The format string and args follow fmt (same as LOG_INFO).
// ---------------------------------------------------------------------------
#define PB_LOG(lvl, ...) \
    do { if (PlayerBotMgr::DebugLevelAtLeast(lvl)) LOG_INFO("playerbots", "[PB L" #lvl "] " __VA_ARGS__); } while (0)

#endif
