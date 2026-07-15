#pragma once
#include <windows.h>

extern bool g_isBindingItemHelperHotkey;

extern int g_itemHelperHotkeyMod;
extern int g_itemHelperHotkeyVk;
extern bool g_itemHelperEnabled;
extern bool g_isBindingItemHelperHotkey;
extern int g_itemHelperHotkeyMod;
extern int g_itemHelperHotkeyVk;

extern float g_itemHelperX;
extern float g_itemHelperY;
extern float g_itemHelperOpacity;
extern bool  g_itemHelperAutoHide;
extern int   g_itemHelperKeyPrev;
extern int   g_itemHelperKeyNext;
extern int   g_itemHelperKeySelect;
extern bool  g_isBindingItemKeyPrev;
extern bool  g_isBindingItemKeyNext;
extern bool  g_isBindingItemKeySelect;
extern float g_itemHelperX;
extern float g_itemHelperY;
extern float g_itemHelperOpacity;
extern bool  g_itemHelperAutoHide;
extern int   g_itemHelperKeyPrev;
extern int   g_itemHelperKeyNext;
extern int   g_itemHelperKeySelect;
extern float g_itemHelperImgOpacity;

namespace itemhelper_overlay
{
    // 初始化遮罩窗口
    void Initialize(HINSTANCE hInst);

    // 切换道具助手遮罩的显示状态（扫描文件、挂载/卸载钩子、显示/隐藏窗口）
    void Toggle(HINSTANCE hInst);
    bool IsOverlayVisible();

    // 执行底层的重新绘制逻辑（使用 UpdateLayeredWindow 实现完美的每像素 Alpha 半透明）
    void Redraw();

    // 加载当前选中文件的 GDI+ 图片对象
    void LoadPreviewImage();

    // 释放预览图片的内存
    void ReleasePreviewImage();

    // 销毁窗口与释放资源
    void Shutdown();
}
