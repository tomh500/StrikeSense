#include "vscript_internal.h"

#include <fstream>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

namespace vscript::detail {

namespace {

std::wstring ParseAcfValue(const std::wstring& content, const std::wstring& key)
{
    const std::wstring pattern = L"\"" + key + L"\"";
    const size_t keyPos = content.find(pattern);
    if (keyPos == std::wstring::npos) return L"";
    const size_t valueStart = content.find(L'"', keyPos + pattern.size());
    if (valueStart == std::wstring::npos) return L"";
    const size_t valueEnd = content.find(L'"', valueStart + 1);
    if (valueEnd == std::wstring::npos) return L"";
    return content.substr(valueStart + 1, valueEnd - valueStart - 1);
}

std::wstring ExtractQuotedValueAfterKey(const std::wstring& content, const std::wstring& key)
{
    const size_t keyPos = content.find(key);
    if (keyPos == std::wstring::npos) return L"";
    const size_t lineEnd = content.find(L'\n', keyPos);
    const std::wstring line = content.substr(keyPos, lineEnd == std::wstring::npos ? std::wstring::npos : lineEnd - keyPos);
    const size_t thirdQuote = line.find(L'"', key.size());
    if (thirdQuote == std::wstring::npos) return L"";
    const size_t fourthQuote = line.find(L'"', thirdQuote + 1);
    if (fourthQuote == std::wstring::npos) return L"";
    return line.substr(thirdQuote + 1, fourthQuote - thirdQuote - 1);
}

std::wstring SteamIdToAccountId32(const std::wstring& steamId, bool steam64)
{
    if (steamId.empty()) return L"";
    if (!steam64) return steamId;
    try {
        constexpr unsigned long long base = 76561197960265728ULL;
        const unsigned long long value64 = std::stoull(steamId);
        if (value64 < base) return L"";
        return std::to_wstring(value64 - base);
    } catch (...) {
        return L"";
    }
}

fs::path GetLocalConfigPathForSteamId(const std::wstring& steamId, bool steam64)
{
    const std::wstring steamPath = GetSteamInstallPathText();
    if (steamPath.empty()) return {};
    const std::wstring id32 = SteamIdToAccountId32(steamId, steam64);
    if (id32.empty()) return {};
    return fs::path(steamPath) / L"userdata" / id32 / L"config" / L"localconfig.vdf";
}

std::wstring ReadAllWideLocal(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return L"";
    std::string raw((std::istreambuf_iterator<char>(in)), {});
    if (raw.size() >= 3 && (unsigned char)raw[0] == 0xEF && (unsigned char)raw[1] == 0xBB && (unsigned char)raw[2] == 0xBF) {
        raw.erase(0, 3);
    }
    return Utf8ToWide(raw);
}

std::wstring FindCs2InstallPath()
{
    const std::wstring steamPath = GetSteamInstallPathText();
    if (steamPath.empty()) return L"";
    const fs::path manifestPath = fs::path(steamPath) / L"steamapps" / L"appmanifest_730.acf";
    if (!fs::exists(manifestPath)) {
        std::wcout << L"[Steam API] 未找到 CS2 manifest: " << manifestPath.wstring() << std::endl;
        return L"";
    }
    const std::wstring content = ReadAllWideLocal(manifestPath);
    const std::wstring installDir = ParseAcfValue(content, L"installdir");
    if (installDir.empty()) {
        std::wcout << L"[Steam API] manifest 中未找到 installdir" << std::endl;
        return L"";
    }
    return (fs::path(steamPath) / L"steamapps" / L"common" / installDir).wstring();
}

value BuildSteamAccountObject(const strikesense::steam_local_account& account)
{
    return ObjectValue({
        { L"account_id32", TextValue(account.accountId32) },
        { L"steam_id64", TextValue(account.steamId64) },
        { L"account_name", TextValue(account.accountName) },
        { L"persona_name", TextValue(account.personaName) },
        { L"timestamp", TextValue(account.timestamp) },
        { L"userdata_path", TextValue(account.userdataPath) },
        { L"localconfig_path", TextValue(account.localConfigPath) },
        { L"most_recent", BoolValue(account.mostRecent) },
        { L"allow_auto_login", BoolValue(account.allowAutoLogin) },
        { L"remember_password", BoolValue(account.rememberPassword) },
        { L"has_userdata", BoolValue(account.hasUserdata) },
        { L"has_localconfig", BoolValue(account.hasLocalConfig) }
    });
}

} // namespace

std::wstring GetSteamInstallPathText()
{
    const std::wstring steamPath = SteamHelper::CallRegister2Steam();
    if (steamPath.empty() || steamPath == L"Read Failed") {
        std::wcout << L"[Steam API] 读取 Steam 安装路径失败" << std::endl;
        return L"";
    }
    std::wcout << L"[Steam API] Steam 安装路径: " << steamPath << std::endl;
    return steamPath;
}

std::wstring GetCs2InstallPathText()
{
    const std::wstring path = FindCs2InstallPath();
    std::wcout << L"[Steam API] CS2 安装路径: " << (path.empty() ? L"<empty>" : path) << std::endl;
    return path;
}

std::wstring GetCs2CfgPathText()
{
    const std::wstring cs2Path = FindCs2InstallPath();
    if (cs2Path.empty()) return L"";
    const std::wstring cfgPath = (fs::path(cs2Path) / L"game" / L"csgo" / L"cfg").wstring();
    std::wcout << L"[Steam API] CS2 cfg 路径: " << cfgPath << std::endl;
    return cfgPath;
}

std::wstring Steam64To32Text(const std::wstring& steam64)
{
    return SteamIdToAccountId32(steam64, true);
}

std::wstring GetSteamLocalConfigPathText(const std::wstring& steamId, bool steam64)
{
    const fs::path path = GetLocalConfigPathForSteamId(steamId, steam64);
    if (path.empty()) return L"";
    std::wcout << L"[Steam API] localconfig 路径: " << path.wstring() << std::endl;
    return path.wstring();
}

std::wstring GetSteamLaunchOptionsText(const std::wstring& steamId, bool steam64)
{
    const fs::path localConfigPath = GetLocalConfigPathForSteamId(steamId, steam64);
    if (localConfigPath.empty() || !fs::exists(localConfigPath)) {
        std::wcout << L"[Steam API] 未找到 localconfig.vdf" << std::endl;
        return L"";
    }
    const std::wstring content = ReadAllWideLocal(localConfigPath);
    if (content.empty()) return L"";
    const size_t app730 = content.find(L"\"730\"");
    if (app730 == std::wstring::npos) {
        std::wcout << L"[Steam API] localconfig 中未找到 AppID 730" << std::endl;
        return L"";
    }
    const std::wstring launchOptions = ExtractQuotedValueAfterKey(content.substr(app730), L"\"LaunchOptions\"");
    std::wcout << L"[Steam API] 读取启动项: " << (launchOptions.empty() ? L"<empty>" : launchOptions) << std::endl;
    return launchOptions;
}

bool WriteSteamGsiConfigFile()
{
    const std::wstring cfgPath = GetCs2CfgPathText();
    if (cfgPath.empty()) return false;
    const fs::path target = fs::path(cfgPath) / L"gamestate_integration_square.cfg";
    std::error_code ec;
    fs::create_directories(target.parent_path(), ec);
    std::ofstream out(target, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        std::wcout << L"[Steam API] 写入 GSI 配置失败: " << target.wstring() << std::endl;
        return false;
    }
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
    const bool ok = out.good();
    std::wcout << L"[Steam API] " << (ok ? L"已写入" : L"写入失败") << L" GSI 配置: " << target.wstring() << std::endl;
    return ok;
}

value BuildSteamUserIdListValue(bool convertTo64)
{
    SteamHelper helper;
    std::vector<value> items;
    for (const std::string& id32 : helper.GetSteamUserIDs()) {
        if (convertTo64) items.push_back(TextValue(Utf8ToWide(helper.ConvertToSteam64ID(id32))));
        else items.push_back(TextValue(Utf8ToWide(id32)));
    }
    std::wcout << L"[Steam API] 已构造本地 Steam ID 列表，数量=" << items.size()
               << L"，模式=" << (convertTo64 ? L"64位" : L"32位") << std::endl;
    return ListValue(items);
}

value BuildSteamAccountsValue()
{
    std::vector<value> items;
    for (const auto& account : strikesense::GetLocalSteamAccounts()) {
        items.push_back(BuildSteamAccountObject(account));
    }
    std::wcout << L"[Steam API] 已构造本地 Steam 账号对象列表，数量=" << items.size() << std::endl;
    return ListValue(items);
}

bool HasSteamLocalUserId(const std::wstring& steamId, bool steam64)
{
    if (steamId.empty()) return false;
    SteamHelper helper;
    if (!steam64) {
        const bool matched = helper.VerSteamID(WideToUtf8(steamId));
        std::wcout << L"[Steam API] 检查本地 Steam32 ID: " << steamId
                   << L" -> " << (matched ? L"存在" : L"不存在") << std::endl;
        return matched;
    }
    const auto id64Set = helper.ConvertAllToSteam64IDs();
    const bool matched = id64Set.find(WideToUtf8(steamId)) != id64Set.end();
    std::wcout << L"[Steam API] 检查本地 Steam64 ID: " << steamId
               << L" -> " << (matched ? L"存在" : L"不存在") << std::endl;
    return matched;
}

} // namespace vscript::detail
