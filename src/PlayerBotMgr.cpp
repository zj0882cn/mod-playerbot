#include "PlayerBotMgr.h"
#include "BotCommon.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "CharacterDatabase.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Log.h"

#define PLAYERBOT_VERSION "v2.2.0"

PlayerBotMgr* PlayerBotMgr::instance()
{
    static PlayerBotMgr inst;
    return &inst;
}

bool PlayerBotMgr::IsBot(ObjectGuid guid) const
{
    return _botGuids.find(guid) != _botGuids.end();
}

void PlayerBotMgr::AddBot(ObjectGuid guid)
{
    _botGuids.insert(guid);
    LOG_INFO("playerbots", "[{}] Bot added: {}", PLAYERBOT_VERSION, guid.ToString());
}

void PlayerBotMgr::RemoveBot(ObjectGuid guid)
{
    _botGuids.erase(guid);
    _botMasters.erase(guid);
    _botStances.erase(guid);
    _botCommands.erase(guid);
    _botAttackTargets.erase(guid);
    _botStayPositions.erase(guid);
    _botReturnModes.erase(guid);
    _botAutoSpells.erase(guid);
    _botSlotAutocast.erase(guid);
    _botSkillCursors.erase(guid);
    LOG_INFO("playerbots", "[{}] Bot removed: {}", PLAYERBOT_VERSION, guid.ToString());
}

size_t PlayerBotMgr::GetCount() const
{
    return _botGuids.size();
}

std::set<ObjectGuid> PlayerBotMgr::GetAllBots() const
{
    return _botGuids;
}

void PlayerBotMgr::SetMaster(ObjectGuid botGuid, ObjectGuid masterGuid)
{
    _botMasters[botGuid] = masterGuid;
    PB_LOG(1, "Bot '{}' master set to '{}' ({} -> {})",
        GetCharacterName(botGuid), GetCharacterName(masterGuid),
        botGuid.ToString(), masterGuid.ToString());
}

ObjectGuid PlayerBotMgr::GetMaster(ObjectGuid botGuid) const
{
    auto itr = _botMasters.find(botGuid);
    if (itr != _botMasters.end())
        return itr->second;
    return ObjectGuid::Empty;
}

void PlayerBotMgr::ClearMaster(ObjectGuid botGuid)
{
    _botMasters.erase(botGuid);
    PB_LOG(1, "Bot '{}' master cleared ({})", GetCharacterName(botGuid), botGuid.ToString());
}

std::string PlayerBotMgr::GetMasterName(ObjectGuid botGuid) const
{
    auto itr = _botMasters.find(botGuid);
    if (itr != _botMasters.end())
    {
        Player* master = ObjectAccessor::FindConnectedPlayer(itr->second);
        if (master)
            return master->GetName();
        return "Offline Master";
    }
    return "None";
}

bool PlayerBotMgr::IsMasterOf(ObjectGuid masterGuid, ObjectGuid botGuid) const
{
    auto itr = _botMasters.find(botGuid);
    return itr != _botMasters.end() && itr->second == masterGuid;
}

void PlayerBotMgr::SetBotStance(ObjectGuid botGuid, BotStance stance)
{
    _botStances[botGuid] = stance;
    PB_LOG(1, "Bot '{}' stance set to {} ({})",
        GetCharacterName(botGuid), StanceName(stance), botGuid.ToString());
}

PlayerBotMgr::BotStance PlayerBotMgr::GetBotStance(ObjectGuid botGuid) const
{
    auto itr = _botStances.find(botGuid);
    if (itr != _botStances.end())
        return itr->second;
    return STANCE_DEFENSIVE;
}

void PlayerBotMgr::SetBotCommand(ObjectGuid botGuid, BotCommand cmd)
{
    if (cmd == BOT_COMMAND_NONE)
        _botCommands.erase(botGuid);
    else
        _botCommands[botGuid] = cmd;
    PB_LOG(1, "Bot '{}' command set to {} ({})",
        GetCharacterName(botGuid), CommandName(cmd), botGuid.ToString());
}

PlayerBotMgr::BotCommand PlayerBotMgr::GetBotCommand(ObjectGuid botGuid) const
{
    auto itr = _botCommands.find(botGuid);
    if (itr != _botCommands.end())
        return itr->second;
    return BOT_COMMAND_NONE;
}

