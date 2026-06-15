#include "sound_player.h"
#include "config.h"
#include <iostream>
#include <filesystem>
#include <windows.h>

namespace fs = std::filesystem;

namespace sound {

// ----------------------------------------------------------
// 根据 id 获取音效文件名（不含扩展名）
// ----------------------------------------------------------
static std::wstring GetSndFileNameById(int id)
{
    switch (id)
    {
    case 1:   return L"1";
    case 2:   return L"2";
    case 3:   return L"3";
    case 4:   return L"4";
    case 5:   return L"5";
    case -1:  return L"deathmatch";  // 多杀/死斗
    case -2:  return L"mvp";
    case -3:  return L"win";
    case -4:  return L"lose";
    case -12: return L"bomb";
    case -13: return L"round";
    case -14: return L"buy";
    case -18: return L"death";
    case -19: return L"gameover";
    case -21: return L"menu";
    default:
        std::cout << "[音效] 未知音效 id=" << id << std::endl;
        return L"";
    }
}

// ----------------------------------------------------------
// 根据 id 获取配置中的自定义路径字段名
// ----------------------------------------------------------
static std::string GetSndConfigKey(int id)
{
    switch (id)
    {
    case 1:   return "snd_1";
    case 2:   return "snd_2";
    case 3:   return "snd_3";
    case 4:   return "snd_4";
    case 5:   return "snd_5";
    case -1:  return "snd_extra";
    case -2:  return "snd_mvp";
    case -3:  return "snd_win";
    case -4:  return "snd_lose";
    case -12: return "snd_bomb";
    case -13: return "snd_round";
    case -14: return "snd_buy";
    case -18: return "snd_death";
    case -19: return "snd_gameover";
    case -21: return "snd_menu";
    default:  return "";
    }
}

// ----------------------------------------------------------
// 从配置中获取音效文件的实际路径
// ----------------------------------------------------------
static std::wstring ResolveSndPath(int id, const config::Settings& cfg)
{
    // 1: 查找配置中的自定义路径
    const std::wstring* customPaths[] = {
        &cfg.snd_1, &cfg.snd_2, &cfg.snd_3, &cfg.snd_4, &cfg.snd_5,
        &cfg.snd_extra, &cfg.snd_mvp, &cfg.snd_win, &cfg.snd_lose,
        &cfg.snd_bomb, &cfg.snd_round, &cfg.snd_buy, &cfg.snd_death,
        &cfg.snd_gameover, &cfg.snd_menu
    };
    int customIdx = 0;
    switch (id) {
    case 1: customIdx = 0; break; case 2: customIdx = 1; break;
    case 3: customIdx = 2; break; case 4: customIdx = 3; break;
    case 5: customIdx = 4; break; case -1: customIdx = 5; break;
    case -2: customIdx = 6; break; case -3: customIdx = 7; break;
    case -4: customIdx = 8; break; case -12: customIdx = 9; break;
    case -13: customIdx = 10; break; case -14: customIdx = 11; break;
    case -18: customIdx = 12; break; case -19: customIdx = 13; break;
    case -21: customIdx = 14; break;
    default: return L"";
    }

    if (!customPaths[customIdx]->empty())
    {
        std::wcout << L"[音效] 使用自定义路径: " << *customPaths[customIdx] << std::endl;
        return *customPaths[customIdx];
    }

    // 2: 使用默认位置
    std::wstring name = GetSndFileNameById(id);
    if (name.empty()) return L"";
    return config::GetDefaultSndPath(name, cfg.ogg);
}

// ----------------------------------------------------------
void Play(int id, float volume)
{
    config::Settings cfg = config::Load();

    if (!cfg.enable_kill_sound)
    {
        std::cout << "[音效] 击杀音效替换已关闭，跳过 id=" << id << std::endl;
        return;
    }

    std::wstring path = ResolveSndPath(id, cfg);
    if (path.empty())
    {
        std::wcout << L"[音效] 找不到音效文件(id=" << id << L")" << std::endl;
        return;
    }

    if (!fs::exists(path))
    {
        std::wcout << L"[音效] 文件不存在: " << path << std::endl;
        return;
    }

    // 用 PlaySound 播放（Win32 API 支持 wav）
    // 对于 ogg 文件，这里仅提示（需要 SDL_mixer 或其他库）
    std::wstring ext = path.substr(path.find_last_of(L'.'));
    if (ext == L".wav" || ext == L".WAV")
    {
        bool ok = PlaySoundW(path.c_str(), nullptr, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
        if (ok)
            std::wcout << L"[音效] 播放: " << path << std::endl;
        else
            std::wcout << L"[音效] 播放失败: " << path << L" (错误: " << GetLastError() << L")" << std::endl;
    }
    else if (ext == L".ogg" || ext == L".OGG")
    {
        std::wcout << L"[音效] OGG 文件需要 SDL_mixer: " << path << std::endl;
        std::cout << "[音效] 提示：你可以在设置中选择 WAV 格式，或后续集成 SDL_mixer 播放 OGG。" << std::endl;
        // 尝试用系统默认播放器打开（备用）
        ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_HIDE);
    }
    else
    {
        std::wcout << L"[音效] 不支持的格式: " << ext << std::endl;
    }
}

} // namespace sound