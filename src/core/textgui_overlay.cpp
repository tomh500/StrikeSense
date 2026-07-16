#include "textgui_overlay.h"

#include "StrikeSense.h"
#include "config.h"
#include "console_log.h"
#include "gsi_server.h"
#include "itemhelper_overlay.h"
#include "mouse_jitter.h"
#include "notifications_overlay.h"
#include "pages.h"
#include "quickstop.h"
#include "volume_mixer.h"
#include "vscript.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <gdiplus.h>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace textgui_overlay {
namespace {

HWND s_hwnd = nullptr;
HINSTANCE s_hInst = nullptr;
bool s_visible = false;
bool s_crosshairRecoilFollow = false;
std::map<std::wstring, std::wstring> s_customLines;
std::set<std::wstring> s_lastFeatureSet;
bool s_hasFeatureSnapshot = false;
bool s_consoleReaderNeeded = false;
constexpr UINT_PTR kRefreshTimer = 3011;

bool has_window()
{
    return s_hwnd && IsWindow(s_hwnd);
}

void hide_overlay()
{
    if (!has_window() || !s_visible) return;
    ShowWindow(s_hwnd, SW_HIDE);
    s_visible = false;
    std::cout << "[Textgui] 游戏窗口未激活，覆盖层已隐藏。" << std::endl;
}

bool should_show_overlay()
{
    return g_textguiEnabled && IsCS2WindowActive();
}

std::wstring script_display_name(const vscript::mounted_script& script)
{
    if (script.hasMetadataName) return vscript::GetScriptDisplayName(script);
    return std::filesystem::path(script.path).filename().wstring();
}

std::vector<std::wstring> collect_enabled_features()
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
    if (IsRageModeEnabled() && IsQuickStopEnabled()) features.push_back(L"自动急停");
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

void sync_notifications_for_features()
{
    const auto features = collect_enabled_features();
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

float text_width_f(Gdiplus::Graphics& g, Gdiplus::Font& font, const std::wstring& text)
{
    if (text.empty()) return 0.f;
    Gdiplus::RectF bounds;
    Gdiplus::StringFormat fmt(Gdiplus::StringFormat::GenericTypographic());
    fmt.SetFormatFlags(fmt.GetFormatFlags() | Gdiplus::StringFormatFlagsMeasureTrailingSpaces);
    g.MeasureString(text.c_str(), static_cast<INT>(text.size()), &font,
        Gdiplus::PointF(0.f, 0.f), &fmt, &bounds);
    return bounds.Width;
}

int text_width(Gdiplus::Graphics& g, Gdiplus::Font& font, const std::wstring& text)
{
    const float width = text_width_f(g, font, text);
    return static_cast<int>(std::ceil(width));
}

Gdiplus::Color color_from_hue(float hue, BYTE alpha, float saturation, float brightness)
{
    hue = std::fmod(hue, 360.f);
    if (hue < 0.f) hue += 360.f;
    saturation = std::clamp(saturation, 0.f, 1.f);
    brightness = std::clamp(brightness, 0.2f, 1.f);
    const float c = brightness * saturation;
    const float x = c * (1.f - std::fabs(std::fmod(hue / 60.f, 2.f) - 1.f));
    const float m = brightness - c;
    float r = 0.f, g = 0.f, b = 0.f;
    if (hue < 60.f) { r = c; g = x; }
    else if (hue < 120.f) { r = x; g = c; }
    else if (hue < 180.f) { g = c; b = x; }
    else if (hue < 240.f) { g = x; b = c; }
    else if (hue < 300.f) { r = x; b = c; }
    else { r = c; b = x; }
    return Gdiplus::Color(alpha,
        static_cast<BYTE>((r + m) * 255.f),
        static_cast<BYTE>((g + m) * 255.f),
        static_cast<BYTE>((b + m) * 255.f));
}

void draw_text(Gdiplus::Graphics& g, const std::wstring& text, Gdiplus::Font& font,
    float x, float y, const Gdiplus::Color& color, BYTE alpha)
{
    const float shadowStrength = std::clamp(g_textguiShadowStrength, 0.f, 1.f);
    Gdiplus::SolidBrush shadow(Gdiplus::Color(static_cast<BYTE>(alpha * shadowStrength), 0, 0, 0));
    Gdiplus::SolidBrush brush(color);
    if (shadowStrength > 0.01f) g.DrawString(text.c_str(), -1, &font, Gdiplus::PointF(x + 1.f, y + 1.f), &shadow);
    g.DrawString(text.c_str(), -1, &font, Gdiplus::PointF(x, y), &brush);
}

void draw_rainbow_text(Gdiplus::Graphics& g, const std::wstring& text, Gdiplus::Font& font,
    float x, float y, BYTE alpha, float baseHue)
{
    const float shadowStrength = std::clamp(g_textguiShadowStrength, 0.f, 1.f);
    const float hueStep = std::clamp(g_textguiRainbowSpread, 4.f, 45.f);
    const float saturation = std::clamp(g_textguiRainbowSaturation, 0.f, 1.f);
    const float brightness = std::clamp(g_textguiRainbowBrightness, 0.2f, 1.f);
    Gdiplus::SolidBrush shadow(Gdiplus::Color(static_cast<BYTE>(alpha * shadowStrength), 0, 0, 0));
    Gdiplus::StringFormat fmt(Gdiplus::StringFormat::GenericTypographic());
    fmt.SetFormatFlags(fmt.GetFormatFlags() | Gdiplus::StringFormatFlagsMeasureTrailingSpaces);
    float cursor = x;
    for (size_t i = 0; i < text.size(); ++i) {
        const std::wstring ch(1, text[i]);
        const float hue = baseHue + static_cast<float>(i) * hueStep;
        Gdiplus::SolidBrush brush(color_from_hue(hue, alpha, saturation, brightness));
        if (shadowStrength > 0.01f) g.DrawString(ch.c_str(), 1, &font, Gdiplus::PointF(cursor + 1.f, y + 1.f), &fmt, &shadow);
        g.DrawString(ch.c_str(), 1, &font, Gdiplus::PointF(cursor, y), &fmt, &brush);
        cursor += text_width_f(g, font, ch);
    }
}

void update_console_reader_need()
{
    const bool needed = g_textguiEnabled && HasLegalCfgCrosshairSwitch();
    if (needed == s_consoleReaderNeeded) return;
    s_consoleReaderNeeded = needed;
    consolelog::SetRuntimeReaderNeeded(needed);
    std::cout << "[Textgui] 准星跟随控制台读取已" << (needed ? "启用" : "停用") << "。" << std::endl;
}

void redraw()
{
    if (!has_window() || !should_show_overlay()) return;

    const int sw = GetSystemMetrics(SM_CXSCREEN);
    const int sh = GetSystemMetrics(SM_CYSCREEN);
    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMeasure = CreateCompatibleDC(hdcScreen);
    HBITMAP measureBitmap = CreateCompatibleBitmap(hdcScreen, 1, 1);
    HBITMAP oldMeasureBitmap = static_cast<HBITMAP>(SelectObject(hdcMeasure, measureBitmap));

    using namespace Gdiplus;
    const float scale = std::clamp(g_textguiScale, 0.75f, 1.8f);
    const float opacity = std::clamp(g_textguiOpacity, 0.2f, 1.0f);
    const BYTE alpha = static_cast<BYTE>(255.f * opacity);
    const Color fixedColor(alpha,
        static_cast<BYTE>(std::clamp(g_textguiR, 0, 255)),
        static_cast<BYTE>(std::clamp(g_textguiG, 0, 255)),
        static_cast<BYTE>(std::clamp(g_textguiB, 0, 255)));
    Font titleFont(L"Microsoft YaHei UI", 20.f * scale, FontStyleBold);
    Font itemFont(L"Microsoft YaHei UI", 13.5f * scale, FontStyleBold);
    const float rainbowSpeed = std::clamp(g_textguiRainbowSpeed, 0.1f, 5.0f);
    const float baseHue = std::fmod(static_cast<float>(GetTickCount64()) * 0.12f * rainbowSpeed, 360.f);
    const float lineSpacing = std::clamp(g_textguiLineSpacing, 0.75f, 1.8f);

    auto features = collect_enabled_features();
    {
        Graphics measure(hdcMeasure);
        measure.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
        std::sort(features.begin(), features.end(), [&](const auto& a, const auto& b) {
            return text_width(measure, itemFont, a) > text_width(measure, itemFont, b);
        });
    }

    Graphics measure(hdcMeasure);
    measure.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    const float rowH = 25.f * scale * lineSpacing;
    const int maxFeatureW = features.empty() ? 0 : text_width(measure, itemFont, features.front());
    const int maxTextW = (std::max)(text_width(measure, titleFont, L"StrikeSense"), maxFeatureW);
    const float areaWf = (std::max)(185.f * scale, maxTextW + 8.f * scale);
    const float areaHf = (g_textguiShowWatermark ? 35.f * scale : 0.f)
        + static_cast<float>(features.size()) * rowH + 8.f * scale;
    const int areaW = (std::max)(1, static_cast<int>(std::ceil(areaWf + 4.f)));
    const int areaH = (std::max)(1, static_cast<int>(std::ceil(areaHf + 4.f)));
    const float margin = 18.f * scale;
    const int dstX = static_cast<int>(std::clamp((sw - areaWf - margin) * std::clamp(g_textguiX, 0.f, 1.f), margin, sw - areaWf - margin));
    const int dstY = static_cast<int>(std::clamp((sh - areaHf - margin) * std::clamp(g_textguiY, 0.f, 1.f), margin, sh - areaHf - margin));

    HDC hdcMem = CreateCompatibleDC(hdcScreen);
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = areaW;
    bmi.bmiHeader.biHeight = -areaH;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HBITMAP oldBitmap = static_cast<HBITMAP>(SelectObject(hdcMem, bitmap));

    {
        using namespace Gdiplus;
        Graphics g(hdcMem);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
        g.Clear(Color(0, 0, 0, 0));

        float cy = 2.f;
        if (g_textguiShowWatermark) {
            const std::wstring title = L"StrikeSense";
            const float tx = areaWf - text_width(g, titleFont, title);
            if (g_textguiRainbow) draw_rainbow_text(g, title, titleFont, tx, cy, alpha, baseHue);
            else draw_text(g, title, titleFont, tx, cy, fixedColor, alpha);
            cy += 35.f * scale;
        }

        for (size_t i = 0; i < features.size(); ++i) {
            const auto& feature = features[i];
            const float tx = areaWf - text_width(g, itemFont, feature);
            if (g_textguiRainbow) draw_rainbow_text(g, feature, itemFont, tx, cy, alpha, baseHue + static_cast<float>(i) * 26.f);
            else draw_text(g, feature, itemFont, tx, cy, fixedColor, alpha);
            cy += rowH;
        }
    }

    POINT dst{ dstX, dstY };
    SIZE size{ areaW, areaH };
    POINT src{ 0, 0 };
    BLENDFUNCTION blend{};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;
    UpdateLayeredWindow(s_hwnd, hdcScreen, &dst, &size, hdcMem, &src, 0, &blend, ULW_ALPHA);

    SelectObject(hdcMem, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(hdcMem);
    SelectObject(hdcMeasure, oldMeasureBitmap);
    DeleteObject(measureBitmap);
    DeleteDC(hdcMeasure);
    ReleaseDC(nullptr, hdcScreen);
}

void update_visibility()
{
    if (!has_window() || !g_textguiEnabled) return;
    if (!should_show_overlay()) {
        hide_overlay();
        return;
    }

    if (!s_visible) {
        ShowWindow(s_hwnd, SW_SHOWNOACTIVATE);
        s_visible = true;
        std::cout << "[Textgui] 覆盖层已显示。" << std::endl;
    }
    redraw();
}

LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_TIMER:
        if (wp == kRefreshTimer) {
            update_visibility();
            return 0;
        }
        break;
    case WM_CLOSE:
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, kRefreshTimer);
        s_hwnd = nullptr;
        s_visible = false;
        return 0;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

} // namespace

void Initialize(HINSTANCE hInst)
{
    s_hInst = hInst;
    if (has_window()) return;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = wnd_proc;
    wc.hInstance = hInst;
    wc.lpszClassName = L"StrikeSense_Textgui";
    RegisterClassExW(&wc);

    s_hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT
        | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        wc.lpszClassName, L"", WS_POPUP, 0, 0,
        GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
        nullptr, nullptr, hInst, nullptr);

