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
    int micro_pulse = 10;
    int min_pulse = 15;
    int max_pulse = 40;
    int cap_pulse = 42;
    int micro_move_at = 55;
    int move_start_at = 120;
    int move_cap_at = 500;
    int curve_percent = 130;
    int horizontal_scale_percent = 100;
    int vertical_scale_percent = 95;
    std::vector<int> silent_keys = { VK_SHIFT, VK_CONTROL };
};

QuickStopConfig& GetQSConfig();
void LoadQuickStopConfig();
void SaveQuickStopConfig();
void ApplyRecommendedQuickStopConfig();
void SetQuickStopEnabled(bool enabled);
bool IsQuickStopEnabled();
void ProcessQuickStopCommand(const std::string& cmd);
void StartQuickStopHook();
void StopQuickStopHook();
void SetQuickStopPause(bool paused);
bool IsQuickStopPaused();
