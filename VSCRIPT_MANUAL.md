# StrikeSense VScript 手册

## 文档版本

- API 文档版本：`2026.07.15`
- 对应程序构建时间戳：`202607151838`
- 适用范围：StrikeSense 内置 VScript 解释器
- 适用场景：围绕 CS2 GSI 状态、玩家自定义触发器、自定义事件编写脚本

## 系统定位

StrikeSense VScript 是用于编写游戏事件脚本的解释型脚本系统，写法尽量贴近 C++。
它适合处理“当某个 GSI 状态变化时，执行某个动作”这一类场景。

脚本目录：
`%UserProfile%/StrikeSense/script`

仓库中的 `vscript_examples` 是示例源码目录，不等于程序运行时只会从这里读脚本。

## 快速上手

如果你是第一次写脚本，建议先只掌握三样东西：

1. 读 GSI 变量，比如 `health`、`round_phase`、`weapon_name`
2. 用 `if(...)` 判断某个条件是否成立
3. 用 `Log(...)`、`Browser(...)`、`Playsnd(...)` 之类的 API 执行动作

最简单的例子：

```cpp
if(health <= 20){
    Log("当前血量过低");
}
```

只在某个状态刚发生时触发一次：

```cpp
if(on:round_phase=="live"){
    Log("本回合刚进入 live");
}
```

只在击杀数发生增长时触发：

```cpp
if(Delta("kills") > 0){
    Log("刚拿到击杀");
}
```

## GSI 变量

### GSI 原始展开变量

GSI 中收到的叶子字段会尽量同步成 `gsi_...` 变量。

示例：

- `gsi_provider_name`：`string`
- `gsi_map_team_ct_score`：`number`
- `gsi_player_state_health`：`number`
- `gsi_player_weapons_weapon_1_ammo_clip`：`number`
- `gsi_field_count`：`number`

### 常用便捷别名变量

当前实现里明确同步的便捷别名变量如下：

| 分类 | 变量 | 类型 |
| --- | --- | --- |
| Provider | `provider_name` | `string` |
| Provider | `provider_appid` | `number` |
| Provider | `provider_version` | `number|string` |
| Provider | `provider_steamid` | `string` |
| Provider | `provider_timestamp` | `number|string` |
| 地图 | `map` | `string` |
| 地图 | `map_mode` | `string` |
| 地图 | `map_phase` | `string` |
| 地图 | `map_round` | `number` |
| 地图 | `map_num_matches_to_win_series` | `number` |
| 队伍 | `team_ct_score` | `number` |
| 队伍 | `team_ct_consecutive_round_losses` | `number` |
| 队伍 | `team_ct_timeouts_remaining` | `number` |
| 队伍 | `team_ct_matches_won_this_series` | `number` |
| 队伍 | `team_t_score` | `number` |
| 队伍 | `team_t_consecutive_round_losses` | `number` |
| 队伍 | `team_t_timeouts_remaining` | `number` |
| 队伍 | `team_t_matches_won_this_series` | `number` |
| 回合 | `round_phase` | `string` |
| 回合 | `round_win_team` | `string` |
| 回合 | `bomb` | `string` |
| 玩家 | `player_name` | `string` |
| 玩家 | `activity` | `string` |
| 玩家 | `steamid` | `string` |
| 玩家 | `team` | `string` |
| 玩家 | `observer_slot` | `number` |
| 玩家状态 | `health` | `number` |
| 玩家状态 | `armor` | `number` |
| 玩家状态 | `helmet` | `bool|number` |
| 玩家状态 | `flashed` | `number` |
| 玩家状态 | `smoked` | `number` |
| 玩家状态 | `burning` | `number` |
| 玩家状态 | `money` | `number` |
| 玩家状态 | `kills` | `number` |
| 玩家状态 | `round_killhs` | `number` |
| 玩家状态 | `equip_value` | `number` |
| 比赛统计 | `mvps` | `number` |
| 比赛统计 | `match_kills` | `number` |
| 比赛统计 | `match_assists` | `number` |
| 比赛统计 | `match_deaths` | `number` |
| 比赛统计 | `match_score` | `number` |
| 当前武器 | `weapon_name` | `string` |
| 当前武器 | `weapon_type` | `string` |
| 当前武器 | `weapon_state` | `string` |
| 当前武器 | `weapon_ammo_clip` | `number` |
| 当前武器 | `weapon_ammo_clip_max` | `number` |
| 当前武器 | `weapon_ammo_reserve` | `number` |

