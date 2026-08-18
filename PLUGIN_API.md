# mod-playerbot 插件支持协议（Plugin API）

> **用途**：定义 mod-playerbot 服务端对插件（如 BotCommander）的**机器可读输出协议**。
> 插件按本文档解析即可稳定工作。**看文档即明白输出标准。**

---

## 一、通用规则

| 规则 | 说明 |
|------|------|
| **前缀** | 每条机器可读输出以 `KEY;` 开头，KEY 全大写 |
| **分隔符** | 统一用分号 `;`；**禁止用 `|`**（`|` 是 WoW 聊天转义符） |
| **颜色码** | 机器可读行**不含** `|c...|r` 颜色码 |
| **在线状态** | `1` = 在线，`0` = 离线 |
| **主人** | 无主为 `None`；主人离线为 `Offline Master`；否则为主人角色名 |
| **姿态** | `defensive` / `passive` / `aggressive`（全小写） |
| **GUID** | 纯 GUID 字符串，如 `Player-0-00000002`；无目标时用 `*` 占位 |
| **传输** | 游戏内经 `/bot ...` 发出，服务端以**系统消息**返回；插件监听 `CHAT_MSG_SYSTEM`，按 `KEY;` 前缀分流 |

---

## 二、命令协议

### 1️⃣ 列表 `bot list`
请求：`/bot list`
响应：
```text
BOTLIST;<total>
BOT;<guid>;<name>;<online>;<master>;<stance>
BOTSTATE;<guid>;<master>;<stance>;<command>;<returnMode>;<attackTarget>;<skillCursor>;<level>;<hpPct>;<mapId>;<inCombat>;<victim>
```
> `BOTSTATE` 行仅在服务端开启 `PlayerBot.DebugLevel >= 3` 时输出，插件应忽略未知前缀。

**示例：**
```text
BOTLIST;4
BOT;Player-0-00000002;Dzrennv;1;测木;defensive
BOT;Player-0-00000003;Qsrennv;1;测木;defensive
BOT;Player-0-00000004;Fsrennv;0;None;passive
BOT;Player-0-00000005;Lrarnv;1;None;aggressive
```

---

### 2️⃣ 标记 `bot set <name>`
响应：
```text
BOTSET;<guid>;<name>;ok;<total>          # 标记成功
BOTSET;<guid>;<name>;already;<total>     # 已是 bot
BOTSET;*;<name>;notfound                 # 角色不存在
```
**示例：**
```text
BOTSET;Player-0-00000002;Dzrennv;ok;4
BOTSET;Player-0-00000002;Dzrennv;already;4
BOTSET;*;张三;notfound
```

---

### 3️⃣ 移除 `bot remove <name>`
响应：
```text
BOTREMOVE;<guid>;<name>;ok;<total>       # 移除成功
BOTREMOVE;<guid>;<name>;notbot;<total>   # 非 bot
BOTREMOVE;*;<name>;notfound              # 角色不存在
```
**示例：**
```text
BOTREMOVE;Player-0-00000002;Dzrennv;ok;3
BOTREMOVE;Player-0-00000002;Dzrennv;notbot;3
BOTREMOVE;*;张三;notfound
```

---

### 4️⃣ 查看主人 `bot master <name>`
响应：
```text
BOTMASTER;<guid>;<name>;<master>         # master 或 None / Offline Master
BOTMASTER;*;<name>;notfound              # 角色不存在
```
**示例：**
```text
BOTMASTER;Player-0-00000002;Dzrennv;测木
BOTMASTER;Player-0-00000004;Fsrennv;None
BOTMASTER;*;张三;notfound
```

---

### 5️⃣ 清除主人 `bot clearmaster <name>`
响应：
```text
BOTCLEARMASTER;<guid>;<name>;ok
BOTCLEARMASTER;*;<name>;notfound
```
**示例：**
```text
BOTCLEARMASTER;Player-0-00000002;Dzrennv;ok
BOTCLEARMASTER;*;张三;notfound
```

---

