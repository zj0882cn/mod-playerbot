#include "BotCommandBar.h"
#include "AllSpellScript.h"
#include "BotCommon.h"
#include "PlayerBotMgr.h"
#include "BotPlayerScript.h"
#include "ObjectAccessor.h"
#include "Spell.h"
#include "SpellInfo.h"
#include "Unit.h"
#include "WorldSession.h"
#include "Log.h"

// =====================================================================
// 命令法术表：复用现有 DBC 法术 ID（learnSpell 不检查职业/种族，任意 master
// 可临时学会）。图标/名字来自 DBC 原法术（先占位，后续可按需调整 ID 或 DBC）。
// 命令法术被 OnSpellCheckCast 拦截（res=失败），不会真正施放。
// =====================================================================
// 动作条槽位约定(3.3.5): 技能栏1=0-11(常驻,玩家常用不占), 技能栏2=12-23
// (=技能栏1替换页,平时隐藏), 技能栏3=24-35(左下技能条,常显)。
// 【最终位置】命令按钮放技能栏3(24-30)左下技能条, 常显不占技能栏1。
// 84+ 后台预留区客户端不显示(已验证)→放弃。法术ID已在 Spell.dbc 验证有效。
const std::vector<BotBarCommand> g_botBarCommands = {
    { "attack",            133, { 24 } },  // Fireball
    { "follow",            585, { 25 } },  // Smite
    { "stay",              686, { 26 } },  // Shadow Bolt
    { "stance-passive",    589, { 27 } },  // Shadow Word: Pain
    { "stance-defensive",  172, { 28 } },  // Corruption
    { "stance-aggressive", 348, { 29 } },  // Immolate
    { "return",            120, { 30 } },  // Conjure Water
};

bool IsBotBarSpell(uint32 spellId)
{
    for (auto const& cmd : g_botBarCommands)
        if (cmd.spellId == spellId)
            return true;
    return false;
}

// ---- 命令执行（对 master 的全体 bot，单条控全体）----
void ExecuteBotBarCommand(Player* master, uint32 spellId, Unit* explTarget)
{
    if (!master)
        return;

    std::set<ObjectGuid> bots = sPlayerBotMgr->GetBotsByMaster(master->GetGUID());
    uint32 count = 0;

    for (auto const& botGuid : bots)
    {
        Player* bot = ObjectAccessor::FindConnectedPlayer(botGuid);
        if (!bot || !bot->IsInWorld())
            continue;

        switch (spellId)
        {
            case 133: // attack：攻击选中目标（无则用 master 当前目标）
            {
                Unit* target = explTarget;
                if (!target || !target->IsAlive() || !bot->IsValidAttackTarget(target))
                    target = nullptr;
                if (!target)
                {
                    target = master->GetVictim();
                    if (!target || !target->IsAlive())
                    {
                        ObjectGuid tg = master->GetTarget();
                        if (!tg.IsEmpty())
                            target = ObjectAccessor::GetUnit(*master, tg);
                    }
                }
                if (target && target->IsAlive() && bot->IsValidAttackTarget(target))
                {
                    sPlayerBotMgr->SetBotAttackTarget(botGuid, target->GetGUID());
                    sPlayerBotMgr->SetBotCommand(botGuid, PlayerBotMgr::BOT_COMMAND_ATTACK);
                    BotPlayerScript::ExecuteBotAttack(bot, target);
                    ++count;
                }
                break;
            }
            case 585: // follow
                sPlayerBotMgr->SetBotCommand(botGuid, PlayerBotMgr::BOT_COMMAND_FOLLOW);
                sPlayerBotMgr->SetBotAttackTarget(botGuid, ObjectGuid::Empty);
                sPlayerBotMgr->ClearBotStayPosition(botGuid);
                sPlayerBotMgr->SetBotReturnMode(botGuid, false);
                BotPlayerScript::ExecuteBotFollow(bot, master);
                ++count;
                break;
            case 686: // stay
                sPlayerBotMgr->SetBotCommand(botGuid, PlayerBotMgr::BOT_COMMAND_STAY);
                sPlayerBotMgr->SetBotAttackTarget(botGuid, ObjectGuid::Empty);
                sPlayerBotMgr->SetBotStayPosition(botGuid,
                    bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ());
                sPlayerBotMgr->SetBotReturnMode(botGuid, true);
                if (bot->IsInCombat())
                    bot->CombatStop(true);
                bot->InterruptNonMeleeSpells(false);
                bot->GetMotionMaster()->Clear();
                bot->GetMotionMaster()->MoveIdle();
                ++count;
                break;
            case 589: // stance passive
                sPlayerBotMgr->SetBotStance(botGuid, PlayerBotMgr::STANCE_PASSIVE);
                ++count;
                break;
            case 809: // stance defensive
                sPlayerBotMgr->SetBotStance(botGuid, PlayerBotMgr::STANCE_DEFENSIVE);
                ++count;
                break;
            case 348: // stance aggressive
                sPlayerBotMgr->SetBotStance(botGuid, PlayerBotMgr::STANCE_AGGRESSIVE);
                ++count;
                break;
            case 120: // return：传送回 master 身边
                sPlayerBotMgr->ClearBotCommand(botGuid);
                sPlayerBotMgr->SetBotAttackTarget(botGuid, ObjectGuid::Empty);
                bot->InterruptNonMeleeSpells(false);
                bot->GetMotionMaster()->Clear();
                bot->TeleportTo(master->GetMapId(),
                    master->GetPositionX(), master->GetPositionY(),
                    master->GetPositionZ(), master->GetOrientation());
                ++count;
                break;
            default:
                break;
        }
    }

    // 触发反馈：屏幕中央通知 master 触发了什么命令、影响了几个 bot
    const char* cmdName = "?";
    for (auto const& c : g_botBarCommands)
        if (c.spellId == spellId)
        {
            cmdName = c.name;
            break;
        }
    if (WorldSession* session = master->GetSession())
        session->SendAreaTriggerMessage("BotBar: {} ({} bot)", cmdName, count);

    PB_LOG(1, "Bar command '{}' (spell {}) by '{}' executed on {} bot(s)",
        cmdName, spellId, master->GetName(), count);
}

