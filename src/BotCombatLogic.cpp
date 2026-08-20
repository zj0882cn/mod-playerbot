#include "BotPlayerScript.h"
#include "BotCommon.h"
#include "PlayerBotMgr.h"
#include "Player.h"
#include "Unit.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "MotionMaster.h"
#include "Group.h"
#include "ObjectAccessor.h"
#include "Chat.h"
#include "Log.h"

#define PLAYERBOT_VERSION "v2.2.0"

BotPlayerScript::BotRoleAnalysis BotPlayerScript::AnalyzeBot(Player* bot)
{
    BotRoleAnalysis result;
    result.role = BotRoleAnalysis::ROLE_UNKNOWN;
    result.hasShield = false;
    result.hasRangedWeapon = false;
    result.preferDistance = 3.0f;
    result.useAggressive = true;
    result.useInterrupts = false;
    result.averageItemLevel = 0.0f;
    result.talentTree1 = 0;
    result.talentTree2 = 0;
    result.talentTree3 = 0;

    //uint8 cls = bot->GetClass();
    uint8 cls = bot->getClass();

    Item* offHand = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
    if (offHand)
    {
        ItemTemplate const* itemTemplate = offHand->GetTemplate();
        if (itemTemplate && itemTemplate->Class == ITEM_CLASS_ARMOR && itemTemplate->SubClass == ITEM_SUBCLASS_ARMOR_SHIELD)
            result.hasShield = true;
    }

    Item* ranged = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_RANGED);
    if (ranged)
        result.hasRangedWeapon = true;

    result.averageItemLevel = CalculateAverageItemLevel(bot);

    switch (cls)
    {
        case CLASS_WARRIOR:
        {
            result.talentTree1 = GetTalentPointsInTree(bot, TALENT_TREE_WARRIOR_ARMS);
            result.talentTree2 = GetTalentPointsInTree(bot, TALENT_TREE_WARRIOR_FURY);
            result.talentTree3 = GetTalentPointsInTree(bot, TALENT_TREE_WARRIOR_PROTECTION);

            if (result.hasShield && result.talentTree3 >= result.talentTree1 && result.talentTree3 >= result.talentTree2)
            {
                result.role = BotRoleAnalysis::ROLE_TANK;
                result.preferDistance = 3.0f;
                result.useInterrupts = true;
            }
            else
            {
                result.role = BotRoleAnalysis::ROLE_MELEE_DPS;
                result.preferDistance = 3.0f;
            }
            break;
        }
        case CLASS_PALADIN:
        {
            result.talentTree1 = GetTalentPointsInTree(bot, TALENT_TREE_PALADIN_HOLY);
            result.talentTree2 = GetTalentPointsInTree(bot, TALENT_TREE_PALADIN_PROTECTION);
            result.talentTree3 = GetTalentPointsInTree(bot, TALENT_TREE_PALADIN_RETRIBUTION);

            if (result.hasShield && result.talentTree2 >= result.talentTree1 && result.talentTree2 >= result.talentTree3)
            {
                result.role = BotRoleAnalysis::ROLE_TANK;
                result.preferDistance = 3.0f;
                result.useInterrupts = true;
            }
            else if (result.talentTree1 > result.talentTree2 && result.talentTree1 > result.talentTree3)
            {
                result.role = BotRoleAnalysis::ROLE_HEALER;
                result.preferDistance = 25.0f;
                result.useAggressive = false;
            }
            else
            {
                result.role = BotRoleAnalysis::ROLE_MELEE_DPS;
                result.preferDistance = 3.0f;
            }
            break;
        }
        case CLASS_DRUID:
        {
            result.talentTree1 = GetTalentPointsInTree(bot, TALENT_TREE_DRUID_BALANCE);
            result.talentTree2 = GetTalentPointsInTree(bot, TALENT_TREE_DRUID_FERAL_COMBAT);
            result.talentTree3 = GetTalentPointsInTree(bot, TALENT_TREE_DRUID_RESTORATION);

            if (result.talentTree3 > result.talentTree1 && result.talentTree3 > result.talentTree2)
            {
                result.role = BotRoleAnalysis::ROLE_HEALER;
                result.preferDistance = 25.0f;
                result.useAggressive = false;
            }
            else if (bot->GetShapeshiftForm() == FORM_BEAR)
            {
                result.role = BotRoleAnalysis::ROLE_TANK;
                result.preferDistance = 3.0f;
            }
            else if (bot->GetShapeshiftForm() == FORM_CAT)
            {
                result.role = BotRoleAnalysis::ROLE_MELEE_DPS;
                result.preferDistance = 3.0f;
            }
            else if (result.talentTree1 > result.talentTree2)
            {
                result.role = BotRoleAnalysis::ROLE_RANGED_DPS;
                result.preferDistance = 30.0f;
            }
            else
            {
                result.role = BotRoleAnalysis::ROLE_MELEE_DPS;
                result.preferDistance = 3.0f;
            }
            break;
        }
        case CLASS_PRIEST:
        {
            result.talentTree1 = GetTalentPointsInTree(bot, TALENT_TREE_PRIEST_HOLY);
            result.talentTree2 = GetTalentPointsInTree(bot, TALENT_TREE_PRIEST_DISCIPLINE);
            result.talentTree3 = GetTalentPointsInTree(bot, TALENT_TREE_PRIEST_SHADOW);

            if (result.talentTree3 > result.talentTree1 && result.talentTree3 > result.talentTree2)
            {
                result.role = BotRoleAnalysis::ROLE_RANGED_DPS;
                result.preferDistance = 30.0f;
            }
            else
            {
                result.role = BotRoleAnalysis::ROLE_HEALER;
                result.preferDistance = 25.0f;
                result.useAggressive = false;
            }
            break;
        }
        case CLASS_SHAMAN:
        {
            result.talentTree1 = GetTalentPointsInTree(bot, TALENT_TREE_SHAMAN_ELEMENTAL);
            result.talentTree2 = GetTalentPointsInTree(bot, TALENT_TREE_SHAMAN_ENHANCEMENT);
            result.talentTree3 = GetTalentPointsInTree(bot, TALENT_TREE_SHAMAN_RESTORATION);

            if (result.talentTree3 > result.talentTree1 && result.talentTree3 > result.talentTree2)
            {
                result.role = BotRoleAnalysis::ROLE_HEALER;
                result.preferDistance = 25.0f;
                result.useAggressive = false;
            }
            else if (result.talentTree2 >= result.talentTree1)
            {
                result.role = BotRoleAnalysis::ROLE_MELEE_DPS;
                result.preferDistance = 3.0f;
            }
            else
            {
                result.role = BotRoleAnalysis::ROLE_RANGED_DPS;
                result.preferDistance = 30.0f;
            }
            break;
        }
        case CLASS_HUNTER:
            result.role = BotRoleAnalysis::ROLE_RANGED_DPS;
            result.preferDistance = 35.0f;
            break;
        case CLASS_MAGE:
        case CLASS_WARLOCK:
            result.role = BotRoleAnalysis::ROLE_RANGED_DPS;
            result.preferDistance = 30.0f;
            break;
        case CLASS_ROGUE:
            result.role = BotRoleAnalysis::ROLE_MELEE_DPS;
            result.preferDistance = 3.0f;
            break;
        case CLASS_DEATH_KNIGHT:
            result.role = BotRoleAnalysis::ROLE_MELEE_DPS;
            result.preferDistance = 3.0f;
            break;
        default:
            result.role = BotRoleAnalysis::ROLE_UNKNOWN;
            break;
    }

    return result;
}

