#pragma once

#include <string>
#include <filesystem>

namespace fs = std::filesystem;

namespace strikesense {

// ----------------------------------------------------------
// 从 Windows 注册表读取 Steam 安装目录
// HKCU\SOFTWARE\Valve\Steam\SteamPath
// ----------------------------------------------------------
std::wstring GetSteamPathFromRegistry();

// ----------------------------------------------------------
// 扫描 Steam 下的 steamapps 目录，找到 appmanifest_730.acf
// 解析出 "installdir" 字段获取 CS2 安装目录
// ----------------------------------------------------------
std::wstring FindCS2InstallDir(const std::wstring& steamPath);

// ----------------------------------------------------------
// 拼接 CS2 的 cfg 目录路径
// {cs2Dir}/game/csgo/cfg
// ----------------------------------------------------------
std::wstring GetCS2CfgPath(const std::wstring& cs2InstallDir);

// ----------------------------------------------------------
// 写入 gamestate_integration_square.cfg
// ----------------------------------------------------------
bool WriteGSIConfig(const std::wstring& cfgDir);

// ----------------------------------------------------------
// 从本地 JSON 配置读取 / 保存 cs2_cfg_path
// ----------------------------------------------------------
std::wstring LoadSavedCfgPath();
void SaveCfgPath(const std::wstring& path);

} // namespace strikesense

