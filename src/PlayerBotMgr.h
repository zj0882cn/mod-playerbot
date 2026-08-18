#ifndef _PLAYERBOT_MGR_H_
#define _PLAYERBOT_MGR_H_

#include "Common.h"
#include "ObjectGuid.h"
#include <array>
#include <map>
#include <set>
#include <string>

class Player;

class PlayerBotMgr
{
public:
    enum BotStance
    {
        STANCE_DEFENSIVE = 0,
        STANCE_PASSIVE = 1,
        STANCE_AGGRESSIVE = 2
    };

    // Pet-style command state (highest priority, overrides react/stance AI).
    // Mirrors a hunter pet's command buttons: Attack / Follow / Stay.
    enum BotCommand
    {
        BOT_COMMAND_NONE   = 0,
        BOT_COMMAND_ATTACK = 1,  // attack explicit target until it dies/is lost
        BOT_COMMAND_FOLLOW = 2,  // follow the master until a new command
        BOT_COMMAND_STAY   = 3   // stand still until a new command
    };

    static PlayerBotMgr* instance();

    bool IsBot(ObjectGuid guid) const;
    void AddBot(ObjectGuid guid);
    void RemoveBot(ObjectGuid guid);
    size_t GetCount() const;
    std::set<ObjectGuid> GetAllBots() const;

    void SetMaster(ObjectGuid botGuid, ObjectGuid masterGuid);
    ObjectGuid GetMaster(ObjectGuid botGuid) const;
    void ClearMaster(ObjectGuid botGuid);
    std::string GetMasterName(ObjectGuid botGuid) const;
    bool IsMasterOf(ObjectGuid masterGuid, ObjectGuid botGuid) const;

    void SetBotStance(ObjectGuid botGuid, BotStance stance);
    BotStance GetBotStance(ObjectGuid botGuid) const;

    // Pet-style command state.
    void SetBotCommand(ObjectGuid botGuid, BotCommand cmd);
    BotCommand GetBotCommand(ObjectGuid botGuid) const;
    void ClearBotCommand(ObjectGuid botGuid) { SetBotCommand(botGuid, BOT_COMMAND_NONE); }

    // Stay position (pet SaveStayPosition): where the bot was when "stay" was
    // clicked. Used so a stay bot only fights targets that come into range and
    // does not chase.
    void SetBotStayPosition(ObjectGuid botGuid, float x, float y, float z);
    bool GetBotStayPosition(ObjectGuid botGuid, float& x, float& y, float& z) const;
    void ClearBotStayPosition(ObjectGuid botGuid);

    // Pet-style "return mode" (mirrors pet CommandState FOLLOW vs STAY for
    // HandleReturnMovement): what the bot does when it has no combat target
    // in ANY stance. false = follow the master (default); true = stay at the
    // stay position (master said "stay"). Set by the follow/stay commands,
    // used by the stance AI and after an ATTACK command target dies.
    void SetBotReturnMode(ObjectGuid botGuid, bool stayAtSpot);
    bool GetBotReturnMode(ObjectGuid botGuid) const;
    void ClearBotReturnMode(ObjectGuid botGuid);

    // Explicit target for BOT_COMMAND_ATTACK. Cleared when it dies/goes invalid.
    void SetBotAttackTarget(ObjectGuid botGuid, ObjectGuid targetGuid);
    ObjectGuid GetBotAttackTarget(ObjectGuid botGuid) const;

    // Pet-style "Auto spells": the set of class spells the master has enabled
    // for this bot to auto-cast in combat (like a pet's auto-cast bar).
    void AddBotAutoSpell(ObjectGuid botGuid, uint32 spellId);
    void RemoveBotAutoSpell(ObjectGuid botGuid, uint32 spellId);
    bool IsBotAutoSpellEnabled(ObjectGuid botGuid, uint32 spellId) const;
    std::vector<uint32> GetBotAutoSpells(ObjectGuid botGuid) const;

