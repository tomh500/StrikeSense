#include "config.h"
#include <windows.h>
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>

namespace config {

// ----------------------------------------------------------
std::wstring GetConfigDir()
{
    wchar_t profile[MAX_PATH] = {};
    GetEnvironmentVariableW(L"USERPROFILE", profile, MAX_PATH);
    fs::path dir = fs::path(profile) / L"StrikeSense" / L"setting";
    return dir.wstring();
}

// ----------------------------------------------------------
std::wstring GetConfigPath()
{
    fs::path dir(GetConfigDir());
    return (dir / L"gsi.json").wstring();
}

// ----------------------------------------------------------
std::wstring GetDefaultSndDir()
{
    wchar_t profile[MAX_PATH] = {};
    GetEnvironmentVariableW(L"USERPROFILE", profile, MAX_PATH);
    return (fs::path(profile) / L"StrikeSense" / L"Snd").wstring();
}

// ----------------------------------------------------------
std::wstring GetDefaultSndPath(const std::wstring& name, bool useOgg)
{
    fs::path sndDir(GetDefaultSndDir());
    std::wstring ext = useOgg ? L".ogg" : L".wav";
    return (sndDir / (name + ext)).wstring();
}

// ----------------------------------------------------------
Settings Load()
{
    Settings s;
    fs::path path(GetConfigPath());

    if (!fs::exists(path))
    {
        std::cout << "[配置] 配置文件不存在，使用默认值。" << std::endl;
        return s;
    }

    try {
        std::ifstream in(path);
        if (!in.is_open()) return s;

        nlohmann::json j;
        in >> j;
        in.close();

        if (j.contains("match") && j["match"].is_boolean()) s.match = j["match"];
        if (j.contains("vol") && j["vol"].is_number_float()) s.volume = j["vol"];
        if (j.contains("ogg") && j["ogg"].is_boolean()) s.ogg = j["ogg"];
        if (j.contains("custom_musickit") && j["custom_musickit"].is_boolean()) s.custom_musickit = j["custom_musickit"];
        if (j.contains("custom_flashbang") && j["custom_flashbang"].is_boolean()) s.custom_flashbang = j["custom_flashbang"];
        if (j.contains("low_memory") && j["low_memory"].is_boolean()) s.low_memory = j["low_memory"];
        if (j.contains("show_mvp") && j["show_mvp"].is_boolean()) s.show_mvp = j["show_mvp"];
        if (j.contains("enable_kill_sound") && j["enable_kill_sound"].is_boolean()) s.enable_kill_sound = j["enable_kill_sound"];

        // 读取音效文件路径
        auto readPath = [&](const std::string& key, std::wstring& target) {
            if (j.contains(key) && j[key].is_string()) {
                std::string u8 = j[key];
                int wlen = MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), (int)u8.length(), nullptr, 0);
                if (wlen > 0) {
                    target.resize(wlen);
                    MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), (int)u8.length(), &target[0], wlen);
                }
            }
        };
        readPath("snd_1", s.snd_1);
        readPath("snd_2", s.snd_2);
        readPath("snd_3", s.snd_3);
        readPath("snd_4", s.snd_4);
        readPath("snd_5", s.snd_5);
        readPath("snd_extra", s.snd_extra);
        readPath("snd_mvp", s.snd_mvp);
        readPath("snd_win", s.snd_win);
        readPath("snd_lose", s.snd_lose);
        readPath("snd_bomb", s.snd_bomb);
        readPath("snd_round", s.snd_round);
        readPath("snd_buy", s.snd_buy);
        readPath("snd_death", s.snd_death);
        readPath("snd_gameover", s.snd_gameover);
        readPath("snd_menu", s.snd_menu);

        std::cout << "[配置] 加载配置成功。" << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[配置] 加载配置出错: " << e.what() << std::endl;
    }

    return s;
}

// ----------------------------------------------------------
bool Save(const Settings& s)
{
    fs::path dir(GetConfigDir());

    std::error_code ec;
    if (!fs::exists(dir))
        fs::create_directories(dir, ec);

    fs::path path = dir / L"gsi.json";

    try {
        nlohmann::json j;

        j["__comments"] = nlohmann::json::object();
        j["__comments"]["match"] = "新版本已弃用该选项，请保持true";
        j["__comments"]["vol"] = "播放音量，取值范围0.0~1.0";
        j["__comments"]["ogg"] = "音频格式是否使用 .ogg格式";
        j["__comments"]["custom_musickit"] = "是否启用自定义音乐包逻辑";
        j["__comments"]["custom_flashbang"] = "是否启用闪光叠加页面";
        j["__comments"]["low_memory"] = "是否开启低内存模式";
        j["__comments"]["enable_kill_sound"] = "是否开启击杀音效替换";

        j["match"] = s.match;
        j["vol"] = s.volume;
        j["ogg"] = s.ogg;
        j["custom_musickit"] = s.custom_musickit;
        j["custom_flashbang"] = s.custom_flashbang;
        j["low_memory"] = s.low_memory;
        j["show_mvp"] = s.show_mvp;
        j["enable_kill_sound"] = s.enable_kill_sound;

        // 保存音效路径
        auto writePath = [&](const std::string& key, const std::wstring& val) {
            if (val.empty()) return;
            int u8Len = WideCharToMultiByte(CP_UTF8, 0, val.c_str(), (int)val.length(),
                                            nullptr, 0, nullptr, nullptr);
            if (u8Len > 0) {
                std::string u8(u8Len, '\0');
                WideCharToMultiByte(CP_UTF8, 0, val.c_str(), (int)val.length(),
                                    &u8[0], u8Len, nullptr, nullptr);
                j[key] = u8;
            }
        };
        writePath("snd_1", s.snd_1);
        writePath("snd_2", s.snd_2);
        writePath("snd_3", s.snd_3);
        writePath("snd_4", s.snd_4);
        writePath("snd_5", s.snd_5);
        writePath("snd_extra", s.snd_extra);
        writePath("snd_mvp", s.snd_mvp);
        writePath("snd_win", s.snd_win);
        writePath("snd_lose", s.snd_lose);
        writePath("snd_bomb", s.snd_bomb);
        writePath("snd_round", s.snd_round);
        writePath("snd_buy", s.snd_buy);
        writePath("snd_death", s.snd_death);
        writePath("snd_gameover", s.snd_gameover);
        writePath("snd_menu", s.snd_menu);

        std::ofstream out(path);
        if (!out.is_open()) return false;

        out << j.dump(2);
        out.close();

        std::cout << "[配置] 保存配置成功。" << std::endl;
        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[配置] 保存配置出错: " << e.what() << std::endl;
        return false;
    }
}

} // namespace config