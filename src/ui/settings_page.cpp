#include "pages.h"
#include "gsi_server.h"
#include "config.h"
#include "i18n.h"
#include "textgui_overlay.h"
#include "ui_theme.h"

static bool s_toggleStates[7] = {false, true, false, false, false, false, false};
static Gdiplus::RectF s_themePresetRects[uitheme::preset_count];

void PaintSettingsPage(Gdiplus::Graphics& g, int cx, int cw, int, HWND)
{
    using namespace Gdiplus;
    using namespace i18n;
    const auto& theme = uitheme::get_palette();

    ui::DrawHeader(g, cx, cw, T(Keys::SETTINGS_TITLE));
    SolidBrush knob(theme.accent_strong);
    SolidBrush text(theme.text);
    SolidBrush dim(theme.dim);
    SolidBrush button(theme.button_background);
    SolidBrush selected(theme.card_selected_background);
    Pen border(theme.button_border, 1.0f);
    Font titleFont(L"Microsoft YaHei", 12);
    Font smallFont(L"Microsoft YaHei", 9);

    config::Settings& c = gsi::GetConfig();
    s_toggleStates[0] = c.custom_musickit;
    s_toggleStates[1] = c.enable_kill_sound;
    s_toggleStates[2] = c.force_interrupt;
    s_toggleStates[3] = c.custom_flashbang;
    s_toggleStates[4] = c.low_memory;
    s_toggleStates[5] = c.show_mvp;
    s_toggleStates[6] = c.ogg;

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
        ui::DrawToggle(g, cx + cw - 60, y, s_toggleStates[i]);
        y += 36;
    }
    y += 10;
    const int sw = cw - 80;

    g.DrawString(T(Keys::SETTINGS_VOL), -1, &titleFont,
        PointF(static_cast<REAL>(cx + 10), static_cast<REAL>(y)), &text);
    ui::DrawSlider(g, cx + 80, y, sw, c.volume);

    const float kx = cx + 80 + static_cast<int>(sw * c.volume) - 8.f;
    g.FillEllipse(&knob, kx, static_cast<REAL>(y - 6), 16.f, 16.f);

    wchar_t vt[32]{};
    swprintf_s(vt, L"%.0f%%", c.volume * 100.f);
    g.DrawString(vt, -1, &titleFont,
        PointF(static_cast<REAL>(cx + 80 + sw + 8), static_cast<REAL>(y - 4)), &text);

    const int sectionY = y + 52;
    g.DrawString(L"程序主题", -1, &titleFont, PointF(static_cast<REAL>(cx + 10), static_cast<REAL>(sectionY)), &text);
    g.DrawString(L"纯颜色组", -1, &smallFont, PointF(static_cast<REAL>(cx + 10), static_cast<REAL>(sectionY + 26)), &dim);
    g.DrawString(L"高级视觉组", -1, &smallFont, PointF(static_cast<REAL>(cx + 10), static_cast<REAL>(sectionY + 98)), &dim);

    for (int i = 0; i < uitheme::preset_count; ++i) {
        const bool advanced = uitheme::is_advanced_preset(i);
        const int groupIndex = advanced ? (i - uitheme::preset_default_plus) : i;
        const int row = groupIndex / 3;
        const int col = groupIndex % 3;
        const int startY = advanced ? sectionY + 120 : sectionY + 48;
        const int x = cx + 10 + col * 132;
        const int boxY = startY + row * 34;
        s_themePresetRects[i] = RectF(static_cast<REAL>(x), static_cast<REAL>(boxY), 120.f, 26.f);
        g.FillRectangle(i == g_uiThemePreset ? &selected : &button, s_themePresetRects[i]);
        g.DrawRectangle(&border, s_themePresetRects[i]);
        g.DrawString(uitheme::get_preset_name(i), -1, &smallFont,
            PointF(static_cast<REAL>(x + 8), static_cast<REAL>(boxY + 5)), &text);
    }
}

void CheckSettingsClick(HWND hw, int mx, int my)
{
    int cx = SIDEBAR_W + 12;
    RECT rc{};
    GetClientRect(hw, &rc);
    const int cw = rc.right - rc.left - cx - 12;

    for (int i = 0; i < 7; ++i) {
        const int tx = cx + cw - 60;
        const int ty = 50 + i * 36;
        if (ui::CheckToggleClick(mx, my, tx, ty)) {
            s_toggleStates[i] = !s_toggleStates[i];

            config::Settings& c = gsi::GetConfig();
            c.custom_musickit = s_toggleStates[0];
            c.enable_kill_sound = s_toggleStates[1];
            c.force_interrupt = s_toggleStates[2];
            c.custom_flashbang = s_toggleStates[3];
            c.low_memory = s_toggleStates[4];
            c.show_mvp = s_toggleStates[5];
            c.ogg = s_toggleStates[6];

            config::Save(c);
            gsi::RefreshConfig();
            RefreshTextguiOverlay();
            InvalidateRect(hw, nullptr, FALSE);
            return;
        }
    }

    float val = 0.0f;
    const int slY = 50 + 7 * 36 + 10;
    if (ui::CheckSliderClick(mx, my, cx + 80, slY, cw - 80, val)) {
        config::Settings& c = gsi::GetConfig();
        c.volume = val;
        config::Save(c);
        gsi::RefreshConfig();
        InvalidateRect(hw, nullptr, FALSE);
        return;
    }

    for (int i = 0; i < uitheme::preset_count; ++i) {
        const auto& rect = s_themePresetRects[i];
        if (mx >= rect.X && mx <= rect.X + rect.Width &&
            my >= rect.Y && my <= rect.Y + rect.Height) {
            uitheme::set_preset(i);
            SaveEvolutionParams();
            InvalidateRect(hw, nullptr, FALSE);
            return;
        }
    }
}
