# StrikeSense VScript 手册

## 1. 定位

StrikeSense VScript 是面向玩家的轻量级解释型脚本系统，目标不是完整复刻 C++ 编译器，而是在尽量接近 C++ 书写习惯的前提下，提供更安全、更直接、更容易调试的游戏脚本能力。

它主要做两件事：

1. 把游戏通过 GSI 推送过来的原始状态同步成脚本变量。
2. 把 StrikeSense 自己在运行期推导出的内部状态、窗口控制、音量控制、Steam 环境信息也暴露给脚本。

脚本目录：

`%UserProfile%/StrikeSense/script`

仓库里的 `vscript_examples` 只是示例源码目录，不代表程序运行时一定从这里读取。

## 2. 运行能力与权限

### 2.1 运行能力等级

- `user`
  - 面向正式稳定使用。
  - 默认只允许安全 API。
  - 如果脚本用到了高权限 API，必须通过元信息明确声明作者 Windows 用户名。
- `userdebug`
  - 对应更高权限的调试环境。
  - 可以挂载并执行高权限脚本。
- `eng`
  - 工程测试环境。
  - 具备最高脚本能力。

### 2.2 高权限 API

以下 API 属于高权限能力：

- `ShellExecute(command)`
- `CFile(path, content)`
- `DFile(path)`
- `OwriteFile(path, content)`
- `AwriteFile(path, content)`

这些 API 在 `user` 能力下不会直接默认开放。

## 3. 元信息

脚本头部支持以下元信息：

```cpp
// @name: 语法总览脚本
// @author: jingy
// @provider: StrikeSense
// @version: 2026.07.04
// @notice: 这个脚本用于说明语法和 API
// @modifier: self_user=jingy
```

### 3.1 字段说明

#### 3.1.1 `@name`

- 类型：`string`
- 作用：脚本显示名称

#### 3.1.2 `@author`

- 类型：`string`
- 作用：脚本作者

#### 3.1.3 `@provider`

- 类型：`string`
- 作用：脚本来源

#### 3.1.4 `@version`

- 类型：`string`
- 作用：脚本版本号

#### 3.1.5 `@notice`

- 类型：`string`
- 作用：用于界面提示说明

#### 3.1.6 `@modifier`

- 类型：`string`
- 当前支持：`self_user=<Windows用户名>`
- 作用：当脚本包含高权限 API 时，用于限制只有特定本机用户才能挂载和执行

## 4. 语法规则

### 4.1 基础规则

- 普通语句通常以 `;` 结尾。
- 控制块结尾的 `;` 可以省略。
- 函数定义结尾的 `;` 可以省略。
- 支持 `//` 行注释。
- 支持 `/* ... */` 块注释。

### 4.2 当前支持的变量类型

- `int`
- `float`
- `double`
- `string`
- `bool`
- `auto`
- `vector<T>`
- `array<T>`

### 4.3 `auto` 规则

- `auto` 会尽量保留原始返回类型。
- 如果函数返回 `list`，`auto` 会继续是 `list`。
- 如果函数返回 `object`，`auto` 会继续是 `object`。
- 如果函数返回 `string / number / bool`，`auto` 也不会额外强制转换。

### 4.4 表达式与运算

支持以下常见运算：

- 赋值：`=`
- 算术：`+` `-` `*` `/`
- 比较：`==` `!=` `>` `<` `>=` `<=`
- 逻辑：`&&` `||` `!`
- 复合赋值：`+=` `-=` `*=` `/=`
- 自增自减：`i++` `++i` `i--` `--i`

### 4.5 容器语法

```cpp
vector<int> nums = { 1, 2, 3 };
array<string> states = { "idle", "live", "over" };

int a = nums[0];
string s = states[1];
```

### 4.6 对象访问语法

当函数返回 `object` 或 `list<object>` 时，可以直接按接近 C++/脚本语言的方式访问：

