# StrikeSense CScript 说明

CScript 用外部程序把一个按键事件拆成多个单动作 CFG 执行片段，用于替代已经失效的多动作绑定方式。

## 工作方式

启用超频配置中的“为多绑定的脚本提供支持”后，StrikeSense 会：

1. 在 CS2 的 `cfg` 目录创建 `StrikeTicker.cfg`。
2. 向 `autoexec.cfg` 写入以下托管块：

```cfg
//--StrikeSense CScript Ticker--
bind <当前 Ticker 按键> "exec StrikeTicker.cfg"
//--StrikeSense CScript Ticker END--
```

3. CS2 位于前台时，以每秒 64 次的频率模拟一次当前 Ticker 按键的按下和松开；默认按键是 `kp_9`，可在展开区域重新绑定。
4. 捕获已绑定脚本的真实键盘按下、松开事件；`SendInput` 生成的事件不会再次进入脚本队列。
5. 每个 15.625ms 时间片最多向 `StrikeTicker.cfg` 写入一个动作，然后模拟当前 Ticker 按键。
6. 一个事件的最后动作执行后，下一个时间片会清空 `StrikeTicker.cfg`，防止最后动作被重复执行。
7. 队列为空且不需要清空文件时，仍维持 64Hz ticker，但不进行无意义文件 I/O。

如果游戏已在运行，需要在控制台执行一次 `exec autoexec`，或重启游戏，使 Ticker 绑定生效。

当前 Ticker 按键是保留按键，不能同时绑定给 CScript。修改 Ticker 按键后，原按键会写入 `unbind`，新按键会写入 `bind`。

## 脚本目录

默认目录：

```text
%UserProfile%\StrikeSense\sourcecfg
```

首次运行会生成 `testscript.cscript` 和 `jumpthrow.cscript`。仓库中的示例位于：

```text
sourcecfg/testscript.cscript
sourcecfg/jumpthrow.cscript
```

## 语法

脚本扩展名必须是 `.cscript`。脚本必须同时包含一次 `@OnPressed` 和一次 `@OnReleased`。

```cscript
// 双斜杠可以写注释；字符串中的 // 不会被当成注释。
@OnPressed {
    "+forward":0;
    "alias a b;+jump":1;
}

@OnReleased {
    "-forward":0;
    "-jump":1;
    "+lookatweapon":2;
    "-lookatweapon":3;
}
```

每条动作的格式为：

```text
"起源引擎命令":动作序号;
```

规则：

- 动作序号必须从 `0` 开始并连续。
- 文件中的动作可以不按序号排列，加载时会按序号排序。
- 命令必须使用双引号包裹。
- 最后一条动作允许省略结尾分号，但建议始终保留。
- `@OnPressed` 和 `@OnReleased` 都不能为空。
- 字符串支持 `\"`、`\\`、`\n`、`\r`、`\t` 转义。

## 单动作校验

一个动作字符串中最多只能出现一个普通指令。以下指令被视为可并行辅助指令，不计入普通指令数量：

- `alias`
- `bind`
- `say`
- `say_team`
- `echoln`

合法示例：

```cscript
"alias a b;alias c d;bind u test;+jump":0;
```

其中 `+jump` 是唯一的普通指令。

非法示例：

```cscript
"+forward;+jump":0;
```

这里包含两个普通动作指令，因此加载失败。

被引号包裹的分号不会被拆分。例如 alias 的命令体可以使用转义引号：

```cscript
"alias test \"+forward;-forward\";+jump":0;
```

## 执行标记

StrikeSense 会在每次写入的动作后追加 `echoln` 标记。按下 `w` 的第一个动作示例：

```cfg
+forward;echoln "[cscript]pressed w has been executed 0"
```

最后一个动作的序号后会追加 `!`：

```cfg
-jump;echoln "[cscript]released w has been executed 1!"
```

控制台日志读取器会识别这些标记，并在 CScript 面板中显示最近一次来自 `console.log` 的执行确认。

## 多按键竞争处理

每个已挂载脚本都有独立事件通道：

- 同一脚本内，按下序列一定先于随后到达的松开序列，不会出现松开动作插到按下动作中间。
- 不同脚本之间使用轮转调度，每个活动脚本每轮执行一个动作。
- 新按键事件不会覆盖已有事件，也不会丢失动作。

例如 `w` 和 `a` 同时触发两个多动作脚本时，可能按以下顺序执行：

```text
w pressed 0
a pressed 0
w pressed 1
a pressed 1
清空
...
```

实际顺序取决于两个物理事件到达钩子的先后，但每个脚本自身的动作顺序始终稳定。

## 支持的单按键

界面只录入单按键，不录入组合键。当前支持：

- `a` 到 `z`
- 主键盘 `0` 到 `9`
- `kp_0` 到 `kp_9`；其中当前 Ticker 按键不可同时绑定给脚本
- `ctrl`、`rctrl`
- `shift`、`rshift`
- `alt`、`ralt`
- `capslock`、`tab`、`space`、`enter`、`backspace`、`escape`
- `-`、`=`
- `f1` 到 `f12`
- 方向键、`home`、`end`、`pgup`、`pgdn`、`ins`、`del`
- `kp_plus`、`kp_minus`、`kp_multiply`、`kp_slash`、`kp_del`

## 配置文件

挂载脚本、单键绑定和开关偏好保存在：

```text
%UserProfile%\StrikeSense\setting\cscript_config.json
```

CScript 面板每页固定显示三个脚本，并提供 Ticker 按键绑定、脚本挂载、脚本按键绑定、重载、卸载、上一页和下一页操作。急停参数与 CScript 子控件使用进化页相同的 `>` / `v` 折叠样式，展开后会直接撑开下方模块的位置。
