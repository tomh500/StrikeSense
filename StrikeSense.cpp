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
#include <commdlg.h>

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
// 打开文件选择对话框，返回选中的文件路径
// ============================================================
static std::wstring BrowseSoundFile(HWND hParent, const wchar_t* title)
{
    wchar_t path[MAX_PATH] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hParent;
    ofn.lpstrFilter = L"音频文件\0*.wav;*.ogg\0所有文件\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

    if (GetOpenFileNameW(&ofn))
        return path;
    return L"";
}

// ============================================================
// 获取音效标签文本（显示文件名或默认信息）
// ============================================================
static std::wstring GetSoundLabel(const std::wstring& customPath, const wchar_t* name, bool useOgg)
{
    if (!customPath.empty())
    {
        fs::path p(customPath);
        return p.filename().wstring();
    }
    std::wstring ext = useOgg ? L".ogg" : L".wav";
    return std::wstring(name) + ext + L" (默认)";
}

// ============================================================
// 刷新主窗口所有音效标签
// ============================================================
static void RefreshSoundLabels(HWND hWnd)
{
    config::Settings cfg = config::Load();

    auto setLabel = [&](int idc, const std::wstring& path, const wchar_t* name) {
        HWND hLabel = GetDlgItem(hWnd, idc);
        if (hLabel)
            SetWindowTextW(hLabel, GetSoundLabel(path, name, cfg.ogg).c_str());
    };

    setLabel(IDC_SND_LABEL_1, cfg.snd_1, L"1");
    setLabel(IDC_SND_LABEL_2, cfg.snd_2, L"2");
    setLabel(IDC_SND_LABEL_3, cfg.snd_3, L"3");
    setLabel(IDC_SND_LABEL_4, cfg.snd_4, L"4");
    setLabel(IDC_SND_LABEL_5, cfg.snd_5, L"5");
    setLabel(IDC_SND_LABEL_EXTRA, cfg.snd_extra, L"deathmatch");
}

// ============================================================
// 选择音效文件的通用处理
// ============================================================
static void OnBrowseSound(HWND hWnd, std::wstring config::Settings::*ptr, int labelId, const wchar_t* title)
{
    std::wstring path = BrowseSoundFile(hWnd, title);
    if (path.empty()) return;

    config::Settings cfg = config::Load();
    cfg.*ptr = path;
    config::Save(cfg);
    RefreshSoundLabels(hWnd);
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

    config::EnsureDirectoriesExist();
    config::Load();
    sound::Init();

    // 启动 GSI HTTP 服务器
    std::cout << "[主程序] 初始化 GSI 服务器..." << std::endl;
    if (gsi::Initialize())
    {
        std::cout << "[主程序] GSI 服务器初始化成功，启动监听..." << std::endl;
        gsi::StartServer();
    }
    else
        std::cerr << "[主程序] 警告：GSI 服务器初始化失败！" << std::endl;

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

    std::cout << "[主程序] 正在停止 GSI 服务器..." << std::endl;
    gsi::StopServer();
    gsi::Cleanup();

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
    HWND hWnd = CreateWindowW(szWindowClass, szTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, 820, 480,
        nullptr, nullptr, hInstance, nullptr);
    if (!hWnd) return FALSE;
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    return TRUE;
}

// CS2 cfg 路径
static std::wstring GetCS2CfgPath()
{
    std::wstring saved = strikesense::LoadSavedCfgPath();
    if (!saved.empty() && fs::exists(saved))
    {
        std::wcout << L"[GSI] 使用已保存的 cfg 路径: " << saved << std::endl;
        return saved;
    }
    std::wcout << L"[GSI] 从注册表读取 Steam 路径..." << std::endl;
    std::wstring steamPath = strikesense::GetSteamPathFromRegistry();
    if (steamPath.empty())
    {
        std::wcout << L"[GSI] 注册表读取失败！" << std::endl;
        return L"";
    }
    std::wstring cs2Dir = strikesense::FindCS2InstallDir(steamPath);
    if (cs2Dir.empty())
    {
        std::wcout << L"[GSI] 未找到 CS2！" << std::endl;
        return L"";
    }
    std::wstring cfgPath = strikesense::GetCS2CfgPath(cs2Dir);
    std::wcout << L"[GSI] cfg 目录: " << cfgPath << std::endl;
    return cfgPath;
}

