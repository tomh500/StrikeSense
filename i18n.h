#pragma once
#include <string>
#include <unordered_map>

// ===== 国际化 (i18n) =====
extern bool g_langCN;

namespace i18n {
    void Init();

    // 获取翻译文本
    const wchar_t* T(const char* key);
    void Switch();  // 切换语言后刷新

    // 常用字符串键名
    namespace Keys {
        // 侧边栏
        extern const char* SIDEBAR_FILE;
        extern const char* SIDEBAR_SETTINGS;
        extern const char* SIDEBAR_EVOLUTION;
        extern const char* SIDEBAR_LEGAL;
        extern const char* SIDEBAR_OVERCLOCK;
        extern const char* SIDEBAR_ITEMHELPER;

        // 通用
        extern const char* STATUS_SAVED;
        extern const char* STATUS_LOADING;
        extern const char* STATUS_ERROR;

        // 合法配置
        extern const char* LEGAL_TITLE;
        extern const char* LEGAL_STATUS;
        extern const char* LEGAL_SAVE;
        extern const char* LEGAL_REFRESH;
        extern const char* LEGAL_EDIT_HINT;
        extern const char* LEGAL_ADD_SOCD;
        extern const char* LEGAL_REMOVE_SOCD;
        extern const char* LEGAL_ADD_MWHEEL;
        extern const char* LEGAL_REMOVE_MWHEEL;
        extern const char* LEGAL_ADD_MS;
        extern const char* LEGAL_REMOVE_MS;
        extern const char* LEGAL_ADD_CHSW;
        extern const char* LEGAL_REMOVE_CHSW;
        extern const char* LEGAL_NORMAL;
        extern const char* LEGAL_ATTACK;
        extern const char* LEGAL_CUSTOM_HINT;

        // 遗产核心
        extern const char* SETTINGS_TITLE;
        extern const char* SETTINGS_VOL;

        // 进化分支
        extern const char* EVO_TITLE;
        extern const char* EVO_VOL_ADJ;
        extern const char* EVO_HOTKEY;
        extern const char* EVO_CROSSHAIR;
        extern const char* EVO_THICKNESS;
        extern const char* EVO_SCALE;
        extern const char* EVO_STYLE;
        extern const char* EVO_ENABLE;
    }
}