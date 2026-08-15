#include "BotPlayerScript.h"
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

#define PLAYERBOT_VERSION "v2.1.0.2"

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

void BotPlayerScript::DoCombat(Player* bot, Unit* target)
{
    if (!bot || !target || !target->IsAlive())
        return;

    BotRoleAnalysis analysis = AnalyzeBot(bot);
    float preferDist = analysis.preferDistance;
    float distance = bot->GetDistance(target);

    bool isMelee = (analysis.role == BotRoleAnalysis::ROLE_TANK ||
                    analysis.role == BotRoleAnalysis::ROLE_MELEE_DPS);

    if (isMelee)
    {
        // Melee: close in until within melee attack range.
        if (distance > bot->GetMeleeRange(target))
        {
            bot->GetMotionMaster()->MoveChase(target);
            return;
        }
    }
    else
    {
        // Ranged/healer: keep preferred distance.
        if (distance > preferDist + 1.0f)
        {
            bot->GetMotionMaster()->MoveChase(target, preferDist);
            return;
        }
    }

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
                bot->CastSpell(target, 1978, false);
            else if (CanCastSpell(bot, target, 56641))
                bot->CastSpell(target, 56641, false);
            if (bot->GetWeaponForAttack(RANGED_ATTACK, true))
                bot->Attack(target, true);
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
