#include "volume_mixer.h"
#include <iostream>
#include <thread>
#include <atomic>
#include <audiopolicy.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <map>

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "avrt.lib")

namespace {
    std::thread g_volThread;
    std::atomic<bool> g_volRunning{ false };
    float g_targetFactor = 0.5f; // 1.0=正常, 0.5=降低到50%

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

    static float GetCS2SessionVolume(IMMDeviceEnumerator* pEnum, IAudioSessionManager2*& outMgr, IAudioSessionControl*& outCtrl)
    {
        outMgr = nullptr;
        outCtrl = nullptr;

        IMMDevice* pDevice = nullptr;
        HRESULT hr = pEnum->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
        if (FAILED(hr)) return -1.0f;

        IAudioSessionManager2* pMgr = nullptr;
        hr = pDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, (void**)&pMgr);
        pDevice->Release();
        if (FAILED(hr)) return -1.0f;

        IAudioSessionEnumerator* pSessionEnum = nullptr;
        hr = pMgr->GetSessionEnumerator(&pSessionEnum);
        if (FAILED(hr)) { pMgr->Release(); return -1.0f; }

        int count = 0;
        pSessionEnum->GetCount(&count);

        float result = -1.0f;
        for (int i = 0; i < count; ++i)
        {
            IAudioSessionControl* pCtrl = nullptr;
            hr = pSessionEnum->GetSession(i, &pCtrl);
            if (FAILED(hr)) continue;

            IAudioSessionControl2* pCtrl2 = nullptr;
            hr = pCtrl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&pCtrl2);
            if (SUCCEEDED(hr))
            {
                DWORD pid = 0;
                pCtrl2->GetProcessId(&pid);

                // 检查是否是 cs2.exe
                HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
                if (hProc)
                {
                    wchar_t exePath[MAX_PATH] = {};
                    DWORD sz = MAX_PATH;
                    if (QueryFullProcessImageNameW(hProc, 0, exePath, &sz))
                    {
                        std::wstring path(exePath);
                        if (path.find(L"cs2.exe") != std::string::npos ||
                            path.find(L"Counter-Strike 2") != std::string::npos)
                        {
                            outMgr = pMgr;
                            outMgr->AddRef();
                            outCtrl = pCtrl;
                            outCtrl->AddRef();

                            ISimpleAudioVolume* pVol = nullptr;
                            hr = pCtrl->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&pVol);
                            if (SUCCEEDED(hr))
                            {
                                float cur = 0;
                                pVol->GetMasterVolume(&cur);
                                result = cur;
                                pVol->Release();
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

        // 如果找到了 CS2 会话，暂时保留枚举器引用在外面用
        return result;
    }

    static void SetCS2Volume(float vol)
    {
        IMMDeviceEnumerator* pEnum = nullptr;
        HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
            __uuidof(IMMDeviceEnumerator), (void**)&pEnum);
        if (FAILED(hr)) return;

        IAudioSessionManager2* pMgr = nullptr;
        IAudioSessionControl* pCtrl = nullptr;
        float cur = GetCS2SessionVolume(pEnum, pMgr, pCtrl);

        if (pCtrl)
        {
            ISimpleAudioVolume* pVol = nullptr;
            hr = pCtrl->QueryInterface(__uuidof(ISimpleAudioVolume), (void**)&pVol);
            if (SUCCEEDED(hr))
            {
                pVol->SetMasterVolume(vol, nullptr);
                pVol->Release();
            }
        }

        if (pCtrl) pCtrl->Release();
        if (pMgr) pMgr->Release();
        pEnum->Release();
    }
}

void StartCS2VolumeControl(float reduction)
{
    if (g_volRunning) return;
    g_targetFactor = reduction;
    g_volRunning = true;

    g_volThread = std::thread([]() {
        CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

        float savedVolume = -1.0f; // 保存 CS2 原来的音量

        IMMDeviceEnumerator* pEnum = nullptr;
        CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
            __uuidof(IMMDeviceEnumerator), (void**)&pEnum);

        while (g_volRunning)
        {
            bool cs2Active = IsCS2WindowActive();

            if (cs2Active)
            {
                // 保存原始音量（只保存一次）
                if (savedVolume < 0)
                {
                    IAudioSessionManager2* pMgr = nullptr;
                    IAudioSessionControl* pCtrl = nullptr;
                    float cur = GetCS2SessionVolume(pEnum, pMgr, pCtrl);
                    if (cur >= 0)
                    {
                        savedVolume = cur;
                        std::cout << "[音量] CS2 原始音量: " << (int)(savedVolume * 100) << "%" << std::endl;
                    }
                    if (pCtrl) pCtrl->Release();
                    if (pMgr) pMgr->Release();
                }

                // 应用降低
                float target = savedVolume * g_targetFactor;
                SetCS2Volume(target);
                std::cout << "[音量] CS2 前台 → 已降低到 " << (int)(target * 100) << "%" << std::endl;
            }
            else
            {
                // CS2 不在前台 → 恢复原始音量
                if (savedVolume >= 0)
                {
                    SetCS2Volume(savedVolume);
                    std::cout << "[音量] CS2 后台 → 已恢复至 " << (int)(savedVolume * 100) << "%" << std::endl;
                    savedVolume = -1.0f; // 下次重新获取
                }
            }

            // 每 500ms 检测一次
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        // 退出时恢复音量
        if (savedVolume >= 0)
        {
            SetCS2Volume(savedVolume);
            std::cout << "[音量] 停止控制 → 已恢复原始音量" << std::endl;
        }

        if (pEnum) pEnum->Release();
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