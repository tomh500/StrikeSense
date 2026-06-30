# StrikeSense VScript 手册

VScript 是 StrikeSense 的轻量脚本系统，用来把 GSI 状态转换成动作。

脚本建议目录：

`%UserProfile%/StrikeSense/script`

程序现在只会确保这个脚本目录存在，不会再自动创建 `assets` 之类的示范资源目录。仓库里的 `vscript_examples` 只是示例源码，发布程序本体保持干净。

## 权限分级

- `user`：稳定版，只有安全命令。
- `userdebug`：对应 nightly / OEM 解锁，可执行 shell 和文件写入。
- `eng`：测试专用，允许更激进的调试与轮询能力。

## 基本语法

- 语句以 `;` 结尾。
- 支持 `//` 与 `/* */` 注释。
- 支持 `if / else / while / for / goto / return`。
- 条件现在支持 `== != > < >= <= && || !`。
- `on:` 表示“整个条件从假变真时只触发一次”。

示例：

```cpp
if(on:weapon_fired==true && weapon_name=="weapon_hkp2000"){
    Playsnd("%USERPROFILE%/StrikeSense/snd/1.wav", 1.0, 2001);
};
```

## 元信息

脚本顶部可写元信息，挂载页优先显示这里的名字和说明：

```cpp
// @name: P2000 开火音效
// @provider: StrikeSense
// @version: 1.0.0
// @notice: 手持 P2000 且开火时播放 1.wav。
```

提示框会按文字量动态调整大小。

## 常用变量

### 便捷变量

- `provider_name`
- `provider_appid`
- `provider_version`
- `provider_steamid`
- `map`
- `map_mode`
- `map_phase`
- `round_phase`
- `round_win_team`
- `bomb`
- `player_name`
- `activity`
- `steamid`
- `team`
- `kills`
- `health`
- `flashed`
- `mvps`
- `death_mute`
- `weapon_name`
- `weapon_type`
- `weapon_state`
- `weapon_ammo_clip`
- `weapon_ammo_clip_max`
- `weapon_ammo_reserve`
- `weapon_fired`
- `weapon_reloading`
- `weapon_switched`

### 上一帧便捷变量

- `prev_round_phase`
- `prev_kills`
- `prev_health`
- `prev_weapon_name`
- `prev_weapon_type`
- `prev_weapon_state`
- `prev_weapon_ammo_clip`

### 原始 GSI 展平变量

收到的 GSI JSON 会尽量完整展开为 `gsi_...` 变量，例如：

- `gsi_player_state_health`
- `gsi_player_weapons_weapon_0_name`
- `gsi_allplayers_7656119xxxx_state_health`

同时 `gsi_server` 侧也会同步缓存整份原型字段，脚本侧可通过这些展平变量直接消费。`gsi_field_count` 表示当前已同步的叶子字段数量。

缺失字段会写成 `void`。

## 内置函数

### 基础动作

```cpp
CloseGameWindow();
KillGameProcess();
RunGameProcess();
ShowGameProcess();
Browser("https://example.com", true);
Top("chrome.exe", true);
Drawimg("path", x, y, alpha, opacity, ttl_ms, id);
Closeimg(id);
Playsnd("path", volume, id);
Stopsnd(id);
Sleep(ms);
Log("调试文本");
```

### userdebug / eng

```cpp
ShellExecute("command");
CFile("path", "content");
DFile("path");
OwriteFile("path", "content");
AwriteFile("path", "content");
```

### StrikeSense 状态控制

```cpp
SetDeathVolume(0.35);
SetDeathMute(true);
SetCrosshairEnabled(true);
SetCrosshairVisual(255, 40, 40, 2, 2, 0.22);
SetCrosshair(true, 255, 40, 40, 2, 2, 0.22); // 兼容旧写法
```

`SetCrosshairEnabled` 和 `SetCrosshairVisual` 现在已拆开，推荐分开使用。

## 示例

### 狙击枪自动准星

```cpp
if(on:(weapon_name=="weapon_awp" || weapon_name=="weapon_ssg08")){
    SetCrosshairVisual(255, 40, 40, 2, 2, 0.22);
    SetCrosshairEnabled(true);
};

if(on:(weapon_name=="weapon_scar20" || weapon_name=="weapon_g3sg1")){
    SetCrosshairVisual(255, 220, 80, 2, 2, 0.22);
    SetCrosshairEnabled(true);
};

if(on:weapon_type!="SniperRifle"){
    SetCrosshairEnabled(false);
};
```

### P2000 开火播放音效

```cpp
if(on:weapon_fired==true && weapon_name=="weapon_hkp2000"){
    Playsnd("%USERPROFILE%/StrikeSense/snd/1.wav", 1.0, 2001);
};
```