    if (!s_hwnd) {
        std::cout << "[Textgui] 创建覆盖层窗口失败。" << std::endl;
        return;
    }

    SetTimer(s_hwnd, kRefreshTimer, 33, nullptr);
    ShowWindow(s_hwnd, SW_HIDE);
    std::cout << "[Textgui] 覆盖层窗口已创建。" << std::endl;
}

void ApplyEnabled(bool enabled)
{
    g_textguiEnabled = enabled;
    update_console_reader_need();
    if (enabled) {
        if (!has_window()) Initialize(s_hInst ? s_hInst : hInst);
        if (!s_hasFeatureSnapshot) {
            const auto features = collect_enabled_features();
            s_lastFeatureSet = std::set<std::wstring>(features.begin(), features.end());
            s_hasFeatureSnapshot = true;
        }
        SetWindowPos(s_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        update_visibility();
        std::cout << "[Textgui] 已开启。" << std::endl;
        return;
    }

    if (has_window()) ShowWindow(s_hwnd, SW_HIDE);
    s_visible = false;
    update_console_reader_need();
    std::cout << "[Textgui] 已关闭。" << std::endl;
}

void Refresh()
{
    if (!g_textguiEnabled) return;
    update_console_reader_need();
    if (!has_window()) Initialize(s_hInst ? s_hInst : hInst);
    sync_notifications_for_features();
    update_visibility();
}

void Shutdown()
{
    consolelog::SetRuntimeReaderNeeded(false);
    s_consoleReaderNeeded = false;
    if (has_window()) DestroyWindow(s_hwnd);
    s_hwnd = nullptr;
    s_visible = false;
    std::cout << "[Textgui] 已释放覆盖层。" << std::endl;
}

void RegisterCustomLine(const std::wstring& id, const std::wstring& text)
{
    if (id.empty()) return;
    s_customLines[id] = text;
    std::wcout << L"[Textgui] 脚本注册文字: " << id << L" => " << text << std::endl;
    Refresh();
}

void RemoveCustomLine(const std::wstring& id)
{
    if (id.empty()) return;
    s_customLines.erase(id);
    std::wcout << L"[Textgui] 脚本移除文字: " << id << std::endl;
    Refresh();
}

void UpdateCrosshairRecoilSignal(const std::wstring& text)
{
    if (text.find(L"/cr1") != std::wstring::npos) {
        if (s_crosshairRecoilFollow) return;
        s_crosshairRecoilFollow = true;
        std::cout << "[Textgui] 已读取准星跟随后坐力状态: 开启" << std::endl;
        Refresh();
    } else if (text.find(L"/cr0") != std::wstring::npos) {
        if (!s_crosshairRecoilFollow) return;
        s_crosshairRecoilFollow = false;
        std::cout << "[Textgui] 已读取准星跟随后坐力状态: 关闭" << std::endl;
        Refresh();
    }
}

} // namespace textgui_overlay
