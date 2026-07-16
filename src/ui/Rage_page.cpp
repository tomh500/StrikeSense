#include "pages.h"
#include "console_log.h"
#include "cscript.h"
#include "cscript_panel.h"
#include "i18n.h"
#include "quickstop.h"
#include "resource.h"
#include "textgui_overlay.h"
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
static Gdiplus::RectF g_LenientManualStopToggleRect;
static Gdiplus::RectF g_ConsoleLogToggleRect;
static Gdiplus::RectF g_QuickStopExpandRect;
static bool g_quickStopExpanded = false;

// 滑块区域（仅用于点击检测，值直接读写 cfg）
static constexpr int kQSSliderCount = 11;
static std::array<Gdiplus::RectF, kQSSliderCount> g_sliderRects;
static std::array<Gdiplus::RectF, kQSSliderCount> g_valueRects;
static constexpr int kQSSliderMin[kQSSliderCount] = { 0, 1, 1, 1, 1, 1, 1, 50, 10, 1, 1 };
static constexpr int kQSSliderMax[kQSSliderCount] = { 3000, 1000, 1000, 1000, 1000, 200, 1000, 1000, 1000, 500, 500 };

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

void PaintRagePage(Gdiplus::Graphics& g, int cx, int cw, int, HWND) {
    using namespace Gdiplus;
    ui::DrawHeader(g, cx, cw, _(i18n::Keys::Rage_TITLE));
    Font rF(L"Microsoft YaHei", 11), sF(L"Microsoft YaHei", 9), xsF(L"Microsoft YaHei", 8);
    SolidBrush tdCol(Color(255, 30, 60, 100));
    SolidBrush knB(Color(255, 60, 160, 230));
    SolidBrush warnCol(Color(255, 200, 80, 80));
    SolidBrush valueBackground(Color(255, 252, 254, 255));
    SolidBrush foldBackground(Color(255, 231, 244, 252));
    Pen valueBorder(Color(255, 145, 195, 225), 1.0f);
    Pen foldBorder(Color(255, 150, 200, 230));

    g.DrawString(_(i18n::Keys::Rage_WARN_NOSAVE), -1, &xsF, PointF((REAL)(cx + 10), 38.f), &warnCol);
    g.DrawString(_(i18n::Keys::Rage_ENABLE_TEXT), -1, &rF, PointF((REAL)(cx + 10), 60.f), &tdCol);
    g_RageToggleRect = RectF((REAL)(cx + 160), 56.f, 50.f, 24.f);
    ui::DrawToggle(g, cx + 160, 56, g_rageEnabled);
    if (!g_rageEnabled) return;

    const int quickStopY = 100;
    g.DrawString(_(i18n::Keys::Rage_QUICKSTOP), -1, &rF,
        PointF((REAL)(cx + 10), (REAL)quickStopY), &tdCol);
    g_QSToggleRect = RectF((REAL)(cx + 220), (REAL)(quickStopY - 4), 50.f, 24.f);
    const bool quickStopEnabled = IsQuickStopEnabled();
    ui::DrawToggle(g, cx + 220, quickStopY - 4, quickStopEnabled);
    if (quickStopEnabled) {
        g_QuickStopExpandRect = RectF((REAL)(cx + 286), (REAL)(quickStopY - 4), 24.f, 24.f);
        g.FillRectangle(&foldBackground, g_QuickStopExpandRect);
        g.DrawRectangle(&foldBorder, g_QuickStopExpandRect);
        g.DrawString(g_quickStopExpanded ? L"v" : L">", -1, &sF,
            PointF(g_QuickStopExpandRect.X + 8.f, g_QuickStopExpandRect.Y + 4.f), &tdCol);
    } else {
        g_QuickStopExpandRect = RectF{};
        g_quickStopExpanded = false;
    }

    int nextModuleY = quickStopY + 34;
    auto& cfg = GetQSConfig();
    if (quickStopEnabled && g_quickStopExpanded) {
        const int lenientY = nextModuleY;
        g.DrawString(_(i18n::Keys::Rage_LENIENT_MANUAL_STOP), -1, &sF,
            PointF((REAL)(cx + 28), (REAL)lenientY), &tdCol);
        g_LenientManualStopToggleRect = RectF((REAL)(cx + 220), (REAL)(lenientY - 4), 50.f, 24.f);
        ui::DrawToggle(g, cx + 220, lenientY - 4, cfg.lenient_manual_stop);

        struct SliderDef { const char* nameKey; int* value; int minV; int maxV; };
        SliderDef defs[] = {
            { i18n::Keys::Rage_JUMP_DISABLE_MS, &cfg.jump_disable_ms, kQSSliderMin[0], kQSSliderMax[0] },
            { i18n::Keys::Rage_MICRO_PULSE, &cfg.micro_pulse, kQSSliderMin[1], kQSSliderMax[1] },
            { i18n::Keys::Rage_MIN_PULSE, &cfg.min_pulse, kQSSliderMin[2], kQSSliderMax[2] },
            { i18n::Keys::Rage_MAX_PULSE, &cfg.max_pulse, kQSSliderMin[3], kQSSliderMax[3] },
            { i18n::Keys::Rage_CAP_PULSE, &cfg.cap_pulse, kQSSliderMin[4], kQSSliderMax[4] },
            { i18n::Keys::Rage_MICRO_MOVE, &cfg.micro_move_at, kQSSliderMin[5], kQSSliderMax[5] },
            { i18n::Keys::Rage_MOVE_START, &cfg.move_start_at, kQSSliderMin[6], kQSSliderMax[6] },
            { i18n::Keys::Rage_MOVE_CAP, &cfg.move_cap_at, kQSSliderMin[7], kQSSliderMax[7] },
            { i18n::Keys::Rage_CURVE, &cfg.curve_percent, kQSSliderMin[8], kQSSliderMax[8] },
            { i18n::Keys::Rage_HORIZONTAL_SCALE, &cfg.horizontal_scale_percent, kQSSliderMin[9], kQSSliderMax[9] },
            { i18n::Keys::Rage_VERTICAL_SCALE, &cfg.vertical_scale_percent, kQSSliderMin[10], kQSSliderMax[10] },
        };
        const int sliderWidth = (std::max)(100, cw - 280);
        const int sliderBaseY = lenientY + 40;
        for (int i = 0; i < kQSSliderCount; ++i) {
            const int sy = sliderBaseY + i * 34;
            g.DrawString(_(defs[i].nameKey), -1, &sF, PointF((REAL)(cx + 10), (REAL)sy), &tdCol);
            const int barX = cx + 160;
            const int clamped = std::clamp(*defs[i].value, defs[i].minV, defs[i].maxV);
            const float normalized = static_cast<float>(clamped - defs[i].minV)
                / static_cast<float>(defs[i].maxV - defs[i].minV);
            ui::DrawSlider(g, barX, sy, sliderWidth, normalized);
            g.FillEllipse(&knB, (REAL)(barX + (int)(sliderWidth * normalized) - 8),
                (REAL)(sy - 6), 16.f, 16.f);
            wchar_t valueText[16]{};
            swprintf_s(valueText, L"%d", *defs[i].value);
            g_valueRects[i] = RectF((REAL)(barX + sliderWidth + 6), (REAL)(sy - 10), 68.f, 22.f);
            g.FillRectangle(&valueBackground, g_valueRects[i]);
            g.DrawRectangle(&valueBorder, g_valueRects[i]);
            g.DrawString(valueText, -1, &sF,
                PointF(g_valueRects[i].X + 7.f, g_valueRects[i].Y + 2.f), &tdCol);
            g_sliderRects[i] = RectF((REAL)barX, (REAL)(sy - 8), (REAL)sliderWidth, 24.f);
        }
        nextModuleY = sliderBaseY + kQSSliderCount * 34 + 8;
    } else {
        g_LenientManualStopToggleRect = RectF{};
        for (auto& rect : g_sliderRects) rect = RectF{};
        for (auto& rect : g_valueRects) rect = RectF{};
    }

    nextModuleY = cscriptui::PaintSection(g, cx, cw, nextModuleY, nullptr);
    const int consoleLogY = nextModuleY;
    g.DrawString(_(i18n::Keys::Rage_CONSOLE_LOG), -1, &rF,
        PointF((REAL)(cx + 10), (REAL)consoleLogY), &tdCol);
    g_ConsoleLogToggleRect = RectF((REAL)(cx + 220), (REAL)(consoleLogY - 4), 50.f, 24.f);
    ui::DrawToggle(g, cx + 220, consoleLogY - 4, consolelog::IsEnabled());
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
            if (cscript::IsEnabled()) cscript::SetEnabled(true);
            if (consolelog::IsEnabled()) consolelog::SetEnabled(true);
        }
        else
        {
            StopQuickStopForRageDisabled();
            cscript::StopForRageDisabled();
            consolelog::StopForRageDisabled();
        }
        RefreshTextguiOverlay();
        InvalidateRect(hw, nullptr, FALSE);
        return;
    }

    if (!g_rageEnabled) return;

    if (cscriptui::CheckClick(hw, mx, my)) {
        return;
    }

    if (mx >= g_QuickStopExpandRect.X && mx <= g_QuickStopExpandRect.X + g_QuickStopExpandRect.Width &&
        my >= g_QuickStopExpandRect.Y && my <= g_QuickStopExpandRect.Y + g_QuickStopExpandRect.Height) {
        g_quickStopExpanded = !g_quickStopExpanded;
        std::cout << "[急停界面] 子控件已" << (g_quickStopExpanded ? "展开" : "折叠") << "。" << std::endl;
        InvalidateRect(hw, nullptr, FALSE);
        return;
    }

    RectF* clr = &g_ConsoleLogToggleRect;
    if (mx >= clr->X && mx <= clr->X + clr->Width &&
        my >= clr->Y && my <= clr->Y + clr->Height) {
        consolelog::SetEnabled(!consolelog::IsEnabled());
        std::cout << "[控制台日志] UI 请求切换读控制台支持开关，实际状态: "
                  << (consolelog::IsEnabled() ? "开启" : "关闭") << std::endl;
        RefreshTextguiOverlay();
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
        RefreshTextguiOverlay();
        InvalidateRect(hw, nullptr, FALSE);
        return;
    }

    if (!IsQuickStopEnabled() || !g_quickStopExpanded) return;

    RectF* lmsr = &g_LenientManualStopToggleRect;
    if (mx >= lmsr->X && mx <= lmsr->X + lmsr->Width &&
        my >= lmsr->Y && my <= lmsr->Y + lmsr->Height) {
        auto& cfg = GetQSConfig();
        cfg.lenient_manual_stop = !cfg.lenient_manual_stop;
        ApplyQuickStopConfigChanges();
        std::cout << "[急停] 宽容手动急停 BETA 已切换为: "
                  << (cfg.lenient_manual_stop ? "开启" : "关闭") << std::endl;
        InvalidateRect(hw, nullptr, FALSE);
        return;
    }

    // 滑块检测
    auto& cfg = GetQSConfig();
    struct { const char* nameKey; int* v; int min, max; } targets[] = {
        { i18n::Keys::Rage_JUMP_DISABLE_MS, &cfg.jump_disable_ms, kQSSliderMin[0], kQSSliderMax[0] },
        { i18n::Keys::Rage_MICRO_PULSE, &cfg.micro_pulse, kQSSliderMin[1], kQSSliderMax[1] },
        { i18n::Keys::Rage_MIN_PULSE, &cfg.min_pulse, kQSSliderMin[2], kQSSliderMax[2] },
        { i18n::Keys::Rage_MAX_PULSE, &cfg.max_pulse, kQSSliderMin[3], kQSSliderMax[3] },
        { i18n::Keys::Rage_CAP_PULSE, &cfg.cap_pulse, kQSSliderMin[4], kQSSliderMax[4] },
        { i18n::Keys::Rage_MICRO_MOVE, &cfg.micro_move_at, kQSSliderMin[5], kQSSliderMax[5] },
        { i18n::Keys::Rage_MOVE_START, &cfg.move_start_at, kQSSliderMin[6], kQSSliderMax[6] },
        { i18n::Keys::Rage_MOVE_CAP, &cfg.move_cap_at, kQSSliderMin[7], kQSSliderMax[7] },
        { i18n::Keys::Rage_CURVE, &cfg.curve_percent, kQSSliderMin[8], kQSSliderMax[8] },
        { i18n::Keys::Rage_HORIZONTAL_SCALE, &cfg.horizontal_scale_percent, kQSSliderMin[9], kQSSliderMax[9] },
        { i18n::Keys::Rage_VERTICAL_SCALE, &cfg.vertical_scale_percent, kQSSliderMin[10], kQSSliderMax[10] },
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
            *targets[i].v = static_cast<int>(std::lround(
                targets[i].min + t * (targets[i].max - targets[i].min)));
            ApplyQuickStopConfigChanges();
            std::cout << "[急停] 滑块 " << i << " = " << *targets[i].v << std::endl;
            InvalidateRect(hw, nullptr, FALSE);
            return;
        }
    }
}

bool IsRageModeEnabled() { return g_rageEnabled; }

void EnableRageModeFromLaunch(HWND hw)
{
    g_rageEnabled = true;
    std::cout << "[超频配置] 检测到 -semirage 启动参数，已直接启用超频配置总开关。" << std::endl;
    if (IsQuickStopEnabled()) SetQuickStopEnabled(true);
    if (cscript::IsEnabled()) cscript::SetEnabled(true);
    if (consolelog::IsEnabled()) consolelog::SetEnabled(true);
    RefreshTextguiOverlay();
    if (hw) InvalidateRect(hw, nullptr, FALSE);
}
