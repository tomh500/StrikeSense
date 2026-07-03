#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace strikesense {

struct steam_local_account {
    std::wstring accountId32;
    std::wstring steamId64;
    std::wstring accountName;
    std::wstring personaName;
    std::wstring timestamp;
    std::wstring userdataPath;
    std::wstring localConfigPath;
    bool mostRecent = false;
    bool allowAutoLogin = false;
    bool rememberPassword = false;
    bool hasUserdata = false;
    bool hasLocalConfig = false;
};

std::vector<steam_local_account> GetLocalSteamAccounts();

} // namespace strikesense
