#include "BotPlayerScript.h"
#include "BotGroupScript.h"
#include "BotCommon.h"
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
#include "CharmInfo.h"
#include "GridNotifiers.h"
#include "CellImpl.h"
#include "Config.h"
#include <list>

#define PLAYERBOT_VERSION "v2.2.0"
// Git commit this source corresponds to. Injected by mod-playerbot.cmake at
// (re)configure time (git rev-parse --short HEAD). Fallback if not injected.
#ifndef PLAYERBOT_REV
#define PLAYERBOT_REV "unknown"
#endif
// Compile date/time (auto-embedded at build) - visible in the worldserver
// startup log so the server admin can verify the running binary is current.
#define PLAYERBOT_BUILD __DATE__ " " __TIME__

BotPlayerScript::BotPlayerScript() : PlayerScript("BotPlayerScript", {
    PLAYERHOOK_ON_LOGIN,
    PLAYERHOOK_ON_UPDATE,
    PLAYERHOOK_ON_CREATURE_KILL,
}) { }

#ifdef PLAYERBOT_NEW_PLAYERSCRIPT
void BotPlayerScript::OnPlayerLogin(Player* player)
#else
void BotPlayerScript::OnLogin(Player* player)
#endif
{
    if (!sPlayerBotMgr->IsBot(player->GetGUID()))
        return;

    BotNotify(player, "|cff00ff00[Bot]|r Bot mode active.");
    BotNotify(player, "|cff00ff00[Bot]|r Master: |cffff00ff{}|r", 
        sPlayerBotMgr->GetMasterName(player->GetGUID()).c_str());
    BotNotify(player, "|cff00ff00[Bot]|r Stance: |cffff00ffDefensive|r (use /bot stance to change)");

    LOG_INFO("playerbots", "[{}] Bot '{}' logged in", PLAYERBOT_VERSION, player->GetName());
    PB_LOG(1, "Hook OnLogin: bot '{}' (master '{}', stance {})",
        player->GetName(),
        sPlayerBotMgr->GetMasterName(player->GetGUID()).c_str(),
        sPlayerBotMgr->StanceName(sPlayerBotMgr->GetBotStance(player->GetGUID())));
}