void PlayerBotMgr::SetBotAttackTarget(ObjectGuid botGuid, ObjectGuid targetGuid)
{
    if (targetGuid.IsEmpty())
        _botAttackTargets.erase(botGuid);
    else
        _botAttackTargets[botGuid] = targetGuid;
    PB_LOG(1, "Bot '{}' attack target set to {} ({})",
        GetCharacterName(botGuid), targetGuid.ToString(), botGuid.ToString());
}

ObjectGuid PlayerBotMgr::GetBotAttackTarget(ObjectGuid botGuid) const
{
    auto itr = _botAttackTargets.find(botGuid);
    if (itr != _botAttackTargets.end())
        return itr->second;
    return ObjectGuid::Empty;
}

void PlayerBotMgr::SetBotStayPosition(ObjectGuid botGuid, float x, float y, float z)
{
    _botStayPositions[botGuid] = { x, y, z };
    PB_LOG(1, "Bot '{}' stay position set to {:.1f} {:.1f} {:.1f} ({})",
        GetCharacterName(botGuid), x, y, z, botGuid.ToString());
}

bool PlayerBotMgr::GetBotStayPosition(ObjectGuid botGuid, float& x, float& y, float& z) const
{
    auto itr = _botStayPositions.find(botGuid);
    if (itr == _botStayPositions.end())
        return false;
    x = itr->second[0];
    y = itr->second[1];
    z = itr->second[2];
    return true;
}

void PlayerBotMgr::ClearBotStayPosition(ObjectGuid botGuid)
{
    _botStayPositions.erase(botGuid);
    PB_LOG(1, "Bot '{}' stay position cleared ({})", GetCharacterName(botGuid), botGuid.ToString());
}

void PlayerBotMgr::SetBotReturnMode(ObjectGuid botGuid, bool stayAtSpot)
{
    _botReturnModes[botGuid] = stayAtSpot;
    PB_LOG(1, "Bot '{}' return mode set to {} ({})",
        GetCharacterName(botGuid), stayAtSpot ? "stay" : "follow", botGuid.ToString());
}

bool PlayerBotMgr::GetBotReturnMode(ObjectGuid botGuid) const
{
    auto itr = _botReturnModes.find(botGuid);
    if (itr == _botReturnModes.end())
        return false; // default: follow the master
    return itr->second;
}

void PlayerBotMgr::ClearBotReturnMode(ObjectGuid botGuid)
{
    _botReturnModes.erase(botGuid);
    PB_LOG(1, "Bot '{}' return mode cleared ({})", GetCharacterName(botGuid), botGuid.ToString());
}

void PlayerBotMgr::AddBotAutoSpell(ObjectGuid botGuid, uint32 spellId)
{
    _botAutoSpells[botGuid].insert(spellId);
    PB_LOG(1, "Bot '{}' auto-spell added {} ({})", GetCharacterName(botGuid), spellId, botGuid.ToString());
}

void PlayerBotMgr::RemoveBotAutoSpell(ObjectGuid botGuid, uint32 spellId)
{
    auto itr = _botAutoSpells.find(botGuid);
    if (itr != _botAutoSpells.end())
        itr->second.erase(spellId);
    PB_LOG(1, "Bot '{}' auto-spell removed {} ({})", GetCharacterName(botGuid), spellId, botGuid.ToString());
}

bool PlayerBotMgr::IsBotAutoSpellEnabled(ObjectGuid botGuid, uint32 spellId) const
{
    auto itr = _botAutoSpells.find(botGuid);
    if (itr != _botAutoSpells.end())
        return itr->second.find(spellId) != itr->second.end();
    return false;
}

std::vector<uint32> PlayerBotMgr::GetBotAutoSpells(ObjectGuid botGuid) const
{
    std::vector<uint32> result;
    auto itr = _botAutoSpells.find(botGuid);
    if (itr != _botAutoSpells.end())
        result.assign(itr->second.begin(), itr->second.end());
    return result;
}

