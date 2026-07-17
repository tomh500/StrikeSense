#include "pages.h"
#include "volume_mixer.h"
#include "i18n.h" // 确保包含了国际化头文件
#include <fstream>
#include <nlohmann/json.hpp>
#include <thread>
#include <atomic>
#include <array>
#include <cmath>
#include <cerrno>
#include <iostream>
#include <limits>
#include "normalgen.h"
#include "StrikeSense.h"
#include "Hotkey.h"
#include "itemhelper_overlay.h"
#include "textgui_overlay.h"
#include "notifications_overlay.h"
#include "input_environment.h"
#include "resource.h"
#include "ui_theme.h"
#include "vscript.h"

namespace fs = std::filesystem;

namespace {

bool can_use_advanced_theme()
{
    return vscript::GetBuildCode() == vscript::buildcode::eng;
}

int visible_theme_preset()
{
    if (!can_use_advanced_theme() && uitheme::is_advanced_preset(g_uiThemePreset)) {
        std::cout << "[界面主题] 当前权限不允许使用高级视觉组，已回退到 Default。" << std::endl;
        return uitheme::preset_default;
    }
    return g_uiThemePreset;
}

} // namespace

// 假设这些是来自全局或其他头文件的外部声明，保持你原有的逻辑不变
extern HWND g_hwnd;
static void StartCrosshair(HINSTANCE hInst) ;
static void StopCrosshair() ;
extern bool g_crossThreadRunning;
bool g_isBindingHotkey = false; // 添加这行：标记是否正在录入快捷键
bool g_isBindingItemHelperHotkey = false; // 添加这行：标记是否正在录入道具助手快捷键

static Gdiplus::RectF g_lenientWindowToggleRect;
static Gdiplus::RectF g_inputEnvironmentToggleRect;
static Gdiplus::RectF g_inputEnvironmentResetRect;

