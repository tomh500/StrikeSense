#pragma once
#include <string>

extern bool g_langCN;
#define _(K) i18n::T(K)

namespace i18n {
    void Init();
    const wchar_t* T(const char* key);
    void Switch();
    
namespace Keys {
        extern const char* SIDEBAR_FILE, *SIDEBAR_SETTINGS, *SIDEBAR_EVOLUTION, *SIDEBAR_LEGAL, *SIDEBAR_Rage, *SIDEBAR_ITEMHELPER;
        extern const char* SOUNDS_TITLE, *SOUNDS_GSI_RUNNING, *SOUNDS_GSI_STOPPED, *SOUNDS_ENABLED, *SOUNDS_DISABLED, *SOUNDS_SELECT, *SOUNDS_FOLDER_BTN;
        extern const char* SETTINGS_TITLE, *SETTINGS_VOL, *SETTINGS_CUSTOM_MUSIC, *SETTINGS_KILL_SOUND, *SETTINGS_FLASH, *SETTINGS_LOWMEM, *SETTINGS_MVP, *SETTINGS_OGG;
        extern const char* EVO_TITLE, *EVO_VOL_ADJ, *EVO_HOTKEY, *EVO_CROSSHAIR, *EVO_THICKNESS, *EVO_SCALE, *EVO_STYLE, *EVO_ENABLE;
        extern const char* EVO_STYLE_HOLLOW, *EVO_STYLE_SOLID, *EVO_STYLE_CLASSIC, *EVO_WAITING_KEY, *EVO_CLICK_MODIFY, *EVO_STATUS_NORMAL, *EVO_STATUS_MUTED;
        extern const char* EVO_HINT_MUTE, *EVO_STATUS_DISABLED, *EVO_LOCK_VIEW;
        extern const char* LEGAL_TITLE, *LEGAL_SAVE, *LEGAL_REFRESH, *LEGAL_EDIT_HINT, *LEGAL_ADD_SOCD, *LEGAL_REMOVE_SOCD, *LEGAL_ADD_MWHEEL, *LEGAL_REMOVE_MWHEEL;
        extern const char* LEGAL_ADD_MS, *LEGAL_REMOVE_MS, *LEGAL_ADD_CHSW, *LEGAL_REMOVE_CHSW, *LEGAL_NORMAL, *LEGAL_ATTACK, *LEGAL_CUSTOM_HINT;
        extern const char* LEGAL_STATUS_PREFIX, *LEGAL_SAVED, *LEGAL_NOT_FOUND;
        extern const char* Rage_TITLE, *Rage_PLACEHOLDER, *Rage_QUICKSTOP, *Rage_MIN_PULSE, *Rage_MAX_PULSE, *Rage_CAP_PULSE, *Rage_MOVE_START, *Rage_MOVE_CAP;
        extern const char* ITEM_TITLE, *ITEM_PLACEHOLDER;

        // ===== 修复：改成符合项目结构的 const char* 外部声明 =====
        extern const char* Rage_WARN_NOSAVE;
        extern const char* Rage_ENABLE_TEXT;
        extern const char* Rage_HINT_LINE1;
        extern const char* Rage_HINT_LINE2;
        extern const char* Rage_REQ_ADMIN_TITLE;
        extern const char* Rage_REQ_ADMIN_MSG;
        extern const char* Rage_RISK_WARNING_TITLE;
        extern const char* Rage_RISK_WARNING_MSG;
    }
}