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
#include <gdiplus.h>
#include <commdlg.h>
#include <string>

#pragma comment(lib, "gdiplus.lib")

namespace fs = std::filesystem;

#define MAX_LOADSTRING 100

HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];
Console g_Console;
std::wstring g_gsiCfgPath;

static ULONG_PTR g_gdiToken = 0;

ATOM                MyRegisterClass(HINSTANCE);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    ConfirmPathDlgProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    SettingsDlgProc(HWND, UINT, WPARAM, LPARAM);

static std::wstring GetCS2CfgPath();
static void OnCreateGSIConfig(HWND);

// ============================================================
// 音效文件浏览
// ============================================================
static void BrowseAndSave(int soundId, HWND hWnd)
{
    wchar_t path[MAX_PATH] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hWnd;
    ofn.lpstrFilter = L"音频文件 (*.wav;*.ogg)\0*.wav;*.ogg\0所有文件\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

    if (!GetOpenFileNameW(&ofn))
        return;

    config::Settings cfg = config::Load();
    switch (soundId) {
    case 1: cfg.snd_1 = path; break; case 2: cfg.snd_2 = path; break;
    case 3: cfg.snd_3 = path; break; case 4: cfg.snd_4 = path; break;
    case 5: cfg.snd_5 = path; break; case -1: cfg.snd_extra = path; break;
    }
    config::Save(cfg);
    InvalidateRect(hWnd, nullptr, TRUE);
}

// ============================================================
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR lpCmdLine,
                     _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // 初始化 GDI+
    Gdiplus::GdiplusStartupInput gdiInput;
    Gdiplus::GdiplusStartup(&g_gdiToken, &gdiInput, nullptr);

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

    std::cout << "[主程序] 正在停止 GSI 服务器..." << std::endl;
    gsi::StopServer(); gsi::Cleanup();
    sound::Quit();
    Gdiplus::GdiplusShutdown(g_gdiToken);
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
        CW_USEDEFAULT, 0, 620, 520,
        nullptr, nullptr, hInstance, nullptr);
    if (!hWnd) return FALSE;
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    return TRUE;
}

