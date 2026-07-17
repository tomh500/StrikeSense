#include "pages.h"
#include "gsi_server.h"
#include "config.h"
#include "i18n.h"
#include "textgui_overlay.h"
#include "ui_theme.h"

#include <algorithm>

namespace {

bool g_toggleStates[7] = { false, true, false, false, false, false, false };
Gdiplus::RectF g_themePresetRects[uitheme::preset_count];
Gdiplus::RectF g_basicThemeFoldRect;
Gdiplus::RectF g_advancedThemeFoldRect;
bool g_basicThemeExpanded = true;
bool g_advancedThemeExpanded = true;

bool hit(const Gdiplus::RectF& rect, int x, int y)
{
    return x >= rect.X && x <= rect.X + rect.Width
        && y >= rect.Y && y <= rect.Y + rect.Height;
}

void draw_theme_group(Gdiplus::Graphics& g, int cx, const wchar_t* title,
    Gdiplus::RectF& foldRect, bool expanded, int first, int last, int& y)
{
    using namespace Gdiplus;
    const auto& theme = uitheme::get_palette();
    Font titleFont(L"Microsoft YaHei", 12);
    SolidBrush text(theme.text);

    g.DrawString(title, -1, &titleFont, PointF(static_cast<REAL>(cx + 10), static_cast<REAL>(y)), &text);
    foldRect = RectF(static_cast<REAL>(cx + 134), static_cast<REAL>(y - 4), 24.0f, 24.0f);
    ui::DrawFoldButton(g, foldRect, expanded);
    y += 32;

    if (!expanded) return;

    for (int i = first; i <= last; ++i) {
        const int groupIndex = i - first;
        const int row = groupIndex / 3;
        const int col = groupIndex % 3;
        const int x = cx + 10 + col * 132;
        const int boxY = y + row * 34;
        g_themePresetRects[i] = RectF(static_cast<REAL>(x), static_cast<REAL>(boxY), 120.0f, 26.0f);
        ui::DrawRoundedButton(g, g_themePresetRects[i], uitheme::get_preset_name(i), i == g_uiThemePreset, true);
    }

    y += ((last - first + 3) / 3) * 34 + 10;
}

} // namespace

void PaintSettingsPage(Gdiplus::Graphics& g, int cx, int cw, int, HWND)
{
    using namespace Gdiplus;
    using namespace i18n;
    const auto& theme = uitheme::get_palette();

    ui::DrawHeader(g, cx, cw, T(Keys::SETTINGS_TITLE));
    SolidBrush text(theme.text);
    Font titleFont(L"Microsoft YaHei", 12);

    config::Settings& c = gsi::GetConfig();
    g_toggleStates[0] = c.custom_musickit;
    g_toggleStates[1] = c.enable_kill_sound;
    g_toggleStates[2] = c.force_interrupt;
    g_toggleStates[3] = c.custom_flashbang;
    g_toggleStates[4] = c.low_memory;
    g_toggleStates[5] = c.show_mvp;
    g_toggleStates[6] = c.ogg;

    const wchar_t* labels[] = {
        T(Keys::SETTINGS_CUSTOM_MUSIC),
        T(Keys::SETTINGS_KILL_SOUND),
        T(Keys::SETTINGS_FORCE_INTERRUPT),
        T(Keys::SETTINGS_FLASH),
        T(Keys::SETTINGS_LOWMEM),
        T(Keys::SETTINGS_MVP),
        T(Keys::SETTINGS_OGG)
    };

    int y = 50;
    for (int i = 0; i < 7; ++i) {
        g.DrawString(labels[i], -1, &titleFont, PointF(static_cast<REAL>(cx + 10), static_cast<REAL>(y)), &text);
        ui::DrawToggle(g, cx + cw - 60, y, g_toggleStates[i]);
        y += 36;
    }

    y += 10;
    const int sliderWidth = cw - 80;
    g.DrawString(T(Keys::SETTINGS_VOL), -1, &titleFont,
        PointF(static_cast<REAL>(cx + 10), static_cast<REAL>(y)), &text);
    ui::DrawSliderWithKnob(g, cx + 80, y, sliderWidth, c.volume);

    wchar_t volumeText[32]{};
    swprintf_s(volumeText, L"%.0f%%", c.volume * 100.0f);
    g.DrawString(volumeText, -1, &titleFont,
        PointF(static_cast<REAL>(cx + 80 + sliderWidth + 8), static_cast<REAL>(y - 4)), &text);
}

