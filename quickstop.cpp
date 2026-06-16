#include "quickstop.h"
#include "config.h"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

// CS2 窗口标题匹配
static bool IsCS2Foreground()
{
    HWND fg = GetForegroundWindow();
    if (!fg) return false;
    wchar_t title[256];
    GetWindowTextW(fg, title, 256);
    std::wstring wt(title);
    return (wt.find(L"Counter-Strike 2") != std::string::npos ||
            wt.find(L"反恐精英：全球攻势") != std::string::npos);
}

namespace fs = std::filesystem;

static QuickStopConfig s_qsConfig;

static std::wstring GetQuickStopPath()
{
    return config::GetConfigDir() + L"\\quickstop.json";
}

QuickStopConfig& GetQSConfig()
{
    return s_qsConfig;
}

void LoadQuickStopConfig()
{
    fs::path path(GetQuickStopPath());
    if (!fs::exists(path))
    {
        std::cout << "[急停] quickstop.json 不存在，使用默认值。" << std::endl;
        SaveQuickStopConfig();
        return;
    }

    try {
        std::ifstream in(path);
        if (!in.is_open()) return;

        nlohmann::json j;
        in >> j;
        in.close();

        auto gb = [&](const char* k, bool& v) { if (j.contains(k) && j[k].is_boolean()) v = j[k]; };
        auto gi = [&](const char* k, int& v) { if (j.contains(k) && j[k].is_number()) v = j[k].get<int>(); };

        gb("enabled", s_qsConfig.enabled);
        gi("min_pulse", s_qsConfig.min_pulse);
        gi("max_pulse", s_qsConfig.max_pulse);
        gi("cap_pulse", s_qsConfig.cap_pulse);
        gi("move_start_at", s_qsConfig.move_start_at);
        gi("move_cap_at", s_qsConfig.move_cap_at);

        std::cout << "[急停] quickstop.json 加载成功。" << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "[急停] 加载失败: " << e.what() << std::endl;
    }
}

void SaveQuickStopConfig()
{
    auto& s = s_qsConfig;
    config::EnsureDirectoriesExist();

    try {
        nlohmann::json j;
        j["enabled"] = s.enabled;
        j["min_pulse"] = s.min_pulse;
        j["max_pulse"] = s.max_pulse;
        j["cap_pulse"] = s.cap_pulse;
        j["move_start_at"] = s.move_start_at;
        j["move_cap_at"] = s.move_cap_at;

        std::ofstream out(GetQuickStopPath());
        if (out.is_open())
        {
            out << j.dump(2);
            out.close();
            std::cout << "[急停] quickstop.json 已保存。" << std::endl;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "[急停] 保存失败: " << e.what() << std::endl;
    }
}

// ===== 急停核心状态（完全复刻 CS2MouseHook/KICore） =====
namespace {
    std::atomic<bool> ws_blocking{ false }, ad_blocking{ false };
    std::atomic<bool> ignore_w{ false }, ignore_s{ false }, ignore_a{ false }, ignore_d{ false };
    bool is_holding_gun = true;
    std::map<char, std::chrono::steady_clock::time_point> start_times;
    bool jiting = false;
    std::atomic<bool> pause_jiting{ false };

    bool IsSilentKeyPressed()
    {
        for (int vk : s_qsConfig.silent_keys)
        {
            if (GetAsyncKeyState(vk) & 0x8000) return true;
        }
        return false;
    }

    void SendHardwareKey(WORD vKey, bool down)
    {
        INPUT input = { 0 };
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = vKey;
        input.ki.wScan = (WORD)MapVirtualKeyW(vKey, MAPVK_VK_TO_VSC);
        input.ki.dwFlags = (down ? 0 : KEYEVENTF_KEYUP) | KEYEVENTF_SCANCODE;
        SendInput(1, &input, sizeof(INPUT));
    }

    void PerformDynamicStop(WORD vKey, int move_duration_ms, std::atomic<bool>* blocking_flag)
    {
        if (GetAsyncKeyState(vKey) & 0x8000)
        {
            blocking_flag->store(false);
            return;
        }

        // 线性插值计算脉冲
        const auto& cfg = s_qsConfig;
        int stop_pulse = cfg.min_pulse;
        if (move_duration_ms > cfg.move_start_at)
        {
            stop_pulse = cfg.min_pulse + (move_duration_ms - cfg.move_start_at) * (cfg.max_pulse - cfg.min_pulse) / (cfg.move_cap_at - cfg.move_start_at);
        }
        if (stop_pulse > cfg.cap_pulse) stop_pulse = cfg.cap_pulse;

        // 执行模拟按键
        SendHardwareKey(vKey, true);
        std::this_thread::sleep_for(std::chrono::milliseconds(stop_pulse));

        // 设置忽略旗标，防止反向按键触发自己的 start 事件
        if (vKey == 'W') ignore_w = true;
        else if (vKey == 'S') ignore_s = true;
        else if (vKey == 'A') ignore_a = true;
        else if (vKey == 'D') ignore_d = true;

        SendHardwareKey(vKey, false);

        // 阻塞保护期：防止连点抽风
        std::this_thread::sleep_for(std::chrono::milliseconds(60));
        blocking_flag->store(false);
    }

    void UpdateWeaponState(const std::string& json)
    {
        if (json.find("\"weapons\"") == std::string::npos) return;

        size_t activePos = json.find("\"state\": \"active\"");
        if (activePos == std::string::npos) return;

        size_t searchStart = (activePos > 150) ? activePos - 150 : 0;
        std::string weaponZone = json.substr(searchStart, 200);

        bool active_is_knife = (weaponZone.find("\"type\": \"Knife\"") != std::string::npos);
        bool active_is_nade = (weaponZone.find("\"type\": \"Grenade\"") != std::string::npos);
        bool active_is_c4 = (weaponZone.find("\"type\": \"C4\"") != std::string::npos);

        is_holding_gun = !(active_is_knife || active_is_nade || active_is_c4);
    }
}

// ===== 暂停控制 =====
void SetQuickStopPause(bool paused) { pause_jiting = paused; }
bool IsQuickStopPaused() { return pause_jiting; }

// ===== 全局键盘钩子 =====
static HHOOK g_quickStopHook = nullptr;
static std::atomic<bool> g_hookRunning{ false };

static LRESULT CALLBACK QuickStopLowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION)
    {
        // 只在 CS2 前台时才处理急停
        if (!IsCS2Foreground()) return CallNextHookEx(nullptr, nCode, wParam, lParam);

        auto* kb = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        if (kb)
        {
            WORD vk = (WORD)kb->vkCode;
            char keyChar = 0;
            if (vk == 'W') keyChar = 'W';
            else if (vk == 'A') keyChar = 'A';
            else if (vk == 'S') keyChar = 'S';
            else if (vk == 'D') keyChar = 'D';

            if (keyChar)
            {
                if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)
                {
                    ProcessQuickStopCommand(std::string("quickstart_") + keyChar);
                }
                else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP)
                {
                    ProcessQuickStopCommand(std::string("quickstop_") + keyChar);
                }
            }
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