// ---- 激活/取消命令条 ----
void BotBarApply(Player* master)
{
    if (!master)
        return;
    for (auto const& cmd : g_botBarCommands)
    {
        master->learnSpell(cmd.spellId, true);                       // temporary, 不落库
        // addActionButton 默认标 NEW(落库), 但临时法术不落库→重登按钮会因 HasSpell
        // 失败被删。设 UNCHANGED 让按钮也纯内存(不落库), 由登录时重新 apply 恢复。
        for (uint8 slot : cmd.slots)
            if (ActionButton* ab = master->addActionButton(slot, cmd.spellId, ACTION_BUTTON_SPELL))
                ab->uState = ACTIONBUTTON_UNCHANGED;
    }
    master->SendActionButtons(1);
    PB_LOG(1, "Bar apply to '{}': {} commands across {} slots + action bar sent",
        master->GetName(), g_botBarCommands.size(), g_botBarCommands.size() * g_botBarCommands[0].slots.size());
}

void BotBarRemove(Player* master)
{
    if (!master)
        return;
    for (auto const& cmd : g_botBarCommands)
    {
        master->removeSpell(cmd.spellId, SPEC_MASK_ALL, true);       // 只移除临时法术
        for (uint8 slot : cmd.slots)
            master->removeActionButton(slot);
    }
    master->SendActionButtons(1);
    PB_LOG(1, "Bar remove from '{}'", master->GetName());
}

// ---- AllSpellScript 钩子：识别命令法术，执行命令并阻止施法 ----
class BotAllSpellScript : public AllSpellScript
{
public:
    BotAllSpellScript() : AllSpellScript("bot_all_spell") { }

    void OnSpellCheckCast(Spell* spell, bool /*strict*/, SpellCastResult& res) override
    {
        if (!spell)
            return;
        Unit* caster = spell->GetCaster();
        if (!caster || !caster->IsPlayer())
            return;

        SpellInfo const* spellInfo = spell->GetSpellInfo();
        if (!spellInfo || !IsBotBarSpell(spellInfo->Id))
            return;

        Player* master = caster->ToPlayer();
        Unit* target = spell->m_targets.GetUnitTarget();

        // 执行命令，然后让这次施法失败（不真正施放）
        ExecuteBotBarCommand(master, spellInfo->Id, target);
        res = SPELL_FAILED_INTERRUPTED;
    }
};

void AddBotCommandBarScripts()
{
    new BotAllSpellScript();
}
