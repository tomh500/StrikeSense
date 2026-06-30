#include "volume_mixer.h"
#include <iostream>
#include <thread>
#include <atomic>
#include <mutex>
#include <audiopolicy.h>
#include <mmdeviceapi.h>
#include "gsi_server.h"
#include "config.h"

#pragma comment(lib, "ole32.lib")

bool IsCS2WindowActive()
    {
        HWND fg = GetForegroundWindow();
        if (!fg) return false;
        wchar_t title[256];
        GetWindowTextW(fg, title, 256);
        std::wstring wt(title);
        return (wt.find(L"Counter-Strike 2") != std::string::npos ||
                wt.find(L"反恐精英：全球攻势") != std::string::npos);
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
