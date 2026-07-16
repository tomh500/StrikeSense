#include "cscript.h"

#include "config.h"
#include "console_log.h"
#include "pages.h"
#include "steam_helper.h"
#include "volume_mixer.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cctype>
#include <cwctype>
#include <deque>
#include <fstream>
#include <iostream>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <unordered_set>

namespace cscript {
namespace {

using clock_type = std::chrono::steady_clock;

constexpr int kTickerHz = 64;
constexpr auto kTickerInterval = std::chrono::microseconds(1'000'000 / kTickerHz);
constexpr UINT kTickerVirtualKey = VK_NUMPAD9;
constexpr wchar_t kTickerSourceKey[] = L"kp_9";
constexpr char kManagedBlockStart[] = "//--StrikeSense CScript Ticker--";
constexpr char kManagedBlockEnd[] = "//--StrikeSense CScript Ticker END--";

struct parsed_script {
    std::vector<std::string> on_pressed;
    std::vector<std::string> on_released;
};

struct script_record {
    mounted_script public_state;
    parsed_script parsed;
};

struct command_sequence {
    std::uint64_t script_id = 0;
    std::string phase;
    std::string source_key;
    std::vector<std::string> commands;
    std::size_t next_command = 0;
};

struct script_lane {
    std::deque<command_sequence> sequences;
    bool ready = false;
};

std::atomic<bool> g_enabled{ false };
std::atomic<bool> g_worker_running{ false };
std::thread g_worker;
HHOOK g_keyboard_hook = nullptr;
std::mutex g_state_mutex;
std::condition_variable g_worker_cv;
std::vector<script_record> g_scripts;
std::vector<mounted_script> g_public_scripts;
std::unordered_map<std::uint64_t, script_lane> g_lanes;
std::deque<std::uint64_t> g_ready_lanes;
std::unordered_map<std::uint64_t, bool> g_physical_down;
std::filesystem::path g_ticker_cfg_path;
std::wstring g_runtime_status = L"尚未启动";
std::uint64_t g_next_script_id = 1;
bool g_clear_pending = false;

std::filesystem::path ConfigPath()
{
    return std::filesystem::path(config::GetConfigDir()) / L"cscript_config.json";
}

std::filesystem::path BaseDirectory()
{
    wchar_t profile[MAX_PATH]{};
    GetEnvironmentVariableW(L"USERPROFILE", profile, MAX_PATH);
    return std::filesystem::path(profile) / L"StrikeSense";
}

void EnsureExampleScript()
{
    const std::filesystem::path directory = BaseDirectory() / L"cscript";
    std::filesystem::create_directories(directory);
    const std::filesystem::path example = directory / L"testscript.cscript";
    if (std::filesystem::exists(example)) return;

    static constexpr char kExample[] = R"cscript(// StrikeSense CScript 示例：请在界面中为本脚本绑定一个单按键。
@OnPressed {
    "+forward":0;
    "alias a b;+jump":1;
}

@OnReleased {
    "-forward":0;
    "-jump":1;
    "+lookatweapon":2;
    "-lookatweapon":3;
}
)cscript";
    std::ofstream output(example, std::ios::binary);
    if (output.is_open()) {
        output.write(kExample, static_cast<std::streamsize>(std::char_traits<char>::length(kExample)));
        std::wcout << L"[CScript] 已生成示例脚本：" << example.wstring() << std::endl;
    }
}

std::string WideToUtf8(const std::wstring& value)
{
    if (value.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
        nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
        result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring Utf8ToWide(const std::string& value)
{
    if (value.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0) return std::wstring(value.begin(), value.end());
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size);
    return result;
}

std::string ToLowerAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string TrimAscii(std::string value)
{
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    }).base();
    if (first >= last) return {};
    return std::string(first, last);
}

std::string StripComments(const std::string& text)
{
    std::string result;
    result.reserve(text.size());
    bool in_string = false;
    bool escaped = false;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char ch = text[i];
        if (in_string) {
            result.push_back(ch);
            if (escaped) escaped = false;
            else if (ch == '\\') escaped = true;
            else if (ch == '"') in_string = false;
            continue;
        }
        if (ch == '"') {
            in_string = true;
            result.push_back(ch);
            continue;
        }
        if (ch == '/' && i + 1 < text.size() && text[i + 1] == '/') {
            while (i < text.size() && text[i] != '\n') ++i;
            if (i < text.size()) result.push_back('\n');
            continue;
        }
        result.push_back(ch);
    }
    return result;
}

