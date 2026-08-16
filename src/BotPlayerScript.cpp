#include "BotPlayerScript.h"
#include "BotGroupScript.h"
#include "PlayerBotMgr.h"
#include "Group.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Chat.h"
#include "Log.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "DBCStores.h"
#include "MotionMaster.h"
#include "PetDefines.h"

#define PLAYERBOT_VERSION "v2.1.0.2"

BotPlayerScript::BotPlayerScript() : PlayerScript("BotPlayerScript", {
    PLAYERHOOK_ON_LOGIN,
    PLAYERHOOK_ON_UPDATE,
}) { }

void BotPlayerScript::OnPlayerLogin(Player* player)
{
    if (!sPlayerBotMgr->IsBot(player->GetGUID()))
        return;

    ChatHandler handler(player->GetSession());
    handler.PSendSysMessage("|cff00ff00[Bot]|r Bot mode active.");
    handler.PSendSysMessage("|cff00ff00[Bot]|r Master: |cffff00ff{}|r", 
        sPlayerBotMgr->GetMasterName(player->GetGUID()).c_str());
    handler.PSendSysMessage("|cff00ff00[Bot]|r Stance: |cffff00ffDefensive|r (use /bot stance to change)");

    LOG_INFO("playerbots", "[{}] Bot '{}' logged in", PLAYERBOT_VERSION, player->GetName());
}

void BotPlayerScript::OnPlayerUpdate(Player* player, uint32 p_time)
{
    if (!sPlayerBotMgr->IsBot(player->GetGUID()))
        return;

    _updateTimer += p_time;
    if (_updateTimer < 500)
        return;
    _updateTimer = 0;

    ObjectGuid masterGuid = sPlayerBotMgr->GetMaster(player->GetGUID());
    Group* group = player->GetGroup();
    bool inGroup = group && group->IsMember(player->GetGUID());

    if (!inGroup)
    {
        if (!masterGuid.IsEmpty())
        {
            sPlayerBotMgr->ClearMaster(player->GetGUID());
            ResetCombatState(player);
        }
        return;
    }

    if (masterGuid.IsEmpty())
    {
        ObjectGuid leaderGuid = group->GetLeaderGUID();
        if (!leaderGuid.IsEmpty())
        {
            sPlayerBotMgr->SetMaster(player->GetGUID(), leaderGuid);
            masterGuid = leaderGuid;
        }
        else
            return;
    }

    bool masterInGroup = group && group->IsMember(masterGuid);
    Player* master = ObjectAccessor::FindConnectedPlayer(masterGuid);

    if (!masterInGroup)
    {
        LeaveGroupAndClearMaster(player, "Master left the group.");
        return;
    }

    if (!master || !master->IsInWorld())
    {
        if (_disconnectTimer.find(player->GetGUID()) == _disconnectTimer.end())
        {
            _disconnectTimer[player->GetGUID()] = 300000;
            ChatHandler handler(player->GetSession());
            handler.PSendSysMessage("|cffffff00[Bot]|r Master disconnected. Waiting 5 minutes...");
        }
        else
        {
            _disconnectTimer[player->GetGUID()] -= p_time;
            if (_disconnectTimer[player->GetGUID()] <= 0)
            {
                LeaveGroupAndClearMaster(player, "Master disconnected timeout.");
                _disconnectTimer.erase(player->GetGUID());
                return;
            }
        }
        if (player->IsInCombat())
            player->CombatStop(true);
        if (player->GetMotionMaster()->GetCurrentMovementGeneratorType() != FOLLOW_MOTION_TYPE)
        {
            player->GetMotionMaster()->MoveFollow(master ? master : player, 
                GetFollowDistance(player), GetFollowAngle(player));
        }
        return;
    }

    _disconnectTimer.erase(player->GetGUID());

    if (!master->IsAlive())
    {
        if (player->IsInCombat())
            player->CombatStop(true);
        if (player->GetMotionMaster()->GetCurrentMovementGeneratorType() != FOLLOW_MOTION_TYPE)
        {
            player->GetMotionMaster()->MoveFollow(master, GetFollowDistance(player), GetFollowAngle(player));
        }
        return;
    }

    ObjectGuid currentLeader = group->GetLeaderGUID();
    if (masterGuid != currentLeader)
    {
        Player* newLeader = ObjectAccessor::FindConnectedPlayer(currentLeader);
        if (newLeader && newLeader->IsAlive())
        {
            sPlayerBotMgr->SetMaster(player->GetGUID(), currentLeader);
            masterGuid = currentLeader;
            master = newLeader;
            ChatHandler handler(player->GetSession());
            handler.PSendSysMessage("|cff00ff00[Bot]|r Master switched to new leader |cffff00ff{}|r.", 
                newLeader->GetName());
        }
    }

    if (!player->IsAlive())
        return;

    // Cross-map follow: teleport to the master when they change map or instance.
    if (player->GetMapId() != master->GetMapId() || player->GetInstanceId() != master->GetInstanceId())
    {
        MapEntry const* masterMapEntry = sMapStore.LookupEntry(master->GetMapId());
        if (masterMapEntry && !masterMapEntry->IsBattlegroundOrArena() &&
            !player->IsBeingTeleported() && !master->IsBeingTeleported())
        {
            player->TeleportTo(master->GetMapId(),
                master->GetPositionX(), master->GetPositionY(), master->GetPositionZ(), master->GetOrientation());
        }
        return;
    }

    if (player->HasUnitState(UNIT_STATE_STUNNED) || 
        player->HasUnitState(UNIT_STATE_FLEEING) ||
        player->HasUnitState(UNIT_STATE_CONTROLLED))
        return;

    PlayerBotMgr::BotStance stance = sPlayerBotMgr->GetBotStance(player->GetGUID());

    if (stance == PlayerBotMgr::STANCE_PASSIVE)
    {
        if (player->IsInCombat() && player->GetVictim())
        {
            Unit* attacker = player->GetVictim();
            if (attacker && attacker->IsAlive())
            {
                DoCombat(player, attacker);
                return;
            }
        }
        if (player->IsInCombat())
            player->CombatStop(true);
        if (master && player->GetMotionMaster()->GetCurrentMovementGeneratorType() != FOLLOW_MOTION_TYPE)
        {
            player->GetMotionMaster()->MoveFollow(master, GetFollowDistance(player), GetFollowAngle(player));
        }
        return;
    }

    if (stance == PlayerBotMgr::STANCE_DEFENSIVE)
    {
        if (player->IsInCombat() && player->GetVictim())
        {
            Unit* attacker = player->GetVictim();
            if (attacker && attacker->IsAlive())
            {
                DoCombat(player, attacker);
                return;
            }
        }
        if (master && master->GetVictim())
        {
            Unit* masterTarget = master->GetVictim();
            if (masterTarget && masterTarget->IsAlive() && player->IsValidAttackTarget(masterTarget))
            {
                DoCombat(player, masterTarget);
                return;
            }
        }
        if (master && player->GetMotionMaster()->GetCurrentMovementGeneratorType() != FOLLOW_MOTION_TYPE)
        {
            player->GetMotionMaster()->MoveFollow(master, GetFollowDistance(player), GetFollowAngle(player));
        }
        return;
    }

    if (stance == PlayerBotMgr::STANCE_AGGRESSIVE)
    {
        if (master && master->GetVictim())
        {
            Unit* masterTarget = master->GetVictim();
            if (masterTarget && masterTarget->IsAlive() && player->IsValidAttackTarget(masterTarget))
            {
                DoCombat(player, masterTarget);
                return;
            }
        }
        if (master && player->GetMotionMaster()->GetCurrentMovementGeneratorType() != FOLLOW_MOTION_TYPE)
        {
            player->GetMotionMaster()->MoveFollow(master, GetFollowDistance(player), GetFollowAngle(player));
        }
        return;
    }
}