```cpp
auto accounts = GetSteamAccounts();
auto first = accounts[0];

Log(first.persona_name);
Log(first["account_name"]);
Log(accounts[0].steam_id64);
Log(first.localconfig_path);
Log(accounts.size);
```

### 4.7 控制流

支持：

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

#### 4.7.1 `goto`

- `goto label;` 会先在当前 block 内查找目标标签。
- 如果当前 block 没找到，会向外层 block 继续传播。
- 这次修复后，它的体感更接近 C/C++ 的局部跳转，而不是整个脚本乱跳。

#### 4.7.2 `return`

- 顶层脚本中的 `return;`
  - 类型：`void`
  - 作用：只结束当前脚本上下文，不会切断其他脚本执行。
- 函数中的 `return expr;`
  - 作用：把值返回给函数调用方。
- 这次修复后，返回状态已经放进脚本执行上下文，不再使用全局静态返回标志。

### 4.8 `on:` 边沿条件

`on:` 表示条件从假变真时只触发一次：

```cpp
if(on:round_phase=="live"){
    Log("刚进入 live");
}
```

## 5. 函数

### 5.1 函数定义

```cpp
int Add(int a, int b){
    return a + b;
}

bool IsAlive(){
    return health > 0;
}

void Ping(){
    Log("ping");
    return;
}
```

### 5.2 函数返回类型

当前建议使用：

- `int`
- `float`
- `double`
- `string`
- `bool`
- `void`
- `auto`
- `vector<T>`
- `array<T>`

### 5.3 函数返回值示例

```cpp
int Sum3(int a, int b, int c){
    return a + b + c;
}

string BuildTag(string name){
    return name + "_ok";
}
```

## 6. 综合示例脚本

下面这个脚本覆盖了当前最常用的语法、函数、容器、对象访问、Steam 本地账号对象提取、启动项读取、`if / else if / elseif / else / goto / continue / return` 等写法。

```cpp
// @name: VScript 综合演示
// @author: jingy
// @provider: StrikeSense
// @version: 2026.07.04
// @notice: 演示函数、返回值、容器、对象字段访问、Steam 账号 API 与条件分支
// @modifier: self_user=jingy

vector<int> marks = { 1, 2, 3 };
array<string> states = { "idle", "live", "over" };

double weight = 1.5;
auto retry = 0;

int Sum3(int a, int b, int c){
    return a + b + c;
}

bool ShouldPopup(){
    return on:round_phase=="live" && health > 0;
}

if(ShouldPopup()){
    Log("回合刚进入 live，开始执行综合示范脚本");
}

for(int i=0; i<6; i++){
    retry += 1;
    if(i == 1){
        continue;
    }else if(i == 4){
        goto steam_demo;
    }
}

steam_demo:
int total = Sum3(marks[0], marks[1], marks[2]);
string tag = states[1];
tag += "_ok";
weight *= 2;
Log("total=" + total);
Log("tag=" + tag);
Log("weight=" + weight);

string steamPath = GetSteamPath();
string cs2Path = GetCS2InstallPath();
string cfgPath = GetCS2CfgPath();
Log("SteamPath=" + steamPath);
Log("CS2Path=" + cs2Path);
Log("CfgPath=" + cfgPath);

auto ids32 = GetSteamUserIDs32();
auto ids64 = GetSteamUserIDs64();
Log("本地 Steam32 数量=" + Size(ids32));
Log("本地 Steam64 数量=" + Size(ids64));

auto accounts = GetSteamAccounts();
int accountCount = Size(accounts);
Log("本地 Steam 账号数量=" + accountCount);
Log("accounts 类型=" + TypeOf(accounts));

for(int i=0; i<accountCount; i++){
    auto account = accounts[i];
    Log("------");
    Log("索引=" + i);
    Log("account_name=" + account.account_name);
    Log("persona_name=" + account.persona_name);
    Log("steam_id64=" + account.steam_id64);
    Log("userdata_path=" + account.userdata_path);
    Log("localconfig_path=" + account.localconfig_path);

    if(account.most_recent){
        Log("判断：这是最近登录账号");
    }else if(account.allow_auto_login){
        Log("判断：这个账号允许自动登录");
    }elseif(Contains(account.account_name, "alt")){
        Log("判断：这看起来像备用账号");
    }else{
        Log("判断：普通账号");
    }

    if(HasField(account, "remember_password") && account.remember_password){
        Log("这个账号记住了密码");
    }

    if(account.has_localconfig){
        string launchOptions = GetSteamLaunchOptions64(account.steam_id64);
        if(Contains(launchOptions, "-vulkan")){
            Log("这个账号的 CS2 启动项里包含 -vulkan");
        }else{
            Log("这个账号当前没有配置 -vulkan");
        }
    }
}

if(accountCount > 0){
    auto firstAccount = accounts[0];
    if(HasSteamUser64(firstAccount.steam_id64)){
        Log("校验成功：第一个账号的 Steam64 的确存在于本机 userdata");
    }
    Log("Steam64To32=" + Steam64To32(firstAccount.steam_id64));
    Log("GetSteamLocalConfigPath64=" + GetSteamLocalConfigPath64(firstAccount.steam_id64));
}

return;
```

