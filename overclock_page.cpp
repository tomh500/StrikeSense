#include "pages.h"

void PaintOverclockPage(Gdiplus::Graphics& g, int cx, int cw, int H, HWND) {
    using namespace Gdiplus;
    ui::DrawHeader(g, cx, cw, L"超频配置");
    Font rF(L"Microsoft YaHei", 11);
    SolidBrush tdCol(Color(255, 30, 60, 100));
    g.DrawString(L"功能开发中...", -1, &rF, PointF(cx + 10, 60), &tdCol);
}

void CheckOverclockClick(HWND, int, int) {}