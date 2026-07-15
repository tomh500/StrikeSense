#pragma once
#include <Windows.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include "config.h"

// ===== 页面枚举 =====
enum Page {
    PAGE_SOUNDS = 0,
    PAGE_SETTINGS = 1,
    PAGE_EVOLUTION = 2,
    PAGE_LEGALCFG = 3,
    PAGE_Rage = 4,
    PAGE_ITEMHELPER = 5,
    PAGE_VSCRIPT = 6,
    PAGE_COUNT
};

// ===== 侧边栏 =====
extern const int SIDEBAR_W;
struct SidebarItem {
    const wchar_t* label;
    int page;
    int y;
};
extern const SidebarItem g_sidebarItems[];
extern int g_currentPage;

// ===== 全局共享变量 =====
extern HINSTANCE hInst;
extern bool g_langCN;
extern bool g_styleDropdownOpen;
extern int  g_dropdownSelection;
extern Gdiplus::RectF g_dropdownRects[6];
extern WCHAR szWindowClass[];
extern HANDLE g_hMutex; // 全局互斥锁（用于管理员提权时释放）

// 进化页面变量
extern float g_death_vol;
extern bool  g_deathMute;
extern int   g_hotkeyMod, g_hotkeyVk;
extern bool  g_hotkeyWaiting;
extern Gdiplus::RectF g_deathMuteToggleRect;

// 准星变量
extern bool  g_crosshairEnabled;
extern int   g_crosshairR, g_crosshairG, g_crosshairB;
extern int   g_crosshairStyle, g_crosshairThickness;
extern int   g_crosshairGap, g_crosshairLength;
extern bool  g_crosshairCenterDot;
extern float g_crosshairScale;

// Textgui 变量
extern bool  g_textguiEnabled;
extern float g_textguiX, g_textguiY, g_textguiScale, g_textguiOpacity;
extern float g_textguiLineSpacing, g_textguiShadowStrength;
extern float g_textguiRainbowSpeed;
extern float g_textguiRainbowSpread, g_textguiRainbowSaturation, g_textguiRainbowBrightness;
extern int   g_textguiR, g_textguiG, g_textguiB;
extern bool  g_textguiShowWatermark;
extern bool  g_textguiRainbow;

// ===== 进化参数持久化 =====
void SaveEvolutionParams();
void LoadEvolutionParams();
void ApplyCrosshairEnabled(bool enabled);
void ApplyCrosshairVisual(int r, int g, int b, int style, int thickness, float scale,
    int gap, int length, bool centerDot);
void RefreshCrosshairOverlay();
void ApplyTextguiEnabled(bool enabled);
void RefreshTextguiOverlay();

// ===== 通用 UI 工具 =====
namespace ui {
    void DrawHeader(Gdiplus::Graphics& g, int cx, int cw, const wchar_t* title);
    void DrawToggle(Gdiplus::Graphics& g, int tx, int ty, bool state);
    void DrawSlider(Gdiplus::Graphics& g, int sx, int sy, int sw, float value);
    bool CheckToggleClick(int mx, int my, int tx, int ty);
    bool CheckSliderClick(int mx, int my, int sx, int sy, int sw, float& outVal);
}

// ===== 各页面 Paint =====
void PaintSidebar(Gdiplus::Graphics& g, int W, int H);
void PaintSoundsPage(Gdiplus::Graphics& g, int cx, int cw, int H, HWND hw);
void PaintSettingsPage(Gdiplus::Graphics& g, int cx, int cw, int H, HWND hw);
void PaintEvolutionPage(Gdiplus::Graphics& g, int cx, int cw, int H, HWND hw);
void PaintLegalCfgPage(Gdiplus::Graphics& g, int cx, int cw, int H, HWND hw);
void PaintRagePage(Gdiplus::Graphics& g, int cx, int cw, int H, HWND hw);
void PaintItemHelperPage(Gdiplus::Graphics& g, int cx, int cw, int H, HWND hw);
void PaintVscriptPage(Gdiplus::Graphics& g, int cx, int cw, int H, HWND hw);

// ===== 各页面 Click =====
void CheckSidebarClick(HWND hw, int mx, int my);
void CheckSoundsClick(HWND hw, int mx, int my);
void CheckSettingsClick(HWND hw, int mx, int my);
void CheckEvolutionClick(HWND hw, int mx, int my);
void CheckLegalCfgClick(HWND hw, int mx, int my);
void CheckRageClick(HWND hw, int mx, int my);
void CheckItemHelperClick(HWND hw, int mx, int my);
void CheckVscriptClick(HWND hw, int mx, int my);
bool IsRageModeEnabled();
void EnableRageModeFromLaunch(HWND hw);
bool IsLegalCfgManaged();
bool HasLegalCfgSOCD();
bool HasLegalCfgMwheelJump();
bool HasLegalCfgMixedSensitivity();
bool HasLegalCfgCrosshairSwitch();
bool HasLegalCfgSoundReplace();

// ===== 合法配置键盘输入 =====
void InitLegalCfgPage();
bool ProcessLegalCfgKeyInput(HWND hw, UINT msg, WPARAM wp, LPARAM lp);
