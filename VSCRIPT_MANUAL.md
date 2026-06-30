# StrikeSense VScript 手册

VScript 是 StrikeSense 的轻量脚本系统，用来把 GSI 状态转换成可执行动作。脚本文件建议放在：

`%UserProfile%/StrikeSense/script`

程序启动时只会创建脚本目录和资源目录，不再把示范脚本内容写在 C++ 源码里。

当前版本中，示范脚本作为项目里的 `.vscrpit` 文件维护。用户可以把示范脚本复制到 `%UserProfile%/StrikeSense/script` 后挂载。

## 构建权限

- `Release`: `user`
- `Nightly`: `userdebug`
- `Debug`: `eng`

高权限构建可以执行低权限命令。`userdebug` 命令在 `user` 下会被拒绝，除非 OEM 解锁有效。

## 脚本结构

每条语句用 `;` 结尾。字符串支持跨行拼接：

```cpp
Browser("https://www.douyin.com/"
"home", true);
```

支持注释：

```cpp
// 行注释
/*
块注释
*/
```

脚本开头可以写元信息，挂载页面会优先显示脚本名：

```cpp
// @name: 死后刷抖音
// @provider: StrikeSense
// @version: 1.0.0
// @notice: 死亡时打开页面，新回合切回游戏。
```

公告信息过长时不会直接铺在列表里，页面会显示一个感叹号图标，鼠标悬停后显示公告浮窗。

## 条件

普通条件会在每次执行时判断：

```cpp
if(map=="de_nuke"){
    Top("cs2.exe", true);
};
```

`on:` 表示状态刚刚变成目标值时触发一次，适合持续轮询脚本：

```cpp
if(on:death_mute==true){
    Browser("https://www.douyin.com/", true);
};
```

没有 `on:` 的脚本开启持续执行时：

- `user`: 拒绝轮询。
- `userdebug`: 弹窗询问。
- `eng`: 直接允许。

## 控制流

支持最小 C 风格控制流：

```cpp
int i = 0;
while(i<5){
    i = i + 1;
};

for(int k=0; k<3; k=k+1){
    Sleep(50);
};

start:
goto start;
return;
```

循环有 1000 次上限，避免脚本卡死 UI 线程。

## GSI 变量

常用变量：

- `map`: 地图名，例如 `de_nuke`
- `map_mode`: 模式
- `map_phase`: 地图阶段
- `round_phase`: 回合阶段，例如 `freezetime`、`live`、`over`
- `bomb`: 炸弹状态
- `activity`: 玩家活动状态
- `steamid`: 玩家 SteamID
- `team`: 玩家队伍
- `kills`: 本回合击杀数
- `health`: 血量
- `flashed`: 闪光值
- `mvps`: MVP 数
- `death_mute`: 血量小于等于 0 时为 `true`
- `weapon_name`: 当前手持武器名，例如 `weapon_awp`
- `weapon_type`: 当前手持武器类型，例如 `SniperRifle`
- `weapon_state`: 当前武器状态
- `gsi_...`: 原始 GSI JSON 会尽量展开成变量，例如 `gsi_player_state_health`

缺失字段会写成 `void`。

## API

### user 权限

```cpp
CloseGameWindow();
KillGameProcess();
RunGameProcess();
ShowGameProcess();
Browser("url", true);
Top("process.exe", true);
Drawimg("path", x, y, alpha, opacity, ttl_ms, id);
Closeimg(id);
Playsnd("path", volume, id);
Stopsnd(id);
Sleep(ms);
```

`Browser` 的第二个参数可选，`true` 表示尝试把浏览器置顶。`Top` 的第二个参数为 `false` 时取消置顶。

### userdebug 权限

```cpp
ShellExecute("command");
CFile("path", "content");
DFile("path");
OwriteFile("path", "content");
AwriteFile("path", "content");
```

### StrikeSense 状态

```cpp
SetDeathVolume(0.35);
SetDeathMute(true);
SetCrosshair(true, 255, 0, 0, 0, 2, 0.2);
```

这些函数会修改进化分支相关状态并保存配置。需要管理员权限但权限不足时会忽略实际启用动作，避免 UI 状态和真实状态不一致。

## 示例

死亡打开抖音，冻结时间切回游戏：

```cpp
if(on:death_mute==true){
    CloseGameWindow();
    Browser("https://www.douyin.com/", true);
};

if(on:round_phase=="freezetime"){
    ShowGameProcess();
};
```

1 到 5 杀显示图标：

```cpp
if(on:kills==1){ Drawimg("%USERPROFILE%/StrikeSense/script/assets/kills/1.png", 0, 360, true, 1.0, 3000, 101); };
if(on:kills==2){ Drawimg("%USERPROFILE%/StrikeSense/script/assets/kills/2.png", 0, 360, true, 1.0, 3000, 102); };
```

狙击枪自动准星：

```cpp
if(on:weapon_name=="weapon_awp"){
    SetCrosshair(true, 255, 40, 40, 2, 2, 0.22);
};

if(on:weapon_type!="SniperRifle"){
    SetCrosshair(false, 255, 0, 0, 0, 2, 0.2);
};
```

## OEM Key 时间戳

OEM 工具使用 12 位时间戳：

`yyyyMMddHHmm`

例如 `202606301845` 表示 2026 年 6 月 30 日 18:45。