    // ---- Custom skill bar (pet-style, 12 slots) ----
    // Each bot has 12 skill slots, matching the player's FIRST action bar
    // (12 buttons), so bar management maps 1:1 to the real client. Slot N is
    // read straight from the bot character's action-bar button N-1 - there is
    // NO manual spell assignment (real-client behaviour). ALL slots are
    // auto-cast by default; the master can turn individual slots off with
    // /bot autospell <N> (like a pet bar).
    static constexpr uint8 MAX_SKILL_SLOTS = 12;

    // Read the skill in slot N (1-12) from the bot's own first action bar
    // (button N-1). Returns 0 for empty/non-spell/passive buttons.
    uint32 GetBotSkillSlot(Player* bot, uint8 slot) const;
    void SetBotSlotAutocast(ObjectGuid botGuid, uint8 slot, bool enable);
    bool IsBotSlotAutocast(ObjectGuid botGuid, uint8 slot) const;

    // Rotation cursor for the skill-bar auto-cast scan, so a bot cycles
    // through its action-bar skills instead of always casting the first one.
    void SetBotSkillCursor(ObjectGuid botGuid, uint32 cursor);
    uint32 GetBotSkillCursor(ObjectGuid botGuid) const;

    ObjectGuid FindPlayerByName(std::string const& name);
    // Look up a character's name. Returns the online name if the player is
    // connected, otherwise queries the characters database (offline lookup).
    std::string GetCharacterName(ObjectGuid guid) const;

    // Pet-style: return all bots currently owned by the given master.
    std::set<ObjectGuid> GetBotsByMaster(ObjectGuid masterGuid) const;

    // ---- Debug logging (AZ-style, controlled by worldserver.conf) ----
    // Reuses AzerothCore's "playerbots" log filter + LOG_INFO, gated by the
    // module's own PlayerBot.DebugLevel config (0-3, default 0 = off) so
    // production stays quiet and debugging turns on without touching the
    // core's global Log.Level.
    //   DebugLevel 1 = events      : every hook entry, state change, command
    //   DebugLevel 2 = behaviour   : + per-tick AI decisions (targets, casts)
    //   DebugLevel 3 = dump        : bot list dumps full in-memory state
    void SetDebugLevel(uint32 level);
    uint32 GetDebugLevel() const;
    static bool DebugLevelAtLeast(uint32 level);
    static bool DebugEnabled(); // level >= 1
    static bool DebugTrace();   // level >= 2
    static bool DebugDump();    // level >= 3

    // Human-readable names for the stance / command enums (logs + bot list).
    static char const* StanceName(BotStance stance);
    static char const* CommandName(BotCommand cmd);

    // Dump every in-memory state held for one bot (all maps below) into a
    // readable multi-line string. Used by `bot list` and DebugDump to debug
    // a bot from the server side.
    std::string DumpBotState(ObjectGuid botGuid) const;

private:
    PlayerBotMgr() = default;
    ~PlayerBotMgr() = default;

    uint32 _debugLevel = 0;

    std::set<ObjectGuid> _botGuids;
    std::map<ObjectGuid, ObjectGuid> _botMasters;
    std::map<ObjectGuid, BotStance> _botStances;
    std::map<ObjectGuid, BotCommand> _botCommands;
    std::map<ObjectGuid, ObjectGuid> _botAttackTargets;
    std::map<ObjectGuid, std::array<float, 3>> _botStayPositions;
    // Whether the bot should hold position (true) or follow the master (false)
    // when it has no combat target.
    std::map<ObjectGuid, bool> _botReturnModes;
    std::map<ObjectGuid, std::set<uint32>> _botAutoSpells;
    // Per-slot autocast toggles for the 12-slot bar (skills themselves are
    // read from the bot's own action bar; only autocast is stored here).
    std::map<ObjectGuid, std::array<bool, MAX_SKILL_SLOTS>> _botSlotAutocast;
    // Skill auto-cast rotation cursor per bot.
    std::map<ObjectGuid, uint32> _botSkillCursors;
};

#define sPlayerBotMgr PlayerBotMgr::instance()

#endif
