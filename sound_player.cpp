#include "sound_player.h"
#include "config.h"
#include <iostream>
#include <filesystem>
#include <SDL.h>
#include <SDL_mixer.h>

namespace fs = std::filesystem;

namespace sound {

static bool s_sdlInitialized = false;

// ----------------------------------------------------------
bool Init()
{
    if (s_sdlInitialized) return true;

    if (SDL_Init(SDL_INIT_AUDIO) < 0)
    {
        std::cerr << "[音效] SDL_Init 失败: " << SDL_GetError() << std::endl;
        return false;
    }

    int mixFlags = MIX_INIT_OGG | MIX_INIT_MP3;
    int init = Mix_Init(mixFlags);
    if ((init & mixFlags) != mixFlags)
    {
        std::cerr << "[音效] Mix_Init 失败: " << Mix_GetError() << std::endl;
        // 继续，可能部分格式不支持
    }

    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096) == -1)
    {
        std::cerr << "[音效] Mix_OpenAudio 失败: " << Mix_GetError() << std::endl;
        return false;
    }

    Mix_AllocateChannels(8);
    std::cout << "[音效] SDL_mixer 初始化成功。" << std::endl;
    s_sdlInitialized = true;
    return true;
}

// ----------------------------------------------------------
void Quit()
{
    if (s_sdlInitialized)
    {
        Mix_CloseAudio();
        Mix_Quit();
        SDL_Quit();
        s_sdlInitialized = false;
    }
}

// ----------------------------------------------------------
static int SndIdToChannel(int id)
{
    switch (id) {
    case 1: case 2: case 3: case 4: case 5:
    case -1: return 0; // kill channel
    case -2: return 1; // MVP
    case -3: case -4: return 2; // win/lose
    case -12: return 3; // bomb
    default: return -1; // auto
    }
}

// ----------------------------------------------------------
static std::wstring ResolveSndPath(int id)
{
    config::Settings cfg = config::Load();

    // 优先使用自定义路径
    const std::wstring* paths[] = {
        &cfg.snd_1, &cfg.snd_2, &cfg.snd_3, &cfg.snd_4, &cfg.snd_5,
        &cfg.snd_extra, &cfg.snd_mvp, &cfg.snd_win, &cfg.snd_lose,
        &cfg.snd_bomb, &cfg.snd_round, &cfg.snd_buy, &cfg.snd_death,
        &cfg.snd_gameover, &cfg.snd_menu
    };
    int idx = -1;
    switch (id) {
    case 1: idx = 0; break; case 2: idx = 1; break;
    case 3: idx = 2; break; case 4: idx = 3; break;
    case 5: idx = 4; break; case -1: idx = 5; break;
    case -2: idx = 6; break; case -3: idx = 7; break;
    case -4: idx = 8; break; case -12: idx = 9; break;
    case -13: idx = 10; break; case -14: idx = 11; break;
    case -18: idx = 12; break; case -19: idx = 13; break;
    case -21: idx = 14; break;
    default: return L"";
    }
    if (idx >= 0 && idx < 15 && !paths[idx]->empty())
        return *paths[idx];

    // 使用默认路径
    const wchar_t* names[] = {
        L"1", L"2", L"3", L"4", L"5", L"deathmatch",
        L"mvp", L"win", L"lose", L"bomb", L"round",
        L"buy", L"death", L"gameover", L"menu"
    };
    if (idx >= 0 && idx < 15)
        return config::GetDefaultSndPath(names[idx], cfg.ogg);
    return L"";
}

// ----------------------------------------------------------
void Play(int id, float volume)
{
    config::Settings cfg = config::Load();
    if (!cfg.enable_kill_sound) return;

    std::wstring path = ResolveSndPath(id);
    if (path.empty() || !fs::exists(path))
    {
        std::wcout << L"[音效] 文件不存在(id=" << id << L"): " << path << std::endl;
        return;
    }

    // 转换到 UTF-8 用于 SDL_mixer
    std::string pathA = std::filesystem::path(path).string();

    Mix_Chunk* chunk = Mix_LoadWAV(pathA.c_str());
    if (!chunk)
    {
        std::cerr << "[音效] 加载失败 " << pathA << ": " << Mix_GetError() << std::endl;
        return;
    }

    int vol = static_cast<int>(volume * MIX_MAX_VOLUME);
    if (vol > MIX_MAX_VOLUME) vol = MIX_MAX_VOLUME;
    Mix_VolumeChunk(chunk, vol);

    int channel = SndIdToChannel(id);
    if (channel >= 0) Mix_HaltChannel(channel);

    int played = Mix_PlayChannel(channel, chunk, 0);
    if (played == -1)
    {
        std::cerr << "[音效] 播放失败: " << Mix_GetError() << std::endl;
        Mix_FreeChunk(chunk);
    }
    else
    {
        std::wcout << L"[音效] 播放 id=" << id << L" (" << path << L") 通道=" << played << std::endl;
    }
}

} // namespace sound