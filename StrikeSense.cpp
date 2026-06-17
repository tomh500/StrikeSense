// StrikeSense.cpp — 程序入口
#include "framework.h"
#include "StrikeSense.h"
#include "console.h"
#include "steam_helper.h"
#include "gsi_server.h"
#include "config.h"
#include "sound_player.h"
#include "antistupid.h"
#include "i18n.h"
#include "quickstop.h"
#include "volume_mixer.h"
#include <iostream>
#include <filesystem>
#include <ShlObj.h>
#include <gdiplus.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include <exception>
#include "flashoverlay.h"
#include "normalgen.h"
#include "Hotkey.h"

#pragma comment(lib, "gdiplus.lib")

namespace fs = std::filesystem;
#define MAX_LOADSTRING 100

// ===== 全局变量定义 =====
HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING], szWindowClass[MAX_LOADSTRING];
Console g_Console;
std::wstring g_gsiCfgPath;
static ULONG_PTR g_gdiToken = 0;
HANDLE g_hMutex = nullptr; // 全局互斥锁句柄（用于管理员提权时释放）

int g_currentPage = 0;
bool g_langCN = true;
bool g_styleDropdownOpen = false;
int g_dropdownSelection = -1;
Gdiplus::RectF g_dropdownRects[3];

float g_death_vol = 0.8f;
bool  g_deathMute = false;
int   g_hotkeyMod = MOD_CONTROL;
int   g_hotkeyVk = 'M';
bool  g_hotkeyWaiting = false;
Gdiplus::RectF g_deathMuteToggleRect;

bool  g_crosshairEnabled = false;
int   g_crosshairR = 255, g_crosshairG = 0, g_crosshairB = 0;
int   g_crosshairStyle = 0;
int   g_crosshairThickness = 2;
float g_crosshairScale = 0.2f;

bool g_itemHelperEnabled = false;   //道具助手开关
int  g_itemHelperHotkeyMod = 0;
int  g_itemHelperHotkeyVk  = VK_HOME;



// ===== 前向声明 =====
ATOM MyRegisterClass(HINSTANCE);
HWND InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK ConfirmPathDlgProc(HWND, UINT, WPARAM, LPARAM);
static std::wstring GetCS2CfgPath();
static void OnCreateGSIConfig(HWND);
static void PaintAll(HWND, HDC);



static void LogTerminate()
{
    try
    {
        if (auto ep = std::current_exception()) {
            try { std::rethrow_exception(ep); }
            catch (const std::exception& e) { std::cerr << "[terminate] std::exception: " << e.what() << std::endl; }
            catch (...) { std::cerr << "[terminate] unknown exception" << std::endl; }
        } else {
            std::cerr << "[terminate] called without current_exception" << std::endl;
        }
    }
    catch (...) {}
    abort();
}