namespace evolutionui {

constexpr int kCrosshairStyleCount = 6;
constexpr int kNotificationsStyleCount = 6;
constexpr float kTextguiRainbowSpeedMin = 0.1f;
constexpr float kTextguiRainbowSpeedMax = 5.0f;
constexpr float kTextguiRainbowSpreadMin = 4.0f;
constexpr float kTextguiRainbowSpreadMax = 45.0f;
Gdiplus::RectF crosshairEnableRect;
Gdiplus::RectF hotkeyRect;
Gdiplus::RectF centerDotRect;
Gdiplus::RectF volumeSliderRect;
Gdiplus::RectF volumeValueRect;
std::array<Gdiplus::RectF, kCrosshairStyleCount> styleRects;
std::array<Gdiplus::RectF, 3> rgbSliderRects;
std::array<Gdiplus::RectF, 3> rgbValueRects;
std::array<Gdiplus::RectF, 4> parameterSliderRects;
std::array<Gdiplus::RectF, 4> parameterValueRects;
Gdiplus::RectF textguiEnableRect;
Gdiplus::RectF textguiFoldRect;
Gdiplus::RectF textguiWatermarkRect;
Gdiplus::RectF textguiRainbowRect;
Gdiplus::RectF textguiBackdropRect;
Gdiplus::RectF textguiBackdropOpacityRect;
Gdiplus::RectF textguiBackdropOpacityValueRect;
Gdiplus::RectF textguiLogoDetailRect;
Gdiplus::RectF textguiLogoDetailValueRect;
Gdiplus::RectF textguiSloganRect;
std::array<Gdiplus::RectF, 10> textguiSliderRects;
std::array<Gdiplus::RectF, 10> textguiSliderValueRects;
std::array<Gdiplus::RectF, 3> textguiColorRects;
std::array<Gdiplus::RectF, 3> textguiColorValueRects;
std::array<Gdiplus::RectF, 3> textguiAccessoryColorRects;
std::array<Gdiplus::RectF, 3> textguiAccessoryColorValueRects;
Gdiplus::RectF notificationsEnableRect;
Gdiplus::RectF notificationsFoldRect;
Gdiplus::RectF notificationsDurationRect;
Gdiplus::RectF notificationsDurationValueRect;
std::array<Gdiplus::RectF, kNotificationsStyleCount> notificationsStyleRects;
Gdiplus::RectF crosshairFoldRect;
bool textguiCollapsed = true;
bool notificationsCollapsed = true;
bool crosshairCollapsed = true;

struct input_context {
    std::wstring label;
    std::wstring value;
    bool accepted = false;
};

INT_PTR CALLBACK input_dialog_proc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* context = reinterpret_cast<input_context*>(GetWindowLongPtrW(dialog, DWLP_USER));
    if (message == WM_INITDIALOG) {
        context = reinterpret_cast<input_context*>(lParam);
        SetWindowLongPtrW(dialog, DWLP_USER, reinterpret_cast<LONG_PTR>(context));
        SetDlgItemTextW(dialog, IDC_EVOLUTION_INPUT_LABEL, context->label.c_str());
        SetDlgItemTextW(dialog, IDC_EVOLUTION_INPUT_EDIT, context->value.c_str());
        SendDlgItemMessageW(dialog, IDC_EVOLUTION_INPUT_EDIT, EM_SETSEL, 0, -1);
        SetFocus(GetDlgItem(dialog, IDC_EVOLUTION_INPUT_EDIT));
        return FALSE;
    }
    if (message != WM_COMMAND || !context) return FALSE;
    if (LOWORD(wParam) == IDOK) {
        const int length = GetWindowTextLengthW(GetDlgItem(dialog, IDC_EVOLUTION_INPUT_EDIT));
        std::wstring value(static_cast<size_t>(length) + 1, L'\0');
        GetDlgItemTextW(dialog, IDC_EVOLUTION_INPUT_EDIT, value.data(), length + 1);
        value.resize(static_cast<size_t>(length));
        context->value = value;
        context->accepted = true;
        EndDialog(dialog, IDOK);
        return TRUE;
    }
    if (LOWORD(wParam) == IDCANCEL) {
        EndDialog(dialog, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}

bool prompt_input(HWND owner, const std::wstring& label, std::wstring& value)
{
    input_context context{ label, value, false };
    DialogBoxParamW(hInst, MAKEINTRESOURCEW(IDD_EVOLUTION_INPUT), owner,
        input_dialog_proc, reinterpret_cast<LPARAM>(&context));
    if (!context.accepted) return false;
    value = context.value;
    return true;
}

bool parse_number_in_int_range(const std::wstring& text, double& value)
{
    if (text.empty()) return false;
    wchar_t* end = nullptr;
    errno = 0;
    value = std::wcstod(text.c_str(), &end);
    while (end && iswspace(*end) != 0) ++end;
    return errno != ERANGE && end && *end == L'\0' && std::isfinite(value)
        && value >= static_cast<double>((std::numeric_limits<int>::min)())
        && value <= static_cast<double>((std::numeric_limits<int>::max)());
}

std::string wide_to_utf8(const std::wstring& value)
{
    if (value.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
        nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<size_t>((std::max)(0, size)), '\0');
    if (size > 0) WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
        result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring utf8_to_wide(const std::string& value)
{
    if (value.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    std::wstring result(static_cast<size_t>((std::max)(0, size)), L'\0');
    if (size > 0) MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size);
    return result;
}

bool Hit(const Gdiplus::RectF& rect, int x, int y)
{
    return x >= rect.X && x <= rect.X + rect.Width
        && y >= rect.Y && y <= rect.Y + rect.Height;
}

void DrawCrosshairShape(Gdiplus::Graphics& g, float centerX, float centerY,
    Gdiplus::Color color, int style, float thickness, float scale,
    int gap, int length, bool centerDot)
{
    using namespace Gdiplus;
    thickness = std::clamp(thickness, 0.1f, 1000.f);
    scale = std::clamp(scale, 0.001f, 100.f);
    gap = std::clamp(gap, -100000, 100000);
    length = std::clamp(length, -100000, 100000);
    Pen pen(color, thickness);
    pen.SetStartCap(LineCapRound);
    pen.SetEndCap(LineCapRound);
    SolidBrush brush(color);
    const float multiplier = (std::max)(0.05f, scale * 5.f);
    const float radius = 20.f * scale;
    const float scaledGap = gap * multiplier;
    const float scaledLength = length * multiplier;

    switch (style) {
    case 0:
        g.DrawEllipse(&pen, centerX - radius, centerY - radius, radius * 2.f, radius * 2.f);
        break;
    case 1:
        g.FillEllipse(&brush, centerX - radius, centerY - radius, radius * 2.f, radius * 2.f);
        break;
    case 2:
        g.DrawLine(&pen, centerX - scaledLength, centerY, centerX + scaledLength, centerY);
        g.DrawLine(&pen, centerX, centerY - scaledLength, centerX, centerY + scaledLength);
        break;
    case 3:
        g.DrawLine(&pen, centerX - scaledGap - scaledLength, centerY, centerX - scaledGap, centerY);
        g.DrawLine(&pen, centerX + scaledGap, centerY, centerX + scaledGap + scaledLength, centerY);
        g.DrawLine(&pen, centerX, centerY - scaledGap - scaledLength, centerX, centerY - scaledGap);
        g.DrawLine(&pen, centerX, centerY + scaledGap, centerX, centerY + scaledGap + scaledLength);
        break;
    case 4:
        g.DrawLine(&pen, centerX - scaledGap - scaledLength, centerY - scaledGap - scaledLength,
            centerX - scaledGap, centerY - scaledGap);
        g.DrawLine(&pen, centerX + scaledGap, centerY - scaledGap,
            centerX + scaledGap + scaledLength, centerY - scaledGap - scaledLength);
        g.DrawLine(&pen, centerX - scaledGap - scaledLength, centerY + scaledGap + scaledLength,
            centerX - scaledGap, centerY + scaledGap);
        g.DrawLine(&pen, centerX + scaledGap, centerY + scaledGap,
            centerX + scaledGap + scaledLength, centerY + scaledGap + scaledLength);
        break;
    case 5:
        g.DrawLine(&pen, centerX - scaledGap - scaledLength, centerY, centerX - scaledGap, centerY);
        g.DrawLine(&pen, centerX + scaledGap, centerY, centerX + scaledGap + scaledLength, centerY);
        g.DrawLine(&pen, centerX, centerY + scaledGap, centerX, centerY + scaledGap + scaledLength);
        break;
    default:
        break;
    }

    if (centerDot && style != 0 && style != 1) {
        const float dotRadius = (std::max)(1.5f, thickness * 0.8f);
        g.FillEllipse(&brush, centerX - dotRadius, centerY - dotRadius, dotRadius * 2.f, dotRadius * 2.f);
    }
}

} // namespace evolutionui

static std::wstring GetEvolutionConfigPath() {
    wchar_t p[MAX_PATH] = {};
    GetEnvironmentVariableW(L"USERPROFILE", p, MAX_PATH);
    return fs::path(p) / L"StrikeSense" / L"setting" / L"evolution.json";
}



void SaveEvolutionParams() {
    nlohmann::json j;
    j["death_vol"] = g_death_vol; j["death_mute"] = g_deathMute;
    j["langCN"] = g_langCN;
    j["hotkey_mod"] = g_hotkeyMod; j["hotkey_vk"] = g_hotkeyVk;
    j["crosshair_enabled"] = g_crosshairEnabled;
    j["lenient_cs2_window_detection"] = IsLenientCS2WindowDetection();
    j["input_environment_intercept"] = inputenvironment::IsEnabled();
    j["crosshair_r"] = g_crosshairR; j["crosshair_g"] = g_crosshairG; j["crosshair_b"] = g_crosshairB;
    j["crosshair_style"] = g_crosshairStyle;
    j["crosshair_thickness"] = g_crosshairThickness; j["crosshair_scale"] = g_crosshairScale;
    j["crosshair_gap"] = g_crosshairGap; j["crosshair_length"] = g_crosshairLength;
    j["crosshair_center_dot"] = g_crosshairCenterDot;
    j["textgui_enabled"] = g_textguiEnabled;
    j["textgui_x"] = g_textguiX; j["textgui_y"] = g_textguiY;
    j["textgui_scale"] = g_textguiScale; j["textgui_opacity"] = g_textguiOpacity;
    j["textgui_line_spacing"] = g_textguiLineSpacing;
    j["textgui_shadow_strength"] = g_textguiShadowStrength;
    j["textgui_rainbow_speed"] = g_textguiRainbowSpeed;
    j["textgui_rainbow_spread"] = g_textguiRainbowSpread;
    j["textgui_rainbow_saturation"] = g_textguiRainbowSaturation;
    j["textgui_rainbow_brightness"] = g_textguiRainbowBrightness;
    j["textgui_r"] = g_textguiR; j["textgui_g"] = g_textguiG; j["textgui_b"] = g_textguiB;
    j["textgui_accessory_r"] = g_textguiAccessoryR;
    j["textgui_accessory_g"] = g_textguiAccessoryG;
    j["textgui_accessory_b"] = g_textguiAccessoryB;
    j["textgui_show_watermark"] = g_textguiShowWatermark;
    j["textgui_rainbow"] = g_textguiRainbow;
    j["textgui_backdrop"] = g_textguiBackdrop;
    j["textgui_backdrop_opacity"] = g_textguiBackdropOpacity;
    j["textgui_logo_detail_thickness"] = g_textguiLogoDetailThickness;
    j["textgui_custom_slogan"] = evolutionui::wide_to_utf8(g_textguiCustomSlogan);
    j["notifications_enabled"] = g_notificationsEnabled;
    j["notifications_duration"] = g_notificationsDuration;
    j["notifications_style"] = g_notificationsStyle;
    j["ui_theme_preset"] = visible_theme_preset();
    
    j["item_helper_enabled"] = g_itemHelperEnabled;
    j["item_helper_hotkey_mod"] = g_itemHelperHotkeyMod;
    j["item_helper_hotkey_vk"] = g_itemHelperHotkeyVk;

    // 道具助手新增设置
    j["item_helper_x"] = g_itemHelperX;
    j["item_helper_y"] = g_itemHelperY;
    j["item_helper_opacity"] = g_itemHelperOpacity;
    j["item_helper_img_opacity"] = g_itemHelperImgOpacity; // 保存图片透明度
    j["item_helper_autohide"] = g_itemHelperAutoHide;      // 保存自动销毁
    j["item_helper_key_prev"] = g_itemHelperKeyPrev;
    j["item_helper_key_next"] = g_itemHelperKeyNext;
    j["item_helper_key_sel"] = g_itemHelperKeySelect;
    if (g_itemHelperEnabled) {
    // 这里的 g_hwnd 是你的全局主窗口句柄（HWND），确保它在这个函数可用
    Hotkey::UpdateItemHelperHotkey(g_hwnd); 
}

    std::ofstream out(GetEvolutionConfigPath());
    if (out.is_open()) { out << j.dump(2); out.close(); }
}

void LoadUiThemePresetBeforeWindow() {
    fs::path p(GetEvolutionConfigPath());
    if (!fs::exists(p)) {
        g_uiThemePreset = uitheme::preset_default;
        return;
    }

    try {
        std::ifstream in(p);
        if (!in.is_open()) {
            g_uiThemePreset = uitheme::preset_default;
            return;
        }

        nlohmann::json j;
        in >> j;
        if (j.contains("ui_theme_preset") && j["ui_theme_preset"].is_number_integer()) {
            g_uiThemePreset = j["ui_theme_preset"].get<int>();
        } else {
            g_uiThemePreset = uitheme::preset_default;
        }

        g_uiThemePreset = std::clamp(g_uiThemePreset, 0, uitheme::preset_count - 1);
        if (!can_use_advanced_theme() && uitheme::is_advanced_preset(g_uiThemePreset)) {
            g_uiThemePreset = uitheme::preset_default;
            std::cout << "[界面主题] 窗口创建前检测到非 eng 权限高级主题配置，已回退到 Default。" << std::endl;
        }
    }
    catch (...) {
        g_uiThemePreset = uitheme::preset_default;
        std::cout << "[界面主题] 窗口创建前读取主题配置失败，已使用 Default。" << std::endl;
    }
}

void LoadEvolutionParams() {
    fs::path p(GetEvolutionConfigPath());
    if (!fs::exists(p)) return;
    try {
        std::ifstream in(p); if (!in.is_open()) return;
        nlohmann::json j; in >> j; in.close();
        auto gv = [&](const char* k, auto& v) { if (j.contains(k) && j[k].is_number()) v = j[k].get<std::remove_reference_t<decltype(v)>>(); };
        auto gb = [&](const char* k, bool& v) { if (j.contains(k) && j[k].is_boolean()) v = j[k]; };
        
        gv("death_vol", g_death_vol); 
        
        // --- 核心修改：无论配置里是什么，强制设为关闭 ---
        g_deathMute = false; 
        StopCS2VolumeControl(); // 确保钩子处于停止状态
        // ----------------------------------------------

        gv("hotkey_mod", g_hotkeyMod); gv("hotkey_vk", g_hotkeyVk);
        gb("crosshair_enabled", g_crosshairEnabled);
        bool lenientWindowDetection = IsLenientCS2WindowDetection();
        gb("lenient_cs2_window_detection", lenientWindowDetection);
        SetLenientCS2WindowDetection(lenientWindowDetection);
        bool inputEnvironmentIntercept = inputenvironment::IsEnabled();
        gb("input_environment_intercept", inputEnvironmentIntercept);
        inputenvironment::SetEnabled(inputEnvironmentIntercept);
        gb("item_helper_enabled", g_itemHelperEnabled);
        gv("crosshair_r", g_crosshairR); gv("crosshair_g", g_crosshairG); gv("crosshair_b", g_crosshairB);
        gv("crosshair_style", g_crosshairStyle); gv("crosshair_thickness", g_crosshairThickness);
        gv("crosshair_scale", g_crosshairScale);
        gv("crosshair_gap", g_crosshairGap); gv("crosshair_length", g_crosshairLength);
        gb("crosshair_center_dot", g_crosshairCenterDot);
        gb("textgui_enabled", g_textguiEnabled);
        gv("textgui_x", g_textguiX); gv("textgui_y", g_textguiY);
        gv("textgui_scale", g_textguiScale); gv("textgui_opacity", g_textguiOpacity);
        gv("textgui_line_spacing", g_textguiLineSpacing);
        gv("textgui_shadow_strength", g_textguiShadowStrength);
        gv("textgui_rainbow_speed", g_textguiRainbowSpeed);
        gv("textgui_rainbow_spread", g_textguiRainbowSpread);
        gv("textgui_rainbow_saturation", g_textguiRainbowSaturation);
        gv("textgui_rainbow_brightness", g_textguiRainbowBrightness);
        gv("textgui_r", g_textguiR); gv("textgui_g", g_textguiG); gv("textgui_b", g_textguiB);
        gv("textgui_accessory_r", g_textguiAccessoryR);
        gv("textgui_accessory_g", g_textguiAccessoryG);
        gv("textgui_accessory_b", g_textguiAccessoryB);
        gb("textgui_show_watermark", g_textguiShowWatermark);
        gb("textgui_rainbow", g_textguiRainbow);
        gb("textgui_backdrop", g_textguiBackdrop);
        gv("textgui_backdrop_opacity", g_textguiBackdropOpacity);
        gv("textgui_logo_detail_thickness", g_textguiLogoDetailThickness);
        if (j.contains("textgui_custom_slogan") && j["textgui_custom_slogan"].is_string()) {
            g_textguiCustomSlogan = evolutionui::utf8_to_wide(j["textgui_custom_slogan"].get<std::string>());
        }
        gb("notifications_enabled", g_notificationsEnabled);
        gv("notifications_duration", g_notificationsDuration);
        gv("notifications_style", g_notificationsStyle);
        gv("ui_theme_preset", g_uiThemePreset);
        g_notificationsStyle = std::clamp(g_notificationsStyle, 0, evolutionui::kNotificationsStyleCount - 1);
        g_uiThemePreset = std::clamp(g_uiThemePreset, 0, uitheme::preset_count - 1);
        if (!can_use_advanced_theme() && uitheme::is_advanced_preset(g_uiThemePreset)) {
            g_uiThemePreset = uitheme::preset_default;
            std::cout << "[界面主题] 非 eng 权限启动，已禁用高级视觉主题并回退到 Default。" << std::endl;
        }

        gb("item_helper_enabled", g_itemHelperEnabled);

        gv("item_helper_hotkey_mod", g_itemHelperHotkeyMod);
        gv("item_helper_hotkey_vk", g_itemHelperHotkeyVk);

        gb("langCN", g_langCN);

        // 道具助手基础读取
        gb("item_helper_enabled", g_itemHelperEnabled);
        gv("item_helper_hotkey_mod", g_itemHelperHotkeyMod);
        gv("item_helper_hotkey_vk", g_itemHelperHotkeyVk);

        // 道具助手新增读取
        gv("item_helper_x", g_itemHelperX);
        gv("item_helper_y", g_itemHelperY);
        gv("item_helper_opacity", g_itemHelperOpacity);
        gv("item_helper_img_opacity", g_itemHelperImgOpacity); // 读取图片透明度
        gb("item_helper_autohide", g_itemHelperAutoHide);      // 读取自动销毁
        gv("item_helper_key_prev", g_itemHelperKeyPrev);
        gv("item_helper_key_next", g_itemHelperKeyNext);
        gv("item_helper_key_sel", g_itemHelperKeySelect);

        ApplyCrosshairVisual(g_crosshairR, g_crosshairG, g_crosshairB, g_crosshairStyle,
            g_crosshairThickness, g_crosshairScale, g_crosshairGap, g_crosshairLength, g_crosshairCenterDot);
        ApplyCrosshairEnabled(g_crosshairEnabled);
        ApplyTextguiEnabled(g_textguiEnabled);
        
    } catch (...) {
        // 异常捕获时也保险起见重置
        g_deathMute = false;
        StopCS2VolumeControl();
    }
}

// ===== 准星线程 =====
static HWND g_crossHWnd = nullptr;
static std::thread g_crossThread;
bool g_crossThreadRunning = false;
static bool g_crosshairWindowVisible = false;
static constexpr UINT_PTR kCrosshairVisibilityTimer = 1;

static void UpdateCrosshairWindowVisibility(HWND hw)
{
    const bool shouldShow = IsCS2WindowActive();
    if (shouldShow == g_crosshairWindowVisible) return;

    g_crosshairWindowVisible = shouldShow;
    ShowWindow(hw, shouldShow ? SW_SHOWNOACTIVATE : SW_HIDE);
    if (shouldShow) {
        InvalidateRect(hw, nullptr, FALSE);
        UpdateWindow(hw);
    }
    std::cout << "[狙击准星] CS2 窗口状态变化，覆盖层已"
              << (shouldShow ? "显示" : "隐藏") << std::endl;
}

static LRESULT CALLBACK CrosshairWndProc(HWND hw, UINT m, WPARAM wp, LPARAM lp) {
    switch (m) {
    case WM_CLOSE: DestroyWindow(hw); return 0;
    case WM_TIMER:
        if (wp == kCrosshairVisibilityTimer) {
            UpdateCrosshairWindowVisibility(hw);
            return 0;
        }
        break;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC hdc = BeginPaint(hw, &ps);
        RECT rc; GetClientRect(hw, &rc);
        int W = rc.right - rc.left, H = rc.bottom - rc.top;
        HDC md = CreateCompatibleDC(hdc); HBITMAP mb = CreateCompatibleBitmap(hdc, W, H);
        HBITMAP ob = (HBITMAP)SelectObject(md, mb);
        using namespace Gdiplus;
        Graphics gx(md); gx.SetSmoothingMode(SmoothingModeAntiAlias);
        int cx = W / 2, cy = H / 2;
        Color crCol(255, static_cast<BYTE>(std::clamp(g_crosshairR, 0, 255)),
            static_cast<BYTE>(std::clamp(g_crosshairG, 0, 255)),
            static_cast<BYTE>(std::clamp(g_crosshairB, 0, 255)));
        evolutionui::DrawCrosshairShape(gx, static_cast<float>(cx), static_cast<float>(cy), crCol,
            g_crosshairStyle, static_cast<float>(g_crosshairThickness), g_crosshairScale,
            g_crosshairGap, g_crosshairLength, g_crosshairCenterDot);
        BitBlt(hdc, 0, 0, W, H, md, 0, 0, SRCCOPY);
        SelectObject(md, ob); DeleteObject(mb); DeleteDC(md);
        EndPaint(hw, &ps); return 0;
    }
    case WM_ERASEBKGND: return 1;
    case WM_DESTROY:
        KillTimer(hw, kCrosshairVisibilityTimer);
        g_crosshairWindowVisible = false;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hw, m, wp, lp);
}

static void StartCrosshair(HINSTANCE hInst) {
    if (g_crossThreadRunning) return;
    g_crossThreadRunning = true;
    g_crossThread = std::thread([hInst]() {
        WNDCLASSEXW wc = {}; wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = CrosshairWndProc; wc.hInstance = hInst;
        wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
        wc.lpszClassName = L"StrikeSense_Crosshair";
        RegisterClassExW(&wc);
        int sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
        HWND cw = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED
            | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
            L"StrikeSense_Crosshair", L"", WS_POPUP, 0, 0, sw, sh, nullptr, nullptr, hInst, nullptr);
        if (!cw) { g_crossThreadRunning = false; return; }
        SetLayeredWindowAttributes(cw, RGB(0, 0, 0), 0, LWA_COLORKEY);
        g_crossHWnd = cw;
        SetTimer(cw, kCrosshairVisibilityTimer, 1000, nullptr);
        UpdateCrosshairWindowVisibility(cw);
        MSG m; while (GetMessage(&m, nullptr, 0, 0)) { TranslateMessage(&m); DispatchMessage(&m); }
        g_crossHWnd = nullptr; g_crossThreadRunning = false;
    });
    g_crossThread.detach();
}

