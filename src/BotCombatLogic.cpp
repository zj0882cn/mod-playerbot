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

    PB_LOG(2, "Bot '{}' DoCombat target '{}' dist {:.1f} chase {} role {}",
        bot->GetName(), target->GetName(), distance, chase, int(analysis.role));

    bool isMelee = (analysis.role == BotRoleAnalysis::ROLE_TANK ||
                    analysis.role == BotRoleAnalysis::ROLE_MELEE_DPS);

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
    if (bot->GetTarget() != target->GetGUID())
        bot->SetTarget(target->GetGUID());
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

    // Skill-bar auto-cast (real client behaviour). CastAutoSpells returns true
    // if it cast something (Auto Shot started and/or an active skill). Only
    // when nothing could be cast do we fall back to the hard-coded class
    // defaults below (action bar empty or everything on cooldown), so a fresh
    // bot still fights. Healers keep their heal-first logic (they are handled
    // by DoHealerCombat below, not by the action bar).
    if (analysis.role != BotRoleAnalysis::ROLE_HEALER && CastAutoSpells(bot, target))
        return;

    switch (analysis.role)
    {
        case BotRoleAnalysis::ROLE_TANK:
            DoTankCombat(bot, target, analysis);
            break;
        case BotRoleAnalysis::ROLE_HEALER:
            DoHealerCombat(bot, target, analysis);
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
                bot->CastSpell(target, 355, false);

            if (rage >= 15 && !target->HasAura(7384) && CanCastSpell(bot, target, 7384))
                bot->CastSpell(target, 7384, false);

            if (CanCastSpell(bot, bot, 2565))
                bot->CastSpell(bot, 2565, false);

            if (rage >= 15 && CanCastSpell(bot, target, 78))
                bot->CastSpell(target, 78, false);

            if (bot->GetHealthPct() < 20 && CanCastSpell(bot, bot, 12975))
                bot->CastSpell(bot, 12975, false);

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
                bot->CastSpell(bot, 25780, false);

            if (target->GetVictim() != bot && CanCastSpell(bot, target, 6940))
                bot->CastSpell(target, 6940, false);

            if (CanCastSpell(bot, target, 20271))
                bot->CastSpell(target, 20271, false);

            if (CanCastSpell(bot, bot, 19752))
                bot->CastSpell(bot, 19752, false);

            if (bot->GetHealthPct() < 20 && CanCastSpell(bot, bot, 31850))
                bot->CastSpell(bot, 31850, false);

            bot->Attack(target, true);
            break;
        }
        case CLASS_DRUID:
        {
            if (bot->GetShapeshiftForm() != FORM_BEAR && CanCastSpell(bot, bot, 5487))
                bot->CastSpell(bot, 5487, false);

            if (target->GetVictim() != bot && CanCastSpell(bot, target, 5209))
                bot->CastSpell(target, 5209, false);

            if (CanCastSpell(bot, target, 48574))
                bot->CastSpell(target, 48574, false);

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
                        bot->CastSpell(lowest, 2060, false);
                    break;
                case CLASS_PALADIN:
                    if (CanCastSpell(bot, lowest, 635))
                        bot->CastSpell(lowest, 635, false);
                    break;
                case CLASS_SHAMAN:
                    if (CanCastSpell(bot, lowest, 49276))
                        bot->CastSpell(lowest, 49276, false);
                    break;
                case CLASS_DRUID:
                    if (CanCastSpell(bot, lowest, 5186))
                        bot->CastSpell(lowest, 5186, false);
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
                bot->CastSpell(target, 23881, false);
            else if (rage >= 25 && CanCastSpell(bot, target, 1680))
                bot->CastSpell(target, 1680, false);
            else if (rage >= 15 && CanCastSpell(bot, target, 78))
                bot->CastSpell(target, 78, false);
            bot->Attack(target, true);
            break;
        }
        case CLASS_ROGUE:
        {
            uint32 energy = bot->GetPower(POWER_ENERGY);
            if (!bot->HasAura(5171) && energy >= 25 && CanCastSpell(bot, bot, 5171))
                bot->CastSpell(bot, 5171, false);
            else if (!target->HasInArc(M_PI, bot) && energy >= 60 && CanCastSpell(bot, target, 53))
                bot->CastSpell(target, 53, false);
            else if (energy >= 45 && CanCastSpell(bot, target, 1752))
                bot->CastSpell(target, 1752, false);
            bot->Attack(target, true);
            break;
        }
        case CLASS_PALADIN:
        {
            if (!target->HasAura(20271) && CanCastSpell(bot, target, 20271))
                bot->CastSpell(target, 20271, false);
            else if (CanCastSpell(bot, target, 35395))
                bot->CastSpell(target, 35395, false);
            if (bot->GetHealthPct() < 30 && CanCastSpell(bot, bot, 635))
                bot->CastSpell(bot, 635, false);
            bot->Attack(target, true);
            break;
        }
        case CLASS_SHAMAN:
        {
            if (bot->GetDistance(target) > 10.0f && CanCastSpell(bot, target, 49238))
                bot->CastSpell(target, 49238, false);
            else if (CanCastSpell(bot, bot, 8512))
                bot->CastSpell(bot, 8512, false);
            if (bot->GetHealthPct() < 30 && CanCastSpell(bot, bot, 49276))
                bot->CastSpell(bot, 49276, false);
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
                    bot->CastSpell(target, 770, false);
                else if (CanCastSpell(bot, target, 8921))
                    bot->CastSpell(target, 8921, false);
                if (bot->GetHealthPct() < 30 && CanCastSpell(bot, bot, 5186))
                    bot->CastSpell(bot, 5186, false);
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
                bot->CastSpell(target, 122, false);
            if (CanCastSpell(bot, target, 133))
                bot->CastSpell(target, 133, false);
            break;
        }
        case CLASS_HUNTER:
        {
            if (!target->HasAura(1978) && CanCastSpell(bot, target, 1978))
                bot->CastSpell(target, 1978, false);          // Serpent Sting
            else if (CanCastSpell(bot, target, 56641))
                bot->CastSpell(target, 56641, false);         // Steady Shot

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
                bot->CastSpell(target, 75, false);            // Auto Shot
            break;
        }
        case CLASS_WARLOCK:
        {
            if (!target->HasAura(47809) && CanCastSpell(bot, target, 47809))
                bot->CastSpell(target, 47809, false);
            else if (CanCastSpell(bot, target, 47811))
                bot->CastSpell(target, 47811, false);
            if (bot->GetDistance(target) < 8.0f && CanCastSpell(bot, target, 5782))
                bot->CastSpell(target, 5782, false);
            break;
        }
        case CLASS_PRIEST:
        {
            if (!target->HasAura(589) && CanCastSpell(bot, target, 589))
                bot->CastSpell(target, 589, false);
            else if (CanCastSpell(bot, target, 8092))
                bot->CastSpell(target, 8092, false);
            else if (CanCastSpell(bot, target, 15407))
                bot->CastSpell(target, 15407, false);
            if (bot->GetHealthPct() < 30 && CanCastSpell(bot, bot, 17))
                bot->CastSpell(bot, 17, false);
            if (bot->GetWeaponForAttack(RANGED_ATTACK, true))
                bot->Attack(target, true);
            break;
        }
        case CLASS_SHAMAN:
        {
            if (CanCastSpell(bot, target, 49238))
                bot->CastSpell(target, 49238, false);
            break;
        }
        case CLASS_DRUID:
        {
            if (!target->HasAura(770) && CanCastSpell(bot, target, 770))
                bot->CastSpell(target, 770, false);
            else if (CanCastSpell(bot, target, 8921))
                bot->CastSpell(target, 8921, false);
            if (bot->GetHealthPct() < 30 && CanCastSpell(bot, bot, 5186))
                bot->CastSpell(bot, 5186, false);
            break;
        }
        default:
            bot->Attack(target, true);
            break;
    }
}
