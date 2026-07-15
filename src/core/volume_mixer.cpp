#include "volume_mixer.h"
#include <iostream>
#include <thread>
#include <atomic>
#include <mutex>
#include <algorithm>
#include <cwctype>
#include <functional>
#include <audiopolicy.h>
#include <mmdeviceapi.h>
#include "gsi_server.h"
#include "config.h"

#pragma comment(lib, "ole32.lib")

static std::atomic<bool> g_lenientCs2WindowDetection{ false };

bool IsCS2WindowActive()
    {
        if (g_lenientCs2WindowDetection.load()) return true;
        HWND fg = GetForegroundWindow();
        if (!fg) return false;
        wchar_t title[256];
        GetWindowTextW(fg, title, 256);
        std::wstring wt(title);
        return (wt.find(L"Counter-Strike 2") != std::string::npos ||
                wt.find(L"反恐精英：全球攻势") != std::string::npos);
    }

void SetLenientCS2WindowDetection(bool enabled)
{
    g_lenientCs2WindowDetection.store(enabled);
    std::cout << "[窗口检测] 宽容检测游戏窗口已切换为: "
              << (enabled ? "开启" : "关闭") << std::endl;
}

bool IsLenientCS2WindowDetection()
{
    return g_lenientCs2WindowDetection.load();
}

namespace {
    std::thread g_volThread;
    std::atomic<bool> g_volRunning{ false };
    std::atomic<float> g_targetFactor{ 0.5f };
    static float s_savedGsiOriginalVolume = -1.0f;
    static std::mutex s_gsiVolumeMutex;

    static float ClampVolumeFactor(float factor)
    {
        if (factor < 0.0f) return 0.0f;
        if (factor > 1.0f) return 1.0f;
        return factor;
    }

    static float ClampVolumePercent(float volumePercent)
    {
        if (volumePercent < 0.0f) return 0.0f;
        if (volumePercent > 100.0f) return 100.0f;
        return volumePercent;
    }

