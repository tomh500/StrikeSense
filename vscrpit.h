#pragma once

#include <Windows.h>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace vscrpit {

enum class buildcode {
    user = 0,
    userdebug = 1,
    eng = 2
};

struct mounted_script {
    std::wstring path;
    bool continuous = false;
};

void Initialize(HINSTANCE instance, HWND owner);
void Shutdown();

buildcode GetBuildCode();
buildcode GetRuntimeCapability();
bool HasUserDebugCapability();
bool EnsureOemUnlockFile();
bool IsOemUnlockValid();

void UpdateFromGsi(const nlohmann::json& state);
void TickContinuousScripts();
bool ExecuteScriptFile(const std::wstring& path);

std::vector<mounted_script>& MountedScripts();
void LoadMountedScripts();
void SaveMountedScripts();
void AddMountedScript(const std::wstring& path);
void RemoveMountedScript(size_t index);
void ToggleContinuous(size_t index);

std::wstring GetScriptConfigPath();
std::wstring GetDefaultScriptDir();
std::wstring GetExampleScriptPath();
void EnsureExampleScript();

} // namespace vscrpit
