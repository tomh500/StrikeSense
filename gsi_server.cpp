// winsock2.h 必须放在任何 windows.h 之前（包括间接包含）
#ifndef _WINSOCK2API_
#include <winsock2.h>
#endif
#include "gsi_server.h"
#include "config.h"
#include "sound_player.h"
#include "quickstop.h"
#include <iostream>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <httplib.h>
#include <nlohmann/json.hpp>

namespace gsi {

int g_debug = 0;

static httplib::Server* s_server = nullptr;
static std::thread s_serverThread;
static std::atomic<bool> s_running{ false };
static bool s_wsaInitialized = false;

// GSI 状态（从 legacy/Event.cpp 照搬）
static std::string s_lastPhase;
static int s_lastKills = 0;
static int s_lastMvps = 0;
static bool s_deadMuted = false;
static bool s_waitingForLive = true;
static bool s_roundStarted = false;
static int s_mvpCandidateKills = 0;
static bool s_mvpPushedThisRound = false;
static int s_mvpsAtRoundStart = 0;
static bool s_gameoverPushed = false;
static bool s_bombPlantedThisRound = false;
static std::atomic<bool> s_bombSoundPlaying{ false };
static std::string s_playerTeam;

// ===== 事件队列 =====
static std::queue<int> s_eventQueue;
static std::mutex s_queueMutex;

void QueueEvent(int id)
{
    std::lock_guard<std::mutex> lock(s_queueMutex);
    s_eventQueue.push(id);
}

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
        int id = q.front(); q.pop();

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
        else if (id == -13) // 回合开始—音乐包
        {
            std::cout << "[音效] 回合开始" << std::endl;
            if (cfg.custom_musickit)
                sound::Play(-13, cfg.volume);
        }
        else if (id == -14) // 购买—音乐包
        {
            std::cout << "[音效] 购买阶段" << std::endl;
            if (cfg.custom_musickit)
                sound::Play(-14, cfg.volume);
        }
        else if (id == -12) // 炸弹—音乐包
        {
            std::cout << "[音效] 炸弹" << std::endl;
            if (cfg.custom_musickit)
                sound::Play(-12, cfg.volume);
        }
        else if (id == -2) // MVP—音乐包
        {
            std::cout << "[音效] MVP" << std::endl;
            if (cfg.custom_musickit)
                sound::Play(-2, cfg.volume);
        }
        else if (id == -3) // 胜利—音乐包
        {
            std::cout << "[音效] 胜利!" << std::endl;
            if (cfg.custom_musickit)
                sound::Play(-3, cfg.volume);
        }
        else if (id == -4) // 失败—音乐包
        {
            std::cout << "[音效] 失败" << std::endl;
            if (cfg.custom_musickit)
                sound::Play(-4, cfg.volume);
        }
        else if (id == -18) // 死亡—始终播放（不受 enable_kill_sound 控制）
        {
            std::cout << "[音效] 玩家死亡" << std::endl;
            sound::Play(-18, cfg.volume);
        }
        else if (id == -19) // 游戏结束
        {
            std::cout << "[音效] 游戏结束" << std::endl;
            if (cfg.custom_musickit)
                sound::Play(-19, cfg.volume);
        }
    }
}

