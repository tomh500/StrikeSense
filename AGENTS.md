# 编码风格规范
- 架构要求：严禁过度使用类和对象（OOP）。优先使用 `namespace` 将逻辑分组，使用纯函数 (Free Functions) 进行状态变换。
- 只有在真正需要维护对象生命周期（如音频缓冲区管理）时才允许使用简单的 `struct`，禁止使用复杂的继承体系。
- 命名空间命名：使用全小写，如 `namespace gsi { ... }`。


# 代码基本要求
- 严格使用C++17，及其最新特性，用新不用旧，有新特性写法就用新特性
- filesystem库优先使用，而不要用wchar去存储
- 所有操作尽可能地加cout调试信息输出，输出语言为中文
- 不要把函数一股脑写到main，而是懂得适当地新建文件，把不同种类的函数放到不同的cpp文件，而且活用命名空间，除非特别泛用的函数可以写成全局函数

# 项目要求
- 这是个软件，软件会存储信息，以Json的形式存储，存储目录为%UserProfile%/StrikeSense
- 请每次编译在rc文件修改版本号为时间戳年月日小时分钟


# Git 工作流规范
- 每次完成一个功能单元（如“配置 HTTP 服务器”或“解析击杀字段”）后，必须立即执行 `git add .`。
- 执行 `git commit -m "..."`。提交信息必须简明扼要，说明做了什么变动。
- 严禁在修改多个不相关功能后才提交，保持提交粒度足够小，以便快速回档。


首选 apply_patch 改文件。
如果必须脚本写文件，显式用 UTF8Encoding($false)。
保持原文件换行，不顺手重写整文件。
不用 PowerShell 默认 Set-Content / WriteAllText 裸写源码

# 主题和公共绘图要求
- 新建或修改 UI 控件时，必须优先使用 `pages.h` 中 `ui::` 公共绘图接口和 `src/app/ui_common.cpp` 的实现，不要在页面里重新写一套按钮、开关、滑块、导航项动画。
- 控件颜色必须来自 `uitheme::get_palette()` / `include/core/ui_theme.h` 的全局 `palette`，禁止在控件内部写死传统科技蓝或一次性主题色。
- 常用控件映射：按钮使用 `ui::DrawRoundedButton`，折叠按钮使用 `ui::DrawFoldButton`，开关使用 `ui::DrawToggle`，滑块使用 `ui::DrawSlider` 或 `ui::DrawSliderWithKnob`，侧边栏导航使用 `ui::DrawNavigationButton`，页头使用 `ui::DrawHeader`。
- 如果页面确实需要自定义圆角表格、输入框、下拉框或提示块，也必须从 `uitheme::palette` 取背景、文字、描边、提示、警告和成功颜色，并保持文字颜色一起跟随主题。
- 基础视觉组顺序为：`Defult`、`水色系`、`暗黑红系`、`金秋`、`春意盎然`、`紫颂果`。这些主题都走公共动画和渐变控件体系。
- 高级视觉组顺序为：`Vape`、`LiquidBounce`、`Gemini`、`GPT`、`DeepSeek`。当前全部处于开发中，按钮置灰且不可点击，点击只提示“正在开发中”。
- 动画刷新由公共 UI 时钟管理：交互时约 90fps，空闲降频到低刷新。不要在单个控件里私自开独立定时器或直接瞬间切换颜色。
- 程序启动默认主题是 `Defult`，并且窗口创建前要先读取保存的主题配置，避免启动后仍显示旧蓝色。

- 一般来讲 %UserProfile%/StrikeSense是程序的数据目录，如果我说了相对路径 一般是说从%UserProfile%/StrikeSense开始 例如 img/1.jpg 指的是%UserProfile%/StrikeSense/img/1.jpg