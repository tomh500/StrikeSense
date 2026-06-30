#pragma once
#include <windows.h>
#include <string>

bool SetProcessVolumeByName(const std::wstring& processName, float volumePercent);
void StartCS2VolumeControl(float reduction);
void StopCS2VolumeControl();
void SetCS2VolumeReduction(float factor);
float GetCS2VolumeReduction();
bool IsCS2VolumeActive();
