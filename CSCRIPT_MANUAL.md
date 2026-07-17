# StrikeSense CScript 手册

## 文档版本

- 手册版本：`2026.07.16`
- 适用对象：想在 StrikeSense 里编写按键脚本的普通用户
- 脚本扩展名：`.cscript`

## 这是什么

CScript 用来把一个按键拆成多步 Source 命令，适合做：

- 跳投
- 一键执行多条连续命令
- 按下和松开执行不同动作
- 给常用动作补上通知提示

如果你只想“绑定一个键，按下做什么，松开做什么”，那 CScript 就够用了。

## 开始前要做什么

1. 在 StrikeSense 里开启 `为多绑定的脚本提供支持`
2. 打开 CScript 面板
3. 挂载一个 `.cscript` 文件
4. 给这个脚本绑定一个按键

脚本默认目录：

```text
%UserProfile%\StrikeSense\sourcecfg
```

首次使用时，程序通常会生成示例脚本，方便你直接改。

## 最基本的写法

一个合法的 CScript 必须同时包含：

- `@OnPressed`
- `@OnReleased`

示例：

```cscript
// 跳投：请在界面中绑定跳投键。
@OnPressed {
    "+jump":0;
    "-attack":1;
    "-attack2":2;
}

@OnReleased {
    "-jump":0;
}
```

含义：

- 按下绑定键时，按顺序执行 `+jump`、`-attack`、`-attack2`
- 松开绑定键时，执行 `-jump`

## 动作格式

每一条动作都必须写成：

```text
"命令":序号;
```

例如：

```cscript
"+jump":0;
"-attack":1;
```

规则如下：

- 命令必须放在双引号里
- 序号必须是数字
- 序号从 `0` 开始
- 同一个代码块里的序号必须连续
- 建议每一行都写分号 `;`

## 两个代码块分别做什么

### `@OnPressed`

按下你绑定的那个键时执行。

示例：

```cscript
@OnPressed {
    "slot3":0;
    "+lookatweapon":1;
}
```

### `@OnReleased`

松开你绑定的那个键时执行。

示例：

```cscript
@OnReleased {
    "-lookatweapon":0;
}
```

## 常见示例

### 跳投

```cscript
@OnPressed {
    "+jump":0;
    "-attack":1;
    "-attack2":2;
}

@OnReleased {
    "-jump":0;
}
```

### 按下显示开启通知，松开显示关闭通知

```cscript
@OnPressed {
    "echoln /notificationE 跳投;+jump":0;
}

@OnReleased {
    "echoln /notificationD 跳投;-jump":0;
}
```

### 先执行辅助命令，再执行一次主要动作

```cscript
@OnPressed {
    "alias test say hello;+jump":0;
}

@OnReleased {
    "-jump":0;
}
```

## 字符串怎么写

外层双引号是脚本语法的一部分：

```cscript
"命令内容":0;
```

如果命令内容里面还要再出现双引号，必须转义成 `\"`。

例如：

```cscript
"echoln \"/notificationE 跳投\";+jump":0;
```

如果你不转义，解析器会以为字符串提前结束，常见报错就是：

```text
命令后缺少序号分隔符 ':'
```

## 单条动作的限制

一条动作里，允许有一些辅助命令，但只应该有一个主要动作命令。

推荐这样写：

```cscript
"alias a b;+jump":0;
```

不推荐这样写：

```cscript
"+forward;+jump":0;
```

如果你不确定，最稳妥的做法就是：

- 一条动作只放一个主要动作
- 多步逻辑拆成多行
- 用序号控制顺序

## 注释

支持单行注释：

```cscript
// 这是注释
```

示例：

```cscript
// 这是一个简单脚本
@OnPressed {
    "+jump":0;
}

@OnReleased {
    "-jump":0;
}
```

## 支持绑定的按键

界面目前录入的是“单按键”，不是组合键。

常见支持范围：

- `a` 到 `z`
- 主键盘 `0` 到 `9`
- 小键盘 `kp_0` 到 `kp_9`
- `ctrl`、`rctrl`
- `shift`、`rshift`
- `alt`、`ralt`
- `capslock`
- `tab`
- `space`
- `enter`
- `backspace`
- `escape`
- `-`
- `=`
- `]`
- `f1` 到 `f12`
- 方向键
- `home`、`end`、`pgup`、`pgdn`
- `ins`、`del`
- `kp_plus`、`kp_minus`、`kp_multiply`、`kp_slash`、`kp_del`

说明：

- 用作系统保留用途的按键，不能再同时绑定给普通脚本
- 如果你在界面里录不进去，通常说明当前版本不支持那个键

## 配置文件位置

CScript 的挂载、绑定和开关信息会保存在：

```text
%UserProfile%\StrikeSense\setting\cscript_config.json
```

一般不需要手动改，优先在界面里操作。

## 常见报错与排查

### `命令后缺少序号分隔符 ':'`

常见原因：

- 字符串里的双引号没转义
- 写成了 `"命令"0;`
- 前面的引号没有闭合

错误示例：

```cscript
"echoln "/notificationE 跳投;+jump":0;
```

正确示例：

```cscript
"echoln /notificationE 跳投;+jump":0;
```

或者：

```cscript
"echoln \"/notificationE 跳投\";+jump":0;
```

### `动作序号必须从 0 开始`

错误示例：

```cscript
@OnPressed {
    "+jump":1;
}
```

正确示例：

```cscript
@OnPressed {
    "+jump":0;
}
```

### `序号不连续`

错误示例：

```cscript
@OnPressed {
    "+jump":0;
    "-attack":2;
}
```

正确示例：

```cscript
@OnPressed {
    "+jump":0;
    "-attack":1;
}
```

### 脚本能挂载，但按键没反应

请按顺序检查：

1. 是否开启了 `为多绑定的脚本提供支持`
2. 是否已经给脚本绑定按键
3. 脚本是否同时有 `@OnPressed` 和 `@OnReleased`
4. CS2 是否已经重新读取配置

如果你刚开启功能，重进游戏通常最省事。

## 推荐写法

- 一个脚本只做一件事
- 动作尽量短，不要一行塞太多命令
- 复杂动作拆成多条并编号
- 先写最小可用版本，再逐步加通知或附加命令
- 改完就重新加载脚本测试

## 一个稳妥的模板

```cscript
// 在界面中给这个脚本绑定一个键
@OnPressed {
    "echoln /notificationE 我的脚本":0;
    "+jump":1;
}

@OnReleased {
    "echoln /notificationD 我的脚本":0;
    "-jump":1;
}
```