## 7. API 总表

### 7.1 基础工具 API

#### 7.1.1 `Size(value)`

- 参数：
  - `value`
    - 类型：`list | object | string | number | bool`
- 返回：
  - 类型：`number`
- 行为：
  - `list` 返回元素数量
  - `object` 返回字段数量
  - `string` 返回字符长度
  - 其他类型按数值处理

#### 7.1.2 `TypeOf(value)`

- 参数：
  - `value`
    - 类型：任意
- 返回：
  - 类型：`string`
- 可能返回：
  - `number`
  - `string`
  - `bool`
  - `list`
  - `object`
  - `void`

#### 7.1.3 `IsVoid(value)`

- 参数：
  - `value`
    - 类型：任意
- 返回：
  - 类型：`bool`

#### 7.1.4 `HasField(object, field_name)`

- 参数：
  - `object`
    - 类型：`object`
  - `field_name`
    - 类型：`string`
- 返回：
  - 类型：`bool`

#### 7.1.5 `Contains(container, target)`

- 参数：
  - `container`
    - 类型：`string | list | object`
  - `target`
    - 类型：任意
- 返回：
  - 类型：`bool`
- 行为：
  - `string`：检查是否包含子串
  - `list`：按文本值逐个比较
  - `object`：检查字段名是否存在

#### 7.1.6 `StartsWith(text, prefix)`

- 参数：
  - `text`
    - 类型：`string`
  - `prefix`
    - 类型：`string`
- 返回：
  - 类型：`bool`

#### 7.1.7 `EndsWith(text, suffix)`

- 参数：
  - `text`
    - 类型：`string`
  - `suffix`
    - 类型：`string`
- 返回：
  - 类型：`bool`

### 7.2 Steam 环境 API

#### 7.2.1 `GetSteamPath()`

- 参数：无
- 返回：
  - 类型：`string`
- 含义：本机 Steam 安装路径

#### 7.2.2 `GetCS2InstallPath()`

- 参数：无
- 返回：
  - 类型：`string`
- 含义：本机 CS2 安装路径

#### 7.2.3 `GetCS2CfgPath()`

- 参数：无
- 返回：
  - 类型：`string`
- 含义：本机 CS2 `game/csgo/cfg` 路径

#### 7.2.4 `WriteSteamGSIConfig()`

- 参数：无
- 返回：
  - 类型：`bool`
- 含义：写入 GSI 配置文件到 CS2 配置目录

#### 7.2.5 `GetSteamUserIDs32()`

- 参数：无
- 返回：
  - 类型：`list`
  - 元素类型：`string`
- 含义：扫描本机 Steam `userdata`，返回所有本地账号的 Steam32 ID

#### 7.2.6 `GetSteamUserIDs64()`

