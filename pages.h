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
    PAGE_OVERCLOCK = 4,
    PAGE_ITEMHELPER = 5,
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
extern Gdiplus::RectF g_dropdownRects[3];
extern WCHAR szWindowClass[];

// 进化页面变量
extern float g_death_vol;
extern bool  g_deathMute;
extern int   g_hotkeyMod, g_hotkeyVk;
extern bool  g_hotkeyWaiting;

// 准星变量
extern bool  g_crosshairEnabled;
extern int   g_crosshairR, g_crosshairG, g_crosshairB;
extern int   g_crosshairStyle, g_crosshairThickness;
extern float g_crosshairScale;

// ===== 进化参数持久化 =====
void SaveEvolutionParams();
void LoadEvolutionParams();

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
void PaintOverclockPage(Gdiplus::Graphics& g, int cx, int cw, int H, HWND hw);
void PaintItemHelperPage(Gdiplus::Graphics& g, int cx, int cw, int H, HWND hw);

// ===== 各页面 Click =====
void CheckSidebarClick(HWND hw, int mx, int my);
void CheckSoundsClick(HWND hw, int mx, int my);
void CheckSettingsClick(HWND hw, int mx, int my);
void CheckEvolutionClick(HWND hw, int mx, int my);
void CheckLegalCfgClick(HWND hw, int mx, int my);
void CheckOverclockClick(HWND hw, int mx, int my);
void CheckItemHelperClick(HWND hw, int mx, int my);

// ===== 合法配置键盘输入 =====
void InitLegalCfgPage();
bool ProcessLegalCfgKeyInput(HWND hw, UINT msg, WPARAM wp, LPARAM lp);