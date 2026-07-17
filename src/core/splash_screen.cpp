#include "splash_screen.h"
#include "resource.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <cmath>
#include <iostream>
#include <memory>
#include <objidl.h>
#include <string>
#include <vector>
#include <gdiplus.h>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace splashscreen {
namespace {

constexpr wchar_t kClassName[] = L"StrikeSenseSplashWindow";
constexpr double kIntroMs = 2000.0;
constexpr double kLogoMs = 220.0;
constexpr double kExitMs = 320.0;
constexpr double kFadeMs = 80.0;
constexpr DWORD kFrameDelayMs = 8;
constexpr int kTargetWidth = 460;

std::unique_ptr<Gdiplus::Image> g_boot_image;
ULONG_PTR g_class_registered_for = 0;

double now_ms()
{
    using clock = std::chrono::steady_clock;
    static const auto start = clock::now();
    const auto elapsed = clock::now() - start;
    return std::chrono::duration<double, std::milli>(elapsed).count();
}

float clamp01(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

float ease_in_out_cubic(float t)
{
    t = clamp01(t);
    if (t < 0.5f) return 4.0f * t * t * t;
    const float f = -2.0f * t + 2.0f;
    return 1.0f - (f * f * f) / 2.0f;
}

float ease_in_back(float t)
{
    t = clamp01(t);
    constexpr float c1 = 1.70158f;
    constexpr float c3 = c1 + 1.0f;
    return c3 * t * t * t - c1 * t * t;
}

float ease_out_expo(float t)
{
    t = clamp01(t);
    return t >= 1.0f ? 1.0f : 1.0f - std::pow(2.0f, -10.0f * t);
}

float exit_scale(float t)
{
    t = clamp01(t);
    if (t < 0.28f) {
        return 1.0f + 0.035f * ease_in_back(t / 0.28f);
    }
    const float shrink = ease_out_expo((t - 0.28f) / 0.72f);
    return 1.0f + (0.01f - 1.0f) * shrink;
}

int scaled(int value, float scale)
{
    return static_cast<int>(std::round(value * scale));
}

float dpi_scale_for_window(HWND hwnd)
{
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
        auto get_dpi = reinterpret_cast<GetDpiForWindowFn>(GetProcAddress(user32, "GetDpiForWindow"));
        if (get_dpi) return static_cast<float>(get_dpi(hwnd)) / 96.0f;
    }
    HDC dc = GetDC(hwnd);
    const int dpi = dc ? GetDeviceCaps(dc, LOGPIXELSX) : 96;
    if (dc) ReleaseDC(hwnd, dc);
    return static_cast<float>(dpi) / 96.0f;
}

std::unique_ptr<Gdiplus::Image> load_png_from_resource(HINSTANCE instance)
{
    HRSRC resource = FindResourceW(instance, MAKEINTRESOURCEW(IDB_BOOT_PNG), L"PNG");
    if (!resource) {
        std::cout << "[启动动画] 未找到内嵌 boot.png 资源。" << std::endl;
        return nullptr;
    }

    HGLOBAL loaded = LoadResource(instance, resource);
    const DWORD size = SizeofResource(instance, resource);
    const void* data = LockResource(loaded);
    if (!loaded || !data || size == 0) {
        std::cout << "[启动动画] boot.png 资源读取失败。" << std::endl;
        return nullptr;
    }

    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!memory) return nullptr;

    void* target = GlobalLock(memory);
    std::memcpy(target, data, size);
    GlobalUnlock(memory);

    IStream* stream = nullptr;
    if (FAILED(CreateStreamOnHGlobal(memory, TRUE, &stream))) {
        GlobalFree(memory);
        return nullptr;
    }

    std::unique_ptr<Gdiplus::Image> image(Gdiplus::Image::FromStream(stream));
    stream->Release();
    if (!image || image->GetLastStatus() != Gdiplus::Ok) {
        std::cout << "[启动动画] boot.png 解码失败。" << std::endl;
        return nullptr;
    }

    std::cout << "[启动动画] 已从资源加载 boot.png。" << std::endl;
    return image;
}

LRESULT CALLBACK splash_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
    switch (message) {
    case WM_NCHITTEST:
        return HTTRANSPARENT;
    case WM_ERASEBKGND:
        return 1;
    default:
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
}

void register_class(HINSTANCE instance)
{
    if (g_class_registered_for == reinterpret_cast<ULONG_PTR>(instance)) return;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = splash_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);
    g_class_registered_for = reinterpret_cast<ULONG_PTR>(instance);
}

