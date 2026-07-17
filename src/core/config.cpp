    #include "config.h"
    #include <windows.h>
    #include <algorithm>
    #include <iostream>
    #include <fstream>
    #include <nlohmann/json.hpp>

    namespace config {

    Settings g_cache;
    static bool s_cacheValid = false;

    void InvalidateCache() { s_cacheValid = false; }

    std::wstring GetDefaultImgDir()
{
    wchar_t profile[MAX_PATH] = {};
    GetEnvironmentVariableW(L"USERPROFILE", profile, MAX_PATH);

    return (
        fs::path(profile)
        / L"StrikeSense"
        / L"img"
    ).wstring();
}
void EnsureDirectory(const fs::path& p)
{
    std::error_code ec;

    if (fs::create_directories(p, ec))
    {
        std::wcout << L"[创建目录] " << p.wstring() << std::endl;
    }
    else if (ec)
    {
        std::wcout << L"[创建失败] "
                   << p.wstring()
                   << L" err="
                   << ec.message().c_str()
                   << std::endl;
    }
}

    std::wstring GetDefaultFlashPath()
{
    return (
        fs::path(GetDefaultImgDir())
        / L"flash.jpg"
    ).wstring();
}

    void EnsureDirectoriesExist()
    {
        wchar_t profile[MAX_PATH] = {};
        GetEnvironmentVariableW(L"USERPROFILE", profile, MAX_PATH);
        fs::path base = fs::path(profile) / L"StrikeSense";
        EnsureDirectory(base);
        EnsureDirectory(base / L"setting");
        EnsureDirectory(base / L"snd");
        EnsureDirectory(base / L"img");
        EnsureDirectory(base / L"itemhelper");
        EnsureDirectory(base / L"itemhelper" / L"de_dust2");
        EnsureDirectory(base / L"itemhelper" / L"de_inferno");
        EnsureDirectory(base / L"itemhelper" / L"de_mirage");
        EnsureDirectory(base / L"itemhelper" / L"de_nuke");
        EnsureDirectory(base / L"itemhelper" / L"de_overpass");
        EnsureDirectory(base / L"itemhelper" / L"de_vertigo");
        EnsureDirectory(base / L"itemhelper" / L"de_ancient");
        EnsureDirectory(base / L"itemhelper" / L"de_train");
        EnsureDirectory(base / L"itemhelper" / L"de_cache");
        EnsureDirectory(base / L"itemhelper" / L"cs_office");
        EnsureDirectory(base / L"itemhelper" / L"othermaps");
        std::cout << "[配置] 目录已创建: " << base.string() << std::endl;
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
        s = Settings();
        fs::path path(GetConfigPath());

        if (!fs::exists(path))
        {
            std::cout << "[配置] 创建默认 gsi.json" << std::endl;
            s.flash_image = GetDefaultFlashPath();
            s.snd_lastsec = GetDefaultSndPath(L"lastsec", s.ogg);
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

            // 只有这些字段，其他字段（snd_*）在 sound_config.json
            if (j.contains("enable_kill_sound") && j["enable_kill_sound"].is_boolean())
                s.enable_kill_sound = j["enable_kill_sound"];
            else
                s.enable_kill_sound = false;

            if (j.contains("vol") && j["vol"].is_number())
                s.volume = j["vol"].get<float>();

            if (j.contains("ogg") && j["ogg"].is_boolean())
                s.ogg = j["ogg"];

            if (j.contains("custom_musickit") && j["custom_musickit"].is_boolean())
                s.custom_musickit = j["custom_musickit"];

            if (j.contains("force_interrupt") && j["force_interrupt"].is_boolean())
                s.force_interrupt = j["force_interrupt"];

            if (j.contains("custom_flashbang") && j["custom_flashbang"].is_boolean())
                s.custom_flashbang = j["custom_flashbang"];

            if (j.contains("low_memory") && j["low_memory"].is_boolean())
                s.low_memory = j["low_memory"];

            if (j.contains("show_mvp") && j["show_mvp"].is_boolean())
                s.show_mvp = j["show_mvp"];

            if (j.contains("close_behavior") && j["close_behavior"].is_number_integer())
                s.close_behavior = std::clamp(j["close_behavior"].get<int>(), 0, 2);

            std::cout << "[配置] gsi.json 加载成功。" << std::endl;

            // 然后加载音效配置
            LoadSoundConfig(s);
        }
        catch (const std::exception& e) {
            std::cerr << "[配置] 加载 gsi.json 出错: " << e.what() << std::endl;
        }

        s_cacheValid = true;
        return s;
    }

    bool Save(const Settings& s)
    {
        g_cache = s;
        s_cacheValid = true;
        EnsureDirectoriesExist();

        // ======= 保存 gsi.json（原版格式） =======
        fs::path path(GetConfigPath());
        try {
            nlohmann::json j;

            // 先读取已有文件保留 __comments
            if (fs::exists(path))
            {
                try {
                    std::ifstream in(path);
                    if (in.is_open()) {
                        in >> j;
                        in.close();
                    }
                }
                catch (...) {}
            }

            // 写入/覆盖字段
            j["match"] = true;
            j["vol"] = s.volume;
            j["ogg"] = s.ogg;
            j["custom_musickit"] = s.custom_musickit;
            j["force_interrupt"] = s.force_interrupt;
            j["custom_flashbang"] = s.custom_flashbang;
            j["low_memory"] = s.low_memory;
            j["show_mvp"] = s.show_mvp;
            j["enable_kill_sound"] = s.enable_kill_sound;
            j["close_behavior"] = s.close_behavior;

            // 保留 __comments
            if (!j.contains("__comments"))
            {
                j["__comments"] = nlohmann::json::object();
                j["__comments"]["tips"] = "这里是配置文件的说明，帮助你快速了解每个选项的作用";
                j["__comments"]["match"] = "新版本已弃用该选项，请保持true";
                j["__comments"]["mvp"] = "新版本已弃用该选项，请保持true";
                j["__comments"]["vol"] = "播放音量，取值范围vol∈(0.0 , 1.0]";
                j["__comments"]["ogg"] = "音频格式是否使用 .ogg格式";
                j["__comments"]["custom_musickit"] = "是否启用自定义音乐包逻辑";
                j["__comments"]["force_interrupt"] = "是否在新阶段开始时强制打断当前自定义音乐盒通道，避免回合结束、MVP、安包等音效与下一阶段重叠";
                j["__comments"]["custom_flashbang"] = "是否启用闪光叠加页面";
                j["__comments"]["low_memory"] = "是否开启低内存模式";
                j["__comments"]["show_mvp"] = "是否展示MVP信息板";
                j["__comments"]["enable_kill_sound"] = "是否开启击杀音效替换";
                j["__comments"]["close_behavior"] = "关闭主窗口时的行为：0=询问，1=隐藏到托盘，2=关闭程序";
            }

            std::ofstream out(path);
            if (out.is_open()) {
                out << j.dump(2);
                out.close();
            }
        }
        catch (...) {}

        // ======= 保存音效配置到独立文件 =======
        SaveSoundConfig(s);

        std::cout << "[配置] 保存成功。" << std::endl;
        return true;
    }

    // ============================================================
    // 音效配置文件独立管理
    // ============================================================

    static std::wstring GetSoundConfigPath()
    {
        return (fs::path(GetConfigDir()) / L"sound_config.json").wstring();
    }

    // 字段对照表
    struct SoundField {
        const char* key;
        std::wstring Settings::*ptr;
    };

    static const SoundField s_soundFields[] = {
        {"snd_1", &Settings::snd_1}, {"snd_2", &Settings::snd_2},
        {"snd_3", &Settings::snd_3}, {"snd_4", &Settings::snd_4},
        {"snd_5", &Settings::snd_5}, {"snd_extra", &Settings::snd_extra},
        {"snd_mvp", &Settings::snd_mvp}, {"snd_win", &Settings::snd_win},
        {"snd_lose", &Settings::snd_lose}, {"snd_bomb", &Settings::snd_bomb},
        {"snd_round", &Settings::snd_round}, {"snd_buy", &Settings::snd_buy},
        {"snd_death", &Settings::snd_death}, {"snd_gameover", &Settings::snd_gameover},
        {"snd_menu", &Settings::snd_menu},
        {"snd_lastsec", &Settings::snd_lastsec},
        {"flash_image", &Settings::flash_image}
    };

    // 替换原有的 LoadSoundConfig 函数
    void LoadSoundConfig(Settings& s)
    {
        fs::path path(GetSoundConfigPath());
        if (!fs::exists(path))
        {
            std::cout << "[音效配置] 创建默认 sound_config.json" << std::endl;

            if (s.flash_image.empty())
                s.flash_image = GetDefaultFlashPath();

            // ======= 新增：生成默认十秒倒计时路径 =======
            if (s.snd_lastsec.empty())
                s.snd_lastsec = GetDefaultSndPath(L"lastsec", s.ogg);

            SaveSoundConfig(s);
            return;
        }

        try {
            std::ifstream in(path);
            if (!in.is_open()) return;

            nlohmann::json j;
            in >> j;
            in.close();

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

            for (const auto& f : s_soundFields)
                readStr(f.key, s.*(f.ptr));

            // ======= 新增：向下兼容旧配置文件升级 =======
            if (s.snd_lastsec.empty()) {
                s.snd_lastsec = GetDefaultSndPath(L"lastsec", s.ogg);
                std::cout << "[音效配置] 检测到旧版本配置文件，已补全默认十秒倒计时音效。" << std::endl;
            }

            std::cout << "[音效配置] 加载成功。" << std::endl;
        }
        catch (const std::exception& e) {
            std::cerr << "[音效配置] 加载出错: " << e.what() << std::endl;
        }
    }

    void SaveSoundConfig(const Settings& s)
    {
        fs::path path(GetSoundConfigPath());
        try {
            nlohmann::json j;

            auto writeStr = [&](const std::string& key, const std::wstring& val) {
                if (val.empty()) { j[key] = ""; return; }
                int u8Len = WideCharToMultiByte(CP_UTF8, 0, val.c_str(), (int)val.length(), nullptr, 0, nullptr, nullptr);
                if (u8Len > 0) {
                    std::string u8(u8Len, '\0');
                    WideCharToMultiByte(CP_UTF8, 0, val.c_str(), (int)val.length(), &u8[0], u8Len, nullptr, nullptr);
                    j[key] = u8;
                }
            };

            for (const auto& f : s_soundFields)
                writeStr(f.key, s.*(f.ptr));

            std::ofstream out(path);
            if (out.is_open()) {
                out << j.dump(2);
                out.close();
            }
        }
        catch (...) {}
    }

    } // namespace config
