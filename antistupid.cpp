#include "antistupid.h"
#include "steam_helper.h"
#include <windows.h>
#include <iostream>
#include <filesystem>
#include <unordered_set>

namespace fs = std::filesystem;

namespace antistupid {

// 封禁列表（64位 SteamID）
static const std::unordered_set<std::string> s_bannedList = {
    "76561198446466324"
};

// ----------------------------------------------------------
bool CheckAndBlock()
{
    // 1. 读取 Steam 安装路径
    std::wstring steamPath = strikesense::GetSteamPathFromRegistry();
    if (steamPath.empty())
    {
        std::cout << "[反封禁] 无法读取 Steam 安装路径，跳过检查。" << std::endl;
        return false;
    }

    // 2. 读取 Steam\userdata\ 下的文件夹（32位 SteamID）
    fs::path userdataPath = fs::path(steamPath) / L"userdata";
    if (!fs::exists(userdataPath))
    {
        std::cout << "[反封禁] userdata 文件夹不存在。" << std::endl;
        return false;
    }

    std::cout << "[反封禁] 正在扫描 Steam 用户..." << std::endl;

    for (const auto& entry : fs::directory_iterator(userdataPath))
    {
        if (!fs::is_directory(entry))
            continue;

        std::wstring dirName = entry.path().filename().wstring();

        // 过滤掉非数字命名的文件夹
        bool isNumeric = true;
        for (wchar_t c : dirName)
        {
            if (c < L'0' || c > L'9')
            {
                isNumeric = false;
                break;
            }
        }
        if (!isNumeric || dirName.empty())
            continue;

        // 将 32 位 SteamID（string）转为 64 位
        std::string id32;
        for (wchar_t wc : dirName)
            id32 += (char)wc;

        unsigned long long base = 76561197960265728ULL;
        unsigned long long steam32 = std::stoull(id32);
        unsigned long long steam64 = steam32 + base;
        std::string steam64str = std::to_string(steam64);

        std::cout << "[反封禁] 发现用户: 32位=" << id32 << " → 64位=" << steam64str << std::endl;

        // 检查封禁列表
        if (s_bannedList.find(steam64str) != s_bannedList.end())
        {
            std::wcerr << L"[反封禁] 匹配到封禁用户！SteamID64=" << steam64str.c_str() << std::endl;

            wchar_t msg[512];
            swprintf_s(msg, L"检测到被封禁的 Steam 帐户 (ID: %hs) 在本机登录过！\n程序将无法启动。",
                       steam64str.c_str());
            MessageBoxW(nullptr, msg, L"StrikeSense - 反封禁", MB_OK | MB_ICONERROR);
            return true; // 阻止启动
        }
    }

    std::cout << "[反封禁] 未发现封禁用户。" << std::endl;
    return false;
}

} // namespace antistupid