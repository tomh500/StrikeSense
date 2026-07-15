#pragma once
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <chrono>
#include <map>
#include <Windows.h>

struct QuickStopConfig {
    bool enabled = false;
    int min_pulse = 15;
    int max_pulse = 40;
    int cap_pulse = 42;
    int move_start_at = 100;
    int move_cap_at = 500;
    std::vector<int> silent_keys = { VK_SHIFT, VK_CONTROL };
};

QuickStopConfig& GetQSConfig();
void LoadQuickStopConfig();
void SaveQuickStopConfig();
void SetQuickStopEnabled(bool enabled, bool persist = true);
bool IsQuickStopEnabled();
void ProcessQuickStopCommand(const std::string& cmd);
void StartQuickStopHook();
void StopQuickStopHook();
void SetQuickStopPause(bool paused);
bool IsQuickStopPaused();