std::vector<std::string> SplitSourceCommands(const std::string& command)
{
    std::vector<std::string> result;
    std::string current;
    bool in_string = false;
    bool escaped = false;
    for (const char ch : command) {
        if (in_string) {
            current.push_back(ch);
            if (escaped) escaped = false;
            else if (ch == '\\') escaped = true;
            else if (ch == '"') in_string = false;
            continue;
        }
        if (ch == '"') {
            in_string = true;
            current.push_back(ch);
            continue;
        }
        if (ch == ';') {
            const std::string trimmed = TrimAscii(current);
            if (!trimmed.empty()) result.push_back(trimmed);
            current.clear();
            continue;
        }
        current.push_back(ch);
    }
    const std::string trimmed = TrimAscii(current);
    if (!trimmed.empty()) result.push_back(trimmed);
    return result;
}

bool ValidateParallelCommand(const std::string& command, std::wstring& error)
{
    if (command.find('\n') != std::string::npos || command.find('\r') != std::string::npos) {
        error = L"命令字符串不能包含换行";
        return false;
    }

    static const std::unordered_set<std::string> parallel_commands{
        "alias", "bind", "say", "say_team", "echoln"
    };
    int action_command_count = 0;
    for (const std::string& part : SplitSourceCommands(command)) {
        std::istringstream stream(part);
        std::string name;
        stream >> name;
        name = ToLowerAscii(name);
        if (!name.empty() && parallel_commands.find(name) == parallel_commands.end())
            ++action_command_count;
    }
    if (action_command_count > 1) {
        error = L"同一动作中只能包含一个普通指令；alias、bind、say、say_team、echoln 可并行";
        return false;
    }
    if (action_command_count == 0 && SplitSourceCommands(command).empty()) {
        error = L"命令不能为空";
        return false;
    }
    return true;
}

class parser {
public:
    explicit parser(std::string source) : source_(StripComments(source)) {}

    bool Parse(parsed_script& result, std::wstring& error)
    {
        bool saw_pressed = false;
        bool saw_released = false;
        while (true) {
            SkipWhitespace();
            if (AtEnd()) break;
            if (!Consume('@')) return Fail(L"需要以 @OnPressed 或 @OnReleased 开始", error);
            const std::string modifier = ParseIdentifier();
            if (modifier != "OnPressed" && modifier != "OnReleased")
                return Fail(L"未知修饰符：" + Utf8ToWide(modifier), error);
            SkipWhitespace();
            if (!Consume('{')) return Fail(L"修饰符后缺少左花括号", error);

            std::vector<std::pair<std::size_t, std::string>> indexed_commands;
            while (true) {
                SkipWhitespace();
                if (Consume('}')) break;
                if (AtEnd()) return Fail(L"脚本块缺少右花括号", error);

                std::string command;
                if (!ParseString(command, error)) return false;
                SkipWhitespace();
                if (!Consume(':')) return Fail(L"命令后缺少序号分隔符 ':'", error);
                SkipWhitespace();
                const std::optional<std::size_t> index = ParseIndex();
                if (!index.has_value()) return Fail(L"动作序号必须是从 0 开始的非负整数", error);
                SkipWhitespace();
                if (!Consume(';')) {
                    SkipWhitespace();
                    if (Peek() != '}') return Fail(L"每条动作必须以分号结束", error);
                }

                std::wstring validation_error;
                if (!ValidateParallelCommand(command, validation_error))
                    return Fail(L"动作 " + std::to_wstring(*index) + L" 非法：" + validation_error, error);
                indexed_commands.emplace_back(*index, std::move(command));
            }

            std::sort(indexed_commands.begin(), indexed_commands.end(), [](const auto& left, const auto& right) {
                return left.first < right.first;
            });
            std::vector<std::string> commands;
            commands.reserve(indexed_commands.size());
            for (std::size_t i = 0; i < indexed_commands.size(); ++i) {
                if (indexed_commands[i].first != i)
                    return Fail(L"动作序号必须从 0 开始且连续，缺少序号 " + std::to_wstring(i), error);
                commands.push_back(std::move(indexed_commands[i].second));
            }
            if (commands.empty()) return Fail(L"@" + Utf8ToWide(modifier) + L" 不能为空", error);

            if (modifier == "OnPressed") {
                if (saw_pressed) return Fail(L"@OnPressed 只能出现一次", error);
                saw_pressed = true;
                result.on_pressed = std::move(commands);
            } else {
                if (saw_released) return Fail(L"@OnReleased 只能出现一次", error);
                saw_released = true;
                result.on_released = std::move(commands);
            }
        }
        if (!saw_pressed || !saw_released)
            return Fail(L"脚本必须同时包含 @OnPressed 和 @OnReleased", error);
        return true;
    }

private:
    void SkipWhitespace()
    {
        while (!AtEnd() && std::isspace(static_cast<unsigned char>(source_[position_])) != 0) ++position_;
    }