    static std::wstring ToLowerCopy(std::wstring text)
    {
        std::transform(text.begin(), text.end(), text.begin(),
            [](wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
        return text;
    }

    static std::wstring ExtractProcessFileName(const std::wstring& processName)
    {
        const size_t pos = processName.find_last_of(L"\\/");
        if (pos == std::wstring::npos) return processName;
        return processName.substr(pos + 1);
    }

    static bool MatchProcessName(const std::wstring& candidatePath, const std::wstring& expectedProcessName)
    {
        const std::wstring candidateName = ToLowerCopy(ExtractProcessFileName(candidatePath));
        const std::wstring expectedName = ToLowerCopy(ExtractProcessFileName(expectedProcessName));
        return !candidateName.empty() && !expectedName.empty() && candidateName == expectedName;
    }

    static void ApplyGsiVolumeReduction(float factor)
    {
        std::lock_guard<std::mutex> lock(s_gsiVolumeMutex);
        config::Settings& c = gsi::GetConfig();
        if (s_savedGsiOriginalVolume < 0.0f)
        {
            s_savedGsiOriginalVolume = c.volume;
            std::cout << "[音量] 已记录 StrikeSense 原始音量: "
                      << (int)(s_savedGsiOriginalVolume * 100) << "%" << std::endl;
        }

        c.volume = s_savedGsiOriginalVolume * ClampVolumeFactor(factor);
        std::cout << "[音量] StrikeSense 辅助音量已同步调整至: "
                  << (int)(c.volume * 100) << "%" << std::endl;
    }

    static void RestoreGsiVolume()
    {
        std::lock_guard<std::mutex> lock(s_gsiVolumeMutex);
        if (s_savedGsiOriginalVolume >= 0.0f)
        {
            config::Settings& c = gsi::GetConfig();
            c.volume = s_savedGsiOriginalVolume;
            std::cout << "[音量] StrikeSense 辅助音量已恢复至: "
                      << (int)(c.volume * 100) << "%" << std::endl;
            s_savedGsiOriginalVolume = -1.0f;
        }
    }



    // 找到 CS2 的音频会话并保存接口引用，返回原始音量
    static float GetCS2VolumeAndSession(ISimpleAudioVolume** outVol)
    {
        *outVol = nullptr;
        IMMDeviceEnumerator* pEnum = nullptr;
        HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
            __uuidof(IMMDeviceEnumerator), (void**)&pEnum);
        if (FAILED(hr)) return -1.0f;

        IMMDevice* pDevice = nullptr;
        hr = pEnum->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
        if (FAILED(hr)) { pEnum->Release(); return -1.0f; }

        IAudioSessionManager2* pMgr = nullptr;
        hr = pDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, (void**)&pMgr);
        pDevice->Release();
        if (FAILED(hr)) { pEnum->Release(); return -1.0f; }

        IAudioSessionEnumerator* pSessionEnum = nullptr;
        hr = pMgr->GetSessionEnumerator(&pSessionEnum);
        if (FAILED(hr)) { pMgr->Release(); pEnum->Release(); return -1.0f; }

        int count = 0;
        pSessionEnum->GetCount(&count);

        float result = -1.0f;
        for (int i = 0; i < count; ++i)
        {
            IAudioSessionControl* pCtrl = nullptr;
            if (FAILED(pSessionEnum->GetSession(i, &pCtrl))) continue;

            IAudioSessionControl2* pCtrl2 = nullptr;
            if (SUCCEEDED(pCtrl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&pCtrl2)))
            {
                DWORD pid = 0;
                pCtrl2->GetProcessId(&pid);
                HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
                if (hProc)
                {
                    wchar_t exePath[MAX_PATH] = {};
                    DWORD sz = MAX_PATH;
                    if (QueryFullProcessImageNameW(hProc, 0, exePath, &sz))
                    {
                        std::wstring path(exePath);
                        if (path.find(L"cs2.exe") != std::string::npos)
                        {
                            ISimpleAudioVolume* pVol = nullptr;
                            if (SUCCEEDED(pCtrl->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&pVol)))
                            {
                                float cur = 0;
                                pVol->GetMasterVolume(&cur);
                                result = cur;
                                *outVol = pVol; // 传出，调用方负责 Release
                            }
                        }
                    }
                    CloseHandle(hProc);
                }
                pCtrl2->Release();
            }
            pCtrl->Release();
        }

        pSessionEnum->Release();
        pMgr->Release();
        pEnum->Release();
        return result;
    }

    static bool ForEachProcessAudioSession(
        const std::wstring& processName,
        const std::function<void(ISimpleAudioVolume*, float)>& callback,
        int* matchedCount = nullptr)
    {
        if (matchedCount) *matchedCount = 0;

        IMMDeviceEnumerator* pEnum = nullptr;
        HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
            __uuidof(IMMDeviceEnumerator), (void**)&pEnum);
        if (FAILED(hr))
        {
            std::wcout << L"[音量] 创建设备枚举器失败，HRESULT=" << hr << std::endl;
            return false;
        }

        IMMDevice* pDevice = nullptr;
        hr = pEnum->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
        if (FAILED(hr))
        {
            std::wcout << L"[音量] 获取默认音频输出设备失败，HRESULT=" << hr << std::endl;
            pEnum->Release();
            return false;
        }

        IAudioSessionManager2* pMgr = nullptr;
        hr = pDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, (void**)&pMgr);
        pDevice->Release();
        if (FAILED(hr))
        {
            std::wcout << L"[音量] 激活音频会话管理器失败，HRESULT=" << hr << std::endl;
            pEnum->Release();
            return false;
        }

        IAudioSessionEnumerator* pSessionEnum = nullptr;
        hr = pMgr->GetSessionEnumerator(&pSessionEnum);
        if (FAILED(hr))
        {
            std::wcout << L"[音量] 获取音频会话枚举器失败，HRESULT=" << hr << std::endl;
            pMgr->Release();
            pEnum->Release();
            return false;
        }

        int count = 0;
        pSessionEnum->GetCount(&count);
        bool anyMatched = false;

        for (int i = 0; i < count; ++i)
        {
            IAudioSessionControl* pCtrl = nullptr;
            if (FAILED(pSessionEnum->GetSession(i, &pCtrl))) continue;

            IAudioSessionControl2* pCtrl2 = nullptr;
            if (SUCCEEDED(pCtrl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&pCtrl2)))
            {
                DWORD pid = 0;
                pCtrl2->GetProcessId(&pid);
                HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
                if (hProc)
                {
                    wchar_t exePath[MAX_PATH] = {};
                    DWORD size = MAX_PATH;
                    if (QueryFullProcessImageNameW(hProc, 0, exePath, &size) &&
                        MatchProcessName(exePath, processName))
                    {
                        ISimpleAudioVolume* pVol = nullptr;
                        if (SUCCEEDED(pCtrl->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&pVol)))
                        {
                            float currentVolume = 0.0f;
                            if (SUCCEEDED(pVol->GetMasterVolume(&currentVolume)))
                            {
                                callback(pVol, currentVolume);
                                anyMatched = true;
                                if (matchedCount) ++(*matchedCount);
                            }
                            pVol->Release();
                        }
                    }
                    CloseHandle(hProc);
                }
                pCtrl2->Release();
            }
            pCtrl->Release();
        }

        pSessionEnum->Release();
        pMgr->Release();
        pEnum->Release();
        return anyMatched;
    }
}

