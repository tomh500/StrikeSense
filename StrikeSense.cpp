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
#include <filesystem>
#include <ShlObj.h>
#include <commdlg.h>
#include <string>

namespace fs = std::filesystem;

#define MAX_LOADSTRING 100

HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];
Console g_Console;
std::wstring g_gsiCfgPath;

// 主窗口上的子控件句柄
HWND g_hLabel1 = nullptr, g_hLabel2 = nullptr, g_hLabel3 = nullptr, g_hLabel4 = nullptr, g_hLabel5 = nullptr, g_hLabelExtra = nullptr;

ATOM MyRegisterClass(HINSTANCE);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK About(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK ConfirmPathDlgProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK SettingsDlgProc(HWND, UINT, WPARAM, LPARAM);

static std::wstring GetCS2CfgPath();
static void OnCreateGSIConfig(HWND);
static void OnBrowseSound(HWND hWnd, int soundId);
static void RefreshSoundLabels(HWND hWnd);

// ============================================================
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR lpCmdLine,
                     _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    g_Console.InitRedirection();
    config::EnsureDirectoriesExist();
    config::Load();
    sound::Init();
    sound::PreloadSounds();

    if (gsi::Initialize()) gsi::StartServer();

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

    gsi::StopServer(); gsi::Cleanup();
    sound::Quit();
    return (int)msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(wcex);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
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
        CW_USEDEFAULT, 0, 650, 500,
        nullptr, nullptr, hInstance, nullptr);
    if (!hWnd) return FALSE;
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    return TRUE;
}

// ============================================================
// 文件浏览
// ============================================================
static void OnBrowseSound(HWND hWnd, int soundId)
{
    wchar_t path[MAX_PATH] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFilter = L"音频文件 (*.wav;*.ogg)\0*.wav;*.ogg\0所有文件\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

    if (!GetOpenFileNameW(&ofn)) return;

    config::Settings cfg = config::Load();
    switch (soundId) {
    case 1: cfg.snd_1 = path; break; case 2: cfg.snd_2 = path; break;
    case 3: cfg.snd_3 = path; break; case 4: cfg.snd_4 = path; break;
    case 5: cfg.snd_5 = path; break; case -1: cfg.snd_extra = path; break;
    }
    config::Save(cfg);
    RefreshSoundLabels(hWnd);
}

// ============================================================
// 刷新标签文本
// ============================================================
static void RefreshSoundLabels(HWND hWnd)
{
    config::Settings cfg = config::Load();

    struct { HWND hLabel; std::wstring path; const wchar_t* defName; } items[] = {
        { g_hLabel1, cfg.snd_1, L"1.wav (默认)" },
        { g_hLabel2, cfg.snd_2, L"2.wav (默认)" },
        { g_hLabel3, cfg.snd_3, L"3.wav (默认)" },
        { g_hLabel4, cfg.snd_4, L"4.wav (默认)" },
        { g_hLabel5, cfg.snd_5, L"5.wav (默认)" },
        { g_hLabelExtra, cfg.snd_extra, L"deathmatch.wav (默认)" },
    };

    for (auto& item : items)
    {
        if (!item.hLabel || !IsWindow(item.hLabel)) continue;
        std::wstring text;
        if (!item.path.empty()) {
            fs::path p(item.path);
            text = p.filename().wstring();
        } else {
            text = item.defName;
        }
        SetWindowTextW(item.hLabel, text.c_str());
    }
}

// ============================================================
static std::wstring GetCS2CfgPath()
{
    std::wstring saved = strikesense::LoadSavedCfgPath();
    if (!saved.empty() && fs::exists(saved)) return saved;
    std::wstring steamPath = strikesense::GetSteamPathFromRegistry();
    if (steamPath.empty()) return L"";
    std::wstring cs2Dir = strikesense::FindCS2InstallDir(steamPath);
    if (cs2Dir.empty()) return L"";
    return strikesense::GetCS2CfgPath(cs2Dir);
}

