#pragma once

#include <windows.h>

namespace flashoverlay
{
    void Initialize(HINSTANCE hInst);

    void Show();
    void Hide();
    void Shutdown();
}