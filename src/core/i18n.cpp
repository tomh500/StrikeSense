#include "i18n.h"
#include <unordered_map>

static std::unordered_map<std::string, std::wstring> s_cn, s_en;

void i18n::Init() {
    auto& cn = s_cn; auto& en = s_en;
    cn[Keys::SIDEBAR_FILE] = L"文件位置"; en[Keys::SIDEBAR_FILE] = L"File Location";
    cn[Keys::SIDEBAR_SETTINGS] = L"遗产核心"; en[Keys::SIDEBAR_SETTINGS] = L"Legacy Core";
    cn[Keys::SIDEBAR_EVOLUTION] = L"进化分支"; en[Keys::SIDEBAR_EVOLUTION] = L"Nextgen Mods";
    cn[Keys::SIDEBAR_LEGAL] = L"合法配置"; en[Keys::SIDEBAR_LEGAL] = L"Legit Config";
    cn[Keys::SIDEBAR_Rage] = L"超频配置"; en[Keys::SIDEBAR_Rage] = L"Semi Rage";
    cn[Keys::SIDEBAR_ITEMHELPER] = L"道具助手"; en[Keys::SIDEBAR_ITEMHELPER] = L"Items Helper";
    cn["SIDEBAR_VSCRIPT"] = L"自定脚本"; en["SIDEBAR_VSCRIPT"] = L"Scripts";
    
    cn[Keys::SOUNDS_TITLE] = L"文件位置"; en[Keys::SOUNDS_TITLE] = L"File Location";
    cn[Keys::SOUNDS_GSI_RUNNING] = L"运行中"; en[Keys::SOUNDS_GSI_RUNNING] = L"Running";
    cn[Keys::SOUNDS_GSI_STOPPED] = L"未启动"; en[Keys::SOUNDS_GSI_STOPPED] = L"Stopped";
    cn[Keys::SOUNDS_ENABLED] = L"已启用"; en[Keys::SOUNDS_ENABLED] = L"Enabled";
    cn[Keys::SOUNDS_DISABLED] = L"已禁用"; en[Keys::SOUNDS_DISABLED] = L"Disabled";
    cn[Keys::SOUNDS_SELECT] = L"选择..."; en[Keys::SOUNDS_SELECT] = L"Browse...";
    cn[Keys::SOUNDS_FOLDER_BTN] = L"打开默认音频文件夹"; en[Keys::SOUNDS_FOLDER_BTN] = L"Open Audio Folder";
    cn[Keys::SOUNDS_IMAGE_BTN] = L"打开默认图片文件夹"; en[Keys::SOUNDS_IMAGE_BTN] = L"Open Image Folder";

    cn[Keys::SETTINGS_TITLE] = L"遗产核心"; en[Keys::SETTINGS_TITLE] = L"Legacy Core";
    cn[Keys::SETTINGS_VOL] = L"音量"; en[Keys::SETTINGS_VOL] = L"Volume";
    cn[Keys::SETTINGS_CUSTOM_MUSIC] = L"自定义音乐包"; en[Keys::SETTINGS_CUSTOM_MUSIC] = L"Custom Music Kit";
    cn[Keys::SETTINGS_KILL_SOUND] = L"击杀音效替换"; en[Keys::SETTINGS_KILL_SOUND] = L"Kill Sound";
    cn[Keys::SETTINGS_FORCE_INTERRUPT] = L"强制打断"; en[Keys::SETTINGS_FORCE_INTERRUPT] = L"Force Interrupt";
    cn[Keys::SETTINGS_FLASH] = L"闪光叠加"; en[Keys::SETTINGS_FLASH] = L"Flash Overlay";
    cn[Keys::SETTINGS_LOWMEM] = L"低内存模式"; en[Keys::SETTINGS_LOWMEM] = L"Low Memory";
    cn[Keys::SETTINGS_MVP] = L"MVP信息板"; en[Keys::SETTINGS_MVP] = L"MVP Board";
    cn[Keys::SETTINGS_OGG] = L"OGG格式"; en[Keys::SETTINGS_OGG] = L"OGG Format";

    cn[Keys::EVO_TITLE] = L"进化分支"; en[Keys::EVO_TITLE] = L"Evolution";
    cn[Keys::EVO_VOL_ADJ] = L"即时音量调整器"; en[Keys::EVO_VOL_ADJ] = L"Instant Volume";
    cn[Keys::EVO_HOTKEY] = L"即时音量调整器快捷键"; en[Keys::EVO_HOTKEY] = L"Instant Volume Hotkey";
    cn[Keys::EVO_CROSSHAIR] = L"狙击准星设置"; en[Keys::EVO_CROSSHAIR] = L"Crosshair Settings";
    cn[Keys::EVO_THICKNESS] = L"粗细"; en[Keys::EVO_THICKNESS] = L"Thickness";
    cn[Keys::EVO_SCALE] = L"缩放"; en[Keys::EVO_SCALE] = L"Scale";
    cn[Keys::EVO_STYLE] = L"样式"; en[Keys::EVO_STYLE] = L"Style";
    cn[Keys::EVO_ENABLE] = L"启用"; en[Keys::EVO_ENABLE] = L"Enable";
    cn[Keys::EVO_STYLE_HOLLOW] = L"空心圆"; en[Keys::EVO_STYLE_HOLLOW] = L"Hollow";
    cn[Keys::EVO_STYLE_SOLID] = L"实心圆"; en[Keys::EVO_STYLE_SOLID] = L"Solid";
    cn[Keys::EVO_STYLE_CLASSIC] = L"经典"; en[Keys::EVO_STYLE_CLASSIC] = L"Classic";
    cn["EVO_STYLE_CROSS"] = L"分离十字"; en["EVO_STYLE_CROSS"] = L"Cross";
    cn["EVO_STYLE_CORNERS"] = L"四角标"; en["EVO_STYLE_CORNERS"] = L"Corners";
    cn["EVO_STYLE_T"] = L"T 型"; en["EVO_STYLE_T"] = L"T Shape";
    cn["EVO_CROSSHAIR_ON"] = L"已启用"; en["EVO_CROSSHAIR_ON"] = L"Enabled";
    cn["EVO_CROSSHAIR_OFF"] = L"关闭时隐藏参数"; en["EVO_CROSSHAIR_OFF"] = L"Options hidden";
    cn[Keys::EVO_WAITING_KEY] = L"按下任何字母键或数字键..."; en[Keys::EVO_WAITING_KEY] = L"Press any letter or number...";
    cn[Keys::EVO_CLICK_MODIFY] = L"点击修改快捷键"; en[Keys::EVO_CLICK_MODIFY] = L"Click to rebind";
    cn[Keys::EVO_STATUS_NORMAL] = L"正常"; en[Keys::EVO_STATUS_NORMAL] = L"Normal";
    cn[Keys::EVO_STATUS_MUTED] = L"启用调整"; en[Keys::EVO_STATUS_MUTED] = L"Enable";
    cn[Keys::EVO_HINT_MUTE] = L"此开关开启后，仅降低CS2进程的音量，不影响其他程序，本功能需要管理员权限";en[Keys::EVO_HINT_MUTE] = L"When enabled, only lowers CS2 volume without affecting other apps.Requires admin privileges.";
    cn[Keys::EVO_STATUS_DISABLED] = L"(已禁用)";en[Keys::EVO_STATUS_DISABLED] = L"(Disabled)";
    cn[Keys::EVO_LOCK_VIEW] = L"已封锁（仅查看）";en[Keys::EVO_LOCK_VIEW] = L"Blocked (View Only)";
    cn["EVO_TEXTGUI"] = L"Textgui"; en["EVO_TEXTGUI"] = L"Textgui";
    cn["EVO_TEXTGUI_X"] = L"X"; en["EVO_TEXTGUI_X"] = L"X";
    cn["EVO_TEXTGUI_Y"] = L"Y"; en["EVO_TEXTGUI_Y"] = L"Y";
    cn["EVO_TEXTGUI_SCALE"] = L"\u7f29\u653e"; en["EVO_TEXTGUI_SCALE"] = L"Scale";
    cn["EVO_TEXTGUI_OPACITY"] = L"\u900f\u660e\u5ea6"; en["EVO_TEXTGUI_OPACITY"] = L"Opacity";
    cn["EVO_TEXTGUI_LINE_SPACING"] = L"\u884c\u8ddd"; en["EVO_TEXTGUI_LINE_SPACING"] = L"Line Spacing";
    cn["EVO_TEXTGUI_SHADOW"] = L"\u9634\u5f71"; en["EVO_TEXTGUI_SHADOW"] = L"Shadow";
    cn["EVO_TEXTGUI_RAINBOW_SPEED"] = L"\u5f69\u8679\u901f\u5ea6"; en["EVO_TEXTGUI_RAINBOW_SPEED"] = L"Rainbow Speed";
    cn["EVO_TEXTGUI_RAINBOW_SPREAD"] = L"\u5f69\u8679\u8de8\u5ea6"; en["EVO_TEXTGUI_RAINBOW_SPREAD"] = L"Rainbow Spread";
    cn["EVO_TEXTGUI_RAINBOW_SATURATION"] = L"\u5f69\u8679\u9971\u548c"; en["EVO_TEXTGUI_RAINBOW_SATURATION"] = L"Rainbow Saturation";
    cn["EVO_TEXTGUI_RAINBOW_BRIGHTNESS"] = L"\u5f69\u8679\u4eae\u5ea6"; en["EVO_TEXTGUI_RAINBOW_BRIGHTNESS"] = L"Rainbow Brightness";
    cn["EVO_TEXTGUI_MARK"] = L"StrikeSense \u6807\u8bc6"; en["EVO_TEXTGUI_MARK"] = L"StrikeSense Mark";
    cn["EVO_TEXTGUI_RAINBOW"] = L"ARGB \u5f69\u8679"; en["EVO_TEXTGUI_RAINBOW"] = L"ARGB Rainbow";
    cn["EVO_NOTIFICATIONS"] = L"\u901a\u77e5\u63d0\u793a"; en["EVO_NOTIFICATIONS"] = L"Notifications";
    cn["EVO_NOTIFICATIONS_DURATION"] = L"\u6301\u7eed\u65f6\u95f4"; en["EVO_NOTIFICATIONS_DURATION"] = L"Duration";

    cn["TEXTGUI_CUSTOM_MUSICKIT"] = L"\u81ea\u5b9a\u4e49\u97f3\u4e50\u5305"; en["TEXTGUI_CUSTOM_MUSICKIT"] = L"Custom Music Kit";
    cn["TEXTGUI_KILL_SOUND"] = L"\u51fb\u6740\u97f3\u6548\u66ff\u6362"; en["TEXTGUI_KILL_SOUND"] = L"Kill Sound";
    cn["TEXTGUI_FORCE_INTERRUPT"] = L"\u5f3a\u5236\u6253\u65ad\u97f3\u6548"; en["TEXTGUI_FORCE_INTERRUPT"] = L"Force Interrupt";
    cn["TEXTGUI_FLASH_OVERLAY"] = L"\u95ea\u5149\u8986\u76d6\u56fe"; en["TEXTGUI_FLASH_OVERLAY"] = L"Flash Overlay";
    cn["TEXTGUI_LOW_MEMORY"] = L"\u4f4e\u5185\u5b58\u6a21\u5f0f"; en["TEXTGUI_LOW_MEMORY"] = L"Low Memory";
    cn["TEXTGUI_MVP_INFO"] = L"MVP\u4fe1\u606f"; en["TEXTGUI_MVP_INFO"] = L"MVP Info";
    cn["TEXTGUI_SOCD"] = L"\u540e\u8986\u76d6\u79fb\u52a8"; en["TEXTGUI_SOCD"] = L"SOCD";
    cn["TEXTGUI_MWHEEL_JUMP"] = L"\u6eda\u8f6e\u8df3"; en["TEXTGUI_MWHEEL_JUMP"] = L"Mouse Wheel Jump";
    cn["TEXTGUI_MIXED_SENSITIVITY"] = L"\u6df7\u5408\u7075\u654f\u5ea6"; en["TEXTGUI_MIXED_SENSITIVITY"] = L"Mixed Sensitivity";
    cn["TEXTGUI_RECOIL_CROSSHAIR"] = L"\u51c6\u661f\u8ddf\u968f\u540e\u5750\u529b"; en["TEXTGUI_RECOIL_CROSSHAIR"] = L"Recoil Crosshair";
    cn["TEXTGUI_KNIFE_SOUND"] = L"\u5207\u5200\u97f3\u6548\u66ff\u6362"; en["TEXTGUI_KNIFE_SOUND"] = L"Knife Sound";
    cn["TEXTGUI_DEATH_VOLUME"] = L"\u6b7b\u4ea1\u97f3\u91cf\u63a7\u5236"; en["TEXTGUI_DEATH_VOLUME"] = L"Death Volume";
    cn["TEXTGUI_SNIPER_CROSSHAIR"] = L"\u72d9\u51fb\u51c6\u661f"; en["TEXTGUI_SNIPER_CROSSHAIR"] = L"Sniper Crosshair";
    cn["TEXTGUI_ITEM_HELPER"] = L"\u9053\u5177\u52a9\u624b"; en["TEXTGUI_ITEM_HELPER"] = L"Item Helper";
    cn["TEXTGUI_QUICK_STOP"] = L"\u81ea\u52a8\u6025\u505c"; en["TEXTGUI_QUICK_STOP"] = L"Quick Stop";
    cn["TEXTGUI_MOUSE_JITTER"] = L"\u591a\u7ed1\u5b9a\u811a\u672c"; en["TEXTGUI_MOUSE_JITTER"] = L"Multi-Bind Script";
    cn["TEXTGUI_CONSOLE_LOG"] = L"\u63a7\u5236\u53f0\u65e5\u5fd7"; en["TEXTGUI_CONSOLE_LOG"] = L"Console Log";
    cn["TEXTGUI_STYLE_HOLLOW"] = L"\u7a7a\u5fc3\u5706"; en["TEXTGUI_STYLE_HOLLOW"] = L"Hollow Circle";
    cn["TEXTGUI_STYLE_CROSS"] = L"\u5341\u5b57"; en["TEXTGUI_STYLE_CROSS"] = L"Cross";
    cn["TEXTGUI_STYLE_DOT"] = L"\u5706\u70b9"; en["TEXTGUI_STYLE_DOT"] = L"Dot";
    cn["TEXTGUI_STYLE_CORNERS"] = L"\u56db\u89d2"; en["TEXTGUI_STYLE_CORNERS"] = L"Corners";
    cn["TEXTGUI_STYLE_T"] = L"T\u5f62"; en["TEXTGUI_STYLE_T"] = L"T Shape";
    cn["TEXTGUI_STYLE_X"] = L"X\u5f62"; en["TEXTGUI_STYLE_X"] = L"X Shape";

    cn[Keys::LEGAL_TITLE] = L"合法配置"; en[Keys::LEGAL_TITLE] = L"Legit Config";
    cn[Keys::LEGAL_SAVE] = L"保存"; en[Keys::LEGAL_SAVE] = L"Save";
    cn[Keys::LEGAL_REFRESH] = L"刷新"; en[Keys::LEGAL_REFRESH] = L"Refresh";
    cn[Keys::LEGAL_EDIT_HINT] = L"点击编辑框编辑 | PageUp/Down翻页 | 鼠标点击移动光标";
    en[Keys::LEGAL_EDIT_HINT] = L"Click to edit | PageUp/Down scroll | Click to position cursor";
    cn[Keys::LEGAL_ADD_SOCD] = L"写入SOCD"; en[Keys::LEGAL_ADD_SOCD] = L"Add SOCD";
    cn[Keys::LEGAL_REMOVE_SOCD] = L"移除SOCD"; en[Keys::LEGAL_REMOVE_SOCD] = L"RM SOCD";
    cn[Keys::LEGAL_ADD_MWHEEL] = L"写入滚轮跳"; en[Keys::LEGAL_ADD_MWHEEL] = L"Add MwheelJump";
    cn[Keys::LEGAL_REMOVE_MWHEEL] = L"移除滚轮跳"; en[Keys::LEGAL_REMOVE_MWHEEL] = L"RM MwheelJump";
    cn["LEGAL_ADD_MS"] = L"写入混合灵敏度"; en["LEGAL_ADD_MS"] = L"Add MixedSens";
    cn[Keys::LEGAL_REMOVE_MS] = L"移除混合灵敏度"; en[Keys::LEGAL_REMOVE_MS] = L"RM MixedSens";
    cn[Keys::LEGAL_ADD_CHSW] = L"写入准星跟随切换"; en[Keys::LEGAL_ADD_CHSW] = L"Add CrosshairSW";
    cn[Keys::LEGAL_REMOVE_CHSW] = L"移除准星跟随切换"; en[Keys::LEGAL_REMOVE_CHSW] = L"RM CrosshairSW";
    cn[Keys::LEGAL_ADD_SRP] = L"写入切刀声替换"; en[Keys::LEGAL_ADD_SRP] = L"Add Knife SoundReplace";
    cn[Keys::LEGAL_RM_SRP] = L"移除切刀声替换"; en[Keys::LEGAL_RM_SRP] = L"RM Knife SoundReplace";
    cn[Keys::LEGAL_NORMAL] = L"常规"; en[Keys::LEGAL_NORMAL] = L"Normal";
    cn[Keys::LEGAL_ATTACK] = L"开火"; en[Keys::LEGAL_ATTACK] = L"Attack";
    cn[Keys::LEGAL_CUSTOM_HINT] = L"请自行修改配置文件中的绑定"; en[Keys::LEGAL_CUSTOM_HINT] = L"Edit keybinds in config";
    cn[Keys::LEGAL_STATUS_PREFIX] = L"状态:"; en[Keys::LEGAL_STATUS_PREFIX] = L"Status:";
    cn[Keys::LEGAL_SAVED] = L"已保存"; en[Keys::LEGAL_SAVED] = L"Saved";
    cn[Keys::LEGAL_NOT_FOUND] = L"未找到"; en[Keys::LEGAL_NOT_FOUND] = L"Not found";

    cn[Keys::Rage_TITLE] = L"超频配置"; en[Keys::Rage_TITLE] = L"Semi Rage";
    cn[Keys::Rage_PLACEHOLDER] = L"功能开发中..."; en[Keys::Rage_PLACEHOLDER] = L"Coming soon...";
    cn[Keys::Rage_QUICKSTOP] = L"自动急停 (OpenDear)"; en[Keys::Rage_QUICKSTOP] = L"Auto QuickStop (OpenDear)";
    cn[Keys::Rage_LENIENT_MANUAL_STOP] = L"宽容手动急停 (BETA)"; en[Keys::Rage_LENIENT_MANUAL_STOP] = L"Lenient Manual Stop (BETA)";
    cn[Keys::Rage_MICRO_PULSE] = L"微步脉冲 (ms)"; en[Keys::Rage_MICRO_PULSE] = L"Micro Pulse (ms)";
    cn[Keys::Rage_MIN_PULSE] = L"最小脉冲 (ms)"; en[Keys::Rage_MIN_PULSE] = L"Min Pulse (ms)";
    cn[Keys::Rage_MAX_PULSE] = L"最大脉冲 (ms)"; en[Keys::Rage_MAX_PULSE] = L"Max Pulse (ms)";
    cn[Keys::Rage_CAP_PULSE] = L"上限脉冲 (ms)"; en[Keys::Rage_CAP_PULSE] = L"Cap Pulse (ms)";
    cn[Keys::Rage_MICRO_MOVE] = L"微步阈值 (ms)"; en[Keys::Rage_MICRO_MOVE] = L"Micro Move (ms)";
    cn[Keys::Rage_MOVE_START] = L"常规起点 (ms)"; en[Keys::Rage_MOVE_START] = L"Normal Start (ms)";
    cn[Keys::Rage_MOVE_CAP] = L"长移动封顶 (ms)"; en[Keys::Rage_MOVE_CAP] = L"Long Move Cap (ms)";
    cn[Keys::Rage_CURVE] = L"增长曲线 (%)"; en[Keys::Rage_CURVE] = L"Growth Curve (%)";
    cn[Keys::Rage_HORIZONTAL_SCALE] = L"横向 Peek 倍率 (%)"; en[Keys::Rage_HORIZONTAL_SCALE] = L"Horizontal Peek (%)";
    cn[Keys::Rage_VERTICAL_SCALE] = L"前后移动倍率 (%)"; en[Keys::Rage_VERTICAL_SCALE] = L"Forward/Back (%)";
    cn[Keys::Rage_MOUSE_JITTER] = L"为多绑定的脚本提供支持"; en[Keys::Rage_MOUSE_JITTER] = L"Support multi-bind scripts";
    cn[Keys::Rage_CONSOLE_LOG] = L"为读控制台的方法提供支持"; en[Keys::Rage_CONSOLE_LOG] = L"Support console.log reader";
    cn[Keys::ITEM_TITLE] = L"道具助手"; en[Keys::ITEM_TITLE] = L"Item Helper";
    cn[Keys::ITEM_PLACEHOLDER] = L"功能开发中..."; en[Keys::ITEM_PLACEHOLDER] = L"Coming soon...";

    cn[Keys::Rage_WARN_NOSAVE] = L"⚠ 由于供应商要求，超频配置启用状态不保存，每次启动程序必须手动启用";
    en[Keys::Rage_WARN_NOSAVE] = L"⚠ Due to provider request, Rage mode state cannot be saved. Manually enable it each launch.";
    
    cn[Keys::Rage_ENABLE_TEXT] = L"启用超频配置:";
    en[Keys::Rage_ENABLE_TEXT] = L"Enable Rage Mode:";
    
    cn[Keys::Rage_HINT_LINE1] = L"Micro Pulse / Min Pulse / Max Pulse / Cap Pulse：控制反向按键的最短、起步、长按和最终上限时长。";
    en[Keys::Rage_HINT_LINE1] = L"Micro/Min/Max/Cap Pulse: shortest, base, long-move and hard-cap duration for counter-strafe taps.";

    cn[Keys::Rage_HINT_LINE2] = L"Micro Move / Move Start / Move Cap：决定按住多久算微移、多久进入常规补偿，以及多久后不再继续增加急停力度。";
    en[Keys::Rage_HINT_LINE2] = L"Micro Move / Move Start / Move Cap: define micro-move, normal compensation start, and when scaling stops increasing.";
    cn[Keys::Rage_HINT_LINE3] = L"Curve：控制从 Min 到 Max 的增长快慢；值越大，越偏向在后段才把急停力度拉满。";
    en[Keys::Rage_HINT_LINE3] = L"Curve: controls how quickly the pulse grows from Min to Max; higher values ramp later.";
    cn[Keys::Rage_HINT_LINE4] = L"Horizontal Peek / Forward-Back：分别缩放左右急停和前后急停力度，100% 代表保持原始计算结果。";
    en[Keys::Rage_HINT_LINE4] = L"Horizontal Peek / Forward-Back: scale left-right and forward-back quickstop strength; 100% keeps the raw result.";
    cn[Keys::Rage_HINT_LINE5] = L"多绑定脚本支持用于鼠标抖动类脚本；控制台支持用于读取新增 console.log；右侧数字可直接输入。";
    en[Keys::Rage_HINT_LINE5] = L"Multi-bind support is for mouse-jitter scripts; console support reads new console.log entries; click values to type directly.";
    cn[Keys::Rage_HINT_LINE6] = L"宽容手动急停：短促人工反向补停不会再次触发回拉；反方向键按住较久仍按正常换向处理。";
    en[Keys::Rage_HINT_LINE6] = L"Lenient manual stop: short manual counter-taps avoid rebound; longer holds remain normal direction changes.";
    cn[Keys::Rage_JUMP_DISABLE_MS] = L"跳跃禁用时长 (ms)"; en[Keys::Rage_JUMP_DISABLE_MS] = L"Jump Disable Time (ms)";
    
    cn[Keys::Rage_REQ_ADMIN_TITLE] = L"⚠️ 权限不足";
    en[Keys::Rage_REQ_ADMIN_TITLE] = L"⚠️ Insufficient Privileges";
    
    cn[Keys::Rage_REQ_ADMIN_MSG] = L"Rage 模式需要管理员权限才能正常工作。\n是否重新以管理员身份启动程序？";
    en[Keys::Rage_REQ_ADMIN_MSG] = L"Rage mode requires Administrator privileges.\nDo you want to restart as Admin?";
    
    cn[Keys::Rage_RISK_WARNING_TITLE] = L"⚠️ 警告：Rage 模式";
    en[Keys::Rage_RISK_WARNING_TITLE] = L"⚠️ Warning: Rage Mode";
    
    cn[Keys::Rage_RISK_WARNING_MSG] = L"本页面的配置来自DearMacro，需要谨慎使用。\n我们不对它的安全性做保证。\n使用本页面造成的虚拟财产损失后果自负。\n\n您还要开启吗？";
    en[Keys::Rage_RISK_WARNING_MSG] = L"Configs are from DearMacro. Use with caution.\nWe DO NOT guarantee account safety.\nUse at your own risk.\n\nDo you still want to enable?";

    // ======  道具助手新增国际化文本  ======
    cn["ITEM_ENABLE"] = L"启用道具助手";     en["ITEM_ENABLE"] = L"Enable Items Helper";
    cn["ITEM_HOTKEY"] = L"快捷键: %s";       en["ITEM_HOTKEY"] = L"Hotkey: %s";
    cn["ITEM_BINDING"] = L"按下任意键...";    en["ITEM_BINDING"] = L"Press any key...";
    cn["ITEM_CLICK_MOD"] = L"点击修改快捷键"; en["ITEM_CLICK_MOD"] = L"Click to change hotkey";
    
    cn["ITEM_BTN_RECOVERY"] = L"恢复默认设置"; en["ITEM_BTN_RECOVERY"] = L"Reocvery Setting";
    cn["ITEM_BTN_MAKE"] = L"前往制作图片"; en["ITEM_BTN_MAKE"] = L"Goto Make Picture";
    cn["ITEM_BTN_OPEN_DIR"] = L"打开道具目录"; en["ITEM_BTN_OPEN_DIR"] = L"Open Directory";
    cn["ITEM_USAGE_TITLE"] = L"使用说明:";     en["ITEM_USAGE_TITLE"] = L"How to Use:";
    cn["ITEM_USAGE_LINE1"] = L"1. 将图片文件放入对应的地图文件夹内。"; 
    en["ITEM_USAGE_LINE1"] = L"1. Place image files into the corresponding map folders.";
    cn["ITEM_USAGE_LINE2"] = L"2. 在游戏中按下设置的快捷键呼出菜单。";
    en["ITEM_USAGE_LINE2"] = L"2. Press the hotkey in-game to bring up the menu.";
    cn["ITEM_USAGE_LINE3"] = L"3. 使用上下方向键 [↑] [↓] 切换选择道具。";
    en["ITEM_USAGE_LINE3"] = L"3. Use Up/Down arrows [↑] [↓] to switch selection.";
    cn["ITEM_USAGE_LINE4"] = L"4. 按下 [Enter] 键查看/关闭道具高清大图。";
    en["ITEM_USAGE_LINE4"] = L"4. Press [Enter] to view or close full-screen images.";

    // ======  道具助手新增国际化文本  ======
    cn["ITEM_POS_X"] = L"X 坐标";            en["ITEM_POS_X"] = L"Position X";
    cn["ITEM_POS_Y"] = L"Y 坐标";            en["ITEM_POS_Y"] = L"Position Y";
    cn["ITEM_OPACITY"] = L"透明度";          en["ITEM_OPACITY"] = L"Opacity";
    cn["ITEM_PIC_OPACITY"] = L"图片透明度";  en["ITEM_PIC_OPACITY"] = L"Pic Opacity";
    cn["ITEM_AUTOHIDE"] = L"桌面自动销毁";   en["ITEM_AUTOHIDE"] = L"Auto-Hide on Desktop";
    cn["ITEM_KEY_PREV"] = L"上一项快捷键: %s"; en["ITEM_KEY_PREV"] = L"Prev Item Key: %s";
    cn["ITEM_KEY_NEXT"] = L"下一项快捷键: %s"; en["ITEM_KEY_NEXT"] = L"Next Item Key: %s";
    cn["ITEM_KEY_SEL"] = L"确认/预览快捷键: %s"; en["ITEM_KEY_SEL"] = L"Select/Preview Key: %s";
    cn["ITEM_MSG_REC"] = L"确认将参数都设置为默认值吗？"; en["ITEM_MSG_REC"] = L"recovery setting?";

    cn["VSCRIPT_TITLE"] = L"自定脚本"; en["VSCRIPT_TITLE"] = L"Scripts";
    cn["VSCRIPT_MOUNT"] = L"挂载脚本"; en["VSCRIPT_MOUNT"] = L"Mount";
    cn["VSCRIPT_OPEN_DIR"] = L"打开目录"; en["VSCRIPT_OPEN_DIR"] = L"Open Dir";
    cn["VSCRIPT_BUILD"] = L"构建"; en["VSCRIPT_BUILD"] = L"Build";
    cn["VSCRIPT_CAPABILITY"] = L"能力"; en["VSCRIPT_CAPABILITY"] = L"Capability";
    cn["VSCRIPT_MOUNTED"] = L"已挂载脚本"; en["VSCRIPT_MOUNTED"] = L"Mounted Scripts";
    cn["VSCRIPT_CONTINUOUS"] = L"持续"; en["VSCRIPT_CONTINUOUS"] = L"Loop";
    cn["VSCRIPT_RUN"] = L"执行"; en["VSCRIPT_RUN"] = L"Run";
    cn["VSCRIPT_REMOVE"] = L"卸载"; en["VSCRIPT_REMOVE"] = L"Remove";
    cn["VSCRIPT_POLLING"] = L"轮询中"; en["VSCRIPT_POLLING"] = L"Polling";
    cn["VSCRIPT_EMPTY"] = L"暂无挂载脚本。示范脚本已生成在脚本目录。"; en["VSCRIPT_EMPTY"] = L"No mounted scripts. Samples are in the script folder.";
    cn["VSCRIPT_PREVIOUS"] = L"上一页"; en["VSCRIPT_PREVIOUS"] = L"Previous";
    cn["VSCRIPT_NEXT"] = L"下一页"; en["VSCRIPT_NEXT"] = L"Next";
    cn["VSCRIPT_PAGE"] = L"第 %d / %d 页，共 %d 个脚本"; en["VSCRIPT_PAGE"] = L"Page %d / %d, %d scripts";

    //刀
// 刀型翻译定义
    cn["LEGAL_SRP_NONE"] = L"不替换"; en["LEGAL_SRP_NONE"] = L"None";
    cn["LEGAL_SRP_STILLETTO"] = L"短剑"; en["LEGAL_SRP_STILLETTO"] = L"Stiletto";
    cn["LEGAL_SRP_GYPSY"] = L"折刀"; en["LEGAL_SRP_GYPSY"] = L"Navaja";
    cn["LEGAL_SRP_PUSH"] = L"双匕"; en["LEGAL_SRP_PUSH"] = L"Shadow Daggers";
    cn["LEGAL_SRP_WIDOW"] = L"锯齿"; en["LEGAL_SRP_WIDOW"] = L"Talon";
    cn["LEGAL_SRP_FALCHION"] = L"折叠"; en["LEGAL_SRP_FALCHION"] = L"Falchion";
    cn["LEGAL_SRP_URSUS"] = L"博伊"; en["LEGAL_SRP_URSUS"] = L"Ursus";
    cn["LEGAL_SRP_BUTTERFLY"] = L"蝴蝶"; en["LEGAL_SRP_BUTTERFLY"] = L"Butterfly";
}

