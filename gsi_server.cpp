#include "gsi_server.h"
#include <iostream>
#include <thread>
#include <atomic>

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")

#include <httplib.h>
#include <nlohmann/json.hpp>

namespace gsi {

// ----------------------------------------------------------
// 全局变量
// ----------------------------------------------------------
int g_debug = 1;  // 调试开关：1=打印原始 GSI JSON

static httplib::Server* s_server = nullptr;
static std::thread s_serverThread;
static std::atomic<bool> s_running{ false };
static bool s_wsaInitialized = false;

// ----------------------------------------------------------
// GSI POST 处理回调
// ----------------------------------------------------------
static void OnGSIRequest(const httplib::Request& req, httplib::Response& res)
{
    std::string rawJson = req.body;

    // 只在非空时处理
    if (rawJson.empty())
    {
        std::cout << "[GSI] 收到空数据，忽略。" << std::endl;
        res.status = 200;
        res.set_content("OK", "text/plain");
        return;
    }

    std::cout << "[GSI] 收到 CS2 GSI 数据！(" << rawJson.length() << " 字节)" << std::endl;

    // 如果调试开关打开，打印原始 JSON
    if (g_debug)
    {
        std::cout << "======== GSI 原始 JSON 开始 ========" << std::endl;
        std::cout << rawJson << std::endl;
        std::cout << "======== GSI 原始 JSON 结束 ========" << std::endl;
    }

    // 解析 JSON 提取基本信息
    try {
        nlohmann::json j = nlohmann::json::parse(rawJson);

        // Provider 信息
        if (j.contains("provider") && j["provider"].is_object())
        {
            auto& p = j["provider"];
            if (p.contains("name"))
                std::cout << "[GSI] 提供者: " << p["name"].get<std::string>() << std::endl;
            if (p.contains("appid"))
                std::cout << "[GSI] AppID: " << p["appid"].get<int>() << std::endl;
            if (p.contains("steamid"))
                std::cout << "[GSI] SteamID(64): " << p["steamid"].get<std::string>() << std::endl;
        }

        // 地图信息
        if (j.contains("map") && j["map"].is_object())
        {
            auto& m = j["map"];
            if (m.contains("name"))
                std::cout << "[GSI] 当前地图: " << m["name"].get<std::string>() << std::endl;
            if (m.contains("mode"))
                std::cout << "[GSI] 游戏模式: " << m["mode"].get<std::string>() << std::endl;
            if (m.contains("phase"))
                std::cout << "[GSI] 地图阶段: " << m["phase"].get<std::string>() << std::endl;
            if (m.contains("round"))
                std::cout << "[GSI] 当前回合数: " << m["round"].get<int>() << std::endl;
        }

        // 回合信息
        if (j.contains("round") && j["round"].is_object())
        {
            auto& r = j["round"];
            if (r.contains("phase"))
                std::cout << "[GSI] 回合阶段: " << r["phase"].get<std::string>() << std::endl;
        }

        // 玩家信息
        if (j.contains("player") && j["player"].is_object())
        {
            auto& pl = j["player"];
            if (pl.contains("name"))
                std::cout << "[GSI] 玩家名称: " << pl["name"].get<std::string>() << std::endl;
            if (pl.contains("steamid"))
                std::cout << "[GSI] 玩家 SteamID: " << pl["steamid"].get<std::string>() << std::endl;
            if (pl.contains("team"))
                std::cout << "[GSI] 所在队伍: " << pl["team"].get<std::string>() << std::endl;
            if (pl.contains("activity"))
                std::cout << "[GSI] 活动状态: " << pl["activity"].get<std::string>() << std::endl;
        }

        // 炸弹信息
        if (j.contains("bomb") && j["bomb"].is_object())
        {
            auto& b = j["bomb"];
            if (b.contains("state"))
                std::cout << "[GSI] 炸弹状态: " << b["state"].get<std::string>() << std::endl;
        }
    }
    catch (const nlohmann::json::exception& e)
    {
        std::cerr << "[GSI] JSON 解析错误: " << e.what() << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[GSI] 处理错误: " << e.what() << std::endl;
    }

    // 回复 CS2
    res.status = 200;
    res.set_content("OK", "text/plain");
}

// ----------------------------------------------------------
bool Initialize()
{
    if (s_wsaInitialized)
        return true;

    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0)
    {
        std::cerr << "[GSI] WSAStartup 失败，错误码: " << result << std::endl;
        return false;
    }
    s_wsaInitialized = true;
    std::cout << "[GSI] Winsock 初始化成功。" << std::endl;
    return true;
}

// ----------------------------------------------------------
void Cleanup()
{
    if (s_wsaInitialized)
    {
        WSACleanup();
        s_wsaInitialized = false;
        std::cout << "[GSI] Winsock 清理完成。" << std::endl;
    }
}

// ----------------------------------------------------------
bool StartServer()
{
    if (s_running)
    {
        std::cout << "[GSI] 服务器已在运行中，忽略重复启动。" << std::endl;
        return true;
    }

    if (!s_wsaInitialized)
    {
        if (!Initialize())
            return false;
    }

    s_server = new httplib::Server();

    // 注册 POST / — CS2 GSI 使用 POST 方式发送数据
    s_server->Post("/", OnGSIRequest);

    // 同时注册 GET / 用于测试
    s_server->Get("/", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("StrikeSense GSI Server is running.", "text/plain");
    });

    s_running = true;
    std::cout << "[GSI] === GSI HTTP 服务器正在启动，监听 0.0.0.0:1009 ===" << std::endl;
    std::cout << "[GSI] CS2 的连接地址为: http://127.0.0.1:1009" << std::endl;
    std::cout << "[GSI] 确保 CS2 配置文件中 uri 与此一致！" << std::endl;

    s_serverThread = std::thread([]() {
        if (!s_server->listen("0.0.0.0", 1009))
        {
            std::cerr << "[GSI] 错误：服务器监听失败！端口 1009 可能被占用！" << std::endl;
            s_running = false;
        }
    });
    s_serverThread.detach();

    std::cout << "[GSI] HTTP 服务器已在后台线程启动，等待 CS2 连接..." << std::endl;
    return true;
}

// ----------------------------------------------------------
void StopServer()
{
    if (s_server)
    {
        s_server->stop();
        delete s_server;
        s_server = nullptr;
        s_running = false;
        std::cout << "[GSI] HTTP 服务器已停止。" << std::endl;
    }
}

// ----------------------------------------------------------
bool IsRunning()
{
    return s_running;
}

} // namespace gsi