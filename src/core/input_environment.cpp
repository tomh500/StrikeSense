#include "input_environment.h"

#include <Windows.h>

#include <atomic>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <string_view>
#include <thread>

namespace inputenvironment {
namespace {

std::atomic<bool> g_enabled{ false };
std::atomic<bool> g_input_active{ false };
HHOOK g_keyboard_hook = nullptr;
std::thread g_hook_thread;
std::mutex g_hook_lifecycle_mutex;
std::mutex g_hook_ready_mutex;
std::condition_variable g_hook_ready_cv;
DWORD g_hook_thread_id = 0;
bool g_hook_ready = false;
bool g_hook_installed = false;

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

void HookThreadMain()
{
    g_hook_thread_id = GetCurrentThreadId();
    MSG message{};
    PeekMessageW(&message, nullptr, WM_USER, WM_USER, PM_NOREMOVE);
    g_keyboard_hook = SetWindowsHookExW(WH_KEYBOARD_LL, KeyboardProc, GetModuleHandleW(nullptr), 0);
    {
        std::lock_guard ready_lock(g_hook_ready_mutex);
        g_hook_installed = g_keyboard_hook != nullptr;
        g_hook_ready = true;
    }
    g_hook_ready_cv.notify_one();

    if (g_keyboard_hook) {
        while (GetMessageW(&message, nullptr, 0, 0) > 0) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        UnhookWindowsHookEx(g_keyboard_hook);
        g_keyboard_hook = nullptr;
    }
    g_hook_thread_id = 0;
}

void InstallHook()
{
    std::lock_guard lifecycle_lock(g_hook_lifecycle_mutex);
    if (g_keyboard_hook) return;
    if (g_hook_thread.joinable()) g_hook_thread.join();
    {
        std::lock_guard ready_lock(g_hook_ready_mutex);
        g_hook_ready = false;
        g_hook_installed = false;
    }
    g_hook_thread = std::thread(HookThreadMain);
    {
        std::unique_lock ready_lock(g_hook_ready_mutex);
        g_hook_ready_cv.wait(ready_lock, [] { return g_hook_ready; });
    }
    if (g_hook_installed)
        std::cout << "[输入环境] 键盘状态钩子已安装到独立输入线程。" << std::endl;
    else {
        if (g_hook_thread.joinable()) g_hook_thread.join();
        std::cout << "[输入环境] 键盘状态钩子安装失败。" << std::endl;
    }
}

void UninstallHook()
{
    std::lock_guard lifecycle_lock(g_hook_lifecycle_mutex);
    if (g_hook_thread_id != 0) PostThreadMessageW(g_hook_thread_id, WM_QUIT, 0, 0);
    if (g_hook_thread.joinable()) g_hook_thread.join();
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
