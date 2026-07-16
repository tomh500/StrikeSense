#include "quickstop.h"
#include "config.h"
#include "pages.h"
#include "input_environment.h"
#include "volume_mixer.h"
#include <algorithm>
#include <array>
#include <condition_variable>
#include <cstdint>
#include <cmath>
#include <fstream>
#include <iostream>
#include <mutex>
#include <nlohmann/json.hpp>




namespace fs = std::filesystem;

static QuickStopConfig s_qsConfig;
static HHOOK g_quickStopHook = nullptr;
static HHOOK g_quickStopMouseHook = nullptr;
static std::atomic<bool> g_hookRunning{ false };
static std::thread g_hookThread;
static std::mutex g_hookLifecycleMutex;
static std::mutex g_hookReadyMutex;
static std::condition_variable g_hookReadyCv;
static DWORD g_hookThreadId = 0;
static bool g_hookReady = false;

static std::wstring GetQuickStopPath()
{
    return config::GetConfigDir() + L"\\quickstop.json";
}

QuickStopConfig& GetQSConfig()
{
    return s_qsConfig;
}

bool IsQuickStopEnabled()
{
    return s_qsConfig.enabled;
}

void SetQuickStopEnabled(bool enabled)
{
    if (enabled && !IsRageModeEnabled())
    {
        std::cout << "[急停] 超频配置未开启，拒绝打开自动急停。" << std::endl;
        enabled = false;
    }

    if (s_qsConfig.enabled == enabled)
    {
        if (enabled && !g_hookRunning.load())
        {
            std::cout << "[急停] 当前配置为开启，但钩子未运行，准备自动补启动。" << std::endl;
            StartQuickStopHook();
        }
        return;
    }

    s_qsConfig.enabled = enabled;
    std::cout << "[急停] 自动急停开关已切换为: " << (enabled ? "开启" : "关闭") << std::endl;

    if (enabled)
        StartQuickStopHook();
    else
        StopQuickStopHook();

    SaveQuickStopConfig();
}

