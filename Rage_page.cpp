#include "pages.h"
#include "i18n.h"
#include "quickstop.h"
#include <fstream>
#include <nlohmann/json.hpp>

bool g_rageEnabled = false;
static Gdiplus::RectF g_RageToggleRect;

// ===== 急停 UI 状态 =====
static bool g_qsEnabled = false;
static Gdiplus::RectF g_QSToggleRect;

// 滑块范围
struct SliderParam {
    int* value;
    int minVal;
    int maxVal;
    Gdiplus::RectF rect;
};
static SliderParam g_sliders[5];
static int g_sliderCount = 0;

static void SyncQSParams()
{
    auto& cfg = GetQSConfig();
    g_qsEnabled = cfg.enabled;
}

static void SaveQSParam(int idx)
{
    auto& cfg = GetQSConfig();
    int* targets[] = { &cfg.min_pulse, &cfg.max_pulse, &cfg.cap_pulse, &cfg.move_start_at, &cfg.move_cap_at };
    if (idx >= 0 && idx < 5)
        *targets[idx] = *g_sliders[idx].value;
    SaveQuickStopConfig();
    std::cout << "[急停] 滑块 " << idx << " 已保存" << std::endl;
}

void PaintRagePage(Gdiplus::Graphics& g, int cx, int cw, int H, HWND) {
    using namespace Gdiplus;
    ui::DrawHeader(g, cx, cw, _(i18n::Keys::Rage_TITLE));
    Font rF(L"Microsoft YaHei", 11), sF(L"Microsoft YaHei", 9);

    // 暗色主题颜色
    SolidBrush rageTdDark(Color(255, 220, 80, 80));
    SolidBrush tdColDark(Color(255, 200, 220, 240));
    SolidBrush sBgDark(Color(255, 60, 60, 70));
    SolidBrush sFillDark(Color(255, 220, 80, 80));
    SolidBrush knBDark(Color(255, 240, 100, 100));
    SolidBrush offBr(Color(255, 80, 80, 90));
    SolidBrush onBr(Color(255, 220, 80, 80));

    // 启用 Rage 模式开关
    g.DrawString(L"启用 Rage 模式:", -1, &rF, PointF(cx + 10, 60), &rageTdDark);
    g_RageToggleRect = RectF((REAL)(cx + 160), (REAL)56, 50.f, 24.f);
    ui::DrawToggle(g, cx + 160, 56, g_rageEnabled);

    if (!g_rageEnabled) {
        g.DrawString(_(i18n::Keys::Rage_PLACEHOLDER), -1, &rF, PointF(cx + 10, 100), &rageTdDark);
        return;
    }

    // ===== 急停区域 =====
    int yBase = 100;

    // 标题和开关
    g.DrawString(_(i18n::Keys::Rage_QUICKSTOP), -1, &rF, PointF(cx + 10, yBase), &rageTdDark);
    g_QSToggleRect = RectF((REAL)(cx + 220), (REAL)(yBase - 4), 50.f, 24.f);
    ui::DrawToggle(g, cx + 220, yBase - 4, g_qsEnabled);

    if (!g_qsEnabled) return;

    // 同步最新值
    SyncQSParams();
    auto& cfg = GetQSConfig();

    // 5个滑块: min_pulse, max_pulse, cap_pulse, move_start_at, move_cap_at
    struct SliderDef {
        const char* label;
        int* value;
        int minV, maxV;
    };
    SliderDef defs[] = {
        { "Rage_MIN_PULSE", &cfg.min_pulse, 0, 100 },
        { "Rage_MAX_PULSE", &cfg.max_pulse, 0, 100 },
        { "Rage_CAP_PULSE", &cfg.cap_pulse, 0, 100 },
        { "Rage_MOVE_START", &cfg.move_start_at, 0, 1000 },
        { "Rage_MOVE_CAP", &cfg.move_cap_at, 0, 1000 },
    };

    int slW = cw - 280;
    if (slW < 100) slW = 100;
    g_sliderCount = 5;

    for (int i = 0; i < 5; ++i) {
        int sy = yBase + 40 + i * 40;
        const wchar_t* wlabel = i18n::T(defs[i].label);
        g.DrawString(wlabel, -1, &sF, PointF(cx + 10, sy), &tdColDark);

        int barX = cx + 160;
        float norm = (float)(*defs[i].value - defs[i].minV) / (float)(defs[i].maxV - defs[i].minV);
        if (norm < 0) norm = 0;
        if (norm > 1) norm = 1;

        // 自定义暗色滑块绘制
        g.FillRectangle(&sBgDark, barX, sy, slW, 10);
        int fw = (int)(slW * norm);
        if (fw > slW) fw = slW;
        g.FillRectangle(&sFillDark, barX, sy, fw, 10);
        float kx2 = (REAL)(barX + fw - 8.f);
        g.FillEllipse(&knBDark, kx2, (REAL)(sy - 6.f), 16.f, 16.f);

        wchar_t valT[16];
        swprintf_s(valT, L"%d", *defs[i].value);
        g.DrawString(valT, -1, &sF, PointF((REAL)(barX + slW + 8), (REAL)(sy - 2)), &tdColDark);

        // 记录滑块区域用于点击检测
        g_sliders[i].value = defs[i].value;
        g_sliders[i].minVal = defs[i].minV;
        g_sliders[i].maxVal = defs[i].maxV;
        g_sliders[i].rect = RectF((REAL)barX, (REAL)(sy - 8), (REAL)slW, 24.f);
    }

    // 说明文字
    Font xsF(L"Microsoft YaHei", 8);
    SolidBrush hintCol(Color(180, 160, 160, 170));
    g.DrawString(L"提示：当按住W/A/S/D移动时松手，自动发送反向脉冲实现急停", -1, &xsF, PointF(cx + 10, yBase + 240), &hintCol);
}

