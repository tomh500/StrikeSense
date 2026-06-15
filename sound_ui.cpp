#include "sound_ui.h"
#include "config.h"
#include <windows.h>
#include <gdiplus.h>
#include <commdlg.h>
#include <iostream>
#include <string>

#pragma comment(lib, "gdiplus.lib")

namespace sound_ui {

static HWND     s_hwndPanel = nullptr;
static HWND     s_parentHwnd = nullptr;
static int      s_panelW = 600;

static ULONG_PTR s_gdiToken = 0;

struct SndRow {
    int   id;
    RECT  btnRect;
};

static SndRow s_rows[6] = {
    { 1,  {0}}, { 2,  {0}}, { 3,  {0}},
    { 4,  {0}}, { 5,  {0}}, { -1, {0}},
};

// ----------------------------------------------------------
// 辅助：获取文件名字符串
// ----------------------------------------------------------
static std::wstring GetDisplayName(int id)
{
    config::Settings cfg = config::Load();
    const std::wstring* ptr = nullptr;
    switch (id) {
    case 1:  ptr = &cfg.snd_1; break;
    case 2:  ptr = &cfg.snd_2; break;
    case 3:  ptr = &cfg.snd_3; break;
    case 4:  ptr = &cfg.snd_4; break;
    case 5:  ptr = &cfg.snd_5; break;
    case -1: ptr = &cfg.snd_extra; break;
    default: return L"";
    }
    if (ptr && !ptr->empty()) {
        std::filesystem::path p(*ptr);
        return p.filename().wstring();
    }
    return L"默认";
}

// ----------------------------------------------------------
static void DrawSingleLine(Gdiplus::Graphics& g, int y, int idx, int panelW)
{
    using namespace Gdiplus;

    SndRow& row = s_rows[idx];
    std::wstring name = GetDisplayName(row.id);

    // 行背景
    SolidBrush rowBrush(Color(255, 38, 38, 46));
    g.FillRectangle(&rowBrush, 8, y, panelW - 16, 34);

    // 标签文字
    const wchar_t* labels[] = {
        L"一杀 (1 kill)", L"二杀 (2 kills)", L"三杀 (3 kills)",
        L"四杀 (4 kills)", L"五杀 (5 kills)", L"多杀/死斗 (deathmatch)"
    };
    Font font(L"Microsoft YaHei", 11, FontStyleRegular);
    SolidBrush textBrush(Color(255, 220, 220, 230));
    g.DrawString(labels[idx], -1, &font, PointF(16.0f, y + 7.0f), &textBrush);

    // 文件名
    SolidBrush fileBrush(Color(255, 180, 180, 190));
    g.DrawString(name.c_str(), -1, &font, PointF(220.0f, y + 7.0f), &fileBrush);

    // 按钮
    int btnX = panelW - 70;
    int btnW = 55;
    int btnH = 24;
    SolidBrush btnBrush(Color(255, 60, 60, 72));
    Rect btnRect(btnX, y + 5, btnW, btnH);
    g.FillRectangle(&btnBrush, btnRect);

    // 按钮边框
    Pen btnPen(Color(255, 100, 100, 120), 1.0f);
    g.DrawRectangle(&btnPen, btnRect);

    // 按钮文字
    SolidBrush btnText(Color(255, 220, 220, 230));
    Font btnFont(L"Microsoft YaHei", 9, FontStyleRegular);
    g.DrawString(L"选择...", -1, &btnFont, PointF(btnX + 6.0f, y + 9.0f), &btnText);

    // 保存按钮矩形（屏幕坐标）
    row.btnRect.left = btnX;
    row.btnRect.top = y + 5;
    row.btnRect.right = btnX + btnW;
    row.btnRect.bottom = y + 5 + btnH;
}

// ----------------------------------------------------------
// 子窗口过程
// ----------------------------------------------------------
static LRESULT CALLBACK PanelProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_CREATE:
    {
        s_hwndPanel = hWnd;
        return 0;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);
        RECT rc;
        GetClientRect(hWnd, &rc);
        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;
        s_panelW = w;

        // 双缓冲
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBmp = CreateCompatibleBitmap(hdc, w, h);
        HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

