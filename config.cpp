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

        std::cout << "[配置] 从 " << path.string() << " 加载配置成功。" << std::endl;
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

    // 确保目录存在
    std::error_code ec;
    if (!fs::exists(dir))
        fs::create_directories(dir, ec);

    fs::path path = dir / L"gsi.json";

    try {
        nlohmann::json j;

        // 添加注释说明（JSON 标准不支持注释，但作为数据字段保留）
        j["__comments"] = nlohmann::json::object();
        j["__comments"]["match"] = "新版本已弃用该选项，请保持true";
        j["__comments"]["vol"] = "播放音量，取值范围0.0~1.0";
        j["__comments"]["ogg"] = "音频格式是否使用 .ogg格式";
        j["__comments"]["custom_musickit"] = "是否启用自定义音乐包逻辑";
        j["__comments"]["custom_flashbang"] = "是否启用闪光叠加页面";
        j["__comments"]["low_memory"] = "是否开启低内存模式";

        j["match"] = s.match;
        j["vol"] = s.volume;
        j["ogg"] = s.ogg;
        j["custom_musickit"] = s.custom_musickit;
        j["custom_flashbang"] = s.custom_flashbang;
        j["low_memory"] = s.low_memory;
        j["show_mvp"] = s.show_mvp;

        std::ofstream out(path);
        if (!out.is_open()) return false;

        out << j.dump(2);
        out.close();

        std::cout << "[配置] 保存配置到 " << path.string() << std::endl;
        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[配置] 保存配置出错: " << e.what() << std::endl;
        return false;
    }
}

} // namespace config