// StrikeSense.cpp — 程序入口
#include "framework.h"
#include "StrikeSense.h"
#include "console.h"
#include "steam_helper.h"
#include "gsi_server.h"
#include "config.h"
#include "console_log.h"
#include "sound_player.h"
#include "antistupid.h"
#include "i18n.h"
#include "quickstop.h"
#include "volume_mixer.h"
#include <iostream>
#include <filesystem>
#include <ShlObj.h>
#include <Shellapi.h>
#include <commctrl.h>
#include <gdiplus.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include <exception>
#include "flashoverlay.h"
#include "normalgen.h"
#include "mouse_jitter.h"
#include "Hotkey.h"
#include "itemhelper_overlay.h"
#include "itemhelper_page.h"   
#include "vscript.h"
#include <regex>
#include <sstream>
#include <iterator>
#include <utility>
#include <vector>
#include <Windows.h>
#include "SteamHelper.h"
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "SteamHelper.lib")
#pragma comment(lib, "comctl32.lib")

using namespace std;
using namespace filesystem;
namespace fs = std::filesystem;
#define MAX_LOADSTRING 100
constexpr UINT WM_TRAYICON = WM_APP + 100;
constexpr UINT TRAY_ICON_ID = 1;
constexpr int CLOSE_ACTION_CANCEL = 0;
constexpr int CLOSE_ACTION_TRAY = 1;
constexpr int CLOSE_ACTION_EXIT = 2;
constexpr int TRAY_CMD_RESTORE = 41001;
constexpr int TRAY_CMD_EXIT = 41002;

// ===== 全局变量定义 =====
HWND g_hwnd = nullptr;
HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING], szWindowClass[MAX_LOADSTRING];
Console g_Console;
std::wstring g_gsiCfgPath;
static ULONG_PTR g_gdiToken = 0;
HANDLE g_hMutex = nullptr;
static bool g_forceExit = false;
static bool g_trayIconAdded = false;

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

// ===== 道具助手新增全局变量 =====
float g_itemHelperX = 0.95f;       // X位置 (0.0~1.0，默认靠右)
float g_itemHelperY = 0.5f;        // Y位置 (0.0~1.0，默认居中)
float g_itemHelperOpacity = 1.0f;  // 透明度 (0.0~1.0)
bool  g_itemHelperAutoHide = false;// 桌面自动销毁开关

int   g_itemHelperKeyPrev = VK_UP;      // 上一项快捷键
int   g_itemHelperKeyNext = VK_DOWN;    // 下一项快捷键
int   g_itemHelperKeySelect = VK_RETURN;// 确认/预览快捷键

bool  g_isBindingItemKeyPrev = false;
bool  g_isBindingItemKeyNext = false;
bool  g_isBindingItemKeySelect = false;
float g_itemHelperImgOpacity = 1.0f; //图片预览透明度



// ===== 前向声明 =====
ATOM MyRegisterClass(HINSTANCE);
HWND InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK ConfirmPathDlgProc(HWND, UINT, WPARAM, LPARAM);
static std::wstring GetCS2CfgPath();
static void OnCreateGSIConfig(HWND);
static void PaintAll(HWND, HDC);
static void AddTrayIcon(HWND hw);
static void RemoveTrayIcon(HWND hw);
static void RestoreFromTray(HWND hw);
static void ShowTrayMenu(HWND hw);
static int ResolveCloseAction(HWND hw);
static bool HasLaunchArg(const std::wstring& expected);
int AddCS2vulkanDebugVersion();


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

static bool HasLaunchArg(const std::wstring& expected)
{
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return false;

    bool found = false;
    for (int i = 1; i < argc; ++i) {
        if (_wcsicmp(argv[i], expected.c_str()) == 0) {
            found = true;
            break;
        }
    }
    LocalFree(argv);
    return found;
}

