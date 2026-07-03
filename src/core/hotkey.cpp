#include "hotkey.h"
#include "StrikeSense.h"
#include <iostream>

extern int g_hotkeyMod;
extern int g_hotkeyVk;

extern bool g_itemHelperEnabled;
extern int g_itemHelperHotkeyMod;
extern int g_itemHelperHotkeyVk;

namespace Hotkey
{
    std::wstring HotkeyToString(
        int mod,
        int vk)
    {
        std::wstring keyName;

        if (vk == 0)
            return L"None";

        if (mod & MOD_CONTROL)
            keyName += L"Ctrl+";

        if (mod & MOD_ALT)
            keyName += L"Alt+";

        if (mod & MOD_SHIFT)
            keyName += L"Shift+";

        switch (vk)
        {
        case VK_HOME:   keyName += L"Home"; break;
        case VK_END:    keyName += L"End"; break;
        case VK_INSERT: keyName += L"Insert"; break;
        case VK_DELETE: keyName += L"Delete"; break;

        case VK_F1:  keyName += L"F1"; break;
        case VK_F2:  keyName += L"F2"; break;
        case VK_F3:  keyName += L"F3"; break;
        case VK_F4:  keyName += L"F4"; break;
        case VK_F5:  keyName += L"F5"; break;
        case VK_F6:  keyName += L"F6"; break;
        case VK_F7:  keyName += L"F7"; break;
        case VK_F8:  keyName += L"F8"; break;
        case VK_F9:  keyName += L"F9"; break;
        case VK_F10: keyName += L"F10"; break;
        case VK_F11: keyName += L"F11"; break;
        case VK_F12: keyName += L"F12"; break;

        default:
        {
            if (vk >= 'A' && vk <= 'Z')
                keyName += (wchar_t)vk;
            else if (vk >= '0' && vk <= '9')
                keyName += (wchar_t)vk;
            else
            {
                wchar_t b[16];
                swprintf_s(b, L"Vk=%d", vk);
                keyName += b;
            }
        }
        }

        return keyName;
    }

    void Capture(
        WPARAM wp,
        int& outMod,
        int& outVk)
    {
        outVk = (int)wp;
        outMod = 0;

        if (GetKeyState(VK_CONTROL) & 0x8000)
            outMod |= MOD_CONTROL;

        if (GetKeyState(VK_SHIFT) & 0x8000)
            outMod |= MOD_SHIFT;

        if (GetKeyState(VK_MENU) & 0x8000)
            outMod |= MOD_ALT;
    }

    void UpdateGlobalHotkey(HWND hw)
    {
        UnregisterHotKey(hw, 1001);

        if (g_hotkeyVk == 0)
            return;

        if (!RegisterHotKey(
            hw,
            1001,
            g_hotkeyMod,
            g_hotkeyVk))
        {
            std::cout
                << "[热键注册失败] err="
                << GetLastError()
                << std::endl;
        }
        else
        {
            std::cout
                << "[热键注册成功] mod="
                << g_hotkeyMod
                << " vk="
                << g_hotkeyVk
                << std::endl;
        }
    }

    void UpdateItemHelperHotkey(HWND hw)
    {
        UnregisterHotKey(hw, 1002);

        if (!g_itemHelperEnabled)
            return;

        if (g_itemHelperHotkeyVk == 0)
            return;

        if (!RegisterHotKey(
            hw,
            1002,
            g_itemHelperHotkeyMod,
            g_itemHelperHotkeyVk))
        {
            std::cout
                << "[道具助手] 热键注册失败 "
                << GetLastError()
                << std::endl;
        }
        else
        {
            std::cout
                << "[道具助手] 热键注册成功"
                << std::endl;
        }
    }
}