static void StopCrosshair() {
    if (g_crossHWnd) { PostMessage(g_crossHWnd, WM_CLOSE, 0, 0); int wc = 0; while (g_crossHWnd && wc < 50) { Sleep(50); wc++; } }
    g_crossHWnd = nullptr; g_crossThreadRunning = false;
}

void DestroyCrosshairInternal() { StopCrosshair(); }

void RefreshCrosshairOverlay()
{
    if (!g_crossHWnd) return;
    InvalidateRect(g_crossHWnd, nullptr, FALSE);
    if (IsWindowVisible(g_crossHWnd)) UpdateWindow(g_crossHWnd);
}

void ApplyCrosshairVisual(int r, int g, int b, int style, int thickness, float scale,
    int gap, int length, bool centerDot)
{
    g_crosshairR = std::clamp(r, 0, 255);
    g_crosshairG = std::clamp(g, 0, 255);
    g_crosshairB = std::clamp(b, 0, 255);
    g_crosshairStyle = std::clamp(style, 0, evolutionui::kCrosshairStyleCount - 1);
    g_crosshairThickness = thickness;
    g_crosshairScale = std::isfinite(scale) ? scale : 0.2f;
    g_crosshairGap = gap;
    g_crosshairLength = length;
    g_crosshairCenterDot = centerDot;
    RefreshCrosshairOverlay();
}

void ApplyCrosshairEnabled(bool enabled)
{
    g_crosshairEnabled = enabled;
    if (g_crosshairEnabled) {
        if (!g_crossThreadRunning) StartCrosshair(hInst);
        RefreshCrosshairOverlay();
        return;
    }
    StopCrosshair();
}

void ApplyTextguiEnabled(bool enabled)
{
    g_textguiEnabled = enabled;
    textgui_overlay::ApplyEnabled(g_textguiEnabled);
}

void RefreshTextguiOverlay()
{
    textgui_overlay::Refresh();
}

// ===== UI 绘制层 =====
static void PaintEvolutionPageLegacy(Gdiplus::Graphics& g, int cx, int cw, int H, HWND) {
    using namespace Gdiplus;
    using namespace i18n; // 注入国际化空间以识别 Keys::

    // 1. 标题
    ui::DrawHeader(g, cx, cw, _(Keys::EVO_TITLE));
    
    Font rF(L"Microsoft YaHei", 11), sF(L"Microsoft YaHei", 9);
    SolidBrush tdCol(Color(255, 30, 60, 100)), tbCol(Color(255, 20, 80, 140)), tmDim(Color(255, 100, 130, 160));
    SolidBrush sBg(Color(255, 200, 220, 240)), sFill(Color(255, 80, 180, 240)), knB(Color(255, 60, 160, 230));
    SolidBrush ddBg(Color(255, 220, 240, 255)), ddHoverBg(Color(255, 180, 220, 245));

    // 2. 音量调节器区域
    g.DrawString(_(Keys::EVO_VOL_ADJ), -1, &rF, PointF((float)(cx + 10), 50.f), &tdCol);
    int yVolSlider = 80; int slW = cw - 100;
    ui::DrawSlider(g, cx + 10, yVolSlider, slW, g_death_vol);
    float kx = cx + 10 + (int)(slW * g_death_vol) - 8.f;
    g.FillEllipse(&knB, kx, yVolSlider - 6.f, 16.f, 16.f);
    wchar_t vt[32]; swprintf_s(vt, L"%.0f%%", g_death_vol * 100.f);
    g.DrawString(vt, -1, &rF, PointF((float)(cx + 20 + slW), (float)(yVolSlider - 8)), &tdCol);

    // 3. 开关与长提示语
    std::wstring muteLabel = std::wstring(_(Keys::EVO_STATUS_MUTED)) + L":";
    g.DrawString(muteLabel.c_str(), -1, &sF, PointF((float)(cx + 10), 115.f), &tdCol);
    
    g_deathMuteToggleRect = RectF((REAL)(cx + 80), (REAL)111, 50.f, 24.f);
    ui::DrawToggle(g, cx + 80, 111, g_deathMute);

    g.DrawString(_(Keys::EVO_HINT_MUTE), -1, &sF, PointF((float)(cx + 140), 115.f), &tmDim);

    g.DrawString(L"宽容检测游戏窗口:", -1, &sF, PointF((float)(cx + 10), 145.f), &tdCol);
    g_lenientWindowToggleRect = RectF((REAL)(cx + 140), (REAL)141, 50.f, 24.f);
    ui::DrawToggle(g, cx + 140, 141, IsLenientCS2WindowDetection());

    // 4. 快捷键区域（组合国际化）
    /*
    std::wstring keyName;
    if (g_hotkeyMod & MOD_CONTROL) keyName += L"Ctrl+";
    if (g_hotkeyMod & MOD_ALT)     keyName += L"Alt+";
    if (g_hotkeyMod & MOD_SHIFT)   keyName += L"Shift+";
    if (g_hotkeyVk >= 'A' && g_hotkeyVk <= 'Z')     keyName += (wchar_t)g_hotkeyVk;
    else if (g_hotkeyVk >= '0' && g_hotkeyVk <= '9') keyName += (wchar_t)g_hotkeyVk;
    else { wchar_t b[16]; swprintf_s(b, L"Vk=%d", g_hotkeyVk); keyName += b; }

    SolidBrush disabledCol(Color(180, 150, 150, 160));
    wchar_t hs[128];
    swprintf_s(hs, L"%s: %s %s", _(Keys::EVO_HOTKEY), keyName.c_str(), _(Keys::EVO_STATUS_DISABLED));
    g.DrawString(hs, -1, &rF, PointF((float)(cx + 10), 150.f), &disabledCol);
    {
        Gdiplus::Pen kp(Color(80, 150, 150, 160));
        Gdiplus::RectF keyRect((REAL)(cx + 10), 172.f, 200.f, 20.f);
        g.DrawRectangle(&kp, keyRect);
        g.DrawString(_(Keys::EVO_LOCK_VIEW), -1, &sF, PointF((float)(cx + 14), 174.f), &disabledCol);
    }*/

    // 4. 快捷键区域（组合国际化）【已解封】
    std::wstring keyName;
    if (g_hotkeyVk == 0) {
        keyName = L"None";
    } else {
        if (g_hotkeyMod & MOD_CONTROL) keyName += L"Ctrl+";
        if (g_hotkeyMod & MOD_ALT)     keyName += L"Alt+";
        if (g_hotkeyMod & MOD_SHIFT)   keyName += L"Shift+";
        if (g_hotkeyVk >= 'A' && g_hotkeyVk <= 'Z')     keyName += (wchar_t)g_hotkeyVk;
        else if (g_hotkeyVk >= '0' && g_hotkeyVk <= '9') keyName += (wchar_t)g_hotkeyVk;
        else { wchar_t b[16]; swprintf_s(b, L"Vk=%d", g_hotkeyVk); keyName += b; }
    }

    wchar_t hs[128];
    swprintf_s(hs, L"%s: %s", _(Keys::EVO_HOTKEY), keyName.c_str());
    g.DrawString(hs, -1, &rF, PointF((float)(cx + 10), 178.f), &tdCol); // 恢复高亮色
    {
        Gdiplus::Pen kp(Color(255, 30, 60, 100)); // 恢复高亮色边框
        Gdiplus::RectF keyRect((REAL)(cx + 10), 200.f, 200.f, 20.f);
        g.DrawRectangle(&kp, keyRect);
        
        // 如果在录入状态就显示“按下任意键...”，否则显示“点击修改快捷键”
        const wchar_t* hintStr = g_isBindingHotkey ? _(Keys::EVO_WAITING_KEY) : _(Keys::EVO_CLICK_MODIFY);
        g.DrawString(hintStr, -1, &sF, PointF((float)(cx + 14), 202.f), &tdCol);
    }

    // 5. 准星设置区域
    int yRgb = 263, yRow2 = 298;
    g.DrawString(_(Keys::EVO_CROSSHAIR), -1, &rF, PointF((float)(cx + 10), 233.f), &tdCol);
    int rgbLabelX = cx + 10; int rgbBarW = 80, rgbSpacing = 150;
    const wchar_t* rgbL[] = { L"R", L"G", L"B" };
    int* rgbV[] = { &g_crosshairR, &g_crosshairG, &g_crosshairB };
    for (int i = 0; i < 3; ++i) {
        int bx = rgbLabelX + i * rgbSpacing;
        g.DrawString(rgbL[i], -1, &sF, PointF((float)bx, (float)yRgb), &tdCol);
        ui::DrawSlider(g, bx + 20, yRgb, rgbBarW, (float)(*rgbV[i]) / 255.f);
        wchar_t bf[8]; swprintf_s(bf, L"%d", *rgbV[i]); g.DrawString(bf, -1, &sF, PointF((float)(bx + 105), (float)(yRgb - 2)), &tdCol);
    }

    // 6. 粗细与缩放
    int thX = cx + 10; 
    std::wstring thickLabel = std::wstring(_(Keys::EVO_THICKNESS)) + L":";
    g.DrawString(thickLabel.c_str(), -1, &sF, PointF((float)thX, (float)yRow2), &tdCol);
    
    int thBarX = thX + 40, thBarW = 80;
    ui::DrawSlider(g, thBarX, yRow2, thBarW, g_crosshairThickness / 10.f);
    wchar_t thT[8]; swprintf_s(thT, L"%d", g_crosshairThickness); g.DrawString(thT, -1, &sF, PointF((float)(thBarX + thBarW + 4), (float)(yRow2 - 2)), &tdCol);

    int scX = thBarX + thBarW + 40; 
    std::wstring scaleLabel = std::wstring(_(Keys::EVO_SCALE)) + L":";
    g.DrawString(scaleLabel.c_str(), -1, &sF, PointF((float)scX, (float)yRow2), &tdCol);
    
    int scBarX = scX + 40, scBarW = 100;
    ui::DrawSlider(g, scBarX, yRow2, scBarW, (g_crosshairScale - 0.02f) / 0.58f);
    wchar_t scT[8]; swprintf_s(scT, L"%.2f", g_crosshairScale); g.DrawString(scT, -1, &sF, PointF((float)(scBarX + scBarW + 4), (float)(yRow2 - 2)), &tdCol);

    // 7. 下拉样式选择（核心映射化）
    int styX = scBarX + scBarW + 40; 
    std::wstring styleLabel = std::wstring(_(Keys::EVO_STYLE)) + L":";
    g.DrawString(styleLabel.c_str(), -1, &sF, PointF((float)styX, (float)yRow2), &tdCol);
    
    int ddX = styX + 40, ddW = 100;
    const wchar_t* sty[] = { 
        _(Keys::EVO_STYLE_HOLLOW), 
        _(Keys::EVO_STYLE_SOLID), 
        _(Keys::EVO_STYLE_CLASSIC) 
    };
    {
        SolidBrush ddBtn(Color(255, 180, 220, 250)); Pen ddPen(Color(255, 100, 150, 200));
        RectF ddRect((REAL)ddX, (REAL)(yRow2 - 2), (REAL)ddW, 20.f);
        g.FillRectangle(&ddBtn, ddRect); g.DrawRectangle(&ddPen, ddRect);
        g.DrawString(sty[g_crosshairStyle], -1, &sF, PointF((REAL)ddX + 4, (REAL)yRow2), &tdCol);
        SolidBrush arr(Color(255, 30, 60, 100));
        PointF arrPts[] = { PointF((REAL)(ddX + ddW - 8), (REAL)(yRow2 + 4)), PointF((REAL)(ddX + ddW), (REAL)(yRow2 + 4)), PointF((REAL)(ddX + ddW - 4), (REAL)(yRow2 + 12)) };
        g.FillPolygon(&arr, arrPts, 3);
        if (g_styleDropdownOpen) {
            for (int j = 0; j < 3; ++j) {
                RectF optRect((REAL)ddX, (REAL)(yRow2 + 16 + j * 18), (REAL)ddW, 18.f);
                g_dropdownRects[j] = optRect;
                g.FillRectangle((j == g_dropdownSelection) ? &ddHoverBg : &ddBg, optRect);
                g.DrawRectangle(&ddPen, optRect);
                g.DrawString(sty[j], -1, &sF, PointF((REAL)ddX + 4, (REAL)(yRow2 + 18 + j * 18)), &tdCol);
            }
        }
    }

    // 8. 启用
    int enableX = ddX + ddW + 40; 
    g.DrawString(_(Keys::EVO_ENABLE), -1, &sF, PointF((float)enableX, (float)yRow2), &tdCol);
    ui::DrawToggle(g, enableX + 40, yRow2 - 4, g_crosshairEnabled);
}

