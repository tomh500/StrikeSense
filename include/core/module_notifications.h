#pragma once

#include <string>
#include <vector>

namespace modulenotifications {
std::vector<std::wstring> CollectEnabledFeatures();
void Refresh();
void Shutdown();
void RegisterCustomLine(const std::wstring& id, const std::wstring& text);
void RemoveCustomLine(const std::wstring& id);
void UpdateCrosshairRecoilSignal(const std::wstring& text);
}