void StopQuickStopForRageDisabled()
{
    StopQuickStopHook();
    std::cout << "[急停] 超频配置关闭，已停止自动急停运行，但保留开关偏好。" << std::endl;
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
        gb("lenient_manual_stop", s_qsConfig.lenient_manual_stop);
        gi("jump_disable_ms", s_qsConfig.jump_disable_ms);
        gi("micro_pulse", s_qsConfig.micro_pulse);
        gi("min_pulse", s_qsConfig.min_pulse);
        gi("max_pulse", s_qsConfig.max_pulse);
        gi("cap_pulse", s_qsConfig.cap_pulse);
        gi("micro_move_at", s_qsConfig.micro_move_at);
        gi("move_start_at", s_qsConfig.move_start_at);
        gi("move_cap_at", s_qsConfig.move_cap_at);
        gi("curve_percent", s_qsConfig.curve_percent);
        gi("horizontal_scale_percent", s_qsConfig.horizontal_scale_percent);
        gi("vertical_scale_percent", s_qsConfig.vertical_scale_percent);

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
        j["lenient_manual_stop"] = s.lenient_manual_stop;
        j["jump_disable_ms"] = s.jump_disable_ms;
        j["micro_pulse"] = s.micro_pulse;
        j["min_pulse"] = s.min_pulse;
        j["max_pulse"] = s.max_pulse;
        j["cap_pulse"] = s.cap_pulse;
        j["micro_move_at"] = s.micro_move_at;
        j["move_start_at"] = s.move_start_at;
        j["move_cap_at"] = s.move_cap_at;
        j["curve_percent"] = s.curve_percent;
        j["horizontal_scale_percent"] = s.horizontal_scale_percent;
        j["vertical_scale_percent"] = s.vertical_scale_percent;

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

// ===== 急停核心状态 =====
namespace {
    using Clock = std::chrono::steady_clock;

    struct AxisControl {
        std::atomic<std::uint64_t> generation{ 0 };
        std::mutex request_mutex;
        std::condition_variable request_cv;
        bool request_pending = false;
        bool shutdown = false;
        WORD request_key = 0;
        int request_pulse = 0;
        std::uint64_t request_generation = 0;
        WORD lenient_counter_key = 0;
        Clock::time_point lenient_press_deadline{};
        int lenient_tap_max_ms = 0;
        std::thread worker;
    };

    constexpr std::array<char, 4> kMoveKeys{ 'W', 'A', 'S', 'D' };
    std::array<bool, kMoveKeys.size()> physical_key_down{};
    std::array<bool, kMoveKeys.size()> lenient_manual_stop_candidate{};
    std::array<int, kMoveKeys.size()> lenient_manual_stop_max_ms{};
    std::array<Clock::time_point, kMoveKeys.size()> lenient_manual_stop_times{};
    std::array<Clock::time_point, kMoveKeys.size()> start_times{};
    std::mutex movement_mutex;
    AxisControl ws_axis;
    AxisControl ad_axis;
    std::atomic<bool> is_holding_gun{ true };
    std::atomic<bool> pause_jiting{ false };
    std::atomic<bool> space_key_down{ false };
    std::atomic<std::int64_t> jump_disable_until_ms{ 0 };
    constexpr bool kVerboseQuickStopInputLog = false;

    int MoveKeyIndex(char key)
    {
        const auto it = std::find(kMoveKeys.begin(), kMoveKeys.end(), key);
        return it == kMoveKeys.end() ? -1 : static_cast<int>(std::distance(kMoveKeys.begin(), it));
    }

    AxisControl* GetAxis(char key)
    {
        if (key == 'W' || key == 'S') return &ws_axis;
        if (key == 'A' || key == 'D') return &ad_axis;
        return nullptr;
    }

    bool IsAxisKeyDownUnlocked(const AxisControl* axis)
    {
        if (axis == &ws_axis)
            return physical_key_down[MoveKeyIndex('W')] || physical_key_down[MoveKeyIndex('S')];
        return physical_key_down[MoveKeyIndex('A')] || physical_key_down[MoveKeyIndex('D')];
    }

    bool IsSilentKeyPressed()
    {
        for (int vk : s_qsConfig.silent_keys)
        {
            if (GetAsyncKeyState(vk) & 0x8000) return true;
        }
        return false;
    }

    std::int64_t NowMs()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            Clock::now().time_since_epoch()).count();
    }

    bool IsJumpQuickStopDisabled()
    {
        return NowMs() < jump_disable_until_ms.load();
    }

    void StopAllPulses();

    void RefreshJumpQuickStopDisable(const char* source)
    {
        const int disable_ms = std::clamp(s_qsConfig.jump_disable_ms, 0, 3000);
        if (disable_ms <= 0)
        {
            jump_disable_until_ms.store(0);
            if constexpr (kVerboseQuickStopInputLog) {
                std::cout << "[急停] 检测到跳跃输入(" << source
                          << ")，但跳跃临时禁用时长为 0ms，已跳过。" << std::endl;
            }
            return;
        }

        jump_disable_until_ms.store(NowMs() + disable_ms);
        StopAllPulses();
        if constexpr (kVerboseQuickStopInputLog) {
            std::cout << "[急停] 检测到跳跃输入(" << source
                      << ")，未来 3 秒临时禁用自动急停。" << std::endl;
        }
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

    int CalculateStopPulse(int move_duration_ms, WORD counter_key)
    {
        const auto& cfg = s_qsConfig;
        const int micro_pulse = (std::max)(1, cfg.micro_pulse);
        const int min_pulse = (std::max)(micro_pulse, cfg.min_pulse);
        const int max_pulse = (std::max)(min_pulse, cfg.max_pulse);
        const int cap_pulse = (std::max)(1, cfg.cap_pulse);
        const int micro_move_at = (std::max)(1, cfg.micro_move_at);
        const int move_start_at = (std::max)(micro_move_at, cfg.move_start_at);
        const int move_cap_at = (std::max)(move_start_at + 1, cfg.move_cap_at);

        double pulse = static_cast<double>(micro_pulse);
        if (move_duration_ms > micro_move_at && move_start_at > micro_move_at)
        {
            const double progress = static_cast<double>(std::clamp(
                move_duration_ms, micro_move_at, move_start_at) - micro_move_at) /
                static_cast<double>(move_start_at - micro_move_at);
            pulse = micro_pulse + progress * (min_pulse - micro_pulse);
        }
        if (move_duration_ms > move_start_at)
        {
            const double progress = static_cast<double>(std::clamp(
                move_duration_ms, move_start_at, move_cap_at) - move_start_at) /
                static_cast<double>((std::max)(1, move_cap_at - move_start_at));
            const double curve = (std::max)(1, cfg.curve_percent) / 100.0;
            pulse = min_pulse + std::pow(progress, curve) * (max_pulse - min_pulse);
        }

        const bool horizontal = counter_key == 'A' || counter_key == 'D';
        const int direction_scale = horizontal
            ? (std::max)(1, cfg.horizontal_scale_percent)
            : (std::max)(1, cfg.vertical_scale_percent);
        pulse *= direction_scale / 100.0;
        return std::clamp(static_cast<int>(std::lround(pulse)), 1, cap_pulse);
    }

    int CalculateLenientTapMaxMs(int stop_pulse)
    {
        return std::clamp(stop_pulse + 120, 150, 350);
    }

    void PerformDynamicStop(WORD vKey, int stop_pulse, AxisControl* axis, std::uint64_t generation)
    {
        if (inputenvironment::ShouldPauseAutomation()) {
            std::cout << "[急停] 当前处于输入环境，已暂停新的急停脉冲。" << std::endl;
            return;
        }
        {
            std::lock_guard movement_lock(movement_mutex);
            if (axis->generation.load() != generation || IsAxisKeyDownUnlocked(axis))
                return;
            SendHardwareKey(vKey, true);
        }

        const auto deadline = Clock::now() + std::chrono::milliseconds(stop_pulse);
        while (Clock::now() < deadline)
        {
            if (axis->generation.load() != generation)
                break;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        {
            std::lock_guard movement_lock(movement_mutex);
            SendHardwareKey(vKey, false);

            // 合成 KEYUP 可能覆盖玩家刚按下的同一个键，立即恢复真实按住状态。
            const int key_index = MoveKeyIndex(static_cast<char>(vKey));
            if (key_index >= 0 && physical_key_down[key_index])
                SendHardwareKey(vKey, true);
        }
    }

    void AxisWorkerLoop(AxisControl* axis)
    {
        while (true)
        {
            WORD vKey = 0;
            int stop_pulse = 0;
            std::uint64_t generation = 0;
            {
                std::unique_lock request_lock(axis->request_mutex);
                axis->request_cv.wait(request_lock, [axis] {
                    return axis->shutdown || axis->request_pending;
                });
                if (axis->shutdown) return;

                vKey = axis->request_key;
                stop_pulse = axis->request_pulse;
                generation = axis->request_generation;
                axis->request_pending = false;
            }

            PerformDynamicStop(vKey, stop_pulse, axis, generation);
        }
    }

    void StartAxisWorker(AxisControl& axis)
    {
        std::lock_guard request_lock(axis.request_mutex);
        if (axis.worker.joinable()) return;
        axis.shutdown = false;
        axis.request_pending = false;
        axis.worker = std::thread(AxisWorkerLoop, &axis);
    }

    void StopAxisWorker(AxisControl& axis)
    {
        axis.generation.fetch_add(1);
        {
            std::lock_guard request_lock(axis.request_mutex);
            axis.shutdown = true;
            axis.request_pending = false;
        }
        axis.request_cv.notify_one();
        if (axis.worker.joinable())
            axis.worker.join();
    }

    void StartAxisPulse(WORD vKey, int move_duration_ms, AxisControl* axis)
    {
        const int stop_pulse = CalculateStopPulse(move_duration_ms, vKey);
        const auto generation = axis->generation.fetch_add(1) + 1;

        {
            std::lock_guard movement_lock(movement_mutex);
            // 人工反向键通常会紧跟自动急停脉冲出现。记录一个短窗口，供键盘钩子
            // 判断这次输入是否是玩家手动补急停，而不是一次新的换向移动。
            const int press_window_ms = std::clamp(stop_pulse + 80, 120, 350);
            axis->lenient_counter_key = vKey;
            axis->lenient_press_deadline = Clock::now() + std::chrono::milliseconds(press_window_ms);
            axis->lenient_tap_max_ms = CalculateLenientTapMaxMs(stop_pulse);
        }

        {
            std::lock_guard request_lock(axis->request_mutex);
            axis->request_key = vKey;
            axis->request_pulse = stop_pulse;
            axis->request_generation = generation;
            axis->request_pending = true;
        }
        axis->request_cv.notify_one();

        if constexpr (kVerboseQuickStopInputLog) {
            std::cout << "[急停] 移动 " << move_duration_ms
                      << "ms，反向脉冲 " << stop_pulse << "ms" << std::endl;
        }
    }

    void CancelAxis(AxisControl& axis)
    {
        axis.generation.fetch_add(1);
    }

    void StopAllPulses()
    {
        {
            std::lock_guard movement_lock(movement_mutex);
            lenient_manual_stop_candidate.fill(false);
            lenient_manual_stop_max_ms.fill(0);
            lenient_manual_stop_times.fill(Clock::time_point{});
            for (AxisControl* axis : { &ws_axis, &ad_axis })
            {
                axis->lenient_counter_key = 0;
                axis->lenient_press_deadline = {};
                axis->lenient_tap_max_ms = 0;
            }
        }

        for (AxisControl* axis : { &ws_axis, &ad_axis })
        {
            CancelAxis(*axis);
            std::lock_guard request_lock(axis->request_mutex);
            axis->request_pending = false;
        }
    }

    void ResetPhysicalKeys()
    {
        std::lock_guard movement_lock(movement_mutex);
        physical_key_down.fill(false);
        space_key_down.store(false);
        lenient_manual_stop_candidate.fill(false);
        lenient_manual_stop_max_ms.fill(0);
        lenient_manual_stop_times.fill(Clock::time_point{});
        ws_axis.lenient_counter_key = 0;
        ws_axis.lenient_press_deadline = {};
        ws_axis.lenient_tap_max_ms = 0;
        ad_axis.lenient_counter_key = 0;
        ad_axis.lenient_press_deadline = {};
        ad_axis.lenient_tap_max_ms = 0;
        CancelAxis(ws_axis);
        CancelAxis(ad_axis);
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

        is_holding_gun.store(!(active_is_knife || active_is_nade || active_is_c4));
    }
}

// ===== 暂停控制 =====
void SetQuickStopPause(bool paused)
{
    pause_jiting = paused;
    if (paused) StopAllPulses();
}
bool IsQuickStopPaused() { return pause_jiting; }

bool IsQuickStopRuntimeActive()
{
    return IsRageModeEnabled()
        && s_qsConfig.enabled
        && !pause_jiting.load()
        && !IsSilentKeyPressed()
        && !IsJumpQuickStopDisabled()
        && is_holding_gun.load();
}

void ApplyQuickStopConfigChanges()
{
    SaveQuickStopConfig();
    StopAllPulses();
    if (s_qsConfig.enabled && !g_hookRunning.load())
        StartQuickStopHook();
    std::cout << "[急停] 参数已热应用，旧脉冲已取消，后续移动立即使用新配置。" << std::endl;
}

// ===== 全局键盘钩子 =====
static LRESULT CALLBACK QuickStopLowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION)
    {
        auto* kb = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        // SendInput 产生的按键不参与真实移动计时，也不会反过来触发急停。
        if (kb && !(kb->flags & LLKHF_INJECTED))
        {
            const WORD vk = static_cast<WORD>(kb->vkCode);
            if (vk != VK_SPACE && vk != 'W' && vk != 'A' && vk != 'S' && vk != 'D')
                return CallNextHookEx(nullptr, nCode, wParam, lParam);

            if (!IsCS2WindowActive())
            {
                ResetPhysicalKeys();
                return CallNextHookEx(nullptr, nCode, wParam, lParam);
            }

            if (vk == VK_SPACE)
            {
                if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)
                {
                    if (!space_key_down.exchange(true))
                        RefreshJumpQuickStopDisable("空格");
                }
                else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP)
                {
                    space_key_down.store(false);
                }
                return CallNextHookEx(nullptr, nCode, wParam, lParam);
            }

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

