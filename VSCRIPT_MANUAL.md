# StrikeSense VScript 手册

## 1. 目标

VScript 是 StrikeSense 的轻量脚本系统。

它做两件事：

1. 把游戏通过 GSI 发来的原始状态同步成脚本变量。
2. 把 StrikeSense 自己推导出来的内部结论，同样同步成脚本变量。

脚本目录：

`%UserProfile%/StrikeSense/script`

程序只会确保脚本目录存在，不会额外生成 `assets` 之类的发布资源目录。仓库里的 `vscript_examples` 只是示范源码。

## 2. 运行能力

- `user`
  - 稳定版能力。
  - 默认只能执行安全函数。
  - 如果脚本使用高权限函数，必须在元信息里声明当前 Windows 用户是作者，才能挂载和执行。
- `userdebug`
  - 对应 nightly / OEM 解锁。
  - 可以挂载和执行高权限脚本。
  - 界面会把高权限脚本标成危险样式。
- `eng`
  - 测试专用。
  - 具备最高脚本能力。

## 3. 元信息

脚本头部支持以下元信息：

```cpp
// @name: 语法总览脚本
// @author: jingy
// @provider: StrikeSense
// @version: 1.0.0
// @notice: 这个脚本只用于说明语法，不建议直接拿来做实际功能。
// @modifier: self_user=jingy
```

字段说明：

- `@name`
  - 用途：挂载列表显示名。
- `@author`
  - 用途：显示作者信息。
- `@provider`
  - 用途：显示脚本来源提供者。
- `@version`
  - 用途：显示脚本版本。
- `@notice`
  - 用途：鼠标悬停 `!` 时显示说明。
- `@modifier`
  - 用途：声明修饰符。
  - 当前支持：
    - `self_user=<Windows用户名>`
  - 作用：
    - 当脚本包含高权限函数时，`user` 模式会检查这个用户名是否等于当前 Windows 用户。
    - 相等则允许挂载和执行，但 UI 会显示危险样式。
    - 不相等则 `user` 模式拒绝挂载。

## 4. 基本语法

- 语句必须以 `;` 结尾。
- 支持 `//` 行注释。
- 支持 `/* ... */` 块注释。
- 支持 `if / else / while / for / goto / return`。
- 支持变量声明：
  - `int`
  - `float`
  - `string`
- 支持条件运算：
  - `==`
  - `!=`
  - `>`
  - `<`
  - `>=`
  - `<=`
  - `&&`
  - `||`
  - `!`
- `on:` 表示整个条件从假变真时只触发一次。

## 5. 唯一示范脚本

下面这个脚本只用于讲透语法，不追求实用：

```cpp
// @name: 语法总览脚本
// @author: jingy
// @provider: StrikeSense
// @version: 1.0.0
// @notice: 用一份脚本展示变量、流程控制、字符串、边沿触发和函数调用。
// @modifier: self_user=jingy

int i = 0;
float total = 0;
string tag = "demo";

if(on:round_phase=="live"){
    Log("回合刚进入 live");
    i = 0;
    total = 0;
};

while(i<3){
    total = total + 0.5;
    i = i + 1;
};

for(int k=0; k<2; k=k+1){
    Log("for 循环仍在运行");
};

if(weapon_name=="weapon_hkp2000" && weapon_clip_delta>0){
    Log("检测到一发开火");
}else{
    Log("当前没有满足 P2000 开火条件");
};

if(!internal_in_lobby && (health>0 || death_mute==false)){
    Top("cs2.exe", true);
};

return;
```

## 6. 函数总表

### 6.1 基础控制函数

`CloseGameWindow()`

- 用途：把 CS2 窗口最小化。
- 参数：无。
- 说明：
  - 当前实现会优先尝试把焦点切到现有浏览器窗口，避免直接最小化独占全屏游戏导致窗口状态异常。
  - 如果没有可安全切出的目标窗口，它会跳过强制最小化。

`KillGameProcess()`

- 用途：结束 `cs2.exe` 进程。
- 参数：无。