        {
            Gdiplus::Graphics g(memDC);
            g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
            g.SetTextRenderingHint(Gdiplus::TextRenderingHintAntiAlias);

            // 标题
            Gdiplus::Font titleFont(L"Microsoft YaHei", 14, Gdiplus::FontStyleBold);
            Gdiplus::SolidBrush titleBrush(Gdiplus::Color(255, 80, 180, 240));
            g.DrawString(L"音效配置面板", -1, &titleFont, Gdiplus::PointF(12.0f, 6.0f), &titleBrush);

            // 6 行控件
            for (int i = 0; i < 6; ++i)
                DrawSingleLine(g, 32 + i * 38, i, w);

            // 底部提示
            Gdiplus::Font tipFont(L"Microsoft YaHei", 9, Gdiplus::FontStyleRegular);
            Gdiplus::SolidBrush tipBrush(Gdiplus::Color(255, 140, 140, 150));
            g.DrawString(L"点击 \"选择...\" 按钮自定义音效文件", -1, &tipFont,
                Gdiplus::PointF(12.0f, 272.0f), &tipBrush);
        }

        BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);
        SelectObject(memDC, oldBmp);
        DeleteObject(memBmp);
        DeleteDC(memDC);
        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        int mx = LOWORD(lParam);
        int my = HIWORD(lParam);
        for (int i = 0; i < 6; ++i)
        {
            RECT& r = s_rows[i].btnRect;
            if (mx >= r.left && mx < r.right && my >= r.top && my < r.bottom)
            {
                wchar_t path[MAX_PATH] = {};
                OPENFILENAMEW ofn = {};
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = s_parentHwnd;
                ofn.lpstrFilter = L"音频文件 (*.wav;*.ogg)\0*.wav;*.ogg\0所有文件\0*.*\0";
                ofn.lpstrFile = path;
                ofn.nMaxFile = MAX_PATH;
                ofn.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

                if (GetOpenFileNameW(&ofn))
                {
                    config::Settings cfg = config::Load();
                    switch (s_rows[i].id)
                    {
                    case 1: cfg.snd_1 = path; break;
                    case 2: cfg.snd_2 = path; break;
                    case 3: cfg.snd_3 = path; break;
                    case 4: cfg.snd_4 = path; break;
                    case 5: cfg.snd_5 = path; break;
                    case -1: cfg.snd_extra = path; break;
                    }
                    config::Save(cfg);
                    InvalidateRect(hWnd, nullptr, TRUE);
                }
                break;
            }
        }
        return 0;
    }

    case WM_SIZE:
        InvalidateRect(hWnd, nullptr, TRUE);
        return 0;

    case WM_ERASEBKGND:
        return TRUE; // 禁止默认擦除背景

    case WM_DESTROY:
        return 0;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}

// ----------------------------------------------------------
bool Initialize(HWND hParent, HINSTANCE hInst)
{
    s_parentHwnd = hParent;

    // 初始化 GDI+
    Gdiplus::GdiplusStartupInput gdiInput;
    Gdiplus::GdiplusStartup(&s_gdiToken, &gdiInput, nullptr);

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = PanelProc;
    wc.hInstance = hInst;
    wc.hbrBackground = (HBRUSH)CreateSolidBrush(RGB(28, 28, 34));
    wc.lpszClassName = L"StrikeSense_SoundPanelGDI";
    RegisterClassExW(&wc);

    RECT rc;
    GetClientRect(hParent, &rc);

    s_hwndPanel = CreateWindowExW(0, L"StrikeSense_SoundPanelGDI", L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER,
        8, 8, rc.right - rc.left - 16, 300,
        hParent, nullptr, hInst, nullptr);

    if (!s_hwndPanel)
    {
        std::cerr << "[sound_ui] 面板创建失败" << std::endl;
        return false;
    }

    std::cout << "[sound_ui] GDI+ 面板初始化成功" << std::endl;
    return true;
}

void Render() {}
void HandleClick(int, int) {}
void Resize(int width, int height)
{
    if (s_hwndPanel && IsWindow(s_hwndPanel))
        SetWindowPos(s_hwndPanel, nullptr, 8, 8, width - 16, 300, SWP_NOZORDER);
}

void Shutdown()
{
    if (s_gdiToken)
        Gdiplus::GdiplusShutdown(s_gdiToken);
    if (s_hwndPanel && IsWindow(s_hwndPanel))
        DestroyWindow(s_hwndPanel);
    s_hwndPanel = nullptr;
}

bool NeedsRedraw() { return false; }

} // namespace sound_ui