#pragma once

#include "resource.h"
#include "pages.h"

// 全局变量声明
extern HINSTANCE hInst;
extern int g_currentPage;
extern bool g_langCN;
extern bool g_styleDropdownOpen;
extern int g_dropdownSelection;
extern Gdiplus::RectF g_dropdownRects[3];
extern WCHAR szWindowClass[];

// 准星 / 快捷键
extern float g_death_vol;
extern bool  g_deathMute;
extern int   g_hotkeyMod, g_hotkeyVk;
extern bool  g_hotkeyWaiting;
extern bool  g_crosshairEnabled;
extern int   g_crosshairR, g_crosshairG, g_crosshairB;
extern int   g_crosshairStyle, g_crosshairThickness;
extern float g_crosshairScale;

void DestroyCrosshairInternal();
void InitLegalCfgPage();