// ============================================================
// GSI POST 处理
// ============================================================
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
        config::Settings cfg = config::Load();

        std::string phase;
        std::string activity;
        int roundKills = 0, mvps = 0, health = 100;
        std::string mapMode = "competitive";
        std::string playerSteamid;

        // 提取玩家数据
        if (j.contains("player") && j["player"].is_object())
        {
            auto& pl = j["player"];
            if (pl.contains("activity") && pl["activity"].is_string())
                activity = pl["activity"].get<std::string>();
            if (pl.contains("steamid") && pl["steamid"].is_string())
                playerSteamid = pl["steamid"].get<std::string>();
            if (pl.contains("team") && pl["team"].is_string())
                s_playerTeam = pl["team"].get<std::string>();

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

        if (j.contains("map") && j["map"].is_object())
        {
            auto& m = j["map"];
            if (m.contains("mode") && m["mode"].is_string())
                mapMode = m["mode"].get<std::string>();
        }

        if (j.contains("round") && j["round"].is_object())
        {
            auto& r = j["round"];
            if (r.contains("phase") && r["phase"].is_string())
                phase = r["phase"].get<std::string>();
        }

        // 击杀回退修复（legacy）
        if (roundKills < s_lastKills)
            s_lastKills = roundKills;

        // ===== 阶段切换（legacy 核心逻辑） =====
        if (phase != s_lastPhase)
        {
            // freezetime → buy 音效
            if (phase == "freezetime" && s_lastPhase != "freezetime")
            {
                QueueEvent(-14);
                s_bombPlantedThisRound = false;
            }

            // live 阶段 → 回合开始
            if (phase == "live" && s_waitingForLive )
            {

                s_lastKills = 0;
                s_mvpsAtRoundStart = mvps;
                s_mvpCandidateKills = 0;
                s_mvpPushedThisRound = false;
                s_deadMuted = false;
                s_bombPlantedThisRound = false;
                s_waitingForLive = false;
                s_roundStarted = true;
                if (mapMode == "deathmatch")
                {
                    QueueEvent(-13);
                }
                
            }

            // over → 检查是否刚刚从 live 过来了
            if (s_lastPhase == "live" && (phase == "over" || phase == "gameover"))
            {
                // 回合结束时处理尾刀
                if (roundKills > s_lastKills)
                {
                    for (int k = s_lastKills + 1; k <= roundKills; ++k)
                    {
                        if (mapMode == "deathmatch")
                            QueueEvent(-1);
                        else
                            QueueEvent(k > 5 ? -1 : k);
                    }
                    s_mvpCandidateKills += (roundKills - s_lastKills);
                    s_lastKills = roundKills;
                }

                // 胜负判定
                bool hasWinTeam = false;
                std::string winTeam;
                if (j.contains("round") && j["round"].is_object())
                {
                    auto& r = j["round"];
                    if (r.contains("win_team") && r["win_team"].is_string())
                    {
                        hasWinTeam = true;
                        winTeam = r["win_team"].get<std::string>();
                    }
                }
                else if (j.contains("added") && j["added"].is_object())
                {
                    auto& a = j["added"];
                    if (a.contains("round") && a["round"].is_object())
                    {
                        auto& ar = a["round"];
                        if (ar.contains("win_team") && ar["win_team"].is_string())
                        {
                            hasWinTeam = true;
                            winTeam = ar["win_team"].get<std::string>();
                        }
                    }
                }

                // MVP 判定
                bool isMvp = (mvps > s_mvpsAtRoundStart) ||
                    (!s_mvpPushedThisRound && s_mvpCandidateKills > 0);
                if (isMvp)
                {
                    QueueEvent(-2);
                    s_mvpPushedThisRound = true;
                }

                // 胜负音效
                if (!s_mvpPushedThisRound && hasWinTeam)
                {
                    if (winTeam == s_playerTeam)
                        QueueEvent(-3); // 胜利
                    else
                        QueueEvent(-4); // 失败
                }

                if (phase == "gameover" && !s_gameoverPushed)
                {
                    QueueEvent(-19);
                    s_gameoverPushed = true;
                }

                s_roundStarted = false;
            }

            // 离开 live → 标记等待
            if (phase != "live")
            {
                s_waitingForLive = true;
                s_roundStarted = false;
            }

            s_lastPhase = phase;
        }

        // ===== live 阶段击杀判定（稳态） =====
        if (phase == "live" && activity == "playing")
        {
            // 死亡判定
            if (health <= 0)
            {
                if (!s_deadMuted)
                {
                    QueueEvent(-18);
                    s_deadMuted = true;
                }
                s_mvpPushedThisRound = true; // 死亡后本回合不再触发 MVP
            }

            // 击杀判定
            if (!s_deadMuted && roundKills > s_lastKills)
            {
                for (int k = s_lastKills + 1; k <= roundKills; ++k)
                {
                    if (mapMode == "deathmatch")
                        QueueEvent(-1);
                    else
                        QueueEvent(k > 5 ? -1 : k);
                }
                if (roundKills > s_mvpCandidateKills)
                    s_mvpCandidateKills = roundKills;
                s_lastKills = roundKills;
            }
        }

        // ===== 死斗模式 =====
        if (mapMode == "deathmatch" && roundKills > s_lastKills)
        {
            QueueEvent(-1);
            s_lastKills = roundKills;
        }

        // ===== 炸弹（音乐包） =====
        if (j.contains("round") && j["round"].is_object())
        {
            auto& r = j["round"];
            if (r.contains("bomb") && r["bomb"].is_string())
            {
                std::string bs = r["bomb"].get<std::string>();
                if (bs == "planted" && !s_bombPlantedThisRound)
                {
                    s_bombPlantedThisRound = true;
                    if (cfg.custom_musickit && cfg.enable_kill_sound)
                        QueueEvent(-12);
                }
            }
        }

        // ===== 闪光弹（custom_flashbang） =====
        if (cfg.custom_flashbang)
        {
            int flashedNow = 0, flashedBefore = 0;
            if (j.contains("player") && j["player"].is_object())
            {
                auto& pl = j["player"];
                if (pl.contains("state") && pl["state"].is_object())
                {
                    auto& st = pl["state"];
                    if (st.contains("flashed") && st["flashed"].is_number())
                        flashedNow = st["flashed"].get<int>();
                }
            }
            if (j.contains("previously") && j["previously"].is_object())
            {
                auto& prev = j["previously"];
                if (prev.contains("player") && prev["player"].is_object())
                {
                    auto& ppl = prev["player"];
                    if (ppl.contains("state") && ppl["state"].is_object())
                    {
                        auto& pst = ppl["state"];
                        if (pst.contains("flashed") && pst["flashed"].is_number())
                            flashedBefore = pst["flashed"].get<int>();
                    }
                }
            }
            if (flashedNow > 0 && flashedBefore == 0)
                std::cout << "[FLASH] 玩家被闪光弹击中" << std::endl;
            else if (flashedNow == 0 && flashedBefore > 0)
                std::cout << "[FLASH] 闪光效果结束" << std::endl;
        }

        // ===== 调试输出 =====
        std::cout << "[GSI] phase=" << phase
                  << " act=" << activity
                  << " kills=" << roundKills
                  << " last=" << s_lastKills
                  << " hp=" << health
                  << " map=" << mapMode
                  << std::endl;

        // ===== 急停武器状态检测（GSI JSON 内的武器信息） =====
        if (GetQSConfig().enabled)
            ProcessQuickStopCommand(rawJson);
    }
    catch (const std::exception& e)
    {
        std::cerr << "[GSI] 错误: " << e.what() << std::endl;
    }

    res.status = 200;
    res.set_content("OK", "text/plain");

    ProcessEventQueue();
}
bool Initialize()
{
    if (s_wsaInitialized) return true;
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return false;
    s_wsaInitialized = true;
    return true;
}

