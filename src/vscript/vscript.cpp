#include "vscript_internal.h"

#include "module_notifications.h"
#include "textgui_overlay.h"

#include <algorithm>
#include <chrono>
#include <codecvt>
#include <deque>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <thread>

namespace fs = std::filesystem;

#ifndef STRIKESENSE_BUILD_CODE
#define STRIKESENSE_BUILD_CODE 0
#endif

namespace vscript::detail {

HINSTANCE s_instance = nullptr;
HWND s_owner = nullptr;
buildcode s_runtimeCapability = buildcode::user;
bool s_oemValid = false;
std::vector<mounted_script> s_mounted;
std::map<std::wstring, value> s_vars;
std::map<std::wstring, value> s_prevVars;
nlohmann::json s_gsiSnapshot = nlohmann::json::object();
std::unordered_map<int, image_window> s_images;
std::unordered_map<int, sound_slot> s_sounds;
std::recursive_mutex s_mutex;
std::unordered_map<std::wstring, std::wstring> s_scriptCache;
std::unordered_map<std::wstring, ULONGLONG> s_cooldownTicks;
std::unordered_set<std::wstring> s_stateKeys;
std::unordered_set<std::wstring> s_constVars;
std::deque<std::wstring> s_consoleLogQueue;
execution_context* s_activeExecution = nullptr;
bool s_currentPrivilegedAllowed = false;
std::wstring s_currentScriptPath;
int s_weaponFireCount = 0;
int s_weaponReloadCount = 0;
int s_weaponReserveDropCount = 0;
weapon_snapshot s_lastWeaponSnapshot;

namespace {

struct execution_scope_guard {
    execution_context* previous = nullptr;

    explicit execution_scope_guard(execution_context& current)
        : previous(s_activeExecution)
    {
        s_activeExecution = &current;
    }

    ~execution_scope_guard()
    {
        s_activeExecution = previous;
    }
};

} // namespace

std::wstring Utf8ToWide(const std::string& s)
{
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring out(len, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), len);
    return out;
}

std::string WideToUtf8(const std::wstring& s)
{
    if (s.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0, nullptr, nullptr);
    std::string out(len, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), out.data(), len, nullptr, nullptr);
    return out;
}

fs::path BaseDir()
{
    wchar_t profile[MAX_PATH] = {};
    GetEnvironmentVariableW(L"USERPROFILE", profile, MAX_PATH);
    return fs::path(profile) / L"StrikeSense";
}

std::wstring NowStamp()
{
    SYSTEMTIME st{};
    GetLocalTime(&st);
    wchar_t buf[20]{};
    swprintf_s(buf, L"%04u%02u%02u%02u%02u", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);
    return buf;
}

int DaysForType(const std::wstring& type)
{
    if (type == L"24h") return 1;
    if (type == L"7d") return 7;
    if (type == L"1m") return 31;
    if (type == L"6m") return 183;
    if (type == L"1y") return 366;
    if (type == L"10y") return 3653;
    if (type == L"50y") return 18263;
    if (type == L"forever") return 365000;
    return 0;
}

int DateToDays(int y, int m, int d)
{
    std::tm t{};
    t.tm_year = y - 1900;
    t.tm_mon = m - 1;
    t.tm_mday = d;
    t.tm_isdst = -1;
    return (int)(std::mktime(&t) / 86400);
}

std::time_t MakeTime(int y, int m, int d, int hh, int mm)
{
    std::tm t{};
    t.tm_year = y - 1900;
    t.tm_mon = m - 1;
    t.tm_mday = d;
    t.tm_hour = hh;
    t.tm_min = mm;
    t.tm_isdst = -1;
    return std::mktime(&t);
}

std::time_t CurrentTime()
{
    SYSTEMTIME st{};
    GetLocalTime(&st);
    return MakeTime(st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);
}

uint32_t Fnv1a(const std::string& s)
{
    uint32_t h = 2166136261u;
    for (unsigned char c : s) {
        h ^= c;
        h *= 16777619u;
    }
    return h;
}

std::string Encode64(const std::string& raw)
{
    std::string out;
    int val = 0;
    int bits = -6;
    for (unsigned char c : raw) {
        val = (val << 8) + c;
        bits += 8;
        while (bits >= 0) {
            out.push_back(k_alphabet[(val >> bits) & 0x3F]);
            bits -= 6;
        }
    }
    if (bits > -6) out.push_back(k_alphabet[((val << 8) >> (bits + 8)) & 0x3F]);
    return out;
}

std::optional<std::string> Decode64(const std::string& enc)
{
    std::vector<int> table(256, -1);
    for (int i = 0; i < 64; ++i) table[(unsigned char)k_alphabet[i]] = i;
    std::string out;
    int val = 0;
    int bits = -8;
    for (unsigned char c : enc) {
        if (table[c] < 0) return std::nullopt;
        val = (val << 6) + table[c];
        bits += 6;
        if (bits >= 0) {
            out.push_back(char((val >> bits) & 0xFF));
            bits -= 8;
        }
    }
    return out;
}

std::string XorPayload(std::string raw)
{
    const std::string key = "StrikeSenseOEMKey2026";
    for (size_t i = 0; i < raw.size(); ++i) {
        raw[i] = char((unsigned char)raw[i] ^ (unsigned char)key[i % key.size()] ^ (unsigned char)((i * 29 + 17) & 0xFF));
    }
    return raw;
}

std::wstring MakeOemKey(const std::wstring& stamp, const std::wstring& type)
{
    std::string body = "SSOEM1|" + WideToUtf8(stamp) + "|" + WideToUtf8(type);
    uint32_t check = Fnv1a(body + "|StrikeSense");
    std::ostringstream oss;
    oss << body << "|" << std::hex << check;
    return Utf8ToWide(Encode64(XorPayload(oss.str())));
}

bool ParseOemKey(const std::wstring& key)
{
    auto decoded = Decode64(WideToUtf8(key));
    if (!decoded) return false;
    std::string plain = XorPayload(*decoded);
    std::vector<std::string> parts;
    std::stringstream ss(plain);
    std::string item;
    while (std::getline(ss, item, '|')) parts.push_back(item);
    if (parts.size() != 4 || parts[0] != "SSOEM1") return false;

    std::string body = parts[0] + "|" + parts[1] + "|" + parts[2];
    std::ostringstream check;
    check << std::hex << Fnv1a(body + "|StrikeSense");
    if (check.str() != parts[3]) return false;
    if (parts[1].size() != 12) return false;

    int y = std::stoi(parts[1].substr(0, 4));
    int m = std::stoi(parts[1].substr(4, 2));
    int d = std::stoi(parts[1].substr(6, 2));
    int hh = std::stoi(parts[1].substr(8, 2));
    int mm = std::stoi(parts[1].substr(10, 2));
    int days = DaysForType(Utf8ToWide(parts[2]));
    if (days <= 0) return false;
    std::time_t begin = MakeTime(y, m, d, hh, mm);
    std::time_t now = CurrentTime();
    return now >= begin && now <= begin + static_cast<std::time_t>(days) * 86400;
}

std::wstring ReadAllWide(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return L"";
    std::string raw((std::istreambuf_iterator<char>(in)), {});
    if (raw.size() >= 3 && (unsigned char)raw[0] == 0xEF && (unsigned char)raw[1] == 0xBB && (unsigned char)raw[2] == 0xBF) {
        raw.erase(0, 3);
    }
    return Utf8ToWide(raw);
}

