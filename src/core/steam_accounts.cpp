#include "steam_accounts.h"
#include "SteamHelper.h"

#include <Windows.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>

namespace strikesense {

namespace {

std::wstring Utf8ToWide(const std::string& text)
{
    if (text.empty()) return L"";
    const int count = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), nullptr, 0);
    if (count <= 0) return L"";
    std::wstring out(count, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), (int)text.size(), out.data(), count);
    return out;
}

std::wstring ReadAllWide(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return L"";
    std::string raw((std::istreambuf_iterator<char>(in)), {});
    if (raw.size() >= 3 && (unsigned char)raw[0] == 0xEF && (unsigned char)raw[1] == 0xBB && (unsigned char)raw[2] == 0xBF) {
        raw.erase(0, 3);
    }
    return Utf8ToWide(raw);
}

std::wstring Trim(std::wstring text)
{
    const auto isSpace = [](wchar_t c) { return iswspace(c) != 0; };
    text.erase(text.begin(), std::find_if_not(text.begin(), text.end(), isSpace));
    text.erase(std::find_if_not(text.rbegin(), text.rend(), isSpace).base(), text.end());
    return text;
}

bool IsDigitsOnly(const std::wstring& text)
{
    return !text.empty() && std::all_of(text.begin(), text.end(), [](wchar_t c) { return c >= L'0' && c <= L'9'; });
}

std::optional<std::pair<std::wstring, std::wstring>> ParseQuotedPair(const std::wstring& line)
{
    const size_t first = line.find(L'"');
    if (first == std::wstring::npos) return std::nullopt;
    const size_t second = line.find(L'"', first + 1);
    if (second == std::wstring::npos) return std::nullopt;
    const size_t third = line.find(L'"', second + 1);
    if (third == std::wstring::npos) return std::nullopt;
    const size_t fourth = line.find(L'"', third + 1);
    if (fourth == std::wstring::npos) return std::nullopt;
    return std::make_pair(line.substr(first + 1, second - first - 1), line.substr(third + 1, fourth - third - 1));
}

std::optional<std::wstring> ParseSingleQuotedText(const std::wstring& line)
{
    const size_t first = line.find(L'"');
    if (first == std::wstring::npos) return std::nullopt;
    const size_t second = line.find(L'"', first + 1);
    if (second == std::wstring::npos) return std::nullopt;
    if (line.find(L'"', second + 1) != std::wstring::npos) return std::nullopt;
    return line.substr(first + 1, second - first - 1);
}

std::wstring Steam32To64(const std::wstring& steam32)
{
    if (!IsDigitsOnly(steam32)) return L"";
    constexpr unsigned long long base = 76561197960265728ULL;
    const unsigned long long value32 = std::stoull(steam32);
    return std::to_wstring(base + value32);
}

} // namespace

std::vector<steam_local_account> GetLocalSteamAccounts()
{
    std::vector<steam_local_account> accounts;

    const std::wstring steamPath = SteamHelper::CallRegister2Steam();
    if (steamPath.empty() || steamPath == L"Read Failed") {
        std::wcout << L"[Steam账号] 无法读取 Steam 安装路径" << std::endl;
        return accounts;
    }

    const fs::path loginUsersPath = fs::path(steamPath) / L"config" / L"loginusers.vdf";
    const std::wstring content = ReadAllWide(loginUsersPath);
    if (content.empty()) {
        std::wcout << L"[Steam账号] 无法读取 loginusers.vdf: " << loginUsersPath.wstring() << std::endl;
        return accounts;
    }

    std::wcout << L"[Steam账号] 开始解析本地 Steam 账号列表: " << loginUsersPath.wstring() << std::endl;

    std::wstringstream stream(content);
    std::wstring line;
    steam_local_account current;
    bool inAccount = false;
    int depth = 0;

    while (std::getline(stream, line)) {
        const std::wstring trimmed = Trim(line);
        if (trimmed.empty()) continue;

        if (!inAccount) {
            if (auto accountId = ParseSingleQuotedText(trimmed)) {
                if (IsDigitsOnly(*accountId)) {
                    current = {};
                    current.steamId64 = *accountId;
                    const unsigned long long value64 = std::stoull(current.steamId64);
                    if (value64 >= 76561197960265728ULL) {
                        current.accountId32 = std::to_wstring(value64 - 76561197960265728ULL);
                    }
                }
            }
            if (trimmed == L"{") {
                if (!current.steamId64.empty()) {
                    inAccount = true;
                    depth = 1;
                    current.userdataPath = (fs::path(steamPath) / L"userdata" / current.accountId32).wstring();
                    current.hasUserdata = fs::exists(current.userdataPath);
                    current.localConfigPath = (fs::path(current.userdataPath) / L"config" / L"localconfig.vdf").wstring();
                    current.hasLocalConfig = fs::exists(current.localConfigPath);
                    std::wcout << L"[Steam账号] 进入账号块: steam64=" << current.steamId64 << std::endl;
                }
            }
            continue;
        }

        if (trimmed == L"{") {
            ++depth;
            continue;
        }
        if (trimmed == L"}") {
            --depth;
            if (depth == 0) {
                std::wcout << L"[Steam账号] 已解析账号: account=" << current.accountName
                           << L", persona=" << current.personaName
                           << L", steam64=" << current.steamId64 << std::endl;
                accounts.push_back(current);
                current = {};
                inAccount = false;
            }
            continue;
        }

        auto pair = ParseQuotedPair(trimmed);
        if (!pair) continue;

        const std::wstring& key = pair->first;
        const std::wstring& value = pair->second;
        if (key == L"AccountName") current.accountName = value;
        else if (key == L"PersonaName") current.personaName = value;
        else if (key == L"MostRecent") current.mostRecent = value == L"1";
        else if (key == L"AllowAutoLogin") current.allowAutoLogin = value == L"1";
        else if (key == L"RememberPassword") current.rememberPassword = value == L"1";
        else if (key == L"Timestamp") current.timestamp = value;
    }

    std::wcout << L"[Steam账号] 本地账号总数: " << accounts.size() << std::endl;
    return accounts;
}

} // namespace strikesense
