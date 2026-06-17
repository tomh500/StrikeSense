#include "pages.h"
#include "volume_mixer.h"
#include "i18n.h" // 确保包含了国际化头文件
#include <fstream>
#include <nlohmann/json.hpp>
#include <thread>
#include <atomic>

namespace fs = std::filesystem;

// 假设这些是来自全局或其他头文件的外部声明，保持你原有的逻辑不变
extern HINSTANCE hInst; 
extern HANDLE g_hMutex;

static std::wstring GetEvolutionConfigPath() {
    wchar_t p[MAX_PATH] = {};
    GetEnvironmentVariableW(L"USERPROFILE", p, MAX_PATH);
    return fs::path(p) / L"StrikeSense" / L"setting" / L"evolution.json";
}

void SaveEvolutionParams() {
    nlohmann::json j;
    j["death_vol"] = g_death_vol; j["death_mute"] = g_deathMute;
    j["langCN"] = g_langCN;
    j["hotkey_mod"] = g_hotkeyMod; j["hotkey_vk"] = g_hotkeyVk;
    j["crosshair_enabled"] = g_crosshairEnabled;
    j["crosshair_r"] = g_crosshairR; j["crosshair_g"] = g_crosshairG; j["crosshair_b"] = g_crosshairB;
    j["crosshair_style"] = g_crosshairStyle;
    j["crosshair_thickness"] = g_crosshairThickness; j["crosshair_scale"] = g_crosshairScale;
    std::ofstream out(GetEvolutionConfigPath());
    if (out.is_open()) { out << j.dump(2); out.close(); }
}

void LoadEvolutionParams() {
    fs::path p(GetEvolutionConfigPath());
    if (!fs::exists(p)) return;
    try {
        std::ifstream in(p); if (!in.is_open()) return;
        nlohmann::json j; in >> j; in.close();
        auto gv = [&](const char* k, auto& v) { if (j.contains(k) && j[k].is_number()) v = j[k].get<std::remove_reference_t<decltype(v)>>(); };
        auto gb = [&](const char* k, bool& v) { if (j.contains(k) && j[k].is_boolean()) v = j[k]; };
        gv("death_vol", g_death_vol); gb("death_mute", g_deathMute);
        gv("hotkey_mod", g_hotkeyMod); gv("hotkey_vk", g_hotkeyVk);
        gb("crosshair_enabled", g_crosshairEnabled);
        gv("crosshair_r", g_crosshairR); gv("crosshair_g", g_crosshairG); gv("crosshair_b", g_crosshairB);
        gv("crosshair_style", g_crosshairStyle); gv("crosshair_thickness", g_crosshairThickness);
        gv("crosshair_scale", g_crosshairScale);
        gb("langCN", g_langCN);
    } catch (...) {}
}

// ===== 准星线程 =====
static HWND g_crossHWnd = nullptr;
static std::thread g_crossThread;
static bool g_crossThreadRunning = false;

