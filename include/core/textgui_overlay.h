#pragma once

#include <windows.h>
#include <string>

extern bool g_textguiEnabled;
extern float g_textguiX;
extern float g_textguiY;
extern float g_textguiScale;
extern float g_textguiOpacity;
extern float g_textguiLineSpacing;
extern float g_textguiShadowStrength;
extern float g_textguiRainbowSpeed;
extern float g_textguiRainbowSpread;
extern float g_textguiRainbowSaturation;
extern float g_textguiRainbowBrightness;
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
void RegisterCustomLine(const std::wstring& id, const std::wstring& text);
void RemoveCustomLine(const std::wstring& id);
void UpdateCrosshairRecoilSignal(const std::wstring& text);
}