`RunGameProcess()`

- 用途：通过 `steam://run/730` 启动游戏。
- 参数：无。

`ShowGameProcess()`

- 用途：恢复并激活 CS2 窗口。
- 参数：无。
- 说明：
  - 当前实现会先定位 CS2 主窗口，再走安全激活流程，尽量降低切回失败或窗口尺寸异常的概率。

`Browser(url, top_after_open, prefer_existing_browser)`

- 用途：用系统默认浏览器打开链接。
- 参数：
  - `url`
    - 类型：`string`
    - 用途：目标地址。
  - `top_after_open`
    - 类型：`bool`
    - 用途：是否在打开后，单次把浏览器窗口抬到最前。
    - 说明：这是单次抬前，不是持续置顶。
  - `prefer_existing_browser`
    - 类型：`bool`
    - 用途：是否优先复用已经打开的浏览器窗口。
    - 说明：传 `true` 时，会先尝试切到已打开的 Edge / Chrome / Firefox 等窗口；只有没找到现成窗口时才打开新链接。

`Open(target)`

- 用途：用系统默认方式打开目标路径、程序或链接。
- 参数：
  - `target`
    - 类型：`string`
    - 示例：`"C:/Program Files/PotPlayer/PotPlayerMini64.exe"`、`"https://www.douyin.com/"`

`EnsureProcessWindow(process_name, launch_target, activate)`

- 用途：如果目标进程已存在，就切到它的窗口；如果不存在，就先启动再切过去。
- 参数：
  - `process_name`
    - 类型：`string`
    - 示例：`"PotPlayerMini64.exe"`
  - `launch_target`
    - 类型：`string`
    - 用途：当进程不存在时用于启动的程序路径、快捷方式或链接。
  - `activate`
    - 类型：`bool`
    - 用途：是否在找到窗口后激活它。
- 说明：
  - 适合配合 `ShowGameProcess()` 做“死后切到播放器 / freezetime 切回游戏”这类脚本。

`Top(process_name, activate)`

- 用途：单次把指定进程的窗口抬到最前。
- 参数：
  - `process_name`
    - 类型：`string`
    - 示例：`"cs2.exe"`、`"chrome.exe"`
  - `activate`
    - 类型：`bool`
    - `true`：抬前并尝试激活。
    - `false`：只做一次窗口层级提升，不强制抢焦点。
- 说明：
  - 不是持续 `TOPMOST`。
  - 只是一次性抬前。

`Drawimg(path, x, y, alpha_channel, opacity, ttl_ms, id)`

- 用途：在屏幕上绘制临时图片窗口。
- 参数：
  - `path`
    - 类型：`string`
    - 用途：图片路径。
  - `x`
    - 类型：`int`
    - 用途：相对屏幕中心的 X 偏移。
  - `y`
    - 类型：`int`
    - 用途：相对屏幕中心的 Y 偏移。
  - `alpha_channel`
    - 类型：`bool`
    - 用途：是否按透明通道处理。
  - `opacity`
    - 类型：`float`
    - 范围：`0.0 ~ 1.0`
  - `ttl_ms`
    - 类型：`int`
    - 用途：显示时长，毫秒。
  - `id`
    - 类型：`int`
    - 用途：图片实例 ID。

`Closeimg(id)`

- 用途：关闭指定图片实例。
- 参数：
  - `id`
    - 类型：`int`

`Playsnd(path, volume, id)`

- 用途：播放音频。
- 参数：
  - `path`
    - 类型：`string`
    - 用途：音频路径。
  - `volume`
    - 类型：`float`
    - 范围：`0.0 ~ 1.0`
  - `id`
    - 类型：`int`
    - 用途：音频实例 ID。

`Stopsnd(id)`

- 用途：停止指定音频实例。
- 参数：
  - `id`
    - 类型：`int`

`Sleep(ms)`

- 用途：暂停脚本执行。
- 参数：
  - `ms`
    - 类型：`int`
    - 用途：等待毫秒数。