#ifdef PLAYERBOT_NEW_PLAYERSCRIPT
void BotPlayerScript::OnPlayerUpdate(Player* player, uint32 p_time)
#else
void BotPlayerScript::OnUpdate(Player* player, uint32 p_time)
#endif
{
    if (!sPlayerBotMgr->IsBot(player->GetGUID()))
        return;

    // Pet-style AI: run the core every world tick (like PetAI::UpdateAI runs
    // every AI tick). Only the heavy AnalyzeBot role scan is throttled to ~1s
    // (mirrors PetAI::m_updateAlliesTimer); everything else - command state,
    // stance reaction, target selection, combat, auto-cast - runs every tick
    // so the bot responds as fast as a pet.
    _aiTimer += p_time;
    if (_aiTimer >= 1000)
    {
        _aiTimer = 0;
        _botRoleCache[player->GetGUID()] = AnalyzeBot(player);
    }

    ObjectGuid masterGuid = sPlayerBotMgr->GetMaster(player->GetGUID());
    Group* group = player->GetGroup();
    bool inGroup = group && group->IsMember(player->GetGUID());

    if (!inGroup)
    {
        if (!masterGuid.IsEmpty())
        {
            PB_LOG(1, "Bot '{}' no longer in a group - clearing master + combat", player->GetName());
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
        // In the group-invite flow the leader may not have been added to the
        // member list yet (leader is only AddInvite'd until the invite is
        // accepted). Don't treat a leader-master as having left the group.
        if (!(group && group->GetLeaderGUID() == masterGuid))
        {
            LeaveGroupAndClearMaster(player, "Master left the group.");
            return;
        }
    }

    if (!master || !master->IsInWorld())
    {
        // Master is temporarily out of the world (e.g. teleporting between
        // maps). Don't treat this as a disconnect - instead, follow the
        // master's teleport so the bot moves with them.
        if (master && master->IsBeingTeleported())
        {
            if (player->IsInCombat())
                player->CombatStop(true);

            // Master is teleporting: teleport the bot to the same destination.
            // GetTeleportDest() already holds the target before the semaphore
            // flag is set, so we can reliably follow the master across maps.
            if (!player->IsBeingTeleported())
            {
                WorldLocation const& dest = master->GetTeleportDest();
                PB_LOG(2, "Bot '{}' teleporting to follow master (dest map {})", player->GetName(), dest.GetMapId());
                player->TeleportTo(dest);
            }
            return;
        }

        if (_disconnectTimer.find(player->GetGUID()) == _disconnectTimer.end())
        {
            _disconnectTimer[player->GetGUID()] = 300000;
            PB_LOG(1, "Bot '{}' master disconnected - waiting 5 min", player->GetName());
            BotNotify(player, "|cffffff00[Bot]|r Master disconnected. Waiting 5 minutes...");
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
        UpdateFollow(player, master);
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
            PB_LOG(1, "Bot '{}' master switched to new leader '{}'", player->GetName(), newLeader->GetName());
            BotNotify(player, "|cff00ff00[Bot]|r Master switched to new leader |cffff00ff{}|r.", 
                newLeader->GetName());
        }
    }

    if (!player->IsAlive())
    {
        // Bot died - auto-resurrect so it can keep serving its master.
        // Release spirit to the graveyard first, then resurrect there with
        // full health, so the bot doesn't instantly die again at the same spot.
        if (master && master->IsInWorld() && !master->IsBeingTeleported())
        {
            if (!player->HasCorpse())
            {
                player->BuildPlayerRepop(); // create corpse, become ghost
                player->RepopAtGraveyard(); // move ghost to nearest graveyard
            }
            PB_LOG(1, "Bot '{}' auto-revived at graveyard", player->GetName());
            player->ResurrectPlayer(1.0f);
            player->SpawnCorpseBones();
            BotNotify(player, "|cff00ff00[Bot]|r Revived at graveyard.");
        }
        return;
    }

    // Cross-map follow: teleport to the master when they change map or instance.
    if (player->GetMapId() != master->GetMapId() || player->GetInstanceId() != master->GetInstanceId())
    {
        MapEntry const* masterMapEntry = sMapStore.LookupEntry(master->GetMapId());
        if (masterMapEntry && !masterMapEntry->IsBattlegroundOrArena() &&
            !player->IsBeingTeleported() && !master->IsBeingTeleported())
        {
            PB_LOG(2, "Bot '{}' cross-map teleport to master (map {} -> {})",
                player->GetName(), player->GetMapId(), master->GetMapId());
            player->TeleportTo(master->GetMapId(),
                master->GetPositionX(), master->GetPositionY(), master->GetPositionZ(), master->GetOrientation());
        }
        return;
    }

    // =====================================================================
    // 宠物机制（最简版，P-015）：bot 认主且 master 在线 → charm bot，
    // master 客户端显示宠物条（攻击/跟随/停留/姿态），按钮经
    // CMSG_PET_ACTION → HandlePetActionHelper 的原生 charmed-player 逻辑控制 bot。
    // bot 被 charm 后走下方 IsCharmed() 分支让位，mod-playerbot AI 不干预。
    // =====================================================================
    if (!player->IsCharmed())
    {
        master->SetCharm(player, true);
        if (!player->GetCharmInfo())
        {
            player->InitCharmInfo();
            player->GetCharmInfo()->InitCharmCreateSpells();
        }
        if (master->ToPlayer())
            master->ToPlayer()->CharmSpellInitialize();
        PB_LOG(1, "Bot '{}' charmed by master '{}' (pet-style bar)",
            player->GetName(), master->GetName());
    }

    if (player->HasUnitState(UNIT_STATE_STUNNED) || 
        player->HasUnitState(UNIT_STATE_FLEEING) ||
        player->HasUnitState(UNIT_STATE_CONTROLLED))
        return;

    // =====================================================================
    // 宠物动作条控制（参考宠物条显示机制，bot 用 mod-playerbot AI 执行）：
    // bot 被 charm 仅用于 master 客户端显示宠物条 + 接收 CMSG_PET_ACTION；
    // master 点按钮 → 原生 HandlePetActionHelper 设置 CharmInfo 命令状态，
    // 这里读取【master 命令条】状态映射到 mod-playerbot 命令执行
    // （保留施法/技能 AI，不退化原生宠物）——一个命令条控制全体 bot。
    // =====================================================================
    if (player->IsCharmed())
    {
        Unit* barUnit = master->GetFirstControlled();
        CharmInfo* bar = barUnit ? barUnit->GetCharmInfo() : nullptr;
        if (bar)
        {
            // 攻击命令：攻击 master 当前目标
            if (bar->IsCommandAttack())
            {
                Unit* target = GetMasterAttackTarget(master);
                if (target && target->IsAlive() && player->IsValidAttackTarget(target))
                {
                    sPlayerBotMgr->SetBotAttackTarget(player->GetGUID(), target->GetGUID());
                    DoCombat(player, target);
                }
                return;
            }
            // 停留命令
            if (bar->GetCommandState() == COMMAND_STAY)
            {
                sPlayerBotMgr->SetBotCommand(player->GetGUID(), PlayerBotMgr::BOT_COMMAND_STAY);
                return;
            }
            // 姿态（被动/防御/主动）
            ReactStates react = bar->GetPlayerReactState();
            if (react == REACT_PASSIVE)
                sPlayerBotMgr->SetBotStance(player->GetGUID(), PlayerBotMgr::STANCE_PASSIVE);
            else if (react == REACT_AGGRESSIVE)
                sPlayerBotMgr->SetBotStance(player->GetGUID(), PlayerBotMgr::STANCE_AGGRESSIVE);
            else
                sPlayerBotMgr->SetBotStance(player->GetGUID(), PlayerBotMgr::STANCE_DEFENSIVE);
            // 默认跟随
            sPlayerBotMgr->SetBotCommand(player->GetGUID(), PlayerBotMgr::BOT_COMMAND_FOLLOW);
            UpdateFollow(player, master);
        }
        return;
    }

    // =====================================================================
    // Pet-style state machine (mirrors a hunter pet):
    //   1) Command state (highest priority): Attack / Follow / Stay
    //   2) React state (stance): Passive / Defensive / Aggressive
    // Commands override the react AI until they finish, then the bot returns
    // to the react (stance) behaviour.
    // =====================================================================
    PlayerBotMgr::BotCommand command = sPlayerBotMgr->GetBotCommand(player->GetGUID());

    if (command == PlayerBotMgr::BOT_COMMAND_STAY)
    {
        PB_LOG(2, "Bot '{}' STAY: holding position (stance {})",
            player->GetName(), sPlayerBotMgr->StanceName(sPlayerBotMgr->GetBotStance(player->GetGUID())));
        // Pet-style stay: stand still at the recorded position. Only attack a
        // target that comes into this class's attack range (melee range for
        // melee, casting/Auto Shot range for ranged and hunters) - never chase.
        // Mirror PetAI::CanAttack: a passive bot under STAY never fights back
        // (passive only fights on an explicit attack command, which runs via
        // the ATTACK command state instead).
        PlayerBotMgr::BotStance stayStance = sPlayerBotMgr->GetBotStance(player->GetGUID());
        bool stayPassive = (stayStance == PlayerBotMgr::STANCE_PASSIVE);
        if (!stayPassive && player->IsInCombat() && player->GetVictim() && player->GetVictim()->IsAlive())
        {
            // DoCombat(chase=false) fights inside the attack range and returns
            // the bot to the stay spot when the target goes out of range.
            DoCombat(player, player->GetVictim(), false);
            return;
        }
        // Not in range (no target, or passive): stop combat and hold position.
        if (player->IsInCombat())
            player->CombatStop(true);
        float sx, sy, sz;
        if (sPlayerBotMgr->GetBotStayPosition(player->GetGUID(), sx, sy, sz))
        {
            if (player->GetDistance(sx, sy, sz) > 1.0f)
            {
                player->GetMotionMaster()->Clear();
                player->GetMotionMaster()->MovePoint(player->GetGUID().GetCounter(), sx, sy, sz);
            }
        }
        else
        {
            player->GetMotionMaster()->Clear();
            player->GetMotionMaster()->MoveIdle();
        }
        return;
    }

    if (command == PlayerBotMgr::BOT_COMMAND_ATTACK)
    {
        // Pet-style attack: attack the explicit target until it dies or is
        // lost, then clear the command and FALL THROUGH to the react (stance)
        // AI - the stance re-selects a target if there is one, and only
        // returns to follow / stay spot when nothing is left to fight
        // (mirrors PetAI::KilledUnit: SelectNextTarget, else HandleReturnMovement).
        ObjectGuid attackTargetGuid = sPlayerBotMgr->GetBotAttackTarget(player->GetGUID());
        if (attackTargetGuid.IsEmpty())
        {
            sPlayerBotMgr->ClearBotCommand(player->GetGUID());
        }
        else
        {
            Unit* attackTarget = ObjectAccessor::GetUnit(*player, attackTargetGuid);
            if (!attackTarget || !attackTarget->IsAlive() ||
                !player->IsValidAttackTarget(attackTarget))
            {
                PB_LOG(2, "Bot '{}' ATTACK target {} gone/invalid - clearing attack command",
                    player->GetName(), attackTargetGuid.ToString());
                sPlayerBotMgr->SetBotAttackTarget(player->GetGUID(), ObjectGuid::Empty);
                sPlayerBotMgr->ClearBotCommand(player->GetGUID());
                if (player->IsInCombat())
                    player->CombatStop(true);
                // Fall through to the stance AI below: it re-selects a target
                // (stance-based) and only follows/idles when there is none.
            }
            else
            {
                DoCombat(player, attackTarget);
                return;
            }
        }
    }

    if (command == PlayerBotMgr::BOT_COMMAND_FOLLOW)
    {
        // Follow the master until a new command is given.
        PB_LOG(2, "Bot '{}' FOLLOW: following master '{}'", player->GetName(), master->GetName());
        if (player->IsInCombat())
            player->CombatStop(true);
        UpdateFollow(player, master);
        return;
    }

    // ---- React (stance) AI ----
    PlayerBotMgr::BotStance stance = sPlayerBotMgr->GetBotStance(player->GetGUID());

    if (stance == PlayerBotMgr::STANCE_PASSIVE)
    {
        // Pet REACT_PASSIVE: never fights on its own - no retaliation, no
        // assisting the master, no auto target. It only fights on an explicit
        // attack command (handled by the command state above).
        PB_LOG(2, "Bot '{}' PASSIVE: no auto fight - returning to post", player->GetName());
        if (player->IsInCombat())
            player->CombatStop(true);
        ReturnToPost(player, master);
        return;
    }

    if (stance == PlayerBotMgr::STANCE_DEFENSIVE || stance == PlayerBotMgr::STANCE_AGGRESSIVE)
    {
        // Pet-style target selection: act on the LOCKED target (GetVictim)
        // until it dies/is lost, then pick a new one. The current target is
        // the highest priority - we do NOT switch to retaliate or to follow
        // someone else's target while ours is alive (that made combat erratic).
        // 1) Current locked target first - keep fighting it while alive.
        Unit* target = player->GetVictim();
        if (!target || !target->IsAlive() || !player->IsValidAttackTarget(target))
            target = nullptr;
        // 2) Target gone -> select a new one (PetAI::SelectNextTarget order).
        if (!target)
            target = SelectBotTarget(player, master, stance == PlayerBotMgr::STANCE_AGGRESSIVE);
        if (target)
        {
            // returnMode: follow -> chase the target; stay -> hold position
            // and only fight inside this class's attack range (melee range,
            // or casting/Auto Shot range for ranged and hunters), never chase.
            bool holdPosition = sPlayerBotMgr->GetBotReturnMode(player->GetGUID());
            PB_LOG(2, "Bot '{}' {}: combat target '{}' (hold={})",
                player->GetName(), sPlayerBotMgr->StanceName(stance),
                target->GetName(), holdPosition);
            DoCombat(player, target, !holdPosition);
            return;
        }
        // No target: return to the stay spot or follow, per the master's
        // return-mode (mirrors pet HandleReturnMovement).
        PB_LOG(2, "Bot '{}' {}: no target - returning to post",
            player->GetName(), sPlayerBotMgr->StanceName(stance));
        ReturnToPost(player, master);
        return;
    }
}

// Pet-style return after combat with no target: follow the master, or if the
// master said "stay", hold the stay position instead (mirrors pet
// HandleReturnMovement honoring CommandState FOLLOW vs STAY).
void BotPlayerScript::ReturnToPost(Player* bot, Player* master)
{
    if (!bot)
        return;
    PB_LOG(2, "Bot '{}' ReturnToPost (mode {})",
        bot->GetName(), sPlayerBotMgr->GetBotReturnMode(bot->GetGUID()) ? "stay" : "follow");
    if (bot->IsInCombat())
        bot->CombatStop(true);
    if (sPlayerBotMgr->GetBotReturnMode(bot->GetGUID()))
    {
        // Hold position: return to the stay spot (or stand still).
        float sx, sy, sz;
        if (sPlayerBotMgr->GetBotStayPosition(bot->GetGUID(), sx, sy, sz))
        {
            if (bot->GetDistance(sx, sy, sz) > 1.0f)
            {
                bot->GetMotionMaster()->Clear();
                bot->GetMotionMaster()->MovePoint(bot->GetGUID().GetCounter(), sx, sy, sz);
            }
        }
        else
        {
            bot->GetMotionMaster()->Clear();
            bot->GetMotionMaster()->MoveIdle();
        }
    }
    else
    {
        UpdateFollow(bot, master);
    }
}

// Pet-style next-target selection, mirroring PetAI::SelectNextTarget exactly
// (only called when the current locked target is dead/gone):
//  1) my attacker -> retaliate (pet checks this first so it doesn't drag a
//     bunch of targets back to the owner)
//  2) master's attacker -> defend the master
//  3) master's victim/target -> assist
//  4) allowAutoSelect (aggressive) -> nearest hostile in MAX_AGGRO_RADIUS
Unit* BotPlayerScript::SelectBotTarget(Player* bot, Player* master, bool allowAutoSelect)
{
    if (!bot)
        return nullptr;

    // 1) Retaliate against whoever is attacking the bot.
    if (Unit* myAttacker = bot->getAttackerForHelper())
        if (myAttacker->IsAlive() && bot->IsValidAttackTarget(myAttacker))
        {
            PB_LOG(2, "Bot '{}' target select: retaliate my attacker '{}'", bot->GetName(), myAttacker->GetName());
            return myAttacker;
        }

    // 2) Defend the master from its attacker.
    if (master)
    {
        if (Unit* ownerAttacker = master->getAttackerForHelper())
            if (ownerAttacker->IsAlive() && bot->IsValidAttackTarget(ownerAttacker))
            {
                PB_LOG(2, "Bot '{}' target select: defend master from '{}'", bot->GetName(), ownerAttacker->GetName());
                return ownerAttacker;
            }
    }

    // 3) Assist the master's current target (victim, then target for casters).
    if (master)
    {
        if (Unit* masterTarget = GetMasterAttackTarget(master))
            if (masterTarget->IsAlive() && bot->IsValidAttackTarget(masterTarget))
            {
                PB_LOG(2, "Bot '{}' target select: assist master's target '{}'", bot->GetName(), masterTarget->GetName());
                return masterTarget;
            }
    }

    // 4) Aggressive: nearest hostile in MAX_AGGRO_RADIUS.
    //    同宠物(PetAI)：aggressive 自动选目标 = 45 码内最近敌对，
    //    不加"离 master 距离"限制；防跑远由追击时的离 master 停止逻辑处理。
    if (allowAutoSelect)
        if (Unit* near = SelectNearestAttackTarget(bot, 45.0f))
        {
            PB_LOG(2, "Bot '{}' target select: nearest hostile '{}'", bot->GetName(), near->GetName());
            return near;
        }

    PB_LOG(2, "Bot '{}' target select: no target (autoSelect {})", bot->GetName(), allowAutoSelect);
    return nullptr;
}

// Pet-style follow: immediately follow the master, spreading multiple bots
// across an arc behind him (same formula as GetFollowAngle/GetFollowDistance)
// so they don't stack on the same spot. Static so the command script can
// trigger it instantly.
bool BotPlayerScript::ExecuteBotFollow(Player* bot, Player* master)
{
    if (!bot || !master)
        return false;
    PB_LOG(1, "Bot '{}' FOLLOW command: following '{}'", bot->GetName(), master->GetName());
    if (bot->IsInCombat())
        bot->CombatStop(true);
    bot->InterruptNonMeleeSpells(false);
    uint32 counter = bot->GetGUID().GetCounter();
    float t = static_cast<float>(counter % 7) / 6.0f;
    float angle = M_PI / 6.0f + t * (2.0f * M_PI / 3.0f); // 30°..150° behind master
    float dist = PET_FOLLOW_DIST + 0.5f * static_cast<float>((counter / 7) % 3);
    bot->GetMotionMaster()->Clear();
    bot->GetMotionMaster()->MoveFollow(master, dist, angle);
    return true;
}

// Mirrors PetAI::KilledUnit: the instant the bot kills its current target,
// stop fighting and drop combat so the next AI tick can either pick a new
// target (aggressive) or return to follow - instead of standing still chasing
// the dead target (which used to leave the bot standing around for a while).
void BotPlayerScript::OnCreatureKill(Player* killer, Creature* killed)
{
    if (!killer || !killed)
        return;
    if (!sPlayerBotMgr->IsBot(killer->GetGUID()))
        return;

    PB_LOG(1, "Hook OnCreatureKill: bot '{}' killed creature entry {} ({})",
        killer->GetName(), killed->GetEntry(), killed->GetGUID().ToString());

    // Only react when the bot killed its current target (PetAI::KilledUnit:
    // "if the pet is still attacking something else, return").
    Unit* currentVictim = killer->GetVictim();
    if (currentVictim && currentVictim != killed)
        return;

    // If this was the ATTACK-command target, clear the command state.
    ObjectGuid attackTargetGuid = sPlayerBotMgr->GetBotAttackTarget(killer->GetGUID());
    if (attackTargetGuid == killed->GetGUID())
    {
        sPlayerBotMgr->SetBotAttackTarget(killer->GetGUID(), ObjectGuid::Empty);
        sPlayerBotMgr->ClearBotCommand(killer->GetGUID());
    }

    // Stop fighting and drop combat (PetAI::KilledUnit: AttackStop + ClearThreat).
    killer->AttackStop();
    killer->InterruptNonMeleeSpells(false);
    if (killer->IsInCombat())
        killer->CombatStop(true);

    // Drop any chase of the dead target so the next AI tick can pick a new
    // target or return to follow (prevents standing still at the kill spot).
    killer->GetMotionMaster()->Clear();
    killer->GetMotionMaster()->MoveIdle();
}

// Cached role analysis. Returns the cached AnalyzeBot result for this bot
// (computing it on first use); OnUpdate refreshes it ~1s so combat can read it
// every tick without re-scanning talents/gear each time.
BotPlayerScript::BotRoleAnalysis const& BotPlayerScript::GetBotAnalysis(Player* bot)
{
    auto itr = _botRoleCache.find(bot->GetGUID());
    if (itr != _botRoleCache.end())
        return itr->second;
    _botRoleCache[bot->GetGUID()] = AnalyzeBot(bot);
    return _botRoleCache[bot->GetGUID()];
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

    // Notify master BEFORE clearing master info.
    BotNotify(player, "|cff00ff00[Bot]|r {} Auto-leaving group.", reason);

    sPlayerBotMgr->ClearMaster(player->GetGUID());
    ResetCombatState(player);

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

void BotPlayerScript::UpdateFollow(Player* bot, Player* master)
{
    if (!bot || !master)
        return;

    // Pet-style recall: if the bot is far away (or on another map), teleport
    // to the master's side instead of slowly walking back - like recalling a
    // pet. "Far" means genuinely far (>=2000 yd / another map); within that a
    // bot runs over normally so following stays natural.
    if (bot->GetMapId() != master->GetMapId() ||
        bot->GetDistance(master) > 2000.0f)
    {
        if (!bot->IsBeingTeleported() && !bot->IsInCombat())
        {
            PB_LOG(2, "Bot '{}' pet-style recall teleport to master (dist {:.0f}, map {}->{})",
                bot->GetName(), bot->GetDistance(master), bot->GetMapId(), master->GetMapId());
            bot->GetMotionMaster()->Clear();
            bot->TeleportTo(master->GetMapId(),
                master->GetPositionX(), master->GetPositionY(), master->GetPositionZ(),
                master->GetOrientation());
        }
        return;
    }

    bool isFollowing = bot->GetMotionMaster()->GetCurrentMovementGeneratorType() == FOLLOW_MOTION_TYPE;
    if (!isFollowing || _followMaster != master->GetGUID())
    {
        // Master changed: drop the old follow target before re-following.
        if (isFollowing && _followMaster != master->GetGUID())
            bot->GetMotionMaster()->Clear();
        _followMaster = master->GetGUID();
        bot->GetMotionMaster()->MoveFollow(master, GetFollowDistance(bot), GetFollowAngle(bot));
    }
}

Unit* BotPlayerScript::GetMasterAttackTarget(Player* master)
{
    if (!master)
        return nullptr;

    // Melee victim first.
    if (Unit* victim = master->GetVictim())
        if (victim->IsAlive())
            return victim;

    // Remote/auto-shot attacks (e.g. hunter) may not set a melee victim;
    // fall back to the currently attacked target.
    ObjectGuid targetGuid = master->GetTarget();
    if (!targetGuid.IsEmpty())
        if (Unit* target = ObjectAccessor::GetUnit(*master, targetGuid))
            if (target->IsAlive() && master->IsInCombat())
                return target;

    return nullptr;
}

// Pet REACT_AGGRESSIVE target selection. Mirrors
// Creature::SelectNearestTargetInAttackDistance (that API is Creature-only),
// so scan the grid around the bot and pick the nearest valid hostile unit.
Unit* BotPlayerScript::SelectNearestAttackTarget(Player* bot, float range)
{
    if (!bot || !bot->IsInWorld())
        return nullptr;

    std::list<Unit*> targets;
    Acore::AnyUnfriendlyUnitInObjectRangeCheck u_check(bot, bot, range);
    Acore::UnitListSearcher<Acore::AnyUnfriendlyUnitInObjectRangeCheck> searcher(bot, targets, u_check);
    Cell::VisitAllObjects(bot, searcher, range);

    Unit* nearest = nullptr;
    float bestDist = range;
    for (Unit* u : targets)
    {
        if (!u->IsAlive() || u->IsCritter())
            continue;
        if (!bot->IsValidAttackTarget(u))
            continue;
        float dist = bot->GetDistance(u);
        if (dist < bestDist)
        {
            bestDist = dist;
            nearest = u;
        }
    }
    return nearest;
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
    // P-006: 法力不足时不施放（仅法力职业；无消耗/非法力法术不受影响）
    if (spellInfo->PowerType == POWER_MANA)
    {
        int32 cost = spellInfo->CalcPowerCost(bot, spellInfo->GetSchoolMask());
        if (cost > 0 && static_cast<int32>(bot->GetPower(POWER_MANA)) < cost)
            return false;
    }
    float range = spellInfo->GetMaxRange();
    if (bot->GetDistance(target) > range)
        return false;
    return true;
}

// ---- Friendly-target (buff/heal) detection helpers ----------------
namespace
{
    bool IsEnemyImplicitTarget(Targets t)
    {
        switch (t)
        {
            case TARGET_UNIT_NEARBY_ENEMY:
            case TARGET_UNIT_TARGET_ENEMY:
            case TARGET_UNIT_SRC_AREA_UNK_11:
            case TARGET_UNIT_SRC_AREA_ENEMY:
            case TARGET_UNIT_DEST_AREA_ENEMY:
            case TARGET_UNIT_CONE_ENEMY_24:
            case TARGET_UNIT_CONE_ENEMY_54:
            case TARGET_UNIT_CONE_ENEMY_104:
            case TARGET_DEST_DYNOBJ_ENEMY:
            case TARGET_DEST_TARGET_ENEMY:
            case TARGET_CORPSE_SRC_AREA_ENEMY:
                return true;
            default:
                return false;
        }
    }

    bool IsFriendlyImplicitTarget(Targets t)
    {
        switch (t)
        {
            case TARGET_UNIT_CASTER:
            case TARGET_UNIT_NEARBY_ALLY:
            case TARGET_UNIT_NEARBY_PARTY:
            case TARGET_UNIT_PET:
            case TARGET_UNIT_CASTER_AREA_PARTY:
            case TARGET_UNIT_TARGET_ALLY:
            case TARGET_SRC_CASTER:
            case TARGET_DEST_CASTER:
            case TARGET_DEST_DYNOBJ_ALLY:
            case TARGET_UNIT_SRC_AREA_ALLY:
            case TARGET_UNIT_DEST_AREA_ALLY:
            case TARGET_UNIT_SRC_AREA_PARTY:
            case TARGET_UNIT_DEST_AREA_PARTY:
            case TARGET_UNIT_TARGET_PARTY:
            case TARGET_UNIT_LASTTARGET_AREA_PARTY:
            case TARGET_UNIT_TARGET_CHAINHEAL_ALLY:
            case TARGET_UNIT_CASTER_AREA_RAID:
            case TARGET_UNIT_TARGET_RAID:
            case TARGET_UNIT_NEARBY_RAID:
            case TARGET_UNIT_CONE_ALLY:
            case TARGET_UNIT_MASTER:
            case TARGET_UNIT_TARGET_MINIPET:
            case TARGET_UNIT_SUMMONER:
                return true;
            default:
                return false;
        }
    }
}

// True when the spell is a buff/heal (implicit targets are self/ally/party,
// with no enemy target). Such spells must be cast on the bot or a group
// member, never on the combat target.
bool BotPlayerScript::IsFriendlyTargetSpell(SpellInfo const* spellInfo)
{
    if (!spellInfo)
        return false;
    bool friendly = false;
    for (uint8 i = 0; i < spellInfo->Effects.size(); ++i)
    {
        SpellEffectInfo const& eff = spellInfo->Effects[i];
        if (!eff.IsEffect())
            continue;
        if (IsEnemyImplicitTarget(eff.TargetA.GetTarget()) ||
            IsEnemyImplicitTarget(eff.TargetB.GetTarget()))
            return false; // offensive - cast on the combat target
        if (IsFriendlyImplicitTarget(eff.TargetA.GetTarget()) ||
            IsFriendlyImplicitTarget(eff.TargetB.GetTarget()))
            friendly = true;
    }
    return friendly;
}

// Friendly-target check for buffs/heals: CanCastSpell requires an attackable
// target, so buffs/heals use their own check (self is always a valid target).
bool BotPlayerScript::CanCastFriendlySpell(Player* bot, Unit* target, uint32 spellId)
{
    if (!bot || !target || spellId == 0)
        return false;
    if (!bot->HasSpell(spellId))
        return false;
    if (bot->HasSpellCooldown(spellId))
        return false;
    if (target != bot && !bot->IsValidAssistTarget(target))
        return false;
    SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(spellId);
    if (!spellInfo)
        return false;
    // P-007: 增益/治疗施放同样检查法力，避免空蓝仍刷 buff
    if (spellInfo->PowerType == POWER_MANA)
    {
        int32 cost = spellInfo->CalcPowerCost(bot, spellInfo->GetSchoolMask());
        if (cost > 0 && static_cast<int32>(bot->GetPower(POWER_MANA)) < cost)
            return false;
    }
    float range = spellInfo->GetMaxRange();
    if (range > 0.0f && bot->GetDistance(target) > range)
        return false;
    return true;
}

// Choose who a friendly-target spell is cast on (real-client behaviour):
//  - self / self-centred party/raid AOE buffs  -> the bot itself (they spread
//    around the caster);
//  - master-targeted (e.g. Misdirection)       -> the master;
//  - single-target heal/buff                   -> lowest-health live group
//    member in range, otherwise the bot itself.
Unit* BotPlayerScript::GetFriendlyCastTarget(Player* bot, SpellInfo const* spellInfo)
{
    if (!bot || !spellInfo)
        return nullptr;

    for (uint8 i = 0; i < spellInfo->Effects.size(); ++i)
    {
        SpellEffectInfo const& eff = spellInfo->Effects[i];
        if (!eff.IsEffect())
            continue;
        Targets ta = eff.TargetA.GetTarget();
        Targets tb = eff.TargetB.GetTarget();
        if (ta == TARGET_UNIT_CASTER || tb == TARGET_UNIT_CASTER ||
            ta == TARGET_SRC_CASTER || ta == TARGET_DEST_CASTER ||
            ta == TARGET_UNIT_CASTER_AREA_PARTY || ta == TARGET_UNIT_CASTER_AREA_RAID ||
            ta == TARGET_UNIT_SRC_AREA_PARTY || ta == TARGET_UNIT_SRC_AREA_ALLY ||
            ta == TARGET_UNIT_DEST_AREA_PARTY || ta == TARGET_UNIT_DEST_AREA_ALLY ||
            ta == TARGET_UNIT_LASTTARGET_AREA_PARTY)
            return bot;
        if (ta == TARGET_UNIT_MASTER || tb == TARGET_UNIT_MASTER)
        {
            if (Player* master = ObjectAccessor::FindConnectedPlayer(sPlayerBotMgr->GetMaster(bot->GetGUID())))
                if (master->IsAlive())
                    return master;
            return bot;
        }
    }

    // Single-target heal/buff: lowest-health live group member in range.
    Unit* best = bot;
    float bestPct = bot->GetHealthPct();
    if (Group* group = bot->GetGroup())
    {
        for (auto const& slot : group->GetMemberSlots())
        {
            Player* member = ObjectAccessor::FindConnectedPlayer(slot.guid);
            if (!member || !member->IsAlive() || member == bot)
                continue;
            if (bot->GetDistance(member) > 40.0f)
                continue;
            float pct = member->GetHealthPct();
            if (pct < bestPct)
            {
                bestPct = pct;
                best = member;
            }
        }
    }
    return best;
}

uint32 BotPlayerScript::GetKnownSpell(Player* bot, uint32 baseSpellId)
{
    if (!bot || baseSpellId == 0)
        return 0;
    if (bot->HasSpell(baseSpellId))
        return baseSpellId;

    // Walk the spell chain from the base upward and return the highest rank
    // the bot actually knows. This lets combat use the bot's real skills
    // (e.g. a max-rank Mortal Strike) instead of a low-rank spell id.
    uint32 current = baseSpellId;
    uint32 known = 0;
    for (uint32 i = 0; i < 10; ++i)
    {
        uint32 next = sSpellMgr->GetNextSpellInChain(current);
        if (!next || next == current)
            break;
        if (bot->HasSpell(next))
            known = next;
        current = next;
    }
    return known;
}

// Pet-style auto-cast, mirroring the real client's skill-bar auto-cast: the
// bot's skills come from its OWN action bar - the class default bar from
// playercreateinfo_action holds the melee Attack (6603) and ranged Auto Shot
// (75) buttons plus the class skills - or from a master-configured 12-slot bar
// (matching the player's first action bar).
// Everything is auto-cast: active skills are cast by range/cooldown
// (CanCastSpell), Auto Shot (75) starts the ranged auto-attack loop ("the
// gun"). The melee Attack (6603) button is deliberately NOT cast here - the
// unified melee attack in DoCombat drives it. So a hunter does NOT hard-switch
// weapons by distance: the bar auto-casts Auto Shot at range and DoCombat's
// melee attack covers the "axe" when the target is in melee range - exactly
// like the real client, no hard-coded weapon-switching.
namespace
{
    // P-008: 自解控类技能——效果为移除自身控制效果（昏迷/恐惧/定身/沉默等）
    bool IsSelfControlRemovalSpell(SpellInfo const* si)
    {
        if (!si || !si->HasEffect(SPELL_EFFECT_REMOVE_AURA))
            return false;
        return si->HasEffectMechanic(MECHANIC_STUN) || si->HasEffectMechanic(MECHANIC_FEAR) ||
               si->HasEffectMechanic(MECHANIC_ROOT) || si->HasEffectMechanic(MECHANIC_SILENCE) ||
               si->HasEffectMechanic(MECHANIC_POLYMORPH) || si->HasEffectMechanic(MECHANIC_SLEEP) ||
               si->HasEffectMechanic(MECHANIC_CHARM) || si->HasEffectMechanic(MECHANIC_FREEZE) ||
               si->HasEffectMechanic(MECHANIC_KNOCKOUT) || si->HasEffectMechanic(MECHANIC_DISORIENTED) ||
               si->HasEffectMechanic(MECHANIC_BANISH) || si->HasEffectMechanic(MECHANIC_HORROR);
    }

    // P-008: bot 是否被控制
    bool BotIsControlled(Player* bot)
    {
        static const uint32 ctrlMechanics[] = {
            MECHANIC_STUN, MECHANIC_FEAR, MECHANIC_ROOT, MECHANIC_SILENCE,
            MECHANIC_POLYMORPH, MECHANIC_SLEEP, MECHANIC_CHARM, MECHANIC_FREEZE,
            MECHANIC_KNOCKOUT, MECHANIC_DISORIENTED, MECHANIC_BANISH, MECHANIC_HORROR
        };
        for (uint32 m : ctrlMechanics)
            if (bot->HasAuraWithMechanic(m))
                return true;
        return false;
    }

    // P-009: 净化/免疫类技能——移除/免疫 流血/疾病（含 dispel 毒/疾病类型）
    bool IsCleanseRemovalSpell(SpellInfo const* si)
    {
        if (!si || !si->HasEffect(SPELL_EFFECT_REMOVE_AURA))
            return false;
        return si->HasEffectMechanic(MECHANIC_BLEED) || si->HasEffectMechanic(MECHANIC_INFECTED) ||
               si->Dispel == DISPEL_POISON || si->Dispel == DISPEL_DISEASE;
    }

    // P-009: bot 是否有 流血/疾病 debuff（按机制）
    bool BotHasCleanseableDebuff(Player* bot)
    {
        static const uint32 debuffMechanics[] = { MECHANIC_BLEED, MECHANIC_INFECTED };
        for (uint32 m : debuffMechanics)
            if (bot->HasAuraWithMechanic(m))
                return true;
        return false;
    }

    // P-011: 治疗类技能（含 HOT）
    bool IsHealSpell(SpellInfo const* si)
    {
        if (!si)
            return false;
        return si->HasEffect(SPELL_EFFECT_HEAL) || si->HasEffect(SPELL_EFFECT_HEAL_PCT) ||
               si->HasEffect(SPELL_EFFECT_HEAL_MAX_HEALTH) || si->HasEffect(SPELL_EFFECT_HEAL_MECHANICAL) ||
               si->HasAura(SPELL_AURA_PERIODIC_HEAL);
    }
}

bool BotPlayerScript::CastAutoSpells(Player* bot, Unit* target)
{
    if (!bot || !target)
        return false;

    // P-010: 施法防打断——正在读条/引导时本 tick 不施放新技能，
    // 避免每 tick 的自动施法打断自己当前读条。
    if (bot->IsNonMeleeSpellCast(false))
        return false;

    // 1) Collect the auto-cast skill list straight from the bot's OWN first
    //    action bar (12 slots, matching the real client): slot N = button
    //    N-1. Only slots with autocast enabled are auto-cast; 6603 (melee
    //    Attack) is skipped - it is driven by DoCombat's unified melee attack.
    std::vector<uint32> spells;
    for (uint8 slot = 1; slot <= PlayerBotMgr::MAX_SKILL_SLOTS; ++slot)
    {
        uint32 sid = sPlayerBotMgr->GetBotSkillSlot(bot, slot);
        if (!sid || sid == 6603)
            continue;
        if (!sPlayerBotMgr->IsBotSlotAutocast(bot->GetGUID(), slot))
            continue;
        spells.push_back(sid);
    }
    if (spells.empty())
        return false;

    // 2) Auto Shot (75): the ranged auto-attack ("the gun"). Casting it once
    //    puts it in the CURRENT_AUTOREPEAT_SPELL slot and the core fires it
    //    every ranged tick. It only starts when the target is OUTSIDE melee
    //    range - inside melee range DoCombat stops it and uses the melee
    //    weapon instead, so a hunter never keeps "holding the gun" at melee
    //    distance (exactly like a real hunter: gun at range, axe in melee,
    //    switched by range with no hard-coded weapon logic).
    bool startedRanged = false;
    for (uint32 sid : spells)
    {
        if (sid == 75)
        {
            // P-008: 远程攻击需弹药；无弹药且无"无需弹药"天赋时不启动自动射击
            bool hasAmmo = bot->HasAura(46699) || bot->GetUInt32Value(PLAYER_AMMO_ID) != 0;
            bool outOfMelee = bot->GetDistance(target) > bot->GetMeleeRange(target);
            if (outOfMelee && hasAmmo &&
                bot->GetWeaponForAttack(RANGED_ATTACK, true) &&
                !bot->GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL) &&
                CanCastSpell(bot, target, 75))
            {
                PB_LOG(2, "Bot '{}' started Auto Shot (75) at range", bot->GetName());
                bot->CastSpell(target, 75, false);
                startedRanged = true;
            }
            break;
        }
    }

    // 3) Active skills: cast one per tick, rotating the scan start so every
    //    skill gets a chance (the real client does not spam only slot 1).
    //    Friendly-target spells (buff/heal) are cast on the bot itself or the
    //    lowest-health group member - never on the combat target; offensive
    //    spells are cast on the target (real-client auto-cast behaviour).
    uint32 cursor = sPlayerBotMgr->GetBotSkillCursor(bot->GetGUID());
    for (uint32 i = 0; i < spells.size(); ++i)
    {
        uint32 sid = spells[(cursor + i) % spells.size()];
        if (sid == 75)
            continue; // already handled above
        SpellInfo const* si = sSpellMgr->GetSpellInfo(sid);
        if (!si)
            continue;
        // P-008: 自解控技能——仅被控时施放，否则跳过（不乱放自利等）
        if (IsSelfControlRemovalSpell(si) && !BotIsControlled(bot))
            continue;
        // P-009: 净化/免疫技能——仅有流血/中毒/疾病 debuff 时施放（不乱放石化形态等）
        if (IsCleanseRemovalSpell(si) && !BotHasCleanseableDebuff(bot))
            continue;
        if (IsFriendlyTargetSpell(si))
        {
            Unit* friendly = GetFriendlyCastTarget(bot, si);
            if (!friendly)
                continue;
            // P-007: 已有该增益效果则跳过，避免重复施放耗蓝
            if (friendly->HasAura(sid))
                continue;
            // P-011: 治疗类技能——目标血量充足(>=80%)则不施放，避免没事乱放治疗
            if (IsHealSpell(si) && friendly->GetHealthPct() >= 80.0f)
                continue;
            if (!CanCastFriendlySpell(bot, friendly, sid))
                continue;
            PB_LOG(2, "Bot '{}' auto-cast friendly spell {} on '{}'", bot->GetName(), sid, friendly->GetName());
            bot->CastSpell(friendly, sid, false);
        }
        else
        {
            if (!CanCastSpell(bot, target, sid))
                continue;
            PB_LOG(2, "Bot '{}' auto-cast spell {} on '{}'", bot->GetName(), sid, target->GetName());
            bot->CastSpell(target, sid, false);
        }
        sPlayerBotMgr->SetBotSkillCursor(bot->GetGUID(), (cursor + i + 1) % spells.size());
        return true;
    }
    sPlayerBotMgr->SetBotSkillCursor(bot->GetGUID(), (cursor + 1) % spells.size());

    return startedRanged;
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
    // Load the module's own config (worldserver.conf, provided by the
    // mod_playerbot.conf.dist that ships with the module). DebugLevel gates
    // every hook/event log below: 0=off 1=events 2=behaviour 3=dump.
    uint32 debugLevel = sConfigMgr->GetOption<uint32>("PlayerBot.DebugLevel", 0);
    sPlayerBotMgr->SetDebugLevel(debugLevel);

    LOG_INFO("playerbots", "[{} (rev {}, built {})] Registering PlayerBot scripts... (PlayerBot.DebugLevel={})",
        PLAYERBOT_VERSION, PLAYERBOT_REV, PLAYERBOT_BUILD, debugLevel);
    new BotPlayerScript();
    new BotGroupScript();
    AddBotCommandScripts();
    LOG_INFO("playerbots", "[{} (rev {}, built {})] PlayerBot scripts registered successfully!", PLAYERBOT_VERSION, PLAYERBOT_REV, PLAYERBOT_BUILD);
}