static void OnCreateGSIConfig(HWND hWnd)
{
    g_gsiCfgPath = GetCS2CfgPath();
    if (g_gsiCfgPath.empty())
    {
        wchar_t path[MAX_PATH] = {};
        BROWSEINFOW bi = {}; bi.hwndOwner = hWnd;
        bi.lpszTitle = L"无法自动检测 CS2 路径，请手动选择 game/csgo/cfg 目录";
        bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
        LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
        if (!pidl) return;
        if (SHGetPathFromIDListW(pidl, path)) g_gsiCfgPath = path;
        IMalloc* pMalloc = nullptr;
        if (SUCCEEDED(SHGetMalloc(&pMalloc))) { pMalloc->Free(pidl); pMalloc->Release(); }
        if (g_gsiCfgPath.empty()) return;
    }
    DialogBoxW(hInst, MAKEINTRESOURCEW(IDD_CONFIRM_PATH), hWnd, ConfirmPathDlgProc);
}

INT_PTR CALLBACK ConfirmPathDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_INITDIALOG:
        SetDlgItemTextW(hDlg, IDC_PATH_LABEL, g_gsiCfgPath.c_str());
        { RECT rc; GetWindowRect(GetParent(hDlg), &rc);
          SetWindowPos(hDlg, nullptr, rc.left+(rc.right-rc.left)/2-225,
                       rc.top+(rc.bottom-rc.top)/2-108, 0,0, SWP_NOSIZE|SWP_NOZORDER); }
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDYES:
            if (strikesense::WriteGSIConfig(g_gsiCfgPath)) {
                strikesense::SaveCfgPath(g_gsiCfgPath);
                MessageBoxW(hDlg, L"GSI 配置文件已成功创建！\n请重启 CS2 以生效。", L"成功", MB_OK);
            } else MessageBoxW(hDlg, L"配置文件写入失败！", L"错误", MB_OK);
            EndDialog(hDlg, IDYES); return TRUE;
        case IDC_DELETE_CFG: {
            fs::path f = fs::path(g_gsiCfgPath) / L"gamestate_integration_square.cfg";
            std::error_code ec; fs::remove(f, ec);
            MessageBoxW(hDlg, ec ? L"删除失败！" : L"GSI 配置文件已删除！", L"提示", MB_OK);
            return TRUE;
        }
        case IDC_BROWSE_BTN: {
            wchar_t p[MAX_PATH] = {}; BROWSEINFOW bi = {};
            bi.hwndOwner = hDlg; bi.lpszTitle = L"选择 CS2 cfg 目录";
            bi.ulFlags = BIF_RETURNONLYFSDIRS;
            LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
            if (pidl) {
                SHGetPathFromIDListW(pidl, p); g_gsiCfgPath = p;
                SetDlgItemTextW(hDlg, IDC_PATH_LABEL, p);
                IMalloc* m = nullptr;
                if (SUCCEEDED(SHGetMalloc(&m))) { m->Free(pidl); m->Release(); }
            }
            return TRUE;
        }
        case IDCANCEL: EndDialog(hDlg, IDCANCEL); return TRUE;
        }
    }
    return FALSE;
}

