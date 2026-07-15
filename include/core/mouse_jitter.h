#pragma once

namespace mousejitter {
    void LoadConfig();
    void SaveConfig();
    void SetEnabled(bool enabled);
    bool IsEnabled();
    void StopForRageDisabled();
    void Shutdown();
}
