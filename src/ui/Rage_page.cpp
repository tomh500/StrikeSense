#include "pages.h"
#include "i18n.h"
#include "mouse_jitter.h"
#include "quickstop.h"
#include "resource.h"
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>

// 如果 g_hMutex 声明在其他文件，必须在此处引出
extern HANDLE g_hMutex; 

bool g_rageEnabled = false;
static Gdiplus::RectF g_RageToggleRect;

static Gdiplus::RectF g_QSToggleRect;
static Gdiplus::RectF g_MouseJitterToggleRect;

// 滑块区域（仅用于点击检测，值直接读写 cfg）
static constexpr int kQSSliderCount = 10;
static std::array<Gdiplus::RectF, kQSSliderCount> g_sliderRects;
static std::array<Gdiplus::RectF, kQSSliderCount> g_valueRects;

struct QuickStopInputContext {
    const wchar_t* label = nullptr;
    int current = 0;
    int result = 0;
    bool accepted = false;
};

static INT_PTR CALLBACK QuickStopValueDialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* context = reinterpret_cast<QuickStopInputContext*>(GetWindowLongPtrW(dialog, DWLP_USER));
    if (message == WM_INITDIALOG) {
        context = reinterpret_cast<QuickStopInputContext*>(lParam);
        SetWindowLongPtrW(dialog, DWLP_USER, reinterpret_cast<LONG_PTR>(context));
        SetDlgItemTextW(dialog, IDC_QUICKSTOP_VALUE_LABEL, context->label);
        SetDlgItemTextW(dialog, IDC_QUICKSTOP_VALUE_EDIT, std::to_wstring(context->current).c_str());
        SendDlgItemMessageW(dialog, IDC_QUICKSTOP_VALUE_EDIT, EM_SETSEL, 0, -1);
        SetFocus(GetDlgItem(dialog, IDC_QUICKSTOP_VALUE_EDIT));
        return FALSE;
    }
    if (message != WM_COMMAND || !context) return FALSE;
    if (LOWORD(wParam) == IDOK) {
        wchar_t text[128]{};
        GetDlgItemTextW(dialog, IDC_QUICKSTOP_VALUE_EDIT, text, static_cast<int>(std::size(text)));
        context->result = _wtoi(text);
        context->accepted = true;
        EndDialog(dialog, IDOK);
        return TRUE;
    }
    if (LOWORD(wParam) == IDCANCEL) {
        EndDialog(dialog, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}

static bool PromptQuickStopValue(HWND owner, const wchar_t* label, int current, int& result)
{
    QuickStopInputContext context{ label, current, current, false };
    DialogBoxParamW(hInst, MAKEINTRESOURCEW(IDD_QUICKSTOP_VALUE), owner,
        QuickStopValueDialogProc, reinterpret_cast<LPARAM>(&context));
    if (!context.accepted) return false;
    result = context.result;
    return true;
}

static float QuickStopSliderPosition(int value, int minimum, int maximum)
{
    const double clamped = std::clamp(value, minimum, maximum);
    const double linear = (clamped - minimum) / static_cast<double>(maximum - minimum);
    return static_cast<float>(std::pow(linear, 0.72));
}

static int QuickStopSliderValue(float position, int minimum, int maximum)
{
    const double linear = std::pow(std::clamp(position, 0.0f, 1.0f), 1.0 / 0.72);
    return static_cast<int>(std::lround(minimum + linear * (maximum - minimum)));
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
    SolidBrush valueBackground(Color(255, 252, 254, 255));
    Pen valueBorder(Color(255, 145, 195, 225), 1.0f);

    // 警告：Rage 模式不保存
    g.DrawString(_(i18n::Keys::Rage_WARN_NOSAVE), -1, &xsF, PointF((REAL)(cx + 10), 38.f), &warnCol);

    // 启用 Rage 模式开关
    g.DrawString(_(i18n::Keys::Rage_ENABLE_TEXT), -1, &rF, PointF((REAL)(cx + 10), 60.f), &tdCol);
    g_RageToggleRect = RectF((REAL)(cx + 160), (REAL)56, 50.f, 24.f);
    ui::DrawToggle(g, cx + 160, 56, g_rageEnabled);

    if (!g_rageEnabled) return;

    // ===== 急停区域 =====
    int yBase = 100;

    // 标题和开关
    g.DrawString(_(i18n::Keys::Rage_QUICKSTOP), -1, &rF, PointF((REAL)(cx + 10), (REAL)yBase), &tdCol);
    g_QSToggleRect = RectF((REAL)(cx + 220), (REAL)(yBase - 4), 50.f, 24.f);
    const bool quickStopEnabled = IsQuickStopEnabled();
    ui::DrawToggle(g, cx + 220, yBase - 4, quickStopEnabled);

    const int jitterY = yBase + 34;
    g.DrawString(_(i18n::Keys::Rage_MOUSE_JITTER), -1, &rF, PointF((REAL)(cx + 10), (REAL)jitterY), &tdCol);
    g_MouseJitterToggleRect = RectF((REAL)(cx + 220), (REAL)(jitterY - 4), 50.f, 24.f);
    ui::DrawToggle(g, cx + 220, jitterY - 4, mousejitter::IsEnabled());

    if (!quickStopEnabled) return;

    // 读取最新值
    auto& cfg = GetQSConfig();

    // 修复：类型完全统一为项目的底层核心 const char*
    struct SliderDef {
        const char* nameKey; 
        int* value;
        int minV, maxV;
    };
    SliderDef defs[] = {
        { i18n::Keys::Rage_MICRO_PULSE, &cfg.micro_pulse, 1, 1000 },
        { i18n::Keys::Rage_MIN_PULSE,  &cfg.min_pulse, 1, 1000 },
        { i18n::Keys::Rage_MAX_PULSE,  &cfg.max_pulse, 1, 1000 },
        { i18n::Keys::Rage_CAP_PULSE,  &cfg.cap_pulse, 1, 1000 },
        { i18n::Keys::Rage_MICRO_MOVE, &cfg.micro_move_at, 1, 5000 },
        { i18n::Keys::Rage_MOVE_START, &cfg.move_start_at, 1, 5000 },
        { i18n::Keys::Rage_MOVE_CAP,   &cfg.move_cap_at, 50, 5000 },
        { i18n::Keys::Rage_CURVE, &cfg.curve_percent, 10, 1000 },
        { i18n::Keys::Rage_HORIZONTAL_SCALE, &cfg.horizontal_scale_percent, 1, 500 },
        { i18n::Keys::Rage_VERTICAL_SCALE, &cfg.vertical_scale_percent, 1, 500 },
    };

    int slW = cw - 280;
    if (slW < 100) slW = 100;
    for (int i = 0; i < kQSSliderCount; ++i) {
        int sy = yBase + 74 + i * 34;
        const wchar_t* wlabel = _(defs[i].nameKey); 
        g.DrawString(wlabel, -1, &sF, PointF((REAL)(cx + 10), (REAL)sy), &tdCol);

        int barX = cx + 160;
        const float norm = QuickStopSliderPosition(*defs[i].value, defs[i].minV, defs[i].maxV);

        ui::DrawSlider(g, barX, sy, slW, norm);
        float kx2 = (REAL)(barX + (int)(slW * norm) - 8.f);
        g.FillEllipse(&knB, kx2, (REAL)(sy - 6.f), 16.f, 16.f);

        wchar_t valT[16];
        swprintf_s(valT, L"%d", *defs[i].value);
        g_valueRects[i] = RectF((REAL)(barX + slW + 6), (REAL)(sy - 10), 68.f, 22.f);
        g.FillRectangle(&valueBackground, g_valueRects[i]);
        g.DrawRectangle(&valueBorder, g_valueRects[i]);
        g.DrawString(valT, -1, &sF, PointF(g_valueRects[i].X + 7.0f, g_valueRects[i].Y + 2.0f), &tdCol);

        // 记录滑块区域用于点击检测
        g_sliderRects[i] = RectF((REAL)barX, (REAL)(sy - 8), (REAL)slW, 24.f);
    }

    // 触发条件说明
    const int hintY = yBase + 74 + kQSSliderCount * 34 + 8;
    g.DrawString(_(i18n::Keys::Rage_HINT_LINE1), -1, &xsF, PointF((REAL)(cx + 10), (REAL)hintY), &hintCol);
    g.DrawString(_(i18n::Keys::Rage_HINT_LINE2), -1, &xsF, PointF((REAL)(cx + 10), (REAL)(hintY + 16)), &hintCol);
    g.DrawString(_(i18n::Keys::Rage_HINT_LINE3), -1, &xsF, PointF((REAL)(cx + 10), (REAL)(hintY + 32)), &hintCol);
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
                    _(i18n::Keys::Rage_REQ_ADMIN_MSG),
                    _(i18n::Keys::Rage_REQ_ADMIN_TITLE),
                    MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
                if (ret == IDYES)
                {
                    if (g_hMutex) { CloseHandle(g_hMutex); g_hMutex = nullptr; }
                    wchar_t exePath[MAX_PATH] = {};
                    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
                    SHELLEXECUTEINFOW sei{};
                    sei.cbSize = sizeof(sei);
                    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
                    sei.lpVerb = L"runas";
                    sei.lpFile = exePath;
                    sei.nShow = SW_SHOWNORMAL;

                    if (ShellExecuteExW(&sei))
                    {
                        WaitForInputIdle(sei.hProcess, 5000);

                         DestroyWindow(hw);
                    }
                }
                return;
            }

            int ret = MessageBoxW(hw, 
                _(i18n::Keys::Rage_RISK_WARNING_MSG),
                _(i18n::Keys::Rage_RISK_WARNING_TITLE),
                MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
            if (ret != IDYES) return;
        }
        g_rageEnabled = !g_rageEnabled;
        if (g_rageEnabled)
        {
            if (IsQuickStopEnabled()) SetQuickStopEnabled(true);
            if (mousejitter::IsEnabled()) mousejitter::SetEnabled(true);
        }
        else
        {
            StopQuickStopForRageDisabled();
            mousejitter::StopForRageDisabled();
        }
        InvalidateRect(hw, nullptr, FALSE);
        return;
    }

    if (!g_rageEnabled) return;

    // 急停开关
    RectF* mjr = &g_MouseJitterToggleRect;
    if (mx >= mjr->X && mx <= mjr->X + mjr->Width &&
        my >= mjr->Y && my <= mjr->Y + mjr->Height) {
        mousejitter::SetEnabled(!mousejitter::IsEnabled());
        std::cout << "[多绑定脚本] UI 请求切换支持开关，实际状态: "
                  << (mousejitter::IsEnabled() ? "开启" : "关闭") << std::endl;
        InvalidateRect(hw, nullptr, FALSE);
        return;
    }

    RectF* qsr = &g_QSToggleRect;
    if (mx >= qsr->X && mx <= qsr->X + qsr->Width &&
        my >= qsr->Y && my <= qsr->Y + qsr->Height) {
        const bool requestedEnabled = !IsQuickStopEnabled();
        SetQuickStopEnabled(requestedEnabled);
        const bool actualEnabled = IsQuickStopEnabled();
        std::cout << "[急停] UI 请求切换开关，实际状态: "
                  << (actualEnabled ? "开启" : "关闭") << std::endl;
        InvalidateRect(hw, nullptr, FALSE);
        return;
    }

    if (!IsQuickStopEnabled()) return;

    // 滑块检测
    auto& cfg = GetQSConfig();
    struct { const char* nameKey; int* v; int min, max; } targets[] = {
        { i18n::Keys::Rage_MICRO_PULSE, &cfg.micro_pulse, 1, 1000 },
        { i18n::Keys::Rage_MIN_PULSE, &cfg.min_pulse, 1, 1000 },
        { i18n::Keys::Rage_MAX_PULSE, &cfg.max_pulse, 1, 1000 },
        { i18n::Keys::Rage_CAP_PULSE, &cfg.cap_pulse, 1, 1000 },
        { i18n::Keys::Rage_MICRO_MOVE, &cfg.micro_move_at, 1, 5000 },
        { i18n::Keys::Rage_MOVE_START, &cfg.move_start_at, 1, 5000 },
        { i18n::Keys::Rage_MOVE_CAP, &cfg.move_cap_at, 50, 5000 },
        { i18n::Keys::Rage_CURVE, &cfg.curve_percent, 10, 1000 },
        { i18n::Keys::Rage_HORIZONTAL_SCALE, &cfg.horizontal_scale_percent, 1, 500 },
        { i18n::Keys::Rage_VERTICAL_SCALE, &cfg.vertical_scale_percent, 1, 500 },
    };

    for (int i = 0; i < kQSSliderCount; ++i) {
        const auto& valueRect = g_valueRects[i];
        if (mx >= valueRect.X && mx <= valueRect.X + valueRect.Width &&
            my >= valueRect.Y && my <= valueRect.Y + valueRect.Height) {
            int entered = *targets[i].v;
            if (PromptQuickStopValue(hw, _(targets[i].nameKey), *targets[i].v, entered)) {
                *targets[i].v = entered;
                ApplyQuickStopConfigChanges();
                std::cout << "[急停] 数字输入参数 " << i << " = " << entered
                          << "（允许超出滑块范围）" << std::endl;
                InvalidateRect(hw, nullptr, FALSE);
            }
            return;
        }

        auto& r = g_sliderRects[i];
        if (mx >= r.X && mx <= r.X + r.Width &&
            my >= r.Y && my <= r.Y + r.Height) {
            float t = (float)(mx - r.X) / r.Width;
            if (t < 0) t = 0;
            if (t > 1) t = 1;
            *targets[i].v = QuickStopSliderValue(t, targets[i].min, targets[i].max);
            ApplyQuickStopConfigChanges();
            std::cout << "[急停] 滑块 " << i << " = " << *targets[i].v << std::endl;
            InvalidateRect(hw, nullptr, FALSE);
            return;
        }
    }
}

bool IsRageModeEnabled() { return g_rageEnabled; }