- 参数：无
- 返回：
  - 类型：`list`
  - 元素类型：`string`
- 含义：扫描本机 Steam `userdata`，返回所有本地账号的 Steam64 ID

#### 7.2.7 `HasSteamUser32(id32)`

- 参数：
  - `id32`
    - 类型：`string`
- 返回：
  - 类型：`bool`

#### 7.2.8 `HasSteamUser64(id64)`

- 参数：
  - `id64`
    - 类型：`string`
- 返回：
  - 类型：`bool`

#### 7.2.9 `Steam32To64(id32)`

- 参数：
  - `id32`
    - 类型：`string`
- 返回：
  - 类型：`string`

#### 7.2.10 `Steam64To32(id64)`

- 参数：
  - `id64`
    - 类型：`string`
- 返回：
  - 类型：`string`

#### 7.2.11 `GetSteamLocalConfigPath32(id32)`

- 参数：
  - `id32`
    - 类型：`string`
- 返回：
  - 类型：`string`
- 含义：按 Steam32 ID 获取该账号的 `localconfig.vdf` 路径

#### 7.2.12 `GetSteamLocalConfigPath64(id64)`

- 参数：
  - `id64`
    - 类型：`string`
- 返回：
  - 类型：`string`
- 含义：按 Steam64 ID 获取该账号的 `localconfig.vdf` 路径

#### 7.2.13 `GetSteamLaunchOptions32(id32)`

- 参数：
  - `id32`
    - 类型：`string`
- 返回：
  - 类型：`string`
- 含义：读取该账号 `localconfig.vdf` 中 AppID `730` 的 `LaunchOptions`

#### 7.2.14 `GetSteamLaunchOptions64(id64)`

- 参数：
  - `id64`
    - 类型：`string`
- 返回：
  - 类型：`string`
- 含义：读取该账号 `localconfig.vdf` 中 AppID `730` 的 `LaunchOptions`

#### 7.2.15 `GetSteamAccounts()`

- 参数：无
- 返回：
  - 类型：`list`
  - 元素类型：`object`
- 含义：读取本机 `loginusers.vdf`，并结合本机 `userdata` 目录构造账号对象列表

### 7.3 `GetSteamAccounts()` 对象字段

#### 7.3.1 `account_id32`

- 类型：`string`
- 含义：Steam32 ID

#### 7.3.2 `steam_id64`

- 类型：`string`
- 含义：Steam64 ID

#### 7.3.3 `account_name`

- 类型：`string`
- 含义：登录账号名

#### 7.3.4 `persona_name`

- 类型：`string`
- 含义：Steam 显示昵称

#### 7.3.5 `timestamp`

- 类型：`string`
- 含义：`loginusers.vdf` 原始时间戳文本

#### 7.3.6 `userdata_path`

- 类型：`string`
- 含义：本机该账号对应的 `userdata` 路径

#### 7.3.7 `localconfig_path`

- 类型：`string`
- 含义：本机该账号对应的 `localconfig.vdf` 路径

#### 7.3.8 `most_recent`

- 类型：`bool`
- 含义：是否最近登录

#### 7.3.9 `allow_auto_login`

- 类型：`bool`
- 含义：是否允许自动登录

#### 7.3.10 `remember_password`

- 类型：`bool`
- 含义：是否勾选记住密码

#### 7.3.11 `has_userdata`

- 类型：`bool`
- 含义：本机是否真的存在该账号的 `userdata` 目录

#### 7.3.12 `has_localconfig`

- 类型：`bool`
- 含义：本机是否真的存在该账号的 `localconfig.vdf`

### 7.4 系统与窗口控制 API

#### 7.4.1 `CloseGameWindow()`

- 参数：无
- 返回：
  - 类型：`bool`
- 含义：安全地把 CS2 切出前台

#### 7.4.2 `KillGameProcess()`

- 参数：无
- 返回：
  - 类型：`bool`
- 含义：结束 `cs2.exe`

