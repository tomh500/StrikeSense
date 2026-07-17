#include "notifications_overlay.h"

#include "StrikeSense.h"
#include "volume_mixer.h"

#include <algorithm>
#include <cmath>
#include <vector>
#include <gdiplus.h>
#include <iostream>
#include <mutex>

namespace notifications_overlay {
namespace {

HWND s_hwnd = nullptr;
HINSTANCE s_hInst = nullptr;
std::wstring s_text;
bool s_enabledState = true;
ULONGLONG s_startTick = 0;
bool s_visible = false;
HDC s_hdcMem = nullptr;
HBITMAP s_bitmap = nullptr;
HBITMAP s_oldBitmap = nullptr;
void* s_bits = nullptr;
int s_bufferWidth = 0;
int s_bufferHeight = 0;
std::wstring s_cachedText;
bool s_cachedEnabledState = true;
int s_cachedStyle = -1;
int s_cachedProgressBucket = -1;
int s_cachedWidth = 0;
int s_cachedHeight = 0;
bool s_cacheDirty = true;
constexpr UINT_PTR kTimer = 3021;
constexpr UINT kPushMessage = WM_APP + 3021;
constexpr UINT kFrameMs = 1000 / 90;
constexpr int kAnimMs = 260;
constexpr int kGptEntryMs = 220;
constexpr int kGptExitMs = 190;
constexpr int kGeminiEntryMs = 400;
constexpr int kGeminiExitMs = 250;
constexpr int kDeepSeekEntryMs = 240;
constexpr int kDeepSeekExitMs = 250;
constexpr int kDeepSeekIconPopMs = 150;
constexpr int kLiquidBounceKnobAnimMs = 300;
std::mutex s_pendingMutex;
std::wstring s_pendingText;
bool s_pendingEnabledState = true;

struct notification_item {
    std::wstring text;
    bool enabled = true;
    ULONGLONG startTick = 0;
    ULONGLONG exitTick = 0;
    std::wstring replacementText;
    bool replacementEnabled = true;
    bool hasReplacement = false;
    float y = 0.0f;
    bool yInitialized = false;
};

std::vector<notification_item> s_items;
std::vector<notification_item> s_waitingItems;

bool is_list_style(int style)
{
    return style == 0;
}

float list_slot_y(int slot, int height, int gap, int pad)
{
    return static_cast<float>(pad + (2 - slot) * (height + gap));
}

void append_list_item(notification_item item, int height, int gap, int pad)
{
    const int slot = static_cast<int>((std::min<std::size_t>)(s_items.size(), 2));
    const float targetY = list_slot_y(slot, height, gap, pad);
    item.y = targetY - 22.0f;
    item.yInitialized = true;
    s_items.push_back(std::move(item));
}

void push_list_item(notification_item item, ULONGLONG now, int height, int gap, int pad)
{
    for (auto& active : s_items) {
        if (active.text != item.text) continue;
        if (active.enabled == item.enabled && active.exitTick == 0) {
            active.startTick = now;
            return;
        }
        active.replacementText = std::move(item.text);
        active.replacementEnabled = item.enabled;
        active.hasReplacement = true;
        if (active.exitTick == 0) active.exitTick = now;
        return;
    }

    for (auto& waiting : s_waitingItems) {
        if (waiting.text == item.text) {
            waiting.enabled = item.enabled;
            return;
        }
    }

    if (s_items.size() < 3) {
        append_list_item(std::move(item), height, gap, pad);
    } else {
        if (s_items.front().exitTick == 0) s_items.front().exitTick = now;
        s_waitingItems.push_back(std::move(item));
    }
}

bool has_window()
{
    return s_hwnd && IsWindow(s_hwnd);
}

float ease_out(float t)
{
    t = std::clamp(t, 0.f, 1.f);
    return 1.f - std::pow(1.f - t, 3.f);
}

float ease_bounce(float t)
{
    t = std::clamp(t, 0.f, 1.f);
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.f;
    return 1.f + c3 * std::pow(t - 1.f, 3.f) + c1 * std::pow(t - 1.f, 2.f);
}

float ease_in_quint(float t)
{
    t = std::clamp(t, 0.f, 1.f);
    return t * t * t * t * t;
}

float ease_in_cubic(float t)
{
    t = std::clamp(t, 0.f, 1.f);
    return t * t * t;
}

BYTE alpha_byte(float alpha)
{
    return static_cast<BYTE>(std::lround(std::clamp(alpha, 0.f, 255.f)));
}

Gdiplus::Color with_alpha(const Gdiplus::Color& color, float alpha)
{
    return Gdiplus::Color(alpha_byte(alpha), color.GetR(), color.GetG(), color.GetB());
}

void add_rounded_rect(Gdiplus::GraphicsPath& path, const Gdiplus::RectF& rect, float radius)
{
    path.AddArc(rect.X, rect.Y, radius * 2.f, radius * 2.f, 180.f, 90.f);
    path.AddArc(rect.X + rect.Width - radius * 2.f, rect.Y, radius * 2.f, radius * 2.f, 270.f, 90.f);
    path.AddArc(rect.X + rect.Width - radius * 2.f, rect.Y + rect.Height - radius * 2.f, radius * 2.f, radius * 2.f, 0.f, 90.f);
    path.AddArc(rect.X, rect.Y + rect.Height - radius * 2.f, radius * 2.f, radius * 2.f, 90.f, 90.f);
    path.CloseFigure();
}

Gdiplus::Font& font_yahei_15_bold()
{
    static Gdiplus::Font font(L"Microsoft YaHei UI", 15.f, Gdiplus::FontStyleBold);
    return font;
}

Gdiplus::Font& font_yahei_14_5_bold()
{
    static Gdiplus::Font font(L"Microsoft YaHei UI", 14.5f, Gdiplus::FontStyleBold);
    return font;
}

Gdiplus::Font& font_yahei_13_5_bold()
{
    static Gdiplus::Font font(L"Microsoft YaHei UI", 13.5f, Gdiplus::FontStyleBold);
    return font;
}

Gdiplus::Font& font_yahei_13_5_regular()
{
    static Gdiplus::Font font(L"Microsoft YaHei UI", 13.5f, Gdiplus::FontStyleRegular);
    return font;
}

Gdiplus::Font& font_yahei_13_bold()
{
    static Gdiplus::Font font(L"Microsoft YaHei UI", 13.f, Gdiplus::FontStyleBold);
    return font;
}

Gdiplus::Font& font_yahei_10_5_bold()
{
    static Gdiplus::Font font(L"Microsoft YaHei UI", 10.5f, Gdiplus::FontStyleBold);
    return font;
}

Gdiplus::Font& font_yahei_10_regular()
{
    static Gdiplus::Font font(L"Microsoft YaHei UI", 10.f, Gdiplus::FontStyleRegular);
    return font;
}

Gdiplus::Font& font_yahei_9_regular()
{
    static Gdiplus::Font font(L"Microsoft YaHei UI", 9.f, Gdiplus::FontStyleRegular);
    return font;
}

Gdiplus::Font& font_yahei_9_bold()
{
    static Gdiplus::Font font(L"Microsoft YaHei UI", 9.f, Gdiplus::FontStyleBold);
    return font;
}

Gdiplus::Font& font_segoe_9_5_regular()
{
    static Gdiplus::Font font(L"Segoe UI", 9.5f, Gdiplus::FontStyleRegular);
    return font;
}

Gdiplus::Font& font_segoe_14_bold()
{
    static Gdiplus::Font font(L"Segoe UI", 14.f, Gdiplus::FontStyleBold);
    return font;
}

Gdiplus::Font& font_segoe_11_regular()
{
    static Gdiplus::Font font(L"Segoe UI", 11.f, Gdiplus::FontStyleRegular);
    return font;
}

Gdiplus::SolidBrush& brush_white()
{
    static Gdiplus::SolidBrush brush(Gdiplus::Color(255, 255, 255, 255));
    return brush;
}

Gdiplus::SolidBrush& brush_liquid_sub()
{
    static Gdiplus::SolidBrush brush(Gdiplus::Color(255, 189, 189, 189));
    return brush;
}

Gdiplus::SolidBrush& brush_vape_sub()
{
    static Gdiplus::SolidBrush brush(Gdiplus::Color(255, 205, 205, 205));
    return brush;
}

Gdiplus::SolidBrush& brush_gpt_sub()
{
    static Gdiplus::SolidBrush brush(Gdiplus::Color(255, 185, 185, 185));
    return brush;
}

Gdiplus::SolidBrush& brush_square_enabled()
{
    static Gdiplus::SolidBrush brush(Gdiplus::Color(255, 150, 255, 190));
    return brush;
}

Gdiplus::SolidBrush& brush_square_disabled()
{
    static Gdiplus::SolidBrush brush(Gdiplus::Color(255, 255, 150, 150));
    return brush;
}

Gdiplus::SolidBrush& brush_gemini_disabled()
{
    static Gdiplus::SolidBrush brush(Gdiplus::Color(120, 200, 200, 200));
    return brush;
}

Gdiplus::SolidBrush& brush_deepseek_sub()
{
    static Gdiplus::SolidBrush brush(Gdiplus::Color(255, 161, 161, 170));
    return brush;
}

void hide()
{
    if (!has_window()) return;
    KillTimer(s_hwnd, kTimer);
    if (s_visible) {
        ShowWindow(s_hwnd, SW_HIDE);
        s_visible = false;
    }
}

void release_back_buffer()
{
    if (s_hdcMem && s_oldBitmap) {
        SelectObject(s_hdcMem, s_oldBitmap);
        s_oldBitmap = nullptr;
    }
    if (s_bitmap) {
        DeleteObject(s_bitmap);
        s_bitmap = nullptr;
    }
    if (s_hdcMem) {
        DeleteDC(s_hdcMem);
        s_hdcMem = nullptr;
    }
    s_bits = nullptr;
    s_bufferWidth = 0;
    s_bufferHeight = 0;
    s_cacheDirty = true;
}

bool ensure_back_buffer(HDC hdcScreen, int width, int height)
{
    if (s_hdcMem && s_bitmap && s_bufferWidth == width && s_bufferHeight == height) return true;
    release_back_buffer();

    s_hdcMem = CreateCompatibleDC(hdcScreen);
    if (!s_hdcMem) return false;

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    s_bitmap = CreateDIBSection(s_hdcMem, &bmi, DIB_RGB_COLORS, &s_bits, nullptr, 0);
    if (!s_bitmap) {
        release_back_buffer();
        return false;
    }

    s_oldBitmap = static_cast<HBITMAP>(SelectObject(s_hdcMem, s_bitmap));
    s_bufferWidth = width;
    s_bufferHeight = height;
    std::cout << "[Notifications] 已创建通知后备缓冲: " << width << "x" << height << std::endl;
    return true;
}

void draw_text(Gdiplus::Graphics& g, const std::wstring& text, Gdiplus::Font& font,
    float x, float y, const Gdiplus::Color& color)
{
    Gdiplus::SolidBrush brush(color);
    g.DrawString(text.c_str(), -1, &font, Gdiplus::PointF(x, y), &brush);
}

void draw_text(Gdiplus::Graphics& g, const std::wstring& text, Gdiplus::Font& font,
    float x, float y, Gdiplus::Brush& brush)
{
    g.DrawString(text.c_str(), -1, &font, Gdiplus::PointF(x, y), &brush);
}

void fill_card(Gdiplus::Graphics& g, const Gdiplus::RectF& box, const Gdiplus::Color& bg,
    float radius, const Gdiplus::Color& border = Gdiplus::Color(0, 0, 0, 0))
{
    Gdiplus::GraphicsPath path;
    add_rounded_rect(path, box, radius);
    Gdiplus::SolidBrush bgBrush(bg);
    g.FillPath(&bgBrush, &path);
    if (border.GetAlpha() > 0) {
        Gdiplus::Pen pen(border, 1.f);
        g.DrawPath(&pen, &path);
    }
}

void draw_progress(Gdiplus::Graphics& g, const Gdiplus::RectF& box, float progress,
    const Gdiplus::Color& color, bool glow)
{
    const float barHeight = 4.f;
    Gdiplus::SolidBrush backBrush(Gdiplus::Color(120, 0, 0, 0));
    Gdiplus::RectF back(box.X, box.Y + box.Height - barHeight, box.Width, barHeight);
    g.FillRectangle(&backBrush, back);
    if (glow) {
        for (int i = 3; i >= 1; --i) {
            Gdiplus::SolidBrush glowBrush(Gdiplus::Color(static_cast<BYTE>(18 * i), color.GetR(), color.GetG(), color.GetB()));
            g.FillRectangle(&glowBrush, Gdiplus::RectF(back.X, back.Y - i, back.Width * progress, back.Height + i * 2.f));
        }
    }
    Gdiplus::SolidBrush barBrush(color);
    g.FillRectangle(&barBrush, Gdiplus::RectF(back.X, back.Y, back.Width * progress, back.Height));
}

void draw_liquidbounce(Gdiplus::Graphics& g, const Gdiplus::RectF& box, float knobAnimProgress)
{
    fill_card(g, box, Gdiplus::Color(245, 33, 33, 33), 8.f);

    const Gdiplus::RectF stateBox(box.X + 14.f, box.Y + 13.f, 46.f, 46.f);
    fill_card(g, stateBox,
        s_enabledState ? Gdiplus::Color(255, 76, 175, 80) : Gdiplus::Color(255, 255, 71, 58),
        6.f);

    const float trackWidth = stateBox.Width * 0.58f;
    const float trackX = stateBox.X + (stateBox.Width - trackWidth) * 0.5f;
    const float trackY = stateBox.Y + stateBox.Height * 0.5f;
    Gdiplus::Pen trackPen(Gdiplus::Color(255, 255, 255, 255), 2.f);
    trackPen.SetStartCap(Gdiplus::LineCapRound);
    trackPen.SetEndCap(Gdiplus::LineCapRound);
    g.DrawLine(&trackPen, Gdiplus::PointF(trackX, trackY), Gdiplus::PointF(trackX + trackWidth, trackY));

    const float clamped = std::clamp(knobAnimProgress, 0.f, 1.f);
    const float knobProgress = s_enabledState ? clamped : (1.f - clamped);
    constexpr float knobSize = 9.f;
    const float knobX = trackX + trackWidth * knobProgress - knobSize * 0.5f;
    const float knobY = trackY - knobSize * 0.5f;
    Gdiplus::SolidBrush knobBrush(Gdiplus::Color(255, 255, 255, 255));
    g.FillEllipse(&knobBrush, knobX, knobY, knobSize, knobSize);

    auto& titleFont = font_yahei_15_bold();
    auto& subFont = font_yahei_10_regular();
    draw_text(g, s_enabledState ? L"开启" : L"关闭", titleFont, box.X + 74.f, box.Y + 11.f,
        Gdiplus::Color(255, 255, 255, 255));
    draw_text(g, s_text, subFont, box.X + 74.f, box.Y + 39.f,
        Gdiplus::Color(255, 189, 189, 189));
}

void draw_vape(Gdiplus::Graphics& g, const Gdiplus::RectF& box, float progress)
{
    constexpr float cardRadius = 6.f;
    constexpr float barHeight = 4.f;
    fill_card(g, box, Gdiplus::Color(252, 18, 18, 18), cardRadius,
        Gdiplus::Color(255, 34, 34, 34));

    auto& titleFont = font_yahei_14_5_bold();
    auto& subFont = font_segoe_9_5_regular();
    draw_text(g, s_text, titleFont, box.X + 16.f, box.Y + 11.f, brush_white());

    draw_text(g, s_enabledState ? L"Enable" : L"Disable", subFont,
        box.X + 16.f, box.Y + 36.f, brush_vape_sub());

    const Gdiplus::RectF track(box.X, box.Y + box.Height - barHeight,
        box.Width, barHeight);
    Gdiplus::SolidBrush trackBrush(Gdiplus::Color(255, 38, 38, 38));
    g.FillRectangle(&trackBrush, track);

    const float fillWidth = track.Width * std::clamp(progress, 0.f, 1.f);
    if (fillWidth > 0.f) {
        const Gdiplus::RectF fill(track.X, track.Y, fillWidth, track.Height);
        Gdiplus::SolidBrush fillBrush(Gdiplus::Color(255, 0, 121, 107));
        if (fillWidth < barHeight) {
            g.FillRectangle(&fillBrush, fill);
        } else {
            Gdiplus::GraphicsPath fillPath;
            add_rounded_rect(fillPath, fill, barHeight / 2.f);
            g.FillPath(&fillBrush, &fillPath);
        }
    }
}

void draw_gpt(Gdiplus::Graphics& g, const Gdiplus::RectF& box, float progress)
{
    constexpr float radius = 11.f;
    constexpr float progressHeight = 2.5f;
    const Gdiplus::Color cardColor(244, 32, 33, 35);
    const Gdiplus::Color enabledColor(255, 16, 163, 127);
    const Gdiplus::Color disabledColor(255, 91, 111, 102);
    const Gdiplus::Color accentColor = s_enabledState ? enabledColor : disabledColor;

    for (int i = 3; i >= 1; --i) {
        const float spread = static_cast<float>(i) * 3.f;
        Gdiplus::RectF shadow(box.X - spread * 0.5f, box.Y + spread,
            box.Width + spread, box.Height + spread * 0.25f);
        Gdiplus::GraphicsPath shadowPath;
        add_rounded_rect(shadowPath, shadow, radius + spread * 0.45f);
        Gdiplus::SolidBrush shadowBrush(Gdiplus::Color(static_cast<BYTE>(10 + i * 5), 0, 0, 0));
        g.FillPath(&shadowBrush, &shadowPath);
    }

    Gdiplus::GraphicsPath cardPath;
    add_rounded_rect(cardPath, box, radius);
    Gdiplus::SolidBrush cardBrush(cardColor);
    g.FillPath(&cardBrush, &cardPath);

    const float iconSize = 30.f;
    const float iconX = box.X + 18.f;
    const float iconY = box.Y + 18.f;
    Gdiplus::SolidBrush iconBack(accentColor);
    g.FillEllipse(&iconBack, iconX, iconY, iconSize, iconSize);

    Gdiplus::Pen iconPen(Gdiplus::Color(255, 255, 255, 255), 2.2f);
    iconPen.SetStartCap(Gdiplus::LineCapRound);
    iconPen.SetEndCap(Gdiplus::LineCapRound);
    if (s_enabledState) {
        g.DrawLine(&iconPen, Gdiplus::PointF(iconX + 8.2f, iconY + 15.5f),
            Gdiplus::PointF(iconX + 13.2f, iconY + 20.3f));
        g.DrawLine(&iconPen, Gdiplus::PointF(iconX + 13.2f, iconY + 20.3f),
            Gdiplus::PointF(iconX + 22.2f, iconY + 10.6f));
    } else {
        g.DrawLine(&iconPen, Gdiplus::PointF(iconX + 8.5f, iconY + 15.f),
            Gdiplus::PointF(iconX + 21.5f, iconY + 15.f));
    }

    auto& titleFont = font_yahei_13_5_regular();
    auto& subFont = font_yahei_10_regular();
    Gdiplus::SolidBrush titleBrush(Gdiplus::Color(255, 245, 245, 245));
    draw_text(g, s_text, titleFont, box.X + 62.f, box.Y + 15.f, titleBrush);
    draw_text(g, s_enabledState ? L"Enabled" : L"Disabled", subFont,
        box.X + 62.f, box.Y + 39.f, brush_gpt_sub());

    Gdiplus::SolidBrush progressBack(Gdiplus::Color(70, 255, 255, 255));
    g.FillRectangle(&progressBack, Gdiplus::RectF(box.X + 12.f,
        box.Y + box.Height - progressHeight - 7.f,
        box.Width - 24.f, progressHeight));

    const float progressWidth = (box.Width - 24.f) * std::clamp(progress, 0.f, 1.f);
    if (progressWidth > 0.f) {
        Gdiplus::SolidBrush progressBrush(accentColor);
        g.FillRectangle(&progressBrush, Gdiplus::RectF(box.X + 12.f,
            box.Y + box.Height - progressHeight - 7.f,
            progressWidth, progressHeight));
    }
}

void draw_square(Gdiplus::Graphics& g, const Gdiplus::RectF& box)
{
    fill_card(g, box, Gdiplus::Color(215, 0, 0, 0), 10.f);
    Gdiplus::SolidBrush stripe(s_enabledState ? Gdiplus::Color(255, 80, 220, 120) : Gdiplus::Color(255, 230, 70, 70));
    g.FillRectangle(&stripe, Gdiplus::RectF(box.X, box.Y + 8.f, 3.f, box.Height - 16.f));
    auto& titleFont = font_yahei_13_bold();
    auto& subFont = font_yahei_10_regular();
    draw_text(g, s_text, titleFont, box.X + 16.f, box.Y + 12.f, brush_white());
    draw_text(g, s_enabledState ? L"Enabled" : L"Disabled", subFont,
        box.X + 16.f, box.Y + 38.f, s_enabledState
            ? static_cast<Gdiplus::Brush&>(brush_square_enabled())
            : static_cast<Gdiplus::Brush&>(brush_square_disabled()));
}

void draw_gemini_aurora(Gdiplus::Graphics& g, const Gdiplus::RectF& box, float seconds, float stateAlpha)
{
    if (stateAlpha <= 0.01f) return;

    const float left = box.X + 82.f;
    const float top = box.Y + 49.f;
    const float right = box.X + box.Width - 28.f;
    const float wave = std::sin(seconds * 1.3f) * 5.f;
    Gdiplus::GraphicsPath aurora;
    aurora.StartFigure();
    aurora.AddBezier(left, top + wave,
        left + 58.f, top - 10.f - wave,
        right - 74.f, top + 14.f + wave,
        right, top + 2.f - wave);
    aurora.AddLine(Gdiplus::PointF(right, top + 2.f - wave),
        Gdiplus::PointF(right, top + 20.f));
    aurora.AddBezier(right, top + 20.f,
        right - 72.f, top + 34.f - wave,
        left + 66.f, top + 18.f + wave,
        left, top + 26.f);
    aurora.CloseFigure();

    Gdiplus::LinearGradientBrush auroraBrush(
        Gdiplus::PointF(left, top), Gdiplus::PointF(right, top + 28.f),
        Gdiplus::Color(alpha_byte(26.f * stateAlpha), 0, 220, 255),
        Gdiplus::Color(alpha_byte(24.f * stateAlpha), 255, 150, 200));
    g.FillPath(&auroraBrush, &aurora);
}

void draw_gemini_icon(Gdiplus::Graphics& g, const Gdiplus::PointF& center, float seconds, float stateAlpha)
{
    constexpr float pi = 3.1415926535f;
    const float breathe = s_enabledState ? (1.f + 0.08f * std::sin(seconds * 3.4f)) : 1.f;
    const float ringRadius = 22.f * breathe;

    Gdiplus::Color skyBlue(255, 0, 220, 255);
    Gdiplus::Color softPink(255, 255, 150, 200);
    Gdiplus::Color disabled(120, 200, 200, 200);

    const auto state_color = [&](const Gdiplus::Color& enabledColor, float enabledAlpha) {
        return s_enabledState ? with_alpha(enabledColor, enabledAlpha * stateAlpha) : disabled;
    };

    const Gdiplus::GraphicsState saved = g.Save();
    if (s_enabledState) {
        Gdiplus::Matrix rotation;
        rotation.RotateAt(std::fmod(seconds * 18.f, 360.f), center);
        g.MultiplyTransform(&rotation);
    }
    Gdiplus::Pen ringPen(state_color(skyBlue, 185.f), 2.f);
    ringPen.SetDashStyle(Gdiplus::DashStyleDash);
    const float dash[] = { 3.f, 3.f };
    ringPen.SetDashPattern(dash, 2);
    g.DrawEllipse(&ringPen, center.X - ringRadius, center.Y - ringRadius,
        ringRadius * 2.f, ringRadius * 2.f);
    g.Restore(saved);

    const float coreRadius = s_enabledState ? 12.f * breathe : 11.f;
    Gdiplus::RectF core(center.X - coreRadius, center.Y - coreRadius,
        coreRadius * 2.f, coreRadius * 2.f);
    if (s_enabledState) {
        Gdiplus::LinearGradientBrush coreBrush(core, skyBlue, softPink, 35.f);
        g.FillEllipse(&coreBrush, core);
    } else {
        Gdiplus::SolidBrush coreBrush(disabled);
        g.FillEllipse(&coreBrush, core);
    }

    if (!s_enabledState) return;

    for (int i = 0; i < 8; ++i) {
        const float life = std::fmod(seconds * 0.42f + static_cast<float>(i) * 0.137f, 1.f);
        const float angle = static_cast<float>(i) * (2.f * pi / 8.f) + seconds * 0.35f;
        const float distance = 19.f + life * 23.f;
        const float size = 2.f + std::fmod(static_cast<float>(i) * 1.7f, 1.4f);
        const float alpha = (1.f - life) * 95.f * stateAlpha;
        Gdiplus::SolidBrush particleBrush(Gdiplus::Color(alpha_byte(alpha), 180, 235, 255));
        g.FillEllipse(&particleBrush,
            center.X + std::cos(angle) * distance - size * 0.5f,
            center.Y + std::sin(angle) * distance - size * 0.5f,
            size, size);
    }
}

void draw_gemini(Gdiplus::Graphics& g, const Gdiplus::RectF& box, float elapsedMs)
{
    const float seconds = elapsedMs / 1000.f;
    const float stateAlpha = s_enabledState
        ? std::clamp(elapsedMs / 200.f, 0.f, 1.f)
        : 0.f;

    Gdiplus::RectF glowBox(box.X - 20.f, box.Y - 18.f, box.Width + 40.f, box.Height + 36.f);
    Gdiplus::GraphicsPath glowPath;
    add_rounded_rect(glowPath, glowBox, 18.f);
    Gdiplus::PathGradientBrush glowBrush(&glowPath);
    glowBrush.SetCenterColor(Gdiplus::Color(68, 0, 191, 255));
    Gdiplus::Color glowSurround[] = { Gdiplus::Color(0, 130, 70, 220) };
    INT surroundCount = 1;
    glowBrush.SetSurroundColors(glowSurround, &surroundCount);
    g.FillPath(&glowBrush, &glowPath);

    Gdiplus::GraphicsPath cardPath;
    add_rounded_rect(cardPath, box, 13.f);
    Gdiplus::SolidBrush darkBackground(Gdiplus::Color(180, 18, 18, 24));
    g.FillPath(&darkBackground, &cardPath);

    Gdiplus::LinearGradientBrush tintBrush(
        Gdiplus::PointF(box.X, box.Y), Gdiplus::PointF(box.X + box.Width, box.Y + box.Height),
        Gdiplus::Color(38, 0, 220, 255), Gdiplus::Color(28, 255, 150, 200));
    g.FillPath(&tintBrush, &cardPath);

    Gdiplus::Pen edgePen(Gdiplus::Color(50, 210, 235, 255), 1.f);
    g.DrawPath(&edgePen, &cardPath);

    const Gdiplus::PointF iconCenter(box.X + 43.f, box.Y + box.Height * 0.5f);
    draw_gemini_icon(g, iconCenter, seconds, stateAlpha);
    draw_gemini_aurora(g, box, seconds, stateAlpha);

    auto& titleFont = font_yahei_14_5_bold();
    auto& subFont = font_yahei_10_5_bold();
    draw_text(g, s_text, titleFont, box.X + 82.f, box.Y + 17.f, brush_white());

    const std::wstring state = s_enabledState ? L"Enabled" : L"Disabled";
    const Gdiplus::RectF statusRect(box.X + 82.f, box.Y + 46.f, 148.f, 22.f);
    if (s_enabledState) {
        Gdiplus::LinearGradientBrush statusBrush(statusRect,
            Gdiplus::Color(255, 0, 220, 255),
            Gdiplus::Color(255, 255, 150, 200),
            Gdiplus::LinearGradientModeHorizontal);
        g.DrawString(state.c_str(), -1, &subFont,
            Gdiplus::PointF(statusRect.X, statusRect.Y), &statusBrush);
    } else {
        g.DrawString(state.c_str(), -1, &subFont,
            Gdiplus::PointF(statusRect.X, statusRect.Y), &brush_gemini_disabled());
    }
}

void draw_deepseek(Gdiplus::Graphics& g, const Gdiplus::RectF& box, float elapsedMs)
{
    constexpr bool darkMode = true;
    constexpr float radius = 12.f;
    constexpr float iconSize = 32.f;
    const Gdiplus::Color primary(255, 77, 107, 254);
    const Gdiplus::Color cardColor = darkMode
        ? Gdiplus::Color(221, 34, 34, 34)
        : Gdiplus::Color(238, 255, 255, 255);
    const Gdiplus::Color borderColor = darkMode
        ? Gdiplus::Color(178, 77, 107, 254)
        : Gdiplus::Color(102, 77, 107, 254);
    const Gdiplus::Color titleColor = darkMode
        ? Gdiplus::Color(255, 240, 240, 240)
        : Gdiplus::Color(255, 26, 26, 26);
    const Gdiplus::Color subtitleColor = darkMode
        ? Gdiplus::Color(255, 170, 170, 170)
        : Gdiplus::Color(255, 102, 102, 102);
    const Gdiplus::Color offColor = darkMode
        ? Gdiplus::Color(255, 85, 85, 85)
        : Gdiplus::Color(255, 208, 208, 208);

    const float seconds = elapsedMs / 1000.f;
    const float pulse = 0.3f + 0.5f * (std::sin(seconds * 3.1415926535f) * 0.5f + 0.5f);
    Gdiplus::RectF glowBox(box.X - 16.f, box.Y - 14.f, box.Width + 32.f, box.Height + 28.f);
    Gdiplus::GraphicsPath glowPath;
    add_rounded_rect(glowPath, glowBox, radius + 8.f);
    Gdiplus::PathGradientBrush glowBrush(&glowPath);
    glowBrush.SetCenterColor(Gdiplus::Color(alpha_byte(44.f * pulse), 77, 107, 254));
    Gdiplus::Color glowSurround[] = { Gdiplus::Color(0, 77, 107, 254) };
    INT glowCount = 1;
    glowBrush.SetSurroundColors(glowSurround, &glowCount);
    g.FillPath(&glowBrush, &glowPath);

    Gdiplus::GraphicsPath cardPath;
    add_rounded_rect(cardPath, box, radius);
    Gdiplus::SolidBrush cardBrush(cardColor);
    g.FillPath(&cardBrush, &cardPath);
    Gdiplus::Pen borderPen(borderColor, 1.f);
    g.DrawPath(&borderPen, &cardPath);

    const float iconScaleProgress = std::clamp(elapsedMs / static_cast<float>(kDeepSeekIconPopMs), 0.f, 1.f);
    const float iconScale = 1.f + 0.2f * (1.f - ease_out(iconScaleProgress));
    const Gdiplus::PointF iconCenter(box.X + 16.f + iconSize * 0.5f, box.Y + box.Height * 0.5f);
    const Gdiplus::GraphicsState iconState = g.Save();
    Gdiplus::Matrix iconMatrix;
    iconMatrix.Translate(iconCenter.X, iconCenter.Y);
    iconMatrix.Scale(iconScale, iconScale);
    iconMatrix.Translate(-iconCenter.X, -iconCenter.Y);
    g.MultiplyTransform(&iconMatrix);

    Gdiplus::SolidBrush iconBack(s_enabledState ? primary : offColor);
    g.FillEllipse(&iconBack, iconCenter.X - iconSize * 0.5f, iconCenter.Y - iconSize * 0.5f,
        iconSize, iconSize);
    Gdiplus::Pen iconPen(Gdiplus::Color(255, 255, 255, 255), 2.4f);
    iconPen.SetStartCap(Gdiplus::LineCapRound);
    iconPen.SetEndCap(Gdiplus::LineCapRound);
    if (s_enabledState) {
        g.DrawLine(&iconPen, Gdiplus::PointF(iconCenter.X - 8.f, iconCenter.Y),
            Gdiplus::PointF(iconCenter.X - 2.f, iconCenter.Y + 6.f));
        g.DrawLine(&iconPen, Gdiplus::PointF(iconCenter.X - 2.f, iconCenter.Y + 6.f),
            Gdiplus::PointF(iconCenter.X + 9.f, iconCenter.Y - 7.f));
    } else {
        g.DrawLine(&iconPen, Gdiplus::PointF(iconCenter.X - 7.f, iconCenter.Y - 7.f),
            Gdiplus::PointF(iconCenter.X + 7.f, iconCenter.Y + 7.f));
        g.DrawLine(&iconPen, Gdiplus::PointF(iconCenter.X + 7.f, iconCenter.Y - 7.f),
            Gdiplus::PointF(iconCenter.X - 7.f, iconCenter.Y + 7.f));
    }
    g.Restore(iconState);

    auto& titleFont = font_segoe_14_bold();
    auto& subFont = font_segoe_11_regular();
    Gdiplus::SolidBrush titleBrush(titleColor);
    Gdiplus::SolidBrush subtitleBrush(subtitleColor);
    draw_text(g, s_enabledState ? L"功能已开启" : L"功能已关闭", titleFont,
        box.X + 60.f, box.Y + 15.f, titleBrush);
    draw_text(g, s_text, subFont, box.X + 60.f, box.Y + 39.f, subtitleBrush);
}

void draw_liquidbounce_list()
{
    if (!has_window() || !g_notificationsEnabled || !IsCS2WindowActive()) {
        hide();
        return;
    }

    constexpr int style = 0;
    constexpr int width = 310;
    constexpr int height = 72;
    constexpr int pad = 4;
    constexpr int gap = 10;
    constexpr int slideRoom = 150;
    const int renderWidth = width + pad * 2 + slideRoom;
    const int renderHeight = height * 3 + gap * 2 + pad * 2;
    const ULONGLONG now = GetTickCount64();
    const float durationMs = std::clamp(g_notificationsDuration, 0.05f, 86400.f) * 1000.f;

    for (auto& item : s_items) {
        if (item.exitTick == 0 && static_cast<float>(now - item.startTick) >= durationMs) {
            item.exitTick = now;
        }
    }

    for (auto it = s_items.begin(); it != s_items.end();) {
        if (it->exitTick == 0 || static_cast<float>(now - it->exitTick) < static_cast<float>(kAnimMs)) {
            ++it;
            continue;
        }

        if (it->hasReplacement) {
            notification_item replacement;
            replacement.text = std::move(it->replacementText);
            replacement.enabled = it->replacementEnabled;
            replacement.startTick = now;
            replacement.y = it->y;
            replacement.yInitialized = true;
            it = s_items.erase(it);
            it = s_items.insert(it, std::move(replacement));
            ++it;
            continue;
        }

        it = s_items.erase(it);
    }

    while (!s_waitingItems.empty() && s_items.size() < 3) {
        notification_item item = std::move(s_waitingItems.front());
        s_waitingItems.erase(s_waitingItems.begin());
        item.startTick = now;
        item.exitTick = 0;
        append_list_item(std::move(item), height, gap, pad);
    }

    if (s_items.empty()) {
        hide();
        return;
    }

    const int sw = GetSystemMetrics(SM_CXSCREEN);
    const int sh = GetSystemMetrics(SM_CYSCREEN);
    const int baseX = sw - width - 26;
    const int baseY = sh - height - 42;
    const int windowX = baseX - pad;
    const int windowY = baseY - 2 * (height + gap) - pad;

    HDC hdcScreen = GetDC(nullptr);
    if (!ensure_back_buffer(hdcScreen, renderWidth, renderHeight)) {
        ReleaseDC(nullptr, hdcScreen);
        return;
    }

    using namespace Gdiplus;
    Graphics g(s_hdcMem);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    g.SetCompositingQuality(CompositingQualityHighQuality);
    g.Clear(Color(0, 0, 0, 0));

    for (std::size_t i = 0; i < s_items.size(); ++i) {
        auto& item = s_items[i];
        const float targetY = list_slot_y(static_cast<int>(i), height, gap, pad);
        if (!item.yInitialized) {
            item.y = targetY;
            item.yInitialized = true;
        }
        item.y += (targetY - item.y) * 0.32f;
        if (std::fabs(item.y - targetY) < 0.35f) item.y = targetY;

        const float elapsed = static_cast<float>(now - item.startTick);
        const float entry = ease_out(elapsed / static_cast<float>(kAnimMs));
        float exit = 0.0f;
        if (item.exitTick != 0) {
            exit = ease_out(static_cast<float>(now - item.exitTick) / static_cast<float>(kAnimMs));
        }

        const float x = static_cast<float>(pad) + (1.0f - entry) * 84.0f + exit * static_cast<float>(slideRoom);
        RectF box(x, item.y, static_cast<REAL>(width), static_cast<REAL>(height));
        s_text = item.text;
        s_enabledState = item.enabled;
        const float knobProgress = ease_out(elapsed / static_cast<float>(kLiquidBounceKnobAnimMs));
        draw_liquidbounce(g, box, knobProgress);
    }

    POINT dst{ windowX, windowY };
    SIZE size{ renderWidth, renderHeight };
    POINT src{ 0, 0 };
    BLENDFUNCTION blend{};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255;
    blend.AlphaFormat = AC_SRC_ALPHA;
    UpdateLayeredWindow(s_hwnd, hdcScreen, &dst, &size, s_hdcMem, &src, 0, &blend, ULW_ALPHA);
    ReleaseDC(nullptr, hdcScreen);

    if (!s_visible) {
        ShowWindow(s_hwnd, SW_SHOWNOACTIVATE);
        s_visible = true;
    }
}

void draw()
{
    const int style = std::clamp(g_notificationsStyle, 0, 5);
    if (is_list_style(style)) {
        draw_liquidbounce_list();
        return;
    }

    if (!has_window() || !g_notificationsEnabled || s_text.empty() || !IsCS2WindowActive()) {
        hide();
        return;
    }

    const ULONGLONG now = GetTickCount64();
    const float durationMs = std::clamp(g_notificationsDuration, 0.05f, 86400.f) * 1000.f;
    const float elapsed = static_cast<float>(now - s_startTick);
    if (durationMs <= 1.f || elapsed >= durationMs) {
        s_text.clear();
        hide();
        return;
    }

    const int width = style == 4 ? 350 : (style == 3 ? 370 : (style == 2 ? 340 : (style == 5 ? 330 : 310)));
    const int height = style == 3 ? 86 : (style == 2 ? 78 : (style == 4 ? 70 : 72));
    const int pad = style == 3 ? 28 : (style == 4 ? 24 : (style == 2 ? 12 : (style == 1 ? 8 : 4)));
    const int renderWidth = width + pad * 2;
    const int renderHeight = height + pad * 2;
    const int sw = GetSystemMetrics(SM_CXSCREEN);
    const int sh = GetSystemMetrics(SM_CYSCREEN);
    const float progress = std::clamp(1.f - elapsed / durationMs, 0.f, 1.f);
    const float liquidbounceKnobProgress = ease_out(
        elapsed / static_cast<float>(kLiquidBounceKnobAnimMs));
    float in = ease_out(elapsed / static_cast<float>(kAnimMs));
    if (style == 2) {
        in = ease_out(elapsed / static_cast<float>(kGptEntryMs));
    } else if (style == 4) {
        in = ease_out(elapsed / static_cast<float>(kDeepSeekEntryMs));
    }

    float outDuration = static_cast<float>(kAnimMs);
    if (style == 2) outDuration = static_cast<float>(kGptExitMs);
    else if (style == 3) outDuration = static_cast<float>(kGeminiExitMs);
    else if (style == 4) outDuration = static_cast<float>(kDeepSeekExitMs);

    const float outStart = (std::max)(0.f, durationMs - outDuration);
    float out = 0.f;
    if (elapsed > outStart) {
        const float outProgress = (elapsed - outStart) / outDuration;
        out = style == 3 ? ease_in_quint(outProgress)
            : (style == 4 ? ease_in_cubic(outProgress) : ease_out(outProgress));
    }
    const int baseX = sw - width - 26;
    const int baseY = sh - height - 42;
    int x = baseX + static_cast<int>(std::lround((1.f - in + out) * 360.f));
    int y = baseY;
    float layerAlpha = 1.f - out;
    float geminiScale = 1.f;
    if (style == 1) {
        constexpr float vapeSlideY = 48.f;
        y = baseY - static_cast<int>(std::lround((1.f - in + out) * vapeSlideY));
    } else if (style == 2) {
        x = baseX + static_cast<int>(std::lround((1.f - in + out) * 300.f));
        y = baseY;
        layerAlpha = std::clamp(in * (1.f - out), 0.f, 1.f);
    } else if (style == 3) {
        // Gemini 入场使用“极光凝聚”：右下角浮现、逐渐显形，并在中心轻微回弹。
        const float entryProgress = std::clamp(elapsed / static_cast<float>(kGeminiEntryMs), 0.f, 1.f);
        const float entryCurve = ease_bounce(entryProgress);
        const float entryRemain = 1.f - std::clamp(entryCurve, 0.f, 1.f);
        const float exitCurve = out;
        const float exitRemain = 1.f - exitCurve;
        x = baseX + static_cast<int>(std::lround(entryRemain * 86.f + exitCurve * 54.f));
        y = baseY + static_cast<int>(std::lround(entryRemain * 46.f + std::sin(entryRemain * 3.1415926535f) * 10.f));
        layerAlpha = std::clamp(ease_out(entryProgress) * exitRemain, 0.f, 1.f);
        if (exitCurve > 0.f) {
            // Gemini 退场使用“星尘消散”：原位快速收缩，并向右侧轻滑淡出。
            geminiScale = 0.85f + 0.15f * exitRemain;
            y = baseY;
        } else {
            geminiScale = 0.90f + 0.10f * std::clamp(entryCurve, 0.f, 1.08f)
                + 0.03f * std::sin(entryProgress * 3.1415926535f);
        }
    } else if (style == 4) {
        x = baseX;
        y = baseY + static_cast<int>(std::lround((1.f - in) * 20.f - out * 15.f));
        layerAlpha = std::clamp(in * (1.f - out), 0.f, 1.f);
    }

    HDC hdcScreen = GetDC(nullptr);
    if (!ensure_back_buffer(hdcScreen, renderWidth, renderHeight)) {
        ReleaseDC(nullptr, hdcScreen);
        return;
    }

    const int progressBucket = style == 0
        ? static_cast<int>(std::lround(liquidbounceKnobProgress * 48.f))
        : ((style == 1 || style == 2) ? static_cast<int>(std::lround(progress * 48.f))
            : ((style == 3 || style == 4) ? static_cast<int>((now / kFrameMs) % 240) : 0));
    const bool needsRedraw = s_cacheDirty
        || s_cachedText != s_text
        || s_cachedEnabledState != s_enabledState
        || s_cachedStyle != style
        || s_cachedProgressBucket != progressBucket
        || s_cachedWidth != renderWidth
        || s_cachedHeight != renderHeight;

    if (needsRedraw) {
        using namespace Gdiplus;
        Graphics g(s_hdcMem);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);
        g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
        g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
        g.SetCompositingQuality(CompositingQualityHighQuality);
        g.Clear(Color(0, 0, 0, 0));
        RectF box(static_cast<REAL>(pad), static_cast<REAL>(pad), static_cast<REAL>(width), static_cast<REAL>(height));

        if (style == 0) draw_liquidbounce(g, box, liquidbounceKnobProgress);
        else if (style == 1) draw_vape(g, box, progress);
        else if (style == 2) draw_gpt(g, box, progress);
        else if (style == 3) {
            const Gdiplus::GraphicsState saved = g.Save();
            const Gdiplus::PointF center(box.X + box.Width * 0.5f, box.Y + box.Height * 0.5f);
            Gdiplus::Matrix transform;
            transform.Translate(center.X, center.Y);
            transform.Scale(geminiScale, geminiScale);
            transform.Translate(-center.X, -center.Y);
            g.MultiplyTransform(&transform);
            draw_gemini(g, box, elapsed);
            g.Restore(saved); // 恢复画布状态，避免中心缩放影响后续 HUD 绘制。
        }
        else if (style == 4) draw_deepseek(g, box, elapsed);
        else draw_square(g, box);

        s_cachedText = s_text;
        s_cachedEnabledState = s_enabledState;
        s_cachedStyle = style;
        s_cachedProgressBucket = progressBucket;
        s_cachedWidth = renderWidth;
        s_cachedHeight = renderHeight;
        s_cacheDirty = false;
    }

