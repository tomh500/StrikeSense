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
#include <thread>

// vcpkg 库测试
#include <nlohmann/json.hpp>
#include <httplib.h>
#include <yaml-cpp/yaml.h>

#define MAX_LOADSTRING 100

// 全局变量:
HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];
Console g_Console;

// GSI 调试开关 — 1=打印原始 GSI JSON 到控制台
int g_debugGSI = 1;

// 存储 GSI cfg 路径
std::wstring g_gsiCfgPath;

// GSI HTTP 服务器实例
httplib::Server* g_gsiServer = nullptr;
std::thread g_gsiServerThread;

// 前向声明
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    ConfirmPathDlgProc(HWND, UINT, WPARAM, LPARAM);
void                StartGSIServer();
void                StopGSIServer();

// ----------------------------------------------------------
static bool TestVcpkgLibraries()
{
    try {
        nlohmann::json j = {{"name", "StrikeSense"}, {"version", 1.0}, {"debug", true}};
        (void)j.dump();
    }
    catch (...) { return false; }
    try {
        YAML::Node node; node["test"] = "hello";
        (void)YAML::Dump(node);
    }
    catch (...) { return false; }
    httplib::Client cli("http://localhost:8080");
    (void)cli;
    return true;
}

// ============================================================
// GSI HTTP POST 处理函数 — httplib 接收到 CS2 发送的 JSON
// ============================================================
static void HandleGSIRequest(const httplib::Request& req, httplib::Response& res)
{
    // req.body 包含 CS2 发来的完整 JSON
    std::string rawJson = req.body;

    // 调试输出
    if (g_debugGSI)
    {
        std::cout << "=== GSI 原始数据 ===" << std::endl;
        std::cout << rawJson << std::endl;
        std::cout << "====================" << std::endl;
    }

    // 解析 JSON
    try {
        nlohmann::json j = nlohmann::json::parse(rawJson);

        // 后续：提取 provider、map、player 等字段
        // 这里先做基础解析
        if (j.contains("provider"))
        {
            auto& prov = j["provider"];
            if (prov.contains("name"))
                std::cout << "[GSI] Provider: " << prov["name"].get<std::string>() << std::endl;
            if (prov.contains("steamid"))
                std::cout << "[GSI] SteamID: " << prov["steamid"].get<std::string>() << std::endl;
        }
        if (j.contains("map"))
        {
            auto& map = j["map"];
            if (map.contains("name"))
                std::cout << "[GSI] 地图: " << map["name"].get<std::string>() << std::endl;
        }
        if (j.contains("player"))
        {
            auto& player = j["player"];
            if (player.contains("name"))
                std::wcout << L"[GSI] 玩家: " << player["name"].get<std::string>().c_str() << std::endl;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "[GSI] JSON 解析错误: " << e.what() << std::endl;
    }

    res.status = 200;
    res.set_content("OK", "text/plain");
}

// ============================================================
// 启动 GSI HTTP 服务器（端口 1009）
// ============================================================
void StartGSIServer()
{
    if (g_gsiServer)
        return;  // 已启动

    g_gsiServer = new httplib::Server();

    // 注册 POST 处理 — CS2 用 POST 发 GSI 数据
    g_gsiServer->Post("/", HandleGSIRequest);

    std::cout << "[GSI] HTTP 服务器启动中，监听 0.0.0.0:1009..." << std::endl;

    // 在后台线程启动
    g_gsiServerThread = std::thread([&]() {
        if (!g_gsiServer->listen("0.0.0.0", 1009))
        {
            std::cerr << "[GSI] 服务器启动失败！端口 1009 可能已被占用。" << std::endl;
        }
    });
    g_gsiServerThread.detach();

    std::cout << "[GSI] HTTP 服务器已启动！（线程已分离）" << std::endl;
}

// ============================================================
// 停止 GSI HTTP 服务器
// ============================================================
void StopGSIServer()
{
    if (g_gsiServer)
    {
        g_gsiServer->stop();
        delete g_gsiServer;
        g_gsiServer = nullptr;
        std::cout << "[GSI] HTTP 服务器已停止。" << std::endl;
    }
}

// ============================================================
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    g_Console.InitRedirection();

    if (!TestVcpkgLibraries())
    {
        MessageBoxW(nullptr, L"vcpkg 库初始化失败！", L"错误", MB_ICONERROR);
        return FALSE;
    }

    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_STRIKESENSE, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    if (!InitInstance(hInstance, nCmdShow))
        return FALSE;

    // 启动 GSI HTTP 服务器
    StartGSIServer();

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

    StopGSIServer();
    return (int)msg.wParam;
}

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

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance;
    // 较小的主窗口: 800x550
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
static std::wstring GetCS2CfgPath()
{
    std::wstring saved = strikesense::LoadSavedCfgPath();
    if (!saved.empty() && fs::exists(saved))
        return saved;

    std::wcout << L"[GSI] 从注册表读取 Steam 路径..." << std::endl;
    std::wstring steamPath = strikesense::GetSteamPathFromRegistry();
    if (steamPath.empty())
    {
        std::wcout << L"[GSI] 注册表读取失败！" << std::endl;
        return L"";
    }
    std::wcout << L"[GSI] Steam 路径: " << steamPath << std::endl;

    std::wcout << L"[GSI] 查找 CS2 安装目录..." << std::endl;
    std::wstring cs2Dir = strikesense::FindCS2InstallDir(steamPath);
    if (cs2Dir.empty())
    {
        std::wcout << L"[GSI] 未找到 CS2 (appmanifest_730.acf)！" << std::endl;
        return L"";
    }
    std::wcout << L"[GSI] CS2 安装目录: " << cs2Dir << std::endl;

    std::wstring cfgPath = strikesense::GetCS2CfgPath(cs2Dir);
    std::wcout << L"[GSI] cfg 目录: " << cfgPath << std::endl;

    return cfgPath;
}

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
            // 删除已安装的配置文件
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