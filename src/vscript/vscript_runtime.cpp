#include "vscript_internal.h"

#include <TlHelp32.h>
#include <Shellapi.h>
#include <Shlwapi.h>
#include <Psapi.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <thread>

#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Psapi.lib")

namespace vscript::detail {

namespace {

struct top_request {
    std::wstring process;
    bool activate = true;
    bool firstOnly = false;
};

} // namespace

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

bool IsLikelyMainWindow(HWND hwnd)
{
    if (!IsWindowVisible(hwnd)) return false;
    if (GetWindow(hwnd, GW_OWNER) != nullptr) return false;
    if (GetAncestor(hwnd, GA_ROOT) != hwnd) return false;
    LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    if ((exStyle & WS_EX_TOOLWINDOW) != 0) return false;
    return true;
}

std::wstring NormalizeProcessName(const std::wstring& process)
{
    if (process.empty()) return process;
    if (process.find(L'\\') != std::wstring::npos || process.find(L'/') != std::wstring::npos)
        return PathFindFileNameW(process.c_str());
    return process;
}

bool WindowMatchesProcess(HWND hwnd, const std::wstring& process)
{
    if (!IsLikelyMainWindow(hwnd)) return false;
    const std::wstring name = ProcessNameFromWindow(hwnd);
    if (name.empty()) return false;
    const std::wstring target = NormalizeProcessName(process);
    return _wcsicmp(name.c_str(), target.c_str()) == 0;
}

bool ActivateWindowSafely(HWND hwnd)
{
    if (!hwnd || !IsWindow(hwnd)) return false;

    if (IsIconic(hwnd)) ShowWindowAsync(hwnd, SW_RESTORE);
    else ShowWindowAsync(hwnd, SW_SHOW);

    DWORD currentThreadId = GetCurrentThreadId();
    DWORD foregroundThreadId = 0;
    if (HWND fg = GetForegroundWindow()) foregroundThreadId = GetWindowThreadProcessId(fg, nullptr);

    if (foregroundThreadId && foregroundThreadId != currentThreadId)
        AttachThreadInput(currentThreadId, foregroundThreadId, TRUE);

    BringWindowToTop(hwnd);
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    SetForegroundWindow(hwnd);
    SetActiveWindow(hwnd);
    SetFocus(hwnd);

    if (foregroundThreadId && foregroundThreadId != currentThreadId)
        AttachThreadInput(currentThreadId, foregroundThreadId, FALSE);

    return GetForegroundWindow() == hwnd;
}

std::optional<HWND> FindProcessMainWindow(const std::wstring& process)
{
    struct find_request {
        std::wstring process;
        std::optional<HWND> result;
    } req{ process, std::nullopt };

    EnumWindows([](HWND hwnd, LPARAM lp) -> BOOL {
        auto* request = reinterpret_cast<find_request*>(lp);
        if (!WindowMatchesProcess(hwnd, request->process)) return TRUE;
        request->result = hwnd;
        return FALSE;
    }, reinterpret_cast<LPARAM>(&req));

    return req.result;
}

bool FocusKnownBrowserWindow()
{
    for (const wchar_t* process : { L"msedge.exe", L"chrome.exe", L"firefox.exe", L"browser.exe", L"iexplore.exe" }) {
        if (auto hwnd = FindProcessMainWindow(process)) {
            const bool ok = ActivateWindowSafely(*hwnd);
            std::wcout << L"[脚本] 已尝试切到浏览器窗口，进程=" << process
                       << L"，结果=" << (ok ? L"成功" : L"未完全成功") << std::endl;
            return true;
        }
    }
    std::wcout << L"[脚本] 当前未找到已打开的浏览器窗口" << std::endl;
    return false;
}

BOOL CALLBACK EnumTopProcess(HWND hwnd, LPARAM lp)
{
    auto* req = reinterpret_cast<top_request*>(lp);
    if (!WindowMatchesProcess(hwnd, req->process)) return TRUE;
    if (req->activate) {
        ActivateWindowSafely(hwnd);
    } else {
        BringWindowToTop(hwnd);
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
    return req->firstOnly ? FALSE : TRUE;
}

bool TopProcess(const std::wstring& process, bool activate)
{
    top_request req{ process, activate, false };
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

bool OpenTargetExternal(const std::wstring& target)
{
    if (target.empty()) {
        std::wcout << L"[脚本] 打开目标失败：目标为空" << std::endl;
        return false;
    }

    const std::wstring expanded = ExpandEnvText(target);
    const INT_PTR result = (INT_PTR)ShellExecuteW(nullptr, L"open", expanded.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    const bool ok = result > 32;
    std::wcout << L"[脚本] " << (ok ? L"已请求打开目标: " : L"打开目标失败: ") << expanded << std::endl;
    return ok;
}

bool IsProcessRunningByName(const std::wstring& processName)
{
    const std::wstring normalized = NormalizeProcessName(processName);
    if (normalized.empty()) return false;

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) {
        std::wcout << L"[脚本] 进程枚举失败，无法检查是否运行: " << normalized << std::endl;
        return false;
    }

    bool found = false;
    PROCESSENTRY32W pe{};
    pe.dwSize = sizeof(pe);
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, normalized.c_str()) == 0) {
                found = true;
                break;
            }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    std::wcout << L"[脚本] 进程运行检查: " << normalized
               << L" -> " << (found ? L"存在" : L"不存在") << std::endl;
    return found;
}

bool EnsureProcessWindow(const std::wstring& processName, const std::wstring& launchTarget, bool activate)
{
    const std::wstring normalized = NormalizeProcessName(processName);
    if (normalized.empty()) {
        std::wcout << L"[脚本] EnsureProcessWindow 失败：进程名为空" << std::endl;
        return false;
    }

    if (IsProcessRunningByName(normalized)) {
        std::wcout << L"[脚本] 目标进程已存在，准备切换窗口: " << normalized << std::endl;
        return TopProcess(normalized, activate);
    }

    if (launchTarget.empty()) {
        std::wcout << L"[脚本] 目标进程不存在，且未提供启动目标: " << normalized << std::endl;
        return false;
    }

    const bool launched = OpenTargetExternal(launchTarget);
    if (!launched) return false;

    std::thread([normalized, activate]() {
        for (int i = 0; i < 20; ++i) {
            Sleep(300);
            if (IsProcessRunningByName(normalized)) {
                TopProcess(normalized, activate);
                return;
            }
        }
        std::wcout << L"[脚本] 已启动目标，但超时未找到窗口: " << normalized << std::endl;
    }).detach();
    return true;
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

bool HideGameWindowSafely()
{
    if (FocusKnownBrowserWindow()) {
        std::wcout << L"[脚本] 已优先切到浏览器，避免直接最小化 CS2 导致窗口状态异常" << std::endl;
        return true;
    }

    if (FindProcessMainWindow(L"cs2.exe")) {
        std::wcout << L"[脚本] 检测到 CS2 主窗口，但为了安全未执行强制最小化，请配合 Browser(...) 使用" << std::endl;
        return true;
    }

    std::wcout << L"[脚本] 未找到 CS2 主窗口，无法切出游戏" << std::endl;
    return false;
}

bool ShowGameProcessSafely()
{
    auto hwnd = FindProcessMainWindow(L"cs2.exe");
    if (!hwnd) {
        std::wcout << L"[脚本] 未找到 CS2 主窗口，无法切回游戏" << std::endl;
        return false;
    }

    const bool ok = ActivateWindowSafely(*hwnd);
    std::wcout << L"[脚本] 已尝试安全切回 CS2，结果=" << (ok ? L"成功" : L"未完全成功") << std::endl;
    return true;
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

bool ApplyPerPixelAlphaImage(HWND hwnd, Gdiplus::Image* image, int width, int height, int x, int y, float opacity)
{
    if (!hwnd || !image || width <= 0 || height <= 0) return false;

    Gdiplus::Bitmap surface(width, height, PixelFormat32bppPARGB);
    Gdiplus::Graphics graphics(&surface);
    graphics.SetCompositingMode(Gdiplus::CompositingModeSourceOver);
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
    graphics.Clear(Gdiplus::Color(0, 0, 0, 0));
    graphics.DrawImage(image, 0, 0, width, height);

    Gdiplus::Rect rect(0, 0, width, height);
    Gdiplus::BitmapData bitmapData{};
    if (surface.LockBits(&rect, Gdiplus::ImageLockModeRead, PixelFormat32bppPARGB, &bitmapData) != Gdiplus::Ok) {
        std::wcout << L"[脚本] 锁定位图像素失败，无法应用真 alpha 通道" << std::endl;
        return false;
    }

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC screenDc = GetDC(nullptr);
    HDC memDc = CreateCompatibleDC(screenDc);
    void* dibPixels = nullptr;
    HBITMAP dib = CreateDIBSection(screenDc, &bmi, DIB_RGB_COLORS, &dibPixels, nullptr, 0);
    HBITMAP oldBitmap = nullptr;

    bool ok = false;
    if (screenDc && memDc && dib && dibPixels) {
        oldBitmap = (HBITMAP)SelectObject(memDc, dib);
        const size_t rowBytes = (size_t)width * 4;
        for (int row = 0; row < height; ++row) {
            std::memcpy(
                static_cast<unsigned char*>(dibPixels) + row * rowBytes,
                static_cast<unsigned char*>(bitmapData.Scan0) + row * bitmapData.Stride,
                rowBytes);
        }

        POINT dstPt{ x, y };
        SIZE size{ width, height };
        POINT srcPt{ 0, 0 };
        BLENDFUNCTION blend{};
        blend.BlendOp = AC_SRC_OVER;
        blend.SourceConstantAlpha = (BYTE)std::clamp((int)(opacity * 255.0f), 0, 255);
        blend.AlphaFormat = AC_SRC_ALPHA;

        ok = UpdateLayeredWindow(hwnd, screenDc, &dstPt, &size, memDc, &srcPt, 0, &blend, ULW_ALPHA) == TRUE;
    }

    surface.UnlockBits(&bitmapData);
    if (oldBitmap) SelectObject(memDc, oldBitmap);
    if (dib) DeleteObject(dib);
    if (memDc) DeleteDC(memDc);
    if (screenDc) ReleaseDC(nullptr, screenDc);

    if (!ok) {
        std::wcout << L"[脚本] UpdateLayeredWindow 失败，无法显示真 alpha 图片" << std::endl;
    }
    return ok;
}

bool DrawImageCommand(const std::filesystem::path& path, int offsetX, int offsetY, bool alphaChannel, float opacity, int ttlMs, int id)
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
    HWND hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TRANSPARENT,
        k_imageClass, L"", WS_POPUP, x, y, w, h, nullptr, nullptr, s_instance, nullptr);
    if (!hwnd) return false;
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, id);
    s_images[id] = image_window{ hwnd, std::move(img), w, h, opacity, alphaChannel };
    ShowWindow(hwnd, SW_SHOWNA);