    bool AtEnd() const { return position_ >= source_.size(); }
    char Peek() const { return AtEnd() ? '\0' : source_[position_]; }

    bool Consume(char expected)
    {
        if (Peek() != expected) return false;
        ++position_;
        return true;
    }

    std::string ParseIdentifier()
    {
        const std::size_t start = position_;
        while (!AtEnd()) {
            const unsigned char ch = static_cast<unsigned char>(source_[position_]);
            if (std::isalnum(ch) == 0 && ch != '_') break;
            ++position_;
        }
        return source_.substr(start, position_ - start);
    }

    std::optional<std::size_t> ParseIndex()
    {
        if (AtEnd() || std::isdigit(static_cast<unsigned char>(Peek())) == 0) return std::nullopt;
        std::size_t value = 0;
        while (!AtEnd() && std::isdigit(static_cast<unsigned char>(Peek())) != 0) {
            value = value * 10 + static_cast<std::size_t>(source_[position_] - '0');
            ++position_;
            if (value > 100'000) return std::nullopt;
        }
        return value;
    }

    bool ParseString(std::string& result, std::wstring& error)
    {
        if (!Consume('"')) return Fail(L"动作命令必须使用双引号包裹", error);
        bool escaped = false;
        while (!AtEnd()) {
            const char ch = source_[position_++];
            if (escaped) {
                switch (ch) {
                case 'n': result.push_back('\n'); break;
                case 'r': result.push_back('\r'); break;
                case 't': result.push_back('\t'); break;
                case '\\': result.push_back('\\'); break;
                case '"': result.push_back('"'); break;
                default: result.push_back(ch); break;
                }
                escaped = false;
                continue;
            }
            if (ch == '\\') {
                escaped = true;
                continue;
            }
            if (ch == '"') return true;
            result.push_back(ch);
        }
        return Fail(L"命令字符串缺少结束双引号", error);
    }

    std::size_t CurrentLine() const
    {
        return 1 + static_cast<std::size_t>(std::count(source_.begin(), source_.begin() + position_, '\n'));
    }

    bool Fail(const std::wstring& message, std::wstring& error) const
    {
        error = L"第 " + std::to_wstring(CurrentLine()) + L" 行：" + message;
        return false;
    }

    std::string source_;
    std::size_t position_ = 0;
};

bool ReadAndParseScript(const std::filesystem::path& path, parsed_script& parsed, std::wstring& error)
{
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        error = L"无法打开脚本文件";
        return false;
    }
    std::string text((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB && static_cast<unsigned char>(text[2]) == 0xBF) {
        text.erase(0, 3);
    }
    parsed = {};
    return parser(std::move(text)).Parse(parsed, error);
}

void RefreshPublicScriptsLocked()
{
    g_public_scripts.clear();
    g_public_scripts.reserve(g_scripts.size());
    for (const auto& script : g_scripts) g_public_scripts.push_back(script.public_state);
}

bool ReloadRecordLocked(script_record& record, std::wstring& error)
{
    parsed_script parsed;
    if (!ReadAndParseScript(record.public_state.path, parsed, error)) {
        record.parsed = {};
        record.public_state.valid = false;
        record.public_state.status = error;
        record.public_state.pressed_command_count = 0;
        record.public_state.released_command_count = 0;
        return false;
    }
    record.parsed = std::move(parsed);
    record.public_state.valid = true;
    record.public_state.status = L"脚本有效";
    record.public_state.pressed_command_count = record.parsed.on_pressed.size();
    record.public_state.released_command_count = record.parsed.on_released.size();
    return true;
}

std::filesystem::path ResolveCfgDirectory()
{
    const std::wstring saved = strikesense::LoadSavedCfgPath();
    if (!saved.empty() && std::filesystem::exists(saved)) return std::filesystem::path(saved);
    const std::wstring steam = strikesense::GetSteamPathFromRegistry();
    if (steam.empty()) return {};
    const std::wstring install = strikesense::FindCS2InstallDir(steam);
    if (install.empty()) return {};
    return std::filesystem::path(strikesense::GetCS2CfgPath(install));
}

bool InstallAutoexecBinding(const std::filesystem::path& cfg_directory)
{
    const std::filesystem::path autoexec = cfg_directory / L"autoexec.cfg";
    std::string content;
    {
        std::ifstream input(autoexec, std::ios::binary);
        if (input.is_open()) content.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    }

    const std::size_t block_start = content.find(kManagedBlockStart);
    if (block_start != std::string::npos) {
        const std::size_t block_end = content.find(kManagedBlockEnd, block_start);
        if (block_end != std::string::npos) {
            std::size_t erase_end = block_end + std::char_traits<char>::length(kManagedBlockEnd);
            while (erase_end < content.size() && (content[erase_end] == '\r' || content[erase_end] == '\n')) ++erase_end;
            content.erase(block_start, erase_end - block_start);
        }
    }
    if (!content.empty() && content.back() != '\n') content.push_back('\n');
    content += kManagedBlockStart;
    content += "\nbind kp_9 \"exec StrikeTicker.cfg\"\n";
    content += kManagedBlockEnd;
    content.push_back('\n');

    std::ofstream output(autoexec, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) return false;
    output.write(content.data(), static_cast<std::streamsize>(content.size()));
    output.flush();
    return output.good();
}

bool PrepareTickerFiles()
{
    const std::filesystem::path cfg_directory = ResolveCfgDirectory();
    if (cfg_directory.empty() || !std::filesystem::exists(cfg_directory)) {
        std::lock_guard lock(g_state_mutex);
        g_runtime_status = L"未找到 CS2 cfg 目录";
        std::cout << "[CScript] 未找到 CS2 cfg 目录，无法启动 ticker。" << std::endl;
        return false;
    }

    const std::filesystem::path ticker_path = cfg_directory / L"StrikeTicker.cfg";
    {
        std::ofstream ticker(ticker_path, std::ios::binary | std::ios::trunc);
        if (!ticker.is_open()) {
            std::lock_guard lock(g_state_mutex);
            g_runtime_status = L"无法创建 StrikeTicker.cfg";
            std::wcout << L"[CScript] 无法创建 ticker 文件：" << ticker_path.wstring() << std::endl;
            return false;
        }
    }

    const bool autoexec_ok = InstallAutoexecBinding(cfg_directory);
    {
        std::lock_guard lock(g_state_mutex);
        g_ticker_cfg_path = ticker_path;
        g_runtime_status = autoexec_ok
            ? L"Ticker 已就绪：kp_9 / 64Hz"
            : L"Ticker 已就绪，但 autoexec.cfg 绑定写入失败";
    }
    std::wcout << L"[CScript] StrikeTicker.cfg 已就绪：" << ticker_path.wstring() << std::endl;
    std::cout << (autoexec_ok
        ? "[CScript] 已向 autoexec.cfg 写入 kp_9 ticker 绑定。"
        : "[CScript] autoexec.cfg 写入失败，请手动绑定 kp_9。") << std::endl;
    return true;
}

void ClearQueuesLocked()
{
    g_lanes.clear();
    g_ready_lanes.clear();
    g_physical_down.clear();
    g_clear_pending = false;
}

void QueueSequenceLocked(const script_record& script, bool pressed)
{
    const auto& commands = pressed ? script.parsed.on_pressed : script.parsed.on_released;
    if (!script.public_state.valid || script.public_state.source_key.empty() || commands.empty()) return;

    command_sequence sequence;
    sequence.script_id = script.public_state.id;
    sequence.phase = pressed ? "pressed" : "released";
    sequence.source_key = WideToUtf8(script.public_state.source_key);
    sequence.commands = commands;

    auto& lane = g_lanes[script.public_state.id];
    lane.sequences.push_back(std::move(sequence));
    if (!lane.ready) {
        lane.ready = true;
        g_ready_lanes.push_back(script.public_state.id);
    }
    std::wcout << L"[CScript] 已入队：" << script.public_state.source_key
               << (pressed ? L" 按下" : L" 松开")
               << L"，动作数=" << commands.size() << std::endl;
}

void HandlePhysicalKey(UINT virtual_key, bool extended_key, bool pressed)
{
    std::lock_guard lock(g_state_mutex);
    for (const auto& script : g_scripts) {
        if (script.public_state.virtual_key != virtual_key ||
            script.public_state.extended_key != extended_key) continue;

        bool& was_down = g_physical_down[script.public_state.id];
        if (pressed) {
            if (was_down) continue;
            was_down = true;
            QueueSequenceLocked(script, true);
        } else {
            if (!was_down) continue;
            was_down = false;
            QueueSequenceLocked(script, false);
        }
    }
    g_worker_cv.notify_one();
}

void NormalizeLowLevelKey(const KBDLLHOOKSTRUCT& key, UINT& virtual_key, bool& extended_key)
{
    virtual_key = key.vkCode;
    extended_key = (key.flags & LLKHF_EXTENDED) != 0;
    if (virtual_key == VK_CONTROL)
        virtual_key = extended_key ? VK_RCONTROL : VK_LCONTROL;
    else if (virtual_key == VK_MENU)
        virtual_key = extended_key ? VK_RMENU : VK_LMENU;
    else if (virtual_key == VK_SHIFT) {
        const UINT mapped = MapVirtualKeyW(key.scanCode, MAPVK_VSC_TO_VK_EX);
        virtual_key = mapped == VK_RSHIFT ? VK_RSHIFT : VK_LSHIFT;
        extended_key = false;
    }
}

LRESULT CALLBACK LowLevelKeyboardProc(int code, WPARAM w_param, LPARAM l_param)
{
    if (code == HC_ACTION && g_enabled.load() && IsRageModeEnabled()) {
        const auto* key = reinterpret_cast<const KBDLLHOOKSTRUCT*>(l_param);
        if (key && (key->flags & LLKHF_INJECTED) == 0 && IsCS2WindowActive()) {
            const bool pressed = w_param == WM_KEYDOWN || w_param == WM_SYSKEYDOWN;
            const bool released = w_param == WM_KEYUP || w_param == WM_SYSKEYUP;
            if (pressed || released) {
                UINT virtual_key = 0;
                bool extended_key = false;
                NormalizeLowLevelKey(*key, virtual_key, extended_key);
                HandlePhysicalKey(virtual_key, extended_key, pressed);
            }
        }
    }
    return CallNextHookEx(nullptr, code, w_param, l_param);
}

void SendTickerKey()
{
    const WORD scan_code = static_cast<WORD>(MapVirtualKeyW(kTickerVirtualKey, MAPVK_VK_TO_VSC));
    std::array<INPUT, 2> inputs{};
    for (auto& input : inputs) {
        input.type = INPUT_KEYBOARD;
        input.ki.wVk = kTickerVirtualKey;
        input.ki.wScan = scan_code;
        input.ki.dwFlags = KEYEVENTF_SCANCODE;
    }
    inputs[1].ki.dwFlags |= KEYEVENTF_KEYUP;
    const UINT sent = SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
    if (sent != inputs.size())
        std::cout << "[CScript] kp_9 SendInput 未完整发送，错误码=" << GetLastError() << std::endl;
}

bool WriteTickerFile(const std::string& content)
{
    std::filesystem::path path;
    {
        std::lock_guard lock(g_state_mutex);
        path = g_ticker_cfg_path;
    }
    if (path.empty()) return false;
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) return false;
    output.write(content.data(), static_cast<std::streamsize>(content.size()));
    output.flush();
    return output.good();
}