// ===== WinMain =====
// ===== 修正后的 wWinMain =====
int APIENTRY wWinMain(HINSTANCE hI, HINSTANCE, LPWSTR, int nSC) {
    // 互斥锁：防止多个实例同时运行
    std::set_terminate(LogTerminate);
    g_hMutex = CreateMutexW(nullptr, FALSE, L"StrikeSense_SingleInstanceMutex");
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        std::cout << "[系统] 已有 StrikeSense 实例在运行" << std::endl;
        MessageBoxW(nullptr, L"程序已在运行中，不能重复启动。", L"StrikeSense", MB_OK | MB_ICONINFORMATION);
        if (g_hMutex) CloseHandle(g_hMutex);
        return 1;
    }

    if (antistupid::CheckAndBlock()) { if (g_hMutex) CloseHandle(g_hMutex); return 1; }
    Gdiplus::GdiplusStartupInput in;
    Gdiplus::GdiplusStartup(&g_gdiToken, &in, nullptr);
    g_Console.InitRedirection();
    flashoverlay::Initialize(hInst);
    // ===== 启动信息 =====
    std::cout << "============================================" << std::endl;
    std::cout << "  StrikeSense 测试发布版 202606171635" << std::endl;
    std::cout << "  Copyright (C) 2026 无损平方集团" << std::endl;
    std::cout << "============================================" << std::endl;
    std::cout << "  本程序承诺：" << std::endl;
    std::cout << "  ★ 永不联网！" << std::endl;
    std::cout << "  ★ 绝对无毒！" << std::endl;
    std::cout << "  ★ 数据安全！" << std::endl;
    std::cout << "  ★ 快速好用！" << std::endl;
    std::cout << "============================================" << std::endl;
    std::cout << "  本程序承诺绝不联网！所以无法检查更新" << std::endl;
    std::cout << "============================================" << std::endl;
    config::EnsureDirectoriesExist(); config::Load();
    LoadQuickStopConfig();
    sound::Init(); sound::PreloadSounds();
    if (gsi::Initialize()) gsi::StartServer();

    LoadStringW(hI, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hI, IDC_STRIKESENSE, szWindowClass, MAX_LOADSTRING);
    // i18n 必须在任何绘制前初始化
    i18n::Init();

    // ----------------------------------------------------
    // 【核心修复区域】仅创建一次窗口，并将唯一句柄交给热键注册
    MyRegisterClass(hI);
    
    HWND hwMain = InitInstance(hI, nSC); // 仅创建这一个唯一的有效窗口
    if (!hwMain) return FALSE;

    // 页面初始化
    InitLegalCfgPage();
    
    // 加载本地配置并让动态全局热键生效
    LoadEvolutionParams();  
    Hotkey::UpdateGlobalHotkey(hwMain); 
    Hotkey::UpdateItemHelperHotkey(hwMain);
    // ----------------------------------------------------

    // ===== 自动检测 GSI 配置文件 =====
    {
        fs::path gsiCfg = fs::path(GetCS2CfgPath()) / L"gamestate_integration_square.cfg";
        bool gsiExists = fs::exists(gsiCfg);
        if (!gsiExists)
        {
            std::cout << "[GSI] 未发现 GSI 配置文件，准备安装..." << std::endl;
            // 用消息循环延迟调用，确保窗口已经创建
            PostMessageW(hwMain, WM_COMMAND, IDM_CREATE_GSI_CFG, 0);
        }
        else
        {
            std::cout << "[GSI] GSI 配置文件已存在: " << gsiCfg.string() << std::endl;
        }
    }

    HACCEL hAcc = LoadAccelerators(hI, MAKEINTRESOURCE(IDC_STRIKESENSE));
    MSG m;
    while (GetMessage(&m, nullptr, 0, 0)) {
        if (!TranslateAccelerator(m.hwnd, hAcc, &m)) {
            TranslateMessage(&m);
            DispatchMessage(&m);
        }
    }
    gsi::StopServer(); gsi::Cleanup(); sound::Quit();
    Gdiplus::GdiplusShutdown(g_gdiToken);
    if (g_hMutex) CloseHandle(g_hMutex);
    return (int)m.wParam;
}

ATOM MyRegisterClass(HINSTANCE hI) {
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc); wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc; wc.hInstance = hI;
    wc.hIcon = LoadIcon(hI, MAKEINTRESOURCE(IDI_STRIKESENSE));
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName = MAKEINTRESOURCEW(IDC_STRIKESENSE);
    wc.lpszClassName = szWindowClass;
    wc.hIconSm = LoadIcon(wc.hInstance, MAKEINTRESOURCE(IDI_SMALL));
    return RegisterClassExW(&wc);
}

HWND InitInstance(HINSTANCE hI, int nSC) {
    hInst = hI;
    HWND w = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, 820, 740, nullptr, nullptr, hI, nullptr);
    if (!w) return nullptr;
    ShowWindow(w, nSC); UpdateWindow(w); 
    return w; //返回窗口句柄
}

