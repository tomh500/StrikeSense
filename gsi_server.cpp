#include "gsi_server.h"
#include "config.h"
#include "sound_player.h"
#include <iostream>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <winsock2.h>
#include <httplib.h>
#include <nlohmann/json.hpp>

namespace gsi {

int g_debug = 1;

static httplib::Server* s_server = nullptr;
static std::thread s_serverThread;
static std::atomic<bool> s_running{ false };
static bool s_wsaInitialized = false;

// GSI 状态
static std::string s_lastPhase;
static int s_lastKills = 0;
static int s_lastMvps = 0;
static int s_mvpCandidateKills = 0;
static bool s_mvpPushedThisRound = false;
static int s_mvpsAtRoundStart = 0;

// ===== 事件队列 =====
static std::queue<int> s_eventQueue;
static std::mutex s_queueMutex;

// ----------------------------------------------------------
// 将事件推入队列
// ----------------------------------------------------------
void QueueEvent(int id)
{
    std::lock_guard<std::mutex> lock(s_queueMutex);
    s_eventQueue.push(id);
}

// ----------------------------------------------------------
// 处理事件队列
// ----------------------------------------------------------
void ProcessEventQueue()
{
    config::Settings cfg = config::Load();
    std::queue<int> q;

    {
        std::lock_guard<std::mutex> lock(s_queueMutex);
        q.swap(s_eventQueue);
    }

    while (!q.empty())
    {
        int id = q.front();
        q.pop();

        // 击杀音效替换：只处理 1-5 和 deathmatch (-1)
        if (id >= 1 && id <= 5)
        {
            std::cout << "[音效] " << id << "杀!" << std::endl;
            if (cfg.enable_kill_sound)
                sound::Play(id, cfg.volume);
        }
        else if (id == -1)
        {
            std::cout << "[音效] 多杀/死斗" << std::endl;
            if (cfg.enable_kill_sound)
                sound::Play(-1, cfg.volume);
        }
        // 音乐包音效：由 custom_musickit 控制
        else if (id == -13) // 回合开始
        {
            if (cfg.custom_musickit)
                sound::Play(-13, cfg.volume);
        }
        else if (id == -14) // 购买
        {
            if (cfg.custom_musickit)
                sound::Play(-14, cfg.volume);
        }
        else if (id == -12) // 炸弹
        {
            if (cfg.custom_musickit)
                sound::Play(-12, cfg.volume);
        }
        else if (id == -2) // MVP
        {
            if (cfg.custom_musickit)
                sound::Play(-2, cfg.volume);
        }
        else if (id == -3) // 胜利
        {
            if (cfg.custom_musickit)
                sound::Play(-3, cfg.volume);
        }
        else if (id == -4) // 失败
        {
            if (cfg.custom_musickit)
                sound::Play(-4, cfg.volume);
        }
        else if (id == -18) // 死亡—始终播放
        {
            std::cout << "[音效] 玩家死亡" << std::endl;
            sound::Play(-18, cfg.volume);
        }
    }
}

// ----------------------------------------------------------
// GSI POST 处理
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
        std::cout << "======== GSI 原始 JSON ========" << std::endl;
        std::cout << rawJson << std::endl;
        std::cout << "===============================" << std::endl;
    }

    try {
        nlohmann::json j = nlohmann::json::parse(rawJson);

        // ===== 提取字段 =====
        std::string phase;
        std::string activity;
        int roundKills = 0;
        int mvps = 0;
        int health = 100;

        if (j.contains("player") && j["player"].is_object())
        {
            auto& pl = j["player"];
            if (pl.contains("activity") && pl["activity"].is_string())
                activity = pl["activity"].get<std::string>();
            if (pl.contains("state") && pl["state"].is_object())
            {
                auto& st = pl["state"];
                if (st.contains("round_kills") && st["round_kills"].is_number())
                    roundKills = st["round_kills"].get<int>();
                if (st.contains("health") && st["health"].is_number())
                    health = st["health"].get<int>();
            }
            if (pl.contains("match_stats") && pl["match_stats"].is_object())
            {
                auto& ms = pl["match_stats"];
                if (ms.contains("mvps") && ms["mvps"].is_number())
                    mvps = ms["mvps"].get<int>();
            }
        }

        if (j.contains("round") && j["round"].is_object())
        {
            auto& r = j["round"];
            if (r.contains("phase") && r["phase"].is_string())
                phase = r["phase"].get<std::string>();
        }

        // ===== 击杀回退修复 =====
        if (roundKills < s_lastKills)
            s_lastKills = roundKills;

        // ===== 回合切换 =====
        if (phase != s_lastPhase)
        {
            if (phase == "live")
            {
                s_lastKills = 0;
                s_mvpsAtRoundStart = mvps;
                s_mvpCandidateKills = 0;
                s_mvpPushedThisRound = false;
                QueueEvent(-13); // 回合开始
            }

            if (s_lastPhase == "live" && phase == "over")
            {
                // 回合结束时处理击杀
                if (roundKills > s_lastKills)
                {
                    for (int k = s_lastKills + 1; k <= roundKills; ++k)
                        QueueEvent(k > 5 ? -1 : k);
                    s_mvpCandidateKills += (roundKills - s_lastKills);
                    s_lastKills = roundKills;
                }

                // MVP 判定
                bool isMvp = (mvps > s_mvpsAtRoundStart) ||
                    (!s_mvpPushedThisRound && s_mvpCandidateKills > 0);
                if (isMvp)
                {
                    QueueEvent(-2);
                    s_mvpPushedThisRound = true;
                }
            }

            s_lastPhase = phase;
        }

        // ===== live 阶段击杀判定 =====
        if (phase == "live" && activity == "playing")
        {
            if (roundKills > s_lastKills)
            {
                for (int k = s_lastKills + 1; k <= roundKills; ++k)
                    QueueEvent(k > 5 ? -1 : k);
                s_mvpCandidateKills += (roundKills - s_lastKills);
                s_lastKills = roundKills;
            }
        }

        // ===== 死亡判定（不受 enable_kill_sound 控制） =====
        if (phase == "live" && activity == "playing" && health <= 0)
            QueueEvent(-18);

        // ===== 调试输出 =====
        if (g_debug)
        {
            std::cout << "[GSI] phase=" << phase
                      << " act=" << activity
                      << " kills=" << roundKills
                      << " last=" << s_lastKills
                      << " hp=" << health
                      << std::endl;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "[GSI] 错误: " << e.what() << std::endl;
    }

    res.status = 200;
    res.set_content("OK", "text/plain");

    // 处理事件队列
    ProcessEventQueue();
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
    std::cout << "[GSI] 监听 127.0.0.1:1009" << std::endl;

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