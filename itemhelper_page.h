#pragma once
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

    std::vector<fs::path> files;
    int selectedIndex = 0;

    fs::path currentImage;

    bool needRefresh = true;
};

extern ItemHelperUI g_itemUI;