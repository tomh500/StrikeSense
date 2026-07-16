#pragma once

#include <string>
#include <vector>

namespace modulenotifications {
struct feature_line {
    std::wstring id;
    std::wstring text;
    std::wstring accessory;
};

std::vector<feature_line> CollectEnabledFeatures(bool includeHidden = false);
void Refresh();
void Shutdown();
void RegisterCustomLine(const std::wstring& id, const std::wstring& text,
    const std::wstring& accessory = L"");
void RemoveCustomLine(const std::wstring& id);
void SetModuleHidden(const std::wstring& id, bool hidden);
bool IsModuleHidden(const std::wstring& id);
std::vector<std::wstring> NativeModuleIds();
void UpdateCrosshairRecoilSignal(const std::wstring& text);
bool ProcessConsoleCommand(const std::wstring& text);
bool GetModuleEnabled(const std::wstring& id);
bool SetModuleEnabled(const std::wstring& id, bool enabled);
bool ToggleModuleEnabled(const std::wstring& id);
std::wstring GetModuleValue(const std::wstring& id, const std::wstring& field = L"");
}
