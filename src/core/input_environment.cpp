#include "input_environment.h"

#include <Windows.h>

#include <atomic>
#include <iostream>
#include <string_view>

namespace inputenvironment {
namespace {

std::atomic<bool> g_enabled{ false };
std::atomic<bool> g_input_active{ false };
HHOOK g_keyboard_hook = nullptr;

bool IsStrictCs2Foreground()
{
    const HWND foreground = GetForegroundWindow();
    if (!foreground) return false;

    DWORD process_id = 0;
    GetWindowThreadProcessId(foreground, &process_id);
    if (process_id == GetCurrentProcessId()) return false;

    wchar_t title[256]{};
    GetWindowTextW(foreground, title, static_cast<int>(std::size(title)));
    const std::wstring_view window_title(title);
    return window_title.find(L"Counter-Strike 2") != std::wstring_view::npos
        || window_title.find(L"反恐精英") != std::wstring_view::npos;
}

LRESULT CALLBACK KeyboardProc(int code, WPARAM w_param, LPARAM l_param)
{
    if (code == HC_ACTION && g_enabled.load()) {
        const auto* key = reinterpret_cast<const KBDLLHOOKSTRUCT*>(l_param);
        const bool key_down = w_param == WM_KEYDOWN || w_param == WM_SYSKEYDOWN;
        if (key && key_down && (key->flags & LLKHF_INJECTED) == 0 && IsStrictCs2Foreground()) {
            if (key->vkCode == 'Y' || key->vkCode == 'U' || key->vkCode == VK_OEM_3) {
                g_input_active.store(true);
                std::cout << "[输入环境] 检测到输入入口按键，状态切换为输入中。" << std::endl;
            } else if (key->vkCode == VK_RETURN || key->vkCode == VK_ESCAPE) {
                g_input_active.store(false);
                std::cout << "[输入环境] 检测到退出输入按键，状态切换为非输入。" << std::endl;
            }
        }
    }
    return CallNextHookEx(nullptr, code, w_param, l_param);
}

void InstallHook()
{
    if (g_keyboard_hook) return;
    g_keyboard_hook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardProc, GetModuleHandleW(nullptr), 0);
    if (g_keyboard_hook)
        std::cout << "[输入环境] 键盘状态钩子已安装。" << std::endl;
    else
        std::cout << "[输入环境] 键盘状态钩子安装失败，错误码: " << GetLastError() << std::endl;
}

void UninstallHook()
{
    if (!g_keyboard_hook) return;
    UnhookWindowsHookEx(g_keyboard_hook);
    g_keyboard_hook = nullptr;
    std::cout << "[输入环境] 键盘状态钩子已卸载。" << std::endl;
}

} // namespace

void SetEnabled(bool enabled)
{
    if (g_enabled.exchange(enabled) == enabled) {
        if (enabled) {
            g_input_active.store(false);
            InstallHook();
        }
        return;
    }

    if (enabled) {
        g_input_active.store(false);
        InstallHook();
        std::cout << "[输入环境] 已开启输入环境拦截，当前直接记录为非输入状态。" << std::endl;
    } else {
        UninstallHook();
        g_input_active.store(false);
    }
    std::cout << "[输入环境] 尝试为输入环境拦截已切换为: "
              << (enabled ? "开启" : "关闭") << std::endl;
}

bool IsEnabled()
{
    return g_enabled.load();
}

bool IsInputActive()
{
    return g_input_active.load();
}

bool ShouldPauseAutomation()
{
    return g_enabled.load() && g_input_active.load();
}

bool ShouldSuppressScriptKey(unsigned int virtual_key)
{
    if (!g_enabled.load()) return false;
    return g_input_active.load()
        || virtual_key == 'Y' || virtual_key == 'U'
        || virtual_key == VK_OEM_3
        || virtual_key == VK_RETURN
        || virtual_key == VK_ESCAPE;
}

void ForceNonInputState()
{
    g_input_active.store(false);
    std::cout << "[输入环境] 已强制重置为非输入状态。" << std::endl;
}

void Shutdown()
{
    g_enabled.store(false);
    UninstallHook();
    g_input_active.store(false);
}

} // namespace inputenvironment
