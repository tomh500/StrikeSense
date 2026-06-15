#pragma once

#include <string>
#include <filesystem>

namespace fs = std::filesystem;

// ============================================================
// 配置管理 — namespace config
//
// 配置文件路径: %UserProfile%/StrikeSense/setting/gsi.json
// ============================================================

namespace config {

struct SoundFile {
    std::wstring path;  // 用户自定义路径（空=默认位置）
};

// ----------------------------------------------------------
struct Settings {
    bool match = true;
    float volume = 0.88f;
    bool ogg = false;
    bool custom_musickit = false;
    bool custom_flashbang = false;
    bool low_memory = false;
    bool show_mvp = false;
    bool enable_kill_sound = false;  // 是否开启击杀音效替换

    // 音效文件路径（空=使用默认位置 %UserProfile%/StrikeSense/Snd/）
    std::wstring snd_1;    // 一杀
    std::wstring snd_2;    // 二杀
    std::wstring snd_3;    // 三杀
    std::wstring snd_4;    // 四杀
    std::wstring snd_5;    // 五杀
    std::wstring snd_extra;    // 多杀/死斗
    std::wstring snd_mvp;      // MVP（自定义音乐包）
    std::wstring snd_win;      // 胜利
    std::wstring snd_lose;     // 失败
    std::wstring snd_bomb;     // 炸弹
    std::wstring snd_round;    // 回合开始
    std::wstring snd_buy;      // 购买
    std::wstring snd_death;    // 死亡
    std::wstring snd_gameover; // 游戏结束
    std::wstring snd_menu;     // 菜单
};

// ----------------------------------------------------------
std::wstring GetConfigDir();
std::wstring GetConfigPath();
Settings Load();
bool Save(const Settings& s);

// ----------------------------------------------------------
// 获取默认音效目录 %UserProfile%/StrikeSense/Snd/
// ----------------------------------------------------------
std::wstring GetDefaultSndDir();

// ----------------------------------------------------------
// 根据 id 和 ogg 设置获取默认音效文件路径
// ----------------------------------------------------------
std::wstring GetDefaultSndPath(const std::wstring& name, bool useOgg);

} // namespace config