void BotPlayerScript::LeaveGroupAndClearMaster(Player* player, const char* reason)
{
    Group* group = player->GetGroup();
    if (group)
    {
        if (group->IsLeader(player->GetGUID()))
        {
            for (auto const& slot : group->GetMemberSlots())
            {
                if (slot.guid != player->GetGUID())
                {
                    group->ChangeLeader(slot.guid);
                    break;
                }
            }
        }
        group->RemoveMember(player->GetGUID());
    }

    sPlayerBotMgr->ClearMaster(player->GetGUID());
    ResetCombatState(player);

    ChatHandler handler(player->GetSession());
    handler.PSendSysMessage("|cff00ff00[Bot]|r %s Auto-leaving group.", reason);
    LOG_INFO("playerbots", "[{}] Bot '{}' left group: {}", 
        PLAYERBOT_VERSION, player->GetName(), reason);
}

void BotPlayerScript::ResetCombatState(Player* bot)
{
    if (bot->IsInCombat())
        bot->CombatStop(true);
    if (bot->GetMotionMaster()->GetCurrentMovementGeneratorType() == CHASE_MOTION_TYPE)
        bot->GetMotionMaster()->MoveIdle();
}

bool BotPlayerScript::IsMeleeClass(Player* player)
{
    if (!player)
        return false;
    uint8 cls = player->getClass();
    switch (cls)
    {
        case CLASS_WARRIOR:
        case CLASS_ROGUE:
        case CLASS_PALADIN:
        case CLASS_DEATH_KNIGHT:
            return true;
        case CLASS_SHAMAN:
            return !player->GetWeaponForAttack(RANGED_ATTACK, true);
        case CLASS_DRUID:
            return (player->GetShapeshiftForm() == FORM_BEAR || 
                    player->GetShapeshiftForm() == FORM_CAT);
        default:
            return false;
    }
}

