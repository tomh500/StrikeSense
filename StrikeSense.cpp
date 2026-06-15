// StrikeSense.cpp : 定义应用程序的入口点。
//

#include "framework.h"
#include "StrikeSense.h"

#include "console.h"
#include "steam_helper.h"
#include "gsi_server.h"
#include "config.h"
#include "sound_player.h"
#include <iostream>
#include <cstdlib>
#include <filesystem>
#include <ShlObj.h>
#include <sstream>

#define MAX_LOADSTRING 100

// 全局变量:
HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];
Console g_Console;

// GSI cfg 路径
std::wstring g_gsiCfgPath;

// 前向声明
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    ConfirmPathDlgProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    SettingsDlgProc(HWND, UINT, WPARAM, LPARAM);

static std::wstring GetCS2CfgPath();
static void OnCreateGSIConfig(HWND hWnd);

// ============================================================
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // 第一步：输出重定向
    g_Console.InitRedirection();

    // 确保配置目录存在
    config::EnsureDirectoriesExist();
    config::Load();  // 自动创建默认配置文件

    // 初始化 SDL_mixer 音频系统
    sound::Init();

    std::cout << "[主程序] 端口检测已内置于 GSI 服务器。" << std::endl;

    // 启动 GSI HTTP 服务器
    std::cout << "[主程序] 正在初始化 GSI 服务器..." << std::endl;
    if (gsi::Initialize())
    {
        std::cout << "[主程序] GSI 服务器初始化成功，启动监听..." << std::endl;
        gsi::StartServer();
    }
    else
    {
        std::cerr << "[主程序] 警告：GSI 服务器初始化失败！" << std::endl;
    }

    // 初始化窗口
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_STRIKESENSE, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    if (!InitInstance(hInstance, nCmdShow))
        return FALSE;

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_STRIKESENSE));

    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    // 退出前清理
    std::cout << "[主程序] 正在停止 GSI 服务器..." << std::endl;
    gsi::StopServer();
    gsi::Cleanup();

    return (int)msg.wParam;
}

// ============================================================
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_STRIKESENSE));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_STRIKESENSE);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));
    return RegisterClassExW(&wcex);
}

// ============================================================
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance;
    HWND hWnd = CreateWindowW(szWindowClass, szTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, 800, 550,
        nullptr, nullptr, hInstance, nullptr);
    if (!hWnd) return FALSE;
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    return TRUE;
}

// ============================================================
// CS2 cfg 路径获取 — 优先本地配置，其次自动检测
// ============================================================
static std::wstring GetCS2CfgPath()
{
    // 1. 优先读取本地保存的配置
    std::wstring saved = strikesense::LoadSavedCfgPath();
    if (!saved.empty() && fs::exists(saved))
    {
        std::wcout << L"[GSI] 使用已保存的 cfg 路径: " << saved << std::endl;
        return saved;
    }

    // 2. 通过注册表找 Steam
    std::wcout << L"[GSI] 从注册表读取 Steam 路径..." << std::endl;
    std::wstring steamPath = strikesense::GetSteamPathFromRegistry();
    if (steamPath.empty())
    {
        std::wcout << L"[GSI] 注册表读取失败！" << std::endl;
        return L"";
    }
    std::wcout << L"[GSI] Steam 路径: " << steamPath << std::endl;

    // 3. 解析 appmanifest_730.acf 找 CS2 安装目录
    std::wcout << L"[GSI] 查找 CS2 安装目录..." << std::endl;
    std::wstring cs2Dir = strikesense::FindCS2InstallDir(steamPath);
    if (cs2Dir.empty())
    {
        std::wcout << L"[GSI] 未找到 CS2 (appmanifest_730.acf)！" << std::endl;
        return L"";
    }
    std::wcout << L"[GSI] CS2 安装目录: " << cs2Dir << std::endl;

    // 4. 拼接 cfg 路径
    std::wstring cfgPath = strikesense::GetCS2CfgPath(cs2Dir);
    std::wcout << L"[GSI] cfg 目录: " << cfgPath << std::endl;

    return cfgPath;
}

