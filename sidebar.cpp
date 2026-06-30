#include "pages.h"
#include "i18n.h"

const int SIDEBAR_W = 140;

const SidebarItem g_sidebarItems[] = {
    {L"文件位置",      PAGE_SOUNDS,     52},
    {L"遗产核心",      PAGE_SETTINGS,   82},
    {L"进化分支",      PAGE_EVOLUTION,  112},
    {L"合法配置",      PAGE_LEGALCFG,   142},
    {L"超频配置",      PAGE_Rage,  172},
    {L"道具助手",      PAGE_ITEMHELPER, 202},
    {L"自定脚本",      PAGE_VSCRIPT, 232},
};

void PaintSidebar(Gdiplus::Graphics& g, int W, int H) {
    using namespace Gdiplus;
    SolidBrush bg(Color(255, 200, 230, 250));
    Pen ln(Color(255, 160, 210, 240), 2.0f);
    Font tF(L"Microsoft YaHei", 16, FontStyleBold);
    Font nF(L"Microsoft YaHei", 12);
    Font nAF(L"Microsoft YaHei", 12, FontStyleBold);
    Font sF(L"Microsoft YaHei", 9);
    SolidBrush tb(Color(255, 20, 80, 140));
    SolidBrush td(Color(255, 30, 60, 100));
    SolidBrush naB(Color(255, 160, 210, 245));
    SolidBrush onBr(Color(255, 80, 180, 240));  // 水蓝色
    SolidBrush offBr(Color(255, 180, 180, 190));
    SolidBrush kBr(Color(255, 255, 255, 255));

    g.FillRectangle(&bg, 0, 0, SIDEBAR_W, H);
    g.DrawLine(&ln, SIDEBAR_W, 0, SIDEBAR_W, H);
    g.DrawString(L"StrikeSense", -1, &tF, PointF(4, 12), &tb);

    const wchar_t* labels[] = {i18n::T(i18n::Keys::SIDEBAR_FILE), i18n::T(i18n::Keys::SIDEBAR_SETTINGS), i18n::T(i18n::Keys::SIDEBAR_EVOLUTION), i18n::T(i18n::Keys::SIDEBAR_LEGAL), i18n::T(i18n::Keys::SIDEBAR_Rage), i18n::T(i18n::Keys::SIDEBAR_ITEMHELPER), L"自定脚本"};
    int idx = 0;
    for (auto& it : g_sidebarItems) {
        if (g_currentPage == it.page)
            g.FillRectangle(&naB, 8, it.y, SIDEBAR_W - 16, 24);
        Font& f = (g_currentPage == it.page) ? nAF : nF;
        g.DrawString(labels[idx], -1, &f, PointF(14, it.y + 3),
                     g_currentPage == it.page ? &tb : &td);
        idx++;
    }

    // 语言切换底部
    int langY = H - 40;
    int ltx = SIDEBAR_W / 2 - 30;
    g.DrawString(g_langCN ? L"中文" : L"EN(BETA)", -1, &sF, PointF(10, langY + 2), &td);
    RectF tr((REAL)ltx, (REAL)langY, 50.f, 24.f);
    GraphicsPath tp;
    tp.AddArc(ltx, langY, 24, 24, 90, 180);
    tp.AddArc(ltx + 50 - 24, langY, 24, 24, 270, 180);
    tp.CloseFigure();
    g.FillPath(g_langCN ? &onBr : &offBr, &tp);
    float kkx = g_langCN ? ltx + 50 - 22.f : ltx + 2.f;
    g.FillEllipse(&kBr, kkx, langY + 2.f, 20.f, 20.f);
}

// 传入 hw 的重载版本，由 WndProc 调用
void CheckSidebarClick(HWND hw, int mx, int my) {
    for (auto& it : g_sidebarItems) {
        if (mx >= 8 && mx <= SIDEBAR_W && my >= it.y && my <= it.y + 24) {
            g_currentPage = it.page;
            g_styleDropdownOpen = false;
            if (hw) InvalidateRect(hw, nullptr, FALSE);
            return;
        }
    }
    // 语言切换 — 检测精确区域（开关 x=40~90, y=H-40~H-16）
    if (!hw) return;
    RECT rc;
    GetClientRect(hw, &rc);
    int langY = rc.bottom - rc.top - 40;
    int ltx = SIDEBAR_W / 2 - 30; // 40
    if (mx >= ltx && mx <= ltx + 50 && my >= langY && my <= langY + 24) {
        i18n::Switch(); // 切换语言
        SaveEvolutionParams(); // 保存到 evolution.json
        InvalidateRect(hw, nullptr, FALSE);
    }
}
