#include "config.h"
#include <windows.h>
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>

namespace config {

Settings g_cache;
static bool s_cacheValid = false;

void InvalidateCache() { s_cacheValid = false; }

static void EnsureDirectory(const fs::path& dir)
{
    std::error_code ec;
    if (!fs::exists(dir))
        fs::create_directories(dir, ec);
}

void EnsureDirectoriesExist()
{
    wchar_t profile[MAX_PATH] = {};
    GetEnvironmentVariableW(L"USERPROFILE", profile, MAX_PATH);
    fs::path base = fs::path(profile) / L"StrikeSense";
    EnsureDirectory(base);
    EnsureDirectory(base / L"setting");
    EnsureDirectory(base / L"snd");
    std::cout << "[配置] 目录结构已确保: " << base.string() << std::endl;
}

std::wstring GetConfigDir()
{
    wchar_t profile[MAX_PATH] = {};
    GetEnvironmentVariableW(L"USERPROFILE", profile, MAX_PATH);
    return (fs::path(profile) / L"StrikeSense" / L"setting").wstring();
}

std::wstring GetConfigPath()
{
    return (fs::path(GetConfigDir()) / L"gsi.json").wstring();
}

std::wstring GetDefaultSndDir()
{
    wchar_t profile[MAX_PATH] = {};
    GetEnvironmentVariableW(L"USERPROFILE", profile, MAX_PATH);
    return (fs::path(profile) / L"StrikeSense" / L"snd").wstring();
}

std::wstring GetDefaultSndPath(const std::wstring& name, bool useOgg)
{
    fs::path sndDir(GetDefaultSndDir());
    std::wstring ext = useOgg ? L".ogg" : L".wav";
    return (sndDir / (name + ext)).wstring();
}

Settings Load()
{
    if (s_cacheValid) return g_cache;

    Settings& s = g_cache;
    s = Settings();  // 重置默认值
    fs::path path(GetConfigPath());

    if (!fs::exists(path))
    {
        std::cout << "[配置] 首次运行，创建默认配置:" << path.string() << std::endl;
        Save(s);
        s_cacheValid = true;
        return s;
    }

    try {
        std::ifstream in(path);
        if (!in.is_open()) { s_cacheValid = true; return s; }

        nlohmann::json j;
        in >> j;
        in.close();

        auto readBool = [&](const std::string& key, bool& target) {
            if (j.contains(key) && j[key].is_boolean()) target = j[key];
        };
        auto readFloat = [&](const std::string& key, float& target) {
            if (j.contains(key) && j[key].is_number_float()) target = j[key];
            else if (j.contains(key) && j[key].is_number()) target = j[key].get<float>();
        };
        auto readStr = [&](const std::string& key, std::wstring& target) {
            if (j.contains(key) && j[key].is_string()) {
                std::string u8 = j[key];
                if (u8.empty()) return;
                int wlen = MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), (int)u8.length(), nullptr, 0);
                if (wlen > 0) {
                    target.resize(wlen);
                    MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), (int)u8.length(), &target[0], wlen);
                }
            }
        };

        // force enable_kill_sound to true (overrides old configs)
        s.enable_kill_sound = true;
        if (j.contains("enable_kill_sound") && j["enable_kill_sound"].is_boolean())
            s.enable_kill_sound = j["enable_kill_sound"];
        readBool("ogg", s.ogg);
        readBool("custom_musickit", s.custom_musickit);
        readFloat("vol", s.volume);

        readStr("snd_1", s.snd_1); readStr("snd_2", s.snd_2);
        readStr("snd_3", s.snd_3); readStr("snd_4", s.snd_4);
        readStr("snd_5", s.snd_5); readStr("snd_extra", s.snd_extra);
        readStr("snd_mvp", s.snd_mvp); readStr("snd_win", s.snd_win);
        readStr("snd_lose", s.snd_lose); readStr("snd_bomb", s.snd_bomb);
        readStr("snd_round", s.snd_round); readStr("snd_buy", s.snd_buy);
        readStr("snd_death", s.snd_death); readStr("snd_gameover", s.snd_gameover);
        readStr("snd_menu", s.snd_menu);

        std::cout << "[配置] 加载成功。" << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "[配置] 加载出错: " << e.what() << std::endl;
    }

    s_cacheValid = true;
    return s;
}

bool Save(const Settings& s)
{
    g_cache = s;
    s_cacheValid = true;
    EnsureDirectoriesExist();
    fs::path path(GetConfigPath());

    try {
        nlohmann::json j;
        j["enable_kill_sound"] = s.enable_kill_sound;
        j["ogg"] = s.ogg;
        j["custom_musickit"] = s.custom_musickit;
        j["vol"] = s.volume;

        auto writeStr = [&](const std::string& key, const std::wstring& val) {
            if (val.empty()) { j[key] = ""; return; }
            int u8Len = WideCharToMultiByte(CP_UTF8, 0, val.c_str(), (int)val.length(), nullptr, 0, nullptr, nullptr);
            if (u8Len > 0) {
                std::string u8(u8Len, '\0');
                WideCharToMultiByte(CP_UTF8, 0, val.c_str(), (int)val.length(), &u8[0], u8Len, nullptr, nullptr);
                j[key] = u8;
            }
        };
        writeStr("snd_1", s.snd_1); writeStr("snd_2", s.snd_2);
        writeStr("snd_3", s.snd_3); writeStr("snd_4", s.snd_4);
        writeStr("snd_5", s.snd_5); writeStr("snd_extra", s.snd_extra);
        writeStr("snd_mvp", s.snd_mvp); writeStr("snd_win", s.snd_win);
        writeStr("snd_lose", s.snd_lose); writeStr("snd_bomb", s.snd_bomb);
        writeStr("snd_round", s.snd_round); writeStr("snd_buy", s.snd_buy);
        writeStr("snd_death", s.snd_death); writeStr("snd_gameover", s.snd_gameover);
        writeStr("snd_menu", s.snd_menu);

        std::ofstream out(path);
        if (!out.is_open()) return false;
        out << j.dump(2);
        out.close();

        std::cout << "[配置] 保存成功。" << std::endl;
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "[配置] 保存出错: " << e.what() << std::endl;
        return false;
    }
}

} // namespace config