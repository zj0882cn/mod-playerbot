#ifndef _BOT_COMMAND_BAR_H_
#define _BOT_COMMAND_BAR_H_

#include "Player.h"
#include <vector>

// =====================================================================
// P-016 命令动作条（完全内存，法术路线）
//
// 核心机制：
//   - 命令 = 复用一个现有 DBC 法术 ID（职业无关，learnSpell 不检查职业）
//   - master 用 learnSpell(id, temporary=true) 临时学会 → 客户端动作条/法术书
//     正常显示，且 HasActiveSpell 通过（临时法术不写库）
//   - 点按钮 → CMSG_CAST_SPELL → Spell::CheckCast → AllSpellScript::OnSpellCheckCast
//     （模块钩子，不改核心）→ 识别命令法术 → 执行 mod-playerbot 命令 →
//     res = SPELL_FAILED_INTERRUPTED 阻止真正施法
//   - 动作条按钮：addActionButton(slot, spellId, ACTION_BUTTON_SPELL) 塞进
//     普通动作条任意槽位（0-143），SendActionButtons(1) 下发
//   - 完全内存：命令表在代码、临时法术不落玩家数据、无任何新数据库表
// =====================================================================

// 一个命令 = 一个复用的现有法术 + 一个动作条槽位
struct BotBarCommand
{
    const char* name;    // 命令名（日志用）
    uint32 spellId;      // 复用的现有 DBC 法术 ID
    uint8  slot;         // 动作条槽位 0-143
};

extern const std::vector<BotBarCommand> g_botBarCommands;

// 是否命令法术
bool IsBotBarSpell(uint32 spellId);

// 激活命令条: 临时学会所有命令法术 + 塞进动作条 + 下发
void BotBarApply(Player* master);
// 取消命令条: 移除临时法术 + 清空动作条按钮 + 下发
void BotBarRemove(Player* master);

// 执行命令（由 OnSpellCheckCast 钩子调用）
void ExecuteBotBarCommand(Player* master, uint32 spellId);

// 注册 AllSpellScript（OnSpellCheckCast 拦截）
void AddBotCommandBarScripts();

#endif
