#pragma once
#include <windows.h>
#include <string>

void StartCS2VolumeControl(float reduction);
void StopCS2VolumeControl();
void SetCS2VolumeReduction(float factor);
float GetCS2VolumeReduction();
bool IsCS2VolumeActive();