`Log(text)`

- 用途：向控制台输出中文调试日志。
- 参数：
  - `text`
    - 类型：`string`

### 6.2 高权限函数

这组函数属于危险函数：

- `ShellExecute(command)`
- `CFile(path, content)`
- `DFile(path)`
- `OwriteFile(path, content)`
- `AwriteFile(path, content)`

说明：

- `userdebug / eng` 可以执行。
- `user` 模式只有在脚本声明了 `@modifier: self_user=<当前Windows用户名>` 时才允许。
- 这些脚本在 UI 中会显示危险样式。

参数说明：

`ShellExecute(command)`

- 用途：调用 `cmd /C` 执行命令。
- 参数：
  - `command`
    - 类型：`string`

`CFile(path, content)`

- 用途：创建或覆盖文件。
- 参数：
  - `path`
    - 类型：`string`
  - `content`
    - 类型：`string`

`DFile(path)`

- 用途：删除文件。
- 参数：
  - `path`
    - 类型：`string`

`OwriteFile(path, content)`

- 用途：覆写文件。
- 参数：
  - `path`
    - 类型：`string`
  - `content`
    - 类型：`string`

`AwriteFile(path, content)`

- 用途：追加写入文件。
- 参数：
  - `path`
    - 类型：`string`
  - `content`
    - 类型：`string`

### 6.3 StrikeSense 状态函数

`SetDeathVolume(value)`

- 用途：修改“死亡后即时降低”音量。
- 参数：
  - `value`
    - 类型：`float`
    - 范围：`0.0 ~ 1.0`

`SetProcessVolume(process_name, volume_percent)`

- 用途：直接修改指定进程当前音频会话的系统合成器音量。
- 参数：
  - `process_name`
    - 类型：`string`
    - 示例：`"cs2.exe"`、`"chrome.exe"`、`"Spotify.exe"`
  - `volume_percent`
    - 类型：`float`
    - 范围：`0.0 ~ 100.0`
- 说明：
  - 这里传的是百分比，不是 `0.0 ~ 1.0` 的系数。
  - 该函数等同于 C++ 层的 `SetProcessVolumeByName(processName, volumePercent)`。
  - 只有目标进程已经创建了音频会话时，系统合成器音量才能被改到。

`SetProcessMute(process_name, muted)`

- 用途：直接修改指定进程当前音频会话的静音状态。
- 参数：
  - `process_name`
    - 类型：`string`
    - 示例：`"msedge.exe"`、`"chrome.exe"`
  - `muted`
    - 类型：`bool`
    - `true` 表示静音，`false` 表示取消静音。
- 说明：
  - 该函数等同于 C++ 层的 `SetProcessMuteByName(processName, muted)`。
  - 适合配合 `ShowGameProcess()` / `Browser(...)` 做浏览器静音与取消静音。

`SetDeathMute(enabled)`

- 用途：开启或关闭“死亡后即时降低”。
- 参数：
  - `enabled`
    - 类型：`bool`

`SetCrosshairEnabled(enabled)`

- 用途：只控制脚本准星开关。
- 参数：
  - `enabled`
    - 类型：`bool`

`SetCrosshairVisual(r, g, b, style, thickness, scale)`

- 用途：只更新脚本准星外观，不负责开关。
- 参数：
  - `r`
    - 类型：`int`
    - 范围：`0 ~ 255`
  - `g`
    - 类型：`int`
    - 范围：`0 ~ 255`
  - `b`
    - 类型：`int`
    - 范围：`0 ~ 255`
  - `style`
    - 类型：`int`
    - 可能值：
      - `0` 空心圆
      - `1` 实心圆
      - `2` 经典十字
  - `thickness`
    - 类型：`int`
    - 范围：`1 ~ 10`
  - `scale`
    - 类型：`float`
    - 范围：`0.1 ~ 0.6`

`SetCrosshair(enabled, r, g, b, style, thickness, scale)`

- 用途：兼容旧写法，一次同时设置开关和外观。
- 建议：新脚本优先拆成 `SetCrosshairEnabled` 和 `SetCrosshairVisual`。