const wchar_t* i18n::T(const char* key) {
    auto& m = g_langCN ? s_cn : s_en;
    auto it = m.find(key);
    return it != m.end() ? it->second.c_str() : L"??";
}

void i18n::Switch() { g_langCN = !g_langCN; }

namespace i18n { namespace Keys {
    const char *SIDEBAR_FILE="SIDEBAR_FILE",*SIDEBAR_SETTINGS="SIDEBAR_SETTINGS",*SIDEBAR_EVOLUTION="SIDEBAR_EVOLUTION",*SIDEBAR_LEGAL="SIDEBAR_LEGAL",*SIDEBAR_Rage="SIDEBAR_Rage",*SIDEBAR_ITEMHELPER="SIDEBAR_ITEMHELPER";
    const char *SOUNDS_TITLE="SOUNDS_TITLE",*SOUNDS_GSI_RUNNING="SOUNDS_GSI_RUNNING",*SOUNDS_GSI_STOPPED="SOUNDS_GSI_STOPPED",*SOUNDS_ENABLED="SOUNDS_ENABLED",*SOUNDS_DISABLED="SOUNDS_DISABLED",*SOUNDS_SELECT="SOUNDS_SELECT",*SOUNDS_FOLDER_BTN="SOUNDS_FOLDER_BTN";
    const char *SETTINGS_TITLE="SETTINGS_TITLE",*SETTINGS_VOL="SETTINGS_VOL",*SETTINGS_CUSTOM_MUSIC="SETTINGS_CUSTOM_MUSIC",*SETTINGS_KILL_SOUND="SETTINGS_KILL_SOUND",*SETTINGS_FORCE_INTERRUPT="SETTINGS_FORCE_INTERRUPT",*SETTINGS_FLASH="SETTINGS_FLASH",*SETTINGS_LOWMEM="SETTINGS_LOWMEM",*SETTINGS_MVP="SETTINGS_MVP",*SETTINGS_OGG="SETTINGS_OGG";
    const char *EVO_TITLE="EVO_TITLE",*EVO_VOL_ADJ="EVO_VOL_ADJ",*EVO_HOTKEY="EVO_HOTKEY",*EVO_CROSSHAIR="EVO_CROSSHAIR",*EVO_THICKNESS="EVO_THICKNESS",*EVO_SCALE="EVO_SCALE",*EVO_STYLE="EVO_STYLE",*EVO_ENABLE="EVO_ENABLE";
    const char *EVO_STYLE_HOLLOW="EVO_STYLE_HOLLOW",*EVO_STYLE_SOLID="EVO_STYLE_SOLID",*EVO_STYLE_CLASSIC="EVO_STYLE_CLASSIC",*EVO_WAITING_KEY="EVO_WAITING_KEY",*EVO_CLICK_MODIFY="EVO_CLICK_MODIFY",*EVO_STATUS_NORMAL="EVO_STATUS_NORMAL",*EVO_STATUS_MUTED="EVO_STATUS_MUTED";
    const char *LEGAL_TITLE="LEGAL_TITLE",*LEGAL_SAVE="LEGAL_SAVE",*LEGAL_REFRESH="LEGAL_REFRESH",*LEGAL_EDIT_HINT="LEGAL_EDIT_HINT",*LEGAL_ADD_SOCD="LEGAL_ADD_SOCD",*LEGAL_REMOVE_SOCD="LEGAL_REMOVE_SOCD",*LEGAL_ADD_MWHEEL="LEGAL_ADD_MWHEEL",*LEGAL_REMOVE_MWHEEL="LEGAL_REMOVE_MWHEEL";
    const char *LEGAL_ADD_MS="LEGAL_ADD_MS",*LEGAL_REMOVE_MS="LEGAL_REMOVE_MS",*LEGAL_ADD_CHSW="LEGAL_ADD_CHSW",*LEGAL_REMOVE_CHSW="LEGAL_REMOVE_CHSW",*LEGAL_NORMAL="LEGAL_NORMAL",*LEGAL_ATTACK="LEGAL_ATTACK",*LEGAL_CUSTOM_HINT="LEGAL_CUSTOM_HINT";
    const char *LEGAL_STATUS_PREFIX="LEGAL_STATUS_PREFIX",*LEGAL_SAVED="LEGAL_SAVED",*LEGAL_NOT_FOUND="LEGAL_NOT_FOUND";
    const char *Rage_TITLE="Rage_TITLE",*Rage_PLACEHOLDER="Rage_PLACEHOLDER",*Rage_QUICKSTOP="Rage_QUICKSTOP",*Rage_MICRO_PULSE="Rage_MICRO_PULSE",*Rage_MIN_PULSE="Rage_MIN_PULSE",*Rage_MAX_PULSE="Rage_MAX_PULSE",*Rage_CAP_PULSE="Rage_CAP_PULSE";
    const char *Rage_MICRO_MOVE="Rage_MICRO_MOVE",*Rage_MOVE_START="Rage_MOVE_START",*Rage_MOVE_CAP="Rage_MOVE_CAP",*Rage_CURVE="Rage_CURVE",*Rage_HORIZONTAL_SCALE="Rage_HORIZONTAL_SCALE",*Rage_VERTICAL_SCALE="Rage_VERTICAL_SCALE";
    const char* Rage_MOUSE_JITTER = "Rage_MOUSE_JITTER";
    const char* Rage_CONSOLE_LOG = "Rage_CONSOLE_LOG";
    const char* Rage_LENIENT_MANUAL_STOP = "Rage_LENIENT_MANUAL_STOP";
    const char* Rage_JUMP_DISABLE_MS = "Rage_JUMP_DISABLE_MS";
    const char *ITEM_TITLE="ITEM_TITLE",*ITEM_PLACEHOLDER="ITEM_PLACEHOLDER";
    const char* EVO_HINT_MUTE = "evo_hint_mute";
    const char* EVO_STATUS_DISABLED = "evo_status_disabled";
    const char* EVO_LOCK_VIEW = "evo_lock_view";