std::optional<std::string> TakeNextTickerCommand()
{
    std::lock_guard lock(g_state_mutex);
    if (g_clear_pending) {
        g_clear_pending = false;
        return std::string{};
    }

    while (!g_ready_lanes.empty()) {
        const std::uint64_t script_id = g_ready_lanes.front();
        g_ready_lanes.pop_front();
        auto lane_it = g_lanes.find(script_id);
        if (lane_it == g_lanes.end() || lane_it->second.sequences.empty()) continue;

        script_lane& lane = lane_it->second;
        command_sequence& sequence = lane.sequences.front();
        const std::size_t command_index = sequence.next_command;
        const bool final_command = command_index + 1 >= sequence.commands.size();
        std::string output = sequence.commands[command_index];
        if (!output.empty() && output.back() != ';') output.push_back(';');
        output += "echoln \"[cscript]" + sequence.phase + " " + sequence.source_key
            + " has been executed " + std::to_string(command_index);
        if (final_command) output.push_back('!');
        output += "\"\n";

        ++sequence.next_command;
        if (final_command) lane.sequences.pop_front();
        if (!lane.sequences.empty()) {
            g_ready_lanes.push_back(script_id);
        } else {
            lane.ready = false;
            g_lanes.erase(lane_it);
        }
        g_clear_pending = final_command;
        return output;
    }
    return std::nullopt;
}

