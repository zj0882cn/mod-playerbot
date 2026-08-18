#ifndef _BOT_PLAYER_SCRIPT_H_
#define _BOT_PLAYER_SCRIPT_H_

#include "PlayerScript.h"
#include "ObjectGuid.h"
#include <map>

class Player;
class Unit;
class Creature;
class SpellInfo;

class BotPlayerScript : public PlayerScript
{
public:
    BotPlayerScript();

    // PlayerScript hook names differ across AzerothCore versions:
    //   older cores: OnLogin/OnUpdate
    //   newer cores: OnPlayerLogin/OnPlayerUpdate
    // CMake auto-detects the core and defines PLAYERBOT_NEW_PLAYERSCRIPT accordingly.
#ifdef PLAYERBOT_NEW_PLAYERSCRIPT
    void OnPlayerLogin(Player* player) override;
    void OnPlayerUpdate(Player* player, uint32 p_time) override;
#else
    void OnLogin(Player* player) override;
    void OnUpdate(Player* player, uint32 p_time) override;
    // Called the instant the bot's ATTACK-command target dies (mirrors
    // PetAI::KilledUnit): clear the command and return to follow immediately
    // instead of waiting for the next AI tick to notice the target is gone.
    void OnCreatureKill(Player* killer, Creature* killed) override;
#endif

    // Pet-style command execution mirroring PetAI::AttackStart/DoAttack:
    // immediately attacks the target (handles melee/ranged via Player::Attack
    // + MoveChase). Returns false if the target is not attackable.
    static bool ExecuteBotAttack(Player* bot, Unit* target);
    // Pet-style follow: immediately follow the master, spreading multiple bots
    // across an arc behind him (same spread as UpdateFollow). Static so the
    // command script can trigger it instantly.
    static bool ExecuteBotFollow(Player* bot, Player* master);

private:
    // Combat driver. chase=false mirrors PetAI::DoAttack(target, chase): the
    // bot fights targets inside its attack range (melee range for melee,
    // preferred distance for ranged/hunter) but never chases - target out of
    // range returns to the stay spot (pet STAY behaviour).
    void DoCombat(Player* bot, Unit* target, bool chase = true);
    void ResetCombatState(Player* bot);
    void UpdateFollow(Player* bot, Player* master);
    // Pet-style return after combat with no target: follow the master, or if
    // the master said "stay" hold the stay position instead (mirrors pet
    // HandleReturnMovement honoring CommandState FOLLOW vs STAY).
    void ReturnToPost(Player* bot, Player* master);
    // Public so the command script can execute pet-style commands immediately
    // (rather than only setting a state that the update tick later handles).
    void ExecuteBotCombat(Player* bot, Unit* target);
    Unit* GetMasterAttackTarget(Player* master);
    // Pet REACT_AGGRESSIVE target selection: scan the grid around the bot and
    // return the nearest valid hostile unit (mirrors the Creature-only API
    // Creature::SelectNearestTargetInAttackDistance). Returns nullptr if none.
    Unit* SelectNearestAttackTarget(Player* bot, float range);
    // Pet-style next-target selection, mirroring PetAI::SelectNextTarget order:
    // 1) my attacker -> retaliate; 2) master's attacker -> defend;
    // 3) master's target -> assist; 4) allowAutoSelect(aggressive) -> nearest.
    // Returns nullptr when there is no target (bot then follows/idles).
    Unit* SelectBotTarget(Player* bot, Player* master, bool allowAutoSelect);
    bool CanCastSpell(Player* bot, Unit* target, uint32 spellId);
    // Like CanCastSpell but for a FRIENDLY target (buff/heal): CanCastSpell
    // requires an attackable target, which a friendly cast would fail on.
    bool CanCastFriendlySpell(Player* bot, Unit* target, uint32 spellId);
    // True when the spell's implicit target types are self/ally/party - i.e. a
    // buff or heal that must NOT be cast on the combat target (real-client
    // behaviour: a priest does not throw Renew at a mob).
    bool IsFriendlyTargetSpell(SpellInfo const* spellInfo);
    // Choose who a friendly-target spell is cast on: the bot itself for self /
    // self-centred-party-AOE buffs, the master for master-targeted spells, and
    // otherwise the lowest-health live group member (or the bot) for
    // single-target heals/buffs.
    Unit* GetFriendlyCastTarget(Player* bot, SpellInfo const* spellInfo);
    // Return the highest rank of the given spell chain that the bot actually
    // knows (0 if the bot knows none of them). Lets combat use the bot's real
    // skills instead of a hard-coded low-rank spell id.
    uint32 GetKnownSpell(Player* bot, uint32 baseSpellId);
    // Pet-style auto-cast: iterate the bot's class spell set and cast the
    // highest known rank of any that is usable on the target. Returns true if
    // a spell was cast this tick.
    bool CastAutoSpells(Player* bot, Unit* target);
    bool IsMeleeClass(Player* player);
    float GetAttackRange(Player* player);
    float GetFollowAngle(Player* bot);
    float GetFollowDistance(Player* bot);
    bool IsTalentTreeDominant(Player* player, uint32 treeId);
    static uint32 GetTalentPointsInTree(Player* player, uint32 treeId);
    static float CalculateAverageItemLevel(Player* player);

    struct BotRoleAnalysis
    {
        enum Role
        {
            ROLE_TANK,
            ROLE_HEALER,
            ROLE_MELEE_DPS,
            ROLE_RANGED_DPS,
            ROLE_UNKNOWN
        } role;

        bool hasShield;
        bool hasRangedWeapon;
        float preferDistance;
        bool useAggressive;
        bool useInterrupts;
        float averageItemLevel;

        uint32 talentTree1;
        uint32 talentTree2;
        uint32 talentTree3;
    };

    static BotRoleAnalysis AnalyzeBot(Player* bot);
    // Cached role analysis. AnalyzeBot() is heavy (scans talents + gear), so it
    // is cached per bot and refreshed ~1s in OnUpdate (like PetAI's
    // low-frequency housekeeping timer); combat uses the cache every tick
    // instead of re-scanning each tick.
    BotRoleAnalysis const& GetBotAnalysis(Player* bot);
    void DoTankCombat(Player* bot, Unit* target, BotRoleAnalysis const& analysis);
    void DoHealerCombat(Player* bot, Unit* target, BotRoleAnalysis const& analysis);
    void DoMeleeDPSCombat(Player* bot, Unit* target, BotRoleAnalysis const& analysis);
    void DoRangedDPSCombat(Player* bot, Unit* target, BotRoleAnalysis const& analysis);

    void LeaveGroupAndClearMaster(Player* player, const char* reason);

    // Low-frequency housekeeping timer (mirrors PetAI::m_updateAlliesTimer):
    // drives the role-analysis cache refresh (~1s).
    uint32 _aiTimer = 0;
    std::map<ObjectGuid, uint32> _disconnectTimer;
    ObjectGuid _followMaster; // last master we are following; forces re-follow on change
    // Role analysis cache (AnalyzeBot results), refreshed ~1s.
    std::map<ObjectGuid, BotRoleAnalysis> _botRoleCache;
};

#endif
