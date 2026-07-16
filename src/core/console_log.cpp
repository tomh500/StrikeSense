#include "console_log.h"

#include "config.h"
#include "module_notifications.h"
#include "pages.h"
#include "vscript.h"

#include <Windows.h>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <regex>
#include <string>
#include <thread>

namespace consolelog {
namespace {
    std::atomic<bool> g_enabled{ false };
    std::atomic<bool> g_running{ false };
    std::atomic<bool> g_workerActive{ false };
    std::atomic<bool> g_runtimeReaderNeeded{ false };
    std::atomic<bool> g_skipToEndRequested{ false };
    std::wstring g_logPath;

    std::wstring GetConfigPath()
    {
        return config::GetConfigDir() + L"\\console_log.json";
    }

    std::wstring Utf8ToWide(const std::string& text)
    {
        if (text.empty()) return L"";
        int len = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
        if (len <= 0) return std::wstring(text.begin(), text.end());
        std::wstring out(len, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), out.data(), len);
        return out;
    }

    std::wstring FindConsoleLogPath()
    {
        HKEY key = nullptr;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", 0, KEY_READ, &key) != ERROR_SUCCESS)
            return L"";

        wchar_t steamPath[MAX_PATH]{};
        DWORD bytes = sizeof(steamPath);
        if (RegQueryValueExW(key, L"SteamPath", nullptr, nullptr, reinterpret_cast<LPBYTE>(steamPath), &bytes) != ERROR_SUCCESS) {
            RegCloseKey(key);
            return L"";
        }
        RegCloseKey(key);

        const std::filesystem::path vdfPath = std::filesystem::path(steamPath) / L"steamapps" / L"libraryfolders.vdf";
        if (!std::filesystem::exists(vdfPath)) return L"";

        std::ifstream file(vdfPath);
        std::string line;
        std::string currentPath;
        while (std::getline(file, line)) {
            std::smatch match;
            if (std::regex_search(line, match, std::regex("\"path\"\\s*\"([^\"]+)\""))) {
                currentPath = match[1].str();
            }
            if (line.find("\"730\"") != std::string::npos && !currentPath.empty()) {
                std::wstring libPath(currentPath.begin(), currentPath.end());
                return (std::filesystem::path(libPath) /
                    L"steamapps" / L"common" / L"Counter-Strike Global Offensive" / L"game" / L"csgo" / L"console.log").wstring();
            }
        }
        return L"";
    }