## 7. 变量来源分类

变量分两大类：

1. `GSI 原始变量`
   - 来源：游戏直接发来的 JSON。
   - 特征：能从 GSI 原文里直接找到。
2. `内部变量`
   - 来源：StrikeSense 根据 GSI 和自身状态机推导出的二级结论。
   - 特征：不是游戏直接给的，而是程序算出来的。

## 8. GSI 原始变量

### 8.1 原始展开变量

任何收到的 GSI 字段，都会尽量同步成 `gsi_...` 变量。

例如：

- `gsi_provider_name`
- `gsi_map_team_ct_score`
- `gsi_player_state_health`
- `gsi_player_weapons_weapon_1_ammo_clip`

补充：

- `gsi_field_count`
  - 作用：当前已同步的叶子字段数量。
  - 来源：内部统计，但统计对象是 GSI 原始字段。

### 8.2 便捷别名变量

这些变量本质仍然来自游戏 GSI，但名字更短，脚本更方便写。

- `provider_name`
  - 作用：游戏名称。
  - 可能值：`Counter-Strike: Global Offensive`
  - 来源：游戏 GSI
- `provider_appid`
  - 作用：游戏 AppID。
  - 可能值：`730`
  - 来源：游戏 GSI
- `provider_version`
  - 作用：GSI 提供的版本号。
  - 来源：游戏 GSI
- `provider_steamid`
  - 作用：当前玩家 SteamID。
  - 来源：游戏 GSI
- `provider_timestamp`
  - 作用：GSI 数据时间戳。
  - 来源：游戏 GSI

- `map`
  - 作用：地图名。
  - 可能值：`de_nuke`、`de_mirage`
  - 来源：游戏 GSI
- `map_mode`
  - 作用：模式名。
  - 可能值：`competitive`、`casual`、`deathmatch`
  - 来源：游戏 GSI
- `map_phase`
  - 作用：地图阶段。
  - 来源：游戏 GSI
- `map_round`
  - 作用：当前总回合编号。
  - 来源：游戏 GSI
- `team_ct_score`
- `team_ct_consecutive_round_losses`
- `team_ct_timeouts_remaining`
- `team_ct_matches_won_this_series`
- `team_t_score`
- `team_t_consecutive_round_losses`
- `team_t_timeouts_remaining`
- `team_t_matches_won_this_series`
- `map_num_matches_to_win_series`
  - 作用：都是比分与系列赛相关值。
  - 来源：游戏 GSI

- `round_phase`
  - 作用：当前回合阶段。
  - 可能值：`freezetime`、`live`、`over`、`gameover`
  - 来源：游戏 GSI
- `round_win_team`
  - 作用：回合获胜方。
  - 可能值：`CT`、`T`
  - 来源：游戏 GSI / added.round
- `bomb`
  - 作用：炸弹状态。
  - 可能值：`planted`
  - 来源：游戏 GSI

- `player_name`
- `steamid`
- `team`
- `activity`
- `observer_slot`
  - 作用：当前玩家身份与活动状态。
  - `activity` 可能值：`menu`、`playing`
  - 来源：游戏 GSI

- `health`
- `armor`
- `helmet`
- `flashed`
- `smoked`
- `burning`
- `money`
- `kills`
- `round_killhs`
- `equip_value`
  - 作用：当前玩家状态值。
  - 来源：游戏 GSI

- `mvps`
- `match_kills`
- `match_assists`
- `match_deaths`
- `match_score`
  - 作用：整场比赛统计值。
  - 来源：游戏 GSI

- `weapon_name`
  - 作用：当前激活武器名。
  - 可能值：`weapon_hkp2000`、`weapon_awp`
  - 来源：游戏 GSI
- `weapon_type`
  - 作用：当前激活武器类型。
  - 可能值：`Pistol`、`SniperRifle`、`Knife`
  - 来源：游戏 GSI