void WorkerLoop()
{
    std::cout << "[CScript] 64Hz ticker 线程已启动。" << std::endl;
    auto next_tick = clock_type::now();
    bool was_game_active = false;
    while (g_worker_running.load()) {
        next_tick += kTickerInterval;
        const bool game_active = g_enabled.load() && IsRageModeEnabled() && IsCS2WindowActive();
        if (!game_active && was_game_active) {
            {
                std::lock_guard lock(g_state_mutex);
                ClearQueuesLocked();
            }
            WriteTickerFile("");
            std::cout << "[CScript] CS2 不再位于前台，已清空输入状态与待执行队列。" << std::endl;
        }
        was_game_active = game_active;
        if (game_active) {
            const std::optional<std::string> command = TakeNextTickerCommand();
            if (command.has_value()) {
                if (!WriteTickerFile(*command)) {
                    std::cout << "[CScript] StrikeTicker.cfg 写入失败。" << std::endl;
                    std::lock_guard lock(g_state_mutex);
                    g_runtime_status = L"StrikeTicker.cfg 写入失败";
                }
            }
            SendTickerKey();
        }

        std::unique_lock lock(g_state_mutex);
        g_worker_cv.wait_until(lock, next_tick, [] { return !g_worker_running.load(); });
        const auto now = clock_type::now();
        if (next_tick + std::chrono::milliseconds(100) < now) next_tick = now;
    }
    std::cout << "[CScript] 64Hz ticker 线程已退出。" << std::endl;
}

