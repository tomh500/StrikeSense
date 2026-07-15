#include "pages.h"
#include "gsi_server.h"
#include "config.h"
#include "i18n.h"

static bool s_toggleStates[7] = {false, true, false, false, false, false, false};

void PaintSettingsPage(Gdiplus::Graphics& g, int cx, int cw, int H, HWND) {
    using namespace Gdiplus;
    using namespace i18n;

    ui::DrawHeader(g, cx, cw, T(Keys::SETTINGS_TITLE));
    SolidBrush knB(Color(255, 60, 160, 230));
    Font tF(L"Microsoft YaHei", 12);
    SolidBrush tdCol(Color(255, 30, 60, 100));

    config::Settings& c = gsi::GetConfig();
    s_toggleStates[0] = c.custom_musickit;
    s_toggleStates[1] = c.enable_kill_sound;
    s_toggleStates[2] = c.force_interrupt;
    s_toggleStates[3] = c.custom_flashbang;
    s_toggleStates[4] = c.low_memory;
    s_toggleStates[5] = c.show_mvp;
    s_toggleStates[6] = c.ogg;

    const wchar_t* lbs[] = {
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
        g.DrawString(lbs[i], -1, &tF, PointF(cx + 10, (float)y), &tdCol);
        ui::DrawToggle(g, cx + cw - 60, y, s_toggleStates[i]);
        y += 36;
    }
    y += 10;
    int sw = cw - 80;

    g.DrawString(T(Keys::SETTINGS_VOL), -1, &tF,
        PointF((float)(cx + 10), (float)y), &tdCol);

    ui::DrawSlider(g, cx + 80, y, sw, c.volume);

    float kx = cx + 80 + (int)(sw * c.volume) - 8.f;
    g.FillEllipse(&knB, kx, y - 6.f, 16.f, 16.f);

    wchar_t vt[32];
    swprintf_s(vt, L"%.0f%%", c.volume * 100.f);

    g.DrawString(vt, -1, &tF,
        PointF((float)(cx + 80 + sw + 8),
            (float)(y - 4)),
        &tdCol);
}

void CheckSettingsClick(HWND hw, int mx, int my) {
    int cx = SIDEBAR_W + 12, cw = 0;
    RECT rc;
    GetClientRect(hw, &rc);
    cw = rc.right - rc.left - cx - 12;

    for (int i = 0; i < 7; ++i) {
        int tx = cx + cw - 60;
        int ty = 50 + i * 36;
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
            InvalidateRect(hw, nullptr, FALSE);
            return;
        }
    }

    float val;
    int slY = 50 + 7 * 36 + 10;
    if (ui::CheckSliderClick(mx, my, cx + 80, slY, cw - 80, val)) {
        config::Settings& c = gsi::GetConfig();
        c.volume = val;

        config::Save(c);
        gsi::RefreshConfig();
        InvalidateRect(hw, nullptr, FALSE);
    }
}
