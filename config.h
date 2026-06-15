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
    bool custom_flashbang = false;
    bool low_memory = false;
    bool show_mvp = false;
    bool enable_kill_sound = false;

    std::wstring snd_1, snd_2, snd_3, snd_4, snd_5;
    std::wstring snd_extra;
    std::wstring snd_mvp, snd_win, snd_lose, snd_bomb;
    std::wstring snd_round, snd_buy, snd_death, snd_gameover, snd_menu;
};

void EnsureDirectoriesExist();
std::wstring GetConfigDir();
std::wstring GetConfigPath();
Settings Load();
bool Save(const Settings& s);
std::wstring GetDefaultSndDir();
std::wstring GetDefaultSndPath(const std::wstring& name, bool useOgg);

} // namespace config