void Cleanup()
{
    if (s_wsaInitialized) { WSACleanup(); s_wsaInitialized = false; }
}

// 检查端口 1009 是否被占用，返回占用进程的 exe 名称（空表示空闲）
std::wstring CheckPortInUse()
{
    // 用 Raw Sockets 检查端口监听
    SOCKET testSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (testSock == INVALID_SOCKET) return L"";

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1009);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    // connect 成功说明端口已被占用
    int ret = connect(testSock, (sockaddr*)&addr, sizeof(addr));
    closesocket(testSock);

    if (ret == 0)
    {
        // 端口在用，用 netstat 查占用程序
        // 创建一个临时文件
        std::string result;
        FILE* pipe = _popen("netstat -ano | findstr :1009 | findstr LISTENING", "r");
        if (pipe)
        {
            char buf[256];
            while (fgets(buf, sizeof(buf), pipe))
                result += buf;
            _pclose(pipe);
        }

        // 从 netstat 输出提取 PID
        // 格式: TCP 127.0.0.1:1009 0.0.0.0:0 LISTENING 1234
        auto pos = result.find("LISTENING");
        if (pos != std::string::npos)
        {
            std::string pidStr = result.substr(pos + 10);
            // 去掉空格
            pidStr.erase(0, pidStr.find_first_not_of(" \t\r\n"));
            int pid = atoi(pidStr.c_str());
            if (pid > 0)
            {
                HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
                if (hProc)
                {
                    wchar_t exeName[MAX_PATH] = {};
                    DWORD sz = MAX_PATH;
                    if (QueryFullProcessImageNameW(hProc, 0, exeName, &sz))
                    {
                        std::wstring exePath(exeName);
                        closesocket(testSock);
                        CloseHandle(hProc);
                        return exePath;
                    }
                    CloseHandle(hProc);
                }
            }
        }
        return L"未知进程";
    }

    return L""; // 端口空闲
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