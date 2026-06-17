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
#include <chrono>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <SDL.h>
#include <SDL_mixer.h>

// =========================

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
// ======= 在内存中缓存配置的变量 =============
static config::Settings s_cachedCfg;
// ===========================================

// ===== 事件队列 =====
static std::queue<int> s_eventQueue;
static std::mutex s_queueMutex;

// 实现刷新：让内存缓存重新加载一次磁盘文件
void RefreshConfig()
{
    s_cachedCfg = config::Load();
    std::cout << "[GSI] 收到外部通知，配置已刷新。当前音量: " << s_cachedCfg.volume << std::endl;
}

// 实现获取：直接把内存里的配置引用丢给 UI 层去读写
config::Settings& GetConfig()
{
    return s_cachedCfg;
}

void StopBombSound() {
    // 1. 停止 SDL_mixer 对应的炸弹声道（CH_BOMB 在 Global.h 中定义为 3）
    Mix_HaltChannel(3); 
    
    // 2. 必须将原子布尔值标志置为 false，防止其他线程的播放逻辑发生冲突
    s_bombSoundPlaying = false;
}

void QueueEvent(int id)
{
    std::lock_guard<std::mutex> lock(s_queueMutex);
    s_eventQueue.push(id);
}

void ProcessEventQueue()
{
    //config::Settings cfg = config::Load();
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
            if (s_cachedCfg.enable_kill_sound)
                sound::Play(id, s_cachedCfg.volume);
        }
        else if (id == -1)
        {
            std::cout << "[音效] 多杀/死斗" << std::endl;
            if (s_cachedCfg.enable_kill_sound)
                sound::Play(-1, s_cachedCfg.volume);
        }
        else if (id == -13) // 回合开始—音乐包
        {
            std::cout << "[音效] 回合开始" << std::endl;
            if (s_cachedCfg.custom_musickit)
                sound::Play(-13, s_cachedCfg.volume);
        }
        else if (id == -14) // 购买—音乐包
        {
            std::cout << "[音效] 购买阶段" << std::endl;
            if (s_cachedCfg.custom_musickit)
                sound::Play(-14, s_cachedCfg.volume);
        }
        else if (id == -12) // 炸弹—音乐包
        {
            std::cout << "[音效] 炸弹" << std::endl;
            if (s_cachedCfg.custom_musickit)
                sound::Play(-12, s_cachedCfg.volume);
        }
        else if (id == -2) // MVP—音乐包
        {
            std::cout << "[音效] MVP" << std::endl;
            if (s_cachedCfg.custom_musickit)
                sound::Play(-2, s_cachedCfg.volume);
        }
        else if (id == -3) // 胜利—音乐包
        {
            std::cout << "[音效] 胜利!" << std::endl;
            if (s_cachedCfg.custom_musickit)
                sound::Play(-3, s_cachedCfg.volume);
        }
        else if (id == -4) // 失败—音乐包
        {
            std::cout << "[音效] 失败" << std::endl;
            if (s_cachedCfg.custom_musickit)
                sound::Play(-4, s_cachedCfg.volume);
        }
        else if (id == -18) // 死亡—始终播放（不受 enable_kill_sound 控制）
        {
            std::cout << "[音效] 玩家死亡" << std::endl;
            sound::Play(-18, s_cachedCfg.volume);
        }
        else if (id == -19) // 游戏结束
        {
            std::cout << "[音效] 游戏结束" << std::endl;
            if (s_cachedCfg.custom_musickit)
                sound::Play(-19, s_cachedCfg.volume);
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
        //config::Settings cfg = config::Load();

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

        // ===== 阶段切换（完全还原 legacy 核心逻辑） =====
        if (phase != s_lastPhase)
        {
            // freezetime → buy 音效
            if (phase == "freezetime" && s_lastPhase != "freezetime")
            {
                // 还原原版等待炸弹音效逻辑：如果安放了炸弹，给爆炸/拆除留出音效播放时间，防止被-14打断
                if (s_bombPlantedThisRound) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(3500));
                }
                QueueEvent(-14);
                s_bombPlantedThisRound = false;
            }

            // live 阶段 → 回合开始
            if (phase == "live" && s_waitingForLive)
            {
                s_lastKills = 0;
                s_mvpsAtRoundStart = mvps;
                s_mvpCandidateKills = 0;
                s_mvpPushedThisRound = false;
                s_deadMuted = false;
                s_bombPlantedThisRound = false;
                s_waitingForLive = false;
                s_roundStarted = true;
                s_gameoverPushed = false;
                
                QueueEvent(-13); // 原版无条件推送回合开始音效
            }

            // over/gameover 判定
            if (s_lastPhase == "live" && (phase == "over" || phase == "gameover"))
            {
                std::cout << "[GSI] Round ended or new round started, stopping bomb sound.\n";
                StopBombSound();
                // 回合结束时处理尾刀（修复：增加 !s_deadMuted 判定，防止死后跨回合推击杀）
                if (!s_deadMuted && roundKills > s_lastKills)
                {
                    for (int k = s_lastKills + 1; k <= roundKills; ++k)
                    {
                        if (mapMode == "deathmatch") QueueEvent(-1);
                        else QueueEvent(k > 5 ? -1 : k);
                    }
                    if (roundKills > s_mvpCandidateKills) s_mvpCandidateKills = roundKills;
                    s_lastKills = roundKills;
                }

                // 获取胜负信息
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

                // MVP 判定 (还原原版严谨判定：要么MVP数增加，要么击杀>=3且获胜)
                bool isMvp = false;
                if (mvps > s_mvpsAtRoundStart) isMvp = true;
                if (hasWinTeam && winTeam == s_playerTeam && s_mvpCandidateKills >= 3) isMvp = true;

                if (!s_mvpPushedThisRound && isMvp)
                {
                    QueueEvent(-2);
                    s_mvpPushedThisRound = true;
                }

                // 胜负音效
                if (!s_mvpPushedThisRound && hasWinTeam)
                {
                    if (winTeam == s_playerTeam) QueueEvent(-3); // 胜利
                    else QueueEvent(-4); // 失败
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

        // ===== live 阶段击杀判定（还原原版，去除 activity == "playing" 限制） =====
        if (phase == "live")
        {
            if (mapMode == "deathmatch") 
            {
                if (roundKills > s_lastKills)
                {
                    QueueEvent(-1);
                    s_lastKills = roundKills;
                }
            } 
            else 
            {
                // 死亡判定
                if (health <= 0 && !s_deadMuted)
                {
                    QueueEvent(-18);
                    s_deadMuted = true;
                    // BUG 修复：千万不能在这里加 s_mvpPushedThisRound = true，否则死后拿不到MVP！
                }

                // 击杀判定
                if (!s_deadMuted && roundKills > s_lastKills)
                {
                    for (int k = s_lastKills + 1; k <= roundKills; ++k)
                    {
                        QueueEvent(k > 5 ? -1 : k);
                    }
                    if (roundKills > s_mvpCandidateKills) s_mvpCandidateKills = roundKills;
                    s_lastKills = roundKills;
                }
            }
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
                    // 原版只判定 custom_musickit，这里一并还原
                    if (s_cachedCfg.custom_musickit)
                        QueueEvent(-12);
                }
            }
        }

        // ===== 闪光弹（custom_flashbang） =====
        if (s_cachedCfg.custom_flashbang)
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
        if (g_debug) {
            std::cout << "[GSI] phase=" << phase
                      << " act=" << activity
                      << " kills=" << roundKills
                      << " last=" << s_lastKills
                      << " hp=" << health
                      << " map=" << mapMode
                      << std::endl;
        }

        // ===== 急停武器状态检测 =====
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

std::wstring CheckPortInUse()
{
    SOCKET testSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (testSock == INVALID_SOCKET) return L"";

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1009);
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    int ret = connect(testSock, (sockaddr*)&addr, sizeof(addr));
    closesocket(testSock);

    if (ret == 0)
    {
        std::string result;
        FILE* pipe = _popen("netstat -ano | findstr :1009 | findstr LISTENING", "r");
        if (pipe)
        {
            char buf[256];
            while (fgets(buf, sizeof(buf), pipe))
                result += buf;
            _pclose(pipe);
        }

        auto pos = result.find("LISTENING");
        if (pos != std::string::npos)
        {
            std::string pidStr = result.substr(pos + 10);
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
                        CloseHandle(hProc);
                        return exePath;
                    }
                    CloseHandle(hProc);
                }
            }
        }
        return L"未知进程";
    }

    return L"";
}

bool StartServer()
{
    if (s_running) return true;
    if (!s_wsaInitialized && !Initialize()) return false;

    // ======= 新增：在软件打开/启动服务器时，只读取一次磁盘配置 =======
    s_cachedCfg = config::Load();
    std::cout << "[GSI] 配置已成功加载至内存 (音量: " << s_cachedCfg.volume << ")" << std::endl;
    // ============================================================

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