#include "quickstop.h"
#include "config.h"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

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

// ===== 全局键盘钩子 =====
static HHOOK g_quickStopHook = nullptr;
static std::atomic<bool> g_hookRunning{ false };

static LRESULT CALLBACK QuickStopLowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION)
    {
        auto* kb = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        if (kb)
        {
            WORD vk = (WORD)kb->vkCode;
            // W/A/S/D 按键
            const wchar_t* moveKey = nullptr;
            char keyChar = 0;
            if (vk == 'W') { moveKey = L"W"; keyChar = 'W'; }
            else if (vk == 'A') { moveKey = L"A"; keyChar = 'A'; }
            else if (vk == 'S') { moveKey = L"S"; keyChar = 'S'; }
            else if (vk == 'D') { moveKey = L"D"; keyChar = 'D'; }

            if (moveKey)
            {
                if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)
                {
                    ProcessQuickStop(std::string("quickstart_") + keyChar);
                }
                else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP)
                {
                    ProcessQuickStop(std::string("quickstop_") + keyChar);
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

// ===== 急停逻辑（复刻 CS2MouseHook） =====

// 记录上一次按下的移动方向键时间
static std::chrono::steady_clock::time_point g_lastKeyPressTime;
static std::string g_lastMoveKey; // "W","A","S","D" 或空

static void SendHardwareKey(WORD vk, bool down)
{
    INPUT inp = {};
    inp.type = INPUT_KEYBOARD;
    inp.ki.wVk = vk;
    inp.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
    inp.ki.dwExtraInfo = 0xB00B1E5; // OpenDear 签名
    SendInput(1, &inp, sizeof(INPUT));
}

static int CalcPulseDuration(int holdMs, const QuickStopConfig& cfg)
{
    // 线性插值: holdMs 在 [move_start_at, move_cap_at] 映射到 [min_pulse, max_pulse]
    // 超过 cap 的使用 cap_pulse
    if (holdMs >= cfg.move_cap_at)
        return cfg.cap_pulse;

    if (holdMs <= cfg.move_start_at)
        return cfg.min_pulse;

    float t = (float)(holdMs - cfg.move_start_at) / (float)(cfg.move_cap_at - cfg.move_start_at);
    if (t < 0.f) t = 0.f;
    if (t > 1.f) t = 1.f;
    int pulse = cfg.min_pulse + (int)((cfg.max_pulse - cfg.min_pulse) * t);
    return pulse;
}

void HandleQuickStopGSI(const std::string& rawJson)
{
    // 未启用则忽略
    if (!s_qsConfig.enabled) return;

    // GSI 有携带 player state，但我们暂不实现完整的 GSI 解析，
    // 这里留作扩展：当 GSI 中武器状态变化时，可以触发急停重置
    // 暂时只做日志
    (void)rawJson;
}

void ProcessQuickStop(const std::string& key)
{
    if (!s_qsConfig.enabled) return;

    // 过滤：如果按下的是 Shift/Ctrl 等静默键，不做急停
    for (int vk : s_qsConfig.silent_keys)
    {
        // key 格式如 "quickstop_W", "quickstart_W"
        if (key.find("W") != std::string::npos)
        {
            if (GetAsyncKeyState(vk) & 0x8000)
            {
                std::cout << "[急停] 静默键按下，跳过急停" << std::endl;
                return;
            }
        }
    }

    if (key.find("quickstop_") == 0)
    {
        // 按键抬起 - 执行急停
        char moveKey = key.back(); // W/A/S/D
        std::string oppositeKey;
        switch (moveKey)
        {
        case 'W': oppositeKey = "S"; break;
        case 'S': oppositeKey = "W"; break;
        case 'A': oppositeKey = "D"; break;
        case 'D': oppositeKey = "A"; break;
        default: return;
        }

        auto now = std::chrono::steady_clock::now();
        int holdMs = (int)std::chrono::duration_cast<std::chrono::milliseconds>(now - g_lastKeyPressTime).count();

        // 只有按住时间 < 5000ms 才触发（防止按住太久后松手误触发）
        if (holdMs > 0 && holdMs < 5000)
        {
            int pulse = CalcPulseDuration(holdMs, s_qsConfig);
            std::cout << "[急停] " << moveKey << " 松手, 按住 " << holdMs << "ms, 反向脉冲 " << oppositeKey << " " << pulse << "ms" << std::endl;

            // 发送反向键
            WORD vk = 0;
            if (oppositeKey == "W") vk = 'W';
            else if (oppositeKey == "S") vk = 'S';
            else if (oppositeKey == "A") vk = 'A';
            else if (oppositeKey == "D") vk = 'D';

            if (vk)
            {
                SendHardwareKey(vk, true);   // 按下
                std::this_thread::sleep_for(std::chrono::milliseconds(pulse));
                SendHardwareKey(vk, false);  // 抬起
            }
        }

        g_lastMoveKey.clear();
    }
    else if (key.find("quickstart_") == 0)
    {
        // 按键按下 - 记录时间
        g_lastKeyPressTime = std::chrono::steady_clock::now();
        g_lastMoveKey = key.substr(key.find_last_of('_') + 1);
    }
}