// ===== WinMain =====
// ===== 修正后的 wWinMain =====
int APIENTRY wWinMain(HINSTANCE hI, HINSTANCE, LPWSTR, int nSC) {
    const bool launchSemiRage = HasLaunchArg(L"-semirage");
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
    std::cout << "  StrikeSense 测试发布版 202607151952" << std::endl;
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
    mousejitter::LoadConfig();
    consolelog::LoadConfig();
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
    g_hwnd = hwMain;
    vscript::Initialize(hInst, hwMain);
    if (launchSemiRage) EnableRageModeFromLaunch(hwMain);
    SetTimer(hwMain, 2001, 200, nullptr);

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
    mousejitter::Shutdown();
    consolelog::Shutdown();
    StopQuickStopHook();
    gsi::StopServer(); gsi::Cleanup(); vscript::Shutdown(); sound::Quit();
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
    case PAGE_VSCRIPT: PaintVscriptPage(g, cx, cw, H, hw); break;
    }
    BitBlt(hdc, 0, 0, W, H, md, 0, 0, SRCCOPY);
    SelectObject(md, ob); DeleteObject(mb); DeleteDC(md);
}

static void AddTrayIcon(HWND hw)
{
    if (g_trayIconAdded) return;

    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hw;
    nid.uID = TRAY_ICON_ID;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_STRIKESENSE));
    wcscpy_s(nid.szTip, L"StrikeSense");

    if (Shell_NotifyIconW(NIM_ADD, &nid)) {
        g_trayIconAdded = true;
        std::cout << "[托盘] 已隐藏到系统托盘。" << std::endl;
    }
}

static void RemoveTrayIcon(HWND hw)
{
    if (!g_trayIconAdded) return;

    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hw;
    nid.uID = TRAY_ICON_ID;
    Shell_NotifyIconW(NIM_DELETE, &nid);
    g_trayIconAdded = false;
    std::cout << "[托盘] 已移除系统托盘图标。" << std::endl;
}

static void RestoreFromTray(HWND hw)
{
    RemoveTrayIcon(hw);
    ShowWindow(hw, SW_SHOW);
    ShowWindow(hw, SW_RESTORE);
    SetForegroundWindow(hw);
    std::cout << "[托盘] 主窗口已恢复显示。" << std::endl;
}

static void ShowTrayMenu(HWND hw)
{
    HMENU menu = CreatePopupMenu();
    if (!menu) return;

    AppendMenuW(menu, MF_STRING, TRAY_CMD_RESTORE, L"显示窗口");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, TRAY_CMD_EXIT, L"退出程序");

    POINT pt{};
    GetCursorPos(&pt);
    SetForegroundWindow(hw);
    const int cmd = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hw, nullptr);
    DestroyMenu(menu);

    if (cmd == TRAY_CMD_RESTORE) RestoreFromTray(hw);
    else if (cmd == TRAY_CMD_EXIT) {
        g_forceExit = true;
        DestroyWindow(hw);
    }
}

