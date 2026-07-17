#include "pages.h"
#include "ui_theme.h"

#include <algorithm>
#include <iostream>

namespace {

constexpr UINT kActiveFrameMs = 11;
constexpr UINT kIdleFrameMs = 1000;
constexpr DWORD kInteractionWindowMs = 220;
constexpr DWORD kFastClockHoldMs = 650;

DWORD g_lastInteractionMs = 0;
bool g_fastAnimationClock = false;

void add_rounded_rect(Gdiplus::GraphicsPath& path, const Gdiplus::RectF& rect, Gdiplus::REAL radius)
{
    using namespace Gdiplus;
    const REAL diameter = (std::min)(radius * 2.0f, (std::min)(rect.Width, rect.Height));
    path.AddArc(rect.X, rect.Y, diameter, diameter, 180.0f, 90.0f);
    path.AddArc(rect.X + rect.Width - diameter, rect.Y, diameter, diameter, 270.0f, 90.0f);
    path.AddArc(rect.X + rect.Width - diameter, rect.Y + rect.Height - diameter,
        diameter, diameter, 0.0f, 90.0f);
    path.AddArc(rect.X, rect.Y + rect.Height - diameter, diameter, diameter, 90.0f, 90.0f);
    path.CloseFigure();
}

} // namespace

namespace ui {
using namespace Gdiplus;

void StartAnimationClock(HWND hw)
{
    g_lastInteractionMs = GetTickCount();
    g_fastAnimationClock = false;
    SetTimer(hw, kAnimationTimerId, kIdleFrameMs, nullptr);
    std::cout << "[界面动画] 已启动空闲刷新时钟。" << std::endl;
}

void StopAnimationClock(HWND hw)
{
    KillTimer(hw, kAnimationTimerId);
    g_fastAnimationClock = false;
    std::cout << "[界面动画] 已停止刷新时钟。" << std::endl;
}

void NotifyInteraction(HWND hw)
{
    g_lastInteractionMs = GetTickCount();
    if (!g_fastAnimationClock) {
        SetTimer(hw, kAnimationTimerId, kActiveFrameMs, nullptr);
        g_fastAnimationClock = true;
        std::cout << "[界面动画] 检测到交互，切换到 90fps 刷新。" << std::endl;
    }
}

bool TickAnimation(HWND hw)
{
    const DWORD now = GetTickCount();
    const DWORD elapsed = now - g_lastInteractionMs;
    if (elapsed <= kInteractionWindowMs) {
        InvalidateRect(hw, nullptr, FALSE);
        return true;
    }

    if (g_fastAnimationClock && elapsed > kFastClockHoldMs) {
        SetTimer(hw, kAnimationTimerId, kIdleFrameMs, nullptr);
        g_fastAnimationClock = false;
        std::cout << "[界面动画] 交互结束，降到 1fps 空闲刷新。" << std::endl;
    }
    return false;
}

float EaseOutCubic(float value)
{
    value = (std::max)(0.0f, (std::min)(1.0f, value));
    const float inverse = 1.0f - value;
    return 1.0f - inverse * inverse * inverse;
}

void DrawHeader(Graphics& g, int cx, int cw, const wchar_t* title)
{
    const auto& theme = uitheme::get_palette();
    SolidBrush hdrBg(theme.header_background);
    SolidBrush tbCol(theme.title);
    Font pF(L"Microsoft YaHei", 13, FontStyleBold);
    g.FillRectangle(&hdrBg, cx, 8, cw, 34);
    g.DrawString(title, -1, &pF, PointF(static_cast<REAL>(cx + 10), 14), &tbCol);
}

void DrawToggle(Graphics& g, int tx, int ty, bool state)
{
    const auto& theme = uitheme::get_palette();
    SolidBrush onBr(theme.accent);
    SolidBrush offBr(theme.toggle_off);
    SolidBrush kBr(theme.toggle_knob);
    RectF tr(static_cast<REAL>(tx), static_cast<REAL>(ty), 50.f, 24.f);
    GraphicsPath tp;
    tp.AddArc(tx, ty, 24, 24, 90, 180);
    tp.AddArc(tx + 26, ty, 24, 24, 270, 180);
    tp.CloseFigure();
    g.FillPath(state ? &onBr : &offBr, &tp);
    const float kx = state ? tx + 28.f : tx + 2.f;
    g.FillEllipse(&kBr, kx, static_cast<REAL>(ty + 2), 20.f, 20.f);
}

void DrawSlider(Graphics& g, int sx, int sy, int sw, float value)
{
    const auto& theme = uitheme::get_palette();
    SolidBrush sBg(theme.slider_background);
    SolidBrush sFill(theme.accent);
    value = (std::max)(0.0f, (std::min)(1.0f, value));
    RectF track(static_cast<REAL>(sx), static_cast<REAL>(sy + 3), static_cast<REAL>(sw), 4.0f);
    GraphicsPath trackPath;
    add_rounded_rect(trackPath, track, 2.0f);
    g.FillPath(&sBg, &trackPath);

    const REAL fillWidth = (std::max)(4.0f, static_cast<REAL>(sw) * value);
    RectF fill(track.X, track.Y, (std::min)(track.Width, fillWidth), track.Height);
    GraphicsPath fillPath;
    add_rounded_rect(fillPath, fill, 2.0f);
    g.FillPath(&sFill, &fillPath);
}

void DrawSliderWithKnob(Graphics& g, int sx, int sy, int sw, float value)
{
    value = (std::max)(0.0f, (std::min)(1.0f, value));
    DrawSlider(g, sx, sy, sw, value);
    const auto& theme = uitheme::get_palette();
    SolidBrush knob(theme.accent);
    g.FillEllipse(&knob, sx + sw * value - 7.0f, sy - 5.0f, 14.0f, 14.0f);
}

void DrawRoundedButton(Graphics& g, const RectF& rect, const wchar_t* label, bool selected, bool compact)
{
    const auto& theme = uitheme::get_palette();
    SolidBrush background(selected ? theme.card_selected_background : theme.button_background);
    SolidBrush foreground(theme.button_text);
    Pen border(selected ? theme.accent_strong : theme.button_border, selected ? 1.5f : 0.0f);
    Font font(L"Microsoft YaHei", compact ? 8.0f : 9.0f, FontStyleBold);
    StringFormat format;
    format.SetAlignment(StringAlignmentCenter);
    format.SetLineAlignment(StringAlignmentCenter);
    GraphicsPath path;
    add_rounded_rect(path, rect, 12.0f);
    g.FillPath(&background, &path);
    if (selected) g.DrawPath(&border, &path);
    g.DrawString(label, -1, &font, rect, &format, &foreground);
}

void DrawFoldButton(Graphics& g, const RectF& rect, bool expanded)
{
    DrawRoundedButton(g, rect, expanded ? L"v" : L">", false, true);
}

bool CheckToggleClick(int mx, int my, int tx, int ty)
{
    return mx >= tx && mx <= tx + 50 && my >= ty && my <= ty + 24;
}

bool CheckSliderClick(int mx, int my, int sx, int sy, int sw, float& outVal)
{
    if (mx >= sx && mx <= sx + sw && my >= sy - 8 && my <= sy + 12) {
        float t = static_cast<float>(mx - sx) / static_cast<float>(sw);
        if (t < 0) t = 0;
        if (t > 1) t = 1;
        outVal = t;
        return true;
    }
    return false;
}

} // namespace ui
