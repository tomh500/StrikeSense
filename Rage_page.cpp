#include "pages.h"
#include "i18n.h"
#include "quickstop.h"
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>

bool g_rageEnabled = false;
static Gdiplus::RectF g_RageToggleRect;

// ===== 急停 UI 状态 =====
static bool g_qsEnabled = false;
static Gdiplus::RectF g_QSToggleRect;

// 滑块区域（仅用于点击检测，值直接读写 cfg）
static Gdiplus::RectF g_sliderRects[5];
static int g_sliderCount = 0;

static void SyncQSParams()
{
    auto& cfg = GetQSConfig();
    g_qsEnabled = cfg.enabled;
}

static void SaveQSParam(int idx)
{
    SaveQuickStopConfig();
    std::cout << "[急停] 滑块 " << idx << " 已保存" << std::endl;
}

void PaintRagePage(Gdiplus::Graphics& g, int cx, int cw, int H, HWND) {
    using namespace Gdiplus;
    ui::DrawHeader(g, cx, cw, _(i18n::Keys::Rage_TITLE));
    Font rF(L"Microsoft YaHei", 11), sF(L"Microsoft YaHei", 9);
    Font xsF(L"Microsoft YaHei", 8);

    // 水蓝色主题
    SolidBrush tdCol(Color(255, 30, 60, 100));
    SolidBrush knB(Color(255, 60, 160, 230));
    SolidBrush warnCol(Color(255, 200, 80, 80));
    SolidBrush hintCol(Color(180, 100, 130, 160));

    // 警告：Rage 模式不保存
    g.DrawString(L"⚠ 由于供应商要求，Rage 模式启用状态不保存，每次启动程序必须手动启用", -1, &xsF, PointF(cx + 10, 38), &warnCol);

    // 启用 Rage 模式开关
    g.DrawString(L"启用 Rage 模式:", -1, &rF, PointF(cx + 10, 60), &tdCol);
    g_RageToggleRect = RectF((REAL)(cx + 160), (REAL)56, 50.f, 24.f);
    ui::DrawToggle(g, cx + 160, 56, g_rageEnabled);

    if (!g_rageEnabled) return;

    // ===== 急停区域 =====
    int yBase = 100;

    // 标题和开关
    g.DrawString(_(i18n::Keys::Rage_QUICKSTOP), -1, &rF, PointF(cx + 10, yBase), &tdCol);
    g_QSToggleRect = RectF((REAL)(cx + 220), (REAL)(yBase - 4), 50.f, 24.f);
    ui::DrawToggle(g, cx + 220, yBase - 4, g_qsEnabled);

    if (!g_qsEnabled) return;

    // 读取最新值
    SyncQSParams();
    auto& cfg = GetQSConfig();

    // 5个滑块: min_pulse, max_pulse, cap_pulse, move_start_at, move_cap_at
    struct SliderDef {
        const char* label;
        int* value;
        int minV, maxV;
    };
    SliderDef defs[] = {
        { "Rage_MIN_PULSE", &cfg.min_pulse, 1, 300 },
        { "Rage_MAX_PULSE", &cfg.max_pulse, 1, 300 },
        { "Rage_CAP_PULSE", &cfg.cap_pulse, 1, 300 },
        { "Rage_MOVE_START", &cfg.move_start_at, 1, 3000 },
        { "Rage_MOVE_CAP", &cfg.move_cap_at, 1, 3000 },
    };

    int slW = cw - 280;
    if (slW < 100) slW = 100;
    g_sliderCount = 5;

    for (int i = 0; i < 5; ++i) {
        int sy = yBase + 40 + i * 40;
        const wchar_t* wlabel = i18n::T(defs[i].label);
        g.DrawString(wlabel, -1, &sF, PointF(cx + 10, sy), &tdCol);

        int barX = cx + 160;
        float norm = (float)(*defs[i].value - defs[i].minV) / (float)(defs[i].maxV - defs[i].minV);
        if (norm < 0) norm = 0;
        if (norm > 1) norm = 1;

        ui::DrawSlider(g, barX, sy, slW, norm);
        float kx2 = (REAL)(barX + (int)(slW * norm) - 8.f);
        g.FillEllipse(&knB, kx2, (REAL)(sy - 6.f), 16.f, 16.f);

        wchar_t valT[16];
        swprintf_s(valT, L"%d", *defs[i].value);
        g.DrawString(valT, -1, &sF, PointF((REAL)(barX + slW + 8), (REAL)(sy - 2)), &tdCol);

        // 记录滑块区域用于点击检测
        g_sliderRects[i] = RectF((REAL)barX, (REAL)(sy - 8), (REAL)slW, 24.f);
    }

    // 触发条件说明
    g.DrawString(L"触发条件：按下 W / A / S / D 后松手 → 自动发送反向键（例如松W按S，松A按D）", -1, &xsF, PointF(cx + 10, yBase + 240), &hintCol);
    g.DrawString(L"脉冲时长根据按住时长线性插值（起始→封顶），支持 Shift/Ctrl 静默跳过", -1, &xsF, PointF(cx + 10, yBase + 256), &hintCol);
}

