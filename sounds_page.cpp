#include "pages.h"
#include "gsi_server.h"
#include "i18n.h"
#include <filesystem>
#include <commdlg.h>
#include <ShlObj.h>

namespace fs = std::filesystem;
static Gdiplus::RectF g_folderBtnRect;

struct SoundRow {
    int id;
    const wchar_t* label, * defName;
    std::wstring config::Settings::* cfgPtr;
    Gdiplus::RectF btnRect;
};
static SoundRow s_sounds[] = {
    {1, L"一杀", L"1.wav", &config::Settings::snd_1},
    {2, L"二杀", L"2.wav", &config::Settings::snd_2},
    {3, L"三杀", L"3.wav", &config::Settings::snd_3},
    {4, L"四杀", L"4.wav", &config::Settings::snd_4},
    {5, L"五杀", L"5.wav", &config::Settings::snd_5},
    {-1, L"多杀/死斗", L"deathmatch.wav", &config::Settings::snd_extra},
    {-2, L"MVP", L"mvp.wav", &config::Settings::snd_mvp},
    {-3, L"回合胜利", L"win.wav", &config::Settings::snd_win},
    {-4, L"回合失败", L"lose.wav", &config::Settings::snd_lose},
    {-13, L"回合开始", L"round.wav", &config::Settings::snd_round},
    {-14, L"购买阶段", L"buy.wav", &config::Settings::snd_buy},
    {-12, L"炸弹", L"bomb.wav", &config::Settings::snd_bomb},
    {-18, L"死亡", L"death.wav", &config::Settings::snd_death},
    {-19, L"游戏结束", L"gameover.wav", &config::Settings::snd_gameover},
    {-21, L"菜单", L"menu.wav", &config::Settings::snd_menu},
    {-99, L"闪光(可能) ", L"flash.jpg", &config::Settings::flash_image},
};
static constexpr int SND_COUNT = sizeof(s_sounds) / sizeof(s_sounds[0]);

void PaintSoundsPage(Gdiplus::Graphics& g, int cx, int cw, int H, HWND hw) {
    using namespace Gdiplus;
    using namespace i18n;
    ui::DrawHeader(g, cx, cw, _(Keys::SOUNDS_TITLE));
    Font rF(L"Microsoft YaHei", 11), bF(L"Microsoft YaHei", 9), sF(L"Microsoft YaHei", 9);
    SolidBrush tdCol(Color(255, 30, 60, 100)), tmDim(Color(255, 100, 130, 160)), tbCol(Color(255, 20, 80, 140));
    SolidBrush r0(Color(255, 220, 240, 255)), r1(Color(255, 240, 248, 255));
    SolidBrush btnB(Color(255, 140, 200, 240)), btnHB(Color(255, 60, 160, 230)), btnT(Color(255, 255, 255, 255));
    Pen btnP(Color(255, 100, 170, 220));

    config::Settings c = config::Load();
    wchar_t st[256];
    swprintf_s(st, L"GSI:%s | 音效:%s | 音乐包:%s | 低内存:%s | 音量:%.0f%%",
               gsi::IsRunning() ? L"运行中" : L"未启动",
               c.enable_kill_sound ? L"已启用" : L"已禁用",
               c.custom_musickit ? L"已启用" : L"已禁用",
               c.low_memory ? L"已启用" : L"已禁用", c.volume * 100.f);
    g.DrawString(st, -1, &sF, PointF(cx + 10, 48), &tmDim);

    // 先画声音列表
    int y = 70; const int rh = 30;
    for (int i = 0; i < SND_COUNT; ++i) {
        auto& rw = s_sounds[i];
        SolidBrush* bg = (i % 2 == 0) ? &r0 : &r1;
        g.FillRectangle(bg, cx, y, cw, rh);
        g.DrawString(rw.label, -1, &rF, PointF(cx + 8, y + 5), &tdCol);
        std::wstring nm = rw.defName;
bool df = true;

if (rw.id == -99)
{
    if (!c.flash_image.empty())
    {
        fs::path p(c.flash_image);
        nm = p.filename().wstring();
        df = false;
    }
    else
    {
        nm = L"flash.jpg";
    }
}
else if (rw.cfgPtr)
{
    std::wstring pp = c.*(rw.cfgPtr);

    if (!pp.empty())
    {
        fs::path p(pp);
        nm = p.filename().wstring();
        df = false;
    }
}
        g.DrawString(nm.c_str(), -1, &rF, PointF(cx + 160, y + 5), df ? &tmDim : &tdCol);
        int bx = cx + cw - 100, by = y + 2, bw = 80, bh = 26;
        rw.btnRect = RectF((REAL)bx, (REAL)by, (REAL)bw, (REAL)bh);
        GraphicsPath bp; bp.AddArc(bx, by, 16, 16, 180, 90);
        bp.AddArc(bx + bw - 16, by, 16, 16, 270, 90);
        bp.AddArc(bx + bw - 16, by + bh - 16, 16, 16, 0, 90);
        bp.AddArc(bx, by + bh - 16, 16, 16, 90, 90); bp.CloseFigure();
        POINT pt; GetCursorPos(&pt); ScreenToClient(hw, &pt);
        bool hv = (pt.x >= bx && pt.x <= bx + bw && pt.y >= by && pt.y <= by + bh);
        g.FillPath(hv ? &btnHB : &btnB, &bp); g.DrawPath(&btnP, &bp);
        g.DrawString(L"选择...", -1, &bF, PointF(bx + 12, by + 6), &btnT);
        y += rh + 2;
    }

    // 底部打开默认音频文件夹按钮
    int btnY = y + 10;
    g_folderBtnRect = RectF((REAL)(cx + 10), (REAL)btnY, (REAL)150, (REAL)26);
    GraphicsPath fp; fp.AddArc(cx + 10, btnY, 16, 16, 180, 90);
    fp.AddArc(cx + 10 + 150 - 16, btnY, 16, 16, 270, 90);
    fp.AddArc(cx + 10 + 150 - 16, btnY + 26 - 16, 16, 16, 0, 90);
    fp.AddArc(cx + 10, btnY + 26 - 16, 16, 16, 90, 90); fp.CloseFigure();
    g.FillPath(&btnB, &fp); g.DrawPath(&btnP, &fp);
    g.DrawString(L"打开默认音频文件夹", -1, &bF, PointF(cx + 16, btnY + 6), &tbCol);
}