static void PaintAll(HWND hw, HDC hdc) {
    using namespace Gdiplus;
    RECT rc; GetClientRect(hw, &rc);
    int W = rc.right - rc.left, H = rc.bottom - rc.top;
    HDC md = CreateCompatibleDC(hdc);
    HBITMAP mb = CreateCompatibleBitmap(hdc, W, H);
    HBITMAP ob = (HBITMAP)SelectObject(md, mb);
    Graphics g(md);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintAntiAlias);
    SolidBrush bg(Color(255, 240, 248, 255));
    g.FillRectangle(&bg, 0, 0, W, H);
    PaintSidebar(g, W, H);
    int cx = SIDEBAR_W + 12, cw = W - cx - 12;
    switch (g_currentPage) {
    case PAGE_SOUNDS:     PaintSoundsPage(g, cx, cw, H, hw); break;
    case PAGE_SETTINGS:   PaintSettingsPage(g, cx, cw, H, hw); break;
    case PAGE_EVOLUTION:  PaintEvolutionPage(g, cx, cw, H, hw); break;
    case PAGE_LEGALCFG:   PaintLegalCfgPage(g, cx, cw, H, hw); break;
    case PAGE_Rage:  PaintRagePage(g, cx, cw, H, hw); break;
    case PAGE_ITEMHELPER: PaintItemHelperPage(g, cx, cw, H, hw); break;
    }
    BitBlt(hdc, 0, 0, W, H, md, 0, 0, SRCCOPY);
    SelectObject(md, ob); DeleteObject(mb); DeleteDC(md);
}

LRESULT CALLBACK WndProc(HWND hw, UINT m, WPARAM wp, LPARAM lp) {
    switch (m) {
    case WM_ERASEBKGND: return DefWindowProc(hw, m, wp, lp);
    case WM_CREATE: std::cout << "StrikeSense 启动" << std::endl; break;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hw, &ps);
        PaintAll(hw, hdc); EndPaint(hw, &ps); break;
    }
    case WM_LBUTTONDOWN: {
        int mx = LOWORD(lp), my = HIWORD(lp);
        if (mx < SIDEBAR_W) { CheckSidebarClick(hw, mx, my); break; }
        switch (g_currentPage) {
        case PAGE_SOUNDS:     CheckSoundsClick(hw, mx, my); break;
        case PAGE_SETTINGS:   CheckSettingsClick(hw, mx, my); break;
        case PAGE_EVOLUTION:  CheckEvolutionClick(hw, mx, my); break;
        case PAGE_LEGALCFG:   CheckLegalCfgClick(hw, mx, my); break;
        case PAGE_Rage:  CheckRageClick(hw, mx, my); break;
        case PAGE_ITEMHELPER: CheckItemHelperClick(hw, mx, my); break;
        }
        break;
    }
    // 【修改后】完全独立的两个 case
    case WM_CHAR: {
        return DefWindowProc(hw, m, wp, lp);
    }

    case WM_HOTKEY: {
        if (wp == 1001) { // 捕获到全局动态热键触发
            
            // 权限拦截：未以管理员身份运行则直接静默阻止，不弹窗，不切换状态
            if (!normalgen::CheckAdminPermission()) {
                std::cout << "[全局热键阻止] 游戏内尝试切换失败：无管理员权限。" << std::endl;
                break; 
            }

            // 权限通过，允许切换状态
            g_deathMute = !g_deathMute;
            SaveEvolutionParams();
            
            // 执行具体的音频控制逻辑
            if (g_deathMute) {
                StartCS2VolumeControl(g_death_vol);
            } else {
                StopCS2VolumeControl();
            }
            
            // 通知主窗口重绘
            InvalidateRect(hw, nullptr, FALSE); 
        }

        if (wp == 1002)
{
    std::cout
        << "[道具助手] HOME PRESSED"
        << std::endl;

    return 0;
}
        return 0;
    }
