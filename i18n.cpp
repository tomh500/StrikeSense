#include "i18n.h"
#include <unordered_map>

static std::unordered_map<std::string, std::wstring> s_cn, s_en;

void i18n::Init() {
    auto& cn = s_cn; auto& en = s_en;
    cn[Keys::SIDEBAR_FILE] = L"文件位置"; en[Keys::SIDEBAR_FILE] = L"File Location";
    cn[Keys::SIDEBAR_SETTINGS] = L"遗产核心"; en[Keys::SIDEBAR_SETTINGS] = L"Legacy Core";
    cn[Keys::SIDEBAR_EVOLUTION] = L"进化分支"; en[Keys::SIDEBAR_EVOLUTION] = L"Evolution";
    cn[Keys::SIDEBAR_LEGAL] = L"合法配置"; en[Keys::SIDEBAR_LEGAL] = L"Legit Config";
    cn[Keys::SIDEBAR_Rage] = L"超频配置"; en[Keys::SIDEBAR_Rage] = L"Rage";
    cn[Keys::SIDEBAR_ITEMHELPER] = L"道具助手"; en[Keys::SIDEBAR_ITEMHELPER] = L"Item Helper";
    
    cn[Keys::SOUNDS_TITLE] = L"文件位置"; en[Keys::SOUNDS_TITLE] = L"File Location";
    cn[Keys::SOUNDS_GSI_RUNNING] = L"运行中"; en[Keys::SOUNDS_GSI_RUNNING] = L"Running";
    cn[Keys::SOUNDS_GSI_STOPPED] = L"未启动"; en[Keys::SOUNDS_GSI_STOPPED] = L"Stopped";
    cn[Keys::SOUNDS_ENABLED] = L"已启用"; en[Keys::SOUNDS_ENABLED] = L"Enabled";
    cn[Keys::SOUNDS_DISABLED] = L"已禁用"; en[Keys::SOUNDS_DISABLED] = L"Disabled";
    cn[Keys::SOUNDS_SELECT] = L"选择..."; en[Keys::SOUNDS_SELECT] = L"Browse...";
    cn[Keys::SOUNDS_FOLDER_BTN] = L"打开默认音频文件夹"; en[Keys::SOUNDS_FOLDER_BTN] = L"Open Audio Folder";

    cn[Keys::SETTINGS_TITLE] = L"遗产核心"; en[Keys::SETTINGS_TITLE] = L"Legacy Core";
    cn[Keys::SETTINGS_VOL] = L"音量"; en[Keys::SETTINGS_VOL] = L"Volume";
    cn[Keys::SETTINGS_CUSTOM_MUSIC] = L"自定义音乐包"; en[Keys::SETTINGS_CUSTOM_MUSIC] = L"Custom Music Kit";
    cn[Keys::SETTINGS_KILL_SOUND] = L"击杀音效替换"; en[Keys::SETTINGS_KILL_SOUND] = L"Kill Sound";
    cn[Keys::SETTINGS_FLASH] = L"闪光叠加"; en[Keys::SETTINGS_FLASH] = L"Flash Overlay";
    cn[Keys::SETTINGS_LOWMEM] = L"低内存模式"; en[Keys::SETTINGS_LOWMEM] = L"Low Memory";
    cn[Keys::SETTINGS_MVP] = L"MVP信息板"; en[Keys::SETTINGS_MVP] = L"MVP Board";
    cn[Keys::SETTINGS_OGG] = L"OGG格式"; en[Keys::SETTINGS_OGG] = L"OGG Format";

    cn[Keys::EVO_TITLE] = L"进化分支"; en[Keys::EVO_TITLE] = L"Evolution";
    cn[Keys::EVO_VOL_ADJ] = L"即时音量调整器"; en[Keys::EVO_VOL_ADJ] = L"Instant Volume";
    cn[Keys::EVO_HOTKEY] = L"快捷键"; en[Keys::EVO_HOTKEY] = L"Hotkey";
    cn[Keys::EVO_CROSSHAIR] = L"狙击准星设置"; en[Keys::EVO_CROSSHAIR] = L"Crosshair Settings";
    cn[Keys::EVO_THICKNESS] = L"粗细"; en[Keys::EVO_THICKNESS] = L"Thickness";
    cn[Keys::EVO_SCALE] = L"缩放"; en[Keys::EVO_SCALE] = L"Scale";
    cn[Keys::EVO_STYLE] = L"样式"; en[Keys::EVO_STYLE] = L"Style";
    cn[Keys::EVO_ENABLE] = L"启用"; en[Keys::EVO_ENABLE] = L"Enable";
    cn[Keys::EVO_STYLE_HOLLOW] = L"空心圆"; en[Keys::EVO_STYLE_HOLLOW] = L"Hollow";
    cn[Keys::EVO_STYLE_SOLID] = L"实心圆"; en[Keys::EVO_STYLE_SOLID] = L"Solid";
    cn[Keys::EVO_STYLE_CLASSIC] = L"经典"; en[Keys::EVO_STYLE_CLASSIC] = L"Classic";
    cn[Keys::EVO_WAITING_KEY] = L"按下任何字母键或数字键..."; en[Keys::EVO_WAITING_KEY] = L"Press any letter or number...";
    cn[Keys::EVO_CLICK_MODIFY] = L"点击修改快捷键"; en[Keys::EVO_CLICK_MODIFY] = L"Click to rebind";
    cn[Keys::EVO_STATUS_NORMAL] = L"正常"; en[Keys::EVO_STATUS_NORMAL] = L"Normal";
    cn[Keys::EVO_STATUS_MUTED] = L"降低开启"; en[Keys::EVO_STATUS_MUTED] = L"Muted";

    cn[Keys::LEGAL_TITLE] = L"合法配置"; en[Keys::LEGAL_TITLE] = L"Legit Config";
    cn[Keys::LEGAL_SAVE] = L"保存"; en[Keys::LEGAL_SAVE] = L"Save";
    cn[Keys::LEGAL_REFRESH] = L"刷新"; en[Keys::LEGAL_REFRESH] = L"Refresh";
    cn[Keys::LEGAL_EDIT_HINT] = L"点击编辑框编辑 | PageUp/Down翻页 | 鼠标点击移动光标";
    en[Keys::LEGAL_EDIT_HINT] = L"Click to edit | PageUp/Down scroll | Click to position cursor";
    cn[Keys::LEGAL_ADD_SOCD] = L"写入SOCD"; en[Keys::LEGAL_ADD_SOCD] = L"Add SOCD";
    cn[Keys::LEGAL_REMOVE_SOCD] = L"移除SOCD"; en[Keys::LEGAL_REMOVE_SOCD] = L"Remove SOCD";
    cn[Keys::LEGAL_ADD_MWHEEL] = L"写入滚轮跳"; en[Keys::LEGAL_ADD_MWHEEL] = L"Add MwheelJump";
    cn[Keys::LEGAL_REMOVE_MWHEEL] = L"移除滚轮跳"; en[Keys::LEGAL_REMOVE_MWHEEL] = L"Remove MwheelJump";
    cn[Keys::LEGAL_ADD_MS] = L"写入混合灵敏度"; en[Keys::LEGAL_ADD_MS] = L"Add MixedSens";
    cn[Keys::LEGAL_REMOVE_MS] = L"移除混合灵敏度"; en[Keys::LEGAL_REMOVE_MS] = L"Remove MixedSens";
    cn[Keys::LEGAL_ADD_CHSW] = L"写入准星跟随切换"; en[Keys::LEGAL_ADD_CHSW] = L"Add CrosshairSW";
    cn[Keys::LEGAL_REMOVE_CHSW] = L"移除准星跟随切换"; en[Keys::LEGAL_REMOVE_CHSW] = L"Remove CrosshairSW";
    cn[Keys::LEGAL_NORMAL] = L"常规"; en[Keys::LEGAL_NORMAL] = L"Normal";
    cn[Keys::LEGAL_ATTACK] = L"开火"; en[Keys::LEGAL_ATTACK] = L"Attack";
    cn[Keys::LEGAL_CUSTOM_HINT] = L"请自行修改配置文件中的绑定"; en[Keys::LEGAL_CUSTOM_HINT] = L"Edit keybinds in config";
    cn[Keys::LEGAL_STATUS_PREFIX] = L"状态:"; en[Keys::LEGAL_STATUS_PREFIX] = L"Status:";
    cn[Keys::LEGAL_SAVED] = L"已保存"; en[Keys::LEGAL_SAVED] = L"Saved";
    cn[Keys::LEGAL_NOT_FOUND] = L"未找到"; en[Keys::LEGAL_NOT_FOUND] = L"Not found";

    cn[Keys::Rage_TITLE] = L"超频配置"; en[Keys::Rage_TITLE] = L"Rage";
    cn[Keys::Rage_PLACEHOLDER] = L"功能开发中..."; en[Keys::Rage_PLACEHOLDER] = L"Coming soon...";
    cn[Keys::ITEM_TITLE] = L"道具助手"; en[Keys::ITEM_TITLE] = L"Item Helper";
    cn[Keys::ITEM_PLACEHOLDER] = L"功能开发中..."; en[Keys::ITEM_PLACEHOLDER] = L"Coming soon...";
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
    const char *SETTINGS_TITLE="SETTINGS_TITLE",*SETTINGS_VOL="SETTINGS_VOL",*SETTINGS_CUSTOM_MUSIC="SETTINGS_CUSTOM_MUSIC",*SETTINGS_KILL_SOUND="SETTINGS_KILL_SOUND",*SETTINGS_FLASH="SETTINGS_FLASH",*SETTINGS_LOWMEM="SETTINGS_LOWMEM",*SETTINGS_MVP="SETTINGS_MVP",*SETTINGS_OGG="SETTINGS_OGG";
    const char *EVO_TITLE="EVO_TITLE",*EVO_VOL_ADJ="EVO_VOL_ADJ",*EVO_HOTKEY="EVO_HOTKEY",*EVO_CROSSHAIR="EVO_CROSSHAIR",*EVO_THICKNESS="EVO_THICKNESS",*EVO_SCALE="EVO_SCALE",*EVO_STYLE="EVO_STYLE",*EVO_ENABLE="EVO_ENABLE";
    const char *EVO_STYLE_HOLLOW="EVO_STYLE_HOLLOW",*EVO_STYLE_SOLID="EVO_STYLE_SOLID",*EVO_STYLE_CLASSIC="EVO_STYLE_CLASSIC",*EVO_WAITING_KEY="EVO_WAITING_KEY",*EVO_CLICK_MODIFY="EVO_CLICK_MODIFY",*EVO_STATUS_NORMAL="EVO_STATUS_NORMAL",*EVO_STATUS_MUTED="EVO_STATUS_MUTED";
    const char *LEGAL_TITLE="LEGAL_TITLE",*LEGAL_SAVE="LEGAL_SAVE",*LEGAL_REFRESH="LEGAL_REFRESH",*LEGAL_EDIT_HINT="LEGAL_EDIT_HINT",*LEGAL_ADD_SOCD="LEGAL_ADD_SOCD",*LEGAL_REMOVE_SOCD="LEGAL_REMOVE_SOCD",*LEGAL_ADD_MWHEEL="LEGAL_ADD_MWHEEL",*LEGAL_REMOVE_MWHEEL="LEGAL_REMOVE_MWHEEL";
    const char *LEGAL_ADD_MS="LEGAL_ADD_MS",*LEGAL_REMOVE_MS="LEGAL_REMOVE_MS",*LEGAL_ADD_CHSW="LEGAL_ADD_CHSW",*LEGAL_REMOVE_CHSW="LEGAL_REMOVE_CHSW",*LEGAL_NORMAL="LEGAL_NORMAL",*LEGAL_ATTACK="LEGAL_ATTACK",*LEGAL_CUSTOM_HINT="LEGAL_CUSTOM_HINT";
    const char *LEGAL_STATUS_PREFIX="LEGAL_STATUS_PREFIX",*LEGAL_SAVED="LEGAL_SAVED",*LEGAL_NOT_FOUND="LEGAL_NOT_FOUND";
    const char *Rage_TITLE="Rage_TITLE",*Rage_PLACEHOLDER="Rage_PLACEHOLDER";
    const char *ITEM_TITLE="ITEM_TITLE",*ITEM_PLACEHOLDER="ITEM_PLACEHOLDER";
}}