#### 7.4.3 `RunGameProcess()`

- 参数：无
- 返回：
  - 类型：`bool`
- 含义：通过 `steam://run/730` 启动游戏

#### 7.4.4 `ShowGameProcess()`

- 参数：无
- 返回：
  - 类型：`bool`
- 含义：切回并激活 CS2 主窗口

#### 7.4.5 `Browser(url, top_after_open, prefer_existing_browser)`

- 参数：
  - `url`
    - 类型：`string`
  - `top_after_open`
    - 类型：`bool`
  - `prefer_existing_browser`
    - 类型：`bool`
- 返回：
  - 类型：`bool`

#### 7.4.6 `Open(target)`

- 参数：
  - `target`
    - 类型：`string`
- 返回：
  - 类型：`bool`

#### 7.4.7 `EnsureProcessWindow(process_name, launch_target, activate)`

- 参数：
  - `process_name`
    - 类型：`string`
  - `launch_target`
    - 类型：`string`
  - `activate`
    - 类型：`bool`
- 返回：
  - 类型：`bool`

#### 7.4.8 `Top(process_name, activate)`

- 参数：
  - `process_name`
    - 类型：`string`
  - `activate`
    - 类型：`bool`
- 返回：
  - 类型：`bool`

### 7.5 图像与音频 API

#### 7.5.1 `Drawimg(path, x, y, alpha_channel, opacity, ttl_ms, id)`

- 参数：
  - `path`
    - 类型：`string`
  - `x`
    - 类型：`int`
  - `y`
    - 类型：`int`
  - `alpha_channel`
    - 类型：`bool`
  - `opacity`
    - 类型：`float`
  - `ttl_ms`
    - 类型：`int`
  - `id`
    - 类型：`int`
- 返回：
  - 类型：`bool`

#### 7.5.2 `Closeimg(id)`

- 参数：
  - `id`
    - 类型：`int`
- 返回：
  - 类型：`bool`

#### 7.5.3 `Playsnd(path, volume, id)`

- 参数：
  - `path`
    - 类型：`string`
  - `volume`
    - 类型：`float`
  - `id`
    - 类型：`int`
- 返回：
  - 类型：`bool`

#### 7.5.4 `Stopsnd(id)`

- 参数：
  - `id`
    - 类型：`int`
- 返回：
  - 类型：`bool`

### 7.6 文件与执行 API

#### 7.6.1 `ShellExecute(command)`

- 权限：高权限
- 参数：
  - `command`
    - 类型：`string`
- 返回：
  - 类型：`bool`

#### 7.6.2 `CFile(path, content)`

- 权限：高权限
- 参数：
  - `path`
    - 类型：`string`
  - `content`
    - 类型：`string`
- 返回：
  - 类型：`bool`
- 含义：创建或覆盖文件

#### 7.6.3 `OwriteFile(path, content)`

- 权限：高权限
- 参数：
  - `path`
    - 类型：`string`
  - `content`
    - 类型：`string`
- 返回：
  - 类型：`bool`
- 含义：覆盖写入文件

#### 7.6.4 `AwriteFile(path, content)`

- 权限：高权限
- 参数：
  - `path`
    - 类型：`string`
  - `content`
    - 类型：`string`
- 返回：
  - 类型：`bool`
- 含义：追加写入文件

#### 7.6.5 `DFile(path)`

- 权限：高权限
- 参数：
  - `path`
    - 类型：`string`
- 返回：
  - 类型：`bool`

### 7.7 调试与等待 API

#### 7.7.1 `Sleep(ms)`

- 参数：
  - `ms`
    - 类型：`int`
- 返回：
  - 类型：`bool`

#### 7.7.2 `Log(text)`

- 参数：
  - `text`
    - 类型：任意，最终会转成 `string`
- 返回：
  - 类型：`bool`

### 7.8 音量与十字准星 API

#### 7.8.1 `SetProcessVolume(process_name, volume_percent)`

