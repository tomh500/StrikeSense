#pragma once
#include <windows.h>
#include <string>

bool SetProcessVolumeByName(const std::wstring& processName, float volumePercent);
bool SetProcessMuteByName(const std::wstring& processName, bool muted);
bool IsCS2WindowActive();
void SetLenientCS2WindowDetection(bool enabled);
bool IsLenientCS2WindowDetection();
void StartCS2VolumeControl(float reduction);
void StopCS2VolumeControl();
void SetCS2VolumeReduction(float factor);
float GetCS2VolumeReduction();
bool IsCS2VolumeActive();