// ============================================================
// GDI+ 绘制音效面板
// ============================================================
static void PaintSoundPanel(HDC hdc, RECT& rc)
{
    using namespace Gdiplus;
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;

    // 双缓冲
    HDC memDC = CreateCompatibleDC(hdc);
    HBITMAP memBmp = CreateCompatibleBitmap(hdc, w, h);
    HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

    {
        Graphics g(memDC);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintAntiAlias);

        // 整体背景
        SolidBrush bg(Color(255, 22, 22, 30));
        g.FillRectangle(&bg, 0, 0, w, h);

        // 标题
        Font titleFont(L"Microsoft YaHei", 16, FontStyleBold);
        SolidBrush titleBrush(Color(255, 80, 180, 250));
        g.DrawString(L"音效配置", -1, &titleFont, PointF(12.0f, 8.0f), &titleBrush);

        // GSI 状态
        wchar_t status[256];
        swprintf_s(status, L"GSI 状态: %s | 音效: %s",
            gsi::IsRunning() ? L"运行中" : L"未启动",
            config::Load().enable_kill_sound ? L"已启用" : L"已禁用");
        Font statusFont(L"Microsoft YaHei", 9, FontStyleRegular);
        SolidBrush statusBrush(Color(255, 140, 140, 160));
        g.DrawString(status, -1, &statusFont, PointF(12.0f, 32.0f), &statusBrush);

        config::Settings cfg = config::Load();

        struct { int id; const wchar_t* label; } sounds[] = {
            {1, L"一杀"}, {2, L"二杀"}, {3, L"三杀"},
            {4, L"四杀"}, {5, L"五杀"}, {-1, L"多杀/死斗"}
        };

        Font rowFont(L"Microsoft YaHei", 11, FontStyleRegular);
        SolidBrush rowBg(Color(255, 35, 35, 44));
        SolidBrush rowText(Color(255, 210, 210, 220));
        SolidBrush btnBg(Color(255, 55, 55, 68));
        Pen btnBorder(Color(255, 90, 90, 110));

        for (int i = 0; i < 6; ++i)
        {
            int y = 58 + i * 36;

            // 行背景
            g.FillRectangle(&rowBg, 10, y, w - 20, 32);

            // 标签
            g.DrawString(sounds[i].label, -1, &rowFont, PointF(18.0f, y + 5.0f), &rowText);

            // 文件名
            const std::wstring* ptr = nullptr;
            switch (sounds[i].id) {
            case 1: ptr = &cfg.snd_1; break; case 2: ptr = &cfg.snd_2; break;
            case 3: ptr = &cfg.snd_3; break; case 4: ptr = &cfg.snd_4; break;
            case 5: ptr = &cfg.snd_5; break; case -1: ptr = &cfg.snd_extra; break;
            }

            std::wstring fname = L"默认";
            SolidBrush fnameBrush(Color(255, 150, 150, 160));
            if (ptr && !ptr->empty()) {
                fs::path p(*ptr);
                fname = p.filename().wstring();
                fnameBrush.SetColor(Color(255, 210, 210, 220));
            }
            g.DrawString(fname.c_str(), -1, &rowFont, PointF(100.0f, y + 5.0f), &fnameBrush);

            // 按钮
            int btnX = w - 80;
            Rect btnRect(btnX, y + 4, 60, 24);
            g.FillRectangle(&btnBg, btnRect);
            g.DrawRectangle(&btnBorder, btnRect);

            Font btnFont(L"Microsoft YaHei", 9, FontStyleRegular);
            SolidBrush btnText(Color(255, 210, 210, 220));
            g.DrawString(L"选择...", -1, &btnFont, PointF(btnX + 4.0f, y + 7.0f), &btnText);
        }

        // 底部
        Font tipFont(L"Microsoft YaHei", 9, FontStyleRegular);
        SolidBrush tipBrush(Color(255, 120, 120, 130));
        g.DrawString(L"点击 \"选择...\" 按钮自定义音效文件。默认从 %UserProfile%\\StrikeSense\\snd\\ 读取。",
            -1, &tipFont, PointF(12.0f, h - 24.0f), &tipBrush);
    }

    BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);
    SelectObject(memDC, oldBmp);
    DeleteObject(memBmp);
    DeleteDC(memDC);
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
    {
        SetDlgItemTextW(hDlg, IDC_PATH_LABEL, g_gsiCfgPath.c_str());
        RECT rc; GetWindowRect(GetParent(hDlg), &rc);
        SetWindowPos(hDlg, nullptr, rc.left + (rc.right - rc.left)/2 - 225,
            rc.top + (rc.bottom - rc.top)/2 - 108, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        return TRUE;
    }
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDYES:
            if (strikesense::WriteGSIConfig(g_gsiCfgPath))
            {
                strikesense::SaveCfgPath(g_gsiCfgPath);
                MessageBoxW(hDlg, L"GSI 配置文件已成功创建！\n请重启 CS2 以生效。", L"成功", MB_OK);
            }
            else MessageBoxW(hDlg, L"配置文件写入失败！", L"错误", MB_OK);
            EndDialog(hDlg, IDYES); return TRUE;
        case IDC_DELETE_CFG:
        {
            fs::path f = fs::path(g_gsiCfgPath) / L"gamestate_integration_square.cfg";
            std::error_code ec; fs::remove(f, ec);
            MessageBoxW(hDlg, ec ? L"删除失败！" : L"GSI 配置文件已删除！", L"提示", MB_OK);
            return TRUE;
        }
        case IDC_BROWSE_BTN:
        {
            wchar_t p[MAX_PATH] = {}; BROWSEINFOW bi = {};
            bi.hwndOwner = hDlg; bi.lpszTitle = L"选择 CS2 cfg 目录";
            bi.ulFlags = BIF_RETURNONLYFSDIRS;
            LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
            if (pidl)
            {
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
        std::cout << "StrikeSense 窗口已创建。" << std::endl;
        break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc;
        GetClientRect(hWnd, &rc);
        PaintSoundPanel(hdc, rc);
        EndPaint(hWnd, &ps);
        break;
    }

    case WM_LBUTTONDOWN:
    {
        int mx = LOWORD(lParam);
        int my = HIWORD(lParam);
        // 检查 6 个按钮区域
        for (int i = 0; i < 6; ++i)
        {
            int by = 58 + i * 36 + 4;
            RECT rc;
            GetClientRect(hWnd, &rc);
            int btnX = rc.right - rc.left - 80;
            if (mx >= btnX && mx <= btnX + 60 && my >= by && my <= by + 24)
            {
                int ids[] = { 1, 2, 3, 4, 5, -1 };
                BrowseAndSave(ids[i], hWnd);
                break;
            }
        }
        break;
    }

    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        switch (wmId)
        {
        case IDM_CREATE_GSI_CFG: OnCreateGSIConfig(hWnd); break;
        case IDM_DEBUGGER: g_Console.ShowDebugger(hInst, hWnd); break;
        case IDM_ABOUT: DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About); break;
        case IDM_SETTINGS:
            DialogBoxW(hInst, MAKEINTRESOURCEW(IDD_SETTINGS), hWnd, SettingsDlgProc);
            InvalidateRect(hWnd, nullptr, TRUE);
            break;
        case IDM_EXIT: DestroyWindow(hWnd); break;
        default: return DefWindowProc(hWnd, msg, wParam, lParam);
        }
        break;
    }

    case WM_ERASEBKGND:
        return TRUE; // 自己绘制背景，禁止默认擦除

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
        CheckDlgButton(hDlg, IDC_CK_FLASHBANG, s.custom_flashbang ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_CK_LOW_MEMORY, s.low_memory ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_CK_SHOW_MVP, s.show_mvp ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_CK_USE_OGG, s.ogg ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hDlg, IDC_CK_ENABLE_KILL_SOUND, s.enable_kill_sound ? BST_CHECKED : BST_UNCHECKED);
        {
            wchar_t t[32]; swprintf_s(t, L"%.2f", s.volume);
            SetDlgItemTextW(hDlg, IDC_EDIT_VOL, t);
        }
        {
            RECT rc; GetWindowRect(GetParent(hDlg), &rc);
            SetWindowPos(hDlg, nullptr,
                rc.left + (rc.right-rc.left)/2 - 175,
                rc.top + (rc.bottom-rc.top)/2 - 115,
                0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }
        return TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK)
        {
            s.custom_musickit = (IsDlgButtonChecked(hDlg, IDC_CK_CUSTOM_KIT) == BST_CHECKED);
            s.custom_flashbang = (IsDlgButtonChecked(hDlg, IDC_CK_FLASHBANG) == BST_CHECKED);
            s.low_memory = (IsDlgButtonChecked(hDlg, IDC_CK_LOW_MEMORY) == BST_CHECKED);
            s.show_mvp = (IsDlgButtonChecked(hDlg, IDC_CK_SHOW_MVP) == BST_CHECKED);
            s.ogg = (IsDlgButtonChecked(hDlg, IDC_CK_USE_OGG) == BST_CHECKED);
            s.enable_kill_sound = (IsDlgButtonChecked(hDlg, IDC_CK_ENABLE_KILL_SOUND) == BST_CHECKED);
            wchar_t t[32]; GetDlgItemTextW(hDlg, IDC_EDIT_VOL, t, 32);
            try { float v = std::stof(t); if (v >= 0.0f && v <= 1.0f) s.volume = v; } catch (...) {}
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
    switch (msg)
    {
    case WM_INITDIALOG: return (INT_PTR)TRUE;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
            EndDialog(hDlg, LOWORD(wParam));
        return (INT_PTR)TRUE;
    }
    return (INT_PTR)FALSE;
}