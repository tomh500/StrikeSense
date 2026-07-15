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
    int micro_pulse = 80;
    int min_pulse = 90;
    int max_pulse = 150;
    int cap_pulse = 200;
    int micro_move_at = 55;
    int move_start_at = 120;
    int move_cap_at = 500;
    int curve_percent = 125;
    int horizontal_scale_percent = 100;
    int vertical_scale_percent = 100;
    std::vector<int> silent_keys = { VK_SHIFT, VK_CONTROL };
};

QuickStopConfig& GetQSConfig();
void LoadQuickStopConfig();
void SaveQuickStopConfig();
void ApplyQuickStopConfigChanges();
void SetQuickStopEnabled(bool enabled);
bool IsQuickStopEnabled();
void StopQuickStopForRageDisabled();
void ProcessQuickStopCommand(const std::string& cmd);
void StartQuickStopHook();
void StopQuickStopHook();
void SetQuickStopPause(bool paused);
bool IsQuickStopPaused();
