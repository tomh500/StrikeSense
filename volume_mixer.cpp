#include "volume_mixer.h"
#include <iostream>
#include <thread>
#include <atomic>
#include <audiopolicy.h>
#include <mmdeviceapi.h>
#include "gsi_server.h"
#include "config.h"

#pragma comment(lib, "ole32.lib")

namespace {
    std::thread g_volThread;
    std::atomic<bool> g_volRunning{ false };
    float g_targetFactor = 0.5f;
    static float s_savedGsiOriginalVolume = -1.0f;

    static bool IsCS2WindowActive()
    {
        HWND fg = GetForegroundWindow();
        if (!fg) return false;
        wchar_t title[256];
        GetWindowTextW(fg, title, 256);
        std::wstring wt(title);
        return (wt.find(L"Counter-Strike 2") != std::string::npos ||
                wt.find(L"反恐精英：全球攻势") != std::string::npos);
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

void StartCS2VolumeControl(float reduction)
{
    if (g_volRunning) return;
    g_targetFactor = reduction;
    g_volRunning = true;

    // 1. 同步降低 GSI 内存音量
    config::Settings& c = gsi::GetConfig();
    if (s_savedGsiOriginalVolume < 0.0f) 
    {
        s_savedGsiOriginalVolume = c.volume; 
        c.volume = s_savedGsiOriginalVolume * reduction; 
        std::cout << "[音量] GSI 辅助音量已同步降低至: " << (int)(c.volume * 100) << "%" << std::endl;
    }

    // 2. 启动系统控制线程
g_volThread = std::thread([]() {
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

        ISimpleAudioVolume* pVol = nullptr;
        float savedVolume = -1.0f;
        
        // 核心优化：记录当前是否处于“已降低”状态，避免每 200ms 重复发送系统指令
        bool isLoweredState = false; 

        while (g_volRunning)
        {
            bool cs2Active = IsCS2WindowActive();

            if (cs2Active)
            {
                // 如果没有找到 CS2 会话，尝试查找
                if (!pVol)
                {
                    savedVolume = GetCS2VolumeAndSession(&pVol);
                    if (pVol) {
                        std::cout << "[音量] 捕捉到 CS2 原始音量: " << (int)(savedVolume * 100) << "%" << std::endl;
                    }
                }

                // 只有当获取到了 pVol，并且当前不是“已降低”状态时，才去设置音量
                if (pVol && savedVolume >= 0 && !isLoweredState)
                {
                    float target = savedVolume * g_targetFactor;
                    pVol->SetMasterVolume(target, nullptr);
                    isLoweredState = true; // 锁定状态，下次循环不再重复设置
                    std::cout << "[音量] CS2 前台 → 降低至 " << (int)(target * 100) << "%" << std::endl;
                }
            }
            else
            {
                // CS2 不在前台 → 只有当前处于“已降低”状态时，才需要恢复
                if (pVol && savedVolume >= 0 && isLoweredState)
                {
                    pVol->SetMasterVolume(savedVolume, nullptr);
                    isLoweredState = false; // 解除锁定
                    std::cout << "[音量] CS2 后台 → 临时恢复至 " << (int)(savedVolume * 100) << "%" << std::endl;
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(200)); 
        }

        // 彻底关闭功能时，安全恢复
        if (pVol)
        {
            // 只有当功能退出时，CS2 还处于被降低的状态，才需要再恢复一次
            if (savedVolume >= 0 && isLoweredState)
            {
                pVol->SetMasterVolume(savedVolume, nullptr);
                std::cout << "[音量] 线程退出 → 已恢复 CS2 原始音量: " << (int)(savedVolume * 100) << "%" << std::endl;
            }
            pVol->Release();
            pVol = nullptr;
        }
        CoUninitialize();
    });
}

void StopCS2VolumeControl()
{
    if (!g_volRunning) return;
    g_volRunning = false; // 仅仅下发退出指令，让异步线程自己去恢复系统音频，防止竞争

    // 完美同步：在此处恢复 GSI 工具自身的音量
    if (s_savedGsiOriginalVolume >= 0.0f)
    {
        config::Settings& c = gsi::GetConfig();
        c.volume = s_savedGsiOriginalVolume; 
        std::cout << "[音量] GSI 辅助音量已恢复至原始大小: " << (int)(c.volume * 100) << "%" << std::endl;
        s_savedGsiOriginalVolume = -1.0f; 
    }
}

void SetCS2VolumeReduction(float factor)
{
    g_targetFactor = factor;
}

float GetCS2VolumeReduction()
{
    return g_targetFactor;
}

bool IsCS2VolumeActive()
{
    return g_volRunning;
}