// Public wrapper so the command script can trigger combat immediately
// (pet-style "attack" command).
void BotPlayerScript::ExecuteBotCombat(Player* bot, Unit* target)
{
    PB_LOG(2, "Bot '{}' ExecuteBotCombat target '{}'",
        bot ? bot->GetName().c_str() : "?", target ? target->GetName() : "?");
    DoCombat(bot, target);
}

// 预留接口：团队是否含有明显的 T（坦克）。
// 当前实现固定返回 false——按"无明确 T、全员 DPS、无专职奶"逻辑处理。
// 未来实现：扫描 bot 所在队伍的在线成员，对每个成员调用 AnalyzeBot，
// 若任一成员的 role == ROLE_TANK 则返回 true；此时全队行为切换：
//   - DPS：安心输出（有 T 拉仇恨）
//   - 奶：优先治疗 T
//   - 站位/仇恨：围绕 T 展开
// 调用点：DoCombat() 中的 groupHasTank 钩子。
bool BotPlayerScript::GroupHasTank(Player* bot) const
{
    // TODO(团队 T 检测, 预留): 当前不检测，默认无明确 T。
    return false;
}

// Pet-style attack, mirroring PetAI::AttackStart -> DoAttack. Immediately
// abandons current action, attacks the target (Player::Attack handles
// melee vs ranged), and chases to the appropriate combat distance.
// Mirrors PetAI::_canMeleeAttack combatRange: melee classes close into
// melee range, ranged/healer classes keep their preferred casting distance
// (a mage must NOT run into melee range). Returns false if unattackable.
bool BotPlayerScript::ExecuteBotAttack(Player* bot, Unit* target)
{
    if (!bot || !target || !target->IsAlive())
        return false;
    if (!bot->IsValidAttackTarget(target))
        return false;

    bot->InterruptNonMeleeSpells(false);

    // Every class has melee auto-attack (Player::Attack(true) sets the
    // MELEE_ATTACKING state that drives the melee attack loop). Only hunters
    // additionally have a ranged auto-attack, which is handled by their ranged
    // skills in combat. Melee classes chase into melee range, ranged/healer
    // classes keep their preferred casting distance.
    BotRoleAnalysis analysis = AnalyzeBot(bot);
    bool isMelee = (analysis.role == BotRoleAnalysis::ROLE_TANK ||
                    analysis.role == BotRoleAnalysis::ROLE_MELEE_DPS);

    if (bot->Attack(target, true))
    {
        PB_LOG(1, "Bot '{}' ATTACK command: attacking '{}' (melee {})",
            bot->GetName(), target->GetName(), isMelee);
        bot->GetMotionMaster()->Clear();
        if (isMelee)
            bot->GetMotionMaster()->MoveChase(target);
        else
            bot->GetMotionMaster()->MoveChase(target, analysis.preferDistance);
        return true;
    }

    PB_LOG(1, "Bot '{}' ATTACK command: Player::Attack failed on '{}'",
        bot->GetName(), target->GetName());
    return false;
}