void build_rounded_rect_path(Gdiplus::GraphicsPath& path, float x, float y, float w, float h, float radius)
{
    const float d = radius * 2.0f;
    path.AddArc(x, y, d, d, 180.0f, 90.0f);
    path.AddArc(x + w - d, y, d, d, 270.0f, 90.0f);
    path.AddArc(x + w - d, y + h - d, d, d, 0.0f, 90.0f);
    path.AddArc(x, y + h - d, d, d, 90.0f, 90.0f);
    path.CloseFigure();
}

void draw_text_center(Gdiplus::Graphics& g, const wchar_t* text, float cx, float y, float size, Gdiplus::Color color)
{
    using namespace Gdiplus;
    FontFamily variable_family(L"Segoe UI Variable Display");
    FontFamily fallback_family(L"Segoe UI");
    const FontFamily* family = variable_family.GetLastStatus() == Ok ? &variable_family : &fallback_family;
    Font font(family, size, FontStyleBold, UnitPixel);
    StringFormat format;
    format.SetAlignment(StringAlignmentCenter);
    format.SetLineAlignment(StringAlignmentCenter);
    SolidBrush brush(color);
    RectF box(cx - 420.0f, y - size, 840.0f, size * 2.0f);
    g.DrawString(text, -1, &font, box, &format, &brush);
}

void render(State& state, float alpha, float scale)
{
    using namespace Gdiplus;
    if (!state.hwnd || !g_boot_image) return;

    SetWindowPos(state.hwnd, HWND_TOPMOST, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);

    scale = std::max(scale, 0.01f);
    const int width = std::max(1, static_cast<int>(std::round(state.base_width * scale)));
    const int height = std::max(1, static_cast<int>(std::round(state.base_height * scale)));

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HDC screen = GetDC(nullptr);
    HDC dc = CreateCompatibleDC(screen);
    HBITMAP bitmap = CreateDIBSection(screen, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HBITMAP old_bitmap = static_cast<HBITMAP>(SelectObject(dc, bitmap));

    Graphics g(dc);
    g.SetCompositingMode(CompositingModeSourceCopy);
    g.Clear(Color(0, 0, 0, 0));
    g.SetCompositingMode(CompositingModeSourceOver);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetInterpolationMode(InterpolationModeHighQualityBicubic);
    g.SetPixelOffsetMode(PixelOffsetModeHighQuality);
    g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);

    const float local_scale = static_cast<float>(width) / static_cast<float>(state.base_width);
    const float corner = std::max(1.0f, 18.0f * local_scale);
    GraphicsPath splash_shape;
    build_rounded_rect_path(splash_shape, 0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), corner);
    Region old_clip;
    g.GetClip(&old_clip);
    g.SetClip(&splash_shape);
    g.DrawImage(g_boot_image.get(), Rect(0, 0, width, height));

    const float t = static_cast<float>((now_ms() - state.created_at) / kIntroMs);
    const float logo_alpha = clamp01(static_cast<float>((now_ms() - state.created_at) / kLogoMs));
    const float cx = width * 0.5f;
    const float logo_y = height * 0.5f - 40.0f * local_scale;
    const float font_size = std::max(8.0f, 54.0f * local_scale);
    draw_text_center(g, L"StrikeSense", cx, logo_y + 2.0f * local_scale, font_size, Color(static_cast<BYTE>(35.0f * logo_alpha), 0, 0, 0));
    draw_text_center(g, L"StrikeSense", cx, logo_y, font_size, Color(static_cast<BYTE>(255.0f * logo_alpha), 255, 255, 255));

    const float bar_w = 360.0f * local_scale;
    const float bar_h = std::max(2.0f, 6.0f * local_scale);
    const float bar_x = cx - bar_w * 0.5f;
    const float bar_y = logo_y + 55.0f * local_scale;
    GraphicsPath track;
    build_rounded_rect_path(track, bar_x, bar_y, bar_w, bar_h, bar_h * 0.5f);
    SolidBrush track_brush(Color(40, 255, 255, 255));
    g.FillPath(&track_brush, &track);

    const float reveal = ease_in_out_cubic(t);
    const float reveal_w = std::max(bar_h, bar_w * reveal);
    Region bar_clip(RectF(cx - reveal_w * 0.5f, bar_y - 1.0f, reveal_w, bar_h + 2.0f));
    g.SetClip(&bar_clip, CombineModeIntersect);
    SolidBrush fill_brush(Color(255, 255, 255, 255));
    g.FillPath(&fill_brush, &track);
    g.SetClip(&old_clip);

    POINT center{};
    RECT rc{};
    GetWindowRect(state.hwnd, &rc);
    center.x = (rc.left + rc.right) / 2;
    center.y = (rc.top + rc.bottom) / 2;
    if (center.x == 0 && center.y == 0) {
        HMONITOR monitor = MonitorFromWindow(state.hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFO mi{ sizeof(mi) };
        GetMonitorInfoW(monitor, &mi);
        center.x = (mi.rcWork.left + mi.rcWork.right) / 2;
        center.y = (mi.rcWork.top + mi.rcWork.bottom) / 2;
    }

    POINT source{ 0, 0 };
    SIZE size{ width, height };
    POINT top_left{ center.x - width / 2, center.y - height / 2 };
    BLENDFUNCTION blend{ AC_SRC_OVER, 0, static_cast<BYTE>(std::clamp(alpha, 0.0f, 255.0f)), AC_SRC_ALPHA };
    UpdateLayeredWindow(state.hwnd, screen, &top_left, &size, dc, &source, 0, &blend, ULW_ALPHA);

    SelectObject(dc, old_bitmap);
    DeleteObject(bitmap);
    DeleteDC(dc);
    ReleaseDC(nullptr, screen);
}

