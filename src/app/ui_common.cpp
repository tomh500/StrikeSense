#include "pages.h"
#include "ui_theme.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <unordered_map>

namespace {

constexpr UINT kActiveFrameMs = 11;
constexpr UINT kIdleFrameMs = 1000;
constexpr DWORD kInteractionWindowMs = 220;
constexpr DWORD kFastClockHoldMs = 650;

DWORD g_lastInteractionMs = 0;
bool g_fastAnimationClock = false;
bool g_pointerPressed = false;
int g_pointerX = -10000;
int g_pointerY = -10000;
DWORD g_lastFrameMs = 0;
float g_frameDeltaMs = 16.0f;
bool g_frameAnimating = false;

struct anim_value {
    float current = 0.0f;
    float target = 0.0f;
};

std::unordered_map<std::uint64_t, anim_value> g_animationValues;

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

bool hit_rect(const Gdiplus::RectF& rect)
{
    return g_pointerX >= rect.X && g_pointerX <= rect.X + rect.Width
        && g_pointerY >= rect.Y && g_pointerY <= rect.Y + rect.Height;
}

bool is_basic_visual_theme()
{
    const int preset = uitheme::get_preset();
    return preset >= uitheme::preset_default && preset <= uitheme::preset_purple_song;
}

std::uint64_t anim_key(const Gdiplus::RectF& rect, std::uint32_t salt)
{
    const auto x = static_cast<std::uint64_t>(std::lround(rect.X * 2.0f)) & 0xffff;
    const auto y = static_cast<std::uint64_t>(std::lround(rect.Y * 2.0f)) & 0xffff;
    const auto w = static_cast<std::uint64_t>(std::lround(rect.Width * 2.0f)) & 0xffff;
    const auto h = static_cast<std::uint64_t>(std::lround(rect.Height * 2.0f)) & 0xffff;
    return (static_cast<std::uint64_t>(salt) << 48) ^ (x << 36) ^ (y << 24) ^ (w << 12) ^ h;
}

float animate_to(std::uint64_t key, float target, float durationMs)
{
    auto& value = g_animationValues[key];
    value.target = target;

    const DWORD now = GetTickCount();
    if (g_lastFrameMs == 0) g_lastFrameMs = now;
    const float dt = g_frameDeltaMs;
    const float step = 1.0f - std::pow(1.0f - (std::min)(1.0f, dt / durationMs), 3.0f);
    value.current += (value.target - value.current) * step;

    if (std::fabs(value.current - value.target) > 0.004f) g_frameAnimating = true;
    else value.current = value.target;
    return value.current;
}

Gdiplus::Color mix_color(Gdiplus::Color a, Gdiplus::Color b, float t)
{
    t = (std::max)(0.0f, (std::min)(1.0f, t));
    auto mix = [t](BYTE av, BYTE bv) {
        return static_cast<BYTE>(static_cast<float>(av) + (static_cast<float>(bv) - static_cast<float>(av)) * t);
    };
    return Gdiplus::Color(mix(a.GetA(), b.GetA()), mix(a.GetR(), b.GetR()),
        mix(a.GetG(), b.GetG()), mix(a.GetB(), b.GetB()));
}

Gdiplus::RectF inset_rect(Gdiplus::RectF rect, float inset)
{
    rect.X += inset;
    rect.Y += inset;
    rect.Width -= inset * 2.0f;
    rect.Height -= inset * 2.0f;
    return rect;
}

} // namespace