case WM_KEYDOWN: {
        // 1. 如果当前在合法配置页面，优先处理该页面的输入逻辑
        if (g_currentPage == PAGE_LEGALCFG && ProcessLegalCfgKeyInput(hw, m, wp, lp))
            break;

        // 2. 如果正在 Evolution 页面录入热键，处理录入逻辑
        extern bool g_isBindingHotkey;
        if (g_isBindingHotkey) {
            int vk = (int)wp;
            // 排除修饰键本身
            if (vk != VK_CONTROL && vk != VK_SHIFT && vk != VK_MENU) {
                g_hotkeyVk = vk;
                
                // 顺便录入当前的修饰键状态
                g_hotkeyMod = 0;
                if (GetKeyState(VK_CONTROL) & 0x8000) g_hotkeyMod |= MOD_CONTROL;
                if (GetKeyState(VK_MENU)    & 0x8000) g_hotkeyMod |= MOD_ALT;
                if (GetKeyState(VK_SHIFT)   & 0x8000) g_hotkeyMod |= MOD_SHIFT;

                g_isBindingHotkey = false; // 录入完成
                SaveEvolutionParams();     // 保存配置到文件
                Hotkey::UpdateGlobalHotkey(hw);
                InvalidateRect(hw, nullptr, FALSE); // 刷新界面
            }
            break;
        }

        // 3. 处理快捷键触发逻辑（当辅助程序在前台时）
        if (g_hotkeyVk != 0 && (int)wp == g_hotkeyVk) {
            
            // --- 核心修改：权限拦截（不弹窗，直接阻止切换） ---
            if (!normalgen::CheckAdminPermission()) {
                // 如果没有管理员权限，直接打破逻辑，不改变任何状态，不写 JSON，不弹窗
                std::cout << "[权限阻止] 尝试快捷键切换失败：未以管理员身份运行" << std::endl;
                break; 
            }
            // ------------------------------------------------

            // 检查修饰键是否匹配
            bool ctrlPressed  = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            bool altPressed   = (GetKeyState(VK_MENU)    & 0x8000) != 0;
            bool shiftPressed = (GetKeyState(VK_SHIFT)   & 0x8000) != 0;

            bool targetCtrl  = (g_hotkeyMod & MOD_CONTROL) != 0;
            bool targetAlt   = (g_hotkeyMod & MOD_ALT) != 0;
            bool targetShift = (g_hotkeyMod & MOD_SHIFT) != 0;

            // 如果修饰键对不上，不触发
            if (ctrlPressed != targetCtrl || altPressed != targetAlt || shiftPressed != targetShift) {
                break;
            }

            // 权限和按键都通过，允许改变状态
            g_deathMute = !g_deathMute; // 切换开关状态
            SaveEvolutionParams();      // 保存状态
            
            // 执行音频控制逻辑
            if (g_deathMute) {
                StartCS2VolumeControl(g_death_vol);
            } else {
                StopCS2VolumeControl();
            }
            
            // 关键：通知主窗口重绘
            InvalidateRect(hw, nullptr, FALSE); 
            break;
        }

        if (g_isBindingItemHelperHotkey)    //道具助手绑定
{
    int vk = (int)wp;

    if (vk != VK_CONTROL &&
        vk != VK_SHIFT &&
        vk != VK_MENU)
    {
        g_itemHelperHotkeyVk = vk;

        g_itemHelperHotkeyMod = 0;

        if (GetKeyState(VK_CONTROL) & 0x8000)
            g_itemHelperHotkeyMod |= MOD_CONTROL;

        if (GetKeyState(VK_SHIFT) & 0x8000)
            g_itemHelperHotkeyMod |= MOD_SHIFT;

        if (GetKeyState(VK_MENU) & 0x8000)
            g_itemHelperHotkeyMod |= MOD_ALT;

        g_isBindingItemHelperHotkey = false;

        SaveEvolutionParams();

        Hotkey::UpdateItemHelperHotkey(hw);

        InvalidateRect(
            hw,
            nullptr,
            FALSE);
    }

    return 0;
}

        return DefWindowProc(hw, m, wp, lp);
    }
    case WM_COMMAND: {
        int id = LOWORD(wp);
        switch (id) {
        case IDM_CREATE_GSI_CFG: OnCreateGSIConfig(hw); break;
        case IDM_DEBUGGER: g_Console.ShowDebugger(hInst, hw); break;
        case IDM_ABOUT: DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hw, About); break;
        case IDM_SETTINGS: g_currentPage = PAGE_SETTINGS; InvalidateRect(hw, nullptr, FALSE); break;
        case IDM_EXIT: DestroyWindow(hw); break;
        default: return DefWindowProc(hw, m, wp, lp);
        }
        break;
    }
    case WM_CLOSE: DestroyWindow(hw); break;
    case WM_DESTROY:
{
    flashoverlay::Shutdown();
    UnregisterHotKey(hw, 1001);
    UnregisterHotKey(hw, 1002);
    PostQuitMessage(0);
    return 0;
}
    default: return DefWindowProc(hw, m, wp, lp);
    }
    return 0;
}