bool StartRuntime()
{
    if (!PrepareTickerFiles()) return false;
    if (!g_keyboard_hook) {
        g_keyboard_hook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandleW(nullptr), 0);
        if (!g_keyboard_hook) {
            std::cout << "[CScript] 键盘钩子安装失败，错误码=" << GetLastError() << std::endl;
            std::lock_guard lock(g_state_mutex);
            g_runtime_status = L"键盘钩子安装失败";
            return false;
        }
        std::cout << "[CScript] 全局键盘钩子已安装。" << std::endl;
    }
    if (!g_worker_running.exchange(true)) g_worker = std::thread(WorkerLoop);
    consolelog::SetCscriptReaderNeeded(true);
    return true;
}

void StopRuntime()
{
    consolelog::SetCscriptReaderNeeded(false);
    if (g_keyboard_hook) {
        UnhookWindowsHookEx(g_keyboard_hook);
        g_keyboard_hook = nullptr;
        std::cout << "[CScript] 全局键盘钩子已卸载。" << std::endl;
    }
    if (g_worker_running.exchange(false)) {
        g_worker_cv.notify_all();
        if (g_worker.joinable()) g_worker.join();
    }
    {
        std::lock_guard lock(g_state_mutex);
        ClearQueuesLocked();
    }
    if (!g_ticker_cfg_path.empty()) WriteTickerFile("");
}

} // namespace

void LoadConfig()
{
    config::EnsureDirectoriesExist();
    EnsureExampleScript();
    std::lock_guard lock(g_state_mutex);
    g_scripts.clear();
    g_next_script_id = 1;

    const std::filesystem::path path = ConfigPath();
    if (!std::filesystem::exists(path)) {
        RefreshPublicScriptsLocked();
        g_enabled.store(false);
        std::cout << "[CScript] 配置不存在，使用默认配置。" << std::endl;
        return;
    }
    try {
        std::ifstream input(path);
        nlohmann::json json;
        input >> json;
        if (json.contains("enabled") && json["enabled"].is_boolean())
            g_enabled.store(json["enabled"].get<bool>());
        if (json.contains("scripts") && json["scripts"].is_array()) {
            for (const auto& item : json["scripts"]) {
                if (!item.contains("path") || !item["path"].is_string()) continue;
                script_record record;
                record.public_state.id = item.value("id", g_next_script_id++);
                record.public_state.path = Utf8ToWide(item["path"].get<std::string>());
                record.public_state.virtual_key = item.value("virtual_key", 0u);
                record.public_state.extended_key = item.value("extended_key", false);
                record.public_state.source_key = VirtualKeyToSourceName(
                    record.public_state.virtual_key, record.public_state.extended_key);
                std::wstring parse_error;
                ReloadRecordLocked(record, parse_error);
                g_next_script_id = (std::max)(g_next_script_id, record.public_state.id + 1);
                g_scripts.push_back(std::move(record));
            }
        }
        RefreshPublicScriptsLocked();
        std::cout << "[CScript] 已加载脚本数量：" << g_scripts.size() << std::endl;
    } catch (const std::exception& exception) {
        g_enabled.store(false);
        g_scripts.clear();
        RefreshPublicScriptsLocked();
        std::cout << "[CScript] 配置加载失败：" << exception.what() << std::endl;
    }
}

void SaveConfig()
{
    config::EnsureDirectoriesExist();
    nlohmann::json json;
    json["enabled"] = g_enabled.load();
    json["ticker_key"] = "kp_9";
    json["ticker_hz"] = kTickerHz;
    json["scripts"] = nlohmann::json::array();
    {
        std::lock_guard lock(g_state_mutex);
        for (const auto& script : g_scripts) {
            json["scripts"].push_back({
                {"id", script.public_state.id},
                {"path", WideToUtf8(script.public_state.path)},
                {"virtual_key", script.public_state.virtual_key},
                {"extended_key", script.public_state.extended_key}
            });
        }
    }
    std::ofstream output(ConfigPath());
    if (output.is_open()) {
        output << json.dump(2);
        std::cout << "[CScript] 配置已保存。" << std::endl;
    } else {
        std::cout << "[CScript] 配置保存失败。" << std::endl;
    }
}