// ============================================================
// 路径确认对话框
// ============================================================
INT_PTR CALLBACK ConfirmPathDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_INITDIALOG:
    {
        HWND hEdit = GetDlgItem(hDlg, IDC_PATH_LABEL);
        if (hEdit) SetWindowTextW(hEdit, g_gsiCfgPath.c_str());

        RECT rc;
        GetWindowRect(GetParent(hDlg), &rc);
        int x = rc.left + (rc.right - rc.left) / 2 - 225;
        int y = rc.top + (rc.bottom - rc.top) / 2 - 108;
        SetWindowPos(hDlg, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        return TRUE;
    }

    case WM_COMMAND:
    {
        WORD id = LOWORD(wParam);
        switch (id)
        {
        case IDYES:
        {
            std::wcout << L"[GSI] 用户确认路径，写入配置文件..." << std::endl;
            if (strikesense::WriteGSIConfig(g_gsiCfgPath))
            {
                strikesense::SaveCfgPath(g_gsiCfgPath);
                MessageBoxW(hDlg, L"GSI 配置文件已成功创建！\n请重启 CS2 以生效。",
                           L"成功", MB_OK | MB_ICONINFORMATION);
                std::wcout << L"[GSI] 配置文件写入成功！" << std::endl;
            }
            else
            {
                std::wcout << L"[GSI] 配置文件写入失败！" << std::endl;
                MessageBoxW(hDlg, L"配置文件写入失败！请检查目录权限。",
                           L"错误", MB_OK | MB_ICONERROR);
            }
            EndDialog(hDlg, IDYES);
            return TRUE;
        }

        case IDC_DELETE_CFG:
        {
            fs::path gsiFile = fs::path(g_gsiCfgPath) / L"gamestate_integration_square.cfg";
            if (fs::exists(gsiFile))
            {
                std::error_code ec;
                fs::remove(gsiFile, ec);
                if (!ec)
                {
                    std::wcout << L"[GSI] 已删除: " << gsiFile.wstring() << std::endl;
                    MessageBoxW(hDlg, L"GSI 配置文件已删除！", L"成功", MB_OK | MB_ICONINFORMATION);
                }
                else
                {
                    std::wcout << L"[GSI] 删除失败！" << std::endl;
                    MessageBoxW(hDlg, L"删除失败！", L"错误", MB_OK | MB_ICONERROR);
                }
            }
            else
            {
                MessageBoxW(hDlg, L"该目录下未安装 GSI 配置文件。", L"提示", MB_OK | MB_ICONINFORMATION);
            }
            return TRUE;
        }

        case IDC_BROWSE_BTN:
        {
            wchar_t path[MAX_PATH] = {};
            BROWSEINFOW bi = {};
            bi.hwndOwner = hDlg;
            bi.pszDisplayName = path;
            bi.lpszTitle = L"请选择 CS2 配置目录 (game/csgo/cfg)";
            bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

            LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
            if (pidl)
            {
                if (SHGetPathFromIDListW(pidl, path))
                {
                    g_gsiCfgPath = path;
                    HWND hEdit = GetDlgItem(hDlg, IDC_PATH_LABEL);
                    if (hEdit) SetWindowTextW(hEdit, g_gsiCfgPath.c_str());
                }
                IMalloc* pMalloc = nullptr;
                if (SUCCEEDED(SHGetMalloc(&pMalloc)))
                {
                    pMalloc->Free(pidl);
                    pMalloc->Release();
                }
            }
            return TRUE;
        }

        case IDCANCEL:
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    }
    return FALSE;
}

// ============================================================
static void OnCreateGSIConfig(HWND hWnd)
{
    std::wcout << L"[GSI] === 开始创建 GSI 配置文件 ===" << std::endl;

    g_gsiCfgPath = GetCS2CfgPath();
    if (g_gsiCfgPath.empty())
    {
        std::wcout << L"[GSI] 无法自动检测 CS2 路径，弹出浏览框" << std::endl;

        wchar_t path[MAX_PATH] = {};
        BROWSEINFOW bi = {};
        bi.hwndOwner = hWnd;
        bi.pszDisplayName = path;
        bi.lpszTitle = L"无法自动检测 CS2 路径，请手动选择 game/csgo/cfg 目录";
        bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

        LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
        if (!pidl)
        {
            std::wcout << L"[GSI] 用户取消浏览" << std::endl;
            return;
        }
        if (SHGetPathFromIDListW(pidl, path))
            g_gsiCfgPath = path;
        IMalloc* pMalloc = nullptr;
        if (SUCCEEDED(SHGetMalloc(&pMalloc)))
        {
            pMalloc->Free(pidl);
            pMalloc->Release();
        }
        if (g_gsiCfgPath.empty())
            return;
    }

    DialogBoxW(hInst, MAKEINTRESOURCEW(IDD_CONFIRM_PATH), hWnd, ConfirmPathDlgProc);
}

// ============================================================
// WndProc
// ============================================================
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        std::cout << "StrikeSense 窗口已创建。" << std::endl;
        std::wcout << L"窗口句柄: 0x" << std::hex << (ULONG_PTR)hWnd << std::dec << L"\n";
        break;
    }

    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        switch (wmId)
        {
        case IDM_CREATE_GSI_CFG:
            OnCreateGSIConfig(hWnd);
            break;

        case IDM_DEBUGGER:
            g_Console.ShowDebugger(hInst, hWnd);
            std::cout << "=== Debugger 调试输出已打开 ===" << std::endl;
            std::cout << "时间戳: " << __DATE__ << " " << __TIME__ << std::endl;
            std::cout << "[GSI] 服务器状态: " << (gsi::IsRunning() ? "运行中 ✅" : "未启动 ❌") << std::endl;
            std::cout << "[GSI] 调试输出(原始JSON): " << (gsi::g_debug ? "开启 ✅" : "关闭") << std::endl;
            std::cout << "[GSI] 使用 cs2 调试开关: 设置 g_debug=1/g_debug=0" << std::endl;
            break;

        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;

        case IDM_SETTINGS:
            DialogBoxW(hInst, MAKEINTRESOURCEW(IDD_SETTINGS), hWnd, SettingsDlgProc);
            break;

        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;

        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
        break;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        EndPaint(hWnd, &ps);
        break;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// ============================================================
// 设置对话框过程
// ============================================================
INT_PTR CALLBACK SettingsDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static config::Settings settings;

    switch (msg)
    {
    case WM_INITDIALOG:
    {
        // 加载当前配置
        settings = config::Load();

        // 设置复选框状态
        CheckDlgButton(hDlg, IDC_CK_CUSTOM_KIT, settings.custom_musickit ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_CK_FLASHBANG, settings.custom_flashbang ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_CK_LOW_MEMORY, settings.low_memory ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_CK_SHOW_MVP, settings.show_mvp ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_CK_USE_OGG, settings.ogg ? BST_CHECKED : BST_UNCHECKED);

        // 设置音量编辑框
        wchar_t volText[32];
        swprintf_s(volText, L"%.2f", settings.volume);
        SetDlgItemTextW(hDlg, IDC_EDIT_VOL, volText);

        // 居中
        RECT rc;
        GetWindowRect(GetParent(hDlg), &rc);
        int x = rc.left + (rc.right - rc.left) / 2 - 175;
        int y = rc.top + (rc.bottom - rc.top) / 2 - 140;
        SetWindowPos(hDlg, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

        std::cout << "[设置] 对话框打开，加载配置成功。" << std::endl;
        return TRUE;
    }

    case WM_COMMAND:
    {
        WORD id = LOWORD(wParam);
        if (id == IDOK)
        {
            // 读取复选框状态
            settings.custom_musickit = (IsDlgButtonChecked(hDlg, IDC_CK_CUSTOM_KIT) == BST_CHECKED);
            settings.custom_flashbang = (IsDlgButtonChecked(hDlg, IDC_CK_FLASHBANG) == BST_CHECKED);
            settings.low_memory = (IsDlgButtonChecked(hDlg, IDC_CK_LOW_MEMORY) == BST_CHECKED);
            settings.show_mvp = (IsDlgButtonChecked(hDlg, IDC_CK_SHOW_MVP) == BST_CHECKED);
            settings.ogg = (IsDlgButtonChecked(hDlg, IDC_CK_USE_OGG) == BST_CHECKED);

            // 读取音量
            wchar_t volText[32];
            GetDlgItemTextW(hDlg, IDC_EDIT_VOL, volText, 32);
            try {
                float v = std::stof(volText);
                if (v >= 0.0f && v <= 1.0f)
                    settings.volume = v;
                else
                    MessageBoxW(hDlg, L"音量必须在 0.0 到 1.0 之间！", L"输入错误", MB_OK | MB_ICONWARNING);
            }
            catch (...) {
                MessageBoxW(hDlg, L"音量格式错误，请输入有效的数字！", L"输入错误", MB_OK | MB_ICONWARNING);
            }

            // 保存配置
            if (config::Save(settings))
            {
                std::cout << "[设置] 配置已保存。" << std::endl;
                EndDialog(hDlg, IDOK);
            }
            else
            {
                MessageBoxW(hDlg, L"保存配置失败！", L"错误", MB_OK | MB_ICONERROR);
            }
            return TRUE;
        }
        else if (id == IDCANCEL)
        {
            std::cout << "[设置] 用户取消，配置未修改。" << std::endl;
            EndDialog(hDlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    }
    return FALSE;
}

INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
