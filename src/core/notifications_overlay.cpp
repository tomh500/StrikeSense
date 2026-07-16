#include "notifications_overlay.h"

#include "StrikeSense.h"
#include "volume_mixer.h"

#include <algorithm>
#include <cmath>
#include <gdiplus.h>
#include <iostream>
#include <mmsystem.h>

namespace notifications_overlay {
namespace {

HWND s_hwnd = nullptr;
HINSTANCE s_hInst = nullptr;
std::wstring s_text;
bool s_enabledState = true;
ULONGLONG s_startTick = 0;
bool s_visible = false;
bool s_timerResolutionRaised = false;
HDC s_hdcMem = nullptr;
HBITMAP s_bitmap = nullptr;
HBITMAP s_oldBitmap = nullptr;
void* s_bits = nullptr;
int s_bufferWidth = 0;
int s_bufferHeight = 0;
constexpr UINT_PTR kTimer = 3021;
constexpr UINT kFrameMs = 1000 / 60;
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

float ease_bounce(float t)
{
    t = std::clamp(t, 0.f, 1.f);
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.f;
    return 1.f + c3 * std::pow(t - 1.f, 3.f) + c1 * std::pow(t - 1.f, 2.f);
}

void add_rounded_rect(Gdiplus::GraphicsPath& path, const Gdiplus::RectF& rect, float radius)
{
    path.AddArc(rect.X, rect.Y, radius * 2.f, radius * 2.f, 180.f, 90.f);
    path.AddArc(rect.X + rect.Width - radius * 2.f, rect.Y, radius * 2.f, radius * 2.f, 270.f, 90.f);
    path.AddArc(rect.X + rect.Width - radius * 2.f, rect.Y + rect.Height - radius * 2.f, radius * 2.f, radius * 2.f, 0.f, 90.f);
    path.AddArc(rect.X, rect.Y + rect.Height - radius * 2.f, radius * 2.f, radius * 2.f, 90.f, 90.f);
    path.CloseFigure();
}

void hide()
{
    if (!has_window() || !s_visible) return;
    ShowWindow(s_hwnd, SW_HIDE);
    s_visible = false;
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
    std::cout << "[Notifications] 已创建60FPS后备缓冲: " << width << "x" << height << std::endl;
    return true;
}

void draw_text(Gdiplus::Graphics& g, const std::wstring& text, Gdiplus::Font& font,
    float x, float y, const Gdiplus::Color& color)
{
    Gdiplus::SolidBrush brush(color);
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

void draw_liquidbounce(Gdiplus::Graphics& g, const Gdiplus::RectF& box, float toggleProgress)
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

    const float clamped = std::clamp(toggleProgress, 0.f, 1.f);
    const float knobProgress = s_enabledState ? clamped : (1.f - clamped);
    constexpr float knobSize = 9.f;
    const float knobX = trackX + trackWidth * knobProgress - knobSize * 0.5f;
    const float knobY = trackY - knobSize * 0.5f;
    Gdiplus::SolidBrush knobBrush(Gdiplus::Color(255, 255, 255, 255));
    g.FillEllipse(&knobBrush, knobX, knobY, knobSize, knobSize);

    Gdiplus::Font titleFont(L"Microsoft YaHei UI", 15.f, Gdiplus::FontStyleBold);
    Gdiplus::Font subFont(L"Microsoft YaHei UI", 10.f, Gdiplus::FontStyleRegular);
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

    Gdiplus::Font titleFont(L"Segoe UI Semibold", 14.5f, Gdiplus::FontStyleRegular);
    Gdiplus::Font subFont(L"Segoe UI", 9.5f, Gdiplus::FontStyleRegular);
    draw_text(g, s_text, titleFont, box.X + 16.f, box.Y + 11.f,
        Gdiplus::Color(255, 255, 255, 255));

    draw_text(g, s_enabledState ? L"Enable" : L"Disable", subFont,
        box.X + 16.f, box.Y + 36.f, Gdiplus::Color(255, 205, 205, 205));

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
    fill_card(g, box, Gdiplus::Color(235, 15, 18, 25), 12.f, Gdiplus::Color(180, 90, 170, 255));
    Gdiplus::Font titleFont(L"Microsoft YaHei UI", 13.f, Gdiplus::FontStyleBold);
    Gdiplus::Font subFont(L"Microsoft YaHei UI", 10.f, Gdiplus::FontStyleRegular);
    draw_text(g, L"StrikeSense", titleFont, box.X + 16.f, box.Y + 11.f, Gdiplus::Color(255, 255, 255, 255));
    draw_text(g, s_text + (s_enabledState ? L" Enabled" : L" Disabled"), subFont,
        box.X + 16.f, box.Y + 36.f, Gdiplus::Color(230, 210, 220, 235));
    draw_progress(g, box, progress, s_enabledState ? Gdiplus::Color(255, 74, 222, 150) : Gdiplus::Color(255, 255, 95, 95), false);
}

void draw_gemini(Gdiplus::Graphics& g, const Gdiplus::RectF& box)
{
    fill_card(g, box, Gdiplus::Color(215, 0, 0, 0), 10.f);
    Gdiplus::SolidBrush stripe(s_enabledState ? Gdiplus::Color(255, 80, 220, 120) : Gdiplus::Color(255, 230, 70, 70));
    g.FillRectangle(&stripe, Gdiplus::RectF(box.X, box.Y + 8.f, 3.f, box.Height - 16.f));
    Gdiplus::Font titleFont(L"Microsoft YaHei UI", 13.f, Gdiplus::FontStyleBold);
    Gdiplus::Font subFont(L"Microsoft YaHei UI", 10.f, Gdiplus::FontStyleRegular);
    draw_text(g, s_text, titleFont, box.X + 16.f, box.Y + 12.f, Gdiplus::Color(255, 255, 255, 255));
    draw_text(g, s_enabledState ? L"Enabled" : L"Disabled", subFont,
        box.X + 16.f, box.Y + 38.f, s_enabledState ? Gdiplus::Color(255, 150, 255, 190) : Gdiplus::Color(255, 255, 150, 150));
}

void draw_deepseek(Gdiplus::Graphics& g, const Gdiplus::RectF& box)
{
    Gdiplus::RectF shadow(box.X + 3.f, box.Y + 5.f, box.Width, box.Height);
    fill_card(g, shadow, Gdiplus::Color(65, 0, 0, 0), 12.f);
    fill_card(g, box, Gdiplus::Color(235, 14, 14, 20), 12.f);

    Gdiplus::GraphicsPath path;
    add_rounded_rect(path, box, 12.f);
    Gdiplus::LinearGradientBrush borderBrush(
        Gdiplus::PointF(box.X, box.Y), Gdiplus::PointF(box.X + box.Width, box.Y),
        Gdiplus::Color(20, 0, 242, 254), Gdiplus::Color(190, 79, 172, 254));
    Gdiplus::Pen borderPen(&borderBrush, 1.f);
    g.DrawPath(&borderPen, &path);

    Gdiplus::SolidBrush iconBack(Gdiplus::Color(45, 0, 242, 254));
    Gdiplus::SolidBrush iconBrush(Gdiplus::Color(255, 0, 242, 254));
    g.FillEllipse(&iconBack, box.X + 18.f, box.Y + 16.f, 36.f, 36.f);
    g.FillRectangle(&iconBrush, box.X + 31.f, box.Y + 25.f, 10.f, 18.f);
    g.FillRectangle(&iconBrush, box.X + 25.f, box.Y + 31.f, 22.f, 6.f);

    Gdiplus::Font titleFont(L"Microsoft YaHei UI", 13.5f, Gdiplus::FontStyleBold);
    Gdiplus::Font subFont(L"Microsoft YaHei UI", 9.f, Gdiplus::FontStyleRegular);
    draw_text(g, s_text, titleFont, box.X + 68.f, box.Y + 13.f, Gdiplus::Color(255, 255, 255, 255));
    draw_text(g, L"StrikeSense module toggle", subFont, box.X + 68.f, box.Y + 39.f, Gdiplus::Color(255, 161, 161, 170));

    const std::wstring state = s_enabledState ? L"ENABLED" : L"DISABLED";
    const Gdiplus::Color pillBg = s_enabledState ? Gdiplus::Color(40, 0, 255, 170) : Gdiplus::Color(45, 255, 68, 85);
    const Gdiplus::Color pillText = s_enabledState ? Gdiplus::Color(255, 0, 255, 170) : Gdiplus::Color(255, 255, 68, 85);
    Gdiplus::RectF pill(box.X + box.Width - 116.f, box.Y + 22.f, 96.f, 25.f);
    fill_card(g, pill, pillBg, 12.f);
    Gdiplus::Font pillFont(L"Microsoft YaHei UI", 9.f, Gdiplus::FontStyleBold);
    draw_text(g, state, pillFont, pill.X + 17.f, pill.Y + 5.f, pillText);
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

    const int style = std::clamp(g_notificationsStyle, 0, 4);
    const int width = style == 4 ? 460 : (style == 3 ? 330 : 310);
    const int height = style == 4 ? 68 : 72;
    const int pad = style == 1 ? 8 : 4;
    const int renderWidth = width + pad * 2;
    const int renderHeight = height + pad * 2;
    const int sw = GetSystemMetrics(SM_CXSCREEN);
    const int sh = GetSystemMetrics(SM_CYSCREEN);
    const float progress = std::clamp(1.f - elapsed / durationMs, 0.f, 1.f);
    const float in = style == 4 ? ease_bounce(elapsed / static_cast<float>(kAnimMs)) : ease_out(elapsed / static_cast<float>(kAnimMs));
    const float outStart = (std::max)(0.f, durationMs - kAnimMs);
    const float out = elapsed > outStart ? ease_out((elapsed - outStart) / static_cast<float>(kAnimMs)) : 0.f;
    const int baseX = sw - width - 26;
    const int baseY = sh - height - 42;
    int x = baseX + static_cast<int>((1.f - in + out) * 360.f);
    int y = baseY;
    if (style == 1) {
        constexpr float vapeSlideY = 48.f;
        y = baseY - static_cast<int>((1.f - in + out) * vapeSlideY);
    } else if (style == 4) {
        x = baseX + static_cast<int>(out * 360.f);
        y = baseY - static_cast<int>((1.f - in) * 80.f);
    }

    HDC hdcScreen = GetDC(nullptr);
    if (!ensure_back_buffer(hdcScreen, renderWidth, renderHeight)) {
        ReleaseDC(nullptr, hdcScreen);
        return;
    }

    using namespace Gdiplus;
    Graphics g(s_hdcMem);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    g.Clear(Color(0, 0, 0, 0));
    RectF box(static_cast<REAL>(pad), static_cast<REAL>(pad), static_cast<REAL>(width), static_cast<REAL>(height));

    if (style == 0) draw_liquidbounce(g, box, in);
    else if (style == 1) draw_vape(g, box, progress);
    else if (style == 2) draw_gpt(g, box, progress);
    else if (style == 3) draw_gemini(g, box);
    else draw_deepseek(g, box);

    POINT dst{ x - pad, y - pad };
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

LRESULT CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_TIMER && wp == kTimer) {
        draw();
        return 0;
    }
    if (msg == WM_DESTROY) {
        KillTimer(hwnd, kTimer);
        release_back_buffer();
        if (s_timerResolutionRaised) {
            timeEndPeriod(1);
            s_timerResolutionRaised = false;
            std::cout << "[Notifications] 已恢复系统计时精度。" << std::endl;
        }
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
    if (timeBeginPeriod(1) == TIMERR_NOERROR) {
        s_timerResolutionRaised = true;
        std::cout << "[Notifications] 已启用60FPS计时精度。" << std::endl;
    }
    SetTimer(s_hwnd, kTimer, kFrameMs, nullptr);
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
    s_text = text;
    s_enabledState = enabled;
    s_startTick = GetTickCount64();
    draw();
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
