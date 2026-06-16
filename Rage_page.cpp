#include "pages.h"
#include "i18n.h"

bool g_rageEnabled = false;
static Gdiplus::RectF g_RageToggleRect;

void PaintRagePage(Gdiplus::Graphics& g, int cx, int cw, int H, HWND) {
    using namespace Gdiplus;
    ui::DrawHeader(g, cx, cw, _(i18n::Keys::Rage_TITLE));
    Font rF(L"Microsoft YaHei", 11), sF(L"Microsoft YaHei", 9);
    SolidBrush tdCol(Color(255, 30, 60, 100));

    g.DrawString(L"启用 Rage 模式:", -1, &rF, PointF(cx + 10, 60), &tdCol);
    g_RageToggleRect = RectF((REAL)(cx + 160), (REAL)56, 50.f, 24.f);
    ui::DrawToggle(g, cx + 160, 56, g_rageEnabled);

    if (!g_rageEnabled) {
        g.DrawString(_(i18n::Keys::Rage_PLACEHOLDER), -1, &rF, PointF(cx + 10, 100), &tdCol);
        return;
    }

    SolidBrush rageTd(Color(255, 200, 50, 50));
    g.DrawString(L"Rage 配置功能开发中...", -1, &rF, PointF(cx + 10, 100), &rageTd);
}

void CheckRageClick(HWND hw, int mx, int my) {
    if (mx >= g_RageToggleRect.X && mx <= g_RageToggleRect.X + g_RageToggleRect.Width &&
        my >= g_RageToggleRect.Y && my <= g_RageToggleRect.Y + g_RageToggleRect.Height) {
        if (!g_rageEnabled) {
            int ret = MessageBoxW(hw, 
                L"本页面的配置来自DearMacro，需要谨慎使用。\n我们不对它的安全性做保证。\n使用本页面造成的虚拟财产损失后果自负。\n\n您还要开启吗？",
                L"⚠️ 警告：Rage 模式",
                MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
            if (ret != IDYES) return;
        }
        g_rageEnabled = !g_rageEnabled;
        InvalidateRect(hw, nullptr, FALSE);
    }
}

bool IsRageModeEnabled() { return g_rageEnabled; }