void StartQuickStopHook()
{
    if (g_hookRunning) return;
    g_hookRunning = true;
    g_quickStopHook = SetWindowsHookExW(WH_KEYBOARD_LL, QuickStopLowLevelKeyboardProc,
        GetModuleHandleW(nullptr), 0);
    if (g_quickStopHook)
        std::cout << "[急停] 键盘钩子已安装" << std::endl;
    else
        std::cout << "[急停] 键盘钩子安装失败" << std::endl;
}

void StopQuickStopHook()
{
    if (g_quickStopHook)
    {
        UnhookWindowsHookEx(g_quickStopHook);
        g_quickStopHook = nullptr;
    }
    g_hookRunning = false;
    std::cout << "[急停] 键盘钩子已卸载" << std::endl;
}

// ===== 核心处理 =====
void ProcessQuickStopCommand(const std::string& cmd)
{
    // A. 如果是 GSI JSON 数据
    if (cmd.find('{') != std::string::npos)
    {
        UpdateWeaponState(cmd);
        return;
    }

    // B. 处理移动起点的计时
    if (cmd.find("quickstart_") != std::string::npos)
    {
        char key = toupper(cmd.back());
        start_times[key] = std::chrono::steady_clock::now();
        return;
    }

    // C. 执行急停
    if (cmd.find("quickstop_") != std::string::npos)
    {
        if (pause_jiting) return;           // pause键控制的全局急停开关
        if (!s_qsConfig.enabled) return;    // 总开关
        if (IsSilentKeyPressed()) return;   // Shift/Ctrl 拦截
        if (!is_holding_gun) return;        // 刀/雷 拦截

        char key = toupper(cmd.back());
        WORD counterKey = 0;
        std::atomic<bool>* flag = nullptr;

        if (key == 'W') { if (ignore_w.exchange(false)) return; counterKey = 'S'; flag = &ws_blocking; }
        else if (key == 'S') { if (ignore_s.exchange(false)) return; counterKey = 'W'; flag = &ws_blocking; }
        else if (key == 'A') { if (ignore_a.exchange(false)) return; counterKey = 'D'; flag = &ad_blocking; }
        else if (key == 'D') { if (ignore_d.exchange(false)) return; counterKey = 'A'; flag = &ad_blocking; }

        if (counterKey && flag && !flag->exchange(true))
        {
            auto now = std::chrono::steady_clock::now();
            int duration = (int)std::chrono::duration_cast<std::chrono::milliseconds>(now - start_times[key]).count();
            std::thread(PerformDynamicStop, counterKey, duration, flag).detach();
        }
    }
}