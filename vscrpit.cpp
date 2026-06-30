#include "vscrpit.h"
#include "pages.h"
#include "volume_mixer.h"
#include "normalgen.h"

#include <TlHelp32.h>
#include <Shellapi.h>
#include <Shlwapi.h>
#include <Psapi.h>
#include <SDL_mixer.h>
#include <gdiplus.h>
#include <algorithm>
#include <chrono>
#include <codecvt>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Psapi.lib")

namespace fs = std::filesystem;

#ifndef STRIKESENSE_BUILD_CODE
#define STRIKESENSE_BUILD_CODE 0
#endif

namespace vscrpit {

namespace {

struct value {
    enum class kind { none, number, text, boolean };
    kind type = kind::none;
    double number = 0.0;
    std::wstring text;
    bool boolean = false;
};

struct image_window {
    HWND hwnd = nullptr;
    std::unique_ptr<Gdiplus::Image> image;
    int width = 0;
    int height = 0;
    float alpha = 1.0f;
};

struct sound_slot {
    Mix_Chunk* chunk = nullptr;
    int channel = -1;
};

HINSTANCE s_instance = nullptr;
HWND s_owner = nullptr;
buildcode s_runtimeCapability = buildcode::user;
bool s_oemValid = false;
std::vector<mounted_script> s_mounted;
std::map<std::wstring, value> s_vars;
std::map<std::wstring, value> s_prevVars;
std::unordered_map<int, image_window> s_images;
std::unordered_map<int, sound_slot> s_sounds;
std::recursive_mutex s_mutex;
std::unordered_map<std::wstring, std::wstring> s_scriptCache;
std::unordered_set<std::wstring> s_stateKeys;
bool s_returnRequested = false;
std::optional<std::wstring> s_gotoTarget;

constexpr const wchar_t* k_imageClass = L"StrikeSenseVscrpitImage";
constexpr const char* k_alphabet = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_";

std::wstring Trim(std::wstring s);
value TextValue(const std::wstring& s);
value NumberValue(double n);
value BoolValue(bool b);

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
    if (raw.size() >= 3 && (unsigned char)raw[0] == 0xEF && (unsigned char)raw[1] == 0xBB && (unsigned char)raw[2] == 0xBF)
        raw.erase(0, 3);
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

void LoadScriptMetadata(mounted_script& script)
{
    std::wstring raw = LoadScriptCached(script.path);
    std::wstringstream ss(raw);
    std::wstring line;
    while (std::getline(ss, line)) {
        std::wstring name = ReadMetaValue(line, L"@name");
        std::wstring provider = ReadMetaValue(line, L"@provider");
        std::wstring version = ReadMetaValue(line, L"@version");
        std::wstring notice = ReadMetaValue(line, L"@notice");
        if (!name.empty()) {
            script.name = name;
            script.hasMetadataName = true;
        }
        if (!provider.empty()) script.provider = provider;
        if (!version.empty()) script.version = version;
        if (!notice.empty()) script.notice = notice;
    }
}

bool ScriptHasEdgeGuard(const std::wstring& path)
{
    std::wstring script = LoadScriptCached(path);
    return script.find(L"on:") != std::wstring::npos;
}

bool ConfirmContinuousAllowed(const std::wstring& path)
{
    if (ScriptHasEdgeGuard(path)) return true;
    if (GetRuntimeCapability() == buildcode::eng) {
        std::wcout << L"[脚本权限] eng 构建允许无 on: 轮询脚本: " << path << std::endl;
        return true;
    }
    if (GetRuntimeCapability() == buildcode::userdebug) {
        int result = MessageBoxW(
            s_owner,
            L"这个脚本没有 on: 状态边沿判断，持续执行可能反复打开网页、重复创建文件或反复执行命令。\n\n是否仍然允许它轮询？",
            L"StrikeSense 脚本轮询确认",
            MB_YESNO | MB_ICONWARNING
        );
        return result == IDYES;
    }
    MessageBoxW(
        s_owner,
        L"user 模式禁止轮询没有 on: 状态判断的脚本。\n\n请给脚本加入类似 if(on:death_mute==true){ ... }; 的结构，或提升运行权限。",
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

value TextValue(const std::wstring& s)
{
    value v;
    v.type = value::kind::text;
    v.text = s;
    return v;
}

value NumberValue(double n)
{
    value v;
    v.type = value::kind::number;
    v.number = n;
    return v;
}

value BoolValue(bool b)
{
    value v;
    v.type = value::kind::boolean;
    v.boolean = b;
    return v;
}

bool Truthy(const value& v)
{
    if (v.type == value::kind::boolean) return v.boolean;
    if (v.type == value::kind::number) return v.number != 0.0;
    if (v.type == value::kind::text) return !v.text.empty() && v.text != L"void";
    return false;
}

std::wstring ToText(const value& v)
{
    if (v.type == value::kind::text) return v.text;
    if (v.type == value::kind::boolean) return v.boolean ? L"true" : L"false";
    if (v.type == value::kind::number) {
        std::wostringstream oss;
        oss << v.number;
        return oss.str();
    }
    return L"void";
}

std::wstring ExpandEnvText(const std::wstring& text)
{
    DWORD need = ExpandEnvironmentStringsW(text.c_str(), nullptr, 0);
    if (need == 0) return text;
    std::wstring out(need, L'\0');
    ExpandEnvironmentStringsW(text.c_str(), out.data(), need);
    if (!out.empty() && out.back() == L'\0') out.pop_back();
    return out;
}

double ToNumber(const value& v)
{
    if (v.type == value::kind::number) return v.number;
    if (v.type == value::kind::boolean) return v.boolean ? 1.0 : 0.0;
    if (v.type == value::kind::text) {
        try { return std::stod(v.text); } catch (...) { return 0.0; }
    }
    return 0.0;
}

value GetVar(const std::wstring& name)
{
    auto it = s_vars.find(name);
    if (it != s_vars.end()) return it->second;
    return TextValue(L"void");
}

std::wstring Trim(std::wstring s)
{
    auto isSpace = [](wchar_t c) { return iswspace(c) != 0; };
    s.erase(s.begin(), std::find_if_not(s.begin(), s.end(), isSpace));
    s.erase(std::find_if_not(s.rbegin(), s.rend(), isSpace).base(), s.end());
    return s;
}

std::vector<std::wstring> SplitStatements(const std::wstring& script)
{
    std::vector<std::wstring> out;
    std::wstring cur;
    bool inString = false;
    int brace = 0;
    int paren = 0;
    for (size_t i = 0; i < script.size(); ++i) {
        wchar_t c = script[i];
        if (c == L'"' && (i == 0 || script[i - 1] != L'\\')) inString = !inString;
        if (!inString) {
            if (c == L'(') ++paren;
            if (c == L')' && paren > 0) --paren;
            if (c == L'{') ++brace;
            if (c == L'}') --brace;
            if (c == L';' && brace == 0 && paren == 0) {
                out.push_back(Trim(cur));
                cur.clear();
                continue;
            }
        }
        cur.push_back(c);
    }
    if (!Trim(cur).empty()) out.push_back(Trim(cur));
    return out;
}

std::vector<std::wstring> SplitArgs(const std::wstring& args)
{
    std::vector<std::wstring> out;
    std::wstring cur;
    bool inString = false;
    int paren = 0;
    for (size_t i = 0; i < args.size(); ++i) {
        wchar_t c = args[i];
        if (c == L'"' && (i == 0 || args[i - 1] != L'\\')) inString = !inString;
        if (!inString) {
            if (c == L'(') ++paren;
            if (c == L')') --paren;
            if (c == L',' && paren == 0) {
                out.push_back(Trim(cur));
                cur.clear();
                continue;
            }
        }
        cur.push_back(c);
    }
    if (!Trim(cur).empty()) out.push_back(Trim(cur));
    return out;
}

value EvalExpr(const std::wstring& expr)
{
    std::wstring e = Trim(expr);
    auto findOp = [&](const std::wstring& ops) -> std::pair<size_t, wchar_t> {
        bool inString = false;
        int paren = 0;
        for (size_t i = e.size(); i > 0; --i) {
            size_t idx = i - 1;
            wchar_t c = e[idx];
            if (c == L'"' && (idx == 0 || e[idx - 1] != L'\\')) inString = !inString;
            if (inString) continue;
            if (c == L')') ++paren;
            else if (c == L'(' && paren > 0) --paren;
            if (paren == 0 && ops.find(c) != std::wstring::npos) {
                if (c == L'-' && (idx == 0 || std::wstring(L"+-*/(").find(e[idx - 1]) != std::wstring::npos)) continue;
                return { idx, c };
            }
        }
        return { std::wstring::npos, L'\0' };
    };
    if (e.size() >= 2 && e.front() == L'(' && e.back() == L')') {
        return EvalExpr(e.substr(1, e.size() - 2));
    }
    auto addOp = findOp(L"+-");
    if (addOp.first != std::wstring::npos) {
        value left = EvalExpr(e.substr(0, addOp.first));
        value right = EvalExpr(e.substr(addOp.first + 1));
        if (addOp.second == L'+' && (left.type == value::kind::text || right.type == value::kind::text)) {
            return TextValue(ToText(left) + ToText(right));
        }
        return NumberValue(addOp.second == L'+' ? ToNumber(left) + ToNumber(right) : ToNumber(left) - ToNumber(right));
    }
    auto mulOp = findOp(L"*/");
    if (mulOp.first != std::wstring::npos) {
        value left = EvalExpr(e.substr(0, mulOp.first));
        value right = EvalExpr(e.substr(mulOp.first + 1));
        double r = ToNumber(right);
        if (mulOp.second == L'/' && r == 0.0) return NumberValue(0.0);
        return NumberValue(mulOp.second == L'*' ? ToNumber(left) * r : ToNumber(left) / r);
    }
    if (e.size() >= 2 && e.front() == L'"' && e.back() == L'"') {
        std::wstring text;
        for (size_t i = 1; i + 1 < e.size(); ++i) {
            if (e[i] == L'\\' && i + 1 < e.size()) {
                ++i;
                if (e[i] == L'n') text.push_back(L'\n');
                else text.push_back(e[i]);
            } else {
                text.push_back(e[i]);
            }
        }
        return TextValue(text);
    }
    if (e == L"true") return BoolValue(true);
    if (e == L"false") return BoolValue(false);
    if (e == L"void") return TextValue(L"void");
    if (!e.empty() && (iswdigit(e[0]) || e[0] == L'-')) {
        try { return NumberValue(std::stod(e)); } catch (...) {}
    }
    return GetVar(e);
}

bool CompareValues(const value& l, const std::wstring& op, const value& r)
{
    if (op == L"==" || op == L"=") return ToText(l) == ToText(r);
    if (op == L"!=") return ToText(l) != ToText(r);
    double a = ToNumber(l), b = ToNumber(r);
    if (op == L">") return a > b;
    if (op == L"<") return a < b;
    if (op == L">=") return a >= b;
    if (op == L"<=") return a <= b;
    return false;
}

bool EvalCondition(std::wstring cond)
{
    cond = Trim(cond);
    bool edge = false;
    if (cond.rfind(L"on:", 0) == 0) {
        edge = true;
        cond = cond.substr(3);
    }
    static const std::vector<std::wstring> ops = { L">=", L"<=", L"==", L"!=", L">", L"<", L"=" };
    for (const auto& op : ops) {
        size_t pos = cond.find(op);
        if (pos == std::wstring::npos) continue;
        std::wstring leftName = Trim(cond.substr(0, pos));
        value left = GetVar(leftName);
        value right = EvalExpr(cond.substr(pos + op.size()));
        bool now = CompareValues(left, op, right);
        if (!edge) return now;
        auto prevIt = s_prevVars.find(leftName);
        bool before = false;
        if (prevIt != s_prevVars.end()) before = CompareValues(prevIt->second, op, right);
        return now && !before;
    }
    return Truthy(EvalExpr(cond));
}

std::optional<std::pair<std::wstring, std::wstring>> ParseFunction(const std::wstring& stmt)
{
    size_t p = stmt.find(L'(');
    size_t q = stmt.rfind(L')');
    if (p == std::wstring::npos || q == std::wstring::npos || q < p) return std::nullopt;
    return std::make_pair(Trim(stmt.substr(0, p)), stmt.substr(p + 1, q - p - 1));
}

BOOL CALLBACK EnumMinimizeCs2(HWND hwnd, LPARAM)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (!pid || !IsWindowVisible(hwnd)) return TRUE;
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return TRUE;
    wchar_t path[MAX_PATH]{};
    DWORD size = MAX_PATH;
    bool match = QueryFullProcessImageNameW(h, 0, path, &size) && _wcsicmp(PathFindFileNameW(path), L"cs2.exe") == 0;
    CloseHandle(h);
    if (match) {
        ShowWindow(hwnd, SW_MINIMIZE);
        return FALSE;
    }
    return TRUE;
}

std::wstring ProcessNameFromWindow(HWND hwnd)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (!pid || !IsWindowVisible(hwnd)) return L"";
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return L"";
    wchar_t path[MAX_PATH]{};
    DWORD size = MAX_PATH;
    std::wstring name;
    if (QueryFullProcessImageNameW(h, 0, path, &size)) name = PathFindFileNameW(path);
    CloseHandle(h);
    return name;
}

struct top_request {
    std::wstring process;
    bool topmost = true;
    bool firstOnly = false;
};

BOOL CALLBACK EnumTopProcess(HWND hwnd, LPARAM lp)
{
    auto* req = reinterpret_cast<top_request*>(lp);
    std::wstring name = ProcessNameFromWindow(hwnd);
    if (name.empty()) return TRUE;
    if (_wcsicmp(name.c_str(), req->process.c_str()) != 0) return TRUE;
    SetWindowPos(hwnd, req->topmost ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    if (req->topmost) {
        ShowWindow(hwnd, SW_SHOW);
        SetForegroundWindow(hwnd);
    }
    return req->firstOnly ? FALSE : TRUE;
}

bool TopProcess(const std::wstring& process, bool topmost)
{
    top_request req{ process, topmost, false };
    EnumWindows(EnumTopProcess, reinterpret_cast<LPARAM>(&req));
    return true;
}

bool LaunchUrlExternal(const std::wstring& url)
{
    std::wstring cmd = L"rundll32.exe url.dll,FileProtocolHandler \"" + url + L"\"";
    STARTUPINFOW si{};
    PROCESS_INFORMATION pi{};
    si.cb = sizeof(si);
    std::vector<wchar_t> buffer(cmd.begin(), cmd.end());
    buffer.push_back(L'\0');
    BOOL ok = CreateProcessW(nullptr, buffer.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    if (ok) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
    return ok == TRUE;
}

void TopBrowserSoon()
{
    std::thread([]() {
        Sleep(1200);
        for (const wchar_t* proc : { L"msedge.exe", L"chrome.exe", L"firefox.exe", L"browser.exe", L"iexplore.exe" }) {
            top_request req{ proc, true, true };
            EnumWindows(EnumTopProcess, reinterpret_cast<LPARAM>(&req));
        }
    }).detach();
}

BOOL CALLBACK EnumShowCs2(HWND hwnd, LPARAM)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (!pid || !IsWindowVisible(hwnd)) return TRUE;
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return TRUE;
    wchar_t path[MAX_PATH]{};
    DWORD size = MAX_PATH;
    bool match = QueryFullProcessImageNameW(h, 0, path, &size) && _wcsicmp(PathFindFileNameW(path), L"cs2.exe") == 0;
    CloseHandle(h);
    if (match) {
        ShowWindow(hwnd, SW_RESTORE);
        SetForegroundWindow(hwnd);
        return FALSE;
    }
    return TRUE;
}

bool KillProcessByName(const std::wstring& exe)
{
    bool killed = false;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, exe.c_str()) == 0) {
                HANDLE hp = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                if (hp) {
                    killed = TerminateProcess(hp, 0) || killed;
                    CloseHandle(hp);
                }
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return killed;
}

LRESULT CALLBACK ImageProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_PAINT) {
        PAINTSTRUCT ps{};
        HDC hdc = BeginPaint(hwnd, &ps);
        auto id = (int)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
        auto it = s_images.find(id);
        if (it != s_images.end() && it->second.image) {
            Gdiplus::Graphics g(hdc);
            g.DrawImage(it->second.image.get(), 0, 0, it->second.width, it->second.height);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    if (msg == WM_CLOSE) {
        DestroyWindow(hwnd);
        return 0;
    }
    if (msg == WM_DESTROY) {
        int id = (int)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
        s_images.erase(id);
        std::cout << "[脚本] 图片窗口已销毁 ID=" << id << std::endl;
        return 0;
    }
    if (msg == WM_ERASEBKGND) return TRUE;
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void EnsureImageClass()
{
    static bool registered = false;
    if (registered) return;
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = ImageProc;
    wc.hInstance = s_instance;
    wc.lpszClassName = k_imageClass;
    RegisterClassExW(&wc);
    registered = true;
}

void CloseImage(int id)
{
    auto it = s_images.find(id);
    if (it == s_images.end()) return;
    if (it->second.hwnd) PostMessageW(it->second.hwnd, WM_CLOSE, 0, 0);
    std::cout << "[脚本] 已关闭图片 ID=" << id << std::endl;
}

bool DrawImageCommand(const fs::path& path, int offsetX, int offsetY, bool alphaChannel, float opacity, int ttlMs, int id)
{
    CloseImage(id);
    EnsureImageClass();
    auto img = std::make_unique<Gdiplus::Image>(path.c_str());
    if (img->GetLastStatus() != Gdiplus::Ok) {
        std::wcout << L"[脚本] 图片加载失败: " << path.wstring() << std::endl;
        return false;
    }
    int w = (int)img->GetWidth();
    int h = (int)img->GetHeight();
    int x = GetSystemMetrics(SM_CXSCREEN) / 2 - w / 2 + offsetX;
    int y = GetSystemMetrics(SM_CYSCREEN) / 2 - h / 2 + offsetY;
    HWND hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TRANSPARENT,
        k_imageClass, L"", WS_POPUP, x, y, w, h, nullptr, nullptr, s_instance, nullptr);
    if (!hwnd) return false;
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, id);
    BYTE a = (BYTE)std::clamp((int)(opacity * 255.0f), 0, 255);
    SetLayeredWindowAttributes(hwnd, alphaChannel ? RGB(0, 0, 0) : 0, a, LWA_ALPHA);
    s_images[id] = image_window{ hwnd, std::move(img), w, h, opacity };
    ShowWindow(hwnd, SW_SHOW);
    InvalidateRect(hwnd, nullptr, FALSE);
    if (ttlMs > 0) {
        std::thread([id, ttlMs]() {
            Sleep((DWORD)ttlMs);
            auto it = s_images.find(id);
            if (it != s_images.end() && it->second.hwnd) {
                PostMessageW(it->second.hwnd, WM_CLOSE, 0, 0);
            }
        }).detach();
    }
    std::wcout << L"[脚本] 已绘制图片: " << path.wstring() << L" ID=" << id << std::endl;
    return true;
}

bool PlaySoundCommand(const fs::path& path, float volume, int id)
{
    auto old = s_sounds.find(id);
    if (old != s_sounds.end()) {
        if (old->second.channel >= 0) Mix_HaltChannel(old->second.channel);
        if (old->second.chunk) Mix_FreeChunk(old->second.chunk);
        s_sounds.erase(old);
    }
    Mix_Chunk* chunk = Mix_LoadWAV(path.string().c_str());
    if (!chunk) {
        std::wcout << L"[脚本] 音频加载失败: " << path.wstring() << std::endl;
        return false;
    }
    Mix_VolumeChunk(chunk, std::clamp((int)(volume * MIX_MAX_VOLUME), 0, MIX_MAX_VOLUME));
    int channel = Mix_PlayChannel(-1, chunk, 0);
    if (channel < 0) {
        Mix_FreeChunk(chunk);
        return false;
    }
    s_sounds[id] = sound_slot{ chunk, channel };
    std::wcout << L"[脚本] 播放音频: " << path.wstring() << L" ID=" << id << std::endl;
    return true;
}

void StopSoundCommand(int id)
{
    auto it = s_sounds.find(id);
    if (it == s_sounds.end()) return;
    if (it->second.channel >= 0) Mix_HaltChannel(it->second.channel);
    if (it->second.chunk) Mix_FreeChunk(it->second.chunk);
    s_sounds.erase(it);
    std::cout << "[脚本] 已停止音频 ID=" << id << std::endl;
}

bool RequiresUserDebug(const std::wstring& name)
{
    return name == L"ShellExecute" || name == L"CFile" || name == L"DFile" ||
        name == L"OwriteFile" || name == L"AwriteFile";
}

bool ExecuteFunction(const std::wstring& name, const std::vector<std::wstring>& rawArgs)
{
    if (RequiresUserDebug(name) && !HasUserDebugCapability()) {
        std::wcout << L"[脚本权限] 拒绝 userdebug 命令: " << name << std::endl;
        return false;
    }

    std::vector<value> args;
    for (const auto& a : rawArgs) args.push_back(EvalExpr(a));

    if (name == L"CloseGameWindow") {
        EnumWindows(EnumMinimizeCs2, 0);
        return true;
    }
    if (name == L"KillGameProcess") return KillProcessByName(L"cs2.exe");
    if (name == L"RunGameProcess") return LaunchUrlExternal(L"steam://run/730");
    if (name == L"ShowGameProcess") {
        EnumWindows(EnumShowCs2, 0);
        return true;
    }
    if (name == L"Browser" && args.size() >= 1) {
        bool ok = LaunchUrlExternal(ToText(args[0]));
        if (ok && args.size() >= 2 && Truthy(args[1])) TopBrowserSoon();
        return ok;
    }
    if (name == L"Top" && args.size() >= 1) return TopProcess(ToText(args[0]), args.size() < 2 || Truthy(args[1]));
    if (name == L"Drawimg" && args.size() >= 7) return DrawImageCommand(ExpandEnvText(ToText(args[0])), (int)ToNumber(args[1]), (int)ToNumber(args[2]), Truthy(args[3]), (float)ToNumber(args[4]), (int)ToNumber(args[5]), (int)ToNumber(args[6]));
    if (name == L"Closeimg" && args.size() >= 1) { CloseImage((int)ToNumber(args[0])); return true; }
    if (name == L"Playsnd" && args.size() >= 3) return PlaySoundCommand(ExpandEnvText(ToText(args[0])), (float)ToNumber(args[1]), (int)ToNumber(args[2]));
    if (name == L"Stopsnd" && args.size() >= 1) { StopSoundCommand((int)ToNumber(args[0])); return true; }
    if (name == L"ShellExecute" && args.size() >= 1) {
        std::wstring cmd = L"/C " + ToText(args[0]);
        return (INT_PTR)ShellExecuteW(nullptr, L"open", L"cmd.exe", cmd.c_str(), nullptr, SW_HIDE) > 32;
    }
    if ((name == L"CFile" || name == L"OwriteFile" || name == L"AwriteFile") && args.size() >= 2) {
        fs::path p = ExpandEnvText(ToText(args[0]));
        fs::create_directories(p.parent_path());
        std::ofstream out(p, std::ios::binary | (name == L"AwriteFile" ? std::ios::app : std::ios::trunc));
        out << WideToUtf8(ToText(args[1]));
        return out.good();
    }
    if (name == L"DFile" && args.size() >= 1) {
        std::error_code ec;
        return fs::remove(ExpandEnvText(ToText(args[0])), ec);
    }
    if (name == L"Sleep" && args.size() >= 1) {
        int waitMs = (int)ToNumber(args[0]);
        if (waitMs < 0) waitMs = 0;
        Sleep((DWORD)waitMs);
        return true;
    }
    if (name == L"SetDeathVolume" && args.size() >= 1) {
        g_death_vol = (float)std::clamp(ToNumber(args[0]), 0.0, 1.0);
        SaveEvolutionParams();
        if (s_owner) InvalidateRect(s_owner, nullptr, FALSE);
        return true;
    }
    if (name == L"SetDeathMute" && args.size() >= 1) {
        bool next = Truthy(args[0]);
        if (next) {
            if (!normalgen::CheckAdminPermission()) return true;
            StartCS2VolumeControl(g_death_vol);
        } else {
            StopCS2VolumeControl();
        }
        g_deathMute = next;
        SaveEvolutionParams();
        if (s_owner) InvalidateRect(s_owner, nullptr, FALSE);
        return true;
    }
    if (name == L"SetCrosshair" && args.size() >= 7) {
        g_crosshairEnabled = Truthy(args[0]);
        g_crosshairR = std::clamp((int)ToNumber(args[1]), 0, 255);
        g_crosshairG = std::clamp((int)ToNumber(args[2]), 0, 255);
        g_crosshairB = std::clamp((int)ToNumber(args[3]), 0, 255);
        g_crosshairStyle = std::clamp((int)ToNumber(args[4]), 0, 2);
        g_crosshairThickness = std::clamp((int)ToNumber(args[5]), 1, 10);
        g_crosshairScale = (float)std::clamp(ToNumber(args[6]), 0.1, 0.6);
        SaveEvolutionParams();
        if (s_owner) InvalidateRect(s_owner, nullptr, FALSE);
        return true;
    }
    std::wcout << L"[脚本] 未知函数: " << name << std::endl;
    return false;
}

void ExecuteBlock(const std::wstring& script);
void ExecuteStatement(const std::wstring& stmt);

bool ExtractControlBlock(const std::wstring& s, const std::wstring& keyword, std::wstring& head, std::wstring& body)
{
    if (s.rfind(keyword, 0) != 0) return false;
    size_t lp = s.find(L'(');
    size_t rp = s.find(L')', lp);
    size_t lb = s.find(L'{', rp);
    size_t rb = s.rfind(L'}');
    if (lp == std::wstring::npos || rp == std::wstring::npos || lb == std::wstring::npos || rb == std::wstring::npos || rb < lb) return true;
    head = s.substr(lp + 1, rp - lp - 1);
    body = s.substr(lb + 1, rb - lb - 1);
    return true;
}

bool TryExecuteIf(const std::wstring& stmt)
{
    std::wstring s = Trim(stmt);
    if (s.rfind(L"if", 0) != 0) return false;
    size_t lp = s.find(L'(');
    size_t rp = s.find(L')', lp);
    size_t lb = s.find(L'{', rp);
    size_t rb = s.find(L'}', lb);
    if (lp == std::wstring::npos || rp == std::wstring::npos || lb == std::wstring::npos || rb == std::wstring::npos || rb < lb) return true;
    std::wstring cond = s.substr(lp + 1, rp - lp - 1);
    std::wstring body = s.substr(lb + 1, rb - lb - 1);
    if (EvalCondition(cond)) ExecuteBlock(body);
    else {
        size_t elsePos = s.find(L"else", rb + 1);
        if (elsePos != std::wstring::npos) {
            size_t elb = s.find(L'{', elsePos);
            size_t erb = s.rfind(L'}');
            if (elb != std::wstring::npos && erb != std::wstring::npos && erb > elb) ExecuteBlock(s.substr(elb + 1, erb - elb - 1));
        }
    }
    return true;
}

bool TryExecuteWhile(const std::wstring& stmt)
{
    std::wstring s = Trim(stmt);
    std::wstring cond, body;
    if (!ExtractControlBlock(s, L"while", cond, body)) return false;
    for (int i = 0; i < 1000 && EvalCondition(cond) && !s_returnRequested; ++i) {
        ExecuteBlock(body);
    }
    return true;
}

bool TryExecuteFor(const std::wstring& stmt)
{
    std::wstring s = Trim(stmt);
    std::wstring head, body;
    if (!ExtractControlBlock(s, L"for", head, body)) return false;
    std::vector<std::wstring> parts;
    std::wstring cur;
    for (wchar_t c : head) {
        if (c == L';') {
            parts.push_back(Trim(cur));
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    parts.push_back(Trim(cur));
    if (!parts.empty()) ExecuteStatement(parts[0]);
    for (int i = 0; i < 1000 && !s_returnRequested; ++i) {
        if (parts.size() > 1 && !parts[1].empty() && !EvalCondition(parts[1])) break;
        ExecuteBlock(body);
        if (parts.size() > 2 && !parts[2].empty()) ExecuteStatement(parts[2]);
    }
    return true;
}

void ExecuteStatement(const std::wstring& stmt)
{
    std::wstring s = Trim(stmt);
    if (s.empty()) return;
    if (s_returnRequested) return;
    if (s == L"return") { s_returnRequested = true; return; }
    if (s.rfind(L"goto ", 0) == 0) {
        s_gotoTarget = Trim(s.substr(5));
        return;
    }
    if (TryExecuteIf(s)) return;
    if (TryExecuteWhile(s)) return;
    if (TryExecuteFor(s)) return;

    for (const auto& prefix : { L"int ", L"float ", L"string " }) {
        if (s.rfind(prefix, 0) == 0) {
            size_t eq = s.find(L'=');
            std::wstring name = Trim(s.substr(wcslen(prefix), eq == std::wstring::npos ? std::wstring::npos : eq - wcslen(prefix)));
            s_vars[name] = eq == std::wstring::npos ? value{} : EvalExpr(s.substr(eq + 1));
            return;
        }
    }
    size_t eq = s.find(L'=');
    if (eq != std::wstring::npos && s.find(L"==") == std::wstring::npos) {
        std::wstring name = Trim(s.substr(0, eq));
        s_vars[name] = EvalExpr(s.substr(eq + 1));
        return;
    }
    auto fn = ParseFunction(s);
    if (!fn) return;
    ExecuteFunction(fn->first, SplitArgs(fn->second));
}

void ExecuteBlock(const std::wstring& script)
{
    std::vector<std::wstring> statements = SplitStatements(script);
    std::unordered_map<std::wstring, size_t> labels;
    for (size_t i = 0; i < statements.size(); ++i) {
        std::wstring s = Trim(statements[i]);
        if (!s.empty() && s.back() == L':' && s.find(L' ') == std::wstring::npos) {
            labels[Trim(s.substr(0, s.size() - 1))] = i;
        }
    }
    for (size_t pc = 0; pc < statements.size() && !s_returnRequested; ++pc) {
        std::wstring s = Trim(statements[pc]);
        if (!s.empty() && s.back() == L':' && s.find(L' ') == std::wstring::npos) continue;
        ExecuteStatement(s);
        if (s_gotoTarget) {
            auto it = labels.find(*s_gotoTarget);
            s_gotoTarget.reset();
            if (it != labels.end()) pc = it->second;
        }
    }
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

void RegisterBuildWarning()
{
    if (s_runtimeCapability == buildcode::userdebug && GetBuildCode() == buildcode::user && s_owner) {
        HWND owner = s_owner;
        std::thread([owner]() {
            MessageBoxW(owner, L"检测到 OEM 调试解锁，程序将启用 userdebug 能力。\n\n请确认你理解脚本可以执行 shell 和文件写入等高权限操作。", L"StrikeSense 调试功能警告", MB_OK | MB_ICONWARNING);
        }).detach();
    }
}

} // namespace

void Initialize(HINSTANCE instance, HWND owner)
{
    s_instance = instance;
    s_owner = owner;
    fs::create_directories(GetDefaultScriptDir());
    EnsureExampleScript();
    EnsureOemUnlockFile();
    s_oemValid = IsOemUnlockValid();
    s_runtimeCapability = GetBuildCode();
    if (s_runtimeCapability == buildcode::user && s_oemValid) s_runtimeCapability = buildcode::userdebug;
    RegisterBuildWarning();
    LoadMountedScripts();
    std::cout << "[脚本] vscrpit 初始化完成，构建等级=" << (int)GetBuildCode()
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
    if (fs::exists(path)) return true;
    WriteUtf8(path, MakeOemKey(NowStamp(), L"24h"));
    std::wcout << L"[OEM] 已生成默认 24 小时调试解锁文件: " << path.wstring() << std::endl;
    return true;
}

bool IsOemUnlockValid()
{
    fs::path path = BaseDir() / L".oemunlock";
    if (!fs::exists(path)) return false;
    std::wstring key = Trim(ReadAllWide(path));
    return ParseOemKey(key);
}

void UpdateFromGsi(const nlohmann::json& state)
{
    std::lock_guard<std::recursive_mutex> lock(s_mutex);
    s_prevVars = s_vars;
    for (const auto& key : s_stateKeys) s_vars.erase(key);
    s_stateKeys.clear();
    FlattenJsonState(L"gsi", state);
    if (state.contains("map") && state["map"].is_object()) {
        SetJsonVar(L"map", state["map"], "name");
        SetJsonVar(L"map_mode", state["map"], "mode");
        SetJsonVar(L"map_phase", state["map"], "phase");
    } else {
        SetStateVar(L"map", TextValue(L"void"));
        SetStateVar(L"map_mode", TextValue(L"void"));
        SetStateVar(L"map_phase", TextValue(L"void"));
    }
    if (state.contains("round") && state["round"].is_object()) {
        SetJsonVar(L"round_phase", state["round"], "phase");
        SetJsonVar(L"bomb", state["round"], "bomb");
    } else {
        SetStateVar(L"round_phase", TextValue(L"void"));
        SetStateVar(L"bomb", TextValue(L"void"));
    }
    if (state.contains("player") && state["player"].is_object()) {
        const auto& player = state["player"];
        SetJsonVar(L"activity", player, "activity");
        SetJsonVar(L"steamid", player, "steamid");
        SetJsonVar(L"team", player, "team");
        if (player.contains("state") && player["state"].is_object()) {
            SetJsonVar(L"kills", player["state"], "round_kills");
            SetJsonVar(L"health", player["state"], "health");
            SetJsonVar(L"flashed", player["state"], "flashed");
            SetStateVar(L"death_mute", BoolValue(ToNumber(GetVar(L"health")) <= 0));
        } else {
            SetStateVar(L"kills", TextValue(L"void"));
            SetStateVar(L"health", TextValue(L"void"));
            SetStateVar(L"flashed", TextValue(L"void"));
            SetStateVar(L"death_mute", TextValue(L"void"));
        }
        if (player.contains("match_stats") && player["match_stats"].is_object())
            SetJsonVar(L"mvps", player["match_stats"], "mvps");
        else
            SetStateVar(L"mvps", TextValue(L"void"));

        SetStateVar(L"weapon_name", TextValue(L"void"));
        SetStateVar(L"weapon_type", TextValue(L"void"));
        SetStateVar(L"weapon_state", TextValue(L"void"));
        if (player.contains("weapons") && player["weapons"].is_object()) {
            for (auto it = player["weapons"].begin(); it != player["weapons"].end(); ++it) {
                const auto& weapon = it.value();
                if (!weapon.is_object()) continue;
                std::string stateText = weapon.value("state", "");
                if (stateText != "active") continue;
                SetJsonVar(L"weapon_name", weapon, "name");
                SetJsonVar(L"weapon_type", weapon, "type");
                SetJsonVar(L"weapon_state", weapon, "state");
                break;
            }
        }
    }
}

void TickContinuousScripts()
{
    std::lock_guard<std::recursive_mutex> lock(s_mutex);
    for (const auto& script : s_mounted) {
        if (script.continuous) {
            std::wstring text = LoadScriptCached(script.path);
            if (!text.empty()) {
                s_returnRequested = false;
                s_gotoTarget.reset();
                ExecuteBlock(StripComments(text));
            }
        }
    }
    s_prevVars = s_vars;
}

bool ExecuteScriptFile(const std::wstring& path)
{
    std::lock_guard<std::recursive_mutex> lock(s_mutex);
    std::wstring script = ReadAllWide(path);
    if (script.empty()) {
        std::wcout << L"[脚本] 脚本为空或读取失败: " << path << std::endl;
        return false;
    }
    s_scriptCache[path] = script;
    std::wcout << L"[脚本] 执行脚本: " << path << std::endl;
    s_returnRequested = false;
    s_gotoTarget.reset();
    ExecuteBlock(StripComments(script));
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
    return (fs::path(GetDefaultScriptDir()) / L"death_douyin.vscrpit").wstring();
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
                    s_scriptCache[s.path] = ReadAllWide(s.path);
                    LoadScriptMetadata(s);
                    if (s.continuous && !ConfirmContinuousAllowed(s.path)) s.continuous = false;
                    s_mounted.push_back(s);
                }
            }
        }
        std::cout << "[脚本配置] 已加载挂载脚本数量=" << s_mounted.size() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[脚本配置] 加载失败: " << e.what() << std::endl;
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
    std::cout << "[脚本配置] 已保存挂载脚本数量=" << s_mounted.size() << std::endl;
}

void AddMountedScript(const std::wstring& path)
{
    auto it = std::find_if(s_mounted.begin(), s_mounted.end(), [&](const mounted_script& s) { return s.path == path; });
    s_scriptCache[path] = ReadAllWide(path);
    if (it == s_mounted.end()) {
        mounted_script script;
        script.path = path;
        LoadScriptMetadata(script);
        s_mounted.push_back(script);
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
    if (!s_mounted[index].continuous && !ConfirmContinuousAllowed(s_mounted[index].path)) {
        std::wcout << L"[脚本权限] 已拒绝开启持续执行: " << s_mounted[index].path << std::endl;
        return;
    }
    s_mounted[index].continuous = !s_mounted[index].continuous;
    SaveMountedScripts();
}

void EnsureExampleScript()
{
    fs::path dir = GetDefaultScriptDir();
    fs::create_directories(dir / L"assets" / L"kills");
    std::cout << "[脚本] 已确认脚本目录与资源目录存在" << std::endl;
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

} // namespace vscrpit