static int ResolveCloseAction(HWND hw)
{
    config::Settings settings = config::Load();
    if (settings.close_behavior == CLOSE_ACTION_TRAY || settings.close_behavior == CLOSE_ACTION_EXIT)
        return settings.close_behavior;

    TASKDIALOG_BUTTON buttons[] = {
        { CLOSE_ACTION_TRAY, L"隐藏到托盘\n程序继续在后台运行，可从托盘图标恢复。" },
        { CLOSE_ACTION_EXIT, L"关闭程序\n停止后台功能并退出 StrikeSense。" },
    };

    TASKDIALOGCONFIG cfg{};
    cfg.cbSize = sizeof(cfg);
    cfg.hwndParent = hw;
    cfg.hInstance = hInst;
    cfg.dwFlags = TDF_USE_COMMAND_LINKS | TDF_ALLOW_DIALOG_CANCELLATION;
    cfg.pszWindowTitle = L"关闭 StrikeSense";
    cfg.pszMainInstruction = L"要隐藏到托盘还是关闭程序？";
    cfg.pszContent = L"隐藏到托盘后，可以通过托盘图标重新呼出窗口或彻底退出。";
    cfg.cButtons = static_cast<UINT>(std::size(buttons));
    cfg.pButtons = buttons;
    cfg.nDefaultButton = CLOSE_ACTION_TRAY;
    cfg.pszVerificationText = L"不再询问，记住我的选择";
    cfg.dwCommonButtons = TDCBF_CANCEL_BUTTON;

    int selected = CLOSE_ACTION_CANCEL;
    BOOL checked = FALSE;
    const HRESULT hr = TaskDialogIndirect(&cfg, &selected, nullptr, &checked);
    if (FAILED(hr)) {
        selected = MessageBoxW(hw, L"是否隐藏到系统托盘？\n选择“否”将关闭程序。", L"关闭 StrikeSense",
            MB_YESNOCANCEL | MB_ICONQUESTION | MB_DEFBUTTON1);
        if (selected == IDYES) return CLOSE_ACTION_TRAY;
        if (selected == IDNO) return CLOSE_ACTION_EXIT;
        return CLOSE_ACTION_CANCEL;
    }

    if (selected != CLOSE_ACTION_TRAY && selected != CLOSE_ACTION_EXIT)
        return CLOSE_ACTION_CANCEL;

    if (checked) {
        settings.close_behavior = selected;
        config::Save(settings);
        std::cout << "[托盘] 已保存关闭行为偏好: " << selected << std::endl;
    }

    return selected;
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
        case PAGE_VSCRIPT: CheckVscriptClick(hw, mx, my); break;
        }
        break;
    }
    // 【修改后】完全独立的两个 case
    case WM_CHAR: {
        // 1. 如果当前在合法配置页面，优先处理该页面的字符输入逻辑
        if (g_currentPage == PAGE_LEGALCFG && ProcessLegalCfgKeyInput(hw, m, wp, lp)) {
            return 0; // 如果内部成功处理（返回了 true），这里直接 return 0 拦截消息，不让系统默认处理
        }

        // 2. 如果不是该页面，或者该页面没处理这个按键，再走默认处理
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
            std::cout << "[道具助手] 检测到游戏内快捷键触发信号" << std::endl;
            if (g_itemHelperEnabled) {
                // 执行切换显示或隐藏的动作
                itemhelper_overlay::Toggle(hInst);
            } else {
                std::cout << "[道具助手] 全局功能已被关闭，忽略外部按键事件" << std::endl;
            }
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

        // 道具助手：主开关快捷键绑定
        if (g_isBindingItemHelperHotkey) {
            int vk = (int)wp;
            if (vk != VK_CONTROL && vk != VK_SHIFT && vk != VK_MENU) {
                g_itemHelperHotkeyVk = vk;
                g_itemHelperHotkeyMod = 0;
                if (GetKeyState(VK_CONTROL) & 0x8000) g_itemHelperHotkeyMod |= MOD_CONTROL;
                if (GetKeyState(VK_SHIFT) & 0x8000)   g_itemHelperHotkeyMod |= MOD_SHIFT;
                if (GetKeyState(VK_MENU) & 0x8000)    g_itemHelperHotkeyMod |= MOD_ALT;
                g_isBindingItemHelperHotkey = false;
                SaveEvolutionParams();
                Hotkey::UpdateItemHelperHotkey(hw);
                InvalidateRect(hw, nullptr, FALSE);
            }
            return 0; // 修复：这里必须 return，防止往下走到别的逻辑
        }

        // 道具助手：绑定上一项/下一项/确认热键 (修复嵌套：把它从上面的括号里拿出来了)
        if (g_isBindingItemKeyPrev || g_isBindingItemKeyNext || g_isBindingItemKeySelect) {
            int vk = (int)wp;
            // 过滤掉单独的修饰键
            if (vk != VK_CONTROL && vk != VK_SHIFT && vk != VK_MENU) {
                if (g_isBindingItemKeyPrev) { g_itemHelperKeyPrev = vk; g_isBindingItemKeyPrev = false; }
                if (g_isBindingItemKeyNext) { g_itemHelperKeyNext = vk; g_isBindingItemKeyNext = false; }
                if (g_isBindingItemKeySelect) { g_itemHelperKeySelect = vk; g_isBindingItemKeySelect = false; }

                SaveEvolutionParams();
                InvalidateRect(hw, nullptr, FALSE);
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
        case IDM_EXIT: g_forceExit = true; DestroyWindow(hw); break;
        default: return DefWindowProc(hw, m, wp, lp);
        }
        break;
    }
    case WM_MOUSEMOVE:
        if (g_currentPage == PAGE_VSCRIPT) {
            InvalidateRect(hw, nullptr, FALSE);
        }
        break;
    case WM_TIMER:
        if (wp == 2001) {
            vscript::TickContinuousScripts();
            return 0;
        }
        break;
    case WM_TRAYICON:
        if (lp == WM_LBUTTONDBLCLK) RestoreFromTray(hw);
        else if (lp == WM_RBUTTONUP || lp == WM_CONTEXTMENU) ShowTrayMenu(hw);
        return 0;
    case WM_CLOSE: {
        if (g_forceExit) {
            DestroyWindow(hw);
            break;
        }

        const int action = ResolveCloseAction(hw);
        if (action == CLOSE_ACTION_TRAY) {
            AddTrayIcon(hw);
            ShowWindow(hw, SW_HIDE);
            return 0;
        }
        if (action == CLOSE_ACTION_EXIT) {
            g_forceExit = true;
            DestroyWindow(hw);
            break;
        }
        return 0;
    }
    case WM_DESTROY:
{
    RemoveTrayIcon(hw);
    flashoverlay::Shutdown();
    itemhelper_overlay::Shutdown();
    KillTimer(hw, 2001);
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
            // 1. 首先尝试写入原有的 GSI 配置
            if (strikesense::WriteGSIConfig(g_gsiCfgPath)) {
                strikesense::SaveCfgPath(g_gsiCfgPath);

                // 2. GSI 成功后，紧接着调用追加 Vulkan 启动项的函数
                int vulkanResult = AddCS2vulkanDebugVersion();

                // 3. 根据函数的各种返回值进行分流弹窗提示
                switch (vulkanResult) {
                case 0:
                    // 成功追加了必要启动项
                    MessageBoxW(hD, L"GSI 配置成功！已成功为您的 Steam 账号 CS2 启动项追加了 -vulkan 和 -condebug 参数。\n\n请完全重启 Steam 客户端以使启动项生效！", L"提示", MB_OK | MB_ICONINFORMATION);
                    break;

                case 1:
                    // 本地所有账号本来就都有必要启动项
                    MessageBoxW(hD, L"GSI 配置成功！检测到您的 Steam 账号启动项中本来就已包含 -vulkan 和 -condebug，无需重复添加。", L"提示", MB_OK | MB_ICONINFORMATION);
                    break;

                case 3:
                    // 注册表找不到 Steam 路径
                    MessageBoxW(hD, L"GSI 配置成功！但未能自动添加启动项：在注册表中未检测到标准的 Steam 安装路径，请手动为 CS2 添加 -vulkan 和 -condebug 启动项。", L"警告", MB_OK | MB_ICONWARNING);
                    break;

                case 4:
                    // 找不到账号文件夹 (userdata 为空)
                    MessageBoxW(hD, L"GSI 配置成功！但未能自动添加启动项：未在本地 Steam 目录中发现任何登录过的用户数据，请手动为 CS2 添加 -vulkan 和 -condebug 启动项。", L"警告", MB_OK | MB_ICONWARNING);
                    break;

                case 5:
                    // 物理上找不到任何 localconfig.vdf
                    MessageBoxW(hD, L"GSI 配置成功！但未能自动添加启动项：未找到有效的 Steam 本地配置文件(localconfig.vdf)，请手动为 CS2 添加 -vulkan 和 -condebug 启动项。", L"警告", MB_OK | MB_ICONWARNING);
                    break;

                case 6:
                    // 文件被独占或无权打开
                    MessageBoxW(hD, L"GSI 配置成功！但未能自动添加启动项：本地配置文件当前被占用或拒绝访问，请【完全关闭 Steam 客户端】后再试，或手动添加 -vulkan 和 -condebug 启动项。", L"警告", MB_OK | MB_ICONWARNING);
                    break;

                case 7:
                    // 找到了配置文件，但里面没有 CS2 (730) 记录
                    MessageBoxW(hD, L"GSI 配置成功！但未能自动添加启动项：检测到您的 Steam 账号在该电脑上【从未启动过 CS2】，请至少运行一次游戏后再试，或手动添加 -vulkan 和 -condebug 启动项。", L"警告", MB_OK | MB_ICONWARNING);
                    break;

                case 8:
                    // 正则解析异常或格式不规范
                    MessageBoxW(hD, L"GSI 配置成功！但未能自动添加启动项：本地配置文件格式解析异常，为了安全未进行强行修改，请手动为 CS2 添加 -vulkan 和 -condebug 启动项。", L"警告", MB_OK | MB_ICONWARNING);
                    break;

                default:
                    // 防御性未知错误
                    MessageBoxW(hD, L"GSI 配置成功！但添加启动项时发生了未知的兼容性问题，请手动为 CS2 添加 -vulkan 和 -condebug 启动项。", L"警告", MB_OK | MB_ICONWARNING);
                    break;
                }

            }
            else {
                // GSI 写入本身就失败的情况
                MessageBoxW(hD, L"配置写入失败！请检查游戏路径是否正确或是否拥有管理员权限。", L"错误", MB_OK | MB_ICONERROR);
            }

            EndDialog(hD, IDYES);
            return TRUE;
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

SteamHelper helper;
wstring steamPath = helper.CallRegister2Steam();


int AddCS2vulkanDebugVersion() {
    if (steamPath == L"Read Failed") return 3;

    const vector<string>& userIDs = helper.GetSteamUserIDs();
    if (userIDs.empty()) return 4;

    auto ensureLaunchOptions = [](string currentOptions) {
        bool changed = false;
        const vector<string> requiredOptions = { "-vulkan", "-condebug" };
        for (const string& option : requiredOptions) {
            if (currentOptions.find(option) != string::npos) continue;
            if (!currentOptions.empty() && currentOptions.back() != ' ') currentOptions += " ";
            currentOptions += option;
            changed = true;
        }
        return pair<string, bool>{ currentOptions, changed };
    };

    bool anyAddedTotal = false;
    bool allAlreadyHad = true;
    bool foundAnyVdf = false; // 新增：是否至少找到了一个物理文件
    int lastSpecificError = 5;

    for (const string& userID : userIDs) {
        path vdfPath = path(steamPath) / "userdata" / userID / "config" / "localconfig.vdf";

        if (!exists(vdfPath)) continue;

        foundAnyVdf = true; // 只要进到这里，说明文件物理存在

        string content;
        {
            ifstream inFile(vdfPath, ios::binary);
            if (!inFile.is_open()) {
                lastSpecificError = 6;
                continue;
            }
            stringstream buffer;
            buffer << inFile.rdbuf();
            content = buffer.str();
        }

        if (content.empty()) continue;

        // 查找 730 (CS2/CSGO的AppID)
        size_t pos730 = content.find("\"730\"");
        if (pos730 == string::npos) {
            if (lastSpecificError < 7) lastSpecificError = 7;
            continue;
        }

        // 走到这一步，错误码至少应该是 8 (没找到 LaunchOptions)
        if (lastSpecificError < 8) lastSpecificError = 8;

        bool modified = false;
        size_t posLaunch = content.find("\"LaunchOptions\"", pos730);

        if (posLaunch != string::npos) {
            regex launchRegex("\"LaunchOptions\"\\s+\"([^\"]*)\"");
            smatch match;

            size_t lineEnd = content.find('\n', posLaunch);
            if (lineEnd == string::npos) lineEnd = content.length();
            string line = content.substr(posLaunch, lineEnd - posLaunch);

            if (regex_search(line, match, launchRegex)) {
                string currentOptions = match[1].str();
                auto [newOptions, optionsChanged] = ensureLaunchOptions(currentOptions);
                if (!optionsChanged) {
                    continue;
                }

                allAlreadyHad = false;
                string newLine = "\"LaunchOptions\"\t\t\"" + newOptions + "\"";
                content.replace(posLaunch, line.length(), newLine);
                modified = true;
            }
        }
        else {
            size_t posBrace = content.find('{', pos730);
            if (posBrace != string::npos) {
                string insertStr = "\n\t\t\t\t\t\t\"LaunchOptions\"\t\t\"-vulkan -condebug\"";
                content.insert(posBrace + 1, insertStr);
                modified = true;
                allAlreadyHad = false;
            }
        }

        if (modified) {
            ofstream outFile(vdfPath, ios::trunc | ios::binary);
            if (outFile.is_open()) {
                outFile << content;
                outFile.close();
                anyAddedTotal = true;
            }
        }
    }

    if (anyAddedTotal) return 0;
    if (!foundAnyVdf) return 5; // 一个 VDF 路径都没对上
    if (allAlreadyHad && lastSpecificError >= 7) return 1;
    return lastSpecificError;
}
