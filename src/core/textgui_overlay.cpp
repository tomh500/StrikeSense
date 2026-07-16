#include "textgui_overlay.h"

#include "StrikeSense.h"
#include "module_notifications.h"
#include "pages.h"
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

void draw_box_logo(Gdiplus::Graphics& g, float x, float y, float scale,
    const Gdiplus::Color& color)
{
    using namespace Gdiplus;
    Pen outerPen(color, (std::max)(1.f, 2.4f * scale));
    outerPen.SetLineJoin(LineJoinRound);
    outerPen.SetStartCap(LineCapRound);
    outerPen.SetEndCap(LineCapRound);
    Pen detailPen(color, std::clamp(g_textguiLogoDetailThickness, 0.2f, 8.f) * scale);
    detailPen.SetLineJoin(LineJoinRound);
    detailPen.SetStartCap(LineCapRound);
    detailPen.SetEndCap(LineCapRound);
    const auto point = [&](float px, float py) { return PointF(x + px * scale, y + py * scale); };

    const PointF box[] = {
        point(3.f, 16.f), point(18.f, 22.f), point(34.f, 15.f), point(19.f, 9.f),
        point(3.f, 16.f), point(3.f, 29.f), point(18.f, 36.f), point(34.f, 29.f),
        point(34.f, 15.f), point(18.f, 22.f), point(18.f, 36.f)
    };
    g.DrawLines(&outerPen, box, static_cast<INT>(std::size(box)));

    // AK 枪管朝右上，枪身落在箱口内。
    const PointF receiver[] = {
        point(10.f, 22.f), point(15.f, 17.f), point(23.f, 14.f), point(27.f, 17.f),
        point(20.f, 22.f), point(14.f, 24.f), point(10.f, 22.f)
    };
    g.DrawLines(&detailPen, receiver, static_cast<INT>(std::size(receiver)));
    g.DrawLine(&detailPen, point(23.f, 14.f), point(30.f, 3.f));
    g.DrawLine(&detailPen, point(26.f, 16.f), point(33.f, 5.f));
    g.DrawLine(&detailPen, point(29.f, 3.f), point(34.f, 5.f));
    g.DrawLine(&detailPen, point(9.f, 22.f), point(5.f, 27.f));
    g.DrawLine(&detailPen, point(5.f, 27.f), point(11.f, 25.f));
    const PointF magazine[] = {
        point(18.f, 22.f), point(21.f, 25.f), point(19.f, 30.f), point(15.f, 27.f)
    };
    g.DrawLines(&detailPen, magazine, static_cast<INT>(std::size(magazine)));
    g.DrawLine(&detailPen, point(14.f, 23.f), point(16.f, 28.f));
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
    const float scale = std::clamp(g_textguiScale, 0.05f, 5.f);
    const float opacity = std::clamp(g_textguiOpacity, 0.f, 1.0f);
    const BYTE alpha = static_cast<BYTE>(255.f * opacity);
    const Color fixedColor(alpha,
        static_cast<BYTE>(std::clamp(g_textguiR, 0, 255)),
        static_cast<BYTE>(std::clamp(g_textguiG, 0, 255)),
        static_cast<BYTE>(std::clamp(g_textguiB, 0, 255)));
    Font titleFont(L"Segoe UI", 22.f * scale, FontStyleBold);
    Font sloganFont(L"Microsoft YaHei UI", 11.f * scale, FontStyleRegular);
    Font itemFont(L"Microsoft YaHei UI", 13.5f * scale, FontStyleBold);
    Font accessoryFont(L"Microsoft YaHei UI", 12.5f * scale, FontStyleRegular);
    const float rainbowSpeed = std::clamp(g_textguiRainbowSpeed, -1000.f, 1000.f);
    const float baseHue = std::fmod(static_cast<float>(GetTickCount64()) * 0.12f * rainbowSpeed, 360.f);
    const float lineSpacing = std::clamp(g_textguiLineSpacing, 0.05f, 20.f);

    auto features = modulenotifications::CollectEnabledFeatures();
    {
        Graphics measure(hdcMeasure);
        measure.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
        std::sort(features.begin(), features.end(), [&](const auto& a, const auto& b) {
            const int aw = text_width(measure, itemFont, a.text)
                + (a.accessory.empty() ? 0 : text_width(measure, accessoryFont, L" " + a.accessory));
            const int bw = text_width(measure, itemFont, b.text)
                + (b.accessory.empty() ? 0 : text_width(measure, accessoryFont, L" " + b.accessory));
            return aw > bw;
        });
    }

    Graphics measure(hdcMeasure);
    measure.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
    const float rowH = 25.f * scale * lineSpacing;
    const auto line_width = [&](const modulenotifications::feature_line& line) {
        return text_width(measure, itemFont, line.text)
            + (line.accessory.empty() ? 0 : text_width(measure, accessoryFont, L" " + line.accessory));
    };
    const int maxFeatureW = features.empty() ? 0 : line_width(features.front());
    const int watermarkW = static_cast<int>(44.f * scale) + text_width(measure, titleFont, L"STRIKESENSE");
    const bool showSlogan = !g_textguiCustomSlogan.empty();
    const int sloganW = showSlogan ? text_width(measure, sloganFont, g_textguiCustomSlogan) : 0;
    const int maxTextW = (std::max)({ watermarkW, sloganW, maxFeatureW });
    const float areaWf = (std::max)(185.f * scale, maxTextW + 8.f * scale);
    const float areaHf = (g_textguiShowWatermark ? 39.f * scale : 0.f)
        + (showSlogan ? 22.f * scale : 0.f)
        + static_cast<float>(features.size()) * rowH + 8.f * scale;
    const int areaW = (std::max)(1, static_cast<int>(std::ceil(areaWf + 4.f)));
    const int areaH = (std::max)(1, static_cast<int>(std::ceil(areaHf + 4.f)));
    const float margin = 18.f * scale;
    const float travelX = (std::max)(0.f, sw - areaWf - margin * 2.f);
    const float travelY = (std::max)(0.f, sh - areaHf - margin * 2.f);
    const int dstX = static_cast<int>(std::lround(margin + travelX * g_textguiX));
    const int dstY = static_cast<int>(std::lround(margin + travelY * g_textguiY));

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

        float cy = 4.f * scale;
        if (g_textguiShowWatermark) {
            const std::wstring title = L"STRIKESENSE";
            const float groupWidth = 44.f * scale + text_width(g, titleFont, title);
            const float groupX = areaWf - groupWidth;
            const Color logoColor = g_textguiRainbow
                ? color_from_hue(baseHue, alpha, g_textguiRainbowSaturation, g_textguiRainbowBrightness)
                : fixedColor;
            draw_box_logo(g, groupX + 2.f * scale, cy - 5.f * scale, scale, logoColor);
            if (g_textguiRainbow) {
                draw_rainbow_text(g, title, titleFont, groupX + 44.f * scale,
                    cy - 4.f * scale, alpha, baseHue);
            } else {
                draw_text(g, title, titleFont, groupX + 44.f * scale, cy - 4.f * scale,
                    fixedColor, alpha);
            }
            cy += 39.f * scale;
        }
        if (showSlogan) {
            const float tx = areaWf - text_width(g, sloganFont, g_textguiCustomSlogan);
            if (g_textguiRainbow) {
                draw_rainbow_text(g, g_textguiCustomSlogan, sloganFont, tx, cy,
                    alpha, baseHue + 38.f);
            } else {
                draw_text(g, g_textguiCustomSlogan, sloganFont, tx, cy, fixedColor, alpha);
            }
            cy += 22.f * scale;
        }

        for (size_t i = 0; i < features.size(); ++i) {
            const auto& feature = features[i];
            const std::wstring accessory = feature.accessory.empty() ? L"" : L" " + feature.accessory;
            const float primaryWidth = static_cast<float>(text_width(g, itemFont, feature.text));
            const float accessoryWidth = static_cast<float>(text_width(g, accessoryFont, accessory));
            const float tx = areaWf - primaryWidth - accessoryWidth;
            if (g_textguiBackdrop) {
                const BYTE backdropAlpha = static_cast<BYTE>(255.f * std::clamp(g_textguiBackdropOpacity, 0.f, 1.f));
                SolidBrush backdrop(Color(backdropAlpha, 15, 15, 15));
                const float paddingX = 6.f * scale;
                g.FillRectangle(&backdrop, tx - paddingX, cy - 2.f * scale,
                    primaryWidth + accessoryWidth + paddingX * 2.f, rowH);
            }
            if (g_textguiRainbow) draw_rainbow_text(g, feature.text, itemFont, tx, cy, alpha, baseHue + static_cast<float>(i) * 26.f);
            else draw_text(g, feature.text, itemFont, tx, cy, fixedColor, alpha);
            if (!accessory.empty()) {
                if (g_textguiRainbow) {
                    const float accessoryHue = baseHue + static_cast<float>(i) * 26.f
                        + static_cast<float>(feature.text.size()) * g_textguiRainbowSpread;
                    draw_rainbow_text(g, accessory, accessoryFont, tx + primaryWidth,
                        cy + 1.f * scale, alpha, accessoryHue);
                } else {
                    draw_text(g, accessory, accessoryFont, tx + primaryWidth, cy + 1.f * scale,
                        Color(alpha, static_cast<BYTE>(std::clamp(g_textguiAccessoryR, 0, 255)),
                            static_cast<BYTE>(std::clamp(g_textguiAccessoryG, 0, 255)),
                            static_cast<BYTE>(std::clamp(g_textguiAccessoryB, 0, 255))), alpha);
                }
            }
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
            modulenotifications::Refresh();
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
    modulenotifications::Refresh();
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
    modulenotifications::Refresh();
    if (!g_textguiEnabled) return;
    if (!has_window()) Initialize(s_hInst ? s_hInst : hInst);
    update_visibility();
}

void Shutdown()
{
    modulenotifications::Shutdown();
    if (has_window()) DestroyWindow(s_hwnd);
    s_hwnd = nullptr;
    s_visible = false;
    std::cout << "[Textgui] 已释放覆盖层。" << std::endl;
}

void RegisterCustomLine(const std::wstring& id, const std::wstring& text,
    const std::wstring& accessory)
{
    modulenotifications::RegisterCustomLine(id, text, accessory);
    std::wcout << L"[Textgui] 脚本注册文字: " << id << L" => " << text << std::endl;
    Refresh();
}

void RemoveCustomLine(const std::wstring& id)
{
    modulenotifications::RemoveCustomLine(id);
    std::wcout << L"[Textgui] 脚本移除文字: " << id << std::endl;
    Refresh();
}

void SetModuleHidden(const std::wstring& id, bool hidden)
{
    modulenotifications::SetModuleHidden(id, hidden);
    Refresh();
}

void UpdateCrosshairRecoilSignal(const std::wstring& text)
{
    modulenotifications::UpdateCrosshairRecoilSignal(text);
    Refresh();
}

} // namespace textgui_overlay