std::wstring LoadScriptCached(const std::wstring& path)
{
    auto it = s_scriptCache.find(path);
    if (it != s_scriptCache.end()) return it->second;
    std::wstring text = ReadAllWide(path);
    s_scriptCache[path] = text;
    return text;
}

std::wstring StripComments(const std::wstring& script)
{
    std::wstring out;
    bool line = false;
    bool block = false;
    bool text = false;
    for (size_t i = 0; i < script.size(); ++i) {
        wchar_t c = script[i];
        wchar_t n = (i + 1 < script.size()) ? script[i + 1] : L'\0';
        if (line) {
            if (c == L'\n' || c == L'\r') {
                line = false;
                out.push_back(c);
            }
            continue;
        }
        if (block) {
            if (c == L'*' && n == L'/') {
                block = false;
                ++i;
            }
            continue;
        }
        if (!text && c == L'/' && n == L'/') {
            line = true;
            ++i;
            continue;
        }
        if (!text && c == L'/' && n == L'*') {
            block = true;
            ++i;
            continue;
        }
        if (c == L'"' && (i == 0 || script[i - 1] != L'\\')) text = !text;
        out.push_back(c);
    }
    return out;
}

std::wstring ReadMetaValue(const std::wstring& line, const std::wstring& key)
{
    std::wstring t = Trim(line);
    if (t.rfind(L"//", 0) == 0) t = Trim(t.substr(2));
    if (t.rfind(key, 0) != 0) return L"";
    size_t pos = t.find(L':');
    if (pos == std::wstring::npos) return L"";
    return Trim(t.substr(pos + 1));
}

std::wstring GetWindowsUserName()
{
    wchar_t user[MAX_PATH] = {};
    DWORD size = MAX_PATH;
    if (GetEnvironmentVariableW(L"USERNAME", user, size) > 0) return user;
    return L"";
}

bool EqualsIgnoreCase(const std::wstring& a, const std::wstring& b)
{
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (towlower(a[i]) != towlower(b[i])) return false;
    }
    return true;
}

std::vector<std::wstring> SplitMetaTokens(const std::wstring& text)
{
    std::vector<std::wstring> tokens;
    std::wstring current;
    for (wchar_t c : text) {
        if (c == L';' || c == L',') {
            std::wstring token = Trim(current);
            if (!token.empty()) tokens.push_back(token);
            current.clear();
            continue;
        }
        current.push_back(c);
    }
    std::wstring token = Trim(current);
    if (!token.empty()) tokens.push_back(token);
    return tokens;
}

execution_context& CurrentExecution()
{
    static execution_context fallback;
    return s_activeExecution ? *s_activeExecution : fallback;
}

bool ScriptUsesPrivilegedApis(const std::wstring& script)
{
    const std::wstring clean = StripComments(script);
    for (const wchar_t* apiName : k_privilegedApiNames) {
        std::wstring pattern = std::wstring(apiName) + L"(";
        if (clean.find(pattern) != std::wstring::npos) return true;
    }
    return false;
}

void ApplyModifierMetadata(mounted_script& script)
{
    script.selfUser.clear();
    for (const auto& token : SplitMetaTokens(script.modifier)) {
        size_t pos = token.find(L'=');
        if (pos == std::wstring::npos) continue;
        std::wstring key = Trim(token.substr(0, pos));
        std::wstring value = Trim(token.substr(pos + 1));
        if (EqualsIgnoreCase(key, L"self_user") || EqualsIgnoreCase(key, L"self") || EqualsIgnoreCase(key, L"windows_user")) {
            script.selfUser = value;
        }
    }
}

void ResetScriptMetadata(mounted_script& script)
{
    script.name.clear();
    script.author.clear();
    script.provider.clear();
    script.version.clear();
    script.notice.clear();
    script.modifier.clear();
    script.selfUser.clear();
    script.riskNotice.clear();
    script.hasMetadataName = false;
    script.usesPrivilegedApis = false;
    script.selfAuthoredPrivileged = false;
    script.privilegedAllowed = false;
    script.dangerStyle = false;
    script.missing = false;
    script.showInTextgui = true;
}

void LoadScriptMetadata(mounted_script& script)
{
    ResetScriptMetadata(script);
    std::wstring raw = LoadScriptCached(script.path);
    std::wstringstream ss(raw);
    std::wstring line;
    while (std::getline(ss, line)) {
        std::wstring name = ReadMetaValue(line, L"@name");
        std::wstring author = ReadMetaValue(line, L"@author");
        std::wstring provider = ReadMetaValue(line, L"@provider");
        std::wstring version = ReadMetaValue(line, L"@version");
        std::wstring notice = ReadMetaValue(line, L"@notice");
        std::wstring modifier = ReadMetaValue(line, L"@modifier");
        std::wstring textgui = ReadMetaValue(line, L"@textgui");
        if (!name.empty()) {
            script.name = name;
            script.hasMetadataName = true;
        }
        if (!author.empty()) script.author = author;
        if (!provider.empty()) script.provider = provider;
        if (!version.empty()) script.version = version;
        if (!notice.empty()) script.notice = notice;
        if (!modifier.empty()) script.modifier = modifier;
        if (!textgui.empty()) {
            script.showInTextgui = !(EqualsIgnoreCase(textgui, L"false")
                || EqualsIgnoreCase(textgui, L"off") || textgui == L"0"
                || EqualsIgnoreCase(textgui, L"no"));
        }
    }
    ApplyModifierMetadata(script);
}

bool IsPrivilegedAllowedForScript(const mounted_script& script)
{
    if (!script.usesPrivilegedApis) return true;
    if (GetRuntimeCapability() >= buildcode::userdebug) return true;
    return false;
}

std::wstring BuildRiskNotice(const mounted_script& script)
{
    if (!script.usesPrivilegedApis) return L"";
    if (GetRuntimeCapability() >= buildcode::userdebug) {
        if (!script.selfUser.empty() && !script.selfAuthoredPrivileged) {
            return L"该脚本包含高权限函数。当前程序能力允许执行，但 @modifier 限制的 Windows 用户与当前系统用户不匹配，因此脚本仍会被拒绝。";
        }
        return L"该脚本包含高权限函数。当前运行能力允许挂载与执行；若声明了 @modifier:self_user，则还会继续校验目标 Windows 用户。";
    }
    return L"该脚本包含高权限函数。user 模式下不会因为脚本自带声明而获得高权限，必须通过 userdebug 或 OEM 等程序侧能力解锁。";
}