static LRESULT CALLBACK QuickStopLowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION && (wParam == WM_MOUSEWHEEL || wParam == WM_MOUSEHWHEEL))
    {
        auto* ms = reinterpret_cast<MSLLHOOKSTRUCT*>(lParam);
        if (ms && !(ms->flags & LLMHF_INJECTED))
        {
            if (!IsCS2WindowActive())
                return CallNextHookEx(nullptr, nCode, wParam, lParam);

            RefreshJumpQuickStopDisable("滚轮");
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

static void QuickStopHookThreadMain()
{
    g_hookThreadId = GetCurrentThreadId();
    MSG message{};
    PeekMessageW(&message, nullptr, WM_USER, WM_USER, PM_NOREMOVE);

    g_quickStopHook = SetWindowsHookExW(WH_KEYBOARD_LL, QuickStopLowLevelKeyboardProc,
        GetModuleHandleW(nullptr), 0);
    if (g_quickStopHook) {
        g_quickStopMouseHook = SetWindowsHookExW(WH_MOUSE_LL, QuickStopLowLevelMouseProc,
            GetModuleHandleW(nullptr), 0);
    }

    {
        std::lock_guard readyLock(g_hookReadyMutex);
        g_hookRunning.store(g_quickStopHook != nullptr);
        g_hookReady = true;
    }
    g_hookReadyCv.notify_one();

    if (g_hookRunning.load()) {
        while (GetMessageW(&message, nullptr, 0, 0) > 0) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }

    if (g_quickStopMouseHook) {
        UnhookWindowsHookEx(g_quickStopMouseHook);
        g_quickStopMouseHook = nullptr;
    }
    if (g_quickStopHook) {
        UnhookWindowsHookEx(g_quickStopHook);
        g_quickStopHook = nullptr;
    }
    g_hookRunning.store(false);
    g_hookThreadId = 0;
}

void StartQuickStopHook()
{
    std::lock_guard lifecycleLock(g_hookLifecycleMutex);
    if (g_hookRunning.load()) return;
    if (g_hookThread.joinable()) g_hookThread.join();
    {
        std::lock_guard readyLock(g_hookReadyMutex);
        g_hookReady = false;
    }
    g_hookThread = std::thread(QuickStopHookThreadMain);
    {
        std::unique_lock readyLock(g_hookReadyMutex);
        g_hookReadyCv.wait(readyLock, [] { return g_hookReady; });
    }

    if (g_hookRunning.load()) {
        StartAxisWorker(ws_axis);
        StartAxisWorker(ad_axis);
        if (g_quickStopMouseHook)
            std::cout << "[急停] 鼠标滚轮钩子已安装到独立输入线程。" << std::endl;
        else
            std::cout << "[急停] 鼠标滚轮钩子安装失败，错误码: " << GetLastError() << std::endl;
        std::cout << "[急停] 键盘钩子已安装到独立输入线程。" << std::endl;
    }
    else {
        if (g_hookThread.joinable()) g_hookThread.join();
        std::cout << "[急停] 键盘钩子安装失败，错误码: " << GetLastError() << std::endl;
    }
}

void StopQuickStopHook()
{
    std::lock_guard lifecycleLock(g_hookLifecycleMutex);
    if (g_hookThreadId != 0) PostThreadMessageW(g_hookThreadId, WM_QUIT, 0, 0);
    if (g_hookThread.joinable()) g_hookThread.join();
    ResetPhysicalKeys();
    StopAllPulses();
    StopAxisWorker(ws_axis);
    StopAxisWorker(ad_axis);
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
        const int key_index = MoveKeyIndex(key);
        AxisControl* axis = GetAxis(key);
        if (key_index < 0 || !axis) return;

        std::lock_guard movement_lock(movement_mutex);
        if (physical_key_down[key_index]) return; // 忽略系统长按产生的重复 KEYDOWN
        const auto now = Clock::now();
        const bool is_lenient_manual_stop = s_qsConfig.lenient_manual_stop
            && axis->lenient_counter_key == static_cast<WORD>(key)
            && now <= axis->lenient_press_deadline;
        lenient_manual_stop_candidate[key_index] = is_lenient_manual_stop;
        lenient_manual_stop_max_ms[key_index] = is_lenient_manual_stop
            ? axis->lenient_tap_max_ms
            : 0;
        lenient_manual_stop_times[key_index] = is_lenient_manual_stop
            ? now
            : Clock::time_point{};
        axis->lenient_counter_key = 0;
        axis->lenient_press_deadline = {};
        axis->lenient_tap_max_ms = 0;
        physical_key_down[key_index] = true;
        start_times[key_index] = now;
        CancelAxis(*axis); // 新的真实移动输入立即结束同轴反向脉冲
        if constexpr (kVerboseQuickStopInputLog) {
            if (is_lenient_manual_stop)
            std::cout << "[急停] 检测到人工反向补停，已进入宽容判定: " << key << std::endl;
        }
        return;
    }

    // C. 执行急停
    if (cmd.find("quickstop_") != std::string::npos)
    {
        char key = toupper(cmd.back());
        const int key_index = MoveKeyIndex(key);
        AxisControl* axis = GetAxis(key);
        if (key_index < 0 || !axis) return;

        int duration = 0;
        bool lenient_candidate = false;
        int lenient_tap_max_ms = 0;
        int lenient_duration = 0;
        {
            std::lock_guard movement_lock(movement_mutex);
            if (!physical_key_down[key_index]) return; // 忽略重复或非真实 KEYUP
            physical_key_down[key_index] = false;
            const auto now = Clock::now();
            duration = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
                now - start_times[key_index]).count());
            lenient_candidate = lenient_manual_stop_candidate[key_index];
            lenient_tap_max_ms = lenient_manual_stop_max_ms[key_index];
            if (lenient_candidate)
            {
                lenient_duration = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - lenient_manual_stop_times[key_index]).count());
            }
            lenient_manual_stop_candidate[key_index] = false;
            lenient_manual_stop_max_ms[key_index] = 0;
            lenient_manual_stop_times[key_index] = {};
        }

        if (lenient_candidate && lenient_duration <= lenient_tap_max_ms)
        {
            if constexpr (kVerboseQuickStopInputLog) {
                std::cout << "[急停] 宽容手动急停已接管 " << key
                          << "，补停 " << lenient_duration << "ms，跳过二次反向脉冲。" << std::endl;
            }
            return;
        }
        if constexpr (kVerboseQuickStopInputLog) {
            if (lenient_candidate)
            std::cout << "[急停] 人工反向键补停后继续按住 " << lenient_duration
                      << "ms，超过宽容上限，按正常换向处理。" << std::endl;
        }

        if (pause_jiting) return;           // pause键控制的全局急停开关
        if (inputenvironment::ShouldPauseAutomation()) return;
        if (!s_qsConfig.enabled) return;    // 总开关
        if (IsSilentKeyPressed()) return;   // Shift/Ctrl 拦截
        if (IsJumpQuickStopDisabled()) return; // 跳跃后 3 秒内临时禁用急停
        if (!is_holding_gun.load()) return; // 刀/雷 拦截

        WORD counterKey = 0;
        if (key == 'W') counterKey = 'S';
        else if (key == 'S') counterKey = 'W';
        else if (key == 'A') counterKey = 'D';
        else if (key == 'D') counterKey = 'A';

        if (!counterKey) return;

        if (s_qsConfig.lenient_manual_stop)
        {
            const int counter_index = MoveKeyIndex(static_cast<char>(counterKey));
            std::lock_guard movement_lock(movement_mutex);
            if (counter_index >= 0 && physical_key_down[counter_index])
            {
                const int stop_pulse = CalculateStopPulse(duration, counterKey);
                lenient_manual_stop_candidate[counter_index] = true;
                lenient_manual_stop_max_ms[counter_index] = CalculateLenientTapMaxMs(stop_pulse);
                lenient_manual_stop_times[counter_index] = Clock::now();
                CancelAxis(*axis);
                if constexpr (kVerboseQuickStopInputLog) {
                    std::cout << "[急停] 检测到提前按住的人工反向补停: "
                              << static_cast<char>(counterKey) << std::endl;
                }
                return;
            }
        }

        StartAxisPulse(counterKey, duration, axis);
    }
}
