#include "gsi_server.h"
#include <iostream>
#include <thread>
#include <atomic>

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2ipdef.h>
#include <iphlpapi.h>
#include <tlhelp32.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")

#include <httplib.h>
#include <nlohmann/json.hpp>

namespace gsi {

// ----------------------------------------------------------
// 全局变量
// ----------------------------------------------------------
int g_debug = 1;

static httplib::Server* s_server = nullptr;
static std::thread s_serverThread;
static std::atomic<bool> s_running{ false };
static bool s_wsaInitialized = false;

// ----------------------------------------------------------
// 检测端口 1009 被哪个进程占用
// 返回 PID，0 = 空闲
// ----------------------------------------------------------
int CheckPortInUse()
{
    ULONG bufSize = 0;
    GetExtendedTcpTable(nullptr, &bufSize, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);

    std::vector<char> buf(bufSize);
    PMIB_TCPTABLE_OWNER_PID tcpTable = reinterpret_cast<PMIB_TCPTABLE_OWNER_PID>(buf.data());

    if (GetExtendedTcpTable(tcpTable, &bufSize, FALSE, AF_INET,
                            TCP_TABLE_OWNER_PID_ALL, 0) != NO_ERROR)
        return 0;

    for (DWORD i = 0; i < tcpTable->dwNumEntries; ++i)
    {
        MIB_TCPROW_OWNER_PID& row = tcpTable->table[i];
        // 监听状态 + 本地端口 1009
        if (row.dwState == MIB_TCP_STATE_LISTEN &&
            ntohs((u_short)row.dwLocalPort) == 1009)
        {
            return (int)row.dwOwningPid;
        }
    }

    return 0;
}

// ----------------------------------------------------------
// 根据 PID 获取进程名称
// ----------------------------------------------------------
std::wstring GetProcessName(int pid)
{
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE)
        return L"未知进程";

    PROCESSENTRY32W pe = { sizeof(PROCESSENTRY32W) };
    std::wstring name = L"未知进程";

    if (Process32FirstW(hSnapshot, &pe))
    {
        do {
            if (pe.th32ProcessID == pid)
            {
                name = pe.szExeFile;
                break;
            }
        } while (Process32NextW(hSnapshot, &pe));
    }

    CloseHandle(hSnapshot);
    return name;
}

// ----------------------------------------------------------
// GSI POST 处理回调
// ----------------------------------------------------------
static void OnGSIRequest(const httplib::Request& req, httplib::Response& res)
{
    std::string rawJson = req.body;

    std::cout << "[GSI] 收到 CS2 GSI 请求!(" << rawJson.length() << " 字节)" << std::endl;

    if (rawJson.empty())
    {
        std::cout << "[GSI] 数据为空，忽略。" << std::endl;
        res.status = 200;
        res.set_content("OK (empty)", "text/plain");
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

        std::cout << "[GSI] JSON 解析成功！顶级键数量: " << j.size() << std::endl;

        for (auto& [key, val] : j.items())
            std::cout << "  [GSI] 顶层字段: " << key << std::endl;

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

        if (j.contains("round") && j["round"].is_object())
        {
            auto& r = j["round"];
            if (r.contains("phase"))
                std::cout << "[GSI] 回合阶段: " << r["phase"].get<std::string>() << std::endl;
        }

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

        if (j.contains("bomb") && j["bomb"].is_object())
        {
            auto& b = j["bomb"];
            if (b.contains("state"))
                std::cout << "[GSI] 炸弹: " << b["state"].get<std::string>() << std::endl;
        }
    }
    catch (const nlohmann::json::exception& e)
    {
        std::cerr << "[GSI] JSON 解析错误: " << e.what() << std::endl;
        std::cerr << "[GSI] 原始数据前200字节: " << rawJson.substr(0, 200) << std::endl;
    }
    catch (const std::exception& e)
    {
        std::cerr << "[GSI] 处理错误: " << e.what() << std::endl;
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

    Sleep(300);

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