#include "pages.h"

namespace ui {
    using namespace Gdiplus;

    void DrawHeader(Graphics& g, int cx, int cw, const wchar_t* title) {
        SolidBrush hdrBg(Color(255, 180, 220, 245));
        SolidBrush tbCol(Color(255, 20, 80, 140));
        Font pF(L"Microsoft YaHei", 13, FontStyleBold);
        g.FillRectangle(&hdrBg, cx, 8, cw, 34);
        g.DrawString(title, -1, &pF, PointF(cx + 10, 14), &tbCol);
    }

    void DrawToggle(Graphics& g, int tx, int ty, bool state) {
        SolidBrush onBr(Color(255, 80, 180, 240));  // 水蓝色
        SolidBrush offBr(Color(255, 180, 180, 190));
        SolidBrush kBr(Color(255, 255, 255, 255));
        RectF tr((REAL)tx, (REAL)ty, 50.f, 24.f);
        GraphicsPath tp;
        tp.AddArc(tx, ty, 24, 24, 90, 180);
        tp.AddArc(tx + 50 - 24, ty, 24, 24, 270, 180);
        tp.CloseFigure();
        g.FillPath(state ? &onBr : &offBr, &tp);
        float kx = state ? tx + 50 - 22.f : tx + 2.f;
        g.FillEllipse(&kBr, kx, ty + 2.f, 20.f, 20.f);
    }

    void DrawSlider(Graphics& g, int sx, int sy, int sw, float value) {
        SolidBrush sBg(Color(255, 200, 220, 240));
        SolidBrush sFill(Color(255, 80, 180, 240));
        g.FillRectangle(&sBg, sx, sy, sw, 10);
        int fw = (int)(sw * value);
        if (fw > sw) fw = sw;
        g.FillRectangle(&sFill, sx, sy, fw, 10);
    }

    bool CheckToggleClick(int mx, int my, int tx, int ty) {
        return mx >= tx && mx <= tx + 50 && my >= ty && my <= ty + 24;
    }

    bool CheckSliderClick(int mx, int my, int sx, int sy, int sw, float& outVal) {
        if (mx >= sx && mx <= sx + sw && my >= sy - 8 && my <= sy + 12) {
            float t = (float)(mx - sx) / (float)sw;
            if (t < 0) t = 0;
            if (t > 1) t = 1;
            outVal = t;
            return true;
        }
        return false;
    }
}