// 路径确认对话框
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
            if (strikesense::WriteGSIConfig(g_gsiCfgPath))
            {
                strikesense::SaveCfgPath(g_gsiCfgPath);
                MessageBoxW(hDlg, L"GSI 配置文件已成功创建！\n请重启 CS2 以生效。",
                           L"成功", MB_OK | MB_ICONINFORMATION);
            }
            else
                MessageBoxW(hDlg, L"配置文件写入失败！", L"错误", MB_OK | MB_ICONERROR);
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
                MessageBoxW(hDlg, ec ? L"删除失败！" : L"GSI 配置文件已删除！",
                           L"提示", MB_OK | MB_ICONINFORMATION);
            }
            else
                MessageBoxW(hDlg, L"该目录下未安装 GSI 配置文件。", L"提示", MB_OK | MB_ICONINFORMATION);
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

static void OnCreateGSIConfig(HWND hWnd)
{
    g_gsiCfgPath = GetCS2CfgPath();
    if (g_gsiCfgPath.empty())
    {
        wchar_t path[MAX_PATH] = {};
        BROWSEINFOW bi = {};
        bi.hwndOwner = hWnd;
        bi.pszDisplayName = path;
        bi.lpszTitle = L"无法自动检测 CS2 路径，请手动选择 game/csgo/cfg 目录";
        bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
        LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
        if (!pidl) return;
        if (SHGetPathFromIDListW(pidl, path)) g_gsiCfgPath = path;
        IMalloc* pMalloc = nullptr;
        if (SUCCEEDED(SHGetMalloc(&pMalloc)))
        { pMalloc->Free(pidl); pMalloc->Release(); }
        if (g_gsiCfgPath.empty()) return;
    }
    DialogBoxW(hInst, MAKEINTRESOURCEW(IDD_CONFIRM_PATH), hWnd, ConfirmPathDlgProc);
}

// ============================================================
// WndProc — 包含主窗口子控件创建和处理
// ============================================================
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        std::cout << "StrikeSense 窗口已创建。" << std::endl;

        // ===== 创建音效选择控件 =====
        struct { int labelId; int btnId; const wchar_t* text; int y; }
        rows[] = {
            { IDC_SND_LABEL_1,  IDC_SND_BTN_1,  L"一杀:",  20 },
            { IDC_SND_LABEL_2,  IDC_SND_BTN_2,  L"二杀:",  50 },
            { IDC_SND_LABEL_3,  IDC_SND_BTN_3,  L"三杀:",  80 },
            { IDC_SND_LABEL_4,  IDC_SND_BTN_4,  L"四杀:",  110 },
            { IDC_SND_LABEL_5,  IDC_SND_BTN_5,  L"五杀:",  140 },
            { IDC_SND_LABEL_EXTRA, IDC_SND_BTN_EXTRA, L"多杀/死斗:", 170 },
        };

        for (auto& r : rows)
        {
            CreateWindowW(L"STATIC", r.text,
                          WS_CHILD | WS_VISIBLE | SS_RIGHT,
                          10, r.y, 80, 22, hWnd, nullptr, hInst, nullptr);

            // 标签（文件名显示）
            CreateWindowW(L"STATIC", L"(默认)",
                          WS_CHILD | WS_VISIBLE | SS_LEFT | WS_BORDER,
                          95, r.y, 300, 22, hWnd, (HMENU)(INT_PTR)r.labelId, hInst, nullptr);

            // 浏览按钮
            CreateWindowW(L"BUTTON", L"...",
                          WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                          405, r.y - 1, 30, 22, hWnd, (HMENU)(INT_PTR)r.btnId, hInst, nullptr);
        }

        // 刷新标签
        RefreshSoundLabels(hWnd);
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
            break;

        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;

        case IDM_SETTINGS:
            DialogBoxW(hInst, MAKEINTRESOURCEW(IDD_SETTINGS), hWnd, SettingsDlgProc);
            // 设置关闭后刷新标签
            RefreshSoundLabels(hWnd);
            break;

        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;

        // ===== 音效文件浏览按钮 =====
        case IDC_SND_BTN_1:
            OnBrowseSound(hWnd, &config::Settings::snd_1, IDC_SND_LABEL_1, L"选择一杀音效文件");
            break;
        case IDC_SND_BTN_2:
            OnBrowseSound(hWnd, &config::Settings::snd_2, IDC_SND_LABEL_2, L"选择二杀音效文件");
            break;
        case IDC_SND_BTN_3:
            OnBrowseSound(hWnd, &config::Settings::snd_3, IDC_SND_LABEL_3, L"选择三杀音效文件");
            break;
        case IDC_SND_BTN_4:
            OnBrowseSound(hWnd, &config::Settings::snd_4, IDC_SND_LABEL_4, L"选择四杀音效文件");
            break;
        case IDC_SND_BTN_5:
            OnBrowseSound(hWnd, &config::Settings::snd_5, IDC_SND_LABEL_5, L"选择五杀音效文件");
            break;
        case IDC_SND_BTN_EXTRA:
            OnBrowseSound(hWnd, &config::Settings::snd_extra, IDC_SND_LABEL_EXTRA, L"选择多杀/死斗音效文件");
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