### 上一帧快照变量

当前实现里明确提供的上一帧快照变量如下：

- `prev_round_phase`：`string`
- `prev_kills`：`number`
- `prev_health`：`number`
- `prev_weapon_name`：`string`
- `prev_weapon_type`：`string`
- `prev_weapon_state`：`string`
- `prev_weapon_ammo_clip`：`number`
- `prev_weapon_ammo_reserve`：`number`

### 内部派生变量

当前实现里明确提供的内部/派生变量如下：

- `weapon_fired`：`bool`
- `weapon_reloading`：`bool`
- `weapon_switched`：`bool`
- `weapon_clip_delta`：`number`
- `weapon_reserve_delta`：`number`
- `weapon_fire_count`：`number`
- `weapon_reload_count`：`number`
- `weapon_reserve_drop_count`：`number`
- `death_mute`：`bool`
- `self_alive`：`bool`
- `self_dead_this_round`：`bool`
- `internal_last_phase`：`string`
- `internal_last_kills`：`number`
- `internal_last_mvps`：`number`
- `internal_dead_muted`：`bool`
- `internal_waiting_for_live`：`bool`
- `internal_round_started`：`bool`
- `internal_mvp_candidate_kills`：`number`
- `internal_mvp_pushed_this_round`：`bool`
- `internal_mvps_at_round_start`：`number`
- `internal_gameover_pushed`：`bool`
- `internal_bomb_planted_this_round`：`bool`
- `internal_player_team`：`string`
- `internal_map_mode`：`string`
- `internal_activity`：`string`
- `internal_round_kills`：`number`
- `internal_health`：`number`
- `internal_in_lobby`：`bool`

## 事件辅助 API

这些 API 最适合普通玩家优先上手，因为它们本来就是围绕 GSI 状态变化设计的。

### 变化检测

| API | 参数 | 返回类型 | 说明 |
| --- | --- | --- | --- |
| `Previous(name)` | `string` | `number|string|bool|list|object|void` | 返回上一帧同名变量的值 |
| `Changed(name)` | `string` | `bool` | 当前值是否与上一帧不同 |
| `ChangedTo(name, value)` | `string, any` | `bool` | 是否从别的值切换到了指定值 |
| `Delta(name)` | `string` | `number` | 当前数值减去上一帧数值 |
| `Cooldown(key, ms)` | `string, number` | `bool` | 冷却通过返回 `true`，冷却中返回 `false` |

示例：

```cpp
if(Changed("health")){
    Log("health 变化了");
}

if(ChangedTo("round_phase", "live")){
    Log("本回合刚进入 live");
}

if(Delta("kills") > 0){
    Log("刚拿到击杀");
}

if(Cooldown("live_popup", 3000)){
    Log("3 秒内只会通过一次");
}
```

`Changed(...)` 和 `ChangedTo(...)` 本身就是边沿条件，可以直接用于持续脚本。
系统也兼容 `on:Changed(...)`，但额外的 `on:` 没有必要。

## 第一个实用脚本

```cpp
if(on:round_phase=="live"){
    Log("回合开始");
}

if(Delta("kills") > 0){
    Playsnd("%UserProfile%/StrikeSense/sounds/kill.wav", 0.8, 1);
}

if(health <= 15 && Cooldown("low_hp_warn", 5000)){
    Log("低血量警告");
}
```

## 常规工具 API

