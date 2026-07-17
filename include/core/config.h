#pragma once

#include <string>
#include <filesystem>

namespace fs = std::filesystem;

namespace config {

struct Settings {
    bool match = true;
    float volume = 0.88f;
    bool ogg = false;
    bool custom_musickit = false;
    bool force_interrupt = false;
    bool custom_flashbang = false;
    bool low_memory = false;
    bool show_mvp = false;
    bool enable_kill_sound = false;
    int close_behavior = 0; // 0=询问, 1=隐藏到托盘, 2=关闭程序


    std::wstring snd_1, snd_2, snd_3, snd_4, snd_5;
    std::wstring snd_extra;
    std::wstring snd_mvp, snd_win, snd_lose, snd_bomb;
    std::wstring snd_round, snd_buy, snd_death, snd_gameover, snd_menu;
    std::wstring flash_image;
    std::wstring snd_lastsec;
    
};

void EnsureDirectoriesExist();
std::wstring GetConfigDir();
std::wstring GetConfigPath();
std::wstring GetDefaultSndDir();
std::wstring GetDefaultSndPath(const std::wstring& name, bool useOgg);

// gsi.json + sound_config.json 完整加载/保存
Settings Load();
bool Save(const Settings& s);

// 音效配置文件独立读写
void LoadSoundConfig(Settings& s);
void SaveSoundConfig(const Settings& s);

extern Settings g_cache;
void InvalidateCache();

std::wstring GetDefaultImgDir();
std::wstring GetDefaultFlashPath();

} // namespace config
