#pragma once

namespace consolelog {
    void LoadConfig();
    void SaveConfig();
    void SetEnabled(bool enabled);
    bool IsEnabled();
    void StopForRageDisabled();
    void Shutdown();
}