bool SetEnabled(bool enabled)
{
    if (enabled && !IsRageModeEnabled()) {
        std::cout << "[CScript] 超频配置未开启，拒绝启动 ticker。" << std::endl;
        return false;
    }
    if (enabled) {
        if (g_enabled.load() && g_worker_running.load() && g_keyboard_hook) return true;
        if (!StartRuntime()) {
            g_enabled.store(false);
            SaveConfig();
            return false;
        }
        g_enabled.store(true);
    } else {
        g_enabled.store(false);
        StopRuntime();
    }
    SaveConfig();
    std::cout << "[CScript] 多绑定脚本支持已切换为：" << (enabled ? "开启" : "关闭") << std::endl;
    return g_enabled.load() == enabled;
}

bool IsEnabled()
{
    return g_enabled.load();
}

void StopForRageDisabled()
{
    StopRuntime();
    std::cout << "[CScript] 超频配置关闭，ticker 已停止，但保留开关偏好。" << std::endl;
}

void Shutdown()
{
    StopRuntime();
}

const std::vector<mounted_script>& MountedScripts()
{
    return g_public_scripts;
}

bool AddMountedScript(const std::filesystem::path& path, std::wstring* error)
{
    std::wstring extension = path.extension().wstring();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](wchar_t ch) {
        return static_cast<wchar_t>(std::towlower(ch));
    });
    if (extension != L".cscript") {
        if (error) *error = L"仅支持 .cscript 文件";
        return false;
    }
    std::wstring parse_error;
    bool added = false;
    {
        std::lock_guard lock(g_state_mutex);
        const auto existing = std::find_if(g_scripts.begin(), g_scripts.end(), [&](const script_record& script) {
            return std::filesystem::path(script.public_state.path) == path;
        });
        if (existing != g_scripts.end()) {
            const bool valid = ReloadRecordLocked(*existing, parse_error);
            RefreshPublicScriptsLocked();
            if (error) *error = valid ? L"" : parse_error;
            return valid;
        }
        script_record record;
        record.public_state.id = g_next_script_id++;
        record.public_state.path = path.wstring();
        const bool valid = ReloadRecordLocked(record, parse_error);
        g_scripts.push_back(std::move(record));
        RefreshPublicScriptsLocked();
        added = true;
        if (!valid && error) *error = parse_error;
    }
    if (added) SaveConfig();
    std::wcout << L"[CScript] 已挂载脚本：" << path.wstring() << std::endl;
    return parse_error.empty();
}

void RemoveMountedScript(std::size_t index)
{
    {
        std::lock_guard lock(g_state_mutex);
        if (index >= g_scripts.size()) return;
        const std::uint64_t id = g_scripts[index].public_state.id;
        g_scripts.erase(g_scripts.begin() + static_cast<std::ptrdiff_t>(index));
        g_lanes.erase(id);
        g_physical_down.erase(id);
        g_ready_lanes.erase(std::remove(g_ready_lanes.begin(), g_ready_lanes.end(), id), g_ready_lanes.end());
        RefreshPublicScriptsLocked();
    }
    SaveConfig();
    std::cout << "[CScript] 已卸载脚本。" << std::endl;
}

bool ReloadMountedScript(std::size_t index, std::wstring* error)
{
    bool valid = false;
    std::wstring parse_error;
    {
        std::lock_guard lock(g_state_mutex);
        if (index >= g_scripts.size()) {
            if (error) *error = L"脚本索引无效";
            return false;
        }
        valid = ReloadRecordLocked(g_scripts[index], parse_error);
        RefreshPublicScriptsLocked();
    }
    if (error) *error = parse_error;
    std::cout << "[CScript] 脚本重载结果：" << (valid ? "成功" : "失败") << std::endl;
    return valid;
}

bool SetScriptKey(std::size_t index, UINT virtual_key, bool extended_key, std::wstring* error)
{
    const std::wstring source_name = VirtualKeyToSourceName(virtual_key, extended_key);
    if (source_name.empty()) {
        if (error) *error = L"该按键无法映射为起源引擎按键名";
        return false;
    }
    if (source_name == kTickerSourceKey) {
        if (error) *error = L"kp_9 已被 64Hz ticker 保留，请选择其他按键";
        return false;
    }
    {
        std::lock_guard lock(g_state_mutex);
        if (index >= g_scripts.size()) {
            if (error) *error = L"脚本索引无效";
            return false;
        }
        g_scripts[index].public_state.virtual_key = virtual_key;
        g_scripts[index].public_state.extended_key = extended_key;
        g_scripts[index].public_state.source_key = source_name;
        RefreshPublicScriptsLocked();
    }
    SaveConfig();
    std::wcout << L"[CScript] 脚本按键已绑定为：" << source_name << std::endl;
    return true;
}