// Read the skill in slot N (1-12) straight from the bot character's own FIRST
// action bar (button N-1), matching the real client. No manual assignment:
// the bar IS the bot's action bar. Returns 0 for empty/non-spell/passive AND
// for skills the bot never learned - a leftover spell from another class on
// the bar (e.g. a paladin with Charge on the bar) must NOT be auto-cast.
uint32 PlayerBotMgr::GetBotSkillSlot(Player* bot, uint8 slot) const
{
    if (!bot || slot == 0 || slot > MAX_SKILL_SLOTS)
        return 0;
    if (ActionButton const* ab = bot->GetActionButton(slot - 1))
    {
        if (ab->GetType() != ACTION_BUTTON_SPELL)
            return 0;
        uint32 sid = ab->GetAction();
        if (SpellInfo const* si = sSpellMgr->GetSpellInfo(sid))
            if (si->IsPassive())
                return 0;
        if (!bot->HasSpell(sid))
            return 0; // bot never learned it - ignore (prevents a paladin
                      // auto-casting another class's skill left on the bar)
        return sid;
    }
    return 0;
}

void PlayerBotMgr::SetBotSlotAutocast(ObjectGuid botGuid, uint8 slot, bool enable)
{
    if (slot == 0 || slot > MAX_SKILL_SLOTS)
        return;
    _botSlotAutocast[botGuid][slot - 1] = enable;
    PB_LOG(1, "Bot '{}' slot {} autocast set to {} ({})",
        GetCharacterName(botGuid), slot, enable ? "on" : "off", botGuid.ToString());
}

bool PlayerBotMgr::IsBotSlotAutocast(ObjectGuid botGuid, uint8 slot) const
{
    if (slot == 0 || slot > MAX_SKILL_SLOTS)
        return false;
    auto itr = _botSlotAutocast.find(botGuid);
    if (itr != _botSlotAutocast.end())
        return itr->second[slot - 1];
    return true; // default: ALL slots auto-cast (real-client style); the
                 // master can still turn individual slots off with autospell
}

void PlayerBotMgr::SetBotSkillCursor(ObjectGuid botGuid, uint32 cursor)
{
    _botSkillCursors[botGuid] = cursor;
    PB_LOG(2, "Bot '{}' skill cursor -> {} ({})", GetCharacterName(botGuid), cursor, botGuid.ToString());
}

uint32 PlayerBotMgr::GetBotSkillCursor(ObjectGuid botGuid) const
{
    auto itr = _botSkillCursors.find(botGuid);
    if (itr != _botSkillCursors.end())
        return itr->second;
    return 0;
}

std::set<ObjectGuid> PlayerBotMgr::GetBotsByMaster(ObjectGuid masterGuid) const
{
    std::set<ObjectGuid> result;
    for (auto const& [botGuid, m] : _botMasters)
    {
        if (m == masterGuid)
            result.insert(botGuid);
    }
    return result;
}

// ---- Debug logging (AZ-style, controlled by PlayerBot.DebugLevel) ----

void PlayerBotMgr::SetDebugLevel(uint32 level)
{
    _debugLevel = level;
    LOG_INFO("playerbots", "[{}] PlayerBot debug level set to {}", PLAYERBOT_VERSION, _debugLevel);
}

uint32 PlayerBotMgr::GetDebugLevel() const
{
    return _debugLevel;
}

bool PlayerBotMgr::DebugLevelAtLeast(uint32 level)
{
    return instance()->_debugLevel >= level;
}

bool PlayerBotMgr::DebugEnabled()
{
    return instance()->_debugLevel >= 1;
}

bool PlayerBotMgr::DebugTrace()
{
    return instance()->_debugLevel >= 2;
}

bool PlayerBotMgr::DebugDump()
{
    return instance()->_debugLevel >= 3;
}

char const* PlayerBotMgr::StanceName(BotStance stance)
{
    switch (stance)
    {
        case STANCE_PASSIVE:    return "passive";
        case STANCE_AGGRESSIVE: return "aggressive";
        case STANCE_DEFENSIVE:
        default:                return "defensive";
    }
}

char const* PlayerBotMgr::CommandName(BotCommand cmd)
{
    switch (cmd)
    {
        case BOT_COMMAND_ATTACK: return "ATTACK";
        case BOT_COMMAND_FOLLOW: return "FOLLOW";
        case BOT_COMMAND_STAY:   return "STAY";
        case BOT_COMMAND_NONE:
        default:                 return "NONE";
    }
}