- 参数：
  - `process_name`
    - 类型：`string`
  - `volume_percent`
    - 类型：`float`
- 返回：
  - 类型：`bool`

#### 7.8.2 `SetProcessMute(process_name, muted)`

- 参数：
  - `process_name`
    - 类型：`string`
  - `muted`
    - 类型：`bool`
- 返回：
  - 类型：`bool`

#### 7.8.3 `SetDeathVolume(value)`

- 参数：
  - `value`
    - 类型：`float`
- 返回：
  - 类型：`bool`

#### 7.8.4 `SetDeathMute(enabled)`

- 参数：
  - `enabled`
    - 类型：`bool`
- 返回：
  - 类型：`bool`

#### 7.8.5 `SetCrosshairEnabled(enabled)`

- 参数：
  - `enabled`
    - 类型：`bool`
- 返回：
  - 类型：`bool`

#### 7.8.6 `SetCrosshairVisual(r, g, b, style, thickness, scale)`

- 参数：
  - `r`
    - 类型：`int`
  - `g`
    - 类型：`int`
  - `b`
    - 类型：`int`
  - `style`
    - 类型：`int`
  - `thickness`
    - 类型：`int`
  - `scale`
    - 类型：`float`
- 返回：
  - 类型：`bool`

#### 7.8.7 `SetCrosshairConfig(r, g, b, style, thickness, scale)`

- 参数：与 `SetCrosshairVisual(...)` 相同
- 返回：
  - 类型：`bool`
- 说明：当前作为同义入口存在

#### 7.8.8 `SetCrosshair(enabled, r, g, b, style, thickness, scale)`

- 参数：
  - `enabled`
    - 类型：`bool`
  - `r`
    - 类型：`int`
  - `g`
    - 类型：`int`
  - `b`
    - 类型：`int`
  - `style`
    - 类型：`int`
  - `thickness`
    - 类型：`int`
  - `scale`
    - 类型：`float`
- 返回：
  - 类型：`bool`

## 8. 变量来源分类

### 8.1 GSI 原始展开变量

任何收到的 GSI 字段都会尽量同步成 `gsi_...` 变量。

示例：

- `gsi_provider_name`
  - 类型：`string`
- `gsi_map_team_ct_score`
  - 类型：`number`
- `gsi_player_state_health`
  - 类型：`number`
- `gsi_player_weapons_weapon_1_ammo_clip`
  - 类型：`number`

补充：

- `gsi_field_count`
  - 类型：`number`
  - 含义：当前已同步的原始叶子字段总数

### 8.2 常用便捷别名变量

#### 8.2.1 Provider 类

- `provider_name`
  - 类型：`string`
- `provider_appid`
  - 类型：`number`
- `provider_version`
  - 类型：`number | string`
- `provider_steamid`
  - 类型：`string`
- `provider_timestamp`
  - 类型：`number | string`

#### 8.2.2 地图与队伍类

- `map`
  - 类型：`string`
- `map_mode`
  - 类型：`string`
- `map_phase`
  - 类型：`string`
- `map_round`
  - 类型：`number`
- `team_ct_score`
  - 类型：`number`
- `team_ct_consecutive_round_losses`
  - 类型：`number`
- `team_ct_timeouts_remaining`
  - 类型：`number`
- `team_ct_matches_won_this_series`
  - 类型：`number`
- `team_t_score`
  - 类型：`number`
- `team_t_consecutive_round_losses`
  - 类型：`number`
- `team_t_timeouts_remaining`
  - 类型：`number`
- `team_t_matches_won_this_series`
  - 类型：`number`
- `map_num_matches_to_win_series`
  - 类型：`number`

#### 8.2.3 回合类

- `round_phase`
  - 类型：`string`
- `round_win_team`
  - 类型：`string`
- `bomb`
  - 类型：`string`

#### 8.2.4 玩家身份类

- `player_name`
  - 类型：`string`
- `steamid`
  - 类型：`string`
- `team`
  - 类型：`string`