    POINT dst{ x - pad, y - pad };
    SIZE size{ renderWidth, renderHeight };
    POINT src{ 0, 0 };
    BLENDFUNCTION blend{};
    blend.BlendOp = AC_SRC_OVER;
    blend.SourceConstantAlpha = alpha_byte(layerAlpha * 255.f);
    blend.AlphaFormat = AC_SRC_ALPHA;
    UpdateLayeredWindow(s_hwnd, hdcScreen, &dst, &size, s_hdcMem, &src, 0, &blend, ULW_ALPHA);
    ReleaseDC(nullptr, hdcScreen);

    if (!s_visible) {
        ShowWindow(s_hwnd, SW_SHOWNOACTIVATE);
        s_visible = true;
    }
}

LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == kPushMessage) {
        const int style = std::clamp(g_notificationsStyle, 0, 5);
        {
            std::lock_guard lock(s_pendingMutex);
            if (is_list_style(style)) {
                constexpr int height = 72;
                constexpr int pad = 4;
                constexpr int gap = 10;
                notification_item item;
                item.text = std::move(s_pendingText);
                item.enabled = s_pendingEnabledState;
                item.startTick = GetTickCount64();
                push_list_item(std::move(item), item.startTick, height, gap, pad);
                s_text.clear();
            } else {
                s_items.clear();
                s_waitingItems.clear();
                s_text = std::move(s_pendingText);
                s_enabledState = s_pendingEnabledState;
                s_startTick = GetTickCount64();
                s_cacheDirty = true;
            }
        }
        SetTimer(s_hwnd, kTimer, kFrameMs, nullptr);
        draw();
        return 0;
    }
    if (msg == WM_TIMER && wp == kTimer) {
        draw();
        return 0;
    }
    if (msg == WM_DESTROY) {
        KillTimer(hwnd, kTimer);
        release_back_buffer();
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
        wc.lpszClassName, L"", WS_POPUP, 0, 0, 1, 1,
        nullptr, nullptr, hInst, nullptr);
    if (!s_hwnd) {
        std::cout << "[Notifications] 创建覆盖层失败。" << std::endl;
        return;
    }
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
    {
        std::lock_guard lock(s_pendingMutex);
        s_pendingText = text;
        s_pendingEnabledState = enabled;
    }
    PostMessageW(s_hwnd, kPushMessage, 0, 0);
}

void Show(const std::wstring& text, bool enabled)
{
    Push(text, enabled);
}

void Refresh()
{
    draw();
}

} // namespace notifications_overlay
