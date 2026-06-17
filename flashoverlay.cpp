#include "flashoverlay.h"
#include "config.h"

#include <windows.h>
#include <gdiplus.h>
#include <thread>
#include <atomic>
#include <memory>

#pragma comment(lib,"gdiplus.lib")

namespace flashoverlay
{
    static std::unique_ptr<Gdiplus::Image> s_image;
    static std::wstring s_loadedPath;
    static HWND s_hwnd = nullptr;
    static HINSTANCE s_hInst = nullptr;

    static std::atomic<int> s_alpha(0);
    static std::atomic<bool> s_visible(false);

    // =====================================================
    // 绘制窗口
    // =====================================================

    static void LoadImageIfNeeded()
{
    config::Settings cfg = config::Load();

    std::wstring path = cfg.flash_image;

    if (path.empty())
        path = config::GetDefaultFlashPath();

    if (path == s_loadedPath && s_image)
        return;

    s_image.reset();

    auto img = std::make_unique<Gdiplus::Image>(
        path.c_str()
    );

    if (img->GetLastStatus() == Gdiplus::Ok)
    {
        s_image = std::move(img);
        s_loadedPath = path;
    }
}

    static LRESULT CALLBACK OverlayProc(
        HWND hwnd,
        UINT msg,
        WPARAM wp,
        LPARAM lp)
    {
        switch (msg)
        {
case WM_PAINT:
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);

    RECT rc;
    GetClientRect(hwnd, &rc);

    Gdiplus::Graphics g(hdc);

    if (s_image)
    {
        g.DrawImage(
            s_image.get(),
            0,
            0,
            rc.right,
            rc.bottom
        );
    }

    EndPaint(hwnd, &ps);
    return 0;
}

        case WM_ERASEBKGND:
            return TRUE;
        }

        return DefWindowProc(hwnd,msg,wp,lp);
    }

    // =====================================================
    // 创建窗口
    // =====================================================

    void Initialize(HINSTANCE hInst)
    {
        s_hInst = hInst;

        WNDCLASSEXW wc = {};

        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = OverlayProc;
        wc.hInstance = hInst;
        wc.lpszClassName = L"StrikeSenseFlashOverlay";

        RegisterClassExW(&wc);

        int sw = GetSystemMetrics(SM_CXSCREEN);
        int sh = GetSystemMetrics(SM_CYSCREEN);

        s_hwnd = CreateWindowExW(
            WS_EX_TOPMOST |
            WS_EX_LAYERED |
            WS_EX_TRANSPARENT |
            WS_EX_TOOLWINDOW,

            wc.lpszClassName,
            L"",
            WS_POPUP,

            0,
            0,
            sw,
            sh,

            nullptr,
            nullptr,
            hInst,
            nullptr
        );

        SetLayeredWindowAttributes(
            s_hwnd,
            0,
            0,
            LWA_ALPHA
        );

        LoadImageIfNeeded();
    }

    // =====================================================
    // 淡入
    // =====================================================

    void Show()
    {
        if (!s_hwnd)
            return;
        LoadImageIfNeeded();
        s_visible = true;

    ShowWindow(
        s_hwnd,
        SW_SHOW
    );

        std::thread([]()
        {
            for (int a = 0; a <= 255; a += 15)
            {
                if (!s_visible)
                    return;

                s_alpha = a;

                SetLayeredWindowAttributes(
                    s_hwnd,
                    0,
                    (BYTE)a,
                    LWA_ALPHA
                );

                InvalidateRect(
                    s_hwnd,
                    nullptr,
                    FALSE
                );

                Sleep(10);
            }
        }).detach();
    }

    // =====================================================
    // 淡出
    // =====================================================

    void Hide()
    {
        if (!s_hwnd)
            return;

        s_visible = false;

        std::thread([]()
        {
            for (int a = 255; a >= 0; a -= 15)
            {
                SetLayeredWindowAttributes(
                    s_hwnd,
                    0,
                    (BYTE)a,
                    LWA_ALPHA
                );

                Sleep(10);
            }

            ShowWindow(
                s_hwnd,
                SW_HIDE
            );

        }).detach();
    }

    // =====================================================
    // 销毁
    // =====================================================

    void Shutdown()
{
    s_image.reset();

    if (s_hwnd)
    {
        DestroyWindow(s_hwnd);
        s_hwnd = nullptr;
    }
}
}