#include "textgui_overlay.h"

#include "StrikeSense.h"
#include "config.h"
#include "console_log.h"
#include "gsi_server.h"
#include "mouse_jitter.h"
#include "pages.h"
#include "quickstop.h"
#include "volume_mixer.h"
#include "vscript.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <gdiplus.h>
#include <iostream>
#include <string>
#include <vector>

namespace textgui_overlay {
namespace {

HWND s_hwnd = nullptr;
HINSTANCE s_hInst = nullptr;
bool s_visible = false;
constexpr UINT_PTR kRefreshTimer = 3011;

bool has_window()
{
    return s_hwnd && IsWindow(s_hwnd);
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
    if (IsLegalCfgManaged()) features.push_back(L"合法CFG接管");
    if (g_deathMute) features.push_back(L"死亡音量控制");
    if (g_crosshairEnabled) features.push_back(L"狙击准星");
    if (g_itemHelperEnabled) features.push_back(L"道具助手");
    if (IsRageModeEnabled()) features.push_back(L"超频配置");
    if (IsQuickStopEnabled()) features.push_back(L"自动急停");
    if (mousejitter::IsEnabled()) features.push_back(L"多绑定脚本");
    if (consolelog::IsEnabled()) features.push_back(L"控制台日志");

    for (const auto& script : vscript::MountedScripts()) {
        if (!script.continuous) continue;
        std::wstring name = script_display_name(script);
        if (!name.empty()) features.push_back(name);
    }

    return features;
}

int text_width(Gdiplus::Graphics& g, Gdiplus::Font& font, const std::wstring& text)
{
    Gdiplus::RectF bounds;
    g.MeasureString(text.c_str(), -1, &font, Gdiplus::PointF(0.f, 0.f), &bounds);
    return static_cast<int>(std::ceil(bounds.Width));
}

void draw_text(Gdiplus::Graphics& g, const std::wstring& text, Gdiplus::Font& font,
    float x, float y, Gdiplus::Brush& brush, BYTE alpha)
{
    Gdiplus::SolidBrush shadow(Gdiplus::Color(static_cast<BYTE>(alpha * 0.65f), 0, 0, 0));
    g.DrawString(text.c_str(), -1, &font, Gdiplus::PointF(x + 1.f, y + 1.f), &shadow);
    g.DrawString(text.c_str(), -1, &font, Gdiplus::PointF(x, y), &brush);
}

void redraw()
{
    if (!has_window() || !g_textguiEnabled) return;

    const int sw = GetSystemMetrics(SM_CXSCREEN);
    const int sh = GetSystemMetrics(SM_CYSCREEN);
    HDC hdcScreen = GetDC(nullptr);
    HDC hdcMem = CreateCompatibleDC(hdcScreen);

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = sw;
    bmi.bmiHeader.biHeight = -sh;
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

        const float scale = std::clamp(g_textguiScale, 0.75f, 1.8f);
        const float opacity = std::clamp(g_textguiOpacity, 0.2f, 1.0f);
        const BYTE alpha = static_cast<BYTE>(255.f * opacity);
        const Color color(alpha,
            static_cast<BYTE>(std::clamp(g_textguiR, 0, 255)),
            static_cast<BYTE>(std::clamp(g_textguiG, 0, 255)),
            static_cast<BYTE>(std::clamp(g_textguiB, 0, 255)));

        Font titleFont(L"Microsoft YaHei UI", 20.f * scale, FontStyleBold);
        Font itemFont(L"Microsoft YaHei UI", 13.5f * scale, FontStyleBold);
        SolidBrush textBrush(color);

        auto features = collect_enabled_features();
        std::sort(features.begin(), features.end(), [&](const auto& a, const auto& b) {
            return text_width(g, itemFont, a) > text_width(g, itemFont, b);
        });

        const float margin = 18.f * scale;
        const float rowH = 25.f * scale;
        const int maxFeatureW = features.empty() ? 0 : text_width(g, itemFont, features.front());
        const int maxTextW = (std::max)(text_width(g, titleFont, L"StrikeSense"), maxFeatureW);
        const float areaW = (std::max)(185.f * scale, maxTextW + 4.f * scale);
        const float areaH = (g_textguiShowWatermark ? 35.f * scale : 0.f)
            + static_cast<float>(features.size()) * rowH + 6.f * scale;
        const float x = std::clamp((sw - areaW - margin) * std::clamp(g_textguiX, 0.f, 1.f), margin, sw - areaW - margin);
        const float y = std::clamp((sh - areaH - margin) * std::clamp(g_textguiY, 0.f, 1.f), margin, sh - areaH - margin);

        float cy = y;
        if (g_textguiShowWatermark) {
            const std::wstring title = L"StrikeSense";
            draw_text(g, title, titleFont, x + areaW - text_width(g, titleFont, title), cy, textBrush, alpha);
            cy += 35.f * scale;
        }

        for (const auto& feature : features) {
            draw_text(g, feature, itemFont, x + areaW - text_width(g, itemFont, feature), cy, textBrush, alpha);
            cy += rowH;
        }
    }

    POINT dst{ 0, 0 };
    SIZE size{ sw, sh };
    POINT src{ 0, 0 };
    BLENDFUNCTION blend{};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;
    UpdateLayeredWindow(s_hwnd, hdcScreen, &dst, &size, hdcMem, &src, 0, &blend, ULW_ALPHA);

    SelectObject(hdcMem, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(hdcMem);
    ReleaseDC(nullptr, hdcScreen);
}

void update_visibility()
{
    if (!has_window() || !g_textguiEnabled) return;
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

    SetTimer(s_hwnd, kRefreshTimer, 350, nullptr);
    ShowWindow(s_hwnd, SW_HIDE);
    std::cout << "[Textgui] 覆盖层窗口已创建。" << std::endl;
}

void ApplyEnabled(bool enabled)
{
    g_textguiEnabled = enabled;
    if (enabled) {
        if (!has_window()) Initialize(s_hInst ? s_hInst : hInst);
        SetWindowPos(s_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        update_visibility();
        std::cout << "[Textgui] 已开启。" << std::endl;
        return;
    }

    if (has_window()) ShowWindow(s_hwnd, SW_HIDE);
    s_visible = false;
    std::cout << "[Textgui] 已关闭。" << std::endl;
}

void Refresh()
{
    if (!g_textguiEnabled) return;
    if (!has_window()) Initialize(s_hInst ? s_hInst : hInst);
    s_visible = true;
    ShowWindow(s_hwnd, SW_SHOWNOACTIVATE);
    redraw();
}

void Shutdown()
{
    if (has_window()) DestroyWindow(s_hwnd);
    s_hwnd = nullptr;
    s_visible = false;
    std::cout << "[Textgui] 已释放覆盖层。" << std::endl;
}

} // namespace textgui_overlay