bool RefreshScriptState(mounted_script& script, bool showDialogs)
{
    script.missing = false;
    if (!fs::exists(script.path)) {
        script.missing = true;
        script.continuous = false;
        script.notice = L"脚本文件已不存在，已阻止继续执行。";
        script.riskNotice = script.notice;
        script.dangerStyle = true;
        if (showDialogs && s_owner) {
            std::wstring msg = L"脚本文件不存在，已停止挂载状态：\n" + script.path;
            MessageBoxW(s_owner, msg.c_str(), L"StrikeSense 脚本文件丢失", MB_OK | MB_ICONWARNING);
        }
        return false;
    }

    s_scriptCache[script.path] = ReadAllWide(script.path);
    LoadScriptMetadata(script);
    script.usesPrivilegedApis = ScriptUsesPrivilegedApis(s_scriptCache[script.path]);
    script.selfAuthoredPrivileged = !script.selfUser.empty() && EqualsIgnoreCase(script.selfUser, GetWindowsUserName());
    script.privilegedAllowed = IsPrivilegedAllowedForScript(script);
    if (script.privilegedAllowed && !script.selfUser.empty() && !script.selfAuthoredPrivileged) {
        script.privilegedAllowed = false;
    }
    script.riskNotice = BuildRiskNotice(script);
    script.dangerStyle = script.usesPrivilegedApis;
    if (!script.riskNotice.empty()) {
        if (!script.notice.empty()) script.notice += L"  ";
        script.notice += script.riskNotice;
    }

    if (!script.privilegedAllowed) {
        script.continuous = false;
        if (showDialogs && s_owner) {
            std::wstring msg = L"该脚本包含高权限函数，但当前程序并未授予高权限执行能力。\n\n"
                L"说明：\n"
                L"1. user 模式下，脚本内的 @modifier/self_user 只能做额外限制，不能作为提权依据。\n"
                L"2. 如需执行高权限 API，请先在程序侧解锁 userdebug 或 OEM 能力。";
            if (!script.selfUser.empty() && !script.selfAuthoredPrivileged) {
                msg += L"\n\n另外，该脚本还限制为 Windows 用户 " + script.selfUser + L" 使用，当前用户并不匹配。";
            }
            MessageBoxW(s_owner, msg.c_str(), L"StrikeSense 脚本挂载被拒绝", MB_OK | MB_ICONERROR);
        }
        return false;
    }
    return true;
}

void ResetTransientWeaponVars()
{
    SetStateVar(L"weapon_fired", BoolValue(false));
    SetStateVar(L"weapon_reloading", BoolValue(false));
    SetStateVar(L"weapon_switched", BoolValue(false));
    SetStateVar(L"weapon_clip_delta", NumberValue(0.0));
    SetStateVar(L"weapon_reserve_delta", NumberValue(0.0));
}

bool ScriptHasEdgeGuard(const std::wstring& path)
{
    std::wstring script = LoadScriptCached(path);
    return script.find(L"on:") != std::wstring::npos ||
        script.find(L"Changed(") != std::wstring::npos ||
        script.find(L"ChangedTo(") != std::wstring::npos ||
        script.find(L"TakeConsoleLog(") != std::wstring::npos ||
        script.find(L"TakeConsoleLogContains(") != std::wstring::npos ||
        script.find(L"TakeConsoleLogPrefix(") != std::wstring::npos ||
        script.find(L"ConsumeConsoleLog(") != std::wstring::npos;
}

bool ConfirmContinuousAllowed(const std::wstring& path)
{
    if (ScriptHasEdgeGuard(path)) return true;
    if (GetRuntimeCapability() == buildcode::eng) {
        std::wcout << L"[脚本权限] eng 构建允许时：轮询脚本：" << path << std::endl;
        return true;
    }
    if (GetRuntimeCapability() == buildcode::userdebug) {
        int result = MessageBoxW(
            s_owner,
            L"这个脚本没有 on:、Changed 或 ChangedTo 状态边沿判断，持续执行可能反复打开网页、重复创建文件或反复执行命令。\n\n是否仍然允许它轮询？",
            L"StrikeSense",
            MB_YESNO | MB_ICONWARNING
        );
        return result == IDYES;
    }
    MessageBoxW(
        s_owner,
        L"user 模式禁止轮询没有状态边沿判断的脚本。\n\n请使用 on:、Changed 或 ChangedTo 包裹触发逻辑，或者提升运行权限。",
        L"StrikeSense 脚本结构被拒绝",
        MB_OK | MB_ICONWARNING
    );
    return false;
}

void WriteUtf8(const fs::path& path, const std::wstring& text)
{
    fs::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out << WideToUtf8(text);
}

