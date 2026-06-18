#include "sound_player.h"
#include "config.h"
#include <iostream>
#include <thread>
#include <vector>
#include <filesystem>
#include <unordered_map>
#include <SDL.h>
#include <SDL_mixer.h>

namespace fs = std::filesystem;

namespace sound {

static bool s_sdlInitialized = false;

// 预加载的音效缓存（非低内存模式使用）
static std::unordered_map<int, Mix_Chunk*> s_soundMap;
static std::unordered_map<int, std::string> s_soundFileMap;

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
        std::cerr << "[音效] Mix_Init 部分格式不支持: " << Mix_GetError() << std::endl;

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
    if (!s_sdlInitialized) return;
    // 释放预加载的音效
    for (auto& [id, chunk] : s_soundMap)
        if (chunk) Mix_FreeChunk(chunk);
    s_soundMap.clear();
    s_soundFileMap.clear();

    Mix_CloseAudio();
    Mix_Quit();
    SDL_Quit();
    s_sdlInitialized = false;
}

// ----------------------------------------------------------
static int SndIdToChannel(int id) {
    switch (id) {
    case 1: case 2: case 3: case 4: case 5:
    case -1: return 0;  // Kill
    case -2: return 1;  // MVP
    case -3: case -4: return 2; // Win/Lose
    case -12: case -13: case -14: case -19: return 3; // 音乐包 (Bomb/Round/Buy/GameOver)
    case -18: return 5; // 死亡音效单独通道
    case -21: return 4; // 大厅音乐
    default: return 3;  // 兜底：防止遗漏，默认归入通道3
    }
}

// ----------------------------------------------------------
static std::wstring ResolveSndPath(int id)
{
    config::Settings cfg = config::Load();
    const std::wstring* paths[] = {
        &cfg.snd_1, &cfg.snd_2, &cfg.snd_3, &cfg.snd_4, &cfg.snd_5,
        &cfg.snd_extra, &cfg.snd_mvp, &cfg.snd_win, &cfg.snd_lose,
        &cfg.snd_bomb, &cfg.snd_round, &cfg.snd_buy, &cfg.snd_death,
        &cfg.snd_gameover, &cfg.snd_menu
    };
    int idx = -1;
    switch (id) {
    case 1: idx=0; break; case 2: idx=1; break; case 3: idx=2; break;
    case 4: idx=3; break; case 5: idx=4; break; case -1: idx=5; break;
    case -2: idx=6; break; case -3: idx=7; break; case -4: idx=8; break;
    case -12: idx=9; break; case -13: idx=10; break; case -14: idx=11; break;
    case -18: idx=12; break; case -19: idx=13; break; case -21: idx=14; break;
    default: return L"";
    }
    if (idx >= 0 && idx < 15 && !paths[idx]->empty())
        return *paths[idx];
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
// 预加载所有音效（非低内存模式用）
// ----------------------------------------------------------
void PreloadSounds()
{
    config::Settings cfg = config::Load();
    if (cfg.low_memory)
    {
        std::cout << "[音效] 低内存模式，跳过预加载。" << std::endl;
        return;
    }

    std::vector<int> ids = {1,2,3,4,5,-1};
    if (cfg.custom_musickit)
    {
        ids.push_back(-2); ids.push_back(-3); ids.push_back(-4);
        ids.push_back(-12); ids.push_back(-13); ids.push_back(-14);
        ids.push_back(-18); ids.push_back(-19); ids.push_back(-21);
    }

    for (int id : ids)
    {
        std::wstring path = ResolveSndPath(id);
        std::string pathA = fs::path(path).string();

        s_soundFileMap[id] = pathA; 

        Mix_Chunk* chunk = Mix_LoadWAV(pathA.c_str());
        if (!chunk)
        {
            std::wcout << L"[音效] 预加载失败(id=" << id << L"): " << path << std::endl;
            continue;
        }
        s_soundMap[id] = chunk;
        std::wcout << L"[音效] 预加载成功(id=" << id << L"): " << path << std::endl;
    }
    std::cout << "[音效] 预加载完成，共 " << s_soundMap.size() << " 个音效。" << std::endl;
}

// ----------------------------------------------------------
void Play(int id, float volume)
{
    config::Settings cfg = config::Load();

    // 1. 特殊处理：大厅背景音乐 (ID: -21)
    if (id == -21) {
        std::wstring path = ResolveSndPath(id);
        if (path.empty() || !fs::exists(path)) return;

        if (s_soundMap.count(-21)) {
            Mix_FreeChunk(s_soundMap[-21]);
        }

        s_soundMap[-21] = Mix_LoadWAV(fs::path(path).string().c_str());
        if (!s_soundMap[-21]) return;

        Mix_VolumeChunk(s_soundMap[-21], static_cast<int>(volume * MIX_MAX_VOLUME));
        Mix_HaltChannel(4); // 【修复问题2】大厅独立占用通道4
        Mix_PlayChannel(4, s_soundMap[-21], -1); // 通道4，无限循环
        return;
    }

    if (id == -18)
    {
        // 始终播放死亡音效
    }
    else if (id >= 1 && id <= 5 || id == -1)
    {
        if (!cfg.enable_kill_sound) return;
    }
    else if (!cfg.custom_musickit)
    {
        if (id == -2 || id == -3 || id == -4 || id == -12 || id == -13 || id == -14)
            return;
    }

    // ===== 低内存模式 =====
    if (cfg.low_memory)
    {
        std::wstring path = ResolveSndPath(id);
        if (path.empty() || !fs::exists(path)) return;

        std::string pathA = fs::path(path).string();
        Mix_Chunk* chunk = Mix_LoadWAV(pathA.c_str());
        if (!chunk) return;

        int sdlVol = static_cast<int>(volume * MIX_MAX_VOLUME);
        if (sdlVol > MIX_MAX_VOLUME) sdlVol = MIX_MAX_VOLUME;
        Mix_VolumeChunk(chunk, sdlVol);

        int channel = SndIdToChannel(id);
        if (channel >= 0) Mix_HaltChannel(channel);

        int loops = (id == -21) ? -1 : 0;
        int played = Mix_PlayChannel(channel, chunk, loops);
        if (played == -1)
        {
            Mix_FreeChunk(chunk);
        }
        else
        {
            std::wcout << L"[音效] 播放 id=" << id << L" (低内存模式)" << std::endl;
            std::thread([chunk, played]() {
                while (Mix_Playing(played))
                    SDL_Delay(6);
                Mix_FreeChunk(chunk);
                }).detach();
        }
        return;
    }

    // ===== 正常模式：使用预加载缓存 =====
    auto it = s_soundMap.find(id);
    if (it == s_soundMap.end() || !it->second)
    {
        std::wcout << L"[音效] 未预加载(id=" << id << L")" << std::endl;
        return;
    }

    Mix_Chunk* chunk = it->second;
    int sdlVol = static_cast<int>(volume * MIX_MAX_VOLUME);
    if (sdlVol > MIX_MAX_VOLUME) sdlVol = MIX_MAX_VOLUME;
    Mix_VolumeChunk(chunk, sdlVol);

    int channel = SndIdToChannel(id);
    if (channel >= 0) Mix_HaltChannel(channel);

    int loops = (id == -21) ? -1 : 0;
    int played = Mix_PlayChannel(channel, chunk, loops);
    if (played == -1)
    {
        std::cerr << "[音效] 播放失败: " << Mix_GetError() << std::endl;
    }
    else
    {
        std::wcout << L"[音效] 播放 id=" << id << L" 通道=" << played << std::endl;
    }
}

} // namespace sound