    const char* Rage_WARN_NOSAVE = "Rage_WARN_NOSAVE";
    const char* Rage_ENABLE_TEXT = "Rage_ENABLE_TEXT";
    const char* Rage_HINT_LINE1 = "Rage_HINT_LINE1";
    const char* Rage_HINT_LINE2 = "Rage_HINT_LINE2";
    const char* Rage_HINT_LINE3 = "Rage_HINT_LINE3";
    const char* Rage_HINT_LINE4 = "Rage_HINT_LINE4";
    const char* Rage_HINT_LINE5 = "Rage_HINT_LINE5";
    const char* Rage_HINT_LINE6 = "Rage_HINT_LINE6";
    const char* Rage_REQ_ADMIN_TITLE = "Rage_REQ_ADMIN_TITLE";
    const char* Rage_REQ_ADMIN_MSG = "Rage_REQ_ADMIN_MSG";
    const char* Rage_RISK_WARNING_TITLE = "Rage_RISK_WARNING_TITLE";
    const char* Rage_RISK_WARNING_MSG = "Rage_RISK_WARNING_MSG";
    const char* SOUNDS_IMAGE_BTN = "SOUNDS_IMAGE_BTN";
    const char* ITEM_BTN_MAKE = "ITEM_BTN_MAKE";
    const char* ITEM_BTN_RECOVERY = "ITEM_BTN_RECOVERY";
	const char* ITEM_OPACITY = "ITEM_OPACITY";
    const char* ITEM_PIC_OPACITY = "ITEM_PIC_OPACITY";
    const char* LEGAL_ADD_SRP = "LEGAL_ADD_SRP";
    const char* LEGAL_RM_SRP = "LEGAL_RM_SRP";
}}