void CheckSoundsClick(HWND hw, int mx, int my) {
    // 打开文件夹按钮
    if (mx >= g_folderBtnRect.X && mx <= g_folderBtnRect.X + g_folderBtnRect.Width &&
        my >= g_folderBtnRect.Y && my <= g_folderBtnRect.Y + g_folderBtnRect.Height) {
        wchar_t path[MAX_PATH];
        GetEnvironmentVariableW(L"USERPROFILE", path, MAX_PATH);
        wcscat_s(path, L"\\StrikeSense\\snd");
        ShellExecuteW(hw, L"explore", path, nullptr, nullptr, SW_SHOW);
        return;
    }
    for (int i = 0; i < SND_COUNT; ++i) {
        auto& r = s_sounds[i];
        if (mx < r.btnRect.X || mx > r.btnRect.X + r.btnRect.Width ||
            my < r.btnRect.Y || my > r.btnRect.Y + r.btnRect.Height) continue;
        wchar_t p[MAX_PATH] = {};
        OPENFILENAMEW o = {};
        o.lStructSize = sizeof(o); o.hwndOwner = hw;
        o.lpstrFilter = (r.id == -99) ? L"图片\0*.bmp;*.png;*.jpg\0All\0*.*\0" : L"音频\0*.wav;*.ogg\0All\0*.*\0";
        o.lpstrFile = p; o.nMaxFile = MAX_PATH; o.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;
        if (!GetOpenFileNameW(&o)) return;
        if (r.id == -99)
{
    config::Settings c = config::Load();

    c.flash_image = p;

    config::Save(c);

    InvalidateRect(hw, nullptr, FALSE);
    return;
}
        config::Settings c = config::Load();
        switch (r.id) {
        case 1: c.snd_1 = p; break; case 2: c.snd_2 = p; break;
        case 3: c.snd_3 = p; break; case 4: c.snd_4 = p; break;
        case 5: c.snd_5 = p; break; case -1: c.snd_extra = p; break;
        case -2: c.snd_mvp = p; break; case -3: c.snd_win = p; break;
        case -4: c.snd_lose = p; break; case -12: c.snd_bomb = p; break;
        case -13: c.snd_round = p; break; case -14: c.snd_buy = p; break;
        case -18: c.snd_death = p; break; case -19: c.snd_gameover = p; break;
        case -21: c.snd_menu = p; break;
        }
        config::Save(c); InvalidateRect(hw, nullptr, FALSE); return;
    }
}