void pump_messages()
{
    MSG msg{};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

} // namespace

State Create(HINSTANCE instance)
{
    State state{};
    state.instance = instance;

    if (!g_boot_image) g_boot_image = load_png_from_resource(instance);
    if (!g_boot_image) return state;

    register_class(instance);

    HDC screen = GetDC(nullptr);
    const float dpi_scale = static_cast<float>(GetDeviceCaps(screen, LOGPIXELSX)) / 96.0f;
    ReleaseDC(nullptr, screen);

    HMONITOR monitor = MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi{ sizeof(mi) };
    GetMonitorInfoW(monitor, &mi);
    const int work_w = mi.rcWork.right - mi.rcWork.left;
    const int work_h = mi.rcWork.bottom - mi.rcWork.top;

    const float image_ratio = static_cast<float>(g_boot_image->GetHeight()) / static_cast<float>(g_boot_image->GetWidth());
    int width = scaled(kTargetWidth, dpi_scale);
    int height = static_cast<int>(std::round(width * image_ratio));
    const int max_w = static_cast<int>(work_w * 0.78f);
    const int max_h = static_cast<int>(work_h * 0.78f);
    if (width > max_w) {
        width = max_w;
        height = static_cast<int>(std::round(width * image_ratio));
    }
    if (height > max_h) {
        height = max_h;
        width = static_cast<int>(std::round(height / image_ratio));
    }

    const int x = mi.rcWork.left + (work_w - width) / 2;
    const int y = mi.rcWork.top + (work_h - height) / 2;
    state.base_width = width;
    state.base_height = height;
    state.hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE,
        kClassName,
        L"StrikeSense",
        WS_POPUP,
        x,
        y,
        width,
        height,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (!state.hwnd) {
        std::cout << "[启动动画] Splash 窗口创建失败。" << std::endl;
        return state;
    }

    state.created_at = now_ms();
    state.active = true;
    ShowWindow(state.hwnd, SW_SHOWNOACTIVATE);
    render(state, 255.0f, 1.0f);
    std::cout << "[启动动画] Splash 已显示。" << std::endl;
    return state;
}

void PumpOnce(State& state)
{
    pump_messages();
    if (!state.active) return;

    if (state.exit_mode) {
        const float t = static_cast<float>((now_ms() - state.exit_started_at) / kExitMs);
        const float fade_t = clamp01(static_cast<float>((now_ms() - state.exit_started_at - (kExitMs - kFadeMs)) / kFadeMs));
        render(state, 255.0f * (1.0f - fade_t), exit_scale(t));
        if (t >= 1.0f) Destroy(state);
        return;
    }

    render(state, 255.0f, 1.0f);
}

void PumpUntilReady(State& state, HANDLE ready_event, DWORD minimum_ms)
{
    const double deadline = state.created_at + static_cast<double>(minimum_ms);
    while (true) {
        const bool ready = WaitForSingleObject(ready_event, 0) == WAIT_OBJECT_0;
        if (ready && now_ms() >= deadline) break;
        PumpOnce(state);
        Sleep(kFrameDelayMs);
    }
}

void PlayExit(State& state)
{
    if (!state.active) return;
    std::cout << "[启动动画] 主窗口已就绪，播放收束退出动画。" << std::endl;
    state.exit_mode = true;
    state.exit_started_at = now_ms();
    while (state.active) {
        PumpOnce(state);
        Sleep(kFrameDelayMs);
    }
}

void Destroy(State& state)
{
    if (state.hwnd) DestroyWindow(state.hwnd);
    state.hwnd = nullptr;
    state.active = false;
}

} // namespace splashscreen
