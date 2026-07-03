#pragma once

#include "vscript.h"
#include "pages.h"
#include "volume_mixer.h"
#include "normalgen.h"
#include "gsi_server.h"

#include <Windows.h>
#include <SDL_mixer.h>
#include <gdiplus.h>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace vscript::detail {

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
    bool layeredAlpha = false;
};

struct sound_slot {
    Mix_Chunk* chunk = nullptr;
    int channel = -1;
};

struct weapon_snapshot {
    std::wstring name;
    std::wstring state;
    double clip = -1.0;
    double reserve = -1.0;
    bool valid = false;
};

struct execution_context {
    bool returnRequested = false;
    std::optional<std::wstring> gotoTarget;
    std::optional<value> returnValue;
    std::unordered_map<std::wstring, std::wstring> functions;
    std::vector<std::map<std::wstring, value>> localScopes;
};

extern HINSTANCE s_instance;
extern HWND s_owner;
extern buildcode s_runtimeCapability;
extern bool s_oemValid;
extern std::vector<mounted_script> s_mounted;
extern std::map<std::wstring, value> s_vars;
extern std::map<std::wstring, value> s_prevVars;
extern std::unordered_map<int, image_window> s_images;
extern std::unordered_map<int, sound_slot> s_sounds;
extern std::recursive_mutex s_mutex;
extern std::unordered_map<std::wstring, std::wstring> s_scriptCache;
extern std::unordered_set<std::wstring> s_stateKeys;
extern execution_context* s_activeExecution;
extern bool s_currentPrivilegedAllowed;
extern std::wstring s_currentScriptPath;
extern int s_weaponFireCount;
extern int s_weaponReloadCount;
extern int s_weaponReserveDropCount;
extern weapon_snapshot s_lastWeaponSnapshot;

inline constexpr const wchar_t* k_imageClass = L"StrikeSenseVscriptImage";
inline constexpr const char* k_alphabet = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_";
inline constexpr const wchar_t* k_privilegedApiNames[] = {
    L"ShellExecute", L"CFile", L"DFile", L"OwriteFile", L"AwriteFile"
};