| API | 参数 | 返回类型 | 说明 |
| --- | --- | --- | --- |
| `Size(value)` | `any` | `number` | `list` 返回元素数，`object` 返回字段数，`string` 返回长度 |
| `TypeOf(value)` | `any` | `string` | 返回 `number` `string` `bool` `list` `object` `void` |
| `IsVoid(value)` | `any` | `bool` | 判断是否为空值 |
| `HasField(object, field)` | `object, string` | `bool` | 判断对象字段是否存在 |
| `Contains(container, target)` | `string|list|object, any` | `bool` | 文本包含、列表包含、对象字段存在 |
| `StartsWith(text, prefix)` | `string, string` | `bool` | 前缀判断 |
| `EndsWith(text, suffix)` | `string, string` | `bool` | 后缀判断 |
| `Log(text)` | `any` | `bool` | 输出调试信息 |
| `Sleep(ms)` | `number` | `bool` | 阻塞等待 |

## GSI 强相关对象 API

### 全部玩家对象

| API | 参数 | 返回类型 | 说明 |
| --- | --- | --- | --- |
| `GetAllPlayers()` | 无 | `list<object>` | 返回当前 GSI `allplayers` 的完整对象列表 |

每个玩家对象至少包含：

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `slot` | `string` | GSI 玩家槽位键 |
| `name` | `string` | 玩家名 |
| `team` | `string` | 队伍 |
| `match_stats` | `object` | 比赛统计对象 |
| `state` | `object` | 当前状态对象 |
| `weapons` | `object` 或 `list` | 武器结构，取决于当前 GSI 数据形态 |

常见嵌套字段示例：

- `player.state.health`：`number`
- `player.state.armor`：`number`
- `player.match_stats.kills`：`number`

### 当前玩家武器对象

| API | 参数 | 返回类型 | 说明 |
| --- | --- | --- | --- |
| `GetCurrentPlayerWeapons()` | 无 | `list<object>` | 返回当前玩家武器对象列表 |

每个武器对象常见字段：

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `slot` | `string` | 武器槽位键 |
| `name` | `string` | 武器名 |
| `type` | `string` | 武器类型 |
| `state` | `string` | 武器状态 |
| `ammo_clip` | `number` | 当前弹夹 |
| `ammo_clip_max` | `number` | 弹夹上限 |
| `ammo_reserve` | `number` | 备弹 |

列表包含 GSI 当前提供的全部武器；通过 `weapon.state == "active"` 可以寻找手持武器。
如果只需要判断当前手持武器，优先使用始终与活动武器同步的 `weapon_name`、`weapon_type` 和 `weapon_state`。
离开游戏或 GSI 尚未提供 `player.weapons` 时，本 API 返回空列表。

## Steam 与本地环境 API

### 路径与基础环境

| API | 参数 | 返回类型 | 说明 |
| --- | --- | --- | --- |
| `GetSteamPath()` | 无 | `string` | Steam 安装目录 |
| `GetCS2InstallPath()` | 无 | `string` | CS2 安装目录 |
| `GetCS2CfgPath()` | 无 | `string` | `game/csgo/cfg` 目录 |
| `WriteSteamGSIConfig()` | 无 | `bool` | 写入 GSI 配置文件 |

### Steam 用户 ID

| API | 参数 | 返回类型 | 说明 |
| --- | --- | --- | --- |
| `GetSteamUserIDs32()` | 无 | `list<string>` | 本机所有 Steam32 ID |
| `GetSteamUserIDs64()` | 无 | `list<string>` | 本机所有 Steam64 ID |
| `HasSteamUser32(id32)` | `string` | `bool` | 本机是否存在该 Steam32 |
| `HasSteamUser64(id64)` | `string` | `bool` | 本机是否存在该 Steam64 |
| `Steam32To64(id32)` | `string` | `string` | Steam32 转 Steam64 |
| `Steam64To32(id64)` | `string` | `string` | Steam64 转 Steam32 |

### 本地配置读取

