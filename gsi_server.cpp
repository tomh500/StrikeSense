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

    std::cout << "[GSI] ✅ 收到 CS2 GSI 请求！(" << rawJson.length() << " 字节)" << std::endl;

    if (rawJson.empty())
    {
        std::cout << "[GSI] 数据为空，忽略。" << std::endl;
        res.status = 200;
        res.set_content("OK (empty)", "text/plain");
        return;
    }

    // 调试：打印原始 JSON
    if (g_debug)
    {
        std::cout << "======== GSI 原始 JSON 开始 ========" << std::endl;
        std::cout << rawJson << std::endl;
        std::cout << "======== GSI 原始 JSON 结束 ========" << std::endl;
    }

    // 解析 JSON 提取基本信息
    try {
        nlohmann::json j = nlohmann::json::parse(rawJson);

        std::cout << "[GSI] JSON 解析成功！顶级键数量: " << j.size() << std::endl;

        // 列出所有顶级键
        for (auto& [key, val] : j.items())
            std::cout << "  [GSI] 顶层字段: " << key << std::endl;

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
                std::cout << "[GSI] 地图: " << m["name"].get<std::string>() << std::endl;
            if (m.contains("mode"))
                std::cout << "[GSI] 模式: " << m["mode"].get<std::string>() << std::endl;
            if (m.contains("phase"))
                std::cout << "[GSI] 地图阶段: " << m["phase"].get<std::string>() << std::endl;
            if (m.contains("round"))
                std::cout << "[GSI] 回合: " << m["round"].get<int>() << std::endl;
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
                std::cout << "[GSI] 玩家: " << pl["name"].get<std::string>() << std::endl;
            if (pl.contains("steamid"))
                std::cout << "[GSI] SteamID: " << pl["steamid"].get<std::string>() << std::endl;
            if (pl.contains("team"))
                std::cout << "[GSI] 队伍: " << pl["team"].get<std::string>() << std::endl;
            if (pl.contains("activity"))
                std::cout << "[GSI] 状态: " << pl["activity"].get<std::string>() << std::endl;
        }

        // 炸弹信息
        if (j.contains("bomb") && j["bomb"].is_object())
        {
            auto& b = j["bomb"];
            if (b.contains("state"))
                std::cout << "[GSI] 炸弹: " << b["state"].get<std::string>() << std::endl;
        }
    }
    catch (const nlohmann::json::exception& e)
    {
        std::cerr << "[GSI] ⚠️ JSON 解析错误: " << e.what() << std::endl;
        std::cerr << "[GSI] 原始数据前200字节: " << rawJson.substr(0, 200) << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[GSI] ⚠️ 处理错误: " << e.what() << std::endl;
    }

    std::cout << "[GSI] 回复 CS2: OK" << std::endl;
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
        std::cout << "[GSI] 服务器已在运行中。" << std::endl;
        return true;
    }

    if (!s_wsaInitialized)
    {
        if (!Initialize())
            return false;
    }

    s_server = new httplib::Server();

    // 注册 POST / — CS2 GSI 使用 POST 方式发送 JSON 数据
    s_server->Post("/", OnGSIRequest);

    s_running = true;
    std::cout << "==============================================" << std::endl;
    std::cout << "[GSI] GSI HTTP 服务器启动中..." << std::endl;
    std::cout << "[GSI] 监听地址: 127.0.0.1:1009" << std::endl;
    std::cout << "[GSI] CS2 连接地址: http://127.0.0.1:1009" << std::endl;
    std::cout << "[GSI] 调试输出(原始JSON): " << (g_debug ? "开启" : "关闭") << std::endl;
    std::cout << "==============================================" << std::endl;

    s_serverThread = std::thread([]() {
        std::cout << "[GSI] listen() 线程已进入，等待 CS2 连接..." << std::endl;

        // 关键：监听 127.0.0.1，与 CS2 配置文件中的 uri 一致
        if (!s_server->listen("127.0.0.1", 1009))
        {
            std::cerr << "[GSI] 监听失败！错误: ";
            int err = WSAGetLastError();
            std::cerr << "WSAGetLastError=" << err;
            if (err == 10048) std::cerr << " (端口已被占用)";
            std::cerr << std::endl;
            s_running = false;
        }
        else
        {
            std::cout << "[GSI] listen() 返回，服务器已停止。" << std::endl;
        }
    });
    s_serverThread.detach();

    // 给服务器一点时间启动
    Sleep(300);

    // ===== 自我测试：用 POST 请求验证（CS2 使用 POST 发送数据） =====
    if (s_running)
    {
        std::cout << "[GSI] 正在自检 (POST /) ..." << std::endl;
        try {
            httplib::Client testClient("http://127.0.0.1:1009");
            testClient.set_connection_timeout(0, 1000000);
            // 发送和 CS2 格式类似的 POST 测试数据
            auto testRes = testClient.Post("/", "{\"test\":\"hello\"}", "application/json");
            if (testRes)
            {
                std::cout << "[GSI] 自检响应: HTTP " << testRes->status
                          << " body=" << testRes->body << std::endl;
                std::cout << "[GSI] 服务器已就绪，等待 CS2 连接..." << std::endl;
            }
            else
            {
                std::cout << "[GSI] 自检无响应，服务器可能未正确启动。" << std::endl;
                s_running = false;
            }
        }
        catch (const std::exception& e)
        {
            std::cerr << "[GSI] 自检异常: " << e.what() << std::endl;
            s_running = false;
        }
    }

    if (s_running)
        std::cout << "[GSI] GSI HTTP 服务器启动成功！" << std::endl;
    else
        std::cerr << "[GSI] GSI HTTP 服务器启动失败！" << std::endl;

    return s_running;
}

// ----------------------------------------------------------
void StopServer()
{
    if (s_server)
    {
        std::cout << "[GSI] 正在停止服务器..." << std::endl;
        s_server->stop();
        delete s_server;
        s_server = nullptr;
        s_running = false;
        std::cout << "[GSI] 服务器已停止。" << std::endl;
    }
}

// ----------------------------------------------------------
bool IsRunning()
{
    return s_running;
}

} // namespace gsi