#include "textgui_overlay.h"

#include "pages.h"
#include "StrikeSense.h"
#include "quickstop.h"
#include "mouse_jitter.h"
#include "console_log.h"
#include "volume_mixer.h"

#include <algorithm>
#include <cmath>
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

std::vector<std::wstring> collect_enabled_features()
{
    std::vector<std::wstring> features;
    if (g_deathMute) features.push_back(L"CS2 Volume");
    if (g_crosshairEnabled) features.push_back(L"Crosshair");
    if (g_itemHelperEnabled) features.push_back(L"ItemHelper");
    if (IsRageModeEnabled()) features.push_back(L"Rage");
    if (IsQuickStopEnabled()) features.push_back(L"QuickStop");
    if (mousejitter::IsEnabled()) features.push_back(L"MouseJitter");
    if (consolelog::IsEnabled()) features.push_back(L"ConsoleLog");
    return features;
}

int text_width(Gdiplus::Graphics& g, Gdiplus::Font& font, const std::wstring& text)
{
    Gdiplus::RectF bounds;
    g.MeasureString(text.c_str(), -1, &font, Gdiplus::PointF(0.f, 0.f), &bounds);
    return static_cast<int>(std::ceil(bounds.Width));
}

void fill_round_rect(Gdiplus::Graphics& g, Gdiplus::Brush& brush,
    float x, float y, float w, float h, float r)
{
    Gdiplus::GraphicsPath path;
    path.AddArc(x, y, r, r, 180.f, 90.f);
    path.AddArc(x + w - r, y, r, r, 270.f, 90.f);
    path.AddArc(x + w - r, y + h - r, r, r, 0.f, 90.f);
    path.AddArc(x, y + h - r, r, r, 90.f, 90.f);
    path.CloseFigure();
    g.FillPath(&brush, &path);
}

void draw_text_shadow(Gdiplus::Graphics& g, const std::wstring& text, Gdiplus::Font& font,
    float x, float y, Gdiplus::Brush& brush, BYTE alpha)
{
    Gdiplus::SolidBrush shadow(Gdiplus::Color((std::max)(40, static_cast<int>(alpha * 0.55f)), 0, 0, 0));
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
        const Color accent(alpha, static_cast<BYTE>(std::clamp(g_textguiR, 0, 255)),
            static_cast<BYTE>(std::clamp(g_textguiG, 0, 255)),
            static_cast<BYTE>(std::clamp(g_textguiB, 0, 255)));

        Font titleFont(L"Segoe UI", 18.f * scale, FontStyleBold);
        Font itemFont(L"Segoe UI", 13.f * scale, FontStyleBold);
        auto features = collect_enabled_features();
        std::sort(features.begin(), features.end(), [&](const auto& a, const auto& b) {
            return text_width(g, itemFont, a) > text_width(g, itemFont, b);
        });

        const float margin = 18.f * scale;
        const float rowH = 25.f * scale;
        const int maxTextW = (std::max)(text_width(g, titleFont, L"StrikeSense"),
            features.empty() ? text_width(g, itemFont, L"No modules enabled") : text_width(g, itemFont, features.front()));
        const float panelW = (std::max)(190.f * scale, maxTextW + 44.f * scale);
        const float panelH = (g_textguiShowWatermark ? 42.f * scale : 0.f)
            + (std::max)(1, static_cast<int>(features.size())) * rowH + 18.f * scale;
        const float x = std::clamp((sw - panelW - margin) * std::clamp(g_textguiX, 0.f, 1.f), margin, sw - panelW - margin);
        const float y = std::clamp((sh - panelH - margin) * std::clamp(g_textguiY, 0.f, 1.f), margin, sh - panelH - margin);

        SolidBrush bg(Color(static_cast<BYTE>(150.f * opacity), 8, 10, 14));
        SolidBrush accentBrush(accent);
        SolidBrush white(Color(alpha, 245, 248, 255));
        SolidBrush dim(Color(static_cast<BYTE>(190.f * opacity), 170, 182, 198));
        Pen border(accent, 1.4f * scale);

        fill_round_rect(g, bg, x, y, panelW, panelH, 8.f * scale);
        g.DrawRectangle(&border, x, y, panelW, panelH);
        g.FillRectangle(&accentBrush, x, y, panelW, 3.5f * scale);

        float cy = y + 12.f * scale;
        if (g_textguiShowWatermark) {
            draw_text_shadow(g, L"StrikeSense", titleFont, x + 15.f * scale, cy, white, alpha);
            cy += 35.f * scale;
        }

        if (features.empty()) {
            draw_text_shadow(g, L"No modules enabled", itemFont, x + 15.f * scale, cy, dim, alpha);
        }
        else {
            for (const auto& feature : features) {
                const int w = text_width(g, itemFont, feature);
                const float tx = x + panelW - w - 16.f * scale;
                g.FillRectangle(&accentBrush, x + panelW - 5.f * scale, cy + 3.f * scale, 3.f * scale, 16.f * scale);
                draw_text_shadow(g, feature, itemFont, tx, cy, white, alpha);
                cy += rowH;
            }
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
    const bool shouldShow = IsCS2WindowActive();
    if (shouldShow == s_visible) {
        if (shouldShow) redraw();
        return;
    }

    s_visible = shouldShow;
    ShowWindow(s_hwnd, shouldShow ? SW_SHOWNOACTIVATE : SW_HIDE);
    if (shouldShow) redraw();
    std::cout << "[Textgui] CS2窗口状态变化，覆盖层已" << (shouldShow ? "显示" : "隐藏") << std::endl;
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
    if (s_visible || IsCS2WindowActive()) {
        s_visible = true;
        ShowWindow(s_hwnd, SW_SHOWNOACTIVATE);
        redraw();
    }
}

void Shutdown()
{
    if (has_window()) DestroyWindow(s_hwnd);
    s_hwnd = nullptr;
    s_visible = false;
    std::cout << "[Textgui] 已释放覆盖层。" << std::endl;
}

} // namespace textgui_overlay