static LRESULT CALLBACK CrosshairWndProc(HWND hw, UINT m, WPARAM wp, LPARAM lp) {
    switch (m) {
    case WM_CLOSE: DestroyWindow(hw); return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hw, &ps);
        RECT rc; GetClientRect(hw, &rc);
        int W = rc.right - rc.left, H = rc.bottom - rc.top;
        HDC md = CreateCompatibleDC(hdc); HBITMAP mb = CreateCompatibleBitmap(hdc, W, H);
        HBITMAP ob = (HBITMAP)SelectObject(md, mb);
        using namespace Gdiplus;
        Graphics gx(md); gx.SetSmoothingMode(SmoothingModeAntiAlias);
        int cx = W / 2, cy = H / 2;
        Color crCol(255, (BYTE)g_crosshairR, (BYTE)g_crosshairG, (BYTE)g_crosshairB);
        Pen crPen(crCol, (REAL)g_crosshairThickness); float sz = 20.f * g_crosshairScale;
        if (g_crosshairStyle == 0) {
            gx.DrawEllipse(&crPen, cx - sz, cy - sz, sz * 2, sz * 2);
            gx.DrawLine(&crPen, (REAL)(cx - sz - 4), (REAL)cy, (REAL)(cx - sz + 1), (REAL)cy);
            gx.DrawLine(&crPen, (REAL)(cx + sz - 1), (REAL)cy, (REAL)(cx + sz + 4), (REAL)cy);
            gx.DrawLine(&crPen, (REAL)cx, (REAL)(cy - sz - 4), (REAL)cx, (REAL)(cy - sz + 1));
            gx.DrawLine(&crPen, (REAL)cx, (REAL)(cy + sz - 1), (REAL)cx, (REAL)(cy + sz + 4));
        } else if (g_crosshairStyle == 1) {
            SolidBrush crBr(crCol); gx.FillEllipse(&crBr, cx - sz, cy - sz, sz * 2, sz * 2);
        } else {
            gx.DrawLine(&crPen, cx - 6, cy, cx + 6, cy);
            gx.DrawLine(&crPen, cx, cy - 6, cx, cy + 6);
            gx.DrawEllipse(&crPen, cx - 1.5f, cy - 1.5f, 3.f, 3.f);
        }
        BitBlt(hdc, 0, 0, W, H, md, 0, 0, SRCCOPY);
        SelectObject(md, ob); DeleteObject(mb); DeleteDC(md);
        EndPaint(hw, &ps); return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProc(hw, m, wp, lp);
}

static void StartCrosshair(HINSTANCE hInst) {
    if (g_crossThreadRunning) return;
    g_crossThreadRunning = true;
    g_crossThread = std::thread([hInst]() {
        WNDCLASSEXW wc = {}; wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = CrosshairWndProc; wc.hInstance = hInst;
        wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
        wc.lpszClassName = L"StrikeSense_Crosshair";
        RegisterClassExW(&wc);
        int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
        HWND cw = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
            L"StrikeSense_Crosshair", L"", WS_POPUP, 0, 0, sw, sh, nullptr, nullptr, hInst, nullptr);
        if (!cw) { g_crossThreadRunning = false; return; }
        SetLayeredWindowAttributes(cw, RGB(0, 0, 0), 0, LWA_COLORKEY);
        ShowWindow(cw, SW_SHOW); UpdateWindow(cw); g_crossHWnd = cw;
        MSG m; while (GetMessage(&m, nullptr, 0, 0)) { TranslateMessage(&m); DispatchMessage(&m); }
        g_crossHWnd = nullptr; g_crossThreadRunning = false;
    });
    g_crossThread.detach();
}

static void StopCrosshair() {
    if (g_crossHWnd) { PostMessage(g_crossHWnd, WM_CLOSE, 0, 0); int wc = 0; while (g_crossHWnd && wc < 50) { Sleep(50); wc++; } }
    g_crossHWnd = nullptr; g_crossThreadRunning = false;
}

void DestroyCrosshairInternal() { StopCrosshair(); }