void CheckRageClick(HWND hw, int mx, int my) {
    RectF* tr = &g_RageToggleRect;
    if (mx >= tr->X && mx <= tr->X + tr->Width &&
        my >= tr->Y && my <= tr->Y + tr->Height) {
        if (!g_rageEnabled) {
            int ret = MessageBoxW(hw, 
                L"本页面的配置来自DearMacro，需要谨慎使用。\n我们不对它的安全性做保证。\n使用本页面造成的虚拟财产损失后果自负。\n\n您还要开启吗？",
                L"⚠️ 警告：Rage 模式",
                MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
            if (ret != IDYES) return;
        }
        g_rageEnabled = !g_rageEnabled;
        InvalidateRect(hw, nullptr, FALSE);
        return;
    }

    if (!g_rageEnabled) return;

    // 急停开关
    RectF* qsr = &g_QSToggleRect;
    if (mx >= qsr->X && mx <= qsr->X + qsr->Width &&
        my >= qsr->Y && my <= qsr->Y + qsr->Height) {
        g_qsEnabled = !g_qsEnabled;
        GetQSConfig().enabled = g_qsEnabled;
        SaveQuickStopConfig();
        std::cout << "[急停] 开关: " << (g_qsEnabled ? "开启" : "关闭") << std::endl;
        InvalidateRect(hw, nullptr, FALSE);
        return;
    }

    if (!g_qsEnabled) return;

    // 滑块检测
    for (int i = 0; i < g_sliderCount; ++i) {
        auto& sl = g_sliders[i];
        if (mx >= sl.rect.X && mx <= sl.rect.X + sl.rect.Width &&
            my >= sl.rect.Y && my <= sl.rect.Y + sl.rect.Height) {
            float t = (float)(mx - sl.rect.X) / sl.rect.Width;
            if (t < 0) t = 0;
            if (t > 1) t = 1;
            *sl.value = sl.minVal + (int)((sl.maxVal - sl.minVal) * t);
            SaveQSParam(i);
            InvalidateRect(hw, nullptr, FALSE);
            return;
        }
    }
}

bool IsRageModeEnabled() { return g_rageEnabled; }