void BotPlayerScript::DoCombat(Player* bot, Unit* target, bool chase)
{
    if (!bot || !target || !target->IsAlive())
        return;

    BotRoleAnalysis const& analysis = GetBotAnalysis(bot);
    float preferDist = analysis.preferDistance;
    float distance = bot->GetDistance(target);

    // 预留钩子：团队 T 检测（当前返回 false）。
    // 未来实现 GroupHasTank 后，在此根据返回值切换全体 bot 行为
    // （有 T：DPS 安心输出/奶保 T/站位围绕 T；无 T：奶转 DPS、全员自保）。
    bool groupHasTank = GroupHasTank(bot);

    PB_LOG(2, "Bot '{}' DoCombat target '{}' dist {:.1f} chase {} role {}",
        bot->GetName(), target->GetName(), distance, chase, int(analysis.role));

    bool isMelee = (analysis.role == BotRoleAnalysis::ROLE_TANK ||
                    analysis.role == BotRoleAnalysis::ROLE_MELEE_DPS);

    // 宠物参考(PetAI::_needToStop)：离 master 超过 视野-10 码则停止追击，
    // 防止 bot 脱离队伍跑远引怪，改为返回跟随 master。
    if (Player* master = ObjectAccessor::FindConnectedPlayer(sPlayerBotMgr->GetMaster(bot->GetGUID())))
    {
        if (bot->GetDistance(master) >= master->GetVisibilityRange() - 10.0f)
        {
            bot->AttackStop();
            bot->GetMotionMaster()->Clear();
            UpdateFollow(bot, master);
            return;
        }
    }

    // Pet-style target lock: Player::Attack sets m_attacking (GetVictim), our
    // "current target". The bot acts on this target until it dies, then picks
    // a new one - it never re-evaluates someone else's target every tick (that
    // made combat erratic). Melee classes enter the melee-attacking state;
    // ranged casters just lock the victim without it.
    if (bot->GetVictim() != target)
        bot->Attack(target, isMelee);

    // Pet-style: keep the client target set and face the target before
    // casting/attacking. Pets set the target in Unit::Attack (SetTarget) and
    // re-face at cast time (PetAI::SetInFront) - not on a timer. Movement
    // facing is handled automatically by the spline.
    //
    // NOTE: for players Player::SetTarget is a no-op (UNIT_FIELD_TARGET is
    // normally driven by the client via CMSG_SET_TARGET -> SetSelection), so
    // the old SetTarget call never updated the client-visible target. Use
    // SetSelection instead so UNIT_FIELD_TARGET is really broadcast. Point it
    // at the master - party/raid frames always let a player be selected, so in
    // practice this always succeeds. If master is somehow unavailable
    // (offline / other map) fall back to clearing the selection so no stale
    // target is shown.
    if (Player* master = ObjectAccessor::FindConnectedPlayer(sPlayerBotMgr->GetMaster(bot->GetGUID())))
    {
        if (bot->GetTarget() != master->GetGUID())
            bot->SetSelection(master->GetGUID());
    }
    else if (bot->GetTarget())
    {
        bot->SetSelection(ObjectGuid::Empty);
    }
    // Pet-style facing: always face the target before acting (PetAI::SetInFront
    // does this unconditionally - a real player keeps facing the target, the
    // bot must too to land melee swings / ranged shots). The old
    // HasInArc(M_PI) check only turned when the target was >180° behind, so
    // the bot kept "fighting sideways" and turned too rarely.
    bot->SetFacingToObject(target);

    // Only (re)start the chase when we aren't already chasing THIS target -
    // keeps the ChaseMovementGenerator alive instead of rebuilding it every
    // tick (a pet keeps its chase; it doesn't recreate it each AI tick).
    // A stale CHASE generator left over from a previous target that died /
    // leashed still reports CHASE_MOTION_TYPE while the bot stands still, so
    // also require that the bot is actually MOVING toward the target - this
    // rebuilds the chase instead of freezing in place (aggressive bots that
    // auto-hunt distant targets hit this most).
    bool chasingTarget = bot->GetMotionMaster()->GetCurrentMovementGeneratorType() == CHASE_MOTION_TYPE
                         && bot->GetVictim() == target
                         && bot->isMoving();

    if (chase)
    {
        if (isMelee)
        {
            // Melee: close in until within melee attack range.
            if (distance > bot->GetMeleeRange(target))
            {
                if (!chasingTarget)
                    bot->GetMotionMaster()->MoveChase(target);
                return;
            }
        }
        else
        {
            // Ranged/healer: keep preferred distance.
            if (distance > preferDist + 1.0f)
            {
                if (!chasingTarget)
                    bot->GetMotionMaster()->MoveChase(target, preferDist);
                return;
            }
        }
    }
    else
    {
        // Hold position (pet STAY): fight only inside the attack range for
        // this class - melee uses melee range, ranged casters use their
        // preferred casting range, and hunters their Auto Shot range (inside
        // which they shoot AND swing melee when the target closes in - a
        // hunter is ranged + melee combined). Never chase: a target out of
        // range makes the bot stop and return to the stay spot.
        float attackRange = isMelee ? bot->GetMeleeRange(target) : (preferDist + 1.0f);
        if (distance > attackRange)
        {
            if (bot->IsInCombat())
                bot->CombatStop(true);
            float sx, sy, sz;
            if (sPlayerBotMgr->GetBotStayPosition(bot->GetGUID(), sx, sy, sz))
            {
                if (bot->GetDistance(sx, sy, sz) > 1.0f)
                {
                    bot->GetMotionMaster()->Clear();
                    bot->GetMotionMaster()->MovePoint(bot->GetGUID().GetCounter(), sx, sy, sz);
                }
            }
            return;
        }
    }

    // ---- Real-client style combat: the skill bar drives the skills. ----
    // The melee Attack ("axe") is unified: any class with the target in melee
    // range swings the melee weapon (a hunter too, exactly like the real
    // client). A hunter in melee range ALSO stops Auto Shot ("the gun") - a
    // real hunter does not keep "holding the gun" when the target is at his
    // feet. Auto Shot restarts when the target leaves melee range
    // (CastAutoSpells only starts it outside melee range), so the weapon
    // switches by range with no hard-coded weapon logic.
    if (bot->GetDistance(target) <= bot->GetMeleeRange(target))
    {
        if (bot->GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL))
            bot->InterruptSpell(CURRENT_AUTOREPEAT_SPELL); // stop the gun, use the axe
        bot->Attack(target, true);
    }

    // 明显的坦克(T)：直接走坦克专属逻辑（嘲讽/仇恨/减伤/保命），不按技能条
    // 自动施放——避免坦克把技能条当输出乱放、不拉仇恨不生存。
    if (analysis.role == BotRoleAnalysis::ROLE_TANK)
    {
        DoTankCombat(bot, target, analysis);
        return;
    }

    // 无明确 T 时（GroupHasTank()==false，当前默认）：奶职业也转为 DPS
    // （技能条自动施放 + 远程DPS兜底），不做专职治疗——符合"无专职奶"。
    // 未来 GroupHasTank 返回 true（团队有明显 T）时，奶职业回归 DoHealerCombat
    // 专职治疗（保 T 优先）。
    bool isHealer = analysis.role == BotRoleAnalysis::ROLE_HEALER;
    bool healAsRole = isHealer && groupHasTank;

    // Skill-bar auto-cast (real client behaviour). CastAutoSpells returns true
    // if it cast something (Auto Shot started and/or an active skill). Only
    // when nothing could be cast do we fall back to the hard-coded class
    // defaults below (action bar empty or everything on cooldown), so a fresh
    // bot still fights.
    if (!healAsRole && CastAutoSpells(bot, target))
        return;

    switch (analysis.role)
    {
        case BotRoleAnalysis::ROLE_TANK:
            DoTankCombat(bot, target, analysis);
            break;
        case BotRoleAnalysis::ROLE_HEALER:
            if (healAsRole)
                DoHealerCombat(bot, target, analysis);      // 有 T：专职治疗
            else
                DoRangedDPSCombat(bot, target, analysis);   // 无 T：奶转 DPS
            break;
        case BotRoleAnalysis::ROLE_MELEE_DPS:
            DoMeleeDPSCombat(bot, target, analysis);
            break;
        case BotRoleAnalysis::ROLE_RANGED_DPS:
            DoRangedDPSCombat(bot, target, analysis);
            break;
        default:
            bot->Attack(target, true);
            break;
    }
}