| API | 参数 | 返回类型 | 说明 |
| --- | --- | --- | --- |
| `GetSteamLocalConfigPath32(id32)` | `string` | `string` | 对应账号 `localconfig.vdf` 路径 |
| `GetSteamLocalConfigPath64(id64)` | `string` | `string` | 对应账号 `localconfig.vdf` 路径 |
| `GetSteamLaunchOptions32(id32)` | `string` | `string` | 读取 `LaunchOptions` |
| `GetSteamLaunchOptions64(id64)` | `string` | `string` | 读取 `LaunchOptions` |

### 本地账号对象

| API | 参数 | 返回类型 | 说明 |
| --- | --- | --- | --- |
| `GetSteamAccounts()` | 无 | `list<object>` | 读取 `loginusers.vdf` 并结合 `userdata` 生成本地账号对象列表 |

`GetSteamAccounts()` 返回的对象字段如下：

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `account_id32` | `string` | Steam32 ID |
| `steam_id64` | `string` | Steam64 ID |
| `account_name` | `string` | 登录账号名 |
| `persona_name` | `string` | Steam 显示名称 |
| `timestamp` | `string` | 登录记录中的原始时间文本 |
| `userdata_path` | `string` | 本地 `userdata` 路径 |
| `localconfig_path` | `string` | 本地 `localconfig.vdf` 路径 |
| `most_recent` | `bool` | 是否最近登录 |
| `allow_auto_login` | `bool` | 是否允许自动登录 |
| `remember_password` | `bool` | 是否记住密码 |
| `has_userdata` | `bool` | 本机是否存在对应 `userdata` |
| `has_localconfig` | `bool` | 本机是否存在对应 `localconfig.vdf` |

## 窗口、启动与外部交互 API

### 常规系统控制

| API | 参数 | 返回类型 | 权限组 | 说明 |
| --- | --- | --- | --- | --- |
| `CloseGameWindow()` | 无 | `bool` | 常规 | 安全切出 CS2 |
| `KillGameProcess()` | 无 | `bool` | 常规 | 结束 `cs2.exe` |
| `RunGameProcess()` | 无 | `bool` | 常规 | 通过 `steam://run/730` 启动游戏 |
| `ShowGameProcess()` | 无 | `bool` | 常规 | 切回并激活 CS2 |
| `Browser(url, top_after_open, prefer_existing_browser)` | `string, bool, bool` | `bool` | 常规 | 打开网页或优先复用现有浏览器 |
| `Open(target)` | `string` | `bool` | 常规 | 打开文件、目录或目标 |
| `EnsureProcessWindow(process_name, launch_target, activate)` | `string, string, bool` | `bool` | 常规 | 若进程无窗口则尝试启动并置顶 |
| `Top(process_name, activate)` | `string, bool` | `bool` | 常规 | 置顶指定进程窗口 |

### 高权限文件与命令

| API | 参数 | 返回类型 | 权限组 | 说明 |
| --- | --- | --- | --- | --- |
| `ShellExecute(command)` | `string` | `bool` | 高权限 | 调用 shell 命令 |
| `CFile(path, content)` | `string, string` | `bool` | 高权限 | 创建或覆盖文件 |
| `OwriteFile(path, content)` | `string, string` | `bool` | 高权限 | 覆盖写入 |
| `AwriteFile(path, content)` | `string, string` | `bool` | 高权限 | 追加写入 |
| `DFile(path)` | `string` | `bool` | 高权限 | 删除文件 |

## 图像、音频、音量与准星 API

### 图像与音频

| API | 参数 | 返回类型 | 说明 |
| --- | --- | --- | --- |
| `Drawimg(path, x, y, alpha_channel, opacity, ttl_ms, id)` | `string, int, int, bool, float, int, int` | `bool` | 绘制图片 |
| `Closeimg(id)` | `int` | `bool` | 关闭图片 |
| `Playsnd(path, volume, id)` | `string, float, int` | `bool` | 播放音频 |
| `Stopsnd(id)` | `int` | `bool` | 停止音频 |

