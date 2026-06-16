#include "volume_mixer.h"
#include <iostream>
#include <thread>
#include <atomic>
#include <audiopolicy.h>
#include <mmdeviceapi.h>

#pragma comment(lib, "ole32.lib")

namespace {
    std::thread g_volThread;
    std::atomic<bool> g_volRunning{ false };
    float g_targetFactor = 0.5f;

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

    g_volThread = std::thread([]() {
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

        ISimpleAudioVolume* pVol = nullptr;
        float savedVolume = -1.0f;

        while (g_volRunning)
        {
            bool cs2Active = IsCS2WindowActive();

            if (cs2Active)
            {
                // 如果没有找到 CS2 会话，尝试查找
                if (!pVol)
                {
                    savedVolume = GetCS2VolumeAndSession(&pVol);
                    if (pVol)
                        std::cout << "[音量] CS2 原始音量: " << (int)(savedVolume * 100) << "%" << std::endl;
                }

                // 应用降低（只有成功找到会话后才操作）
                if (pVol && savedVolume >= 0)
                {
                    float target = savedVolume * g_targetFactor;
                    pVol->SetMasterVolume(target, nullptr);
                    // 只打印一次，不刷屏
                    static bool printed = false;
                    if (!printed)
                    {
                        std::cout << "[音量] CS2 前台 → 降低至 " << (int)(target * 100) << "%" << std::endl;
                        printed = true;
                    }
                }
            }
            else
            {
                // CS2 不在前台 → 恢复原始音量
                if (pVol && savedVolume >= 0)
                {
                    pVol->SetMasterVolume(savedVolume, nullptr);
                    std::cout << "[音量] CS2 后台 → 恢复至 " << (int)(savedVolume * 100) << "%" << std::endl;
                    pVol->Release();
                    pVol = nullptr;
                    savedVolume = -1.0f;
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        // 退出时恢复
        if (pVol && savedVolume >= 0)
        {
            pVol->SetMasterVolume(savedVolume, nullptr);
            std::cout << "[音量] 停止控制 → 已恢复原始音量" << std::endl;
            pVol->Release();
        }

        CoUninitialize();
    });
    g_volThread.detach();
}

void StopCS2VolumeControl()
{
    g_volRunning = false;
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