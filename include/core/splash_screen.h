#pragma once

#include <Windows.h>

namespace splashscreen {

struct State {
    HWND hwnd = nullptr;
    HINSTANCE instance = nullptr;
    int base_width = 0;
    int base_height = 0;
    double created_at = 0.0;
    double exit_started_at = 0.0;
    bool exit_mode = false;
    bool active = false;
};

State Create(HINSTANCE instance);
void PumpOnce(State& state);
void PumpUntilReady(State& state, HANDLE ready_event, DWORD minimum_ms);
void PlayExit(State& state);
void Destroy(State& state);
void ReleaseResources();

} // namespace splashscreen
