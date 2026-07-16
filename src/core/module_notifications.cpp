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

#include <filesystem>
#include <iostream>
#include <map>
#include <set>

namespace modulenotifications {
namespace {

bool s_crosshairRecoilFollow = false;
bool s_hasFeatureSnapshot = false;
bool s_consoleReaderNeeded = false;
std::set<std::wstring> s_lastFeatureSet;
std::map<std::wstring, std::wstring> s_customLines;

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

std::vector<std::wstring> CollectEnabledFeatures()
{
    std::vector<std::wstring> features;
    const config::Settings& settings = gsi::GetConfig();

    if (settings.custom_musickit) features.push_back(L"自定义音乐包");
    if (settings.enable_kill_sound) features.push_back(L"击杀音效替换");
    if (settings.force_interrupt) features.push_back(L"强制打断音效");
    if (settings.custom_flashbang) features.push_back(L"闪光覆盖图");
    if (settings.low_memory) features.push_back(L"低内存模式");
    if (settings.show_mvp) features.push_back(L"MVP信息");
    if (HasLegalCfgSOCD()) features.push_back(L"SOCD");
    if (HasLegalCfgMwheelJump()) features.push_back(L"滚轮跳");
    if (HasLegalCfgMixedSensitivity()) features.push_back(L"混合灵敏度");
    if (HasLegalCfgCrosshairSwitch() && s_crosshairRecoilFollow) features.push_back(L"准星跟随后坐力");
    if (HasLegalCfgSoundReplace()) features.push_back(L"切刀音效替换");
    if (g_deathMute) features.push_back(L"死亡音量控制");
    if (g_crosshairEnabled) features.push_back(L"狙击准星");
    if (itemhelper_overlay::IsOverlayVisible()) features.push_back(L"道具助手");
    if (IsRageModeEnabled()) features.push_back(L"超频配置");
    if (IsQuickStopRuntimeActive()) features.push_back(L"自动急停");
    if (mousejitter::IsEnabled()) features.push_back(L"多绑定脚本");
    if (consolelog::IsEnabled()) features.push_back(L"控制台日志");

    for (const auto& script : vscript::MountedScripts()) {
        if (!script.continuous) continue;
        std::wstring name = script_display_name(script);
        if (!name.empty()) features.push_back(name);
    }

    for (const auto& [id, text] : s_customLines) {
        if (!text.empty()) features.push_back(text);
    }

    return features;
}

void Refresh()
{
    update_console_reader_need();

    const auto features = CollectEnabledFeatures();
    const std::set<std::wstring> current(features.begin(), features.end());
    if (!s_hasFeatureSnapshot) {
        s_lastFeatureSet = current;
        s_hasFeatureSnapshot = true;
        return;
    }

    for (const auto& feature : current) {
        if (s_lastFeatureSet.find(feature) == s_lastFeatureSet.end())
            notifications_overlay::Push(feature, true);
    }
    for (const auto& feature : s_lastFeatureSet) {
        if (current.find(feature) == current.end())
            notifications_overlay::Push(feature, false);
    }
    s_lastFeatureSet = current;
}

void Shutdown()
{
    consolelog::SetRuntimeReaderNeeded(false);
    s_consoleReaderNeeded = false;
}

void RegisterCustomLine(const std::wstring& id, const std::wstring& text)
{
    if (id.empty()) return;
    s_customLines[id] = text;
    Refresh();
}

void RemoveCustomLine(const std::wstring& id)
{
    if (id.empty()) return;
    s_customLines.erase(id);
    Refresh();
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