### 6️⃣ 姿态 `bot stance [name] [d|p|a]`
请求参数：姿态支持全称或首字母（`defensive|d`、`passive|p`、`aggressive|a`）。
响应：
```text
BOTSTANCE;<guid>;<name>;<stance>;ok      # 单个 bot
BOTSTANCE;all;*;<stance>;<count>         # 省略名字=设置全体
BOTSTANCE;*;<name>;<stance>;error        # 失败（非 bot/无权限）
```
**示例：**
```text
BOTSTANCE;Player-0-00000002;Dzrennv;aggressive;ok
BOTSTANCE;all;*;defensive;3
BOTSTANCE;*;Dzrennv;passive;error
```

---

### 7️⃣ 行动 `bot attack|follow|stay|return [name]`
响应：
```text
BOTACTION;<guid>;<name>;<action>;ok      # action = attack/follow/stay/return
BOTACTION;*;<name>;<action>;error
```
**示例：**
```text
BOTACTION;Player-0-00000002;Dzrennv;attack;ok
BOTACTION;*;Dzrennv;follow;error
```

---

### 8️⃣ 技能 `bot spell [name] <1-12>`
响应：
```text
BOTSPELL;<guid>;<name>;<slot>;ok
BOTSPELL;*;<name>;<slot>;error
```
**示例：**
```text
BOTSPELL;Player-0-00000002;Dzrennv;3;ok
BOTSPELL;*;Dzrennv;9;error
```

---

### 9️⃣ 自动施法 `bot autospell [name] <1-12>`
响应：
```text
BOTAUTOSPELL;<guid>;<name>;<slot>;<1/0>   # 1=开启 0=关闭
BOTAUTOSPELL;*;<name>;<slot>;error
```
**示例：**
```text
BOTAUTOSPELL;Player-0-00000002;Dzrennv;3;1
BOTAUTOSPELL;Player-0-00000002;Dzrennv;3;0
```

---

### 🔟 技能查看 `bot skill [name] pool|info`
响应：
```text
BOTSKILL;<guid>;<name>;<detail>          # detail 为技能/定位描述
BOTSKILL;*;<name>;error
```
**示例：**
```text
BOTSKILL;Player-0-00000002;Dzrennv;melee;warrior;3
```

---

### ⚠️ 通用错误
```text
BOTERROR;<command>;<message>
```
**示例：**
```text
BOTERROR;stance;仅主人的机器人可更改姿态
```

---

## 三、插件解析参考（Lua 示例）

```lua
local function HandleBotSystemMessage(msg)
    local key, rest = msg:match("^(%u+);(.*)$")
    if not key then return end
    if key == "BOTLIST" then
        total = tonumber(rest)
    elseif key == "BOT" then
        local guid, name, online, master, stance = strsplit(";", rest)
        -- 更新机器人列表行
    elseif key == "BOTSTATE" then
        -- 忽略或用于详细状态
    elseif key == "BOTSET" then
        local guid, name, result, total = strsplit(";", rest)
    end
end
frame:RegisterEvent("CHAT_MSG_SYSTEM")
frame:SetScript("OnEvent", function(_, _, msg) HandleBotSystemMessage(msg) end)
```

---

## 四、服务端实现状态

| 命令 | 格式 | 游戏内 | 控制台 | 备注 |
|------|------|--------|--------|------|
| list | BOTLIST/BOT/BOTSTATE | ✅ 已实现 | ⏳ 待对齐 | 游戏内已标准 |
| set | BOTSET | ⏳ 待实现 | ⏳ 待实现 | 当前人类可读 |
| remove | BOTREMOVE | ⏳ 待实现 | ⏳ 待实现 | 当前人类可读 |
| master | BOTMASTER | ⏳ 待实现 | ⏳ 待实现 | 当前人类可读 |
| clearmaster | BOTCLEARMASTER | ⏳ 待实现 | ⏳ 待实现 | 当前人类可读 |
| stance | BOTSTANCE | ⏳ 待实现 | — | 当前人类可读 |
| attack/follow/stay/return | BOTACTION | ⏳ 待实现 | — | 当前人类可读 |
| spell | BOTSPELL | ⏳ 待实现 | — | 当前人类可读 |
| autospell | BOTAUTOSPELL | ⏳ 待实现 | — | 当前人类可读 |
| skill | BOTSKILL | ⏳ 待实现 | — | 当前人类可读 |