void CheckRageClick(HWND hw, int mx, int my) {
    using namespace Gdiplus;
    RectF* tr = &g_RageToggleRect;
    if (mx >= tr->X && mx <= tr->X + tr->Width &&
        my >= tr->Y && my <= tr->Y + tr->Height) {
        if (!g_rageEnabled) {
            // 检查管理员权限
            BOOL isElevated = FALSE;
            HANDLE hToken = nullptr;
            if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
            {
                TOKEN_ELEVATION te;
                DWORD size = sizeof(te);
                if (GetTokenInformation(hToken, TokenElevation, &te, size, &size))
                    isElevated = te.TokenIsElevated;
                CloseHandle(hToken);
            }

            if (!isElevated)
            {
                int ret = MessageBoxW(hw,
                    L"Rage 模式需要管理员权限才能正常工作。\n是否重新以管理员身份启动程序？",
                    L"⚠️ 权限不足",
                    MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
                if (ret == IDYES)
                {
                    // 重新以管理员身份拉起
                    wchar_t exePath[MAX_PATH] = {};
                    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
                    ShellExecuteW(nullptr, L"runas", exePath, nullptr, nullptr, SW_SHOWNORMAL);
                    PostQuitMessage(0);
                }
                return;
            }

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
        if (g_qsEnabled)
            StartQuickStopHook();
        else
            StopQuickStopHook();
        std::cout << "[急停] 开关: " << (g_qsEnabled ? "开启" : "关闭") << std::endl;
        InvalidateRect(hw, nullptr, FALSE);
        return;
    }

    if (!g_qsEnabled) return;

    // 滑块检测 - 直接修改 cfg 并即时保存
    auto& cfg = GetQSConfig();
    struct { int* v; int min, max; } targets[] = {
        { &cfg.min_pulse, 1, 300 },
        { &cfg.max_pulse, 1, 300 },
        { &cfg.cap_pulse, 1, 300 },
        { &cfg.move_start_at, 1, 3000 },
        { &cfg.move_cap_at, 1, 3000 },
    };

    for (int i = 0; i < g_sliderCount; ++i) {
        auto& r = g_sliderRects[i];
        if (mx >= r.X && mx <= r.X + r.Width &&
            my >= r.Y && my <= r.Y + r.Height) {
            float t = (float)(mx - r.X) / r.Width;
            if (t < 0) t = 0;
            if (t > 1) t = 1;
            *targets[i].v = targets[i].min + (int)((targets[i].max - targets[i].min) * t);
            SaveQuickStopConfig();
            std::cout << "[急停] 滑块 " << i << " = " << *targets[i].v << std::endl;
            InvalidateRect(hw, nullptr, FALSE);
            return;
        }
    }
}

bool IsRageModeEnabled() { return g_rageEnabled; }