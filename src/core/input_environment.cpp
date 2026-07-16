#include "input_environment.h"

#include <Windows.h>

#include <array>
#include <atomic>
#include <chrono>
#include <iostream>
#include <iterator>
#include <string_view>
#include <thread>
#include <vector>

namespace inputenvironment {
namespace {

std::atomic<bool> g_enabled{ false };
std::atomic<bool> g_input_active{ false };
std::atomic<bool> g_initializing{ false };
std::atomic<bool> g_initializer_running{ false };
std::thread g_initializer_thread;
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
        || window_title.find(L"反恐精英：全球攻势") != std::wstring_view::npos;
}

void AppendVirtualKey(std::vector<INPUT>& inputs, WORD virtual_key)
{
    INPUT down{};
    down.type = INPUT_KEYBOARD;
    down.ki.wVk = virtual_key;
    down.ki.wScan = static_cast<WORD>(MapVirtualKeyW(virtual_key, MAPVK_VK_TO_VSC));
    down.ki.dwFlags = KEYEVENTF_SCANCODE;
    inputs.push_back(down);

    INPUT up = down;
    up.ki.dwFlags |= KEYEVENTF_KEYUP;
    inputs.push_back(up);
}

void AppendCtrlA(std::vector<INPUT>& inputs)
{
    INPUT ctrl_down{};
    ctrl_down.type = INPUT_KEYBOARD;
    ctrl_down.ki.wVk = VK_CONTROL;
    ctrl_down.ki.wScan = static_cast<WORD>(MapVirtualKeyW(VK_CONTROL, MAPVK_VK_TO_VSC));
    ctrl_down.ki.dwFlags = KEYEVENTF_SCANCODE;
    inputs.push_back(ctrl_down);

    AppendVirtualKey(inputs, 'A');

    INPUT ctrl_up = ctrl_down;
    ctrl_up.ki.dwFlags |= KEYEVENTF_KEYUP;
    inputs.push_back(ctrl_up);
}

void SendHideConsoleSequence()
{
    constexpr std::string_view command = "hideconsole";
    std::vector<INPUT> inputs;
    inputs.reserve(command.size() * 2 + 8);
    AppendCtrlA(inputs);
    AppendVirtualKey(inputs, VK_BACK);
    for (const char ch : command) {
        const SHORT mapped = VkKeyScanA(ch);
        if (mapped == -1) continue;
        AppendVirtualKey(inputs, static_cast<WORD>(LOBYTE(mapped)));
    }
    AppendVirtualKey(inputs, VK_RETURN);

    const UINT sent = SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
    g_input_active.store(false);
    g_initializing.store(false);
    if (sent == inputs.size())
        std::cout << "[输入环境] 已发送 Ctrl+A、回删、hideconsole、回车，恢复自动输入队列。" << std::endl;
    else
        std::cout << "[输入环境] hideconsole 初始化输入未完整发送，错误码=" << GetLastError() << std::endl;
}

void InitializerLoop()
{
    std::cout << "[输入环境] 等待 CS2 切回前台后执行输入状态初始化。" << std::endl;
    while (g_initializer_running.load() && g_enabled.load()) {
        if (IsStrictCs2Foreground()) {
            SendHideConsoleSequence();
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    g_initializer_running.store(false);
    if (!g_enabled.load()) g_initializing.store(false);
}

void StartInitializer()
{
    if (g_initializer_running.exchange(true)) return;
    if (g_initializer_thread.joinable()) g_initializer_thread.join();
    g_initializer_thread = std::thread(InitializerLoop);
}

void StopInitializer()
{
    g_initializer_running.store(false);
    if (g_initializer_thread.joinable()) g_initializer_thread.join();
}

LRESULT CALLBACK KeyboardProc(int code, WPARAM w_param, LPARAM l_param)
{
    if (code == HC_ACTION && g_enabled.load()) {
        const auto* key = reinterpret_cast<const KBDLLHOOKSTRUCT*>(l_param);
        const bool key_down = w_param == WM_KEYDOWN || w_param == WM_SYSKEYDOWN;
        if (key && key_down && (key->flags & LLKHF_INJECTED) == 0 && IsStrictCs2Foreground()) {
            if (key->vkCode == 'T' || key->vkCode == 'Y') {
                g_input_active.store(true);
                std::cout << "[输入环境] 检测到聊天键，状态切换为输入中。" << std::endl;
            } else if (key->vkCode == VK_OEM_3) {
                const bool shift_pressed = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
                g_input_active.store(shift_pressed);
                std::cout << "[输入环境] 检测到控制台键，状态切换为: "
                          << (shift_pressed ? "输入中" : "非输入") << std::endl;
            } else if (key->vkCode == VK_RETURN) {
                g_input_active.store(false);
                std::cout << "[输入环境] 检测到回车，状态切换为非输入。" << std::endl;
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
        std::cout << "[输入环境] 键盘状态钩子安装失败，错误码=" << GetLastError() << std::endl;
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
            g_initializing.store(true);
            InstallHook();
            StartInitializer();
        }
        return;
    }

    if (enabled) {
        g_input_active.store(false);
        g_initializing.store(true);
        InstallHook();
        StartInitializer();
    } else {
        StopInitializer();
        UninstallHook();
        g_input_active.store(false);
        g_initializing.store(false);
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
    return g_enabled.load() && (g_input_active.load() || g_initializing.load());
}

bool ShouldSuppressScriptKey(unsigned int virtual_key)
{
    if (!g_enabled.load()) return false;
    return g_input_active.load() || g_initializing.load()
        || virtual_key == 'T' || virtual_key == 'Y'
        || virtual_key == VK_OEM_3 || virtual_key == VK_RETURN;
}

void ForceNonInputState()
{
    StopInitializer();
    g_initializing.store(false);
    g_input_active.store(false);
    std::cout << "[输入环境] 已强制重置为非输入状态。" << std::endl;
}

void Shutdown()
{
    g_enabled.store(false);
    StopInitializer();
    UninstallHook();
    g_input_active.store(false);
    g_initializing.store(false);
}

} // namespace inputenvironment