    if (alphaChannel) {
        if (!ApplyPerPixelAlphaImage(hwnd, s_images[id].image.get(), w, h, x, y, opacity)) {
            DestroyWindow(hwnd);
            s_images.erase(id);
            return false;
        }
    } else {
        BYTE a = (BYTE)std::clamp((int)(opacity * 255.0f), 0, 255);
        SetLayeredWindowAttributes(hwnd, 0, a, LWA_ALPHA);
        InvalidateRect(hwnd, nullptr, FALSE);
    }

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

bool PlaySoundCommand(const std::filesystem::path& path, float volume, int id)
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

namespace {

struct function_execution_guard {
    execution_context* previous = nullptr;

    explicit function_execution_guard(execution_context& current)
        : previous(s_activeExecution)
    {
        s_activeExecution = &current;
    }

    ~function_execution_guard()
    {
        s_activeExecution = previous;
    }
};

value ExecuteScriptFunctionValue(const std::wstring& name, const std::vector<value>& args)
{
    execution_context& caller = CurrentExecution();
    auto functionIt = caller.functions.find(name);
    if (functionIt == caller.functions.end()) return TextValue(L"void");

    const std::wstring definition = functionIt->second;
    const size_t bracePos = definition.find(L'{');
    const size_t parenPos = definition.find(L'(');
    const size_t closeParenPos = definition.rfind(L')');
    const size_t endBracePos = definition.rfind(L'}');
    if (bracePos == std::wstring::npos || parenPos == std::wstring::npos || closeParenPos == std::wstring::npos || endBracePos == std::wstring::npos) {
        std::wcout << L"[脚本] 函数定义损坏，无法执行: " << name << std::endl;
        return TextValue(L"void");
    }

    const std::wstring header = Trim(definition.substr(0, bracePos));
    const size_t namePos = header.find_last_of(L' ', parenPos);
    const std::wstring returnType = namePos == std::wstring::npos ? L"void" : Trim(header.substr(0, namePos));
    const std::vector<std::wstring> params = ParseScriptFunctionParams(definition.substr(parenPos + 1, closeParenPos - parenPos - 1));
    const std::wstring body = definition.substr(bracePos + 1, endBracePos - bracePos - 1);

    execution_context child;
    child.functions = caller.functions;
    child.localScopes.emplace_back();
    for (size_t i = 0; i < params.size(); ++i) {
        const value assigned = i < args.size() ? args[i] : TextValue(L"void");
        child.localScopes.back()[params[i]] = assigned;
        std::wcout << L"[脚本] 绑定函数参数: " << name << L"." << params[i] << L" = " << ToText(assigned) << std::endl;
    }

    function_execution_guard guard(child);
    std::wcout << L"[脚本] 开始执行脚本函数: " << name << std::endl;
    ExecuteBlock(body);

    const value result = CoerceValueForType(child.returnValue.value_or(TextValue(L"void")), returnType);
    std::wcout << L"[脚本] 脚本函数返回: " << name << L" -> " << ToText(result) << std::endl;
    return result;
}

} // namespace

value ExecuteFunction(const std::wstring& name, const std::vector<std::wstring>& rawArgs)
{
    if (RequiresUserDebug(name) && !s_currentPrivilegedAllowed) {
        std::wcout << L"[脚本权限] 当前脚本无权执行高权限命令: " << name
                   << L"  脚本=" << s_currentScriptPath << std::endl;
        return BoolValue(false);
    }

    std::vector<value> args;
    for (const auto& a : rawArgs) args.push_back(EvalExpr(a));

    execution_context& exec = CurrentExecution();
    if (exec.functions.find(name) != exec.functions.end()) {
        return ExecuteScriptFunctionValue(name, args);
    }

    if (name == L"CloseGameWindow") return BoolValue(HideGameWindowSafely());
    if (name == L"KillGameProcess") return BoolValue(KillProcessByName(L"cs2.exe"));
    if (name == L"RunGameProcess") return BoolValue(LaunchUrlExternal(L"steam://run/730"));
    if (name == L"ShowGameProcess") return BoolValue(ShowGameProcessSafely());
    if (name == L"Browser" && args.size() >= 1) {
        const bool preferExisting = args.size() >= 3 && Truthy(args[2]);
        bool ok = false;
        if (preferExisting) {
            ok = FocusKnownBrowserWindow();
            if (!ok) {
                std::wcout << L"[脚本] 未找到现有浏览器窗口，准备打开目标网址" << std::endl;
            }
        }
        if (!ok) ok = LaunchUrlExternal(ToText(args[0]));
        if (ok && args.size() >= 2 && Truthy(args[1])) TopBrowserSoon();
        return BoolValue(ok);
    }
    if (name == L"Open" && args.size() >= 1) return BoolValue(OpenTargetExternal(ToText(args[0])));
    if (name == L"EnsureProcessWindow" && args.size() >= 2) {
        return BoolValue(EnsureProcessWindow(
            ToText(args[0]),
            ToText(args[1]),
            args.size() < 3 || Truthy(args[2])));
    }
    if (name == L"Top" && args.size() >= 1) return BoolValue(TopProcess(ToText(args[0]), args.size() < 2 || Truthy(args[1])));
    if (name == L"Drawimg" && args.size() >= 7) {
        return BoolValue(DrawImageCommand(ExpandEnvText(ToText(args[0])), (int)ToNumber(args[1]), (int)ToNumber(args[2]), Truthy(args[3]), (float)ToNumber(args[4]), (int)ToNumber(args[5]), (int)ToNumber(args[6])));
    }
    if (name == L"Closeimg" && args.size() >= 1) {
        CloseImage((int)ToNumber(args[0]));
        return BoolValue(true);
    }
    if (name == L"Playsnd" && args.size() >= 3) return BoolValue(PlaySoundCommand(ExpandEnvText(ToText(args[0])), (float)ToNumber(args[1]), (int)ToNumber(args[2])));
    if (name == L"Stopsnd" && args.size() >= 1) {
        StopSoundCommand((int)ToNumber(args[0]));
        return BoolValue(true);
    }
    if (name == L"ShellExecute" && args.size() >= 1) {
        std::wstring cmd = L"/C " + ToText(args[0]);
        return BoolValue((INT_PTR)ShellExecuteW(nullptr, L"open", L"cmd.exe", cmd.c_str(), nullptr, SW_HIDE) > 32);
    }
    if ((name == L"CFile" || name == L"OwriteFile" || name == L"AwriteFile") && args.size() >= 2) {
        std::filesystem::path p = ExpandEnvText(ToText(args[0]));
        std::filesystem::create_directories(p.parent_path());
        std::ofstream out(p, std::ios::binary | (name == L"AwriteFile" ? std::ios::app : std::ios::trunc));
        out << WideToUtf8(ToText(args[1]));
        return BoolValue(out.good());
    }
    if (name == L"DFile" && args.size() >= 1) {
        std::error_code ec;
        return BoolValue(std::filesystem::remove(ExpandEnvText(ToText(args[0])), ec));
    }
    if (name == L"Sleep" && args.size() >= 1) {
        int waitMs = (int)ToNumber(args[0]);
        if (waitMs < 0) waitMs = 0;
        Sleep((DWORD)waitMs);
        return BoolValue(true);
    }
    if (name == L"Log" && args.size() >= 1) {
        std::wcout << L"[脚本] " << ToText(args[0]) << std::endl;
        return BoolValue(true);
    }
    if (name == L"SetProcessVolume" && args.size() >= 2) {
        const std::wstring processName = ToText(args[0]);
        const float volumePercent = (float)ToNumber(args[1]);
        std::wcout << L"[脚本] 请求设置进程音量，进程=" << processName
                   << L"，目标百分比=" << volumePercent << std::endl;
        return BoolValue(SetProcessVolumeByName(processName, volumePercent));
    }
    if (name == L"SetProcessMute" && args.size() >= 2) {
        const std::wstring processName = ToText(args[0]);
        const bool muted = Truthy(args[1]);
        std::wcout << L"[脚本] 请求设置进程静音，进程=" << processName
                   << L"，静音=" << (muted ? L"true" : L"false") << std::endl;
        return BoolValue(SetProcessMuteByName(processName, muted));
    }
    if (name == L"SetDeathVolume" && args.size() >= 1) {
        g_death_vol = (float)std::clamp(ToNumber(args[0]), 0.0, 1.0);
        SaveEvolutionParams();
        if (s_owner) InvalidateRect(s_owner, nullptr, FALSE);
        return BoolValue(true);
    }
    if (name == L"SetDeathMute" && args.size() >= 1) {
        bool next = Truthy(args[0]);
        if (next) {
            if (!normalgen::CheckAdminPermission()) return BoolValue(true);
            StartCS2VolumeControl(g_death_vol);
        } else {
            StopCS2VolumeControl();
        }
        g_deathMute = next;
        SaveEvolutionParams();
        if (s_owner) InvalidateRect(s_owner, nullptr, FALSE);
        return BoolValue(true);
    }
    if (name == L"SetCrosshairEnabled" && args.size() >= 1) {
        ApplyCrosshairEnabled(Truthy(args[0]));
        SaveEvolutionParams();
        if (s_owner) InvalidateRect(s_owner, nullptr, FALSE);
        return BoolValue(true);
    }
    if ((name == L"SetCrosshairVisual" || name == L"SetCrosshairConfig") && args.size() >= 6) {
        ApplyCrosshairVisual(
            std::clamp((int)ToNumber(args[0]), 0, 255),
            std::clamp((int)ToNumber(args[1]), 0, 255),
            std::clamp((int)ToNumber(args[2]), 0, 255),
            std::clamp((int)ToNumber(args[3]), 0, 2),
            std::clamp((int)ToNumber(args[4]), 1, 10),
            (float)std::clamp(ToNumber(args[5]), 0.1, 0.6));
        SaveEvolutionParams();
        if (s_owner) InvalidateRect(s_owner, nullptr, FALSE);
        return BoolValue(true);
    }
    if (name == L"SetCrosshair" && args.size() >= 7) {
        ApplyCrosshairEnabled(Truthy(args[0]));
        ApplyCrosshairVisual(
            std::clamp((int)ToNumber(args[1]), 0, 255),
            std::clamp((int)ToNumber(args[2]), 0, 255),
            std::clamp((int)ToNumber(args[3]), 0, 255),
            std::clamp((int)ToNumber(args[4]), 0, 2),
            std::clamp((int)ToNumber(args[5]), 1, 10),
            (float)std::clamp(ToNumber(args[6]), 0.1, 0.6));
        SaveEvolutionParams();
        if (s_owner) InvalidateRect(s_owner, nullptr, FALSE);
        return BoolValue(true);
    }
    std::wcout << L"[脚本] 未知函数: " << name << std::endl;
    return BoolValue(false);
}

} // namespace vscript::detail
