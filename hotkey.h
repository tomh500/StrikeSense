#pragma once

#include <Windows.h>
#include <string>

namespace Hotkey
{
    std::wstring HotkeyToString(
        int mod,
        int vk);

    void UpdateGlobalHotkey(HWND hw);

    void UpdateItemHelperHotkey(HWND hw);

    void Capture(WPARAM wp,int& outMod,int& outVk);
}