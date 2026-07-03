#pragma once

#include <windows.h>

// ============================================================
// SDL2 音效配置面板
//
// 在主窗口中嵌入一个 SDL2 渲染区域，显示美观的音效配置界面
// ============================================================

namespace sound_ui {

// 初始化 SDL2 渲染窗口（嵌入到 Win32 主窗口）
// hParent: 主窗口句柄
bool Initialize(HWND hParent, HINSTANCE hInst);

// 渲染音效配置面板
void Render();

// 处理鼠标点击事件（检测按钮区域）
void HandleClick(int mouseX, int mouseY);

// 处理窗口大小变化
void Resize(int width, int height);

// 关闭并清理
void Shutdown();

// 是否需要重绘
bool NeedsRedraw();

} // namespace sound_ui