    std::string StripBomAndCr(std::string line)
    {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.size() >= 3 &&
            static_cast<unsigned char>(line[0]) == 0xEF &&
            static_cast<unsigned char>(line[1]) == 0xBB &&
            static_cast<unsigned char>(line[2]) == 0xBF) {
            line.erase(0, 3);
        }
        return line;
    }

    std::string CleanConsoleLine(const std::string& raw)
    {
        static const std::regex prefixRegex(R"(^\d{2}/\d{2}\s+\d{2}:\d{2}:\d{2}\s*)");
        return std::regex_replace(raw, prefixRegex, "");
    }

    void PublishLine(const std::string& rawLine)
    {
        const std::string raw = StripBomAndCr(rawLine);
        const std::string clean = CleanConsoleLine(raw);
        if (clean.empty()) return;

        if (!g_enabled.load() && g_runtimeReaderNeeded.load()) {
            if (clean.find("/cr1") != std::string::npos || clean.find("/cr0") != std::string::npos)
                modulenotifications::UpdateCrosshairRecoilSignal(Utf8ToWide(clean));
            return;
        }

        vscript::UpdateFromConsoleLog(Utf8ToWide(raw), Utf8ToWide(clean));
        std::wcout << L"[控制台日志] " << Utf8ToWide(clean) << std::endl;
    }

    void PublishLatestCrosshairSignal()
    {
        if (g_logPath.empty() || !std::filesystem::exists(g_logPath)) return;

        std::ifstream file(g_logPath, std::ios::binary);
        if (!file.is_open()) return;

        std::string latest;
        std::string line;
        while (std::getline(file, line)) {
            const std::string clean = CleanConsoleLine(StripBomAndCr(line));
            if (clean.find("/cr1") != std::string::npos || clean.find("/cr0") != std::string::npos)
                latest = clean;
        }
        if (!latest.empty())
            modulenotifications::UpdateCrosshairRecoilSignal(Utf8ToWide(latest));
    }

    void WorkerLoop()
    {
        std::cout << "[控制台日志] 监听线程已启动。" << std::endl;
        std::uintmax_t lastSize = 0;
        bool initializedAtEnd = false;

        while (g_running.load()) {
            try {
                if (g_logPath.empty()) {
                    g_logPath = FindConsoleLogPath();
                    if (!g_logPath.empty())
                        std::wcout << L"[控制台日志] 已找到 console.log: " << g_logPath << std::endl;
                }

                if (g_logPath.empty() || !std::filesystem::exists(g_logPath)) {
                    initializedAtEnd = false;
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    continue;
                }

                const std::uintmax_t currentSize = std::filesystem::file_size(g_logPath);
                if (g_skipToEndRequested.exchange(false) || !initializedAtEnd) {
                    if (g_runtimeReaderNeeded.load() && !g_enabled.load())
                        PublishLatestCrosshairSignal();
                    lastSize = currentSize;
                    initializedAtEnd = true;
                    std::cout << "[控制台日志] 已跳过既有日志，只监听新增内容。" << std::endl;
                }

                if (currentSize < lastSize) lastSize = 0;
                if (currentSize > lastSize) {
                    std::ifstream file(g_logPath, std::ios::binary);
                    if (file.is_open()) {
                        file.seekg(static_cast<std::streamoff>(lastSize));
                        std::string line;
                        while (std::getline(file, line)) {
                            PublishLine(line);
                        }
                        lastSize = currentSize;
                    }
                }
            }
            catch (const std::exception& e) {
                std::cerr << "[控制台日志] 监听异常: " << e.what() << std::endl;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        g_workerActive.store(false);
        std::cout << "[控制台日志] 监听线程已退出。" << std::endl;
    }

    void EnsureWorker()
    {
        g_skipToEndRequested.store(true);
        bool expectedInactive = false;
        if (!g_workerActive.compare_exchange_strong(expectedInactive, true)) {
            g_running.store(true);
            return;
        }

        g_running.store(true);
        std::thread(WorkerLoop).detach();
    }

    void StopWorker()
    {
        g_running.store(false);
    }
}

void LoadConfig()
{
    std::filesystem::path path(GetConfigPath());
    if (!std::filesystem::exists(path)) {
        SaveConfig();
        return;
    }

    try {
        std::ifstream in(path);
        if (!in.is_open()) return;
        nlohmann::json j;
        in >> j;
        if (j.contains("enabled") && j["enabled"].is_boolean())
            g_enabled.store(j["enabled"].get<bool>());
    }
    catch (const std::exception& e) {
        std::cerr << "[控制台日志] 加载配置失败: " << e.what() << std::endl;
    }
}

void SaveConfig()
{
    config::EnsureDirectoriesExist();
    try {
        nlohmann::json j;
        j["enabled"] = g_enabled.load();
        std::ofstream out(GetConfigPath());
        if (out.is_open()) out << j.dump(2);
    }
    catch (const std::exception& e) {
        std::cerr << "[控制台日志] 保存配置失败: " << e.what() << std::endl;
    }
}

void SetEnabled(bool enabled)
{
    if (enabled && !IsRageModeEnabled()) {
        std::cout << "[控制台日志] 超频配置未开启，拒绝打开读控制台支持。" << std::endl;
        enabled = false;
    }

    if (g_enabled.load() == enabled) {
        if (enabled) EnsureWorker();
        return;
    }

    g_enabled.store(enabled);
    std::cout << "[控制台日志] 为读控制台的方法提供支持已切换为: "
              << (enabled ? "开启" : "关闭") << std::endl;

    if (enabled) EnsureWorker();
    else if (!g_runtimeReaderNeeded.load()) StopWorker();

    SaveConfig();
}

bool IsEnabled()
{
    return g_enabled.load();
}

void SetRuntimeReaderNeeded(bool needed)
{
    g_runtimeReaderNeeded.store(needed);
    if (needed) {
        EnsureWorker();
        return;
    }
    if (!g_enabled.load()) StopWorker();
}

void StopForRageDisabled()
{
    if (!g_runtimeReaderNeeded.load()) StopWorker();
    std::cout << "[控制台日志] 超频配置关闭，已停止读取 console.log，但保留开关偏好。" << std::endl;
}

void Shutdown()
{
    g_enabled.store(false);
    g_runtimeReaderNeeded.store(false);
    StopWorker();
}
}
