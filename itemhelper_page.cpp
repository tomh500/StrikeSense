#include "pages.h"
#include "StrikeSense.h"

Gdiplus::RectF g_itemHelperToggleRect;
extern bool g_itemHelperEnabled;
void PaintItemHelperPage(
    Gdiplus::Graphics& g,
    int cx,
    int cw,
    int H,
    HWND)
{
    using namespace Gdiplus;

    ui::DrawHeader(g, cx, cw, L"道具助手");

    Font rF(L"Microsoft YaHei", 11);
    Font sF(L"Microsoft YaHei", 9);

    SolidBrush tdCol(Color(255, 30, 60, 100));

    // 启用开关
    g.DrawString(
        L"启用道具助手",
        -1,
        &rF,
        PointF((float)(cx + 10), 60.f),
        &tdCol);

    g_itemHelperToggleRect =
        RectF((REAL)(cx + 130),
              56.f,
              50.f,
              24.f);

    ui::DrawToggle(
        g,
        cx + 130,
        56,
        g_itemHelperEnabled);

    // 未启用时不渲染后续内容
    if (!g_itemHelperEnabled)
        return;

    // ===== 后续控件全部放这里 =====

    g.DrawString(
        L"功能开发中...",
        -1,
        &rF,
        PointF((float)(cx + 10), 110.f),
        &tdCol);
}

void CheckItemHelperClick(HWND hw, int mx, int my)
{
    if (ui::CheckToggleClick(
        mx,
        my,
        (int)g_itemHelperToggleRect.X,
        (int)g_itemHelperToggleRect.Y))
    {
        g_itemHelperEnabled =
            !g_itemHelperEnabled;

        SaveEvolutionParams();
        UpdateHotkey::UpdateItemHelperHotkey(hw);
        InvalidateRect(
            hw,
            nullptr,
            FALSE);

        return;
    }

    if (!g_itemHelperEnabled)
        return;

    // 后续控件点击逻辑
}