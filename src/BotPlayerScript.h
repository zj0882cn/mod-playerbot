#ifndef _BOT_PLAYER_SCRIPT_H_
#define _BOT_PLAYER_SCRIPT_H_

#include "PlayerScript.h"
#include "ObjectGuid.h"
#include <map>

class Player;
class Unit;

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
#endif

private:
    void DoCombat(Player* bot, Unit* target);
    void ResetCombatState(Player* bot);
    bool CanCastSpell(Player* bot, Unit* target, uint32 spellId);
    bool IsMeleeClass(Player* player);
    float GetAttackRange(Player* player);
    float GetFollowAngle(Player* bot);
    float GetFollowDistance(Player* bot);
    bool IsTalentTreeDominant(Player* player, uint32 treeId);
    uint32 GetTalentPointsInTree(Player* player, uint32 treeId);
    float CalculateAverageItemLevel(Player* player);

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

    BotRoleAnalysis AnalyzeBot(Player* bot);
    void DoTankCombat(Player* bot, Unit* target, BotRoleAnalysis const& analysis);
    void DoHealerCombat(Player* bot, Unit* target, BotRoleAnalysis const& analysis);
    void DoMeleeDPSCombat(Player* bot, Unit* target, BotRoleAnalysis const& analysis);
    void DoRangedDPSCombat(Player* bot, Unit* target, BotRoleAnalysis const& analysis);

    void LeaveGroupAndClearMaster(Player* player, const char* reason);

    uint32 _updateTimer = 0;
    std::map<ObjectGuid, uint32> _disconnectTimer;
};

#endif
