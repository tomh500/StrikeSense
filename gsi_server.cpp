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
#include "flashoverlay.h"
#include "itemhelper_overlay.h"
#include "itemhelper_page.h"  
#include "normalgen.h"
#include <windows.h>
// =========================
extern HINSTANCE hInst;
bool IsCS2WindowActive();

namespace gsi {

    int g_debug = 1;

    static httplib::Server* s_server = nullptr;
    static std::thread s_serverThread;
    static std::thread s_watchdogThread; // 【修复问题4】引入标准线程管理，拒绝隐患重重的 detach
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
    std::string gamemap;
    // 状态剥离：GSI只负责判定是否在大厅，Watchdog负责实时跟进焦点
    static std::atomic<bool> s_isInLobby{ false };
    static std::atomic<bool> s_isWindowActive{ true };
    // 0 = 不在大厅/游戏关闭
    // 1 = 在大厅且游戏有焦点 (播放/恢复音量)
    // 2 = 在大厅但游戏无焦点 (静音继续播放)
    //int g_menuState = 0;
    static int s_lastMenuState = 0;
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
                {
                    Mix_HaltChannel(3); sound::Play(-13, s_cachedCfg.volume);
                }
                   
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
            else if (id == -21) // 大厅菜单—音乐包
            {
                std::cout << "[音效] 大厅菜单" << std::endl;
                if (s_cachedCfg.custom_musickit)
                    sound::Play(-21, s_cachedCfg.volume);
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

                if (m.contains("name") && m["name"].is_string())
                    gamemap = m["name"].get<std::string>();
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

            // ===== 严谨的大厅状态判定 =====
                      // 特征1：无 map 和 round 字段
                      // 特征2：activity == "menu"
            bool hasMatchFields = j.contains("map") || j.contains("round");
            bool currentInLobby = (activity == "menu" && !hasMatchFields);

            if (currentInLobby != s_isInLobby) {
                s_isInLobby = currentInLobby;
                if (currentInLobby) {

                    std::cout << "[大厅音乐] 进入大厅" << std::endl;
                    Mix_HaltChannel(3);
                    s_isWindowActive = IsCS2WindowActive();
                    // 核心修复：进入大厅瞬间，强制同步当前真实的通道音量，根除0音量幽灵播放问题
                    Mix_Volume(4, s_isWindowActive ? static_cast<int>(s_cachedCfg.volume * MIX_MAX_VOLUME) : 0);
                    QueueEvent(-21);
                }
                else {
                    std::cout << "[大厅音乐] 离开大厅" << std::endl;
                    Mix_HaltChannel(4);
                    StopBombSound();
                }
            }
            // =====================================    

            // ===== 阶段切换（完全还原 legacy 核心逻辑） =====
            if (phase != s_lastPhase)
            {
                // freezetime → buy 音效
                if (phase == "freezetime" && s_lastPhase != "freezetime")
                {
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

                    QueueEvent(-13);
                }

                // over/gameover 判定
                if (s_lastPhase == "live" && (phase == "over" || phase == "gameover"))
                {
                    std::cout << "[GSI] Round ended or new round started, stopping bomb sound.\n";
                    StopBombSound();
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

                    // MVP 判定
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
                        if (winTeam == s_playerTeam) QueueEvent(-3);
                        else QueueEvent(-4);
                    }

                    if (phase == "gameover" && !s_gameoverPushed)
                    {
                        QueueEvent(-19);
                        s_gameoverPushed = true;
                    }

                    s_roundStarted = false;
                }

                if (phase != "live")
                {
                    s_waitingForLive = true;
                    s_roundStarted = false;
                }

                s_lastPhase = phase;
            }

            // ===== live 阶段击杀判定 =====
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
                    if (health <= 0 && !s_deadMuted)
                    {
                        QueueEvent(-18);
                        s_deadMuted = true;
                    }

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

            // ===== 炸弹 =====
            if (j.contains("round") && j["round"].is_object())
            {
                auto& r = j["round"];
                if (r.contains("bomb") && r["bomb"].is_string())
                {
                    std::string bs = r["bomb"].get<std::string>();
                    if (bs == "planted" && !s_bombPlantedThisRound)
                    {
                        s_bombPlantedThisRound = true;
                        if (s_cachedCfg.custom_musickit)
                            QueueEvent(-12);
                    }
                }
            }

            // ===== 闪光弹 =====
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
                {
                    std::cout << "[FLASH] 玩家被闪光弹击中" << std::endl;
                    flashoverlay::Show();
                }
                else if (flashedNow == 0 && flashedBefore > 0)
                {
                    std::cout << "[FLASH] 闪光效果结束" << std::endl;
                    flashoverlay::Hide();
                }
            }

            if (g_debug) {
                std::cout << "[GSI] phase=" << phase
                    << " act=" << activity
                    << " kills=" << roundKills
                    << " last=" << s_lastKills
                    << " hp=" << health
                    << " map=" << mapMode
                    << " gamemap=" << gamemap
                    << std::endl;
            }

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

        s_cachedCfg = config::Load();
        std::cout << "[GSI] 配置已成功加载至内存 (音量: " << s_cachedCfg.volume << ")" << std::endl;

        s_server = new httplib::Server();
        s_server->Post("/", OnGSIRequest);
        std::cout << "[GSI] 监听 127.0.0.1:1009" << std::endl;

        s_running = true;

        // ==================== 【核心修复：补回被误删的监听线程】 ====================
        s_serverThread = std::thread([]() {
            if (!s_server->listen("127.0.0.1", 1009))
            {
                std::cerr << "[GSI] 监听失败" << std::endl;
                s_running = false;
            }
            });
        // ========================================================================

        s_watchdogThread = std::thread([]() {
            while (s_running) {
                bool cs2Running = normalgen::IsCS2Running();
                if (!cs2Running) {
                    if (s_isInLobby) {
                        std::cout << "[大厅音乐守护] 游戏进程消失，掐断播放" << std::endl;
                        s_isInLobby = false;
                        Mix_HaltChannel(4);
                        StopBombSound();
                    }
                }
                else if (s_isInLobby) {
                    // 游戏运行中且在大厅，高频轮询焦点状态
                    bool isActive = IsCS2WindowActive();
                    if (isActive != s_isWindowActive) {
                        s_isWindowActive = isActive;
                        if (isActive) {
                            std::cout << "[大厅音乐守护] 游戏获得焦点，恢复音量" << std::endl;
                            Mix_Volume(4, static_cast<int>(s_cachedCfg.volume * MIX_MAX_VOLUME));
                        }
                        else {
                            std::cout << "[大厅音乐守护] 游戏失去焦点，静音大厅" << std::endl;
                            Mix_Volume(4, 0);
                        }
                    }
                }
                // 提高检测频率至 200ms，拒绝 GSI 的节流延迟
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
            });

        Sleep(200);
        return true;
    }

    void StopServer()
    {
        // 【修复问题4】完美安全同步关闭序列，绝不引发未定义异常崩溃
        s_running = false;

        if (s_server) {
            s_server->stop(); // 强行阻断 listen 阻塞
        }

        // 等待后台线程完工再往下走
        if (s_serverThread.joinable()) {
            s_serverThread.join();
        }
        if (s_watchdogThread.joinable()) {
            s_watchdogThread.join();
        }

        if (s_server) {
            delete s_server;
            s_server = nullptr;
        }
    }

    bool IsRunning() { return s_running; }

} // namespace gsi
