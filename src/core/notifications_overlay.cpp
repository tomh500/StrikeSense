#include "notifications_overlay.h"

#include "StrikeSense.h"
#include "volume_mixer.h"

#include <algorithm>
#include <cmath>
#include <gdiplus.h>
#include <iostream>

namespace notifications_overlay {
namespace {

HWND s_hwnd = nullptr;
HINSTANCE s_hInst = nullptr;
std::wstring s_text;
bool s_enabledState = true;
ULONGLONG s_startTick = 0;
bool s_visible = false;
constexpr UINT_PTR kTimer = 3021;
constexpr int kAnimMs = 260;

bool has_window()
{
    return s_hwnd && IsWindow(s_hwnd);
}

float ease_out(float t)
{
    t = std::clamp(t, 0.f, 1.f);
    return 1.f - std::pow(1.f - t, 3.f);
}

void hide()
{
    if (!has_window() || !s_visible) return;
    ShowWindow(s_hwnd, SW_HIDE);
    s_visible = false;
}

void draw()
{
    if (!has_window() || !g_notificationsEnabled || s_text.empty() || !IsCS2WindowActive()) {
        hide();
        return;
    }

    const ULONGLONG now = GetTickCount64();
    const float durationMs = std::clamp(g_notificationsDuration, 1.f, 5.f) * 1000.f;
    const float elapsed = static_cast<float>(now - s_startTick);
    if (durationMs <= 1.f || elapsed >= durationMs) {
        s_text.clear();
        hide();
        return;
    }

    const int sw = GetSystemMetrics(SM_CXSCREEN);
    const int sh = GetSystemMetrics(SM_CYSCREEN);
    const int width = 310;
    const int height = 72;
    const float progress = std::clamp(1.f - elapsed / durationMs, 0.f, 1.f);
    const float in = ease_out(elapsed / static_cast<float>(kAnimMs));
    const float outStart = (std::max)(0.f, durationMs - kAnimMs);
    const float out = elapsed > outStart ? ease_out((elapsed - outStart) / static_cast<float>(kAnimMs)) : 0.f;
    const float slide = (1.f - in + out) * 360.f;

    const int x = sw - width - 26 + static_cast<int>(slide);
    const int y = sh - height - 42;

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

    using namespace Gdiplus;
    Graphics g(hdcMem);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    g.Clear(Color(0, 0, 0, 0));

    const int style = std::clamp(g_notificationsStyle, 0, 2);
    Color bg = style == 1 ? Color(235, 32, 34, 38) : (style == 0 ? Color(225, 18, 20, 26) : Color(235, 24, 28, 36));
    Color border = style == 0 ? Color(210, 80, 180, 255) : (style == 1 ? Color(140, 0, 0, 0) : Color(180, 130, 170, 255));
    Color bar = s_enabledState ? Color(255, 82, 180, 95) : Color(255, 230, 80, 80);
    Color title = Color(255, 255, 255, 255);
    Color sub = Color(220, 210, 220, 235);
    if (style == 2) {
        bg = Color(235, 15, 18, 25);
        border = Color(180, 90, 170, 255);
        bar = s_enabledState ? Color(255, 74, 222, 150) : Color(255, 255, 95, 95);
    }

    RectF box(static_cast<REAL>(x), static_cast<REAL>(y), static_cast<REAL>(width), static_cast<REAL>(height));
    SolidBrush bgBrush(bg);
    Pen borderPen(border, style == 2 ? 1.5f : 1.f);
    GraphicsPath path;
    const float r = style == 1 ? 4.f : 8.f;
    path.AddArc(box.X, box.Y, r * 2.f, r * 2.f, 180.f, 90.f);
    path.AddArc(box.X + box.Width - r * 2.f, box.Y, r * 2.f, r * 2.f, 270.f, 90.f);
    path.AddArc(box.X + box.Width - r * 2.f, box.Y + box.Height - r * 2.f, r * 2.f, r * 2.f, 0.f, 90.f);
    path.AddArc(box.X, box.Y + box.Height - r * 2.f, r * 2.f, r * 2.f, 90.f, 90.f);
    path.CloseFigure();
    g.FillPath(&bgBrush, &path);
    g.DrawPath(&borderPen, &path);

    Font titleFont(L"Microsoft YaHei UI", 13.f, FontStyleBold);
    Font subFont(L"Microsoft YaHei UI", 10.f, FontStyleRegular);
    SolidBrush titleBrush(title);
    SolidBrush subBrush(sub);
    g.DrawString(L"StrikeSense", -1, &titleFont, PointF(box.X + 16.f, box.Y + 11.f), &titleBrush);
    g.DrawString(s_text.c_str(), -1, &subFont, PointF(box.X + 16.f, box.Y + 36.f), &subBrush);

    const float barHeight = style == 0 ? 3.f : 4.f;
    RectF barBack(box.X, box.Y + box.Height - barHeight, box.Width, barHeight);
    SolidBrush backBrush(Color(120, 0, 0, 0));
    SolidBrush barBrush(bar);
    g.FillRectangle(&backBrush, barBack);
    g.FillRectangle(&barBrush, RectF(barBack.X, barBack.Y, barBack.Width * progress, barBack.Height));

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

    if (!s_visible) {
        ShowWindow(s_hwnd, SW_SHOWNOACTIVATE);
        s_visible = true;
    }
}

LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_TIMER && wp == kTimer) {
        draw();
        return 0;
    }
    if (msg == WM_DESTROY) {
        KillTimer(hwnd, kTimer);
        s_hwnd = nullptr;
        s_visible = false;
        return 0;
    }
    if (msg == WM_CLOSE) {
        DestroyWindow(hwnd);
        return 0;
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
    wc.lpszClassName = L"StrikeSense_Notifications";
    RegisterClassExW(&wc);

    s_hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT
        | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        wc.lpszClassName, L"", WS_POPUP, 0, 0,
        GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
        nullptr, nullptr, hInst, nullptr);
    if (!s_hwnd) {
        std::cout << "[Notifications] 创建覆盖层失败。" << std::endl;
        return;
    }
    SetTimer(s_hwnd, kTimer, 16, nullptr);
    ShowWindow(s_hwnd, SW_HIDE);
    std::cout << "[Notifications] 覆盖层已创建。" << std::endl;
}

void Shutdown()
{
    if (has_window()) DestroyWindow(s_hwnd);
    s_hwnd = nullptr;
    s_visible = false;
}

void Push(const std::wstring& text, bool enabled)
{
    if (!g_notificationsEnabled || text.empty()) return;
    if (!has_window()) Initialize(s_hInst ? s_hInst : hInst);
    s_text = text + (enabled ? L" Enabled" : L" Disabled");
    s_enabledState = enabled;
    s_startTick = GetTickCount64();
    draw();
}

void Refresh()
{
    draw();
}

} // namespace notifications_overlay
