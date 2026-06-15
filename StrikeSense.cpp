// StrikeSense.cpp : 定义应用程序的入口点。
//

#include "framework.h"
#include "StrikeSense.h"

#include "console.h"
#include "steam_helper.h"
#include <iostream>
#include <cstdlib>
#include <filesystem>
#include <ShlObj.h>

// vcpkg 库测试 — include 确认它们可用
#include <nlohmann/json.hpp>
#include <httplib.h>
#include <yaml-cpp/yaml.h>

#define MAX_LOADSTRING 100

// 全局变量:
HINSTANCE hInst;                                // 当前实例
WCHAR szTitle[MAX_LOADSTRING];                  // 标题栏文本
WCHAR szWindowClass[MAX_LOADSTRING];            // 主窗口类名
Console g_Console;                              // 调试输出窗口管理

// 此代码模块中包含的函数的前向声明:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    ConfirmPathDlgProc(HWND, UINT, WPARAM, LPARAM);

// 存储 GSI cfg 路径（由对话框填写）
std::wstring g_gsiCfgPath;

// 命令行测试 vcpkg 库（在 main 之前调用）
static bool TestVcpkgLibraries()
{
    try {
        nlohmann::json j = {{"name", "StrikeSense"}, {"version", 1.0}, {"debug", true}};
        (void)j.dump();
    }
    catch (...) { return false; }

    try {
        YAML::Node node;
        node["test"] = "hello";
        (void)YAML::Dump(node);
    }
    catch (...) { return false; }

    httplib::Client cli("http://localhost:8080");
    (void)cli;

    return true;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // === 第一步：初始化输出重定向 ===
    g_Console.InitRedirection();

    // 验证 vcpkg 库链接正确
    if (!TestVcpkgLibraries())
    {
        MessageBoxW(nullptr, L"vcpkg 库初始化失败！", L"错误", MB_ICONERROR);
        return FALSE;
    }

    // 初始化全局字符串
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_STRIKESENSE, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // 执行应用程序初始化:
    if (!InitInstance (hInstance, nCmdShow))
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

    return (int) msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_STRIKESENSE));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_STRIKESENSE);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));
    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance;
   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);
   if (!hWnd) return FALSE;
   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);
   return TRUE;
}

// ============================================================
// 获取 CS2 cfg 路径的逻辑
// ============================================================
static std::wstring GetCS2CfgPath()
{
    // 1. 优先读取本地保存的配置
    std::wstring saved = strikesense::LoadSavedCfgPath();
    if (!saved.empty() && fs::exists(saved))
        return saved;

    // 2. 通过注册表找 Steam
    std::wcout << L"[GSI] 从注册表读取 Steam 路径..." << std::endl;
    std::wstring steamPath = strikesense::GetSteamPathFromRegistry();
    if (steamPath.empty())
    {
        std::wcout << L"[GSI] 注册表读取失败！" << std::endl;
        return L"";
    }
    std::wcout << L"[GSI] Steam 路径: " << steamPath << std::endl;

    // 3. 找 CS2 安装目录
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
// 路径确认对话框过程
// ============================================================
INT_PTR CALLBACK ConfirmPathDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_INITDIALOG:
    {
        // 显示路径
        HWND hEdit = GetDlgItem(hDlg, IDC_PATH_LABEL);
        if (hEdit)
            SetWindowTextW(hEdit, g_gsiCfgPath.c_str());

        // 居中
        RECT rc;
        GetWindowRect(GetParent(hDlg), &rc);
        int x = rc.left + (rc.right - rc.left) / 2 - 225;
        int y = rc.top + (rc.bottom - rc.top) / 2 - 90;
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
            // 用户确认 → 写入 GSI 配置文件
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

        case IDC_BROWSE_BTN:
        {
            // 用户选择浏览 → 打开文件夹选择对话框
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
                    if (hEdit)
                        SetWindowTextW(hEdit, g_gsiCfgPath.c_str());
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
// 处理 "创建 GSI 配置文件"
// ============================================================
static void OnCreateGSIConfig(HWND hWnd)
{
    std::wcout << L"[GSI] === 开始创建 GSI 配置文件 ===" << std::endl;

    // 获取 CS2 cfg 路径
    g_gsiCfgPath = GetCS2CfgPath();
    if (g_gsiCfgPath.empty())
    {
        std::wcout << L"[GSI] 无法自动检测 CS2 路径，弹出浏览框" << std::endl;

        // 自动检测失败 → 直接弹出浏览对话框选择目录
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

    // 显示路径确认对话框
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
            break;

        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
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

// "关于"框的消息处理程序。
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