- `activity`
  - 类型：`string`
- `observer_slot`
  - 类型：`number`

#### 8.2.5 玩家状态类

- `health`
  - 类型：`number`
- `armor`
  - 类型：`number`
- `helmet`
  - 类型：`bool | number`
- `flashed`
  - 类型：`number`
- `smoked`
  - 类型：`number`
- `burning`
  - 类型：`number`
- `money`
  - 类型：`number`
- `kills`
  - 类型：`number`
- `round_killhs`
  - 类型：`number`
- `equip_value`
  - 类型：`number`

#### 8.2.6 比赛统计类

- `mvps`
  - 类型：`number`
- `match_kills`
  - 类型：`number`
- `match_assists`
  - 类型：`number`
- `match_deaths`
  - 类型：`number`
- `match_score`
  - 类型：`number`

#### 8.2.7 武器类

- `weapon_name`
  - 类型：`string`
- `weapon_type`
  - 类型：`string`
- `weapon_state`
  - 类型：`string`
- `weapon_ammo_clip`
  - 类型：`number`
- `weapon_ammo_clip_max`
  - 类型：`number`
- `weapon_ammo_reserve`
  - 类型：`number`

### 8.3 上一帧快照变量

- `prev_round_phase`
  - 类型：`string`
- `prev_kills`
  - 类型：`number`
- `prev_health`
  - 类型：`number`
- `prev_weapon_name`
  - 类型：`string`
- `prev_weapon_type`
  - 类型：`string`
- `prev_weapon_state`
  - 类型：`string`
- `prev_weapon_ammo_clip`
  - 类型：`number`
- `prev_weapon_ammo_reserve`
  - 类型：`number`

## 9. 内部变量

### 9.1 武器事件内部变量

- `weapon_fired`
  - 类型：`bool`
- `weapon_reloading`
  - 类型：`bool`
- `weapon_switched`
  - 类型：`bool`
- `weapon_clip_delta`
  - 类型：`number`
- `weapon_reserve_delta`
  - 类型：`number`
- `weapon_fire_count`
  - 类型：`number`
- `weapon_reload_count`
  - 类型：`number`
- `weapon_reserve_drop_count`
  - 类型：`number`

### 9.2 程序状态内部变量

- `death_mute`
  - 类型：`bool`
- `self_alive`
  - 类型：`bool`
- `self_dead_this_round`
  - 类型：`bool`
- `internal_last_phase`
  - 类型：`string`
- `internal_last_kills`
  - 类型：`number`
- `internal_last_mvps`
  - 类型：`number`
- `internal_dead_muted`
  - 类型：`bool`
- `internal_waiting_for_live`
  - 类型：`bool`
- `internal_round_started`
  - 类型：`bool`
- `internal_mvp_candidate_kills`
  - 类型：`number`
- `internal_mvp_pushed_this_round`
  - 类型：`bool`
- `internal_mvps_at_round_start`
  - 类型：`number`
- `internal_gameover_pushed`
  - 类型：`bool`
- `internal_bomb_planted_this_round`
  - 类型：`bool`
- `internal_player_team`
  - 类型：`string`
- `internal_map_mode`
  - 类型：`string`
- `internal_activity`
  - 类型：`string`
- `internal_round_kills`
  - 类型：`number`
- `internal_health`
  - 类型：`number`
- `internal_in_lobby`
  - 类型：`bool`

## 10. 缺失值规则

如果字段当前不存在，脚本层会得到：

`void`

所以写条件时建议显式判断：

```cpp
if(!IsVoid(player_name)){
    Log(player_name);
}
```

## 11. 示例文件

仓库中的示例脚本：

- `vscript_examples/syntax_showcase.vscript`
- `vscript_examples/steam_accounts_showcase.vscript`

如果你要测试 Steam 账号对象、`Steam64To32`、`GetSteamLaunchOptions64`、`GetSteamLocalConfigPath64`，建议直接从这两个示例开始改。
