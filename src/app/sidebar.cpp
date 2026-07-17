#include "pages.h"
#include "i18n.h"
#include "ui_theme.h"

const int SIDEBAR_W = 140;

const SidebarItem g_sidebarItems[] = {
    {L"文件位置", PAGE_SOUNDS, 52},
    {L"遗产核心", PAGE_SETTINGS, 82},
    {L"进化分支", PAGE_EVOLUTION, 112},
    {L"合法配置", PAGE_LEGALCFG, 142},
    {L"超频配置", PAGE_Rage, 172},
    {L"道具助手", PAGE_ITEMHELPER, 202},
    {L"自定义脚本", PAGE_VSCRIPT, 232},
};

void PaintSidebar(Gdiplus::Graphics& g, int, int H)
{
    using namespace Gdiplus;
    const auto& theme = uitheme::get_palette();
    SolidBrush bg(theme.sidebar_background);
    Pen ln(theme.sidebar_border, 2.0f);
    Font tF(L"Microsoft YaHei", 16, FontStyleBold);
    Font nF(L"Microsoft YaHei", 12);
    Font nAF(L"Microsoft YaHei", 12, FontStyleBold);
    Font sF(L"Microsoft YaHei", 9);
    SolidBrush tb(theme.title);
    SolidBrush td(theme.text);
    SolidBrush naB(theme.sidebar_selected_background);
    SolidBrush onBr(theme.accent);
    SolidBrush offBr(theme.toggle_off);
    SolidBrush kBr(theme.toggle_knob);

    g.FillRectangle(&bg, 0, 0, SIDEBAR_W, H);
    g.DrawLine(&ln, SIDEBAR_W, 0, SIDEBAR_W, H);
    g.DrawString(L"StrikeSense", -1, &tF, PointF(4, 12), &tb);

    const wchar_t* labels[] = {
        i18n::T(i18n::Keys::SIDEBAR_FILE),
        i18n::T(i18n::Keys::SIDEBAR_SETTINGS),
        i18n::T(i18n::Keys::SIDEBAR_EVOLUTION),
        i18n::T(i18n::Keys::SIDEBAR_LEGAL),
        i18n::T(i18n::Keys::SIDEBAR_Rage),
        i18n::T(i18n::Keys::SIDEBAR_ITEMHELPER),
        i18n::T("SIDEBAR_VSCRIPT")
    };
    int idx = 0;
    for (const auto& it : g_sidebarItems) {
        if (g_currentPage == it.page) g.FillRectangle(&naB, 8, it.y, SIDEBAR_W - 16, 24);
        Font& f = (g_currentPage == it.page) ? nAF : nF;
        g.DrawString(labels[idx], -1, &f, PointF(14, static_cast<REAL>(it.y + 3)),
            g_currentPage == it.page ? &tb : &td);
        ++idx;
    }

    const int langY = H - 40;
    const int ltx = SIDEBAR_W / 2 - 30;
    g.DrawString(g_langCN ? L"中文" : L"EN(BETA)", -1, &sF, PointF(10, static_cast<REAL>(langY + 2)), &td);
    GraphicsPath tp;
    tp.AddArc(ltx, langY, 24, 24, 90, 180);
    tp.AddArc(ltx + 26, langY, 24, 24, 270, 180);
    tp.CloseFigure();
    g.FillPath(g_langCN ? &onBr : &offBr, &tp);
    const float kkx = g_langCN ? ltx + 28.f : ltx + 2.f;
    g.FillEllipse(&kBr, kkx, static_cast<REAL>(langY + 2), 20.f, 20.f);
}

void CheckSidebarClick(HWND hw, int mx, int my)
{
    for (const auto& it : g_sidebarItems) {
        if (mx >= 8 && mx <= SIDEBAR_W && my >= it.y && my <= it.y + 24) {
            g_currentPage = it.page;
            g_styleDropdownOpen = false;
            if (hw) InvalidateRect(hw, nullptr, FALSE);
            return;
        }
    }
    if (!hw) return;
    RECT rc{};
    GetClientRect(hw, &rc);
    const int langY = rc.bottom - rc.top - 40;
    const int ltx = SIDEBAR_W / 2 - 30;
    if (mx >= ltx && mx <= ltx + 50 && my >= langY && my <= langY + 24) {
        i18n::Switch();
        SaveEvolutionParams();
        InvalidateRect(hw, nullptr, FALSE);
    }
}