// 设置对话框过程
INT_PTR CALLBACK SettingsDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static config::Settings settings;

    switch (msg)
    {
    case WM_INITDIALOG:
    {
        settings = config::Load();

        CheckDlgButton(hDlg, IDC_CK_CUSTOM_KIT, settings.custom_musickit ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_CK_FLASHBANG, settings.custom_flashbang ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_CK_LOW_MEMORY, settings.low_memory ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_CK_SHOW_MVP, settings.show_mvp ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_CK_USE_OGG, settings.ogg ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_CK_ENABLE_KILL_SOUND, settings.enable_kill_sound ? BST_CHECKED : BST_UNCHECKED);

        wchar_t volText[32];
        swprintf_s(volText, L"%.2f", settings.volume);
        SetDlgItemTextW(hDlg, IDC_EDIT_VOL, volText);

        RECT rc;
        GetWindowRect(GetParent(hDlg), &rc);
        int x = rc.left + (rc.right - rc.left) / 2 - 230;
        int y = rc.top + (rc.bottom - rc.top) / 2 - 210;
        SetWindowPos(hDlg, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

        return TRUE;
    }
    case WM_COMMAND:
    {
        WORD id = LOWORD(wParam);
        if (id == IDOK)
        {
            settings.custom_musickit = (IsDlgButtonChecked(hDlg, IDC_CK_CUSTOM_KIT) == BST_CHECKED);
            settings.custom_flashbang = (IsDlgButtonChecked(hDlg, IDC_CK_FLASHBANG) == BST_CHECKED);
            settings.low_memory = (IsDlgButtonChecked(hDlg, IDC_CK_LOW_MEMORY) == BST_CHECKED);
            settings.show_mvp = (IsDlgButtonChecked(hDlg, IDC_CK_SHOW_MVP) == BST_CHECKED);
            settings.ogg = (IsDlgButtonChecked(hDlg, IDC_CK_USE_OGG) == BST_CHECKED);
            settings.enable_kill_sound = (IsDlgButtonChecked(hDlg, IDC_CK_ENABLE_KILL_SOUND) == BST_CHECKED);

            wchar_t volText[32];
            GetDlgItemTextW(hDlg, IDC_EDIT_VOL, volText, 32);
            try {
                float v = std::stof(volText);
                if (v >= 0.0f && v <= 1.0f)
                    settings.volume = v;
            }
            catch (...) {}

            config::Save(settings);
            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        else if (id == IDCANCEL)
        {
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
    case WM_INITDIALOG: return (INT_PTR)TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
            EndDialog(hDlg, LOWORD(wParam));
        return (INT_PTR)TRUE;
    }
    return (INT_PTR)FALSE;
}