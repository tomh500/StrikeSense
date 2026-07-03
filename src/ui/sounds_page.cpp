#include "pages.h"
#include "gsi_server.h"
#include "i18n.h"
#include <filesystem>
#include <commdlg.h>
#include <ShlObj.h>

namespace fs = std::filesystem;
static Gdiplus::RectF g_folderBtnRect;
extern Gdiplus::RectF g_itemHelperDirBtnRect; // 全局按钮点击判定区

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
    {-20, L"比赛十秒倒计时", L"lastsec.wav", &config::Settings::snd_lastsec}, 
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
    
    // 💎 核心修复：在此处统一声明基础字体，防止下方闭包内发生 sF 局部重定义
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

    // 绘制滚动列表区域
    int y = 70; const int rh = 30;
    for (int i = 0; i < SND_COUNT; ++i) {
        auto& rw = s_sounds[i];
        SolidBrush* bg = (i % 2 == 0) ? &r0 : &r1;
        g.FillRectangle(bg, cx, y, cw, rh);
        g.DrawString(rw.label, -1, &rF, PointF(cx + 8, y + 5), &tdCol);
        
        std::wstring nm = rw.defName;
        bool df = true;

        if (rw.id == -99) {
            if (!c.flash_image.empty()) {
                fs::path p(c.flash_image);
                nm = p.filename().wstring();
                df = false;
            } else {
                nm = L"flash.jpg";
            }
        } else if (rw.cfgPtr) {
            std::wstring pp = c.*(rw.cfgPtr);
            if (!pp.empty()) {
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

    // ====== 💎 4. 并排创建：打开音频文件夹 & 打开图片文件夹 💎 ======
    // 同步 Legal 页面的标准样式画刷与高光配色彩盘
    SolidBrush tb(Color(255, 20, 80, 140));    // 激活态蓝色（按钮文字）
    SolidBrush bb(Color(255, 200, 230, 250));  // 按钮浅蓝填充底色
    Pen bp(Color(255, 150, 190, 220));         // 按钮高光细外包线

    const int BW = 125, BH = 26; // 宽度控制在 125 确保并排紧凑不拥挤
    int bx1 = cx + 10;
    int bx2 = cx + 10 + BW + 10; // 黄金比例 10px 间距
    int by = H - 45;             // 固定靠在主页面最下沿

    // 绑定更新全局交互矩形
    extern Gdiplus::RectF g_folderBtnRect;       
    extern Gdiplus::RectF g_itemHelperDirBtnRect; 
    
    g_folderBtnRect = RectF((REAL)bx1, (REAL)by, (REAL)BW, (REAL)BH);
    g_itemHelperDirBtnRect = RectF((REAL)bx2, (REAL)by, (REAL)BW, (REAL)BH);

    // Lambda 闭包：像素级复刻 Legal 页面的极致圆角渲染风格
    auto DrawRoundButton = [&](const RectF& r, const wchar_t* text) {
        GraphicsPath p;
        p.AddArc(r.X, r.Y, 16.f, 16.f, 180.f, 90.f);
        p.AddArc((REAL)(r.X + r.Width - 16), r.Y, 16.f, 16.f, 270.f, 90.f);
        p.AddArc((REAL)(r.X + r.Width - 16), (REAL)(r.Y + r.Height - 16), 16.f, 16.f, 0.f, 90.f);
        p.AddArc(r.X, (REAL)(r.Y + r.Height - 16), 16.f, 16.f, 90.f, 90.f);
        p.CloseFigure();

        g.FillPath(&bb, &p); // 浅蓝圆角质感平铺
        g.DrawPath(&bp, &p); // 高光描边
        g.DrawString(text, -1, &sF, PointF((REAL)(r.X + 6), (REAL)(r.Y + 5)), &tb);
    };

    // 绘制并排双组合按钮
    DrawRoundButton(g_folderBtnRect, i18n::T("SOUNDS_FOLDER_BTN"));
    DrawRoundButton(g_itemHelperDirBtnRect, i18n::T("SOUNDS_IMAGE_BTN"));
}

void CheckSoundsClick(HWND hw, int mx, int my) {
    // 1. 响应“打开音频文件夹”按钮点击
    if (mx >= g_folderBtnRect.X && mx <= g_folderBtnRect.X + g_folderBtnRect.Width &&
        my >= g_folderBtnRect.Y && my <= g_folderBtnRect.Y + g_folderBtnRect.Height)
    {
        wchar_t userProfile[MAX_PATH] = {};
        if (GetEnvironmentVariableW(L"USERPROFILE", userProfile, MAX_PATH) > 0) {
            fs::path audioPath = fs::path(userProfile) / L"StrikeSense" / L"snd";
            std::error_code ec;
            fs::create_directories(audioPath, ec);
            ShellExecuteW(nullptr, L"open", audioPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
        return;
    }

    // 2. 响应“打开图片文件夹”按钮点击
    if (mx >= g_itemHelperDirBtnRect.X && mx <= g_itemHelperDirBtnRect.X + g_itemHelperDirBtnRect.Width &&
        my >= g_itemHelperDirBtnRect.Y && my <= g_itemHelperDirBtnRect.Y + g_itemHelperDirBtnRect.Height)
    {
        wchar_t userProfile[MAX_PATH] = {};
        if (GetEnvironmentVariableW(L"USERPROFILE", userProfile, MAX_PATH) > 0) {
            fs::path imagePath = fs::path(userProfile) / L"StrikeSense" / L"img";
            std::error_code ec;
            fs::create_directories(imagePath, ec);
            ShellExecuteW(nullptr, L"open", imagePath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
        return;
    }

    // 配置项点击处理循环
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
        
        if (r.id == -99) {
            config::Settings c = config::Load();
            c.flash_image = p;
            config::Save(c);
            InvalidateRect(hw, nullptr, FALSE);
            return;
        }
        
        config::Settings c = config::Load();
        switch (r.id) {
            case 1: c.snd_1 = p; break; 
            case 2: c.snd_2 = p; break;
            case 3: c.snd_3 = p; break; 
            case 4: c.snd_4 = p; break;
            case 5: c.snd_5 = p; break; 
            case -1: c.snd_extra = p; break;
            case -2: c.snd_mvp = p; break; 
            case -3: c.snd_win = p; break;
            case -4: c.snd_lose = p; break; 
            case -12: c.snd_bomb = p; break;
            case -13: c.snd_round = p; break; 
            case -14: c.snd_buy = p; break;
            case -18: c.snd_death = p; break; 
            case -19: c.snd_gameover = p; break;
            case -21: c.snd_menu = p; break;
            case -20: c.snd_lastsec = p; break;
        }
        config::Save(c); 
        InvalidateRect(hw, nullptr, FALSE); 
        return;
    }
}