// ===== UI 绘制层 =====
void PaintEvolutionPage(Gdiplus::Graphics& g, int cx, int cw, int H, HWND) {
    using namespace Gdiplus;
    using namespace i18n; // 注入国际化空间以识别 Keys::

    // 1. 标题
    ui::DrawHeader(g, cx, cw, _(Keys::EVO_TITLE));
    
    Font rF(L"Microsoft YaHei", 11), sF(L"Microsoft YaHei", 9);
    SolidBrush tdCol(Color(255, 30, 60, 100)), tbCol(Color(255, 20, 80, 140)), tmDim(Color(255, 100, 130, 160));
    SolidBrush sBg(Color(255, 200, 220, 240)), sFill(Color(255, 80, 180, 240)), knB(Color(255, 60, 160, 230));
    SolidBrush ddBg(Color(255, 220, 240, 255)), ddHoverBg(Color(255, 180, 220, 245));

    // 2. 音量调节器区域
    g.DrawString(_(Keys::EVO_VOL_ADJ), -1, &rF, PointF((float)(cx + 10), 50.f), &tdCol);
    int yVolSlider = 80; int slW = cw - 100;
    ui::DrawSlider(g, cx + 10, yVolSlider, slW, g_death_vol);
    float kx = cx + 10 + (int)(slW * g_death_vol) - 8.f;
    g.FillEllipse(&knB, kx, yVolSlider - 6.f, 16.f, 16.f);
    wchar_t vt[32]; swprintf_s(vt, L"%.0f%%", g_death_vol * 100.f);
    g.DrawString(vt, -1, &rF, PointF((float)(cx + 20 + slW), (float)(yVolSlider - 8)), &tdCol);

    // 3. 开关与长提示语
    std::wstring muteLabel = std::wstring(_(Keys::EVO_STATUS_MUTED)) + L":";
    g.DrawString(muteLabel.c_str(), -1, &sF, PointF((float)(cx + 10), 115.f), &tdCol);
    
    g_deathMuteToggleRect = RectF((REAL)(cx + 80), (REAL)111, 50.f, 24.f);
    ui::DrawToggle(g, cx + 80, 111, g_deathMute);

    g.DrawString(_(Keys::EVO_HINT_MUTE), -1, &sF, PointF((float)(cx + 140), 115.f), &tmDim);

    // 4. 快捷键区域（组合国际化）
    std::wstring keyName;
    if (g_hotkeyMod & MOD_CONTROL) keyName += L"Ctrl+";
    if (g_hotkeyMod & MOD_ALT)     keyName += L"Alt+";
    if (g_hotkeyMod & MOD_SHIFT)   keyName += L"Shift+";
    if (g_hotkeyVk >= 'A' && g_hotkeyVk <= 'Z')     keyName += (wchar_t)g_hotkeyVk;
    else if (g_hotkeyVk >= '0' && g_hotkeyVk <= '9') keyName += (wchar_t)g_hotkeyVk;
    else { wchar_t b[16]; swprintf_s(b, L"Vk=%d", g_hotkeyVk); keyName += b; }

    SolidBrush disabledCol(Color(180, 150, 150, 160));
    wchar_t hs[128];
    swprintf_s(hs, L"%s: %s %s", _(Keys::EVO_HOTKEY), keyName.c_str(), _(Keys::EVO_STATUS_DISABLED));
    g.DrawString(hs, -1, &rF, PointF((float)(cx + 10), 150.f), &disabledCol);
    {
        Gdiplus::Pen kp(Color(80, 150, 150, 160));
        Gdiplus::RectF keyRect((REAL)(cx + 10), 172.f, 200.f, 20.f);
        g.DrawRectangle(&kp, keyRect);
        g.DrawString(_(Keys::EVO_LOCK_VIEW), -1, &sF, PointF((float)(cx + 14), 174.f), &disabledCol);
    }

    // 5. 准星设置区域
    int yRgb = 235, yRow2 = 270;
    g.DrawString(_(Keys::EVO_CROSSHAIR), -1, &rF, PointF((float)(cx + 10), 205.f), &tdCol);
    int rgbLabelX = cx + 10; int rgbBarW = 80, rgbSpacing = 150;
    const wchar_t* rgbL[] = { L"R", L"G", L"B" };
    int* rgbV[] = { &g_crosshairR, &g_crosshairG, &g_crosshairB };
    for (int i = 0; i < 3; ++i) {
        int bx = rgbLabelX + i * rgbSpacing;
        g.DrawString(rgbL[i], -1, &sF, PointF((float)bx, (float)yRgb), &tdCol);
        ui::DrawSlider(g, bx + 20, yRgb, rgbBarW, (float)(*rgbV[i]) / 255.f);
        wchar_t bf[8]; swprintf_s(bf, L"%d", *rgbV[i]); g.DrawString(bf, -1, &sF, PointF((float)(bx + 105), (float)(yRgb - 2)), &tdCol);
    }

    // 6. 粗细与缩放
    int thX = cx + 10; 
    std::wstring thickLabel = std::wstring(_(Keys::EVO_THICKNESS)) + L":";
    g.DrawString(thickLabel.c_str(), -1, &sF, PointF((float)thX, (float)yRow2), &tdCol);
    
    int thBarX = thX + 40, thBarW = 80;
    ui::DrawSlider(g, thBarX, yRow2, thBarW, g_crosshairThickness / 10.f);
    wchar_t thT[8]; swprintf_s(thT, L"%d", g_crosshairThickness); g.DrawString(thT, -1, &sF, PointF((float)(thBarX + thBarW + 4), (float)(yRow2 - 2)), &tdCol);

    int scX = thBarX + thBarW + 40; 
    std::wstring scaleLabel = std::wstring(_(Keys::EVO_SCALE)) + L":";
    g.DrawString(scaleLabel.c_str(), -1, &sF, PointF((float)scX, (float)yRow2), &tdCol);
    
    int scBarX = scX + 40, scBarW = 100;
    ui::DrawSlider(g, scBarX, yRow2, scBarW, (g_crosshairScale - 0.1f) / 0.5f);
    wchar_t scT[8]; swprintf_s(scT, L"%.2f", g_crosshairScale); g.DrawString(scT, -1, &sF, PointF((float)(scBarX + scBarW + 4), (float)(yRow2 - 2)), &tdCol);

    // 7. 下拉样式选择（核心映射化）
    int styX = scBarX + scBarW + 40; 
    std::wstring styleLabel = std::wstring(_(Keys::EVO_STYLE)) + L":";
    g.DrawString(styleLabel.c_str(), -1, &sF, PointF((float)styX, (float)yRow2), &tdCol);
    
    int ddX = styX + 40, ddW = 100;
    const wchar_t* sty[] = { 
        _(Keys::EVO_STYLE_HOLLOW), 
        _(Keys::EVO_STYLE_SOLID), 
        _(Keys::EVO_STYLE_CLASSIC) 
    };
    {
        SolidBrush ddBtn(Color(255, 180, 220, 250)); Pen ddPen(Color(255, 100, 150, 200));
        RectF ddRect((REAL)ddX, (REAL)(yRow2 - 2), (REAL)ddW, 20.f);
        g.FillRectangle(&ddBtn, ddRect); g.DrawRectangle(&ddPen, ddRect);
        g.DrawString(sty[g_crosshairStyle], -1, &sF, PointF((REAL)ddX + 4, (REAL)yRow2), &tdCol);
        SolidBrush arr(Color(255, 30, 60, 100));
        PointF arrPts[] = { PointF((REAL)(ddX + ddW - 8), (REAL)(yRow2 + 4)), PointF((REAL)(ddX + ddW), (REAL)(yRow2 + 4)), PointF((REAL)(ddX + ddW - 4), (REAL)(yRow2 + 12)) };
        g.FillPolygon(&arr, arrPts, 3);
        if (g_styleDropdownOpen) {
            for (int j = 0; j < 3; ++j) {
                RectF optRect((REAL)ddX, (REAL)(yRow2 + 16 + j * 18), (REAL)ddW, 18.f);
                g_dropdownRects[j] = optRect;
                g.FillRectangle((j == g_dropdownSelection) ? &ddHoverBg : &ddBg, optRect);
                g.DrawRectangle(&ddPen, optRect);
                g.DrawString(sty[j], -1, &sF, PointF((REAL)ddX + 4, (REAL)(yRow2 + 18 + j * 18)), &tdCol);
            }
        }
    }

    // 8. 启用
    int enableX = ddX + ddW + 40; 
    g.DrawString(_(Keys::EVO_ENABLE), -1, &sF, PointF((float)enableX, (float)yRow2), &tdCol);
    ui::DrawToggle(g, enableX + 40, yRow2 - 4, g_crosshairEnabled);
}