// ============================================================
// WndProc
// ============================================================
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        std::cout << "StrikeSense 窗口已创建。" << std::endl;

        // 标题标签
        CreateWindowW(L"STATIC", L"StrikeSense - 音效配置面板",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            15, 5, 400, 24, hWnd, nullptr, hInst, nullptr);

        // GSI 状态标签
        CreateWindowW(L"STATIC", L"GSI: 启动中...",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            15, 30, 500, 18, hWnd, nullptr, hInst, nullptr);

        // 6 行音效选择控件
        const wchar_t* names[] = { L"一杀", L"二杀", L"三杀", L"四杀", L"五杀", L"多杀/死斗" };

        for (int i = 0; i < 6; ++i)
        {
            int y = 56 + i * 34;
            // 名称标签
            CreateWindowW(L"STATIC", names[i],
                WS_CHILD | WS_VISIBLE | SS_LEFT,
                15, y + 4, 60, 20, hWnd, nullptr, hInst, nullptr);

            // 文件名标签（带边框）
            CreateWindowW(L"STATIC", L"默认",
                WS_CHILD | WS_VISIBLE | SS_LEFT | WS_BORDER | SS_SUNKEN,
                80, y + 2, 350, 22, hWnd, (HMENU)(INT_PTR)(2001 + i), hInst, nullptr);

            // 浏览按钮
            CreateWindowW(L"BUTTON", L"...",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                440, y + 2, 35, 22, hWnd, (HMENU)(INT_PTR)(2101 + i), hInst, nullptr);
        }

        // 保存标签句柄（使用控件 ID 获取）
        g_hLabel1 = GetDlgItem(hWnd, 2001);
        g_hLabel2 = GetDlgItem(hWnd, 2002);
        g_hLabel3 = GetDlgItem(hWnd, 2003);
        g_hLabel4 = GetDlgItem(hWnd, 2004);
        g_hLabel5 = GetDlgItem(hWnd, 2005);
        g_hLabelExtra = GetDlgItem(hWnd, 2006);

        // 底部提示
        CreateWindowW(L"STATIC", L"点击 [...] 按钮选择自定义音效文件。默认从 %UserProfile%\\StrikeSense\\snd\\ 读取。",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            15, 270, 600, 18, hWnd, nullptr, hInst, nullptr);

        RefreshSoundLabels(hWnd);
        break;
    }

    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        // 菜单项
        switch (wmId)
        {
        case IDM_CREATE_GSI_CFG: OnCreateGSIConfig(hWnd); break;
        case IDM_DEBUGGER: g_Console.ShowDebugger(hInst, hWnd); break;
        case IDM_ABOUT: DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About); break;
        case IDM_SETTINGS:
            DialogBoxW(hInst, MAKEINTRESOURCEW(IDD_SETTINGS), hWnd, SettingsDlgProc);
            RefreshSoundLabels(hWnd);
            break;
        case IDM_EXIT: DestroyWindow(hWnd); break;
        default: return DefWindowProc(hWnd, msg, wParam, lParam);
        }

        // 音效浏览按钮
        if (wmId >= 2101 && wmId <= 2106)
        {
            int ids[] = { 1, 2, 3, 4, 5, -1 };
            OnBrowseSound(hWnd, ids[wmId - 2101]);
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
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

INT_PTR CALLBACK SettingsDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static config::Settings s;

    switch (msg)
    {
    case WM_INITDIALOG:
        s = config::Load();
        CheckDlgButton(hDlg, IDC_CK_CUSTOM_KIT, s.custom_musickit ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_CK_ENABLE_KILL_SOUND, s.enable_kill_sound ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_CK_FLASHBANG, s.custom_flashbang ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_CK_LOW_MEMORY, s.low_memory ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_CK_SHOW_MVP, s.show_mvp ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_CK_USE_OGG, s.ogg ? BST_CHECKED : BST_UNCHECKED);
        { wchar_t t[32]; swprintf_s(t, L"%.2f", s.volume); SetDlgItemTextW(hDlg, IDC_EDIT_VOL, t); }
        { RECT rc; GetWindowRect(GetParent(hDlg), &rc);
          SetWindowPos(hDlg, nullptr, rc.left+(rc.right-rc.left)/2-175,
                       rc.top+(rc.bottom-rc.top)/2-115, 0,0, SWP_NOSIZE|SWP_NOZORDER); }
        return TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK) {
            s.custom_musickit = (IsDlgButtonChecked(hDlg, IDC_CK_CUSTOM_KIT) == BST_CHECKED);
            s.enable_kill_sound = (IsDlgButtonChecked(hDlg, IDC_CK_ENABLE_KILL_SOUND) == BST_CHECKED);
            s.custom_flashbang = (IsDlgButtonChecked(hDlg, IDC_CK_FLASHBANG) == BST_CHECKED);
            s.low_memory = (IsDlgButtonChecked(hDlg, IDC_CK_LOW_MEMORY) == BST_CHECKED);
            s.show_mvp = (IsDlgButtonChecked(hDlg, IDC_CK_SHOW_MVP) == BST_CHECKED);
            s.ogg = (IsDlgButtonChecked(hDlg, IDC_CK_USE_OGG) == BST_CHECKED);
            wchar_t t[32]; GetDlgItemTextW(hDlg, IDC_EDIT_VOL, t, 32);
            try { float v = std::stof(t); if (v>=0.0f && v<=1.0f) s.volume = v; } catch(...){}
            config::Save(s);
            EndDialog(hDlg, IDOK);
            return TRUE;
        }
        if (LOWORD(wParam) == IDCANCEL) { EndDialog(hDlg, IDCANCEL); return TRUE; }
        break;
    }
    return FALSE;
}

INT_PTR CALLBACK About(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (msg) {
    case WM_INITDIALOG: return (INT_PTR)TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL) EndDialog(hDlg, LOWORD(wParam));
        return (INT_PTR)TRUE;
    }
    return (INT_PTR)FALSE;
}