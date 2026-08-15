# mod-playerbot

AzerothCore 玩家机器人(PlayerBot)模块,版本 `v2.1.0.2`。

将玩家标记为机器人后,该角色会自动跟随主人(Master)、协助战斗。机器人**不会**自动接受组队邀请,需要玩家在客户端手动点击确认;加入队伍后,队长自动成为其主人。

## 游戏内命令

游戏内命令通过聊天框输入,GM 命令需要 `SEC_GAMEMASTER`(GM3)权限。

| 命令 | 权限 | 说明 |
| --- | --- | --- |
| `/bot set $name` | GM3 | 将在线玩家标记为机器人 |
| `/bot remove $name` | GM3 | 将玩家从机器人列表移除 |
| `/bot list` | GM3 | 列出所有机器人及在线状态 |
| `/bot master $name` | GM3 | 查看机器人的主人 |
| `/bot clearmaster $name` | GM3 | 清除机器人的主人 |
| `/bot stance [名字] <姿态>` | GM3 或该机器人的主人 | 设置机器人姿态;省略名字则设置自己名下所有机器人,`<姿态>` 支持完整名或首字母简写 |

### 命令示例与说明

`$name` 为角色名,命令只会查找**在线**玩家。不带参数输入 `/bot` 会在聊天框显示完整帮助信息。

- `/bot set 张三` — 将在线玩家「张三」标记为机器人。标记后该角色会收到提示,登录时默认进入 `Defensive` 姿态。
- `/bot remove 张三` — 将「张三」从机器人列表移除,同时清除其主人与姿态设置。
- `/bot list` — 列出所有机器人,显示在线状态、主人与姿态。
- `/bot master 张三` — 查看机器人「张三」当前的主人(主人离线时显示 `Offline`)。
- `/bot clearmaster 张三` — 清除机器人「张三」的主人,机器人随即停止跟随。
- `/bot stance 张三 aggressive` — 将「张三」的姿态设为「攻击」。
- `/bot stance aggressive` — **省略名字**,将自己名下所有机器人的姿态设为「攻击」(GM 执行则作用于所有机器人)。
- 姿态参数支持完整名或首字母简写(`d` = `defensive`、`p` = `passive`、`a` = `aggressive`)。该命令除 GM3 外,机器人的主人也可以执行。

### 姿态说明

`/bot stance` 的第三个参数(姿态)支持完整名称或首字母简写,两者效果相同:

| 完整写法 | 简写 | 行为 |
| --- | --- | --- |
| `defensive` | `d` | 默认姿态。自身被攻击时反击;主人有目标时协助攻击 |
| `passive` | `p` | 仅在被攻击时反击,不主动协助 |
| `aggressive` | `a` | 主动攻击主人的当前目标 |

用法示例(以下每组命令完全等价):

```text
/bot stance 张三 defensive      /bot stance 张三 d
/bot stance 张三 passive        /bot stance 张三 p
/bot stance 张三 aggressive     /bot stance 张三 a
```

省略机器人名字时,作用于自己名下所有机器人:

```text
/bot stance d          # 名下所有机器人切换为 Defensive
/bot stance p          # 名下所有机器人切换为 Passive
/bot stance a          # 名下所有机器人切换为 Aggressive
```

## 服务器控制台命令

在 worldserver 控制台输入,无需 `bot` 命令前缀以外的权限配置:

```text
bot set <playername>      将玩家标记为机器人
bot remove <playername>   将玩家从机器人列表移除
bot list                  列出所有机器人
```

## 机器人行为

- 机器人登录后默认处于 `Defensive` 姿态。
- 机器人**不自动接受组队邀请**,必须由玩家手动确认。
- 机器人加入队伍后,队长自动成为其主人。
- 机器人跟随主人移动;主人离线超过 5 分钟,机器人自动离队并清除主人。
- 多个机器人跟随同一主人时,会按各自 GUID 分散在主人身后不同角度与距离,避免站位重叠。
- 机器人会根据天赋、装备判断定位(坦克 / 治疗 / 近战 / 远程)并自动施放对应技能。

## 编译

在项目根目录执行:

```bash
./acore.sh compiler build
```

如需清理后重新编译:

```bash
./acore.sh compiler clean
./acore.sh compiler build
```