### 音量控制

| API | 参数 | 返回类型 | 说明 |
| --- | --- | --- | --- |
| `SetProcessVolume(process_name, volume_percent)` | `string, float` | `bool` | 设置进程音量，百分比范围 `0` 到 `100` |
| `SetProcessMute(process_name, muted)` | `string, bool` | `bool` | 设置进程静音 |
| `SetDeathVolume(value)` | `float` | `bool` | 设置死亡静音音量参数，范围 `0.0` 到 `1.0` |
| `SetDeathMute(enabled)` | `bool` | `bool` | 开关死亡静音功能；开启失败（例如无管理员权限）返回 `false` |
| `SetQuickStopEnabled(enabled)` | `bool` | `bool` | 实时开关自动急停；开启要求超频模式已启用 |
| `GetQuickStopEnabled()` | 无 | `bool` | 读取自动急停总开关状态 |
| `SetQuickStopPaused(paused)` | `bool` | `bool` | 暂停或恢复自动急停脉冲执行 |
| `GetQuickStopPaused()` | 无 | `bool` | 读取自动急停暂停状态 |

### 准星控制

| API | 参数 | 返回类型 | 说明 |
| --- | --- | --- | --- |
| `SetCrosshairEnabled(enabled)` | `bool` | `bool` | 开关准星功能 |
| `SetCrosshairVisual(r, g, b, style, thickness, scale)` | `int, int, int, int, int, float` | `bool` | 设置准星显示参数 |
| `SetCrosshairConfig(r, g, b, style, thickness, scale)` | 同上 | `bool` | 与 `SetCrosshairVisual` 同义 |
| `SetCrosshair(enabled, r, g, b, style, thickness, scale)` | `bool, int, int, int, int, int, float` | `bool` | 一次性设置准星开关与样式 |

## 权限与安全模型

### 权限等级

- `user`
  - 普通用户模式。
  - 默认仅允许安全 API。
  - 即使脚本头部声明了 `@modifier: self_user=...`，也不会因此获得高权限。
- `userdebug`
  - 内测/调试模式。
  - 可以执行高权限 API。
  - 若脚本声明了 `self_user`，仍然会继续校验当前 Windows 用户是否匹配。
- `oem`
  - 与 `userdebug` 等价的高权限能力层。
  - 本质上属于程序侧主动解锁的高级权限，不是脚本侧自报即可获得的权限。
- `eng`
  - 工程构建环境。
  - 能力最高，用于开发与调试。

### 关键规则

- `@modifier: self_user=用户名` 只用于“附加限制”，不作为“提权依据”。
- `user` 模式下，高权限 API 必须通过程序侧能力解锁后才能执行。
- `self_user` 的语义是“这个脚本只允许指定 Windows 用户使用”。

### 高权限 API 分组

以下 API 属于高权限能力：

- `ShellExecute(command)`
- `CFile(path, content)`
- `OwriteFile(path, content)`
- `AwriteFile(path, content)`
- `DFile(path)`

## 元信息

脚本头部支持以下元信息：

```cpp
// @name: 语法总览示例
// @author: jingy
// @provider: StrikeSense
// @version: 2026.07.04
// @notice: 这是一个演示脚本
// @modifier: self_user=jingy
```

### 字段说明

| 字段 | 类型 | 作用 |
| --- | --- | --- |
| `@name` | `string` | 脚本显示名 |
| `@author` | `string` | 作者名 |
| `@provider` | `string` | 来源标识 |
| `@version` | `string` | 脚本版本 |
| `@notice` | `string` | UI 提示说明 |
| `@modifier` | `string` | 额外限制条件，目前支持 `self_user=<Windows用户名>` |

## 语法规则

### 基础风格

- 普通语句通常以 `;` 结尾。
- 控制块结尾的 `;` 可以省略。
- 函数定义结尾的 `;` 可以省略。
- `{ ... }` 后面不必强行补 `;`。
- 支持 `//` 行注释。
- 支持 `/* ... */` 块注释。

