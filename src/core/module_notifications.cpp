#include "module_notifications.h"

#include "config.h"
#include "console_log.h"
#include "gsi_server.h"
#include "i18n.h"
#include "itemhelper_overlay.h"
#include "mouse_jitter.h"
#include "notifications_overlay.h"
#include "pages.h"
#include "quickstop.h"
#include "vscript.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <unordered_set>

namespace modulenotifications {
namespace {

bool s_crosshairRecoilFollow = false;
bool s_hasFeatureSnapshot = false;
bool s_consoleReaderNeeded = false;
std::map<std::wstring, std::wstring> s_lastFeatureSet;
std::map<std::wstring, feature_line> s_customLines;
std::unordered_set<std::wstring> s_hiddenModules;

const std::vector<std::wstring> kNativeModuleIds = {
    L"custom_musickit", L"kill_sound", L"force_interrupt", L"flash_overlay",
    L"low_memory", L"mvp_info", L"socd", L"mwheel_jump", L"mixed_sensitivity",
    L"recoil_crosshair", L"knife_sound", L"death_volume", L"sniper_crosshair",
    L"item_helper", L"quick_stop", L"mouse_jitter", L"console_log",
    L"textgui", L"notifications"
};

std::wstring tr(const char* key)
{
    return i18n::T(key);
}

std::wstring script_display_name(const vscript::mounted_script& script)
{
    if (script.hasMetadataName) return vscript::GetScriptDisplayName(script);
    return std::filesystem::path(script.path).filename().wstring();
}

std::wstring crosshair_style_text()
{
    static const char* keys[] = {
        "TEXTGUI_STYLE_HOLLOW", "TEXTGUI_STYLE_CROSS", "TEXTGUI_STYLE_DOT",
        "TEXTGUI_STYLE_CORNERS", "TEXTGUI_STYLE_T", "TEXTGUI_STYLE_X"
    };
    return tr(keys[std::clamp(g_crosshairStyle, 0, 5)]);
}

std::vector<std::wstring> split_tokens(const std::wstring& text)
{
    std::wistringstream stream(text);
    std::vector<std::wstring> tokens;
    std::wstring token;
    while (stream >> token) tokens.push_back(token);
    return tokens;
}

std::wstring join_tokens(const std::vector<std::wstring>& tokens, size_t begin, size_t end)
{
    std::wstring out;
    for (size_t i = begin; i < end && i < tokens.size(); ++i) {
        if (!out.empty()) out += L" ";
        out += tokens[i];
    }
    return out;
}

bool parse_bool_token(const std::wstring& token, bool& out)
{
    if (token == L"1" || token == L"true" || token == L"on" || token == L"enable") {
        out = true;
        return true;
    }
    if (token == L"0" || token == L"false" || token == L"off" || token == L"disable") {
        out = false;
        return true;
    }
    return false;
}

bool is_integer_text(const std::wstring& text)
{
    if (text.empty()) return false;
    size_t i = text[0] == L'-' || text[0] == L'+' ? 1 : 0;
    if (i >= text.size()) return false;
    for (; i < text.size(); ++i) {
        if (text[i] < L'0' || text[i] > L'9') return false;
    }
    return true;
}

void save_settings(config::Settings& settings)
{
    config::Save(settings);
    RefreshTextguiOverlay();
}

void update_console_reader_need()
{
    const bool needed = HasLegalCfgCrosshairSwitch();
    if (needed == s_consoleReaderNeeded) return;
    s_consoleReaderNeeded = needed;
    consolelog::SetRuntimeReaderNeeded(needed);
    std::cout << "[模块通知] 准星跟随控制台读取已"
              << (needed ? "启用" : "停用") << "。" << std::endl;
}

} // namespace

std::vector<feature_line> CollectEnabledFeatures(bool includeHidden)
{
    std::vector<feature_line> features;
    const config::Settings& settings = gsi::GetConfig();

    const auto add = [&](const wchar_t* id, const std::wstring& text,
        const std::wstring& accessory = L"") {
        if (includeHidden || !IsModuleHidden(id)) features.push_back({ id, text, accessory });
    };

    if (settings.custom_musickit) add(L"custom_musickit", tr("TEXTGUI_CUSTOM_MUSICKIT"), settings.ogg ? L"OGG" : L"WAV");
    if (settings.enable_kill_sound) add(L"kill_sound", tr("TEXTGUI_KILL_SOUND"));
    if (settings.force_interrupt) add(L"force_interrupt", tr("TEXTGUI_FORCE_INTERRUPT"));
    if (settings.custom_flashbang) add(L"flash_overlay", tr("TEXTGUI_FLASH_OVERLAY"));
    if (settings.low_memory) add(L"low_memory", tr("TEXTGUI_LOW_MEMORY"));
    if (settings.show_mvp) add(L"mvp_info", tr("TEXTGUI_MVP_INFO"));
    if (HasLegalCfgSOCD()) add(L"socd", tr("TEXTGUI_SOCD"), L"Joy");
    if (HasLegalCfgMwheelJump()) add(L"mwheel_jump", tr("TEXTGUI_MWHEEL_JUMP"), L"Normal");
    if (HasLegalCfgMixedSensitivity()) {
        const auto [normal, attack] = GetLegalCfgMixedSensitivityValues();
        add(L"mixed_sensitivity", tr("TEXTGUI_MIXED_SENSITIVITY"), normal + L" " + attack);
    }
    if (HasLegalCfgCrosshairSwitch() && s_crosshairRecoilFollow) add(L"recoil_crosshair", tr("TEXTGUI_RECOIL_CROSSHAIR"));
    if (HasLegalCfgSoundReplace()) add(L"knife_sound", tr("TEXTGUI_KNIFE_SOUND"));
    if (g_deathMute) add(L"death_volume", tr("TEXTGUI_DEATH_VOLUME"));
    if (g_crosshairEnabled) add(L"sniper_crosshair", tr("TEXTGUI_SNIPER_CROSSHAIR"), crosshair_style_text());
    if (itemhelper_overlay::IsOverlayVisible()) {
        const std::wstring mapName = std::filesystem::path(gsi::gamemap).filename().wstring();
        add(L"item_helper", tr("TEXTGUI_ITEM_HELPER"), mapName);
    }
    if (IsQuickStopRuntimeActive()) add(L"quick_stop", tr("TEXTGUI_QUICK_STOP"));
    if (mousejitter::IsEnabled()) add(L"mouse_jitter", tr("TEXTGUI_MOUSE_JITTER"));
    if (consolelog::IsEnabled()) add(L"console_log", tr("TEXTGUI_CONSOLE_LOG"));

    for (const auto& script : vscript::MountedScripts()) {
        if (!script.continuous || (!includeHidden && !script.showInTextgui)) continue;
        std::wstring name = script_display_name(script);
        const std::wstring id = L"script:" + script.path;
        if (!name.empty() && (includeHidden || !IsModuleHidden(id))) features.push_back({ id, name, L"" });
    }

    for (const auto& [id, line] : s_customLines) {
        if (!line.text.empty() && (includeHidden || !IsModuleHidden(id))) features.push_back(line);
    }

    return features;
}

void Refresh()
{
    update_console_reader_need();

    const auto features = CollectEnabledFeatures(true);
    std::map<std::wstring, std::wstring> current;
    for (const auto& feature : features) current[feature.id] = feature.text;
    if (!s_hasFeatureSnapshot) {
        s_lastFeatureSet = current;
        s_hasFeatureSnapshot = true;
        return;
    }

    for (const auto& [id, text] : current) {
        if (s_lastFeatureSet.find(id) == s_lastFeatureSet.end())
            notifications_overlay::Push(text, true);
    }
    for (const auto& [id, text] : s_lastFeatureSet) {
        if (current.find(id) == current.end())
            notifications_overlay::Push(text, false);
    }
    s_lastFeatureSet = current;
}

void Shutdown()
{
    consolelog::SetRuntimeReaderNeeded(false);
    s_consoleReaderNeeded = false;
}

void RegisterCustomLine(const std::wstring& id, const std::wstring& text,
    const std::wstring& accessory)
{
    if (id.empty()) return;
    s_customLines[id] = { id, text, accessory };
    Refresh();
}

void RemoveCustomLine(const std::wstring& id)
{
    if (id.empty()) return;
    s_customLines.erase(id);
    Refresh();
}

void SetModuleHidden(const std::wstring& id, bool hidden)
{
    if (id.empty()) return;
    if (hidden) s_hiddenModules.insert(id);
    else s_hiddenModules.erase(id);
    std::wcout << L"[Textgui] 模块 " << id
               << (hidden ? L" 已隐藏。" : L" 已恢复显示。") << std::endl;
}

bool IsModuleHidden(const std::wstring& id)
{
    return s_hiddenModules.find(id) != s_hiddenModules.end();
}

std::vector<std::wstring> NativeModuleIds()
{
    return kNativeModuleIds;
}

void UpdateCrosshairRecoilSignal(const std::wstring& text)
{
    if (text.find(L"/cr1") != std::wstring::npos) {
        if (s_crosshairRecoilFollow) return;
        s_crosshairRecoilFollow = true;
        std::cout << "[模块通知] 已读取准星跟随后坐力状态: 开启" << std::endl;
        Refresh();
    } else if (text.find(L"/cr0") != std::wstring::npos) {
        if (!s_crosshairRecoilFollow) return;
        s_crosshairRecoilFollow = false;
        std::cout << "[模块通知] 已读取准星跟随后坐力状态: 关闭" << std::endl;
        Refresh();
    }
}

bool ProcessConsoleCommand(const std::wstring& text)
{
    if (text.rfind(L"/notificationE ", 0) == 0) {
        notifications_overlay::Push(text.substr(15), true);
        return true;
    }
    if (text.rfind(L"/notificationD ", 0) == 0) {
        notifications_overlay::Push(text.substr(15), false);
        return true;
    }

    const auto tokens = split_tokens(text);
    if (tokens.empty()) return false;
    if (tokens[0] == L"/textgui" && tokens.size() >= 3) {
        if (tokens[1] == L"del") {
            RemoveCustomLine(tokens[2]);
            RefreshTextguiOverlay();
            return true;
        }
        if (tokens[1] == L"reg" && tokens.size() >= 5) {
            const std::wstring id = tokens.back();
            if (!is_integer_text(id)) return false;
            RegisterCustomLine(id, tokens[2], join_tokens(tokens, 3, tokens.size() - 1));
            RefreshTextguiOverlay();
            return true;
        }
    }
    if (tokens[0] == L"/toggle" && tokens.size() >= 2) {
        bool enabled = false;
        const bool hasExplicitState = tokens.size() >= 3 && parse_bool_token(tokens[2], enabled);
        const bool ok = hasExplicitState ? SetModuleEnabled(tokens[1], enabled) : ToggleModuleEnabled(tokens[1]);
        if (ok) {
            RefreshTextguiOverlay();
            notifications_overlay::Push(tokens[1], GetModuleEnabled(tokens[1]));
        }
        return ok;
    }
    return false;
}

bool GetModuleEnabled(const std::wstring& id)
{
    const config::Settings& settings = gsi::GetConfig();
    if (id == L"custom_musickit") return settings.custom_musickit;
    if (id == L"kill_sound") return settings.enable_kill_sound;
    if (id == L"force_interrupt") return settings.force_interrupt;
    if (id == L"flash_overlay") return settings.custom_flashbang;
    if (id == L"low_memory") return settings.low_memory;
    if (id == L"mvp_info") return settings.show_mvp;
    if (id == L"socd") return HasLegalCfgSOCD();
    if (id == L"mwheel_jump") return HasLegalCfgMwheelJump();
    if (id == L"mixed_sensitivity") return HasLegalCfgMixedSensitivity();
    if (id == L"recoil_crosshair") return HasLegalCfgCrosshairSwitch() && s_crosshairRecoilFollow;
    if (id == L"knife_sound") return HasLegalCfgSoundReplace();
    if (id == L"death_volume") return g_deathMute;
    if (id == L"sniper_crosshair") return g_crosshairEnabled;
    if (id == L"item_helper") return g_itemHelperEnabled;
    if (id == L"quick_stop") return IsQuickStopEnabled();
    if (id == L"mouse_jitter") return mousejitter::IsEnabled();
    if (id == L"console_log") return consolelog::IsEnabled();
    if (id == L"textgui") return g_textguiEnabled;
    if (id == L"notifications") return g_notificationsEnabled;
    if (s_customLines.find(id) != s_customLines.end()) return !IsModuleHidden(id);
    return false;
}

bool SetModuleEnabled(const std::wstring& id, bool enabled)
{
    config::Settings& settings = gsi::GetConfig();
    if (id == L"custom_musickit") { settings.custom_musickit = enabled; save_settings(settings); return true; }
    if (id == L"kill_sound") { settings.enable_kill_sound = enabled; save_settings(settings); return true; }
    if (id == L"force_interrupt") { settings.force_interrupt = enabled; save_settings(settings); return true; }
    if (id == L"flash_overlay") { settings.custom_flashbang = enabled; save_settings(settings); return true; }
    if (id == L"low_memory") { settings.low_memory = enabled; save_settings(settings); return true; }
    if (id == L"mvp_info") { settings.show_mvp = enabled; save_settings(settings); return true; }
    if (id == L"socd") return SetLegalCfgSOCD(enabled);
    if (id == L"mwheel_jump") return SetLegalCfgMwheelJump(enabled);
    if (id == L"mixed_sensitivity") return SetLegalCfgMixedSensitivity(enabled);
    if (id == L"recoil_crosshair") {
        if (!SetLegalCfgCrosshairSwitch(enabled)) return false;
        s_crosshairRecoilFollow = enabled;
        Refresh();
        return true;
    }
    if (id == L"knife_sound") return SetLegalCfgSoundReplace(enabled);
    if (id == L"death_volume") {
        g_deathMute = enabled;
        SaveEvolutionParams();
        Refresh();
        return true;
    }
    if (id == L"sniper_crosshair") {
        ApplyCrosshairEnabled(enabled);
        SaveEvolutionParams();
        Refresh();
        return true;
    }
    if (id == L"item_helper") {
        g_itemHelperEnabled = enabled;
        Refresh();
        return true;
    }
    if (id == L"quick_stop") {
        SetQuickStopEnabled(enabled);
        Refresh();
        return true;
    }
    if (id == L"mouse_jitter") {
        mousejitter::SetEnabled(enabled);
        Refresh();
        return true;
    }
    if (id == L"console_log") {
        consolelog::SetEnabled(enabled);
        Refresh();
        return true;
    }
    if (id == L"textgui") {
        ApplyTextguiEnabled(enabled);
        SaveEvolutionParams();
        return true;
    }
    if (id == L"notifications") {
        g_notificationsEnabled = enabled;
        SaveEvolutionParams();
        return true;
    }
    if (s_customLines.find(id) != s_customLines.end()) {
        SetModuleHidden(id, !enabled);
        return true;
    }
    return false;
}

bool ToggleModuleEnabled(const std::wstring& id)
{
    return SetModuleEnabled(id, !GetModuleEnabled(id));
}

std::wstring GetModuleValue(const std::wstring& id, const std::wstring& field)
{
    const config::Settings& settings = gsi::GetConfig();
    if (id == L"custom_musickit" && (field.empty() || field == L"format")) return settings.ogg ? L"OGG" : L"WAV";
    if (id == L"mwheel_jump" && (field.empty() || field == L"mode")) return L"Normal";
    if (id == L"socd" && (field.empty() || field == L"mode")) return L"Joy";
    if (id == L"mixed_sensitivity") {
        const auto [normal, attack] = GetLegalCfgMixedSensitivityValues();
        if (field == L"normal") return normal;
        if (field == L"attack") return attack;
        return normal + L" " + attack;
    }
    if (id == L"sniper_crosshair" && (field.empty() || field == L"style")) return crosshair_style_text();
    if (id == L"item_helper" && (field.empty() || field == L"map")) return std::filesystem::path(gsi::gamemap).filename().wstring();
    const auto it = s_customLines.find(id);
    if (it != s_customLines.end()) {
        if (field == L"text") return it->second.text;
        return it->second.accessory;
    }
    return L"";
}

} // namespace modulenotifications