bool SetProcessVolumeByName(const std::wstring& processName, float volumePercent)
{
    if (processName.empty())
    {
        std::wcout << L"[音量] 进程名为空，无法设置音量" << std::endl;
        return false;
    }

    const float clampedPercent = ClampVolumePercent(volumePercent);
    const float normalizedVolume = clampedPercent / 100.0f;
    const HRESULT initHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool shouldUninitialize = SUCCEEDED(initHr);

    if (FAILED(initHr) && initHr != RPC_E_CHANGED_MODE)
    {
        std::wcout << L"[音量] COM 初始化失败，HRESULT=" << initHr << std::endl;
        return false;
    }

    int matchedCount = 0;
    int appliedCount = 0;
    const bool foundSession = ForEachProcessAudioSession(
        processName,
        [&](ISimpleAudioVolume* pVol, float currentVolume)
        {
            const HRESULT hr = pVol->SetMasterVolume(normalizedVolume, nullptr);
            if (SUCCEEDED(hr))
            {
                ++appliedCount;
                std::wcout << L"[音量] 已设置进程 " << processName
                           << L" 音量: " << (int)(currentVolume * 100.0f)
                           << L"% -> " << (int)clampedPercent << L"%" << std::endl;
            }
            else
            {
                std::wcout << L"[音量] 设置进程 " << processName
                           << L" 音量失败，HRESULT=" << hr << std::endl;
            }
        },
        &matchedCount);

    if (!foundSession)
    {
        std::wcout << L"[音量] 未找到进程音频会话: " << processName << std::endl;
    }
    else
    {
        std::wcout << L"[音量] 进程 " << processName
                   << L" 共匹配到 " << matchedCount << L" 个音频会话，成功设置 "
                   << appliedCount << L" 个" << std::endl;
    }

    if (shouldUninitialize) CoUninitialize();
    return appliedCount > 0;
}

bool SetProcessMuteByName(const std::wstring& processName, bool muted)
{
    if (processName.empty())
    {
        std::wcout << L"[音量] 进程名为空，无法设置静音状态" << std::endl;
        return false;
    }

    const HRESULT initHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool shouldUninitialize = SUCCEEDED(initHr);

    if (FAILED(initHr) && initHr != RPC_E_CHANGED_MODE)
    {
        std::wcout << L"[音量] COM 初始化失败，HRESULT=" << initHr << std::endl;
        return false;
    }

    int matchedCount = 0;
    int appliedCount = 0;
    const bool foundSession = ForEachProcessAudioSession(
        processName,
        [&](ISimpleAudioVolume* pVol, float)
        {
            const HRESULT hr = pVol->SetMute(muted, nullptr);
            if (SUCCEEDED(hr))
            {
                ++appliedCount;
                std::wcout << L"[音量] 已设置进程 " << processName
                           << (muted ? L" 为静音" : L" 取消静音") << std::endl;
            }
            else
            {
                std::wcout << L"[音量] 设置进程 " << processName
                           << L" 静音状态失败，HRESULT=" << hr << std::endl;
            }
        },
        &matchedCount);

    if (!foundSession)
    {
        std::wcout << L"[音量] 未找到进程音频会话: " << processName << std::endl;
    }
    else
    {
        std::wcout << L"[音量] 进程 " << processName
                   << L" 共匹配到 " << matchedCount << L" 个音频会话，成功设置 "
                   << appliedCount << L" 个静音状态" << std::endl;
    }

    if (shouldUninitialize) CoUninitialize();
    return appliedCount > 0;
}