float BotPlayerScript::GetAttackRange(Player* player)
{
    if (!player)
        return 3.0f;
    uint8 cls = player->getClass();
    switch (cls)
    {
        case CLASS_MAGE:
        case CLASS_WARLOCK:
        case CLASS_PRIEST:
            return 30.0f;
        case CLASS_HUNTER:
            return 35.0f;
        case CLASS_SHAMAN:
            return player->GetWeaponForAttack(RANGED_ATTACK, true) ? 30.0f : 3.0f;
        case CLASS_DRUID:
            if (player->GetShapeshiftForm() == FORM_BEAR || 
                player->GetShapeshiftForm() == FORM_CAT)
                return 3.0f;
            return 30.0f;
        default:
            return 3.0f;
    }
}

float BotPlayerScript::GetFollowAngle(Player* bot)
{
    if (!bot)
        return PET_FOLLOW_ANGLE;

    // Spread multiple bots across an arc behind the master so they don't stack on the same spot.
    uint32 counter = bot->GetGUID().GetCounter();
    float t = static_cast<float>(counter % 7) / 6.0f;  // 0.0 .. 1.0
    return M_PI / 6.0f + t * (2.0f * M_PI / 3.0f);     // 30° .. 150° arc behind the master
}

float BotPlayerScript::GetFollowDistance(Player* bot)
{
    if (!bot)
        return PET_FOLLOW_DIST;

    uint32 counter = bot->GetGUID().GetCounter();
    return PET_FOLLOW_DIST + 0.5f * static_cast<float>((counter / 7) % 3); // 1.0 / 1.5 / 2.0
}

bool BotPlayerScript::CanCastSpell(Player* bot, Unit* target, uint32 spellId)
{
    if (!bot || !target || spellId == 0)
        return false;
    if (!bot->HasSpell(spellId))
        return false;
    if (bot->HasSpellCooldown(spellId))
        return false;
    if (!bot->IsValidAttackTarget(target))
        return false;
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return false;
    float range = spellInfo->GetMaxRange();
    if (bot->GetDistance(target) > range)
        return false;
    return true;
}

uint32 BotPlayerScript::GetTalentPointsInTree(Player* player, uint32 treeId)
{
    uint32 count = 0;
    for (uint32 i = 0; i < sTalentStore.GetNumRows(); ++i)
    {
        TalentEntry const* talent = sTalentStore.LookupEntry(i);
        if (!talent)
            continue;
        if (talent->TalentTab != treeId)
            continue;
        for (uint8 rank = 0; rank < MAX_TALENT_RANK; ++rank)
        {
            if (talent->RankID[rank] && player->HasTalent(talent->RankID[rank], player->GetActiveSpec()))
                count++;
        }
    }
    return count;
}

bool BotPlayerScript::IsTalentTreeDominant(Player* player, uint32 treeId)
{
    uint32 points = GetTalentPointsInTree(player, treeId);
    uint32 total = 0;
    for (uint32 i = 0; i < sTalentStore.GetNumRows(); ++i)
    {
        TalentEntry const* talent = sTalentStore.LookupEntry(i);
        if (talent)
        {
            for (uint8 rank = 0; rank < MAX_TALENT_RANK; ++rank)
            {
                if (talent->RankID[rank] && player->HasTalent(talent->RankID[rank], player->GetActiveSpec()))
                    total++;
            }
        }
    }
    if (total == 0)
        return false;
    return (points * 100 / total) >= 50;
}

float BotPlayerScript::CalculateAverageItemLevel(Player* player)
{
    float total = 0.0f;
    uint32 count = 0;
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (item)
        {
            total += item->GetTemplate()->ItemLevel;
            count++;
        }
    }
    return count > 0 ? (total / count) : 0.0f;
}

extern void AddBotCommandScripts();

void AddPlayerBotScripts()
{
    LOG_INFO("playerbots", "[{}] Registering PlayerBot scripts...", PLAYERBOT_VERSION);
    new BotPlayerScript();
    new BotGroupScript();
    AddBotCommandScripts();
    LOG_INFO("playerbots", "[{}] PlayerBot scripts registered successfully!", PLAYERBOT_VERSION);
}