### 当前支持的变量类型

- `int`
- `float`
- `double`
- `string`
- `bool`
- `void`
- `auto`
- `vector<T>`
- `array<T>`
- 运行时对象：`list`
- 运行时对象：`object`

### `auto` 规则

`auto` 会尽量保留原值形态，不会强行把复杂对象压扁成字符串。

- 如果函数返回 `list`，`auto` 仍然是 `list`
- 如果函数返回 `object`，`auto` 仍然是 `object`
- 如果函数返回数字、布尔或字符串，`auto` 保持对应基础值

### `const`

现已支持 `const` 变量声明，且不是纯装饰语法，而是有实际写保护。

```cpp
const int aliveLine = 1;
const string modeName = "match";
```

一旦声明为 `const`，后续再赋值会被拒绝。

### 运算符

当前支持：

- 赋值：`=`
- 算术：`+` `-` `*` `/` `%`
- 比较：`==` `!=` `>` `<` `>=` `<=`
- 逻辑：`&&` `||` `!`
- 复合赋值：`+=` `-=` `*=` `/=` `%=` 
- 自增自减：`i++` `++i` `i--` `--i`

比较与逻辑表达式不仅能写在 `if`、`while` 中，也能用于变量初始化、赋值、函数参数和 `return`。

示例：

```cpp
int count = 7;
count %= 3;

if(count % 2 == 1){
    Log("奇数");
}
```

### 容器语法

```cpp
vector<int> nums = { 1, 2, 3 };
array<string> states = { "idle", "live", "over" };

int first = nums[0];
string phase = states[1];
```

### 对象与字段访问

当 API 返回 `object` 或 `list<object>` 时，可以使用点访问和索引访问：

```cpp
auto accounts = GetSteamAccounts();
auto first = accounts[0];

Log(first.persona_name);
Log(first["account_name"]);
Log(accounts[0].steam_id64);
```

### 控制流

当前支持：

- `if`
- `else if`
- `elseif`
- `else`
- `while`
- `for`
- `goto`
- `break`
- `continue`
- `return`

### `goto`

`goto` 会先在当前 block 查找目标标签，当前 block 找不到时会继续向外层传播。

```cpp
for(int i=0; i<10; i++){
    if(i == 6){
        goto done;
    }
}

done:
Log("跳转完成");
```

### `return`

- 顶层脚本的 `return;` 只结束当前脚本
- 函数内的 `return expr;` 会把值返回给调用方
- 子脚本或函数的 `return` 不会把其他脚本一起截断

```cpp
int Sum3(int a, int b, int c){
    return a + b + c;
}
```

### `on:` 边沿触发

`on:` 表示条件从假变真时只触发一次：

```cpp
if(on:round_phase=="live"){
    Log("刚进入 live");
}
```

## 函数

### 函数定义

```cpp
int Add(int a, int b){
    return a + b;
}

bool IsAlive(){
    return health > 0;
}

void Ping(){
    Log("ping");
}
```

### 建议使用的返回类型

- `int`
- `float`
- `double`
- `string`
- `bool`
- `void`
- `auto`
- `vector<T>`
- `array<T>`

### 函数返回值示例

```cpp
string JudgeState(int hp){
    if(hp <= 0){
        return "dead";
    }
    return "alive";
}
```

## 内建函数总表

下面这份清单与当前 `ExecuteFunction(...)` 实现一一对应，可直接用于核对当前版本到底有哪些可调用函数。

### 数据与判断

- `Size`
- `TypeOf`
- `IsVoid`
- `HasField`
- `Contains`
- `StartsWith`
- `EndsWith`
- `Previous`
- `Changed`
- `ChangedTo`
- `Delta`
- `Cooldown`
- `Log`
- `Sleep`

### Steam 与本地环境

