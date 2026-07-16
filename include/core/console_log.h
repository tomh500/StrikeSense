#pragma once

namespace consolelog {
    void LoadConfig();
    void SaveConfig();
    void SetEnabled(bool enabled);
    bool IsEnabled();
    void SetRuntimeReaderNeeded(bool needed);
    void SetCscriptReaderNeeded(bool needed);
    void StopForRageDisabled();
    void Shutdown();
}