void CheckSettingsClick(HWND hw, int mx, int my)
{
    const int cx = SIDEBAR_W + 12;
    RECT rc{};
    GetClientRect(hw, &rc);
    const int cw = rc.right - rc.left - cx - 12;

    for (int i = 0; i < 7; ++i) {
        const int tx = cx + cw - 60;
        const int ty = 50 + i * 36;
        if (ui::CheckToggleClick(mx, my, tx, ty)) {
            g_toggleStates[i] = !g_toggleStates[i];

            config::Settings& c = gsi::GetConfig();
            c.custom_musickit = g_toggleStates[0];
            c.enable_kill_sound = g_toggleStates[1];
            c.force_interrupt = g_toggleStates[2];
            c.custom_flashbang = g_toggleStates[3];
            c.low_memory = g_toggleStates[4];
            c.show_mvp = g_toggleStates[5];
            c.ogg = g_toggleStates[6];

            config::Save(c);
            gsi::RefreshConfig();
            RefreshTextguiOverlay();
            InvalidateRect(hw, nullptr, FALSE);
            return;
        }
    }

    float value = 0.0f;
    const int sliderY = 50 + 7 * 36 + 10;
    if (ui::CheckSliderClick(mx, my, cx + 80, sliderY, cw - 80, value)) {
        config::Settings& c = gsi::GetConfig();
        c.volume = value;
        config::Save(c);
        gsi::RefreshConfig();
        InvalidateRect(hw, nullptr, FALSE);
    }
}

void PaintProgramSettingsPage(Gdiplus::Graphics& g, int cx, int cw, int, HWND)
{
    using namespace Gdiplus;
    const auto& theme = uitheme::get_palette();

    ui::DrawHeader(g, cx, cw, L"\u7a0b\u5e8f\u8bbe\u7f6e");
    Font titleFont(L"Microsoft YaHei", 12);
    Font smallFont(L"Microsoft YaHei", 9);
    SolidBrush text(theme.text);
    SolidBrush dim(theme.dim);

    for (auto& rect : g_themePresetRects) rect = RectF{};

    int y = 58;
    g.DrawString(L"\u7a0b\u5e8f\u4e3b\u9898", -1, &titleFont,
        PointF(static_cast<REAL>(cx + 10), static_cast<REAL>(y)), &text);
    g.DrawString(L"\u9009\u62e9\u540e\u7acb\u5373\u5e94\u7528\u5230\u6240\u6709\u9875\u9762\u3002", -1, &smallFont,
        PointF(static_cast<REAL>(cx + 120), static_cast<REAL>(y + 2)), &dim);
    y += 42;

    draw_theme_group(g, cx, L"\u7eaf\u989c\u8272\u7ec4", g_basicThemeFoldRect, g_basicThemeExpanded,
        uitheme::preset_default, uitheme::preset_spring_green, y);
    draw_theme_group(g, cx, L"\u9ad8\u7ea7\u89c6\u89c9\u7ec4", g_advancedThemeFoldRect, g_advancedThemeExpanded,
        uitheme::preset_default_plus, uitheme::preset_count - 1, y);
}

void CheckProgramSettingsClick(HWND hw, int mx, int my)
{
    if (hit(g_basicThemeFoldRect, mx, my)) {
        g_basicThemeExpanded = !g_basicThemeExpanded;
        InvalidateRect(hw, nullptr, FALSE);
        return;
    }
    if (hit(g_advancedThemeFoldRect, mx, my)) {
        g_advancedThemeExpanded = !g_advancedThemeExpanded;
        InvalidateRect(hw, nullptr, FALSE);
        return;
    }

    for (int i = 0; i < uitheme::preset_count; ++i) {
        if (!hit(g_themePresetRects[i], mx, my)) continue;
        uitheme::set_preset(i);
        SaveEvolutionParams();
        RefreshTextguiOverlay();
        InvalidateRect(hw, nullptr, FALSE);
        return;
    }
}
