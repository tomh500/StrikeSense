#include "mouse_jitter.h"

#include "pages.h"
#include "volume_mixer.h"

#include <Windows.h>
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

namespace mousejitter {
namespace {
    constexpr int kJitterRange = 1;
    constexpr int kHz = 128;

    std::atomic<bool> g_enabled{ false };
    std::atomic<bool> g_running{ false };
    std::atomic<bool> g_workerActive{ false };

    void SendJitterStep(int direction)
    {
        INPUT input{};
        input.type = INPUT_MOUSE;
        input.mi.dwFlags = MOUSEEVENTF_MOVE;
        input.mi.dx = kJitterRange * direction;
        input.mi.dy = 0;
        SendInput(1, &input, sizeof(INPUT));
    }

    void WorkerLoop()
    {
        std::cout << "[多绑定脚本] 鼠标抖动线程已启动。" << std::endl;

        int direction = 1;
        const auto stepTime = std::chrono::microseconds(1000000 / kHz);

        while (g_running.load()) {
            const bool shouldShake = g_enabled.load() && IsRageModeEnabled() && IsCS2WindowActive();
            if (!shouldShake) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            const auto start = std::chrono::high_resolution_clock::now();
            SendJitterStep(direction);
            direction *= -1;

            const auto used = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::high_resolution_clock::now() - start);
            if (used < stepTime) std::this_thread::sleep_for(stepTime - used);
        }

        g_workerActive.store(false);
        std::cout << "[多绑定脚本] 鼠标抖动线程已退出。" << std::endl;
    }

    void EnsureWorker()
    {
        bool expectedInactive = false;
        if (!g_workerActive.compare_exchange_strong(expectedInactive, true)) {
            g_running.store(true);
            return;
        }

        g_running.store(true);
        std::thread(WorkerLoop).detach();
    }

    void StopWorker()
    {
        g_running.store(false);
    }
}

void SetEnabled(bool enabled)
{
    if (enabled && !IsRageModeEnabled()) {
        std::cout << "[多绑定脚本] 超频配置未开启，拒绝打开鼠标抖动支持。" << std::endl;
        enabled = false;
    }

    if (g_enabled.load() == enabled) {
        if (enabled) EnsureWorker();
        return;
    }

    g_enabled.store(enabled);
    std::cout << "[多绑定脚本] 为多绑定脚本提供支持已切换为: "
              << (enabled ? "开启" : "关闭") << std::endl;

    if (enabled) EnsureWorker();
    else StopWorker();
}

bool IsEnabled()
{
    return g_enabled.load();
}

void Shutdown()
{
    g_enabled.store(false);
    StopWorker();
}
}