// Dump everything we hold in memory for one bot (all maps), so `bot list` /
// DebugDump can show the server-side state of a bot for debugging.
std::string PlayerBotMgr::DumpBotState(ObjectGuid botGuid) const
{
    std::string out;
    out += "  Bot " + GetCharacterName(botGuid) + " (" + botGuid.ToString() + ")\n";

    auto masterItr = _botMasters.find(botGuid);
    out += "    master=" + (masterItr != _botMasters.end() ? GetCharacterName(masterItr->second) : "none")
         + " (" + (masterItr != _botMasters.end() ? masterItr->second.ToString() : "empty") + ")\n";

    auto stanceItr = _botStances.find(botGuid);
    out += "    stance=" + std::string(StanceName(stanceItr != _botStances.end() ? stanceItr->second : STANCE_DEFENSIVE)) + "\n";

    auto cmdItr = _botCommands.find(botGuid);
    out += "    command=" + std::string(CommandName(cmdItr != _botCommands.end() ? cmdItr->second : BOT_COMMAND_NONE)) + "\n";

    auto atkItr = _botAttackTargets.find(botGuid);
    out += "    attackTarget=" + (atkItr != _botAttackTargets.end() && !atkItr->second.IsEmpty() ? atkItr->second.ToString() : "none") + "\n";

    auto stayItr = _botStayPositions.find(botGuid);
    if (stayItr != _botStayPositions.end())
        out += "    stayPos=" + std::to_string(stayItr->second[0]) + " "
             + std::to_string(stayItr->second[1]) + " "
             + std::to_string(stayItr->second[2]) + "\n";
    else
        out += "    stayPos=none\n";

    auto modeItr = _botReturnModes.find(botGuid);
    out += "    returnMode=" + std::string(modeItr != _botReturnModes.end() && modeItr->second ? "stay" : "follow") + "\n";

    auto spellsItr = _botAutoSpells.find(botGuid);
    out += "    autoSpells=[";
    if (spellsItr != _botAutoSpells.end())
    {
        bool first = true;
        for (uint32 sid : spellsItr->second)
        {
            out += (first ? "" : ",") + std::to_string(sid);
            first = false;
        }
    }
    out += "]\n";

    auto slotItr = _botSlotAutocast.find(botGuid);
    out += "    slotAutocast=[";
    for (uint8 s = 1; s <= MAX_SKILL_SLOTS; ++s)
    {
        bool on = slotItr != _botSlotAutocast.end() ? slotItr->second[s - 1] : true;
        out += (s > 1 ? "," : "") + std::string(on ? "1" : "0");
    }
    out += "]\n";

    auto curItr = _botSkillCursors.find(botGuid);
    out += "    skillCursor=" + std::to_string(curItr != _botSkillCursors.end() ? curItr->second : 0) + "\n";

    return out;
}

ObjectGuid PlayerBotMgr::FindPlayerByName(std::string const& name)
{
    auto const& players = ObjectAccessor::GetPlayers();
    std::string searchLower = name;
    std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);

    // First, search online players.
    for (auto const& [guid, player] : players)
    {
        if (!player)
            continue;
        std::string pName = player->GetName();
        std::transform(pName.begin(), pName.end(), pName.begin(), ::tolower);
        if (pName == searchLower)
            return guid;
    }

    // Not online - look up the character in the database so that offline
    // characters can be marked as bots too (e.g. before they log in).
    if (QueryResult result = CharacterDatabase.Query(
        "SELECT guid FROM characters WHERE name = '{}'", name))
    {
        Field* fields = result->Fetch();
        if (fields)
            return ObjectGuid::Create<HighGuid::Player>(fields[0].Get<uint32>());
    }

    PB_LOG(1, "Player '{}' not found (online or in characters db)", name);
    return ObjectGuid::Empty;
}

std::string PlayerBotMgr::GetCharacterName(ObjectGuid guid) const
{
    // Online players: return the in-memory name directly.
    if (Player* player = ObjectAccessor::FindConnectedPlayer(guid))
        return player->GetName();

    // Offline: query the characters database.
    if (QueryResult result = CharacterDatabase.Query(
        "SELECT name FROM characters WHERE guid = {}", guid.GetCounter()))
    {
        if (Field* fields = result->Fetch())
            return fields[0].Get<std::string>();
    }

    return guid.ToString();
}