void StopCS2VolumeControl()
{
    // 请求线程退出
    g_volRunning = false;

    // 如果线程可 join，则等待其结束以避免 std::terminate 在赋值/析构时被触发
    if (g_volThread.joinable())
    {
        try
        {
            g_volThread.join();
        }
        catch (...)
        {
            // 极端情况下尝试 detach 以避免 terminate，但应记录日志
            try { g_volThread.detach(); }
            catch (...) {}
        }
    }

    RestoreGsiVolume();
}

void StartCS2VolumeControl(float reduction)
{
    reduction = ClampVolumeFactor(reduction);
    g_targetFactor.store(reduction);

    // 如果已在运行，直接返回
    if (g_volRunning)
    {
        ApplyGsiVolumeReduction(reduction);
        return;
    }

    // 如果已有未 join 的线程残留，先请求其结束并 join，避免后续赋值触发 terminate
    if (g_volThread.joinable())
    {
        g_volRunning = false;
        try { g_volThread.join(); }
        catch (...) { try { g_volThread.detach(); } catch (...) {} }
    }

    g_volRunning = true;

    ApplyGsiVolumeReduction(reduction);

    // 2. 启动系统控制线程（线程内捕获所有异常，防止异常逃逸）
    g_volThread = std::thread([]() {
        try
        {
            CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

            ISimpleAudioVolume* pVol = nullptr;
            float savedVolume = -1.0f;
            bool isLoweredState = false;
            float lastAppliedFactor = -1.0f;

            while (g_volRunning)
            {
                bool cs2Active = IsCS2WindowActive();
                float currentFactor = g_targetFactor.load();

                if (cs2Active)
                {
                    if (!pVol)
                    {
                        savedVolume = GetCS2VolumeAndSession(&pVol);
                        if (pVol) {
                            std::cout << "[音量] 捕捉到 CS2 原始音量: " << (int)(savedVolume * 100) << "%" << std::endl;
                        }
                    }

                    if (pVol && savedVolume >= 0 &&
                        (!isLoweredState || currentFactor != lastAppliedFactor))
                    {
                        float target = savedVolume * currentFactor;
                        pVol->SetMasterVolume(target, nullptr);
                        isLoweredState = true;
                        lastAppliedFactor = currentFactor;
                        std::cout << "[音量] CS2 前台音量已调整至 " << (int)(target * 100) << "%" << std::endl;
                    }
                }
                else
                {
                    if (pVol && savedVolume >= 0 && isLoweredState)
                    {
                        pVol->SetMasterVolume(savedVolume, nullptr);
                        isLoweredState = false;
                        lastAppliedFactor = -1.0f;
                        std::cout << "[音量] CS2 后台 → 临时恢复至 " << (int)(savedVolume * 100) << "%" << std::endl;
                    }
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }

            if (pVol)
            {
                if (savedVolume >= 0 && isLoweredState)
                {
                    pVol->SetMasterVolume(savedVolume, nullptr);
                    std::cout << "[音量] 线程退出 → 已恢复 CS2 原始音量: " << (int)(savedVolume * 100) << "%" << std::endl;
                }
                pVol->Release();
                pVol = nullptr;
            }
            CoUninitialize();
        }
        catch (const std::exception& e)
        {
            std::cerr << "[音量线程] 捕获异常: " << e.what() << std::endl;
        }
        catch (...)
        {
            std::cerr << "[音量线程] 捕获未知异常" << std::endl;
        }
        });
}

void SetCS2VolumeReduction(float factor)
{
    factor = ClampVolumeFactor(factor);
    g_targetFactor.store(factor);
    if (g_volRunning)
        ApplyGsiVolumeReduction(factor);
}

float GetCS2VolumeReduction()
{
    return g_targetFactor.load();
}

bool IsCS2VolumeActive()
{
    return g_volRunning;
}
