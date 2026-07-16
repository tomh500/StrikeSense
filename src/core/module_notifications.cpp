#include "module_notifications.h"

#include "config.h"
#include "console_log.h"
#include "gsi_server.h"
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
    L"item_helper", L"quick_stop", L"mouse_jitter", L"console_log"
};

std::wstring script_display_name(const vscript::mounted_script& script)
{
    if (script.hasMetadataName) return vscript::GetScriptDisplayName(script);
    return std::filesystem::path(script.path).filename().wstring();
}

void update_console_reader_need()
{
    const bool needed = HasLegalCfgCrosshairSwitch();
    if (needed == s_consoleReaderNeeded) return;
    s_consoleReaderNeeded = needed;
    consolelog::SetRuntimeReaderNeeded(needed);
    std::cout << "[模块通知] 准星跟随控制台读取已" << (needed ? "启用" : "停用") << "。" << std::endl;
}

} // namespace

std::vector<feature_line> CollectEnabledFeatures(bool includeHidden)
{
    std::vector<feature_line> features;
    const config::Settings& settings = gsi::GetConfig();

    const auto add = [&](const wchar_t* id, const wchar_t* text,
        const std::wstring& accessory = L"") {
        if (includeHidden || !IsModuleHidden(id)) features.push_back({ id, text, accessory });
    };

    if (settings.custom_musickit) add(L"custom_musickit", L"自定义音乐包", settings.ogg ? L"OGG" : L"WAV");
    if (settings.enable_kill_sound) add(L"kill_sound", L"击杀音效替换");
    if (settings.force_interrupt) add(L"force_interrupt", L"强制打断音效");
    if (settings.custom_flashbang) add(L"flash_overlay", L"闪光覆盖图");
    if (settings.low_memory) add(L"low_memory", L"低内存模式");
    if (settings.show_mvp) add(L"mvp_info", L"MVP信息");
    if (HasLegalCfgSOCD()) add(L"socd", L"后覆盖移动", L"Joy");
    if (HasLegalCfgMwheelJump()) add(L"mwheel_jump", L"滚轮跳", L"Normal");
    if (HasLegalCfgMixedSensitivity()) {
        const auto [normal, attack] = GetLegalCfgMixedSensitivityValues();
        add(L"mixed_sensitivity", L"混合灵敏度", normal + L" " + attack);
    }
    if (HasLegalCfgCrosshairSwitch() && s_crosshairRecoilFollow) add(L"recoil_crosshair", L"准星跟随后坐力");
    if (HasLegalCfgSoundReplace()) add(L"knife_sound", L"切刀音效替换");
    if (g_deathMute) add(L"death_volume", L"死亡音量控制");
    if (g_crosshairEnabled) {
        static const wchar_t* styles[] = { L"空心圆", L"十字", L"圆点", L"四角", L"T形", L"X形" };
        add(L"sniper_crosshair", L"狙击准星", styles[std::clamp(g_crosshairStyle, 0, 5)]);
    }
    if (itemhelper_overlay::IsOverlayVisible()) {
        const std::wstring mapName = std::filesystem::path(gsi::gamemap).filename().wstring();
        add(L"item_helper", L"道具助手", mapName);
    }
    if (IsQuickStopRuntimeActive()) add(L"quick_stop", L"自动急停");
    if (mousejitter::IsEnabled()) add(L"mouse_jitter", L"多绑定脚本");
    if (consolelog::IsEnabled()) add(L"console_log", L"控制台日志");

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
    std::wcout << L"[Textgui] 模块 " << id << (hidden ? L" 已隐藏。" : L" 已恢复显示。") << std::endl;
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

} // namespace modulenotifications