// ===== UI 点击事件层 =====
void CheckEvolutionClick(HWND hw, int mx, int my) {
    int cx = SIDEBAR_W + 12, cw = 0;
    RECT rc; GetClientRect(hw, &rc); cw = rc.right - rc.left - cx - 12;
    int yVolSlider = 80, yRgb = 235, yRow2 = 270; int slW = cw - 100; float val;
    if (ui::CheckSliderClick(mx, my, cx + 10, yVolSlider, slW, val)) {
        g_death_vol = val;
        SaveEvolutionParams();
        if (g_deathMute) {
            SetCS2VolumeReduction(g_death_vol);
            if (!IsCS2VolumeActive()) StartCS2VolumeControl(g_death_vol);
        }
        InvalidateRect(hw, nullptr, FALSE);
        return;
    }
    if (ui::CheckToggleClick(mx, my, (int)g_deathMuteToggleRect.X, (int)g_deathMuteToggleRect.Y)) {
        BOOL isElevated = FALSE; HANDLE hToken = nullptr;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
            TOKEN_ELEVATION te; DWORD size = sizeof(te);
            if (GetTokenInformation(hToken, TokenElevation, &te, size, &size)) isElevated = te.TokenIsElevated;
            CloseHandle(hToken);
        }
        if (!isElevated) {
            int ret = MessageBoxW(hw,
                L"音量降低器需要管理员权限才能正常工作。\n是否重新以管理员身份启动程序？",
                L"⚠️ 权限不足",
                MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
            if (ret == IDYES) {
                if (g_hMutex) { CloseHandle(g_hMutex); g_hMutex = nullptr; }
                wchar_t exePath[MAX_PATH] = {};
                GetModuleFileNameW(nullptr, exePath, MAX_PATH);
                ShellExecuteW(nullptr, L"runas", exePath, nullptr, nullptr, SW_SHOWNORMAL);
                PostQuitMessage(0);
            }
            return;
        }
        g_deathMute = !g_deathMute;
        SaveEvolutionParams();
        if (g_deathMute) StartCS2VolumeControl(g_death_vol);
        else StopCS2VolumeControl();
        InvalidateRect(hw, nullptr, FALSE);
        return;
    }
    int rgbLabelX = cx + 10, rgbBarW = 80, rgbSpacing = 150;
    for (int i = 0; i < 3; ++i) {
        int bx = rgbLabelX + i * rgbSpacing;
        int* rgbV[] = { &g_crosshairR, &g_crosshairG, &g_crosshairB };
        if (ui::CheckSliderClick(mx, my, bx + 20, yRgb, rgbBarW, val)) { *rgbV[i] = (int)(val * 255.f); SaveEvolutionParams(); InvalidateRect(hw, nullptr, FALSE); return; }
    }
    int thBarX = cx + 10 + 40, thBarW = 80;
    if (ui::CheckSliderClick(mx, my, thBarX, yRow2, thBarW, val)) { g_crosshairThickness = 1 + (int)(val * 9.f); SaveEvolutionParams(); InvalidateRect(hw, nullptr, FALSE); return; }
    int scBarX = thBarX + thBarW + 40 + 40, scBarW = 100;
    if (ui::CheckSliderClick(mx, my, scBarX, yRow2, scBarW, val)) { g_crosshairScale = 0.1f + val * 0.5f; SaveEvolutionParams(); InvalidateRect(hw, nullptr, FALSE); return; }
    int ddX = scBarX + scBarW + 40 + 40, ddW = 100;
    if (my >= yRow2 - 2 && my <= yRow2 + 18 && mx >= ddX && mx <= ddX + ddW) { g_styleDropdownOpen = !g_styleDropdownOpen; InvalidateRect(hw, nullptr, FALSE); return; }
    if (g_styleDropdownOpen) {
        for (int j = 0; j < 3; ++j) {
            if (mx >= ddX && mx <= ddX + ddW && my >= yRow2 + 16 + j * 18 && my <= yRow2 + 34 + j * 18) {
                g_crosshairStyle = j; g_styleDropdownOpen = false; SaveEvolutionParams(); InvalidateRect(hw, nullptr, FALSE); return;
            }
        }
        g_styleDropdownOpen = false; InvalidateRect(hw, nullptr, FALSE); return;
    }
    int enableX = ddX + ddW + 40, tx = enableX + 40, tye = yRow2 - 4;
    if (ui::CheckToggleClick(mx, my, tx, tye)) {
        g_crosshairEnabled = !g_crosshairEnabled; SaveEvolutionParams();
        // 这里修正了宏调用或局部 hInst 未声明的情况，直接使用全局/外层的 hInst
        if (g_crosshairEnabled && !g_crossThreadRunning) StartCrosshair(hInst);
        else if (!g_crosshairEnabled) StopCrosshair();
        InvalidateRect(hw, nullptr, FALSE);
    }
}