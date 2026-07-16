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
extern int g_textguiAccessoryR;
extern int g_textguiAccessoryG;
extern int g_textguiAccessoryB;
extern bool g_textguiShowWatermark;
extern bool g_textguiRainbow;
extern bool g_textguiBackdrop;
extern float g_textguiBackdropOpacity;
extern std::wstring g_textguiCustomSlogan;

namespace textgui_overlay {
void Initialize(HINSTANCE hInst);
void ApplyEnabled(bool enabled);
void Refresh();
void Shutdown();
void RegisterCustomLine(const std::wstring& id, const std::wstring& text,
    const std::wstring& accessory = L"");
void RemoveCustomLine(const std::wstring& id);
void SetModuleHidden(const std::wstring& id, bool hidden);
void UpdateCrosshairRecoilSignal(const std::wstring& text);
}