// ===== UI 点击事件层 =====
static void CheckEvolutionClickLegacy(HWND hw, int mx, int my) {

    int cx = SIDEBAR_W + 12, cw = 0;
    RECT rc; GetClientRect(hw, &rc); cw = rc.right - rc.left - cx - 12;
    int yVolSlider = 80, yRgb = 263, yRow2 = 298; int slW = cw - 100; float val;
        // 检测是否点击了快捷键录入框区域 (X: cx+10 ~ cx+210, Y: 172 ~ 192)
    if (mx >= cx + 10 && mx <= cx + 210 && my >= 200 && my <= 220) {
        g_isBindingHotkey = !g_isBindingHotkey;
        SetFocus(hw); // 让窗口拿到键盘焦点
        InvalidateRect(hw, nullptr, FALSE);
        return;
    } else if (g_isBindingHotkey) {
        // 点击了框以外的其他地方，直接取消录入状态
        g_isBindingHotkey = false;
        InvalidateRect(hw, nullptr, FALSE);
    }
    if (ui::CheckSliderClick(mx, my, cx + 10, yVolSlider, slW, val)) {
        g_death_vol = val;
        SaveEvolutionParams();
        if (g_deathMute) {
            SetCS2VolumeReduction(g_death_vol);
            if (!IsCS2VolumeActive()) StartCS2VolumeControl(g_death_vol);
        }
        InvalidateRect(hw, nullptr, FALSE);
        return;
    }
    if (ui::CheckToggleClick(mx, my, (int)g_deathMuteToggleRect.X, (int)g_deathMuteToggleRect.Y)) {

        if (!normalgen::CheckAdminPermission()) {
            int ret = MessageBoxW(hw,
                L"音量降低器需要管理员权限才能正常工作。\n是否重新以管理员身份启动程序？",
                L"⚠️ 权限不足",
                MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
            if (ret == IDYES) {
                if (g_hMutex) { CloseHandle(g_hMutex); g_hMutex = nullptr; }
                wchar_t exePath[MAX_PATH] = {};
                GetModuleFileNameW(nullptr, exePath, MAX_PATH);
                    SHELLEXECUTEINFOW sei{};
                    sei.cbSize = sizeof(sei);
                    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
                    sei.lpVerb = L"runas";
                    sei.lpFile = exePath;
                    sei.nShow = SW_SHOWNORMAL;

                    if (ShellExecuteExW(&sei))
                    {
                        WaitForInputIdle(sei.hProcess, 5000);

                         DestroyWindow(hw);
                    }
            }
            return;
        }
        g_deathMute = !g_deathMute;
        SaveEvolutionParams();
        if (g_deathMute) StartCS2VolumeControl(g_death_vol);
        else StopCS2VolumeControl();
        InvalidateRect(hw, nullptr, FALSE);
        return;
    }
    if (ui::CheckToggleClick(mx, my, (int)g_lenientWindowToggleRect.X, (int)g_lenientWindowToggleRect.Y)) {
        SetLenientCS2WindowDetection(!IsLenientCS2WindowDetection());
        SaveEvolutionParams();
        InvalidateRect(hw, nullptr, FALSE);
        return;
    }
    int rgbLabelX = cx + 10, rgbBarW = 80, rgbSpacing = 150;
    for (int i = 0; i < 3; ++i) {
        int bx = rgbLabelX + i * rgbSpacing;
        int* rgbV[] = { &g_crosshairR, &g_crosshairG, &g_crosshairB };
        if (ui::CheckSliderClick(mx, my, bx + 20, yRgb, rgbBarW, val)) { *rgbV[i] = (int)(val * 255.f); ApplyCrosshairVisual(g_crosshairR, g_crosshairG, g_crosshairB, g_crosshairStyle, g_crosshairThickness, g_crosshairScale, g_crosshairGap, g_crosshairLength, g_crosshairCenterDot); SaveEvolutionParams(); InvalidateRect(hw, nullptr, FALSE); return; }
    }
    int thBarX = cx + 10 + 40, thBarW = 80;
    if (ui::CheckSliderClick(mx, my, thBarX, yRow2, thBarW, val)) { g_crosshairThickness = 1 + (int)(val * 9.f); ApplyCrosshairVisual(g_crosshairR, g_crosshairG, g_crosshairB, g_crosshairStyle, g_crosshairThickness, g_crosshairScale, g_crosshairGap, g_crosshairLength, g_crosshairCenterDot); SaveEvolutionParams(); InvalidateRect(hw, nullptr, FALSE); return; }
    int scBarX = thBarX + thBarW + 40 + 40, scBarW = 100;
    if (ui::CheckSliderClick(mx, my, scBarX, yRow2, scBarW, val)) { g_crosshairScale = 0.02f + val * 0.58f; ApplyCrosshairVisual(g_crosshairR, g_crosshairG, g_crosshairB, g_crosshairStyle, g_crosshairThickness, g_crosshairScale, g_crosshairGap, g_crosshairLength, g_crosshairCenterDot); SaveEvolutionParams(); InvalidateRect(hw, nullptr, FALSE); return; }
    int ddX = scBarX + scBarW + 40 + 40, ddW = 100;
    if (my >= yRow2 - 2 && my <= yRow2 + 18 && mx >= ddX && mx <= ddX + ddW) { g_styleDropdownOpen = !g_styleDropdownOpen; InvalidateRect(hw, nullptr, FALSE); return; }
    if (g_styleDropdownOpen) {
        for (int j = 0; j < 3; ++j) {
            if (mx >= ddX && mx <= ddX + ddW && my >= yRow2 + 16 + j * 18 && my <= yRow2 + 34 + j * 18) {
                g_crosshairStyle = j; g_styleDropdownOpen = false; ApplyCrosshairVisual(g_crosshairR, g_crosshairG, g_crosshairB, g_crosshairStyle, g_crosshairThickness, g_crosshairScale, g_crosshairGap, g_crosshairLength, g_crosshairCenterDot); SaveEvolutionParams(); InvalidateRect(hw, nullptr, FALSE); return;
            }
        }
        g_styleDropdownOpen = false; InvalidateRect(hw, nullptr, FALSE); return;
    }
    int enableX = ddX + ddW + 40, tx = enableX + 40, tye = yRow2 - 4;
    if (ui::CheckToggleClick(mx, my, tx, tye)) {
        ApplyCrosshairEnabled(!g_crosshairEnabled);
        SaveEvolutionParams();
        RefreshTextguiOverlay();
        InvalidateRect(hw, nullptr, FALSE);
    }
}

void PaintEvolutionPage(Gdiplus::Graphics& g, int cx, int cw, int, HWND)
{
    using namespace Gdiplus;
    using namespace evolutionui;
    const auto& theme = uitheme::get_palette();

    ui::DrawHeader(g, cx, cw, _(i18n::Keys::EVO_TITLE));
    Font rF(L"Microsoft YaHei", 11);
    Font sF(L"Microsoft YaHei", 9);
    SolidBrush text(theme.text);
    SolidBrush dim(theme.dim);
    SolidBrush selectedBackground(theme.card_selected_background);
    SolidBrush buttonBackground(theme.button_background);
    SolidBrush previewBackground(theme.preview_background);
    SolidBrush knobBrush(theme.accent_strong);
    Pen selectedBorder(theme.accent_strong, 1.5f);
    Pen buttonBorder(theme.button_border);

    const int sectionX = cx + 10;
    const int sectionWidth = cw - 20;
    const int volumeY = 54;
    g.DrawString(_(i18n::Keys::EVO_VOL_ADJ), -1, &rF,
        PointF(static_cast<REAL>(sectionX), static_cast<REAL>(volumeY)), &text);
    g_deathMuteToggleRect = RectF(static_cast<REAL>(sectionX + 220),
        static_cast<REAL>(volumeY - 4), 50.f, 24.f);
    ui::DrawToggle(g, static_cast<int>(g_deathMuteToggleRect.X),
        static_cast<int>(g_deathMuteToggleRect.Y), g_deathMute);

    g.DrawString(L"CS2 音量", -1, &sF,
            PointF(static_cast<REAL>(sectionX + 18), static_cast<REAL>(volumeY + 39)), &text);
        volumeSliderRect = RectF(static_cast<REAL>(sectionX + 105), static_cast<REAL>(volumeY + 31),
            static_cast<REAL>((std::max)(140, sectionWidth - 250)), 24.f);
        const float volumeSliderValue = std::clamp(g_death_vol, 0.f, 1.f);
        ui::DrawSlider(g, static_cast<int>(volumeSliderRect.X), volumeY + 39,
            static_cast<int>(volumeSliderRect.Width), volumeSliderValue);
        g.FillEllipse(&knobBrush,
            volumeSliderRect.X + volumeSliderRect.Width * volumeSliderValue - 8.f,
            static_cast<REAL>(volumeY + 33), 16.f, 16.f);
        wchar_t volumeText[32]{};
        swprintf_s(volumeText, L"%.0f%%", g_death_vol * 100.f);
        g.DrawString(volumeText, -1, &sF,
            PointF(volumeSliderRect.X + volumeSliderRect.Width + 10.f,
                static_cast<REAL>(volumeY + 31)), &dim);
        volumeValueRect = RectF(volumeSliderRect.X + volumeSliderRect.Width + 7.f,
            static_cast<REAL>(volumeY + 28), 55.f, 21.f);

        std::wstring keyName;
        if (g_hotkeyVk == 0) keyName = L"None";
        else {
            if (g_hotkeyMod & MOD_CONTROL) keyName += L"Ctrl+";
            if (g_hotkeyMod & MOD_ALT) keyName += L"Alt+";
            if (g_hotkeyMod & MOD_SHIFT) keyName += L"Shift+";
            if ((g_hotkeyVk >= 'A' && g_hotkeyVk <= 'Z') || (g_hotkeyVk >= '0' && g_hotkeyVk <= '9'))
                keyName += static_cast<wchar_t>(g_hotkeyVk);
            else keyName += L"Vk=" + std::to_wstring(g_hotkeyVk);
        }
        g.DrawString(_(i18n::Keys::EVO_HOTKEY), -1, &sF,
            PointF(static_cast<REAL>(sectionX + 18), static_cast<REAL>(volumeY + 76)), &text);
        hotkeyRect = RectF(static_cast<REAL>(sectionX + 175), static_cast<REAL>(volumeY + 69), 190.f, 26.f);
        const std::wstring hotkeyText = g_isBindingHotkey ? _(i18n::Keys::EVO_WAITING_KEY) : keyName;
        ui::DrawRoundedButton(g, hotkeyRect, hotkeyText.c_str(), g_isBindingHotkey, true);
    const int nextSectionY = volumeY + 118;

    g.DrawString(L"宽容检测游戏窗口", -1, &rF,
        PointF(static_cast<REAL>(sectionX), static_cast<REAL>(nextSectionY)), &text);
    g_lenientWindowToggleRect = RectF(static_cast<REAL>(sectionX + 220),
        static_cast<REAL>(nextSectionY - 4), 50.f, 24.f);
    ui::DrawToggle(g, static_cast<int>(g_lenientWindowToggleRect.X),
        static_cast<int>(g_lenientWindowToggleRect.Y), IsLenientCS2WindowDetection());

    const int inputEnvironmentY = nextSectionY + 42;
    g.DrawString(L"尝试为输入环境拦截", -1, &rF,
        PointF(static_cast<REAL>(sectionX), static_cast<REAL>(inputEnvironmentY)), &text);
    g_inputEnvironmentToggleRect = RectF(static_cast<REAL>(sectionX + 220),
        static_cast<REAL>(inputEnvironmentY - 4), 50.f, 24.f);
    ui::DrawToggle(g, static_cast<int>(g_inputEnvironmentToggleRect.X),
        static_cast<int>(g_inputEnvironmentToggleRect.Y), inputenvironment::IsEnabled());

    if (inputenvironment::IsEnabled()) {
        g_inputEnvironmentResetRect = RectF(static_cast<REAL>(sectionX + 286),
            static_cast<REAL>(inputEnvironmentY - 5), 154.f, 26.f);
        GraphicsPath resetPath;
        resetPath.AddArc(g_inputEnvironmentResetRect.X, g_inputEnvironmentResetRect.Y, 12.f, 12.f, 180.f, 90.f);
        resetPath.AddArc(g_inputEnvironmentResetRect.X + g_inputEnvironmentResetRect.Width - 12.f,
            g_inputEnvironmentResetRect.Y, 12.f, 12.f, 270.f, 90.f);
        resetPath.AddArc(g_inputEnvironmentResetRect.X + g_inputEnvironmentResetRect.Width - 12.f,
            g_inputEnvironmentResetRect.Y + g_inputEnvironmentResetRect.Height - 12.f, 12.f, 12.f, 0.f, 90.f);
        resetPath.AddArc(g_inputEnvironmentResetRect.X,
            g_inputEnvironmentResetRect.Y + g_inputEnvironmentResetRect.Height - 12.f, 12.f, 12.f, 90.f, 90.f);
        resetPath.CloseFigure();
        g.FillPath(&buttonBackground, &resetPath);
        g.DrawPath(&buttonBorder, &resetPath);
        g.DrawString(L"重置为非输入状态", -1, &sF,
            PointF(g_inputEnvironmentResetRect.X + 14.f, g_inputEnvironmentResetRect.Y + 5.f), &text);
    } else {
        g_inputEnvironmentResetRect = RectF{};
    }

    const int textguiSectionY = inputEnvironmentY + 42;
    g.DrawString(i18n::T("EVO_TEXTGUI"), -1, &rF,
        PointF(static_cast<REAL>(sectionX), static_cast<REAL>(textguiSectionY)), &text);
    textguiEnableRect = RectF(static_cast<REAL>(sectionX + 220),
        static_cast<REAL>(textguiSectionY - 4), 50.f, 24.f);
    ui::DrawToggle(g, static_cast<int>(textguiEnableRect.X),
        static_cast<int>(textguiEnableRect.Y), g_textguiEnabled);
    textguiFoldRect = RectF(static_cast<REAL>(sectionX + 286),
        static_cast<REAL>(textguiSectionY - 4), 24.f, 24.f);
    if (g_textguiEnabled) {
        ui::DrawFoldButton(g, textguiFoldRect, !textguiCollapsed);
    } else {
        textguiFoldRect = RectF{};
    }

    int crosshairY = textguiSectionY + 42;
    if (!g_textguiEnabled || textguiCollapsed) {
        for (auto& rect : textguiSliderRects) rect = RectF{};
        for (auto& rect : textguiSliderValueRects) rect = RectF{};
        for (auto& rect : textguiColorRects) rect = RectF{};
        for (auto& rect : textguiColorValueRects) rect = RectF{};
        for (auto& rect : textguiAccessoryColorRects) rect = RectF{};
        for (auto& rect : textguiAccessoryColorValueRects) rect = RectF{};
        textguiWatermarkRect = RectF{};
        textguiRainbowRect = RectF{};
        textguiBackdropRect = RectF{};
        textguiBackdropOpacityRect = RectF{};
        textguiBackdropOpacityValueRect = RectF{};
        textguiLogoDetailRect = RectF{};
        textguiLogoDetailValueRect = RectF{};
        textguiSloganRect = RectF{};
    }
    else {
        auto drawSliderWithKnob = [&](const Gdiplus::RectF& rect, float value) {
            ui::DrawSlider(g, static_cast<int>(rect.X), static_cast<int>(rect.Y + 8.f),
                static_cast<int>(rect.Width), value);
            g.FillEllipse(&knobBrush, rect.X + rect.Width * value - 7.f,
                rect.Y + 3.f, 14.f, 14.f);
        };

        const wchar_t* labels[] = {
            i18n::T("EVO_TEXTGUI_X"), i18n::T("EVO_TEXTGUI_Y"),
            i18n::T("EVO_TEXTGUI_SCALE"), i18n::T("EVO_TEXTGUI_OPACITY"),
            i18n::T("EVO_TEXTGUI_LINE_SPACING"), i18n::T("EVO_TEXTGUI_SHADOW"),
            i18n::T("EVO_TEXTGUI_RAINBOW_SPEED"), i18n::T("EVO_TEXTGUI_RAINBOW_SPREAD"),
            i18n::T("EVO_TEXTGUI_RAINBOW_SATURATION"), i18n::T("EVO_TEXTGUI_RAINBOW_BRIGHTNESS")
        };
        const float values[] = {
            (g_textguiX + 0.75f) / 2.5f, (g_textguiY + 0.75f) / 2.5f,
            (g_textguiScale - 0.75f) / 1.05f,
            (g_textguiOpacity - 0.2f) / 0.8f,
            (g_textguiLineSpacing - 0.75f) / 1.05f,
            g_textguiShadowStrength,
            (g_textguiRainbowSpeed - evolutionui::kTextguiRainbowSpeedMin)
                / (evolutionui::kTextguiRainbowSpeedMax - evolutionui::kTextguiRainbowSpeedMin),
            (g_textguiRainbowSpread - evolutionui::kTextguiRainbowSpreadMin)
                / (evolutionui::kTextguiRainbowSpreadMax - evolutionui::kTextguiRainbowSpreadMin),
            g_textguiRainbowSaturation,
            (g_textguiRainbowBrightness - 0.2f) / 0.8f
        };
        const std::wstring shown[] = {
            std::to_wstring(static_cast<int>(std::lround(g_textguiX * 100.f))) + L"%",
            std::to_wstring(static_cast<int>(std::lround(g_textguiY * 100.f))) + L"%",
            std::to_wstring(static_cast<int>(std::lround(g_textguiScale * 100.f))) + L"%",
            std::to_wstring(static_cast<int>(std::lround(g_textguiOpacity * 100.f))) + L"%",
            std::to_wstring(static_cast<int>(std::lround(g_textguiLineSpacing * 100.f))) + L"%",
            std::to_wstring(static_cast<int>(std::lround(g_textguiShadowStrength * 100.f))) + L"%",
            std::to_wstring(static_cast<int>(std::lround(g_textguiRainbowSpeed * 100.f))) + L"%",
            std::to_wstring(static_cast<int>(std::lround(g_textguiRainbowSpread))) + L"°",
            std::to_wstring(static_cast<int>(std::lround(g_textguiRainbowSaturation * 100.f))) + L"%",
            std::to_wstring(static_cast<int>(std::lround(g_textguiRainbowBrightness * 100.f))) + L"%"
        };
        const int textguiBodyY = textguiSectionY + 32;
        const int textguiBarWidth = (std::max)(90, sectionWidth / 2 - 150);
        for (int i = 0; i < 10; ++i) {
            const int column = i % 2;
            const int row = i / 2;
            const int x = sectionX + 14 + column * (sectionWidth / 2);
            const int y = textguiBodyY + row * 34;
            g.DrawString(labels[i], -1, &sF, PointF(static_cast<REAL>(x), static_cast<REAL>(y)), &text);
            textguiSliderRects[i] = RectF(static_cast<REAL>(x + 70), static_cast<REAL>(y - 6),
                static_cast<REAL>(textguiBarWidth), 24.f);
            drawSliderWithKnob(textguiSliderRects[i], std::clamp(values[i], 0.f, 1.f));
            g.DrawString(shown[i].c_str(), -1, &sF,
                PointF(textguiSliderRects[i].X + textguiSliderRects[i].Width + 5.f,
                    static_cast<REAL>(y)), &dim);
            textguiSliderValueRects[i] = RectF(
                textguiSliderRects[i].X + textguiSliderRects[i].Width + 3.f,
                static_cast<REAL>(y - 3), 58.f, 20.f);
        }

        const wchar_t* colorLabels[] = { L"R", L"G", L"B" };
        const int colorValues[] = { g_textguiR, g_textguiG, g_textguiB };
        const int colorY = textguiBodyY + 176;
        const int colorWidth = (std::max)(72, (sectionWidth - 210) / 3);
        for (int i = 0; i < 3; ++i) {
            const int x = sectionX + 14 + i * (colorWidth + 70);
            g.DrawString(colorLabels[i], -1, &sF, PointF(static_cast<REAL>(x), static_cast<REAL>(colorY)), &text);
            textguiColorRects[i] = RectF(static_cast<REAL>(x + 18), static_cast<REAL>(colorY - 6),
                static_cast<REAL>(colorWidth), 24.f);
            drawSliderWithKnob(textguiColorRects[i], colorValues[i] / 255.f);
            wchar_t colorText[8]{};
            swprintf_s(colorText, L"%d", colorValues[i]);
            g.DrawString(colorText, -1, &sF,
                PointF(textguiColorRects[i].X + textguiColorRects[i].Width + 4.f,
                    static_cast<REAL>(colorY)), &dim);
            textguiColorValueRects[i] = RectF(
                textguiColorRects[i].X + textguiColorRects[i].Width + 2.f,
                static_cast<REAL>(colorY - 3), 42.f, 20.f);
        }

        const int accessoryValues[] = {
            g_textguiAccessoryR, g_textguiAccessoryG, g_textguiAccessoryB
        };
        for (int i = 0; i < 3; ++i) {
            const int x = sectionX + 14 + i * (colorWidth + 70);
            g.DrawString(colorLabels[i], -1, &sF,
                PointF(static_cast<REAL>(x), static_cast<REAL>(colorY + 34)), &text);
            textguiAccessoryColorRects[i] = RectF(static_cast<REAL>(x + 18),
                static_cast<REAL>(colorY + 28), static_cast<REAL>(colorWidth), 24.f);
            drawSliderWithKnob(textguiAccessoryColorRects[i], accessoryValues[i] / 255.f);
            const std::wstring shownValue = std::to_wstring(accessoryValues[i]);
            g.DrawString(shownValue.c_str(), -1, &sF,
                PointF(textguiAccessoryColorRects[i].X + textguiAccessoryColorRects[i].Width + 4.f,
                    static_cast<REAL>(colorY + 34)), &dim);
            textguiAccessoryColorValueRects[i] = RectF(
                textguiAccessoryColorRects[i].X + textguiAccessoryColorRects[i].Width + 2.f,
                static_cast<REAL>(colorY + 31), 42.f, 20.f);
        }

        g.DrawString(i18n::T("EVO_TEXTGUI_MARK"), -1, &sF,
            PointF(static_cast<REAL>(sectionX + 14), static_cast<REAL>(colorY + 72)), &text);
        textguiWatermarkRect = RectF(static_cast<REAL>(sectionX + 150),
            static_cast<REAL>(colorY + 65), 50.f, 24.f);
        ui::DrawToggle(g, static_cast<int>(textguiWatermarkRect.X),
            static_cast<int>(textguiWatermarkRect.Y), g_textguiShowWatermark);

        g.DrawString(i18n::T("EVO_TEXTGUI_RAINBOW"), -1, &sF,
            PointF(static_cast<REAL>(sectionX + 230), static_cast<REAL>(colorY + 72)), &text);
        textguiRainbowRect = RectF(static_cast<REAL>(sectionX + 350),
            static_cast<REAL>(colorY + 65), 50.f, 24.f);
        ui::DrawToggle(g, static_cast<int>(textguiRainbowRect.X),
            static_cast<int>(textguiRainbowRect.Y), g_textguiRainbow);
        g.DrawString(L"自定义标语", -1, &sF,
            PointF(static_cast<REAL>(sectionX + 14), static_cast<REAL>(colorY + 108)), &text);
        textguiSloganRect = RectF(static_cast<REAL>(sectionX + 105),
            static_cast<REAL>(colorY + 101), static_cast<REAL>((std::max)(180, sectionWidth - 120)), 25.f);
        const std::wstring sloganShown = g_textguiCustomSlogan.empty() ? L"（留空）" : g_textguiCustomSlogan;
        ui::DrawRoundedButton(g, textguiSloganRect, sloganShown.c_str(), false, true);

        g.DrawString(L"阶梯遮罩", -1, &sF,
            PointF(static_cast<REAL>(sectionX + 14), static_cast<REAL>(colorY + 144)), &text);
        textguiBackdropRect = RectF(static_cast<REAL>(sectionX + 105),
            static_cast<REAL>(colorY + 137), 50.f, 24.f);
        ui::DrawToggle(g, static_cast<int>(textguiBackdropRect.X),
            static_cast<int>(textguiBackdropRect.Y), g_textguiBackdrop);
        g.DrawString(L"透明度", -1, &sF,
            PointF(static_cast<REAL>(sectionX + 185), static_cast<REAL>(colorY + 144)), &text);
        textguiBackdropOpacityRect = RectF(static_cast<REAL>(sectionX + 245),
            static_cast<REAL>(colorY + 137), static_cast<REAL>((std::max)(90, sectionWidth - 350)), 24.f);
        drawSliderWithKnob(textguiBackdropOpacityRect, std::clamp(g_textguiBackdropOpacity, 0.f, 1.f));
        const std::wstring backdropShown = std::to_wstring(
            static_cast<int>(std::lround(g_textguiBackdropOpacity * 100.f))) + L"%";
        g.DrawString(backdropShown.c_str(), -1, &sF,
            PointF(textguiBackdropOpacityRect.X + textguiBackdropOpacityRect.Width + 5.f,
                static_cast<REAL>(colorY + 144)), &dim);
        textguiBackdropOpacityValueRect = RectF(
            textguiBackdropOpacityRect.X + textguiBackdropOpacityRect.Width + 3.f,
            static_cast<REAL>(colorY + 141), 50.f, 20.f);

        g.DrawString(L"Logo内线", -1, &sF,
            PointF(static_cast<REAL>(sectionX + 14), static_cast<REAL>(colorY + 180)), &text);
        textguiLogoDetailRect = RectF(static_cast<REAL>(sectionX + 105),
            static_cast<REAL>(colorY + 173), static_cast<REAL>((std::max)(120, sectionWidth - 220)), 24.f);
        const float logoDetailValue = std::clamp((g_textguiLogoDetailThickness - 0.5f) / 2.5f, 0.f, 1.f);
        drawSliderWithKnob(textguiLogoDetailRect, logoDetailValue);
        wchar_t logoDetailText[16]{};
        swprintf_s(logoDetailText, L"%.1f", g_textguiLogoDetailThickness);
        g.DrawString(logoDetailText, -1, &sF,
            PointF(textguiLogoDetailRect.X + textguiLogoDetailRect.Width + 5.f,
                static_cast<REAL>(colorY + 180)), &dim);
        textguiLogoDetailValueRect = RectF(
            textguiLogoDetailRect.X + textguiLogoDetailRect.Width + 3.f,
            static_cast<REAL>(colorY + 177), 48.f, 20.f);
        crosshairY = colorY + 224;
    }

    const int notificationsSectionY = crosshairY;
    g.DrawString(i18n::T("EVO_NOTIFICATIONS"), -1, &rF,
        PointF(static_cast<REAL>(sectionX), static_cast<REAL>(notificationsSectionY)), &text);
    notificationsEnableRect = RectF(static_cast<REAL>(sectionX + 220),
        static_cast<REAL>(notificationsSectionY - 4), 50.f, 24.f);
    ui::DrawToggle(g, static_cast<int>(notificationsEnableRect.X),
        static_cast<int>(notificationsEnableRect.Y), g_notificationsEnabled);
    notificationsFoldRect = RectF(static_cast<REAL>(sectionX + 286),
        static_cast<REAL>(notificationsSectionY - 4), 24.f, 24.f);
    if (g_notificationsEnabled) {
        ui::DrawFoldButton(g, notificationsFoldRect, !notificationsCollapsed);
    } else {
        notificationsFoldRect = RectF{};
    }

    crosshairY = notificationsSectionY + 42;
    if (!g_notificationsEnabled || notificationsCollapsed) {
        notificationsDurationRect = RectF{};
        notificationsDurationValueRect = RectF{};
        for (auto& rect : notificationsStyleRects) rect = RectF{};
    } else {
        const int notifyBodyY = notificationsSectionY + 34;
        g.DrawString(i18n::T("EVO_NOTIFICATIONS_DURATION"), -1, &sF,
            PointF(static_cast<REAL>(sectionX + 14), static_cast<REAL>(notifyBodyY)), &text);
        notificationsDurationRect = RectF(static_cast<REAL>(sectionX + 118),
            static_cast<REAL>(notifyBodyY - 6), static_cast<REAL>((std::max)(120, sectionWidth - 230)), 24.f);
        const float durationValue = std::clamp((g_notificationsDuration - 1.f) / 4.f, 0.f, 1.f);
        ui::DrawSlider(g, static_cast<int>(notificationsDurationRect.X),
            static_cast<int>(notificationsDurationRect.Y + 8.f),
            static_cast<int>(notificationsDurationRect.Width), durationValue);
        g.FillEllipse(&knobBrush,
            notificationsDurationRect.X + notificationsDurationRect.Width * durationValue - 7.f,
            notificationsDurationRect.Y + 3.f, 14.f, 14.f);
        wchar_t durationText[16]{};
        swprintf_s(durationText, L"%.1fs", g_notificationsDuration);
        const std::wstring durationShown = durationText;
        g.DrawString(durationShown.c_str(), -1, &sF,
            PointF(notificationsDurationRect.X + notificationsDurationRect.Width + 6.f,
                static_cast<REAL>(notifyBodyY)), &dim);
        notificationsDurationValueRect = RectF(
            notificationsDurationRect.X + notificationsDurationRect.Width + 3.f,
            static_cast<REAL>(notifyBodyY - 3), 48.f, 20.f);

        const wchar_t* styleNames[kNotificationsStyleCount] = { L"LiquidBounce", L"VAPE", L"GPT", L"Gemini", L"DeepSeek", L"Square" };
        for (int i = 0; i < kNotificationsStyleCount; ++i) {
            notificationsStyleRects[i] = RectF(static_cast<REAL>(sectionX + 14 + (i % 3) * 126),
                static_cast<REAL>(notifyBodyY + 32 + (i / 3) * 31), 112.f, 25.f);
            ui::DrawRoundedButton(g, notificationsStyleRects[i], styleNames[i], i == g_notificationsStyle, true);
        }
        crosshairY = notifyBodyY + 107;
    }

    g.DrawString(_(i18n::Keys::EVO_CROSSHAIR), -1, &rF,
        PointF(static_cast<REAL>(sectionX), static_cast<REAL>(crosshairY)), &text);
    crosshairEnableRect = RectF(static_cast<REAL>(sectionX + 220),
        static_cast<REAL>(crosshairY - 4), 50.f, 24.f);
    ui::DrawToggle(g, static_cast<int>(crosshairEnableRect.X),
        static_cast<int>(crosshairEnableRect.Y), g_crosshairEnabled);
    crosshairFoldRect = RectF(static_cast<REAL>(sectionX + 286),
        static_cast<REAL>(crosshairY - 4), 24.f, 24.f);
    if (g_crosshairEnabled) {
        ui::DrawFoldButton(g, crosshairFoldRect, !crosshairCollapsed);
    } else {
        crosshairFoldRect = RectF{};
    }

    if (!g_crosshairEnabled || crosshairCollapsed) {
        for (auto& rect : styleRects) rect = RectF{};
        for (auto& rect : rgbSliderRects) rect = RectF{};
        for (auto& rect : rgbValueRects) rect = RectF{};
        for (auto& rect : parameterSliderRects) rect = RectF{};
        for (auto& rect : parameterValueRects) rect = RectF{};
        centerDotRect = RectF{};
        return;
    }

    const int bodyY = crosshairY + 34;

    const wchar_t* styleNames[kCrosshairStyleCount] = {
        i18n::T("EVO_STYLE_HOLLOW"), i18n::T("EVO_STYLE_SOLID"), i18n::T("EVO_STYLE_CLASSIC"),
        i18n::T("EVO_STYLE_CROSS"), i18n::T("EVO_STYLE_CORNERS"), i18n::T("EVO_STYLE_T")
    };
    g.DrawString(_(i18n::Keys::EVO_STYLE), -1, &sF,
        PointF(static_cast<REAL>(sectionX + 14), static_cast<REAL>(bodyY + 12)), &text);
    for (int i = 0; i < kCrosshairStyleCount; ++i) {
        const int column = i % 3;
        const int row = i / 3;
        styleRects[i] = RectF(static_cast<REAL>(sectionX + 60 + column * 96),
            static_cast<REAL>(bodyY + 8 + row * 31), 88.f, 25.f);
        ui::DrawRoundedButton(g, styleRects[i], styleNames[i], i == g_crosshairStyle, true);
    }

    RectF preview(static_cast<REAL>(sectionX + sectionWidth - 142),
        static_cast<REAL>(bodyY + 8), 126.f, 58.f);
    g.FillRectangle(&previewBackground, preview);
    evolutionui::DrawCrosshairShape(g, preview.X + preview.Width / 2.f,
        preview.Y + preview.Height / 2.f,
        Color(255, static_cast<BYTE>(g_crosshairR), static_cast<BYTE>(g_crosshairG),
            static_cast<BYTE>(g_crosshairB)),
        g_crosshairStyle, static_cast<float>(g_crosshairThickness), g_crosshairScale,
        g_crosshairGap, g_crosshairLength, g_crosshairCenterDot);

    const int rgbY = bodyY + 88;
    const wchar_t* rgbLabels[] = { L"R", L"G", L"B" };
    const int rgbValues[] = { g_crosshairR, g_crosshairG, g_crosshairB };
    const int rgbWidth = (std::max)(72, (sectionWidth - 210) / 3);
    for (int i = 0; i < 3; ++i) {
        const int x = sectionX + 14 + i * (rgbWidth + 70);
        g.DrawString(rgbLabels[i], -1, &sF, PointF(static_cast<REAL>(x), static_cast<REAL>(rgbY)), &text);
        rgbSliderRects[i] = RectF(static_cast<REAL>(x + 18), static_cast<REAL>(rgbY - 6),
            static_cast<REAL>(rgbWidth), 24.f);
        ui::DrawSlider(g, static_cast<int>(rgbSliderRects[i].X), rgbY + 2,
            static_cast<int>(rgbSliderRects[i].Width), rgbValues[i] / 255.f);
        wchar_t value[8]{};
        swprintf_s(value, L"%d", rgbValues[i]);
        g.DrawString(value, -1, &sF,
            PointF(rgbSliderRects[i].X + rgbSliderRects[i].Width + 4.f, static_cast<REAL>(rgbY)), &dim);
        rgbValueRects[i] = RectF(rgbSliderRects[i].X + rgbSliderRects[i].Width + 2.f,
            static_cast<REAL>(rgbY - 3), 42.f, 20.f);
    }

    const wchar_t* parameterLabels[] = {
        _(i18n::Keys::EVO_THICKNESS), _(i18n::Keys::EVO_SCALE), L"中心间距", L"准星臂长"
    };
    const float parameterValues[] = {
        (g_crosshairThickness - 1) / 9.f, (g_crosshairScale - 0.02f) / 0.58f,
        g_crosshairGap / 16.f, (g_crosshairLength - 4) / 26.f
    };
    const std::wstring displayValues[] = {
        std::to_wstring(g_crosshairThickness),
        std::to_wstring(static_cast<int>(std::lround(g_crosshairScale * 100.f))) + L"%",
        std::to_wstring(g_crosshairGap), std::to_wstring(g_crosshairLength)
    };
    const int parameterBarWidth = (std::max)(90, sectionWidth / 2 - 150);
    for (int i = 0; i < 4; ++i) {
        const int column = i % 2;
        const int row = i / 2;
        const int x = sectionX + 14 + column * (sectionWidth / 2);
        const int y = bodyY + 126 + row * 38;
        g.DrawString(parameterLabels[i], -1, &sF,
            PointF(static_cast<REAL>(x), static_cast<REAL>(y)), &text);
        parameterSliderRects[i] = RectF(static_cast<REAL>(x + 72), static_cast<REAL>(y - 6),
            static_cast<REAL>(parameterBarWidth), 24.f);
        ui::DrawSlider(g, static_cast<int>(parameterSliderRects[i].X), y + 2,
            static_cast<int>(parameterSliderRects[i].Width), std::clamp(parameterValues[i], 0.f, 1.f));
        g.DrawString(displayValues[i].c_str(), -1, &sF,
            PointF(parameterSliderRects[i].X + parameterSliderRects[i].Width + 5.f,
                static_cast<REAL>(y)), &dim);
        parameterValueRects[i] = RectF(
            parameterSliderRects[i].X + parameterSliderRects[i].Width + 3.f,
            static_cast<REAL>(y - 3), 58.f, 20.f);
    }

    g.DrawString(L"显示中心点", -1, &sF,
        PointF(static_cast<REAL>(sectionX + 14), static_cast<REAL>(bodyY + 207)), &text);
    centerDotRect = RectF(static_cast<REAL>(sectionX + 100), static_cast<REAL>(bodyY + 200), 50.f, 24.f);
    ui::DrawToggle(g, static_cast<int>(centerDotRect.X), static_cast<int>(centerDotRect.Y),
        g_crosshairCenterDot);
    g.DrawString(L"仅对十字、四角标和 T 型样式生效", -1, &sF,
        PointF(static_cast<REAL>(sectionX + 165), static_cast<REAL>(bodyY + 207)), &dim);
}

void CheckEvolutionClick(HWND hw, int mx, int my)
{
    using namespace evolutionui;
    float value = 0.f;

    if (Hit(g_deathMuteToggleRect, mx, my)) {
        if (!normalgen::CheckAdminPermission() && !g_deathMute) {
            const int result = MessageBoxW(hw,
                L"音量降低器需要管理员权限才能正常工作。\n是否重新以管理员身份启动程序？",
                L"权限不足", MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
            if (result == IDYES) {
                if (g_hMutex) { CloseHandle(g_hMutex); g_hMutex = nullptr; }
                wchar_t exePath[MAX_PATH]{};
                GetModuleFileNameW(nullptr, exePath, MAX_PATH);
                SHELLEXECUTEINFOW executeInfo{};
                executeInfo.cbSize = sizeof(executeInfo);
                executeInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
                executeInfo.lpVerb = L"runas";
                executeInfo.lpFile = exePath;
                executeInfo.nShow = SW_SHOWNORMAL;
                if (ShellExecuteExW(&executeInfo)) DestroyWindow(hw);
            }
            return;
        }
        g_deathMute = !g_deathMute;
        if (g_deathMute) StartCS2VolumeControl(g_death_vol);
        else StopCS2VolumeControl();
        SaveEvolutionParams();
        RefreshTextguiOverlay();
        std::cout << "[进化分支] 即时音量调整已切换为: " << (g_deathMute ? "开启" : "关闭") << std::endl;
        InvalidateRect(hw, nullptr, FALSE);
        return;
    }

    if (Hit(hotkeyRect, mx, my)) {
        g_isBindingHotkey = !g_isBindingHotkey;
        SetFocus(hw);
        InvalidateRect(hw, nullptr, FALSE);
        return;
    }
    if (g_isBindingHotkey && !Hit(hotkeyRect, mx, my)) g_isBindingHotkey = false;

    if (Hit(volumeValueRect, mx, my)) {
        std::wstring input = std::to_wstring(g_death_vol * 100.f);
        if (!prompt_input(hw, L"输入 CS2 音量百分比", input)) return;
        double entered = 0.0;
        if (!parse_number_in_int_range(input, entered)) {
            MessageBoxW(hw, L"请输入 int 范围内的有效数字。", L"数值无效", MB_OK | MB_ICONWARNING);
            return;
        }
        g_death_vol = static_cast<float>(entered / 100.0);
        if (g_deathMute) SetCS2VolumeReduction(std::clamp(g_death_vol, 0.f, 1.f));
        SaveEvolutionParams();
        InvalidateRect(hw, nullptr, FALSE);
        return;
    }

    if (ui::CheckSliderClick(mx, my,
        static_cast<int>(volumeSliderRect.X), static_cast<int>(volumeSliderRect.Y + 8.f),
        static_cast<int>(volumeSliderRect.Width), value)) {
        g_death_vol = value;
        if (g_deathMute) SetCS2VolumeReduction(g_death_vol);
        SaveEvolutionParams();
        std::cout << "[进化分支] CS2 音量比例已调整为: " << static_cast<int>(g_death_vol * 100.f) << "%" << std::endl;
        InvalidateRect(hw, nullptr, FALSE);
        return;
    }

    if (Hit(g_lenientWindowToggleRect, mx, my)) {
        SetLenientCS2WindowDetection(!IsLenientCS2WindowDetection());
        SaveEvolutionParams();
        std::cout << "[进化分支] 宽容检测游戏窗口已切换" << std::endl;
        InvalidateRect(hw, nullptr, FALSE);
        return;
    }

    if (Hit(g_inputEnvironmentToggleRect, mx, my)) {
        inputenvironment::SetEnabled(!inputenvironment::IsEnabled());
        SaveEvolutionParams();
        std::cout << "[进化分支] 尝试为输入环境拦截已切换为: "
                  << (inputenvironment::IsEnabled() ? "开启" : "关闭") << std::endl;
        InvalidateRect(hw, nullptr, FALSE);
        return;
    }

    if (inputenvironment::IsEnabled() && Hit(g_inputEnvironmentResetRect, mx, my)) {
        inputenvironment::ForceNonInputState();
        InvalidateRect(hw, nullptr, FALSE);
        return;
    }

    if (Hit(textguiEnableRect, mx, my)) {
        ApplyTextguiEnabled(!g_textguiEnabled);
        SaveEvolutionParams();
        notifications_overlay::Push(L"Textgui", g_textguiEnabled);
        std::cout << "[Textgui] UI开关已切换为: " << (g_textguiEnabled ? "开启" : "关闭") << std::endl;
        InvalidateRect(hw, nullptr, FALSE);
        return;
    }

    if (g_textguiEnabled && Hit(textguiFoldRect, mx, my)) {
        textguiCollapsed = !textguiCollapsed;
        InvalidateRect(hw, nullptr, FALSE);
        return;
    }

    if (g_textguiEnabled && !textguiCollapsed) {
        for (int i = 0; i < 10; ++i) {
            if (!Hit(textguiSliderValueRects[i], mx, my)) continue;
            double current = 0.0;
            if (i == 0) current = g_textguiX * 100.0;
            else if (i == 1) current = g_textguiY * 100.0;
            else if (i == 2) current = g_textguiScale * 100.0;
            else if (i == 3) current = g_textguiOpacity * 100.0;
            else if (i == 4) current = g_textguiLineSpacing * 100.0;
            else if (i == 5) current = g_textguiShadowStrength * 100.0;
            else if (i == 6) current = g_textguiRainbowSpeed * 100.0;
            else if (i == 7) current = g_textguiRainbowSpread;
            else if (i == 8) current = g_textguiRainbowSaturation * 100.0;
            else current = g_textguiRainbowBrightness * 100.0;
            std::wstring input = std::to_wstring(current);
            if (!prompt_input(hw, L"输入精确数值（允许超出滑块范围）", input)) return;
            double entered = 0.0;
            if (!parse_number_in_int_range(input, entered)) {
                MessageBoxW(hw, L"请输入 int 范围内的有效数字。", L"数值无效", MB_OK | MB_ICONWARNING);
                return;
            }
            if (i == 0) g_textguiX = static_cast<float>(entered / 100.0);
            else if (i == 1) g_textguiY = static_cast<float>(entered / 100.0);
            else if (i == 2) g_textguiScale = static_cast<float>(entered / 100.0);
            else if (i == 3) g_textguiOpacity = static_cast<float>(entered / 100.0);
            else if (i == 4) g_textguiLineSpacing = static_cast<float>(entered / 100.0);
            else if (i == 5) g_textguiShadowStrength = static_cast<float>(entered / 100.0);
            else if (i == 6) g_textguiRainbowSpeed = static_cast<float>(entered / 100.0);
            else if (i == 7) g_textguiRainbowSpread = static_cast<float>(entered);
            else if (i == 8) g_textguiRainbowSaturation = static_cast<float>(entered / 100.0);
            else g_textguiRainbowBrightness = static_cast<float>(entered / 100.0);
            SaveEvolutionParams();
            RefreshTextguiOverlay();
            std::cout << "[Textgui] 已精确输入参数 " << i << " = " << entered << std::endl;
            InvalidateRect(hw, nullptr, FALSE);
            return;
        }

        for (int i = 0; i < 10; ++i) {
            if (!ui::CheckSliderClick(mx, my, static_cast<int>(textguiSliderRects[i].X),
                static_cast<int>(textguiSliderRects[i].Y + 8.f),
                static_cast<int>(textguiSliderRects[i].Width), value)) continue;
            if (i == 0) g_textguiX = -0.75f + value * 2.5f;
            else if (i == 1) g_textguiY = -0.75f + value * 2.5f;
            else if (i == 2) g_textguiScale = 0.75f + value * 1.05f;
            else if (i == 3) g_textguiOpacity = 0.2f + value * 0.8f;
            else if (i == 4) g_textguiLineSpacing = 0.75f + value * 1.05f;
            else if (i == 5) g_textguiShadowStrength = value;
            else if (i == 6) g_textguiRainbowSpeed = evolutionui::kTextguiRainbowSpeedMin
                + value * (evolutionui::kTextguiRainbowSpeedMax - evolutionui::kTextguiRainbowSpeedMin);
            else if (i == 7) g_textguiRainbowSpread = evolutionui::kTextguiRainbowSpreadMin
                + value * (evolutionui::kTextguiRainbowSpreadMax - evolutionui::kTextguiRainbowSpreadMin);
            else if (i == 8) g_textguiRainbowSaturation = value;
            else g_textguiRainbowBrightness = 0.2f + value * 0.8f;
            ApplyTextguiEnabled(g_textguiEnabled);
            SaveEvolutionParams();
            RefreshTextguiOverlay();
            InvalidateRect(hw, nullptr, FALSE);
            return;
        }

        int* colorValues[] = { &g_textguiR, &g_textguiG, &g_textguiB };
        for (int i = 0; i < 3; ++i) {
            if (Hit(textguiColorValueRects[i], mx, my)) {
                std::wstring input = std::to_wstring(*colorValues[i]);
                if (!prompt_input(hw, L"输入主文字颜色通道值", input)) return;
                double entered = 0.0;
                if (!parse_number_in_int_range(input, entered)) {
                    MessageBoxW(hw, L"请输入 int 范围内的有效数字。", L"数值无效", MB_OK | MB_ICONWARNING);
                    return;
                }
                *colorValues[i] = static_cast<int>(entered);
                SaveEvolutionParams();
                RefreshTextguiOverlay();
                InvalidateRect(hw, nullptr, FALSE);
                return;
            }
            if (!ui::CheckSliderClick(mx, my, static_cast<int>(textguiColorRects[i].X),
                static_cast<int>(textguiColorRects[i].Y + 8.f),
                static_cast<int>(textguiColorRects[i].Width), value)) continue;
            *colorValues[i] = static_cast<int>(std::lround(value * 255.f));
            ApplyTextguiEnabled(g_textguiEnabled);
            SaveEvolutionParams();
            RefreshTextguiOverlay();
            InvalidateRect(hw, nullptr, FALSE);
            return;
        }

        int* accessoryValues[] = {
            &g_textguiAccessoryR, &g_textguiAccessoryG, &g_textguiAccessoryB
        };
        for (int i = 0; i < 3; ++i) {
            if (Hit(textguiAccessoryColorValueRects[i], mx, my)) {
                std::wstring input = std::to_wstring(*accessoryValues[i]);
                if (!prompt_input(hw, L"输入附属参数颜色通道值", input)) return;
                double entered = 0.0;
                if (!parse_number_in_int_range(input, entered)) {
                    MessageBoxW(hw, L"请输入 int 范围内的有效数字。", L"数值无效", MB_OK | MB_ICONWARNING);
                    return;
                }
                *accessoryValues[i] = static_cast<int>(entered);
                SaveEvolutionParams();
                RefreshTextguiOverlay();
                InvalidateRect(hw, nullptr, FALSE);
                return;
            }
            if (!ui::CheckSliderClick(mx, my, static_cast<int>(textguiAccessoryColorRects[i].X),
                static_cast<int>(textguiAccessoryColorRects[i].Y + 8.f),
                static_cast<int>(textguiAccessoryColorRects[i].Width), value)) continue;
            *accessoryValues[i] = static_cast<int>(std::lround(value * 255.f));
            SaveEvolutionParams();
            RefreshTextguiOverlay();
            InvalidateRect(hw, nullptr, FALSE);
            return;
        }

        if (Hit(textguiSloganRect, mx, my)) {
            std::wstring slogan = g_textguiCustomSlogan;
            if (!prompt_input(hw, L"自定义标语（留空则不显示）", slogan)) return;
            g_textguiCustomSlogan = slogan;
            SaveEvolutionParams();
            RefreshTextguiOverlay();
            std::wcout << L"[Textgui] 自定义标语已更新。" << std::endl;
            InvalidateRect(hw, nullptr, FALSE);
            return;
        }

        if (Hit(textguiBackdropRect, mx, my)) {
            g_textguiBackdrop = !g_textguiBackdrop;
            SaveEvolutionParams();
            RefreshTextguiOverlay();
            std::cout << "[Textgui] 阶梯遮罩已切换为: "
                      << (g_textguiBackdrop ? "开启" : "关闭") << std::endl;
            InvalidateRect(hw, nullptr, FALSE);
            return;
        }

        if (Hit(textguiBackdropOpacityValueRect, mx, my)) {
            std::wstring input = std::to_wstring(g_textguiBackdropOpacity * 100.f);
            if (!prompt_input(hw, L"输入遮罩透明度百分比", input)) return;
            double entered = 0.0;
            if (!parse_number_in_int_range(input, entered)) {
                MessageBoxW(hw, L"请输入 int 范围内的有效数字。", L"数值无效", MB_OK | MB_ICONWARNING);
                return;
            }
            g_textguiBackdropOpacity = static_cast<float>(entered / 100.0);
            SaveEvolutionParams();
            RefreshTextguiOverlay();
            InvalidateRect(hw, nullptr, FALSE);
            return;
        }
        if (ui::CheckSliderClick(mx, my, static_cast<int>(textguiBackdropOpacityRect.X),
            static_cast<int>(textguiBackdropOpacityRect.Y + 8.f),
            static_cast<int>(textguiBackdropOpacityRect.Width), value)) {
            g_textguiBackdropOpacity = value;
            SaveEvolutionParams();
            RefreshTextguiOverlay();
            InvalidateRect(hw, nullptr, FALSE);
            return;
        }

        if (Hit(textguiLogoDetailValueRect, mx, my)) {
            std::wstring input = std::to_wstring(g_textguiLogoDetailThickness);
            if (!prompt_input(hw, L"输入 Logo 内部构图线条粗细", input)) return;
            double entered = 0.0;
            if (!parse_number_in_int_range(input, entered)) {
                MessageBoxW(hw, L"请输入 int 范围内的有效数字。", L"数值无效", MB_OK | MB_ICONWARNING);
                return;
            }
            g_textguiLogoDetailThickness = static_cast<float>(entered);
            SaveEvolutionParams();
            RefreshTextguiOverlay();
            InvalidateRect(hw, nullptr, FALSE);
            return;
        }
        if (ui::CheckSliderClick(mx, my, static_cast<int>(textguiLogoDetailRect.X),
            static_cast<int>(textguiLogoDetailRect.Y + 8.f),
            static_cast<int>(textguiLogoDetailRect.Width), value)) {
            g_textguiLogoDetailThickness = 0.5f + value * 2.5f;
            SaveEvolutionParams();
            RefreshTextguiOverlay();
            InvalidateRect(hw, nullptr, FALSE);
            return;
        }

        if (Hit(textguiWatermarkRect, mx, my)) {
            g_textguiShowWatermark = !g_textguiShowWatermark;
            SaveEvolutionParams();
            RefreshTextguiOverlay();
            std::cout << "[Textgui] StrikeSense标识显示已切换为: "
                      << (g_textguiShowWatermark ? "开启" : "关闭") << std::endl;
            InvalidateRect(hw, nullptr, FALSE);
            return;
        }
    }

    if (g_textguiEnabled && !textguiCollapsed && Hit(textguiRainbowRect, mx, my)) {
        g_textguiRainbow = !g_textguiRainbow;
        SaveEvolutionParams();
        RefreshTextguiOverlay();
        notifications_overlay::Push(L"ARGB Rainbow", g_textguiRainbow);
        std::cout << "[Textgui] ARGB彩虹流动已切换为: "
                  << (g_textguiRainbow ? "开启" : "关闭") << std::endl;
        InvalidateRect(hw, nullptr, FALSE);
        return;
    }

    if (Hit(notificationsEnableRect, mx, my)) {
        g_notificationsEnabled = !g_notificationsEnabled;
        SaveEvolutionParams();
        notifications_overlay::Push(L"Notifications", g_notificationsEnabled);
        InvalidateRect(hw, nullptr, FALSE);
        return;
    }
    if (g_notificationsEnabled && Hit(notificationsFoldRect, mx, my)) {
        notificationsCollapsed = !notificationsCollapsed;
        InvalidateRect(hw, nullptr, FALSE);
        return;
    }
    if (g_notificationsEnabled && !notificationsCollapsed) {
        if (Hit(notificationsDurationValueRect, mx, my)) {
            std::wstring input = std::to_wstring(g_notificationsDuration);
            if (!prompt_input(hw, L"输入通知持续秒数", input)) return;
            double entered = 0.0;
            if (!parse_number_in_int_range(input, entered)) {
                MessageBoxW(hw, L"请输入 int 范围内的有效数字。", L"数值无效", MB_OK | MB_ICONWARNING);
                return;
            }
            g_notificationsDuration = static_cast<float>(entered);
            SaveEvolutionParams();
            InvalidateRect(hw, nullptr, FALSE);
            return;
        }
        if (ui::CheckSliderClick(mx, my, static_cast<int>(notificationsDurationRect.X),
            static_cast<int>(notificationsDurationRect.Y + 8.f),
            static_cast<int>(notificationsDurationRect.Width), value)) {
            g_notificationsDuration = 1.f + value * 4.f;
            SaveEvolutionParams();
            notifications_overlay::Push(L"Duration", true);
            InvalidateRect(hw, nullptr, FALSE);
            return;
        }
        for (int i = 0; i < evolutionui::kNotificationsStyleCount; ++i) {
            if (!Hit(notificationsStyleRects[i], mx, my)) continue;
            g_notificationsStyle = i;
            SaveEvolutionParams();
            notifications_overlay::Push(L"Notification Style", true);
            InvalidateRect(hw, nullptr, FALSE);
            return;
        }
    }

    if (Hit(crosshairEnableRect, mx, my)) {
        ApplyCrosshairEnabled(!g_crosshairEnabled);
        SaveEvolutionParams();
        notifications_overlay::Push(L"Sniper Crosshair", g_crosshairEnabled);
        std::cout << "[狙击准星] 已切换为: " << (g_crosshairEnabled ? "开启" : "关闭") << std::endl;
        InvalidateRect(hw, nullptr, FALSE);
        return;
    }

    if (g_crosshairEnabled && Hit(crosshairFoldRect, mx, my)) {
        crosshairCollapsed = !crosshairCollapsed;
        InvalidateRect(hw, nullptr, FALSE);
        return;
    }

    if (!g_crosshairEnabled || crosshairCollapsed) return;

    for (int i = 0; i < kCrosshairStyleCount; ++i) {
        if (!Hit(styleRects[i], mx, my)) continue;
        g_crosshairStyle = i;
        ApplyCrosshairVisual(g_crosshairR, g_crosshairG, g_crosshairB, g_crosshairStyle,
            g_crosshairThickness, g_crosshairScale, g_crosshairGap, g_crosshairLength,
            g_crosshairCenterDot);
        SaveEvolutionParams();
        std::cout << "[狙击准星] 样式已切换为: " << i << std::endl;
        InvalidateRect(hw, nullptr, FALSE);
        return;
    }

    int* rgbValues[] = { &g_crosshairR, &g_crosshairG, &g_crosshairB };
    for (int i = 0; i < 3; ++i) {
        if (Hit(rgbValueRects[i], mx, my)) {
            std::wstring input = std::to_wstring(*rgbValues[i]);
            if (!prompt_input(hw, L"输入准星颜色通道值", input)) return;
            double entered = 0.0;
            if (!parse_number_in_int_range(input, entered)) {
                MessageBoxW(hw, L"请输入 int 范围内的有效数字。", L"数值无效", MB_OK | MB_ICONWARNING);
                return;
            }
            *rgbValues[i] = static_cast<int>(entered);
            RefreshCrosshairOverlay();
            SaveEvolutionParams();
            InvalidateRect(hw, nullptr, FALSE);
            return;
        }
        if (!ui::CheckSliderClick(mx, my, static_cast<int>(rgbSliderRects[i].X),
            static_cast<int>(rgbSliderRects[i].Y + 8.f), static_cast<int>(rgbSliderRects[i].Width), value)) continue;
        *rgbValues[i] = static_cast<int>(std::lround(value * 255.f));
        ApplyCrosshairVisual(g_crosshairR, g_crosshairG, g_crosshairB, g_crosshairStyle,
            g_crosshairThickness, g_crosshairScale, g_crosshairGap, g_crosshairLength,
            g_crosshairCenterDot);
        SaveEvolutionParams();
        InvalidateRect(hw, nullptr, FALSE);
        return;
    }

    for (int i = 0; i < 4; ++i) {
        if (Hit(parameterValueRects[i], mx, my)) {
            double current = i == 0 ? static_cast<double>(g_crosshairThickness)
                : i == 1 ? static_cast<double>(g_crosshairScale * 100.f)
                : i == 2 ? static_cast<double>(g_crosshairGap)
                : static_cast<double>(g_crosshairLength);
            std::wstring input = std::to_wstring(current);
            if (!prompt_input(hw, L"输入精确准星参数（允许超出滑块范围）", input)) return;
            double entered = 0.0;
            if (!parse_number_in_int_range(input, entered)) {
                MessageBoxW(hw, L"请输入 int 范围内的有效数字。", L"数值无效", MB_OK | MB_ICONWARNING);
                return;
            }
            if (i == 0) g_crosshairThickness = static_cast<int>(entered);
            else if (i == 1) g_crosshairScale = static_cast<float>(entered / 100.0);
            else if (i == 2) g_crosshairGap = static_cast<int>(entered);
            else g_crosshairLength = static_cast<int>(entered);
            RefreshCrosshairOverlay();
            SaveEvolutionParams();
            std::cout << "[狙击准星] 已精确输入参数 " << i << " = " << entered << std::endl;
            InvalidateRect(hw, nullptr, FALSE);
            return;
        }
        if (!ui::CheckSliderClick(mx, my, static_cast<int>(parameterSliderRects[i].X),
            static_cast<int>(parameterSliderRects[i].Y + 8.f),
            static_cast<int>(parameterSliderRects[i].Width), value)) continue;
        if (i == 0) g_crosshairThickness = 1 + static_cast<int>(std::lround(value * 9.f));
        else if (i == 1) g_crosshairScale = 0.02f + value * 0.58f;
        else if (i == 2) g_crosshairGap = static_cast<int>(std::lround(value * 16.f));
        else g_crosshairLength = 4 + static_cast<int>(std::lround(value * 26.f));
        ApplyCrosshairVisual(g_crosshairR, g_crosshairG, g_crosshairB, g_crosshairStyle,
            g_crosshairThickness, g_crosshairScale, g_crosshairGap, g_crosshairLength,
            g_crosshairCenterDot);
        SaveEvolutionParams();
        std::cout << "[狙击准星] 参数 " << i << " 已调整" << std::endl;
        InvalidateRect(hw, nullptr, FALSE);
        return;
    }

    if (Hit(centerDotRect, mx, my)) {
        g_crosshairCenterDot = !g_crosshairCenterDot;
        ApplyCrosshairVisual(g_crosshairR, g_crosshairG, g_crosshairB, g_crosshairStyle,
            g_crosshairThickness, g_crosshairScale, g_crosshairGap, g_crosshairLength,
            g_crosshairCenterDot);
        SaveEvolutionParams();
        std::cout << "[狙击准星] 中心点已切换为: " << (g_crosshairCenterDot ? "开启" : "关闭") << std::endl;
        InvalidateRect(hw, nullptr, FALSE);
    }
}