void BotPlayerScript::DoTankCombat(Player* bot, Unit* target, BotRoleAnalysis const& analysis)
{
    uint8 cls = bot->getClass();

    switch (cls)
    {
        case CLASS_WARRIOR:
        {
            if (!analysis.hasShield)
            {
                DoMeleeDPSCombat(bot, target, analysis);
                return;
            }

            uint32 rage = bot->GetPower(POWER_RAGE);

            if (target->GetVictim() != bot && CanCastSpell(bot, target, 355))
                CastBotSpell(bot, target, 355);

            if (rage >= 15 && !target->HasAura(7384) && CanCastSpell(bot, target, 7384))
                CastBotSpell(bot, target, 7384);

            if (CanCastSpell(bot, bot, 2565))
                CastBotSpell(bot, bot, 2565);

            if (rage >= 15 && CanCastSpell(bot, target, 78))
                CastBotSpell(bot, target, 78);

            if (bot->GetHealthPct() < 20 && CanCastSpell(bot, bot, 12975))
                CastBotSpell(bot, bot, 12975);

            bot->Attack(target, true);
            break;
        }
        case CLASS_PALADIN:
        {
            if (!analysis.hasShield)
            {
                DoMeleeDPSCombat(bot, target, analysis);
                return;
            }

            if (!bot->HasAura(25780) && CanCastSpell(bot, bot, 25780))
                CastBotSpell(bot, bot, 25780);

            if (target->GetVictim() != bot && CanCastSpell(bot, target, 6940))
                CastBotSpell(bot, target, 6940);

            if (CanCastSpell(bot, target, 20271))
                CastBotSpell(bot, target, 20271);

            if (CanCastSpell(bot, bot, 19752))
                CastBotSpell(bot, bot, 19752);

            if (bot->GetHealthPct() < 20 && CanCastSpell(bot, bot, 31850))
                CastBotSpell(bot, bot, 31850);

            bot->Attack(target, true);
            break;
        }
        case CLASS_DRUID:
        {
            if (bot->GetShapeshiftForm() != FORM_BEAR && CanCastSpell(bot, bot, 5487))
                CastBotSpell(bot, bot, 5487);

            if (target->GetVictim() != bot && CanCastSpell(bot, target, 5209))
                CastBotSpell(bot, target, 5209);

            if (CanCastSpell(bot, target, 48574))
                CastBotSpell(bot, target, 48574);

            bot->Attack(target, true);
            break;
        }
        default:
            bot->Attack(target, true);
            break;
    }
}

