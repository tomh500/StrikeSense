#pragma once

#include <Windows.h>
#include <gdiplus.h>

namespace cscriptui {
int PaintSection(Gdiplus::Graphics& graphics, int contentX, int contentWidth, int topY, HWND owner);
bool CheckClick(HWND owner, int mouseX, int mouseY);
bool ProcessBindingKey(HWND owner, WPARAM wParam, LPARAM lParam);
bool IsExpanded();
void SetExpanded(bool expanded);
} // namespace cscriptui
