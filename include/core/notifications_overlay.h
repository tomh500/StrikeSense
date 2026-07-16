#pragma once

#include <windows.h>
#include <string>

extern bool g_notificationsEnabled;
extern float g_notificationsDuration;
extern int g_notificationsStyle;

namespace notifications_overlay {
void Initialize(HINSTANCE hInst);
void Shutdown();
void Push(const std::wstring& text, bool enabled);
void Show(const std::wstring& text, bool enabled);
void Refresh();
}
