#include "pages.h"

static bool s_toggleStates[6] = {false, true, false, false, false, false};

void PaintSettingsPage(Gdiplus::Graphics& g, int cx, int cw, int H, HWND) {
    using namespace Gdiplus;
    ui::DrawHeader(g, cx, cw, L"遗产核心");
    Font tF(L"Microsoft YaHei", 12);
    SolidBrush tdCol(Color(255, 30, 60, 100));

    config::Settings c = config::Load();
    s_toggleStates[0] = c.custom_musickit; s_toggleStates[1] = c.enable_kill_sound;
    s_toggleStates[2] = c.custom_flashbang; s_toggleStates[3] = c.low_memory;
    s_toggleStates[4] = c.show_mvp; s_toggleStates[5] = c.ogg;

    const wchar_t* lbs[] = {L"自定义音乐包", L"击杀音效替换", L"闪光叠加", L"低内存模式", L"MVP信息板", L"OGG格式"};
    int y = 50;
    for (int i = 0; i < 6; ++i) {
        g.DrawString(lbs[i], -1, &tF, PointF(cx + 10, y), &tdCol);
        ui::DrawToggle(g, cx + cw - 60, y, s_toggleStates[i]);
        y += 36;
    }
    y += 10; int sw = cw - 80;
    g.DrawString(L"音量", -1, &tF, PointF(cx + 10, y), &tdCol);
    ui::DrawSlider(g, cx + 80, y, sw, c.volume);
    wchar_t vt[32]; swprintf_s(vt, L"%.0f%%", c.volume * 100.f);
    g.DrawString(vt, -1, &tF, PointF(cx + 80 + sw + 8, y - 4), &tdCol);
}

void CheckSettingsClick(HWND hw, int mx, int my) {
    int cx = SIDEBAR_W + 12, cw = 0;
    RECT rc; GetClientRect(hw, &rc); cw = rc.right - rc.left - cx - 12;
    for (int i = 0; i < 6; ++i) {
        int tx = cx + cw - 60, ty = 50 + i * 36;
        if (ui::CheckToggleClick(mx, my, tx, ty)) {
            s_toggleStates[i] = !s_toggleStates[i];
            config::Settings c = config::Load();
            c.custom_musickit = s_toggleStates[0]; c.enable_kill_sound = s_toggleStates[1];
            c.custom_flashbang = s_toggleStates[2]; c.low_memory = s_toggleStates[3];
            c.show_mvp = s_toggleStates[4]; c.ogg = s_toggleStates[5];
            config::Save(c); InvalidateRect(hw, nullptr, FALSE); return;
        }
    }
    float val;
    int slY = 50 + 6 * 36 + 10;
    if (ui::CheckSliderClick(mx, my, cx + 80, slY, cw - 80, val)) {
        config::Settings c = config::Load(); c.volume = val;
        config::Save(c); InvalidateRect(hw, nullptr, FALSE);
    }
}