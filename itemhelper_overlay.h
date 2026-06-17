#pragma once
#include <windows.h>

namespace itemhelper_overlay
{
    // 初始化遮罩窗口
    void Initialize(HINSTANCE hInst);

    // 切换道具助手遮罩的显示状态（扫描文件、挂载/卸载钩子、显示/隐藏窗口）
    void Toggle(HINSTANCE hInst);

    // 执行底层的重新绘制逻辑（使用 UpdateLayeredWindow 实现完美的每像素 Alpha 半透明）
    void Redraw();

    // 加载当前选中文件的 GDI+ 图片对象
    void LoadPreviewImage();

    // 释放预览图片的内存
    void ReleasePreviewImage();

    // 销毁窗口与释放资源
    void Shutdown();
}