void BotPlayerScript::DoHealerCombat(Player* bot, Unit* target, BotRoleAnalysis const& analysis)
{
    Group* group = bot->GetGroup();
    if (group)
    {
        Player* lowest = nullptr;
        float lowestPct = 100.0f;
        for (auto const& slot : group->GetMemberSlots())
        {
            Player* member = ObjectAccessor::FindConnectedPlayer(slot.guid);
            if (!member || !member->IsAlive())
                continue;
            float pct = member->GetHealthPct();
            if (pct < lowestPct)
            {
                lowestPct = pct;
                lowest = member;
            }
        }

        if (lowest && lowestPct < 70)
        {
            uint8 cls = bot->getClass();
            switch (cls)
            {
                case CLASS_PRIEST:
                    if (CanCastSpell(bot, lowest, 2060))
                        CastBotSpell(bot, lowest, 2060);
                    break;
                case CLASS_PALADIN:
                    if (CanCastSpell(bot, lowest, 635))
                        CastBotSpell(bot, lowest, 635);
                    break;
                case CLASS_SHAMAN:
                    if (CanCastSpell(bot, lowest, 49276))
                        CastBotSpell(bot, lowest, 49276);
                    break;
                case CLASS_DRUID:
                    if (CanCastSpell(bot, lowest, 5186))
                        CastBotSpell(bot, lowest, 5186);
                    break;
            }
            return;
        }
    }

    DoRangedDPSCombat(bot, target, analysis);
}