- `weapon_state`
  - 作用：当前激活武器状态。
  - 可能值：`active`、`holstered`、`reloading`
  - 来源：游戏 GSI
- `weapon_ammo_clip`
- `weapon_ammo_clip_max`
- `weapon_ammo_reserve`
  - 作用：当前武器弹药信息。
  - 来源：游戏 GSI

### 8.3 上一帧别名变量

- `prev_round_phase`
- `prev_kills`
- `prev_health`
- `prev_weapon_name`
- `prev_weapon_type`
- `prev_weapon_state`
- `prev_weapon_ammo_clip`
- `prev_weapon_ammo_reserve`

作用：

- 提供上一帧的便捷快照，方便直接写比较逻辑。

来源：

- 内部缓存，但内容来自上一帧 GSI。

## 9. 内部变量

这些变量不是游戏直接给的，而是 StrikeSense 根据 GSI 或状态机推导出来的二级结论。

### 9.1 武器事件内部变量

- `weapon_fired`
  - 作用：这一帧检测到当前武器打出子弹。
  - 判定：`weapon_ammo_clip` 比上一帧减少。
  - 可能值：`true / false`
  - 来源：内部推导

- `weapon_reloading`
  - 作用：这一帧检测到当前武器完成一次装填变化。
  - 判定：`weapon_ammo_clip` 比上一帧增加。
  - 可能值：`true / false`
  - 来源：内部推导

- `weapon_switched`
  - 作用：这一帧检测到切枪。
  - 判定：当前 `weapon_name` 与上一帧不同。
  - 来源：内部推导

- `weapon_clip_delta`
  - 作用：弹匣子弹变化量。
  - 判定：`上一帧 ammo_clip - 当前 ammo_clip`
  - 常见数值：
    - `1` 表示打出一发
    - `0` 表示没有变化
    - 负数常见于装弹
  - 来源：内部推导

- `weapon_reserve_delta`
  - 作用：备用弹药变化量。
  - 判定：`上一帧 ammo_reserve - 当前 ammo_reserve`
  - 常见数值：
    - 正数表示备用弹减少
    - `0` 表示无变化
  - 来源：内部推导

- `weapon_fire_count`
  - 作用：本回合累计开火次数。
  - 重置时机：`round_phase` 刚进入 `live`
  - 来源：内部推导

- `weapon_reload_count`
  - 作用：本回合累计检测到的装填次数。
  - 重置时机：`round_phase` 刚进入 `live`
  - 来源：内部推导

- `weapon_reserve_drop_count`
  - 作用：本回合累计检测到的备用弹药下降次数。
  - 重置时机：`round_phase` 刚进入 `live`
  - 来源：内部推导

### 9.2 程序状态内部变量

- `death_mute`
  - 作用：表示“本回合已判定死亡且当前仍处于死亡状态”的程序级结论。
  - 来源：内部推导

- `self_alive`
  - 作用：当前自己是否存活。
  - 来源：程序内部状态机

- `self_dead_this_round`
  - 作用：当前回合内自己是否已经被程序判定为死过一次。
  - 来源：程序内部状态机

- `internal_last_phase`
- `internal_last_kills`
- `internal_last_mvps`
- `internal_dead_muted`
- `internal_waiting_for_live`
- `internal_round_started`
- `internal_mvp_candidate_kills`
- `internal_mvp_pushed_this_round`
- `internal_mvps_at_round_start`
- `internal_gameover_pushed`
- `internal_bomb_planted_this_round`
- `internal_player_team`
- `internal_map_mode`
- `internal_activity`
- `internal_round_kills`
- `internal_health`
- `internal_in_lobby`

这些变量的共同说明：

- 作用：暴露 StrikeSense 在 `gsi_server` 内部维护的状态机结果。
- 来源：内部变量，不是游戏直接字段。
- 用法：
  - 当你想复用程序现成判断，而不是自己重新用一堆 GSI 字段写条件时，可以直接用它们。

## 10. 缺失值

如果字段当前不存在，会写成：

`void`

脚本里判断时要留意这一点。