- `GetSteamPath`
- `GetCS2InstallPath`
- `GetCS2CfgPath`
- `WriteSteamGSIConfig`
- `GetSteamUserIDs32`
- `GetSteamUserIDs64`
- `HasSteamUser32`
- `HasSteamUser64`
- `Steam32To64`
- `Steam64To32`
- `GetSteamLocalConfigPath32`
- `GetSteamLocalConfigPath64`
- `GetSteamLaunchOptions32`
- `GetSteamLaunchOptions64`
- `GetSteamAccounts`

### GSI 对象

- `GetAllPlayers`
- `GetCurrentPlayerWeapons`

### 窗口、启动与控制

- `CloseGameWindow`
- `KillGameProcess`
- `RunGameProcess`
- `ShowGameProcess`
- `Browser`
- `Open`
- `EnsureProcessWindow`
- `Top`

### 图像与音频

- `Drawimg`
- `Closeimg`
- `Playsnd`
- `Stopsnd`

### 音量与准星

- `SetProcessVolume`
- `SetProcessMute`
- `SetDeathVolume`
- `SetDeathMute`
- `SetQuickStopEnabled`
- `GetQuickStopEnabled`
- `SetQuickStopPaused`
- `GetQuickStopPaused`
- `SetCrosshairEnabled`
- `SetCrosshairVisual`
- `SetCrosshairConfig`
- `SetCrosshair`

### 高权限

- `ShellExecute`
- `CFile`
- `OwriteFile`
- `AwriteFile`
- `DFile`

## 缺失值规则

当某个字段当前不存在时，脚本层会得到：

`void`

建议写法：

```cpp
if(!IsVoid(player_name)){
    Log(player_name);
}
```

## 综合示范脚本

下面这个脚本覆盖了当前最常用的语法与 API：

```cpp
const int aliveLine = 1;
vector<int> marks = { 1, 2, 3 };
array<string> states = { "idle", "live", "over" };
double weight = 1.5;
auto retry = 0;

int Sum3(int a, int b, int c){
    return a + b + c;
}

string JudgeAccount(auto account){
    if(account.most_recent){
        return "最近登录账号";
    }else if(account.allow_auto_login){
        return "允许自动登录账号";
    }elseif(Contains(account.account_name, "alt")){
        return "像是备用账号";
    }else{
        return "普通账号";
    }
}

for(int i=0; i<6; i++){
    retry += 1;
    if(i % 2 == 0){
        continue;
    }else if(i == 5){
        goto steam_demo;
    }
}

steam_demo:
retry %= 3;

if(ChangedTo("round_phase", "live") && Cooldown("round_live_tip", 3000)){
    Log("round_phase 切到了 live");
}

if(Delta("kills") > 0){
    Log("本帧击杀增量=" + Delta("kills"));
}

auto accounts = GetSteamAccounts();
for(int i=0; i<Size(accounts); i++){
    auto account = accounts[i];
    Log("account_name=" + account.account_name);
    Log("分类结果=" + JudgeAccount(account));
}

return;
```

## Steam 多元对象提取示例

这个例子专门演示如何从复杂对象里抽字段，再做 `if / else if / elseif / else`：

```cpp
string DescribeAccount(auto account){
    if(account.most_recent){
        return "最近登录";
    }else if(account.allow_auto_login){
        return "允许自动登录";
    }elseif(account.remember_password){
        return "记住密码";
    }else{
        return "普通账号";
    }
}

auto accounts = GetSteamAccounts();
for(int i=0; i<Size(accounts); i++){
    auto account = accounts[i];
    Log(account.persona_name);
    Log(account.steam_id64);
    Log(DescribeAccount(account));
}
```

## 示例文件

仓库中当前示例脚本：

- `vscript_examples/syntax_showcase.vscript`
- `vscript_examples/steam_accounts_showcase.vscript`
- `vscript_examples/quickstop_snipers_only.vscript`
- `vscript_examples/sniper_crosshair.vscrpit`

文件选择器同时兼容标准扩展名 `.vscript` 和早期示例使用的拼写 `.vscrpit`。
