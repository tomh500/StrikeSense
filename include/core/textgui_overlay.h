#pragma once

#include <windows.h>

extern bool g_textguiEnabled;
extern float g_textguiX;
extern float g_textguiY;
extern float g_textguiScale;
extern float g_textguiOpacity;
extern int g_textguiR;
extern int g_textguiG;
extern int g_textguiB;
extern bool g_textguiShowWatermark;
extern bool g_textguiRainbow;

namespace textgui_overlay {
void Initialize(HINSTANCE hInst);
void ApplyEnabled(bool enabled);
void Refresh();
void Shutdown();
}