void BotPlayerScript::DoMeleeDPSCombat(Player* bot, Unit* target, BotRoleAnalysis const& analysis)
{
    uint8 cls = bot->getClass();

    switch (cls)
    {
        case CLASS_WARRIOR:
        {
            uint32 rage = bot->GetPower(POWER_RAGE);
            if (rage >= 30 && CanCastSpell(bot, target, 23881))
                CastBotSpell(bot, target, 23881);
            else if (rage >= 25 && CanCastSpell(bot, target, 1680))
                CastBotSpell(bot, target, 1680);
            else if (rage >= 15 && CanCastSpell(bot, target, 78))
                CastBotSpell(bot, target, 78);
            bot->Attack(target, true);
            break;
        }
        case CLASS_ROGUE:
        {
            uint32 energy = bot->GetPower(POWER_ENERGY);
            if (!bot->HasAura(5171) && energy >= 25 && CanCastSpell(bot, bot, 5171))
                CastBotSpell(bot, bot, 5171);
            else if (!target->HasInArc(M_PI, bot) && energy >= 60 && CanCastSpell(bot, target, 53))
                CastBotSpell(bot, target, 53);
            else if (energy >= 45 && CanCastSpell(bot, target, 1752))
                CastBotSpell(bot, target, 1752);
            bot->Attack(target, true);
            break;
        }
        case CLASS_PALADIN:
        {
            if (!target->HasAura(20271) && CanCastSpell(bot, target, 20271))
                CastBotSpell(bot, target, 20271);
            else if (CanCastSpell(bot, target, 35395))
                CastBotSpell(bot, target, 35395);
            if (bot->GetHealthPct() < 30 && CanCastSpell(bot, bot, 635))
                CastBotSpell(bot, bot, 635);
            bot->Attack(target, true);
            break;
        }
        case CLASS_SHAMAN:
        {
            if (bot->GetDistance(target) > 10.0f && CanCastSpell(bot, target, 49238))
                CastBotSpell(bot, target, 49238);
            else if (CanCastSpell(bot, bot, 8512))
                CastBotSpell(bot, bot, 8512);
            if (bot->GetHealthPct() < 30 && CanCastSpell(bot, bot, 49276))
                CastBotSpell(bot, bot, 49276);
            if (bot->GetDistance(target) < 5.0f)
                bot->Attack(target, true);
            break;
        }
        case CLASS_DRUID:
        {
            if (bot->GetShapeshiftForm() == FORM_CAT)
            {
                bot->Attack(target, true);
            }
            else
            {
                if (!target->HasAura(770) && CanCastSpell(bot, target, 770))
                    CastBotSpell(bot, target, 770);
                else if (CanCastSpell(bot, target, 8921))
                    CastBotSpell(bot, target, 8921);
                if (bot->GetHealthPct() < 30 && CanCastSpell(bot, bot, 5186))
                    CastBotSpell(bot, bot, 5186);
            }
            break;
        }
        default:
            bot->Attack(target, true);
            break;
    }
}

