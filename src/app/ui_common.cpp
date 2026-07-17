#include "pages.h"
#include "ui_theme.h"

namespace ui {
using namespace Gdiplus;

void DrawHeader(Graphics& g, int cx, int cw, const wchar_t* title)
{
    const auto& theme = uitheme::get_palette();
    SolidBrush hdrBg(theme.header_background);
    SolidBrush tbCol(theme.title);
    Font pF(L"Microsoft YaHei", 13, FontStyleBold);
    g.FillRectangle(&hdrBg, cx, 8, cw, 34);
    g.DrawString(title, -1, &pF, PointF(static_cast<REAL>(cx + 10), 14), &tbCol);
}

void DrawToggle(Graphics& g, int tx, int ty, bool state)
{
    const auto& theme = uitheme::get_palette();
    SolidBrush onBr(theme.accent);
    SolidBrush offBr(theme.toggle_off);
    SolidBrush kBr(theme.toggle_knob);
    RectF tr(static_cast<REAL>(tx), static_cast<REAL>(ty), 50.f, 24.f);
    GraphicsPath tp;
    tp.AddArc(tx, ty, 24, 24, 90, 180);
    tp.AddArc(tx + 26, ty, 24, 24, 270, 180);
    tp.CloseFigure();
    g.FillPath(state ? &onBr : &offBr, &tp);
    const float kx = state ? tx + 28.f : tx + 2.f;
    g.FillEllipse(&kBr, kx, static_cast<REAL>(ty + 2), 20.f, 20.f);
}

void DrawSlider(Graphics& g, int sx, int sy, int sw, float value)
{
    const auto& theme = uitheme::get_palette();
    SolidBrush sBg(theme.slider_background);
    SolidBrush sFill(theme.accent);
    g.FillRectangle(&sBg, sx, sy, sw, 10);
    int fw = static_cast<int>(sw * value);
    if (fw > sw) fw = sw;
    g.FillRectangle(&sFill, sx, sy, fw, 10);
}

bool CheckToggleClick(int mx, int my, int tx, int ty)
{
    return mx >= tx && mx <= tx + 50 && my >= ty && my <= ty + 24;
}

bool CheckSliderClick(int mx, int my, int sx, int sy, int sw, float& outVal)
{
    if (mx >= sx && mx <= sx + sw && my >= sy - 8 && my <= sy + 12) {
        float t = static_cast<float>(mx - sx) / static_cast<float>(sw);
        if (t < 0) t = 0;
        if (t > 1) t = 1;
        outVal = t;
        return true;
    }
    return false;
}

} // namespace ui