std::wstring GetDefaultScriptDir()
{
    return (BaseDirectory() / L"cscript").wstring();
}

std::wstring GetTickerCfgPath()
{
    std::lock_guard lock(g_state_mutex);
    return g_ticker_cfg_path.wstring();
}

std::wstring GetLastRuntimeStatus()
{
    std::lock_guard lock(g_state_mutex);
    return g_runtime_status;
}

void ProcessConsoleLine(const std::wstring& line)
{
    const std::size_t marker = line.find(L"[cscript]");
    if (marker == std::wstring::npos) return;
    const std::wstring acknowledgement = line.substr(marker);
    {
        std::lock_guard lock(g_state_mutex);
        g_runtime_status = L"最近确认：" + acknowledgement;
    }
    std::wcout << L"[CScript] 已从 console.log 收到执行确认：" << acknowledgement << std::endl;
}

std::wstring VirtualKeyToSourceName(UINT virtual_key, bool extended_key)
{
    (void)extended_key;
    if (virtual_key >= 'A' && virtual_key <= 'Z')
        return std::wstring(1, static_cast<wchar_t>(L'a' + virtual_key - 'A'));
    if (virtual_key >= '0' && virtual_key <= '9')
        return std::wstring(1, static_cast<wchar_t>(L'0' + virtual_key - '0'));
    if (virtual_key >= VK_NUMPAD0 && virtual_key <= VK_NUMPAD9)
        return L"kp_" + std::to_wstring(virtual_key - VK_NUMPAD0);
    if (virtual_key >= VK_F1 && virtual_key <= VK_F12)
        return L"f" + std::to_wstring(virtual_key - VK_F1 + 1);

    switch (virtual_key) {
    case VK_LCONTROL: return L"ctrl";
    case VK_RCONTROL: return L"rctrl";
    case VK_LSHIFT: return L"shift";
    case VK_RSHIFT: return L"rshift";
    case VK_LMENU: return L"alt";
    case VK_RMENU: return L"ralt";
    case VK_CAPITAL: return L"capslock";
    case VK_TAB: return L"tab";
    case VK_SPACE: return L"space";
    case VK_RETURN: return L"enter";
    case VK_BACK: return L"backspace";
    case VK_ESCAPE: return L"escape";
    case VK_INSERT: return L"ins";
    case VK_DELETE: return L"del";
    case VK_HOME: return L"home";
    case VK_END: return L"end";
    case VK_PRIOR: return L"pgup";
    case VK_NEXT: return L"pgdn";
    case VK_UP: return L"uparrow";
    case VK_DOWN: return L"downarrow";
    case VK_LEFT: return L"leftarrow";
    case VK_RIGHT: return L"rightarrow";
    case VK_OEM_MINUS: return L"-";
    case VK_OEM_PLUS: return L"=";
    case VK_ADD: return L"kp_plus";
    case VK_SUBTRACT: return L"kp_minus";
    case VK_MULTIPLY: return L"kp_multiply";
    case VK_DIVIDE: return L"kp_slash";
    case VK_DECIMAL: return L"kp_del";
    default: return L"";
    }
}

bool NormalizeWindowKey(WPARAM w_param, LPARAM l_param, UINT& virtual_key, bool& extended_key)
{
    virtual_key = static_cast<UINT>(w_param);
    extended_key = (static_cast<std::uint64_t>(l_param) & (1ull << 24)) != 0;
    const UINT scan_code = (static_cast<UINT>(l_param) >> 16) & 0xFFu;
    if (virtual_key == VK_CONTROL)
        virtual_key = extended_key ? VK_RCONTROL : VK_LCONTROL;
    else if (virtual_key == VK_MENU)
        virtual_key = extended_key ? VK_RMENU : VK_LMENU;
    else if (virtual_key == VK_SHIFT) {
        virtual_key = MapVirtualKeyW(scan_code, MAPVK_VSC_TO_VK_EX) == VK_RSHIFT ? VK_RSHIFT : VK_LSHIFT;
        extended_key = false;
    }
    return !VirtualKeyToSourceName(virtual_key, extended_key).empty();
}

} // namespace cscript