std::wstring SanitizeName(const std::wstring& name)
{
    std::wstring out;
    for (wchar_t c : name) {
        if ((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9')) out.push_back(c);
        else out.push_back(L'_');
    }
    if (!out.empty() && out.front() >= L'0' && out.front() <= L'9') out = L"v_" + out;
    return out;
}

void SetStateVar(const std::wstring& name, const value& v)
{
    s_vars[name] = v;
    s_stateKeys.insert(name);
}

bool IsConstVariable(const std::wstring& name)
{
    execution_context& exec = CurrentExecution();
    for (auto it = exec.localConstScopes.rbegin(); it != exec.localConstScopes.rend(); ++it) {
        if (it->find(name) != it->end()) return true;
    }
    return s_constVars.find(name) != s_constVars.end();
}

void RegisterConstVariable(const std::wstring& name)
{
    execution_context& exec = CurrentExecution();
    if (!exec.localScopes.empty()) {
        if (exec.localConstScopes.size() < exec.localScopes.size()) {
            exec.localConstScopes.resize(exec.localScopes.size());
        }
        exec.localConstScopes.back().insert(name);
        return;
    }
    s_constVars.insert(name);
}

std::wstring BuildScopedCooldownKey(const std::wstring& key)
{
    if (s_currentScriptPath.empty()) return key;
    return s_currentScriptPath + L"::" + key;
}

void FlattenJsonState(const std::wstring& prefix, const nlohmann::json& j)
{
    if (j.is_object()) {
        for (auto it = j.begin(); it != j.end(); ++it) {
            std::wstring key = Utf8ToWide(it.key());
            std::wstring next = prefix.empty() ? SanitizeName(key) : prefix + L"_" + SanitizeName(key);
            FlattenJsonState(next, it.value());
        }
        return;
    }
    if (j.is_array()) {
        for (size_t i = 0; i < j.size(); ++i) {
            FlattenJsonState(prefix + L"_" + std::to_wstring(i), j[i]);
        }
        return;
    }
    if (j.is_string()) SetStateVar(prefix, TextValue(Utf8ToWide(j.get<std::string>())));
    else if (j.is_number()) SetStateVar(prefix, NumberValue(j.get<double>()));
    else if (j.is_boolean()) SetStateVar(prefix, BoolValue(j.get<bool>()));
    else SetStateVar(prefix, TextValue(L"void"));
}

void SetJsonVar(const std::wstring& name, const nlohmann::json& j, const char* key)
{
    if (!j.contains(key)) {
        SetStateVar(name, TextValue(L"void"));
        return;
    }
    const auto& v = j[key];
    if (v.is_string()) SetStateVar(name, TextValue(Utf8ToWide(v.get<std::string>())));
    else if (v.is_number()) SetStateVar(name, NumberValue(v.get<double>()));
    else if (v.is_boolean()) SetStateVar(name, BoolValue(v.get<bool>()));
    else SetStateVar(name, TextValue(L"void"));
}

void SetPrevAlias(const std::wstring& name, const std::wstring& source)
{
    SetStateVar(name, GetVarFromMap(s_prevVars, source));
}

namespace {
    bool StartsWithDetail(const std::wstring& text, const std::wstring& prefix)
    {
        return text.rfind(prefix, 0) == 0;
    }

    void SetTakenConsoleLogVars(const std::wstring& text, const std::wstring& payload, bool matched)
    {
        s_vars[L"console_log_taken_text"] = TextValue(matched ? text : L"void");
        s_vars[L"console_log_taken_payload"] = TextValue(matched ? payload : L"void");
        s_vars[L"console_log_taken_matched"] = BoolValue(matched);
        s_vars[L"console_log_queue_size"] = NumberValue((double)s_consoleLogQueue.size());
    }
}

bool ConsumeConsoleLogExact(const std::wstring& expected)
{
    const std::wstring target = Trim(expected);
    for (auto it = s_consoleLogQueue.begin(); it != s_consoleLogQueue.end(); ++it) {
        if (*it != target) continue;
        const std::wstring text = *it;
        s_consoleLogQueue.erase(it);
        SetTakenConsoleLogVars(text, L"", true);
        std::wcout << L"[脚本] 已消费控制台日志精确匹配: " << text << std::endl;
        return true;
    }
    SetTakenConsoleLogVars(L"", L"", false);
    return false;
}

bool ConsumeConsoleLogContains(const std::wstring& needle)
{
    const std::wstring target = Trim(needle);
    for (auto it = s_consoleLogQueue.begin(); it != s_consoleLogQueue.end(); ++it) {
        if (it->find(target) == std::wstring::npos) continue;
        const std::wstring text = *it;
        s_consoleLogQueue.erase(it);
        SetTakenConsoleLogVars(text, L"", true);
        std::wcout << L"[脚本] 已消费控制台日志包含匹配: " << text << std::endl;
        return true;
    }
    SetTakenConsoleLogVars(L"", L"", false);
    return false;
}

bool ConsumeConsoleLogPrefix(const std::wstring& prefix)
{
    const std::wstring target = prefix;
    for (auto it = s_consoleLogQueue.begin(); it != s_consoleLogQueue.end(); ++it) {
        if (!StartsWithDetail(*it, target)) continue;
        const std::wstring text = *it;
        const std::wstring payload = Trim(text.substr(target.size()));
        s_consoleLogQueue.erase(it);
        SetTakenConsoleLogVars(text, payload, true);
        std::wcout << L"[脚本] 已消费控制台日志前缀匹配: " << text
                   << L"，payload=" << payload << std::endl;
        return true;
    }
    SetTakenConsoleLogVars(L"", L"", false);
    return false;
}

void RegisterBuildWarning()
{
    if (s_runtimeCapability == buildcode::userdebug && GetBuildCode() == buildcode::user && s_owner) {
        HWND owner = s_owner;
        std::thread([owner]() {
            MessageBoxW(owner, L"检测到 OEM 调试解锁，程序将启用 userdebug 能力。\n\n请确认你理解脚本可以执行 shell 和文件写入等高权限操作。", L"StrikeSense 调试功能警告", MB_OK | MB_ICONWARNING);
        }).detach();
    }
}

} // namespace vscript::detail

namespace vscript {

using namespace detail;

void Initialize(HINSTANCE instance, HWND owner)
{
    s_instance = instance;
    s_owner = owner;
    fs::create_directories(GetDefaultScriptDir());
    EnsureExampleScript();
    s_oemValid = IsOemUnlockValid();
    s_runtimeCapability = GetBuildCode();
    if (s_runtimeCapability == buildcode::user && s_oemValid) s_runtimeCapability = buildcode::userdebug;
    RegisterBuildWarning();
    LoadMountedScripts();
    std::cout << "[脚本] vscript 初始化完成，构建等级=" << (int)GetBuildCode()
              << " 运行能力=" << (int)s_runtimeCapability << std::endl;
}

void Shutdown()
{
    for (auto& [id, slot] : s_sounds) {
        if (slot.channel >= 0) Mix_HaltChannel(slot.channel);
        if (slot.chunk) Mix_FreeChunk(slot.chunk);
    }
    s_sounds.clear();
    std::vector<int> ids;
    for (const auto& [id, _] : s_images) ids.push_back(id);
    for (int id : ids) CloseImage(id);
}

buildcode GetBuildCode()
{
    if constexpr (STRIKESENSE_BUILD_CODE <= 0) return buildcode::user;
    else if constexpr (STRIKESENSE_BUILD_CODE == 1) return buildcode::userdebug;
    else return buildcode::eng;
}

buildcode GetRuntimeCapability() { return s_runtimeCapability; }
bool HasUserDebugCapability() { return (int)s_runtimeCapability >= (int)buildcode::userdebug; }

bool EnsureOemUnlockFile()
{
    fs::path path = BaseDir() / L".oemunlock";
    const bool exists = fs::exists(path);
    std::wcout << L"[OEM] 检测解锁文件是否存在: " << path.wstring()
               << L" 结果=" << (exists ? L"存在" : L"不存在") << std::endl;
    return exists;
    if (fs::exists(path)) return true;
    WriteUtf8(path, MakeOemKey(NowStamp(), L"24h"));
    std::wcout << L"[OEM] 系统已经自动为你生成了一个可以让设备持续运行（或保持调试模式开启）24 小时的解锁文件: " << path.wstring() << std::endl;
    return true;
}

bool IsOemUnlockValid()
{
    fs::path path = BaseDir() / L".oemunlock";
    if (!fs::exists(path)) return false;
    std::wstring key = Trim(ReadAllWide(path));
    return ParseOemKey(key);
}

namespace {
    bool StartsWithText(const std::wstring& text, const std::wstring& prefix)
    {
        return text.rfind(prefix, 0) == 0;
    }

    void SetPersistentScriptVar(const std::wstring& name, const detail::value& v)
    {
        detail::s_vars[name] = v;
    }

    void SetModuleScriptVars(bool persistent)
    {
        const auto setVar = [&](const std::wstring& name, const detail::value& v) {
            if (persistent) SetPersistentScriptVar(name, v);
            else detail::SetStateVar(name, v);
        };

        for (const auto& id : modulenotifications::NativeModuleIds()) {
            const std::wstring prefix = L"module_" + detail::SanitizeName(id);
            setVar(prefix, detail::BoolValue(modulenotifications::GetModuleEnabled(id)));

            const std::wstring value = modulenotifications::GetModuleValue(id);
            if (!value.empty()) setVar(prefix + L"_value", detail::TextValue(value));
        }

        setVar(L"module_custom_musickit_format", detail::TextValue(modulenotifications::GetModuleValue(L"custom_musickit", L"format")));
        setVar(L"module_mwheel_jump_mode", detail::TextValue(modulenotifications::GetModuleValue(L"mwheel_jump", L"mode")));
        setVar(L"module_socd_mode", detail::TextValue(modulenotifications::GetModuleValue(L"socd", L"mode")));
        setVar(L"module_mixed_sensitivity_normal", detail::TextValue(modulenotifications::GetModuleValue(L"mixed_sensitivity", L"normal")));
        setVar(L"module_mixed_sensitivity_attack", detail::TextValue(modulenotifications::GetModuleValue(L"mixed_sensitivity", L"attack")));
        setVar(L"module_sniper_crosshair_style", detail::TextValue(modulenotifications::GetModuleValue(L"sniper_crosshair", L"style")));
        setVar(L"module_item_helper_map", detail::TextValue(modulenotifications::GetModuleValue(L"item_helper", L"map")));
    }

    std::wstring AfterPrefixTrimmed(const std::wstring& text, size_t prefixSize)
    {
        if (text.size() <= prefixSize) return L"";
        return detail::Trim(text.substr(prefixSize));
    }
}

void UpdateFromConsoleLog(const std::wstring& raw, const std::wstring& text)
{
    std::lock_guard<std::recursive_mutex> lock(s_mutex);
    static int consoleLogCount = 0;
    ++consoleLogCount;

    const std::wstring clean = Trim(text);
    const bool isScriptSignal = clean == L"/log" || StartsWithText(clean, L"/log ");
    const std::wstring payload = isScriptSignal ? AfterPrefixTrimmed(clean, 4) : L"";
    if (!clean.empty()) {
        s_consoleLogQueue.push_back(clean);
        while (s_consoleLogQueue.size() > 256) s_consoleLogQueue.pop_front();
    }

    bool isChat = false;
    std::wstring channel = L"void";
    std::wstring player = L"void";
    std::wstring location = L"void";
    std::wstring message = L"void";

    if (StartsWithText(clean, L"[")) {
        const size_t channelEnd = clean.find(L']');
        if (channelEnd != std::wstring::npos) {
            const std::wstring rest = Trim(clean.substr(channelEnd + 1));
            size_t colon = rest.find(L'：');
            if (colon == std::wstring::npos) colon = rest.find(L':');
            if (colon != std::wstring::npos) {
                isChat = true;
                channel = clean.substr(1, channelEnd - 1);
                std::wstring playerPart = Trim(rest.substr(0, colon));
                message = Trim(rest.substr(colon + 1));

                size_t locationSep = playerPart.find(L'﹫');
                if (locationSep == std::wstring::npos) locationSep = playerPart.find(L'@');
                if (locationSep != std::wstring::npos) {
                    player = Trim(playerPart.substr(0, locationSep));
                    location = Trim(playerPart.substr(locationSep + 1));
                } else {
                    player = playerPart;
                }
            }
        }
    }

    SetPersistentScriptVar(L"console_log_raw", TextValue(raw));
    SetPersistentScriptVar(L"console_log_text", TextValue(clean));
    SetPersistentScriptVar(L"console_log_count", NumberValue((double)consoleLogCount));
    SetPersistentScriptVar(L"console_log_has_line", BoolValue(true));
    SetPersistentScriptVar(L"console_log_is_script_signal", BoolValue(isScriptSignal));
    SetPersistentScriptVar(L"console_log_command", TextValue(isScriptSignal ? L"/log" : L"void"));
    SetPersistentScriptVar(L"console_log_payload", TextValue(isScriptSignal ? payload : L"void"));
    SetPersistentScriptVar(L"console_log_is_chat", BoolValue(isChat));
    SetPersistentScriptVar(L"console_log_channel", TextValue(channel));
    SetPersistentScriptVar(L"console_log_player", TextValue(player));
    SetPersistentScriptVar(L"console_log_location", TextValue(location));
    SetPersistentScriptVar(L"console_log_message", TextValue(message));
    SetPersistentScriptVar(L"console_log_queue_size", NumberValue((double)s_consoleLogQueue.size()));
    SetModuleScriptVars(true);
    modulenotifications::UpdateCrosshairRecoilSignal(clean);

    std::wcout << L"[脚本] 已更新控制台日志变量，第 " << consoleLogCount
               << L" 行，内容=" << clean << std::endl;
}

void UpdateFromGsi(const nlohmann::json& state)
{
    std::lock_guard<std::recursive_mutex> lock(s_mutex);
    s_gsiSnapshot = state;
    s_prevVars = s_vars;
    for (const auto& key : s_stateKeys) s_vars.erase(key);
    s_stateKeys.clear();
    FlattenJsonState(L"gsi", state);
    SetStateVar(L"gsi_field_count", NumberValue((double)gsi::state::flat.size()));
    SetModuleScriptVars(false);
    SetPrevAlias(L"prev_round_phase", L"round_phase");
    SetPrevAlias(L"prev_kills", L"kills");
    SetPrevAlias(L"prev_health", L"health");
    SetPrevAlias(L"prev_weapon_name", L"weapon_name");
    SetPrevAlias(L"prev_weapon_type", L"weapon_type");
    SetPrevAlias(L"prev_weapon_state", L"weapon_state");
    SetPrevAlias(L"prev_weapon_ammo_clip", L"weapon_ammo_clip");
    SetPrevAlias(L"prev_weapon_ammo_reserve", L"weapon_ammo_reserve");

    if (state.contains("provider") && state["provider"].is_object()) {
        SetJsonVar(L"provider_name", state["provider"], "name");
        SetJsonVar(L"provider_appid", state["provider"], "appid");
        SetJsonVar(L"provider_version", state["provider"], "version");
        SetJsonVar(L"provider_steamid", state["provider"], "steamid");
        SetJsonVar(L"provider_timestamp", state["provider"], "timestamp");
    } else {
        SetStateVar(L"provider_name", TextValue(L"void"));
        SetStateVar(L"provider_appid", TextValue(L"void"));
        SetStateVar(L"provider_version", TextValue(L"void"));
        SetStateVar(L"provider_steamid", TextValue(L"void"));
        SetStateVar(L"provider_timestamp", TextValue(L"void"));
    }
    if (state.contains("map") && state["map"].is_object()) {
        SetJsonVar(L"map", state["map"], "name");
        SetJsonVar(L"map_mode", state["map"], "mode");
        SetJsonVar(L"map_phase", state["map"], "phase");
        SetJsonVar(L"map_round", state["map"], "round");
        if (state["map"].contains("team_ct") && state["map"]["team_ct"].is_object()) {
            SetJsonVar(L"team_ct_score", state["map"]["team_ct"], "score");
            SetJsonVar(L"team_ct_consecutive_round_losses", state["map"]["team_ct"], "consecutive_round_losses");
            SetJsonVar(L"team_ct_timeouts_remaining", state["map"]["team_ct"], "timeouts_remaining");
            SetJsonVar(L"team_ct_matches_won_this_series", state["map"]["team_ct"], "matches_won_this_series");
        }
        if (state["map"].contains("team_t") && state["map"]["team_t"].is_object()) {
            SetJsonVar(L"team_t_score", state["map"]["team_t"], "score");
            SetJsonVar(L"team_t_consecutive_round_losses", state["map"]["team_t"], "consecutive_round_losses");
            SetJsonVar(L"team_t_timeouts_remaining", state["map"]["team_t"], "timeouts_remaining");
            SetJsonVar(L"team_t_matches_won_this_series", state["map"]["team_t"], "matches_won_this_series");
        }
        SetJsonVar(L"map_num_matches_to_win_series", state["map"], "num_matches_to_win_series");
    } else {
        SetStateVar(L"map", TextValue(L"void"));
        SetStateVar(L"map_mode", TextValue(L"void"));
        SetStateVar(L"map_phase", TextValue(L"void"));
        SetStateVar(L"map_round", TextValue(L"void"));
        SetStateVar(L"team_ct_score", TextValue(L"void"));
        SetStateVar(L"team_ct_consecutive_round_losses", TextValue(L"void"));
        SetStateVar(L"team_ct_timeouts_remaining", TextValue(L"void"));
        SetStateVar(L"team_ct_matches_won_this_series", TextValue(L"void"));
        SetStateVar(L"team_t_score", TextValue(L"void"));
        SetStateVar(L"team_t_consecutive_round_losses", TextValue(L"void"));
        SetStateVar(L"team_t_timeouts_remaining", TextValue(L"void"));
        SetStateVar(L"team_t_matches_won_this_series", TextValue(L"void"));
        SetStateVar(L"map_num_matches_to_win_series", TextValue(L"void"));
    }
    if (state.contains("round") && state["round"].is_object()) {
        SetJsonVar(L"round_phase", state["round"], "phase");
        SetJsonVar(L"bomb", state["round"], "bomb");
        SetJsonVar(L"round_win_team", state["round"], "win_team");
    } else {
        SetStateVar(L"round_phase", TextValue(L"void"));
        SetStateVar(L"bomb", TextValue(L"void"));
        SetStateVar(L"round_win_team", TextValue(L"void"));
    }
    if (state.contains("player") && state["player"].is_object()) {
        const auto& player = state["player"];
        SetJsonVar(L"player_name", player, "name");
        SetJsonVar(L"activity", player, "activity");
        SetJsonVar(L"steamid", player, "steamid");
        SetJsonVar(L"team", player, "team");
        SetJsonVar(L"observer_slot", player, "observer_slot");
        if (player.contains("state") && player["state"].is_object()) {
            SetJsonVar(L"kills", player["state"], "round_kills");
            SetJsonVar(L"health", player["state"], "health");
            SetJsonVar(L"flashed", player["state"], "flashed");
            SetJsonVar(L"armor", player["state"], "armor");
            SetJsonVar(L"helmet", player["state"], "helmet");
            SetJsonVar(L"smoked", player["state"], "smoked");
            SetJsonVar(L"burning", player["state"], "burning");
            SetJsonVar(L"money", player["state"], "money");
            SetJsonVar(L"round_killhs", player["state"], "round_killhs");
            SetJsonVar(L"equip_value", player["state"], "equip_value");
            SetStateVar(L"death_mute", BoolValue(ToNumber(GetVar(L"health")) <= 0));
        } else {
            SetStateVar(L"kills", TextValue(L"void"));
            SetStateVar(L"health", TextValue(L"void"));
            SetStateVar(L"flashed", TextValue(L"void"));
            SetStateVar(L"armor", TextValue(L"void"));
            SetStateVar(L"helmet", TextValue(L"void"));
            SetStateVar(L"smoked", TextValue(L"void"));
            SetStateVar(L"burning", TextValue(L"void"));
            SetStateVar(L"money", TextValue(L"void"));
            SetStateVar(L"round_killhs", TextValue(L"void"));
            SetStateVar(L"equip_value", TextValue(L"void"));
            SetStateVar(L"death_mute", TextValue(L"void"));
            SetStateVar(L"self_alive", TextValue(L"void"));
            SetStateVar(L"self_dead_this_round", TextValue(L"void"));
        }
        if (player.contains("match_stats") && player["match_stats"].is_object()) {
            SetJsonVar(L"mvps", player["match_stats"], "mvps");
            SetJsonVar(L"match_kills", player["match_stats"], "kills");
            SetJsonVar(L"match_assists", player["match_stats"], "assists");
            SetJsonVar(L"match_deaths", player["match_stats"], "deaths");
            SetJsonVar(L"match_score", player["match_stats"], "score");
        } else {
            SetStateVar(L"mvps", TextValue(L"void"));
            SetStateVar(L"match_kills", TextValue(L"void"));
            SetStateVar(L"match_assists", TextValue(L"void"));
            SetStateVar(L"match_deaths", TextValue(L"void"));
            SetStateVar(L"match_score", TextValue(L"void"));
        }

        SetStateVar(L"weapon_name", TextValue(L"void"));
        SetStateVar(L"weapon_type", TextValue(L"void"));
        SetStateVar(L"weapon_state", TextValue(L"void"));
        SetStateVar(L"weapon_ammo_clip", TextValue(L"void"));
        SetStateVar(L"weapon_ammo_clip_max", TextValue(L"void"));
        SetStateVar(L"weapon_ammo_reserve", TextValue(L"void"));
        SetStateVar(L"weapon_fired", BoolValue(false));
        SetStateVar(L"weapon_reloading", BoolValue(false));
        SetStateVar(L"weapon_switched", BoolValue(false));
        SetStateVar(L"weapon_clip_delta", NumberValue(0.0));
        SetStateVar(L"weapon_reserve_delta", NumberValue(0.0));
        SetStateVar(L"weapon_fire_count", NumberValue((double)s_weaponFireCount));
        SetStateVar(L"weapon_reload_count", NumberValue((double)s_weaponReloadCount));
        SetStateVar(L"weapon_reserve_drop_count", NumberValue((double)s_weaponReserveDropCount));
        std::wstring activeWeaponSlot;
        if (player.contains("weapons") && player["weapons"].is_object()) {
            for (auto it = player["weapons"].begin(); it != player["weapons"].end(); ++it) {
                const auto& weapon = it.value();
                if (!weapon.is_object()) continue;
                std::string stateText = weapon.value("state", "");
                if (stateText != "active") continue;
                activeWeaponSlot = Utf8ToWide(it.key());
                SetJsonVar(L"weapon_name", weapon, "name");
                SetJsonVar(L"weapon_type", weapon, "type");
                SetJsonVar(L"weapon_state", weapon, "state");
                SetJsonVar(L"weapon_ammo_clip", weapon, "ammo_clip");
                SetJsonVar(L"weapon_ammo_clip_max", weapon, "ammo_clip_max");
                SetJsonVar(L"weapon_ammo_reserve", weapon, "ammo_reserve");
                break;
            }
        }
        const std::wstring nowWeaponName = ToText(GetVar(L"weapon_name"));
        const std::wstring nowWeaponState = ToText(GetVar(L"weapon_state"));
        const double nowClip = ToNumber(GetVar(L"weapon_ammo_clip"));
        const double nowReserve = ToNumber(GetVar(L"weapon_ammo_reserve"));
        const bool roundJustWentLive = ToText(GetVar(L"round_phase")) == L"live" && ToText(GetVarFromMap(s_prevVars, L"round_phase")) != L"live";
        if (roundJustWentLive) {
            s_weaponFireCount = 0;
            s_weaponReloadCount = 0;
            s_weaponReserveDropCount = 0;
            s_lastWeaponSnapshot = weapon_snapshot{};
        }
        const bool currentWeaponValid = !nowWeaponName.empty() && nowWeaponName != L"void" && nowClip >= 0.0;
        const bool sameWeapon = currentWeaponValid && s_lastWeaponSnapshot.valid && s_lastWeaponSnapshot.name == nowWeaponName;
        const bool switched = currentWeaponValid && (!s_lastWeaponSnapshot.valid || s_lastWeaponSnapshot.name != nowWeaponName);
        double prevClip = sameWeapon ? s_lastWeaponSnapshot.clip : -1.0;
        double prevReserve = sameWeapon ? s_lastWeaponSnapshot.reserve : -1.0;
        if (!activeWeaponSlot.empty() && state.contains("previously") && state["previously"].is_object()) {
            const auto& previously = state["previously"];
            if (previously.contains("player") && previously["player"].is_object()) {
                const auto& prevPlayer = previously["player"];
                if (prevPlayer.contains("weapons") && prevPlayer["weapons"].is_object()) {
                    const auto& prevWeapons = prevPlayer["weapons"];
                    const std::string slotUtf8 = WideToUtf8(activeWeaponSlot);
                    if (prevWeapons.contains(slotUtf8) && prevWeapons[slotUtf8].is_object()) {
                        const auto& prevWeapon = prevWeapons[slotUtf8];
                        if (prevWeapon.contains("ammo_clip") && prevWeapon["ammo_clip"].is_number()) {
                            prevClip = prevWeapon["ammo_clip"].get<double>();
                        }
                        if (prevWeapon.contains("ammo_reserve") && prevWeapon["ammo_reserve"].is_number()) {
                            prevReserve = prevWeapon["ammo_reserve"].get<double>();
                        }
                    }
                }
            }
        }
        const bool fired = sameWeapon && prevClip > nowClip && nowWeaponState != L"reloading";
        const bool reserveDropped = sameWeapon && prevReserve > nowReserve && nowReserve >= 0.0;
        const bool reloading = sameWeapon && nowClip > prevClip;
        if (fired) ++s_weaponFireCount;
        if (reloading) ++s_weaponReloadCount;
        if (reserveDropped) ++s_weaponReserveDropCount;
        SetStateVar(L"weapon_switched", BoolValue(switched));
        SetStateVar(L"weapon_fired", BoolValue(fired));
        SetStateVar(L"weapon_reloading", BoolValue(reloading));
        SetStateVar(L"weapon_clip_delta", NumberValue(prevClip - nowClip));
        SetStateVar(L"weapon_reserve_delta", NumberValue(prevReserve - nowReserve));
        SetStateVar(L"weapon_fire_count", NumberValue((double)s_weaponFireCount));
        SetStateVar(L"weapon_reload_count", NumberValue((double)s_weaponReloadCount));
        SetStateVar(L"weapon_reserve_drop_count", NumberValue((double)s_weaponReserveDropCount));

        if (currentWeaponValid) {
            s_lastWeaponSnapshot.name = nowWeaponName;
            s_lastWeaponSnapshot.state = nowWeaponState;
            s_lastWeaponSnapshot.clip = nowClip;
            s_lastWeaponSnapshot.reserve = nowReserve;
            s_lastWeaponSnapshot.valid = true;
        } else {
            s_lastWeaponSnapshot = weapon_snapshot{};
        }

        SetStateVar(L"internal_last_phase", TextValue(Utf8ToWide(gsi::runtime::last_phase)));
        SetStateVar(L"internal_last_kills", NumberValue((double)gsi::runtime::last_kills));
        SetStateVar(L"internal_last_mvps", NumberValue((double)gsi::runtime::last_mvps));
        SetStateVar(L"internal_dead_muted", BoolValue(gsi::runtime::dead_muted));
        SetStateVar(L"internal_waiting_for_live", BoolValue(gsi::runtime::waiting_for_live));
        SetStateVar(L"internal_round_started", BoolValue(gsi::runtime::round_started));
        SetStateVar(L"internal_mvp_candidate_kills", NumberValue((double)gsi::runtime::mvp_candidate_kills));
        SetStateVar(L"internal_mvp_pushed_this_round", BoolValue(gsi::runtime::mvp_pushed_this_round));
        SetStateVar(L"internal_mvps_at_round_start", NumberValue((double)gsi::runtime::mvps_at_round_start));
        SetStateVar(L"internal_gameover_pushed", BoolValue(gsi::runtime::gameover_pushed));
        SetStateVar(L"internal_bomb_planted_this_round", BoolValue(gsi::runtime::bomb_planted_this_round));
        SetStateVar(L"internal_player_team", TextValue(Utf8ToWide(gsi::runtime::player_team)));
        SetStateVar(L"internal_map_mode", TextValue(Utf8ToWide(gsi::runtime::map_mode)));
        SetStateVar(L"internal_activity", TextValue(Utf8ToWide(gsi::runtime::activity)));
        SetStateVar(L"internal_round_kills", NumberValue((double)gsi::runtime::round_kills));
        SetStateVar(L"internal_health", NumberValue((double)gsi::runtime::health));
        SetStateVar(L"internal_in_lobby", BoolValue(gsi::runtime::in_lobby));
        const bool selfAlive = gsi::runtime::health > 0;
        const bool selfDeadThisRound = gsi::runtime::dead_muted;
        SetStateVar(L"self_alive", BoolValue(selfAlive));
        SetStateVar(L"self_dead_this_round", BoolValue(selfDeadThisRound));
        SetStateVar(L"death_mute", BoolValue(selfDeadThisRound && !selfAlive));
    } else {
        SetStateVar(L"player_name", TextValue(L"void"));
        SetStateVar(L"activity", TextValue(L"void"));
        SetStateVar(L"steamid", TextValue(L"void"));
        SetStateVar(L"team", TextValue(L"void"));
        SetStateVar(L"observer_slot", TextValue(L"void"));
        SetStateVar(L"kills", TextValue(L"void"));
        SetStateVar(L"health", TextValue(L"void"));
        SetStateVar(L"flashed", TextValue(L"void"));
        SetStateVar(L"armor", TextValue(L"void"));
        SetStateVar(L"helmet", TextValue(L"void"));
        SetStateVar(L"smoked", TextValue(L"void"));
        SetStateVar(L"burning", TextValue(L"void"));
        SetStateVar(L"money", TextValue(L"void"));
        SetStateVar(L"round_killhs", TextValue(L"void"));
        SetStateVar(L"equip_value", TextValue(L"void"));
        SetStateVar(L"mvps", TextValue(L"void"));
        SetStateVar(L"match_kills", TextValue(L"void"));
        SetStateVar(L"match_assists", TextValue(L"void"));
        SetStateVar(L"match_deaths", TextValue(L"void"));
        SetStateVar(L"match_score", TextValue(L"void"));
        SetStateVar(L"death_mute", TextValue(L"void"));
        SetStateVar(L"self_alive", TextValue(L"void"));
        SetStateVar(L"self_dead_this_round", TextValue(L"void"));
        SetStateVar(L"weapon_name", TextValue(L"void"));
        SetStateVar(L"weapon_type", TextValue(L"void"));
        SetStateVar(L"weapon_state", TextValue(L"void"));
        SetStateVar(L"weapon_ammo_clip", TextValue(L"void"));
        SetStateVar(L"weapon_ammo_clip_max", TextValue(L"void"));
        SetStateVar(L"weapon_ammo_reserve", TextValue(L"void"));
        SetStateVar(L"weapon_fired", BoolValue(false));
        SetStateVar(L"weapon_reloading", BoolValue(false));
        SetStateVar(L"weapon_switched", BoolValue(false));
        SetStateVar(L"weapon_clip_delta", NumberValue(0.0));
        SetStateVar(L"weapon_reserve_delta", NumberValue(0.0));
        SetStateVar(L"weapon_fire_count", NumberValue((double)s_weaponFireCount));
        SetStateVar(L"weapon_reload_count", NumberValue((double)s_weaponReloadCount));
        SetStateVar(L"weapon_reserve_drop_count", NumberValue((double)s_weaponReserveDropCount));
        SetStateVar(L"internal_last_phase", TextValue(Utf8ToWide(gsi::runtime::last_phase)));
        SetStateVar(L"internal_last_kills", NumberValue((double)gsi::runtime::last_kills));
        SetStateVar(L"internal_last_mvps", NumberValue((double)gsi::runtime::last_mvps));
        SetStateVar(L"internal_dead_muted", BoolValue(gsi::runtime::dead_muted));
        SetStateVar(L"internal_waiting_for_live", BoolValue(gsi::runtime::waiting_for_live));
        SetStateVar(L"internal_round_started", BoolValue(gsi::runtime::round_started));
        SetStateVar(L"internal_mvp_candidate_kills", NumberValue((double)gsi::runtime::mvp_candidate_kills));
        SetStateVar(L"internal_mvp_pushed_this_round", BoolValue(gsi::runtime::mvp_pushed_this_round));
        SetStateVar(L"internal_mvps_at_round_start", NumberValue((double)gsi::runtime::mvps_at_round_start));
        SetStateVar(L"internal_gameover_pushed", BoolValue(gsi::runtime::gameover_pushed));
        SetStateVar(L"internal_bomb_planted_this_round", BoolValue(gsi::runtime::bomb_planted_this_round));
        SetStateVar(L"internal_player_team", TextValue(Utf8ToWide(gsi::runtime::player_team)));
        SetStateVar(L"internal_map_mode", TextValue(Utf8ToWide(gsi::runtime::map_mode)));
        SetStateVar(L"internal_activity", TextValue(Utf8ToWide(gsi::runtime::activity)));
        SetStateVar(L"internal_round_kills", NumberValue((double)gsi::runtime::round_kills));
        SetStateVar(L"internal_health", NumberValue((double)gsi::runtime::health));
        SetStateVar(L"internal_in_lobby", BoolValue(gsi::runtime::in_lobby));
    }
}

void TickContinuousScripts()
{
    std::lock_guard<std::recursive_mutex> lock(s_mutex);
    for (auto& script : s_mounted) {
        if (script.continuous) {
            if (!RefreshScriptState(script, false)) continue;
            std::wstring text = LoadScriptCached(script.path);
            if (!text.empty()) {
                s_currentPrivilegedAllowed = script.privilegedAllowed;
                s_currentScriptPath = script.path;
                execution_context exec;
                execution_scope_guard guard(exec);
                //std::wcout << L"[脚本] 开始连续执行脚本，上下文已隔离： " << script.path << std::endl;
                ExecuteBlock(StripComments(text));
                s_currentPrivilegedAllowed = false;
                s_currentScriptPath.clear();
            }
        }
    }
    ResetTransientWeaponVars();
    s_prevVars = s_vars;
}

bool ExecuteScriptFile(const std::wstring& path)
{
    std::lock_guard<std::recursive_mutex> lock(s_mutex);
    mounted_script temp;
    temp.path = path;
    if (!RefreshScriptState(temp, true)) return false;
    std::wstring script = ReadAllWide(path);
    if (script.empty()) {
        std::wcout << L"[脚本] 脚本为空或读取失败：" << path << std::endl;
        return false;
    }
    s_scriptCache[path] = script;
    std::wcout << L"[脚本] 执行脚本： " << path << std::endl;
    s_currentPrivilegedAllowed = temp.privilegedAllowed;
    s_currentScriptPath = path;
    execution_context exec;
    execution_scope_guard guard(exec);
    std::wcout << L"[脚本] 开始单次执行脚本，上下文已隔离：" << path << std::endl;
    ExecuteBlock(StripComments(script));
    s_currentPrivilegedAllowed = false;
    s_currentScriptPath.clear();
    return true;
}

std::vector<mounted_script>& MountedScripts() { return s_mounted; }

std::wstring GetScriptConfigPath()
{
    return (BaseDir() / L"setting" / L"vscript_config.json").wstring();
}

std::wstring GetDefaultScriptDir()
{
    return (BaseDir() / L"script").wstring();
}

std::wstring GetExampleScriptPath()
{
    return (fs::path(GetDefaultScriptDir()) / L"syntax_showcase.vscript").wstring();
}

void LoadMountedScripts()
{
    s_mounted.clear();
    fs::path path = GetScriptConfigPath();
    if (!fs::exists(path)) {
        SaveMountedScripts();
        return;
    }
    try {
        std::ifstream in(path);
        nlohmann::json j;
        in >> j;
        if (j.contains("mounted") && j["mounted"].is_array()) {
            for (const auto& item : j["mounted"]) {
                mounted_script s;
                if (item.contains("path") && item["path"].is_string()) s.path = Utf8ToWide(item["path"].get<std::string>());
                if (item.contains("continuous") && item["continuous"].is_boolean()) s.continuous = item["continuous"].get<bool>();
                if (!s.path.empty()) {
                    RefreshScriptState(s, false);
                    if (s.continuous && !ConfirmContinuousAllowed(s.path)) s.continuous = false;
                    if (!s.usesPrivilegedApis || s.privilegedAllowed || GetRuntimeCapability() >= buildcode::userdebug) {
                        s_mounted.push_back(s);
                    }
                }
            }
        }
        std::cout << "[脚本配置] 已加载挂载脚本数量：" << s_mounted.size() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[脚本配置] 加载失败： " << e.what() << std::endl;
    }
}

void SaveMountedScripts()
{
    fs::path path = GetScriptConfigPath();
    fs::create_directories(path.parent_path());
    nlohmann::json j;
    j["mounted"] = nlohmann::json::array();
    for (const auto& script : s_mounted) {
        j["mounted"].push_back({ {"path", WideToUtf8(script.path)}, {"continuous", script.continuous} });
    }
    std::ofstream out(path);
    out << j.dump(2);
    std::cout << "[脚本配置] 已保存挂载脚本数量：" << s_mounted.size() << std::endl;
}

void AddMountedScript(const std::wstring& path)
{
    auto it = std::find_if(s_mounted.begin(), s_mounted.end(), [&](const mounted_script& s) { return s.path == path; });
    if (it == s_mounted.end()) {
        mounted_script script;
        script.path = path;
        if (!RefreshScriptState(script, true)) return;
        s_mounted.push_back(script);
    } else {
        if (!RefreshScriptState(*it, true)) return;
    }
    SaveMountedScripts();
}

void RemoveMountedScript(size_t index)
{
    if (index >= s_mounted.size()) return;
    s_scriptCache.erase(s_mounted[index].path);
    s_mounted.erase(s_mounted.begin() + index);
    SaveMountedScripts();
}

void ToggleContinuous(size_t index)
{
    if (index >= s_mounted.size()) return;
    if (!RefreshScriptState(s_mounted[index], true)) {
        SaveMountedScripts();
        return;
    }
    if (!s_mounted[index].continuous && !ConfirmContinuousAllowed(s_mounted[index].path)) {
        std::wcout << L"[脚本配置] 已保存挂载脚本数量： " << s_mounted[index].path << std::endl;
        return;
    }
    s_mounted[index].continuous = !s_mounted[index].continuous;
    SaveMountedScripts();
}

void EnsureExampleScript()
{
    fs::path dir = GetDefaultScriptDir();
    fs::create_directories(dir);
    std::cout << "[脚本] 已确认脚本目录存在，不会额外生成示例资源目录" << std::endl;
}

const std::wstring& GetScriptDisplayName(const mounted_script& script)
{
    return script.hasMetadataName ? script.name : script.path;
}

std::wstring GetScriptNotice(const mounted_script& script)
{
    std::wstring out;
    if (!script.provider.empty()) out += L"Provider: " + script.provider + L"  ";
    if (!script.version.empty()) out += L"Version: " + script.version + L"  ";
    if (!script.notice.empty()) out += script.notice;
    return out;
}

} // namespace vscript

