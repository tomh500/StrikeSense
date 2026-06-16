#include "i18n.h"
#include <unordered_map>

static std::unordered_map<std::string, std::wstring> s_cn;
static std::unordered_map<std::string, std::wstring> s_en;

void i18n::Init() {
    s_cn[Keys::SIDEBAR_FILE]       = L"文件位置";
    s_en[Keys::SIDEBAR_FILE]       = L"File Location";
    s_cn[Keys::SIDEBAR_SETTINGS]    = L"遗产核心";
    s_en[Keys::SIDEBAR_SETTINGS]    = L"Legacy Core";
    s_cn[Keys::SIDEBAR_EVOLUTION]   = L"进化分支";
    s_en[Keys::SIDEBAR_EVOLUTION]   = L"Evolution";
    s_cn[Keys::SIDEBAR_LEGAL]      = L"合法配置";
    s_en[Keys::SIDEBAR_LEGAL]      = L"Legal Config";
    s_cn[Keys::SIDEBAR_OVERCLOCK]  = L"超频配置";
    s_en[Keys::SIDEBAR_OVERCLOCK]  = L"Overclock";
    s_cn[Keys::SIDEBAR_ITEMHELPER] = L"道具助手";
    s_en[Keys::SIDEBAR_ITEMHELPER] = L"Item Helper";

    s_cn[Keys::LEGAL_TITLE]         = L"合法配置";
    s_en[Keys::LEGAL_TITLE]         = L"Legal Config";
    s_cn[Keys::LEGAL_SAVE]          = L"保存";
    s_en[Keys::LEGAL_SAVE]          = L"Save";
    s_cn[Keys::LEGAL_REFRESH]       = L"刷新";
    s_en[Keys::LEGAL_REFRESH]       = L"Refresh";
    s_cn[Keys::LEGAL_ADD_SOCD]      = L"写入SOCD";
    s_en[Keys::LEGAL_ADD_SOCD]      = L"Add SOCD";
    s_cn[Keys::LEGAL_REMOVE_SOCD]   = L"移除SOCD";
    s_en[Keys::LEGAL_REMOVE_SOCD]   = L"Remove SOCD";
    s_cn[Keys::LEGAL_ADD_MWHEEL]    = L"写入滚轮跳";
    s_en[Keys::LEGAL_ADD_MWHEEL]    = L"Add MwheelJump";
    s_cn[Keys::LEGAL_REMOVE_MWHEEL] = L"移除滚轮跳";
    s_en[Keys::LEGAL_REMOVE_MWHEEL] = L"Remove MwheelJump";
    s_cn[Keys::LEGAL_ADD_MS]        = L"写入混合灵敏度";
    s_en[Keys::LEGAL_ADD_MS]        = L"Add MixedSens";
    s_cn[Keys::LEGAL_REMOVE_MS]     = L"移除混合灵敏度";
    s_en[Keys::LEGAL_REMOVE_MS]     = L"Remove MixedSens";
    s_cn[Keys::LEGAL_ADD_CHSW]      = L"写入准星跟随切换";
    s_en[Keys::LEGAL_ADD_CHSW]      = L"Add CrosshairSW";
    s_cn[Keys::LEGAL_REMOVE_CHSW]   = L"移除准星跟随切换";
    s_en[Keys::LEGAL_REMOVE_CHSW]   = L"Remove CrosshairSW";
    s_cn[Keys::LEGAL_NORMAL]        = L"常规";
    s_en[Keys::LEGAL_NORMAL]        = L"Normal";
    s_cn[Keys::LEGAL_ATTACK]        = L"开火";
    s_en[Keys::LEGAL_ATTACK]        = L"Attack";
    s_cn[Keys::LEGAL_CUSTOM_HINT]   = L"请自行修改配置文件中的绑定";
    s_en[Keys::LEGAL_CUSTOM_HINT]   = L"Edit keybinds in config file";

    s_cn[Keys::SETTINGS_TITLE]      = L"遗产核心";
    s_en[Keys::SETTINGS_TITLE]      = L"Legacy Core";
    s_cn[Keys::SETTINGS_VOL]        = L"音量";
    s_en[Keys::SETTINGS_VOL]        = L"Volume";

    s_cn[Keys::EVO_TITLE]           = L"进化分支";
    s_en[Keys::EVO_TITLE]           = L"Evolution";
    s_cn[Keys::EVO_VOL_ADJ]         = L"即时音量调整器";
    s_en[Keys::EVO_VOL_ADJ]         = L"Instant Volume";
    s_cn[Keys::EVO_HOTKEY]          = L"快捷键";
    s_en[Keys::EVO_HOTKEY]          = L"Hotkey";
    s_cn[Keys::EVO_CROSSHAIR]       = L"狙击准星设置";
    s_en[Keys::EVO_CROSSHAIR]       = L"Crosshair Settings";
    s_cn[Keys::EVO_THICKNESS]       = L"粗细";
    s_en[Keys::EVO_THICKNESS]       = L"Thickness";
    s_cn[Keys::EVO_SCALE]           = L"缩放";
    s_en[Keys::EVO_SCALE]           = L"Scale";
    s_cn[Keys::EVO_STYLE]           = L"样式";
    s_en[Keys::EVO_STYLE]           = L"Style";
    s_cn[Keys::EVO_ENABLE]          = L"启用";
    s_en[Keys::EVO_ENABLE]          = L"Enable";
}

