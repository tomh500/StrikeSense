#pragma once

#include <Windows.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace cscript {

struct mounted_script {
    std::uint64_t id = 0;
    std::wstring path;
    UINT virtual_key = 0;
    bool extended_key = false;
    std::wstring source_key;
    bool valid = false;
    std::wstring status;
    std::size_t pressed_command_count = 0;
    std::size_t released_command_count = 0;
};

void LoadConfig();
void SaveConfig();
bool SetEnabled(bool enabled);
bool IsEnabled();
void StopForRageDisabled();
void Shutdown();

const std::vector<mounted_script>& MountedScripts();
bool AddMountedScript(const std::filesystem::path& path, std::wstring* error = nullptr);
void RemoveMountedScript(std::size_t index);
bool ReloadMountedScript(std::size_t index, std::wstring* error = nullptr);
bool SetScriptKey(std::size_t index, UINT virtualKey, bool extendedKey, std::wstring* error = nullptr);
bool SetTickerKey(UINT virtualKey, bool extendedKey, std::wstring* error = nullptr);
std::wstring GetTickerSourceKey();

std::wstring GetDefaultScriptDir();
std::wstring GetTickerCfgPath();
std::wstring GetLastRuntimeStatus();
void ProcessConsoleLine(const std::wstring& line);
std::wstring VirtualKeyToSourceName(UINT virtualKey, bool extendedKey);
bool NormalizeWindowKey(WPARAM wParam, LPARAM lParam, UINT& virtualKey, bool& extendedKey);

} // namespace cscript