// ===== GSI 配置路径 =====
static std::wstring GetCS2CfgPath() {
    std::wstring sv = strikesense::LoadSavedCfgPath();
    if (!sv.empty() && fs::exists(sv)) return sv;
    std::wstring sp = strikesense::GetSteamPathFromRegistry();
    if (sp.empty()) return L"";
    std::wstring cd = strikesense::FindCS2InstallDir(sp);
    if (cd.empty()) return L"";
    return strikesense::GetCS2CfgPath(cd);
}

static void OnCreateGSIConfig(HWND hw) {
    g_gsiCfgPath = GetCS2CfgPath();
    if (g_gsiCfgPath.empty()) {
        wchar_t p[MAX_PATH] = {};
        BROWSEINFOW b = {}; b.hwndOwner = hw;
        b.lpszTitle = L"选择CS2 cfg目录"; b.ulFlags = BIF_RETURNONLYFSDIRS;
        LPITEMIDLIST pid = SHBrowseForFolderW(&b);
        if (!pid) return;
        SHGetPathFromIDListW(pid, p); g_gsiCfgPath = p;
        IMalloc* m = nullptr;
        if (SUCCEEDED(SHGetMalloc(&m))) { m->Free(pid); m->Release(); }
    }
    DialogBoxW(hInst, MAKEINTRESOURCEW(IDD_CONFIRM_PATH), hw, ConfirmPathDlgProc);
}

INT_PTR CALLBACK ConfirmPathDlgProc(HWND hD, UINT m, WPARAM wp, LPARAM lp) {
    switch (m) {
    case WM_INITDIALOG:
        SetDlgItemTextW(hD, IDC_PATH_LABEL, g_gsiCfgPath.c_str());
        { RECT r; GetWindowRect(GetParent(hD), &r);
        SetWindowPos(hD, nullptr, r.left + (r.right - r.left) / 2 - 225,
            r.top + (r.bottom - r.top) / 2 - 108, 0, 0, SWP_NOSIZE | SWP_NOZORDER); }
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case IDYES:
            if (strikesense::WriteGSIConfig(g_gsiCfgPath)) {
                strikesense::SaveCfgPath(g_gsiCfgPath);
                MessageBoxW(hD, L"成功！", L"提示", MB_OK);
            } else MessageBoxW(hD, L"失败！", L"错误", MB_OK);
            EndDialog(hD, IDYES); return TRUE;
        case IDC_DELETE_CFG: {
            fs::path f = fs::path(g_gsiCfgPath) / L"gamestate_integration_square.cfg";
            std::error_code ec; fs::remove(f, ec);
            MessageBoxW(hD, ec ? L"失败" : L"已删除", L"提示", MB_OK); return TRUE;
        }
        case IDC_BROWSE_BTN: {
            wchar_t p[MAX_PATH] = {}; BROWSEINFOW b = {};
            b.hwndOwner = hD; b.lpszTitle = L"选择cfg目录"; b.ulFlags = BIF_RETURNONLYFSDIRS;
            LPITEMIDLIST pid = SHBrowseForFolderW(&b);
            if (pid) {
                SHGetPathFromIDListW(pid, p); g_gsiCfgPath = p;
                SetDlgItemTextW(hD, IDC_PATH_LABEL, p);
                IMalloc* mm = nullptr;
                if (SUCCEEDED(SHGetMalloc(&mm))) { mm->Free(pid); mm->Release(); }
            }
            return TRUE;
        }
        case IDCANCEL: EndDialog(hD, IDCANCEL); return TRUE;
        }
    }
    return FALSE;
}

INT_PTR CALLBACK About(HWND hD, UINT m, WPARAM wp, LPARAM lp) {
    UNREFERENCED_PARAMETER(lp);
    switch (m) {
    case WM_INITDIALOG: return (INT_PTR)TRUE;
    case WM_COMMAND:
        if (LOWORD(wp) == IDOK || LOWORD(wp) == IDCANCEL) EndDialog(hD, LOWORD(wp));
        return (INT_PTR)TRUE;
    }
    return (INT_PTR)FALSE;
}