const wchar_t* i18n::T(const char* key) {
    auto& map = g_langCN ? s_cn : s_en;
    auto it = map.find(key);
    if (it != map.end()) return it->second.c_str();
    return L"??";
}

void i18n::Switch() {
    g_langCN = !g_langCN;
}

namespace i18n {
    namespace Keys {
        const char* SIDEBAR_FILE = "SIDEBAR_FILE";
        const char* SIDEBAR_SETTINGS = "SIDEBAR_SETTINGS";
        const char* SIDEBAR_EVOLUTION = "SIDEBAR_EVOLUTION";
        const char* SIDEBAR_LEGAL = "SIDEBAR_LEGAL";
        const char* SIDEBAR_OVERCLOCK = "SIDEBAR_OVERCLOCK";
        const char* SIDEBAR_ITEMHELPER = "SIDEBAR_ITEMHELPER";
        const char* STATUS_SAVED = "STATUS_SAVED";
        const char* STATUS_LOADING = "STATUS_LOADING";
        const char* STATUS_ERROR = "STATUS_ERROR";
        const char* LEGAL_TITLE = "LEGAL_TITLE";
        const char* LEGAL_STATUS = "LEGAL_STATUS";
        const char* LEGAL_SAVE = "LEGAL_SAVE";
        const char* LEGAL_REFRESH = "LEGAL_REFRESH";
        const char* LEGAL_EDIT_HINT = "LEGAL_EDIT_HINT";
        const char* LEGAL_ADD_SOCD = "LEGAL_ADD_SOCD";
        const char* LEGAL_REMOVE_SOCD = "LEGAL_REMOVE_SOCD";
        const char* LEGAL_ADD_MWHEEL = "LEGAL_ADD_MWHEEL";
        const char* LEGAL_REMOVE_MWHEEL = "LEGAL_REMOVE_MWHEEL";
        const char* LEGAL_ADD_MS = "LEGAL_ADD_MS";
        const char* LEGAL_REMOVE_MS = "LEGAL_REMOVE_MS";
        const char* LEGAL_ADD_CHSW = "LEGAL_ADD_CHSW";
        const char* LEGAL_REMOVE_CHSW = "LEGAL_REMOVE_CHSW";
        const char* LEGAL_NORMAL = "LEGAL_NORMAL";
        const char* LEGAL_ATTACK = "LEGAL_ATTACK";
        const char* LEGAL_CUSTOM_HINT = "LEGAL_CUSTOM_HINT";
        const char* SETTINGS_TITLE = "SETTINGS_TITLE";
        const char* SETTINGS_VOL = "SETTINGS_VOL";
        const char* EVO_TITLE = "EVO_TITLE";
        const char* EVO_VOL_ADJ = "EVO_VOL_ADJ";
        const char* EVO_HOTKEY = "EVO_HOTKEY";
        const char* EVO_CROSSHAIR = "EVO_CROSSHAIR";
        const char* EVO_THICKNESS = "EVO_THICKNESS";
        const char* EVO_SCALE = "EVO_SCALE";
        const char* EVO_STYLE = "EVO_STYLE";
        const char* EVO_ENABLE = "EVO_ENABLE";
    }
}