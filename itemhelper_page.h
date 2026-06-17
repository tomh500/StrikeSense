#pragma once
#include <vector>        // 解决 std::vector 报错
#include <filesystem>    // 解决 std::filesystem 报错

extern bool g_isBindingItemHelperHotkey;

extern int g_itemHelperHotkeyMod;
extern int g_itemHelperHotkeyVk;

enum ItemHelperState
{
    IH_IDLE = 0,
    IH_BROWSE,
    IH_PREVIEW
};

struct ItemHelperUI
{
    bool showOverlay = false;
    ItemHelperState state = IH_IDLE;

    // 严格使用标准库全称，防止外部包含时产生命名空间污染
    std::vector<std::filesystem::path> files; 
    int selectedIndex = 0;

    std::filesystem::path currentImage;

    bool needRefresh = true;
};

extern ItemHelperUI g_itemUI;