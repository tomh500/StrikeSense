#include "steam_helper.h"
#include <windows.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <nlohmann/json.hpp>

namespace strikesense {

// ----------------------------------------------------------
std::wstring GetSteamPathFromRegistry()
{
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"SOFTWARE\\Valve\\Steam",
                      0, KEY_READ, &hKey) != ERROR_SUCCESS)
        return L"";

    wchar_t buffer[512] = {};
    DWORD bufferSize = sizeof(buffer);

    if (RegQueryValueExW(hKey, L"SteamPath", nullptr, nullptr,
                         (LPBYTE)buffer, &bufferSize) != ERROR_SUCCESS)
    {
        RegCloseKey(hKey);
        return L"";
    }

    RegCloseKey(hKey);
    return std::wstring(buffer);
}

// ----------------------------------------------------------
// 简单 ACF/VDF 解析器：找 "installdir" 键的值
// appmanifest_730.acf 格式：
//   "AppState"
//   {
//       ...
//       "installdir"    "Counter-Strike Global Offensive"
//       ...
//   }
// ----------------------------------------------------------
static std::string ParseACFValue(const std::string& content, const std::string& key)
{
    size_t pos = content.find('"' + key + '"');
    if (pos == std::string::npos)
        return "";

    // 跳过 key 本身
    pos += key.length() + 4;  // 4 = 两个引号 + 空白

    // 跳过空白
    while (pos < content.length() && (content[pos] == ' ' || content[pos] == '\t'))
        pos++;

    // 读取值（在引号中）
    if (pos < content.length() && content[pos] == '"')
    {
        pos++;
        size_t end = content.find('"', pos);
        if (end != std::string::npos)
            return content.substr(pos, end - pos);
    }

    return "";
}

// ----------------------------------------------------------
std::wstring FindCS2InstallDir(const std::wstring& steamPath)
{
    fs::path steamAppsPath = fs::path(steamPath) / L"steamapps";
    fs::path manifestPath = steamAppsPath / L"appmanifest_730.acf";

    if (!fs::exists(manifestPath))
        return L"";

    // 读取文件
    std::ifstream file(manifestPath);
    if (!file.is_open())
        return L"";

    std::stringstream ss;
    ss << file.rdbuf();
    file.close();

    std::string content = ss.str();
    std::string installdir = ParseACFValue(content, "installdir");

    if (installdir.empty())
        return L"";

    // 转换为 wstring
    int wlen = MultiByteToWideChar(CP_UTF8, 0, installdir.c_str(),
                                   (int)installdir.length(), nullptr, 0);
    if (wlen <= 0)
        return L"";

    std::wstring wdir(wlen, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, installdir.c_str(),
                        (int)installdir.length(), &wdir[0], wlen);

    // 拼接完整路径
    return (steamAppsPath / L"common" / wdir).wstring();
}

// ----------------------------------------------------------
std::wstring GetCS2CfgPath(const std::wstring& cs2InstallDir)
{
    fs::path cfgPath = fs::path(cs2InstallDir) / L"game" / L"csgo" / L"cfg";
    return cfgPath.wstring();
}