namespace ui {
using namespace Gdiplus;

void BeginFrame()
{
    const DWORD now = GetTickCount();
    if (g_lastFrameMs != 0) g_frameDeltaMs = static_cast<float>((std::min)(now - g_lastFrameMs, static_cast<DWORD>(40)));
    g_lastFrameMs = now;
    g_frameAnimating = false;
}

void SetPointerState(HWND hw, int mx, int my, bool pressed)
{
    g_pointerX = mx;
    g_pointerY = my;
    g_pointerPressed = pressed;
    if (is_basic_visual_theme()) NotifyInteraction(hw);
}

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
    if (!is_basic_visual_theme()) return;
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
    if (elapsed <= kInteractionWindowMs || g_frameAnimating) {
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
    SolidBrush tbCol(theme.title);
    Font pF(L"Microsoft YaHei", 13, FontStyleBold);
    if (!is_basic_visual_theme()) {
        SolidBrush hdrBg(theme.header_background);
        g.FillRectangle(&hdrBg, cx, 8, cw, 34);
        g.DrawString(title, -1, &pF, PointF(static_cast<REAL>(cx + 10), 14), &tbCol);
        return;
    }

    RectF rect(static_cast<REAL>(cx), 8.0f, static_cast<REAL>(cw), 34.0f);
    LinearGradientBrush hdr(rect, theme.header_background, theme.card_alt_background, LinearGradientModeVertical);
    g.FillRectangle(&hdr, rect);
    g.DrawString(title, -1, &pF, PointF(static_cast<REAL>(cx + 10), 14), &tbCol);
}

void DrawToggle(Graphics& g, int tx, int ty, bool state)
{
    const auto& theme = uitheme::get_palette();
    RectF tr(static_cast<REAL>(tx), static_cast<REAL>(ty), 50.f, 24.f);
    if (!is_basic_visual_theme()) {
        SolidBrush onBr(theme.accent);
        SolidBrush offBr(theme.toggle_off);
        SolidBrush kBr(theme.toggle_knob);
        GraphicsPath tp;
        tp.AddArc(tx, ty, 24, 24, 90, 180);
        tp.AddArc(tx + 26, ty, 24, 24, 270, 180);
        tp.CloseFigure();
        g.FillPath(state ? &onBr : &offBr, &tp);
        const float kx = state ? tx + 28.f : tx + 2.f;
        g.FillEllipse(&kBr, kx, static_cast<REAL>(ty + 2), 20.f, 20.f);
        return;
    }

    const bool hover = hit_rect(tr);
    const float hoverT = animate_to(anim_key(tr, 10), hover ? 1.0f : 0.0f, 160.0f);
    const float stateT = animate_to(anim_key(tr, 11), state ? 1.0f : 0.0f, 180.0f);
    const float pressT = animate_to(anim_key(tr, 12), (hover && g_pointerPressed) ? 1.0f : 0.0f, 120.0f);

    const Color trackTop = mix_color(theme.toggle_off, theme.accent_soft, stateT);
    const Color trackBottom = mix_color(theme.card_alt_background, theme.accent, stateT);
    LinearGradientBrush trackBrush(tr, mix_color(trackTop, theme.accent_soft, hoverT * 0.22f),
        mix_color(trackBottom, theme.accent_strong, hoverT * 0.22f), LinearGradientModeVertical);
    SolidBrush kBr(theme.toggle_knob);
    GraphicsPath tp;
    tp.AddArc(tx, ty, 24, 24, 90, 180);
    tp.AddArc(tx + 26, ty, 24, 24, 270, 180);
    tp.CloseFigure();
    if (stateT > 0.03f) {
        Pen glow(Color(static_cast<BYTE>(80 * stateT), theme.accent.GetR(), theme.accent.GetG(), theme.accent.GetB()), 3.0f);
        g.DrawPath(&glow, &tp);
    }
    g.FillPath(&trackBrush, &tp);
    const float knobSize = 20.0f + hoverT * 1.5f - pressT * 1.0f;
    const float kx = tx + 2.0f + 26.0f * stateT + (20.0f - knobSize) / 2.0f;
    const float ky = static_cast<float>(ty + 2) + (20.0f - knobSize) / 2.0f;
    g.FillEllipse(&kBr, kx, ky, knobSize, knobSize);
}

void DrawSlider(Graphics& g, int sx, int sy, int sw, float value)
{
    const auto& theme = uitheme::get_palette();
    value = (std::max)(0.0f, (std::min)(1.0f, value));
    if (!is_basic_visual_theme()) {
        SolidBrush sBg(theme.slider_background);
        SolidBrush sFill(theme.accent);
        g.FillRectangle(&sBg, sx, sy, sw, 10);
        int fw = static_cast<int>(sw * value);
        if (fw > sw) fw = sw;
        g.FillRectangle(&sFill, sx, sy, fw, 10);
        return;
    }

    RectF hit(static_cast<REAL>(sx), static_cast<REAL>(sy - 7), static_cast<REAL>(sw), 22.0f);
    const float hoverT = animate_to(anim_key(hit, 20), hit_rect(hit) ? 1.0f : 0.0f, 150.0f);
    RectF track(static_cast<REAL>(sx), static_cast<REAL>(sy + 3), static_cast<REAL>(sw), 4.0f);
    LinearGradientBrush sBg(track, theme.slider_background, theme.card_alt_background, LinearGradientModeHorizontal);
    GraphicsPath trackPath;
    add_rounded_rect(trackPath, track, 2.0f);
    g.FillPath(&sBg, &trackPath);

    const REAL fillWidth = (std::max)(4.0f, static_cast<REAL>(sw) * value);
    RectF fill(track.X, track.Y, (std::min)(track.Width, fillWidth), track.Height);
    LinearGradientBrush sFill(fill, theme.accent, mix_color(theme.accent_soft, theme.toggle_knob, hoverT * 0.18f),
        LinearGradientModeHorizontal);
    GraphicsPath fillPath;
    add_rounded_rect(fillPath, fill, 2.0f);
    g.FillPath(&sFill, &fillPath);
}

void DrawSliderWithKnob(Graphics& g, int sx, int sy, int sw, float value)
{
    value = (std::max)(0.0f, (std::min)(1.0f, value));
    DrawSlider(g, sx, sy, sw, value);
    const auto& theme = uitheme::get_palette();
    if (!is_basic_visual_theme()) {
        SolidBrush knob(theme.accent_strong);
        g.FillEllipse(&knob, sx + sw * value - 7.0f, sy - 5.0f, 14.0f, 14.0f);
        return;
    }

    RectF hit(static_cast<REAL>(sx), static_cast<REAL>(sy - 7), static_cast<REAL>(sw), 22.0f);
    const float hoverT = animate_to(anim_key(hit, 21), hit_rect(hit) ? 1.0f : 0.0f, 150.0f);
    const float size = 14.0f + hoverT * 2.0f;
    const float x = sx + sw * value - size / 2.0f;
    const float y = sy - 5.0f - hoverT;
    if (hoverT > 0.02f) {
        SolidBrush ring(Color(static_cast<BYTE>(70 * hoverT), theme.accent_soft.GetR(), theme.accent_soft.GetG(), theme.accent_soft.GetB()));
        g.FillEllipse(&ring, x - 3.0f, y - 3.0f, size + 6.0f, size + 6.0f);
    }
    LinearGradientBrush knob(RectF(x, y, size, size), theme.accent_soft, theme.accent_strong, LinearGradientModeVertical);
    g.FillEllipse(&knob, x, y, size, size);
}

void DrawRoundedButton(Graphics& g, const RectF& rect, const wchar_t* label, bool selected, bool compact)
{
    const auto& theme = uitheme::get_palette();
    if (!is_basic_visual_theme()) {
        SolidBrush background(selected ? theme.card_selected_background : theme.button_background);
        SolidBrush foreground(theme.button_text);
        Pen border(selected ? theme.accent_strong : theme.button_border, selected ? 1.5f : 1.0f);
        Font font(L"Microsoft YaHei", compact ? 8.0f : 9.0f, FontStyleBold);
        StringFormat format;
        format.SetAlignment(StringAlignmentCenter);
        format.SetLineAlignment(StringAlignmentCenter);
        GraphicsPath path;
        const REAL diameter = (std::min)(16.0f, rect.Height);
        path.AddArc(rect.X, rect.Y, diameter, diameter, 180.0f, 90.0f);
        path.AddArc(rect.X + rect.Width - diameter, rect.Y, diameter, diameter, 270.0f, 90.0f);
        path.AddArc(rect.X + rect.Width - diameter, rect.Y + rect.Height - diameter,
            diameter, diameter, 0.0f, 90.0f);
        path.AddArc(rect.X, rect.Y + rect.Height - diameter, diameter, diameter, 90.0f, 90.0f);
        path.CloseFigure();
        g.FillPath(&background, &path);
        g.DrawPath(&border, &path);
        g.DrawString(label, -1, &font, rect, &format, &foreground);
        return;
    }

    const bool hover = hit_rect(rect);
    const float hoverT = animate_to(anim_key(rect, 30), hover ? 1.0f : 0.0f, 160.0f);
    const float pressT = animate_to(anim_key(rect, 31), (hover && g_pointerPressed) ? 1.0f : 0.0f, 120.0f);
    RectF drawRect = rect;
    drawRect.Y -= hoverT * 1.5f;
    drawRect = inset_rect(drawRect, pressT * 1.0f);
    const Color top = selected ? theme.accent_soft : theme.button_background;
    const Color bottom = selected ? theme.accent : theme.card_alt_background;
    LinearGradientBrush background(drawRect, mix_color(top, theme.accent_soft, hoverT * 0.28f),
        mix_color(bottom, theme.accent_strong, pressT * 0.25f), LinearGradientModeVertical);
    SolidBrush foreground(theme.button_text);
    Pen border(mix_color(theme.button_border, theme.accent_soft, hoverT * 0.35f), selected ? 1.5f : 1.0f);
    Font font(L"Microsoft YaHei", compact ? 8.0f : 9.0f, FontStyleBold);
    StringFormat format;
    format.SetAlignment(StringAlignmentCenter);
    format.SetLineAlignment(StringAlignmentCenter);
    GraphicsPath path;
    add_rounded_rect(path, drawRect, 12.0f);
    if (hoverT > 0.02f || selected) {
        SolidBrush glow(Color(static_cast<BYTE>((selected ? 38 : 22) + hoverT * 28),
            theme.accent.GetR(), theme.accent.GetG(), theme.accent.GetB()));
        RectF glowRect = drawRect;
        glowRect.Y += 2.0f;
        GraphicsPath glowPath;
        add_rounded_rect(glowPath, glowRect, 12.0f);
        g.FillPath(&glow, &glowPath);
    }
    g.FillPath(&background, &path);
    g.DrawPath(&border, &path);
    g.DrawString(label, -1, &font, drawRect, &format, &foreground);
}

void DrawFoldButton(Graphics& g, const RectF& rect, bool expanded)
{
    DrawRoundedButton(g, rect, expanded ? L"v" : L">", false, true);
}

void DrawNavigationButton(Graphics& g, const RectF& rect, const wchar_t* label, bool selected)
{
    const auto& theme = uitheme::get_palette();
    if (!is_basic_visual_theme()) {
        SolidBrush background(theme.sidebar_selected_background);
        SolidBrush text(selected ? theme.title : theme.text);
        Font font(L"Microsoft YaHei", 12, selected ? FontStyleBold : FontStyleRegular);
        if (selected) g.FillRectangle(&background, rect);
        g.DrawString(label, -1, &font, PointF(rect.X + 6.0f, rect.Y + 3.0f), &text);
        return;
    }

    const bool hover = hit_rect(rect);
    const float hoverT = animate_to(anim_key(rect, 40), hover ? 1.0f : 0.0f, 160.0f);
    const float selectedT = animate_to(anim_key(rect, 41), selected ? 1.0f : 0.0f, 180.0f);
    const float slide = hoverT * 2.0f;

    RectF bgRect(rect.X + slide, rect.Y, rect.Width - slide, rect.Height);
    GraphicsPath bgPath;
    add_rounded_rect(bgPath, bgRect, 10.0f);
    const Color top = mix_color(theme.sidebar_background, theme.sidebar_selected_background, (std::max)(hoverT * 0.45f, selectedT));
    const Color bottom = mix_color(theme.card_alt_background, theme.accent_strong, selectedT * 0.26f + hoverT * 0.08f);
    LinearGradientBrush bg(bgRect, top, bottom, LinearGradientModeHorizontal);
    if (hoverT > 0.02f || selectedT > 0.02f) g.FillPath(&bg, &bgPath);

    RectF indicator(rect.X, rect.Y + 4.0f, 3.0f, rect.Height - 8.0f);
    SolidBrush accent(Color(static_cast<BYTE>(210 * selectedT + 90 * hoverT),
        theme.accent.GetR(), theme.accent.GetG(), theme.accent.GetB()));
    GraphicsPath indicatorPath;
    add_rounded_rect(indicatorPath, indicator, 2.0f);
    if (selectedT > 0.02f || hoverT > 0.02f) g.FillPath(&accent, &indicatorPath);

    Font font(L"Microsoft YaHei", 12, selected ? FontStyleBold : FontStyleRegular);
    SolidBrush text(mix_color(theme.text, theme.title, selectedT));
    g.DrawString(label, -1, &font, PointF(rect.X + 12.0f + slide, rect.Y + 3.0f), &text);
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
