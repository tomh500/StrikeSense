#include "gsi_server.h"
#include "config.h"
#include "sound_player.h"
#include <iostream>
#include <thread>
#include <atomic>
#include <winsock2.h>
#include <httplib.h>
#include <nlohmann/json.hpp>

namespace gsi {

int g_debug = 1;

static httplib::Server* s_server = nullptr;
static std::thread s_serverThread;
static std::atomic<bool> s_running{ false };
static bool s_wsaInitialized = false;

// GSI 事件状态
static std::string s_lastPhase;
static int s_lastKills = 0;

// ----------------------------------------------------------
static void OnGSIRequest(const httplib::Request& req, httplib::Response& res)
{
    std::string rawJson = req.body;

    if (rawJson.empty())
    {
        res.status = 200;
        res.set_content("OK", "text/plain");
        return;
    }

    if (g_debug)
    {
        std::cout << "======== GSI 原始 JSON 开始 ========" << std::endl;
        std::cout << rawJson << std::endl;
        std::cout << "======== GSI 原始 JSON 结束 ========" << std::endl;
    }

    try {
        nlohmann::json j = nlohmann::json::parse(rawJson);
        config::Settings cfg = config::Load();

        // 提取字段
        int roundKills = 0;
        std::string phase;
        std::string activity;
        std::string mapMode = "competitive";
        int health = 100;

        if (j.contains("player") && j["player"].is_object())
        {
            auto& player = j["player"];
            if (player.contains("state") && player["state"].is_object())
            {
                auto& state = player["state"];
                if (state.contains("round_kills") && state["round_kills"].is_number())
                    roundKills = state["round_kills"].get<int>();
                if (state.contains("health") && state["health"].is_number())
                    health = state["health"].get<int>();
            }
            if (player.contains("activity") && player["activity"].is_string())
                activity = player["activity"].get<std::string>();
        }

        if (j.contains("round") && j["round"].is_object())
        {
            auto& round = j["round"];
            if (round.contains("phase") && round["phase"].is_string())
                phase = round["phase"].get<std::string>();
        }

        if (j.contains("map") && j["map"].is_object())
        {
            auto& map = j["map"];
            if (map.contains("mode") && map["mode"].is_string())
                mapMode = map["mode"].get<std::string>();
        }

        if (g_debug)
        {
            std::cout << "[GSI] phase=" << phase << " act=" << activity
                      << " kills=" << roundKills << " last=" << s_lastKills
                      << " hp=" << health << std::endl;
        }

        // ===== 回合开始重置 =====
        if (phase == "live" && s_lastPhase != "live")
        {
            std::cout << "[GSI] 回合开始，重置击杀计数" << std::endl;
            s_lastKills = 0;
            if (cfg.custom_musickit && cfg.enable_kill_sound)
                sound::Play(-13, cfg.volume);
        }

        // ===== 击杀判定（适用于 live 和 over 阶段） =====
        // 注意：round_kills 在回合结束后仍然存在，所以我们在 over 阶段也能拿到击杀数
        if (cfg.enable_kill_sound && roundKills > s_lastKills)
        {
            for (int k = s_lastKills + 1; k <= roundKills; ++k)
            {
                if (k > 5)
                {
                    std::cout << "[GSI] 超过五杀(" << k << ")，播放 deathmatch" << std::endl;
                    sound::Play(-1, cfg.volume);
                }
                else
                {
                    std::cout << "[GSI] " << k << "杀! 播放音效" << std::endl;
                    sound::Play(k, cfg.volume);
                }
            }
            s_lastKills = roundKills;
        }

        // ===== 死亡判定 =====
        if (cfg.enable_kill_sound && phase == "live" && health <= 0 && activity == "playing")
        {
            std::cout << "[GSI] 玩家死亡" << std::endl;
            sound::Play(-18, cfg.volume);
        }

        // ===== 死斗模式 =====
        if (cfg.enable_kill_sound && mapMode == "deathmatch" && roundKills > s_lastKills)
        {
            std::cout << "[GSI] 死斗击杀" << std::endl;
            sound::Play(-1, cfg.volume);
            s_lastKills = roundKills;
        }

        // ===== 购买阶段 =====
        if (phase == "freezetime" && s_lastPhase != "freezetime")
        {
            if (cfg.custom_musickit && cfg.enable_kill_sound)
                sound::Play(-14, cfg.volume);
        }

        s_lastPhase = phase;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[GSI] 处理错误: " << e.what() << std::endl;
    }

    res.status = 200;
    res.set_content("OK", "text/plain");
}

// ----------------------------------------------------------
bool Initialize()
{
    if (s_wsaInitialized) return true;
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cerr << "[GSI] WSAStartup 失败" << std::endl;
        return false;
    }
    s_wsaInitialized = true;
    return true;
}

void Cleanup()
{
    if (s_wsaInitialized) { WSACleanup(); s_wsaInitialized = false; }
}

bool StartServer()
{
    if (s_running) return true;
    if (!s_wsaInitialized && !Initialize()) return false;

    s_server = new httplib::Server();
    s_server->Post("/", OnGSIRequest);

    std::cout << "[GSI] 服务器启动，监听 127.0.0.1:1009" << std::endl;

    s_running = true;
    s_serverThread = std::thread([]() {
        if (!s_server->listen("127.0.0.1", 1009))
        {
            std::cerr << "[GSI] 监听失败" << std::endl;
            s_running = false;
        }
    });
    s_serverThread.detach();
    Sleep(200);
    return s_running;
}

void StopServer()
{
    if (s_server) { s_server->stop(); delete s_server; s_server = nullptr; s_running = false; }
}

bool IsRunning() { return s_running; }

} // namespace gsi