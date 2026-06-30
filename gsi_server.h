#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

// =========== 新增：前置声明 config 命名空间与 Settings 结构体 ===========
namespace config {
    struct Settings;
}
// ====================================================================

// ============================================================
// GSI HTTP 服务器 — 纯函数 + namespace
//
// 在 namespace gsi 中封装 CS2 Game State Integration 的所有逻辑
// ============================================================

namespace gsi {

// ----------------------------------------------------------
// 初始化（WSAStartup）— 在 Win32 GUI 应用需要显式调用
// 返回 true 成功
// ----------------------------------------------------------
bool Initialize();

// ----------------------------------------------------------
// 清理（WSACleanup）
// ----------------------------------------------------------
void Cleanup();

// ----------------------------------------------------------
// 启动 HTTP 服务器（后台线程），监听 0.0.0.0:1009
// 返回 true 表示启动成功
// ----------------------------------------------------------
bool StartServer();

// ----------------------------------------------------------
// 停止 HTTP 服务器
// ----------------------------------------------------------
void StopServer();

// ----------------------------------------------------------
// 服务器是否正在运行
// ----------------------------------------------------------
bool IsRunning();

// ----------------------------------------------------------
// 调试开关 — 置 1 则在收到 GSI 数据时打印原始 JSON
// 默认值为 1
// ----------------------------------------------------------
extern int g_debug;

// ----------------------------------------------------------
// 检测端口 1009 是否被占用，返回占用进程的可执行文件路径，空表示空闲
// ----------------------------------------------------------
std::wstring CheckPortInUse();

// ----------------------------------------------------------
// 新增：刷新内存中的配置缓存（重新读盘）
// ----------------------------------------------------------
void RefreshConfig();

// ----------------------------------------------------------
// 新增：获取当前内存中缓存的配置引用，供 UI 界面直接读取
// ----------------------------------------------------------
config::Settings& GetConfig();

extern std::string gamemap;

namespace state {
extern nlohmann::json full;
extern nlohmann::json provider;
extern nlohmann::json map;
extern nlohmann::json team;
extern nlohmann::json round;
extern nlohmann::json player;
extern nlohmann::json player_state;
extern nlohmann::json player_id;
extern nlohmann::json player_match_stats;
extern nlohmann::json allplayers;
extern nlohmann::json allplayers_id;
extern nlohmann::json allplayers_state;
extern nlohmann::json allplayers_match_stats;
extern nlohmann::json bomb;
extern nlohmann::json player_name;
extern nlohmann::json player_weapons;
extern nlohmann::json previously;
extern nlohmann::json added;
extern std::unordered_map<std::string, std::string> flat;

void SyncFromJson(const nlohmann::json& stateJson);
}

namespace runtime {
extern std::string last_phase;
extern int last_kills;
extern int last_mvps;
extern bool dead_muted;
extern bool waiting_for_live;
extern bool round_started;
extern int mvp_candidate_kills;
extern bool mvp_pushed_this_round;
extern int mvps_at_round_start;
extern bool gameover_pushed;
extern bool bomb_planted_this_round;
extern std::string player_team;
extern std::string map_mode;
extern std::string activity;
extern int round_kills;
extern int health;
extern bool in_lobby;

void SyncDerived(
    const std::string& currentMapMode,
    const std::string& currentActivity,
    int currentRoundKills,
    int currentHealth,
    bool currentInLobby);
}

} // namespace gsi