std::wstring Utf8ToWide(const std::string& s);
std::string WideToUtf8(const std::wstring& s);
std::filesystem::path BaseDir();
std::wstring NowStamp();
int DaysForType(const std::wstring& type);
int DateToDays(int y, int m, int d);
std::time_t MakeTime(int y, int m, int d, int hh, int mm);
std::time_t CurrentTime();
uint32_t Fnv1a(const std::string& s);
std::string Encode64(const std::string& raw);
std::optional<std::string> Decode64(const std::string& enc);
std::string XorPayload(std::string raw);
std::wstring MakeOemKey(const std::wstring& stamp, const std::wstring& type);
bool ParseOemKey(const std::wstring& key);
std::wstring ReadAllWide(const std::filesystem::path& path);
std::wstring LoadScriptCached(const std::wstring& path);
std::wstring StripComments(const std::wstring& script);
std::wstring ReadMetaValue(const std::wstring& line, const std::wstring& key);
std::wstring GetWindowsUserName();
bool EqualsIgnoreCase(const std::wstring& a, const std::wstring& b);
std::vector<std::wstring> SplitMetaTokens(const std::wstring& text);
bool ScriptUsesPrivilegedApis(const std::wstring& script);
void ApplyModifierMetadata(mounted_script& script);
void ResetScriptMetadata(mounted_script& script);
void LoadScriptMetadata(mounted_script& script);
bool IsPrivilegedAllowedForScript(const mounted_script& script);
std::wstring BuildRiskNotice(const mounted_script& script);
bool RefreshScriptState(mounted_script& script, bool showDialogs);
void ResetTransientWeaponVars();
bool ScriptHasEdgeGuard(const std::wstring& path);
bool ConfirmContinuousAllowed(const std::wstring& path);
void WriteUtf8(const std::filesystem::path& path, const std::wstring& text);
std::wstring SanitizeName(const std::wstring& name);
void SetStateVar(const std::wstring& name, const value& v);
void FlattenJsonState(const std::wstring& prefix, const nlohmann::json& j);
value TextValue(const std::wstring& s);
value NumberValue(double n);
value BoolValue(bool b);
bool Truthy(const value& v);
std::wstring ToText(const value& v);
std::wstring ExpandEnvText(const std::wstring& text);
double ToNumber(const value& v);
value GetVar(const std::wstring& name);
value GetVarFromMap(const std::map<std::wstring, value>& vars, const std::wstring& name);
std::wstring Trim(std::wstring s);
std::vector<std::wstring> SplitStatements(const std::wstring& script);
std::vector<std::wstring> SplitArgs(const std::wstring& args);
value EvalExprWithVars(const std::wstring& expr, const std::map<std::wstring, value>& vars);
value EvalExpr(const std::wstring& expr);
value CoerceValueForType(const value& input, const std::wstring& typeName);
bool CompareValues(const value& l, const std::wstring& op, const value& r);
size_t FindLogicalOp(const std::wstring& text, const std::wstring& op);
bool EvalConditionWithVars(std::wstring cond, const std::map<std::wstring, value>& vars);
bool EvalCondition(std::wstring cond);
std::optional<std::pair<std::wstring, std::wstring>> ParseFunction(const std::wstring& stmt);
bool TryParseScriptFunctionDefinition(const std::wstring& stmt, std::wstring& functionName, std::wstring& functionBody);
std::vector<std::wstring> ParseScriptFunctionParams(const std::wstring& headerArgs);
execution_context& CurrentExecution();
std::wstring ProcessNameFromWindow(HWND hwnd);
bool IsLikelyMainWindow(HWND hwnd);
std::wstring NormalizeProcessName(const std::wstring& process);
bool WindowMatchesProcess(HWND hwnd, const std::wstring& process);
bool ActivateWindowSafely(HWND hwnd);
std::optional<HWND> FindProcessMainWindow(const std::wstring& process);
bool FocusKnownBrowserWindow();
BOOL CALLBACK EnumTopProcess(HWND hwnd, LPARAM lp);
bool TopProcess(const std::wstring& process, bool activate);
bool LaunchUrlExternal(const std::wstring& url);
bool OpenTargetExternal(const std::wstring& target);
bool IsProcessRunningByName(const std::wstring& processName);
bool EnsureProcessWindow(const std::wstring& processName, const std::wstring& launchTarget, bool activate);
void TopBrowserSoon();
bool HideGameWindowSafely();
bool ShowGameProcessSafely();
bool KillProcessByName(const std::wstring& exe);
LRESULT CALLBACK ImageProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
void EnsureImageClass();
void CloseImage(int id);
bool ApplyPerPixelAlphaImage(HWND hwnd, Gdiplus::Image* image, int width, int height, int x, int y, float opacity);
bool DrawImageCommand(const std::filesystem::path& path, int offsetX, int offsetY, bool alphaChannel, float opacity, int ttlMs, int id);
bool PlaySoundCommand(const std::filesystem::path& path, float volume, int id);
void StopSoundCommand(int id);
bool RequiresUserDebug(const std::wstring& name);
value ExecuteFunction(const std::wstring& name, const std::vector<std::wstring>& rawArgs);
void ExecuteBlock(const std::wstring& script);
void ExecuteStatement(const std::wstring& stmt);
bool ExtractControlBlock(const std::wstring& s, const std::wstring& keyword, std::wstring& head, std::wstring& body);
bool TryExecuteIf(const std::wstring& stmt);
bool TryExecuteWhile(const std::wstring& stmt);
bool TryExecuteFor(const std::wstring& stmt);
void SetJsonVar(const std::wstring& name, const nlohmann::json& j, const char* key);
void SetPrevAlias(const std::wstring& name, const std::wstring& source);
void RegisterBuildWarning();

} // namespace vscript::detail