void BotPlayerScript::DoRangedDPSCombat(Player* bot, Unit* target, BotRoleAnalysis const& analysis)
{
    uint8 cls = bot->getClass();

    switch (cls)
    {
        case CLASS_MAGE:
        {
            if (bot->GetDistance(target) < 10.0f && CanCastSpell(bot, target, 122))
                CastBotSpell(bot, target, 122);
            if (CanCastSpell(bot, target, 133))
                CastBotSpell(bot, target, 133);
            break;
        }
        case CLASS_HUNTER:
        {
            if (!target->HasAura(1978) && CanCastSpell(bot, target, 1978))
                CastBotSpell(bot, target, 1978);          // Serpent Sting
            else if (CanCastSpell(bot, target, 56641))
                CastBotSpell(bot, target, 56641);         // Steady Shot

            // A hunter is a HYBRID: ranged Auto Shot is the core, but a target
            // that closes to melee range is also hit with the melee weapon.
            // Both auto-attack loops run side by side, exactly like a real
            // hunter (and like a pet that has both melee and ranged attacks).
            // Within melee range we start the MELEE_ATTACKING loop; at any
            // range we keep Auto Shot in the CURRENT_AUTOREPEAT_SPELL slot so
            // the core's _UpdateAutoRepeatSpell fires it every RANGED_ATTACK
            // tick. Do NOT rely on Attack(target, true) alone - that only
            // drives melee, useless at ranged distance.
            if (bot->GetDistance(target) <= bot->GetMeleeRange(target))
                bot->Attack(target, true);                    // melee auto-attack

            if (bot->GetWeaponForAttack(RANGED_ATTACK, true) &&
                !bot->GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL))
                CastBotSpell(bot, target, 75);            // Auto Shot
            break;
        }
        case CLASS_WARLOCK:
        {
            if (!target->HasAura(47809) && CanCastSpell(bot, target, 47809))
                CastBotSpell(bot, target, 47809);
            else if (CanCastSpell(bot, target, 47811))
                CastBotSpell(bot, target, 47811);
            if (bot->GetDistance(target) < 8.0f && CanCastSpell(bot, target, 5782))
                CastBotSpell(bot, target, 5782);
            break;
        }
        case CLASS_PRIEST:
        {
            if (!target->HasAura(589) && CanCastSpell(bot, target, 589))
                CastBotSpell(bot, target, 589);
            else if (CanCastSpell(bot, target, 8092))
                CastBotSpell(bot, target, 8092);
            else if (CanCastSpell(bot, target, 15407))
                CastBotSpell(bot, target, 15407);
            if (bot->GetHealthPct() < 30 && CanCastSpell(bot, bot, 17))
                CastBotSpell(bot, bot, 17);
            if (bot->GetWeaponForAttack(RANGED_ATTACK, true))
                bot->Attack(target, true);
            break;
        }
        case CLASS_SHAMAN:
        {
            if (CanCastSpell(bot, target, 49238))
                CastBotSpell(bot, target, 49238);
            break;
        }
        case CLASS_DRUID:
        {
            if (!target->HasAura(770) && CanCastSpell(bot, target, 770))
                CastBotSpell(bot, target, 770);
            else if (CanCastSpell(bot, target, 8921))
                CastBotSpell(bot, target, 8921);
            if (bot->GetHealthPct() < 30 && CanCastSpell(bot, bot, 5186))
                CastBotSpell(bot, bot, 5186);
            break;
        }
        default:
            bot->Attack(target, true);
            break;
    }
}
