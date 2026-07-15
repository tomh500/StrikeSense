#pragma once

#include "resource.h"
#include "pages.h"

// 全局变量声明
extern HINSTANCE hInst;
extern int g_currentPage;
extern bool g_langCN;
extern bool g_styleDropdownOpen;
extern int g_dropdownSelection;
extern Gdiplus::RectF g_dropdownRects[6];
extern WCHAR szWindowClass[];
extern HANDLE g_hMutex;
extern bool g_itemHelperEnabled;
extern int g_itemHelperHotkeyMod;
extern int g_itemHelperHotkeyVk;
extern bool g_isBindingItemHelperHotkey;

extern bool g_itemHelperEnabled;

// 准星 / 快捷键
extern float g_death_vol;
extern bool  g_deathMute;
extern int   g_hotkeyMod, g_hotkeyVk;
extern bool  g_hotkeyWaiting;
extern bool  g_crosshairEnabled;
extern int   g_crosshairR, g_crosshairG, g_crosshairB;
extern int   g_crosshairStyle, g_crosshairThickness;
extern int   g_crosshairGap, g_crosshairLength;
extern bool  g_crosshairCenterDot;
extern float g_crosshairScale;
extern bool  g_textguiEnabled;
extern float g_textguiX, g_textguiY, g_textguiScale, g_textguiOpacity;
extern float g_textguiRainbowSpeed;
extern int   g_textguiR, g_textguiG, g_textguiB;
extern bool  g_textguiShowWatermark;
extern bool  g_textguiRainbow;

void DestroyCrosshairInternal();
void InitLegalCfgPage();