// ----------------------------------------------------------
bool WriteGSIConfig(const std::wstring& cfgDir)
{
    fs::path cfgPath = fs::path(cfgDir);
    if (!fs::exists(cfgPath))
    {
        // 尝试创建目录
        std::error_code ec;
        fs::create_directories(cfgPath, ec);
        if (ec)
            return false;
    }

    fs::path gsiFile = cfgPath / L"gamestate_integration_square.cfg";

    std::ofstream out(gsiFile);
    if (!out.is_open())
        return false;

    out << "\"Console Sample v.1\"\n"
        << "{\n"
        << "  \"uri\" \"http://127.0.0.1:1009\"\n"
        << "  \"timeout\" \"5.0\"\n"
        << "  \"buffer\" \"0.1\"\n"
        << "  \"throttle\" \"0.5\"\n"
        << "  \"heartbeat\" \"30.0\"\n"
        << "  \"output\"\n"
        << "  {\n"
        << "    \"precision_time\" \"3\"\n"
        << "    \"precision_position\" \"1\"\n"
        << "    \"precision_vector\" \"3\"\n"
        << "  }\n"
        << "  \"data\"\n"
        << "  {\n"
        << "    \"provider\"               \"1\"\n"
        << "    \"map\"                    \"1\"\n"
        << "    \"team\"                   \"1\"\n"
        << "    \"round\"                  \"1\"\n"
        << "    \"player_state\"           \"1\"\n"
        << "    \"player_id\"              \"1\"\n"
        << "    \"player_match_stats\"     \"1\"\n"
        << "    \"allplayers_id\"          \"1\"\n"
        << "    \"allplayers_state\"       \"1\"\n"
        << "    \"allplayers_match_stats\" \"1\"\n"
        << "    \"bomb\"                   \"1\"\n"
        << "    \"player_name\"            \"1\"\n"
        << "    \"player_weapons\"         \"1\"\n"
        << "  }\n"
        << "}\n";

    out.close();
    return true;
}

// ----------------------------------------------------------
std::wstring LoadSavedCfgPath()
{
    // 配置存储目录: %USERPROFILE%/StrikeSense
    wchar_t profile[MAX_PATH] = {};
    GetEnvironmentVariableW(L"USERPROFILE", profile, MAX_PATH);
    fs::path configDir = fs::path(profile) / L"StrikeSense";
    fs::path configFile = configDir / L"setting" / L"gsi.json";
    if (!fs::exists(configFile))
        return L"";

    try {
        std::ifstream in(configFile);
        if (!in.is_open())
            return L"";

        nlohmann::json j;
        in >> j;
        in.close();

        if (j.contains("cs2_cfg_path") && j["cs2_cfg_path"].is_string())
        {
            std::string utf8 = j["cs2_cfg_path"];
            int wlen = MultiByteToWideChar(CP_UTF8, 0,
                utf8.c_str(), (int)utf8.length(), nullptr, 0);
            if (wlen > 0)
            {
                std::wstring wpath(wlen, L'\0');
                MultiByteToWideChar(CP_UTF8, 0,
                    utf8.c_str(), (int)utf8.length(), &wpath[0], wlen);
                return wpath;
            }
        }
    }
    catch (...) {
    }

    return L"";
}

// ----------------------------------------------------------
void SaveCfgPath(const std::wstring& path)
{
    // 配置存储目录: %USERPROFILE%/StrikeSense
    wchar_t profile[MAX_PATH] = {};
    GetEnvironmentVariableW(L"USERPROFILE", profile, MAX_PATH);
    fs::path configDir = fs::path(profile) / L"StrikeSense" / L"setting";
    if (!fs::exists(configDir))
    {
        std::error_code ec;
        fs::create_directories(configDir, ec);
    }

    fs::path configFile = configDir / L"gsi.json";

    try {
        nlohmann::json j;

        // 如果文件已存在，先读取
        if (fs::exists(configFile))
        {
            std::ifstream in(configFile);
            if (in.is_open())
            {
                try { in >> j; } catch (...) {}
                in.close();
            }
        }

        // 将 wstring 转为 UTF-8 string
        int utf8Len = WideCharToMultiByte(CP_UTF8, 0,
            path.c_str(), (int)path.length(), nullptr, 0, nullptr, nullptr);
        if (utf8Len > 0)
        {
            std::string utf8(utf8Len, '\0');
            WideCharToMultiByte(CP_UTF8, 0,
                path.c_str(), (int)path.length(), &utf8[0], utf8Len,
                nullptr, nullptr);
            j["cs2_cfg_path"] = utf8;
        }

        std::ofstream out(configFile);
        if (out.is_open())
        {
            out << j.dump(2);
            out.close();
        }
    }
    catch (...) {
    }
}

} // namespace strikesense