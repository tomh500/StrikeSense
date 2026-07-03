#include "pages.h"
#include "StrikeSense.h"
#include "i18n.h"
#include "itemhelper_page.h"
#include "Hotkey.h"
#include <shlobj.h> 
#include <filesystem>
#include <string>
#include "itemhelper_overlay.h"

namespace fs = std::filesystem;

extern HINSTANCE hInst;
namespace itemhelper_overlay { void Toggle(HINSTANCE hInst); }

extern float g_itemHelperImgOpacity;

// 交互区域矩形定义
Gdiplus::RectF g_itemHelperToggleRect;
Gdiplus::RectF g_itemHelperHotkeyRect;
Gdiplus::RectF g_itemHelperDirBtnRect;
Gdiplus::RectF g_itemAutoHideToggleRect;
Gdiplus::RectF g_itemKeyPrevRect;
Gdiplus::RectF g_itemKeyNextRect;
Gdiplus::RectF g_itemKeySelRect;
Gdiplus::RectF g_itemHelperResetBtnRect; // 【新增】恢复默认按钮区域
Gdiplus::RectF g_itemHelperMakerBtnRect; // 【新增】前往制作网页按钮区域

static fs::path GetItemHelperDirectory() {
    wchar_t userProfile[MAX_PATH] = {};
    if (GetEnvironmentVariableW(L"USERPROFILE", userProfile, MAX_PATH) > 0) {
        return fs::path(userProfile) / L"StrikeSense" / L"itemhelper";
    }
    return fs::current_path() / L"items";
}

// VK虚拟键码转换为人类直观可读的常用按键名称
static std::wstring GetKeyName(int vk) {
    if (vk <= 0) return L"未绑定";
    if (vk == VK_UP) return L"Up";
    if (vk == VK_DOWN) return L"Down";
    if (vk == VK_LEFT) return L"Left";
    if (vk == VK_RIGHT) return L"Right";
    if (vk == VK_RETURN) return L"Enter";
    if (vk == VK_SPACE) return L"Space";
    if (vk == VK_ESCAPE) return L"ESC";

    UINT scanCode = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC);
    wchar_t name[64] = { 0 };
    if (GetKeyNameTextW(scanCode << 16, name, 64) > 0) {
        return name;
    }
    return L"Key " + std::to_wstring(vk);
}

void PaintItemHelperPage(Gdiplus::Graphics& g, int cx, int cw, int H, HWND) {
    using namespace Gdiplus;
    ui::DrawHeader(g, cx, cw, i18n::T("ITEM_TITLE"));

    Font rF(L"Microsoft YaHei", 11), sF(L"Microsoft YaHei", 9);
    SolidBrush tc(Color(255, 30, 60, 100)), tb(Color(255, 20, 80, 140)), td(Color(255, 100, 130, 160));
    SolidBrush bb(Color(255, 200, 230, 250)); Pen bp(Color(255, 150, 190, 220)), kp(Color(255, 30, 60, 100));

    // 1. 总开关 (Y=60)
    g.DrawString(i18n::T("ITEM_ENABLE"), -1, &rF, PointF((float)(cx + 10), 60.f), &tc);
    g_itemHelperToggleRect = RectF((REAL)(cx + 130), 56.f, 50.f, 24.f);
    ui::DrawToggle(g, cx + 130, 56, g_itemHelperEnabled);

    if (!g_itemHelperEnabled) return;

    // 2. 主开关快捷键 (Y=92)
    std::wstring keyName = Hotkey::HotkeyToString(g_itemHelperHotkeyMod, g_itemHelperHotkeyVk);
    wchar_t hs[128]; swprintf_s(hs, i18n::T("ITEM_HOTKEY"), keyName.c_str());
    g.DrawString(hs, -1, &rF, PointF((float)(cx + 10), 92.f), &tc);
    g_itemHelperHotkeyRect = RectF((REAL)(cx + 10), 118.f, 200.f, 20.f);
    g.DrawRectangle(&kp, g_itemHelperHotkeyRect.X, g_itemHelperHotkeyRect.Y, g_itemHelperHotkeyRect.Width, g_itemHelperHotkeyRect.Height);
    const wchar_t* hintStr = g_isBindingItemHelperHotkey ? i18n::T("ITEM_BINDING") : i18n::T("ITEM_CLICK_MOD");
    g.DrawString(hintStr, -1, &sF, PointF((float)(cx + 14), 118.f), &tc);

    // 3. 桌面自动销毁开关 - 调整为垂直排列，位于X坐标滑块上方 (Y=150)
    g.DrawString(i18n::T("ITEM_AUTOHIDE"), -1, &rF, PointF((float)(cx + 10), 150.f), &tc);
    g_itemAutoHideToggleRect = RectF((REAL)(cx + 130), 146.f, 50.f, 24.f);
    ui::DrawToggle(g, cx + 130, 146, g_itemHelperAutoHide);

    // 带进化分支风格小圆圈滑块渲染 Lambda
    auto DrawSliderWithKnob = [&](int x, int y, int w, float val) {
        ui::DrawSlider(g, x, y, w, val);
        SolidBrush knB(Color(255, 60, 160, 230)); // 科技蓝圆点
        float kx = x + (w * val) - 7.f;
        g.FillEllipse(&knB, kx, y - 5.f, 14.f, 14.f);
        };

    // 4. 四个属性调整滑块
    int ySlider = 190; int slW = cw - 150;

    g.DrawString(i18n::T("ITEM_POS_X"), -1, &rF, PointF((float)(cx + 10), (float)ySlider), &tc);
    DrawSliderWithKnob(cx + 110, ySlider + 8, slW, g_itemHelperX);

    ySlider += 38;
    g.DrawString(i18n::T("ITEM_POS_Y"), -1, &rF, PointF((float)(cx + 10), (float)ySlider), &tc);
    DrawSliderWithKnob(cx + 110, ySlider + 8, slW, g_itemHelperY);

    ySlider += 38;
    g.DrawString(i18n::T("ITEM_OPACITY"), -1, &rF, PointF((float)(cx + 10), (float)ySlider), &tc);
    DrawSliderWithKnob(cx + 110, ySlider + 8, slW, g_itemHelperOpacity);

    ySlider += 38;
    g.DrawString(i18n::T("ITEM_PIC_OPACITY"), -1, &rF, PointF((float)(cx + 10), (float)ySlider), &tc);
    DrawSliderWithKnob(cx + 110, ySlider + 8, slW, g_itemHelperImgOpacity);

    // 5. 内部导航/确认按键设置
    int yKeys = ySlider + 45;
    auto DrawSubKey = [&](int y, int& vk, bool isBinding, const char* langKey, Gdiplus::RectF& rect) {
        std::wstring name = GetKeyName(vk);
        wchar_t buf[128]; swprintf_s(buf, i18n::T(langKey), name.c_str());
        g.DrawString(buf, -1, &sF, PointF((float)(cx + 10), (float)y), &tc);
        rect = RectF((REAL)(cx + 180), (REAL)y, 120.f, 20.f);
        g.DrawRectangle(&kp, rect.X, rect.Y, rect.Width, rect.Height);
        g.DrawString(isBinding ? i18n::T("ITEM_BINDING") : i18n::T("ITEM_CLICK_MOD"), -1, &sF, PointF((float)(cx + 184), (float)y), &tc);
        };

    DrawSubKey(yKeys, g_itemHelperKeyPrev, g_isBindingItemKeyPrev, "ITEM_KEY_PREV", g_itemKeyPrevRect);
    DrawSubKey(yKeys + 28, g_itemHelperKeyNext, g_isBindingItemKeyNext, "ITEM_KEY_NEXT", g_itemKeyNextRect);
    DrawSubKey(yKeys + 56, g_itemHelperKeySelect, g_isBindingItemKeySelect, "ITEM_KEY_SEL", g_itemKeySelRect);

    // 6. 使用说明与底部控制按钮 (整体向上微调，空出最底部给多按钮排列)
    float usageY = H - 165.f;
    g.DrawString(i18n::T("ITEM_USAGE_TITLE"), -1, &rF, PointF((float)(cx + 10), usageY), &tc);
    g.DrawString(i18n::T("ITEM_USAGE_LINE1"), -1, &sF, PointF((float)(cx + 15), usageY + 22.f), &td);
    g.DrawString(i18n::T("ITEM_USAGE_LINE2"), -1, &sF, PointF((float)(cx + 15), usageY + 40.f), &td);
    g.DrawString(i18n::T("ITEM_USAGE_LINE3"), -1, &sF, PointF((float)(cx + 15), usageY + 58.f), &td);
    g.DrawString(i18n::T("ITEM_USAGE_LINE4"), -1, &sF, PointF((float)(cx + 15), usageY + 76.f), &td);

    // 统一配置下方三个按钮的尺寸与绘制逻辑
    const int BW = 125, BH = 26;
    int btnY = H - 45; // 完全放置在说明正下方

    auto DrawCustomButton = [&](Gdiplus::RectF& rect, int bx, const wchar_t* label) {
        rect = RectF((REAL)bx, (REAL)btnY, (REAL)BW, (REAL)BH);
        GraphicsPath path;
        path.AddArc((REAL)bx, (REAL)btnY, 12.f, 12.f, 180.f, 90.f);
        path.AddArc((REAL)(bx + BW - 12), (REAL)btnY, 12.f, 12.f, 270.f, 90.f);
        path.AddArc((REAL)(bx + BW - 12), (REAL)(btnY + BH - 12), 12.f, 12.f, 0.f, 90.f);
        path.AddArc((REAL)bx, (REAL)(btnY + BH - 12), 12.f, 12.f, 90.f, 90.f);
        path.CloseFigure();
        g.FillPath(&bb, &path); g.DrawPath(&bp, &path);
        g.DrawString(label, -1, &sF, PointF((REAL)(bx + 14), (REAL)(btnY + 5)), &tb);
        };

    // 依次横向渲染三个功能按钮
    DrawCustomButton(g_itemHelperDirBtnRect, cx + 10, i18n::T("ITEM_BTN_OPEN_DIR"));
    DrawCustomButton(g_itemHelperResetBtnRect, cx + 145, i18n::T("ITEM_BTN_RECOVERY"));
    DrawCustomButton(g_itemHelperMakerBtnRect, cx + 280,i18n::T("ITEM_BTN_MAKE"));
}

void CheckItemHelperClick(HWND hw, int mx, int my) {
    auto PtInRect = [](int x, int y, const Gdiplus::RectF& r) { return x >= r.X && x <= r.X + r.Width && y >= r.Y && y <= r.Y + r.Height; };

    // 1. 顶层总开关响应
    if (ui::CheckToggleClick(mx, my, (int)g_itemHelperToggleRect.X, (int)g_itemHelperToggleRect.Y)) {
        g_itemHelperEnabled = !g_itemHelperEnabled;
        if (!g_itemHelperEnabled && g_itemUI.showOverlay) itemhelper_overlay::Toggle(hInst);
        SaveEvolutionParams(); Hotkey::UpdateItemHelperHotkey(hw);
        InvalidateRect(hw, nullptr, FALSE); return;
    }

    if (!g_itemHelperEnabled) return;

    // 2. 底部按钮1：打开道具目录
    if (PtInRect(mx, my, g_itemHelperDirBtnRect)) {
        fs::path targetPath = GetItemHelperDirectory();
        std::error_code ec; fs::create_directories(targetPath, ec);
        ShellExecuteW(nullptr, L"open", targetPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        return;
    }

    // 3. 底部按钮2：恢复默认设置 (带 MsgBox 询问)
    if (PtInRect(mx, my, g_itemHelperResetBtnRect)) {
        int result = MessageBoxW(hw,i18n::T("ITEM_MSG_REC"), L"提示", MB_YESNO | MB_ICONQUESTION);
        if (result == IDYES) {
            g_itemHelperX = 0.95f;       // X位置 (0.0~1.0，默认靠右)
           g_itemHelperY = 0.5f;        // Y位置 (0.0~1.0，默认居中)
            g_itemHelperOpacity = 1.0f;
            g_itemHelperImgOpacity = 1.0f;
            g_itemHelperAutoHide = false;
            g_itemHelperKeyPrev = VK_UP;    
            g_itemHelperKeyNext = VK_DOWN;    
            g_itemHelperKeySelect = VK_RETURN;


            SaveEvolutionParams();
            Hotkey::UpdateItemHelperHotkey(hw);
            InvalidateRect(hw, nullptr, FALSE);
        }
        return;
    }

    // 4. 底部按钮3：前往制作图片 (外部浏览器跳转)
    if (PtInRect(mx, my, g_itemHelperMakerBtnRect)) {
        ShellExecuteW(nullptr, L"open", L"https://luotiany1.top/StrikeSense/itemmaker", nullptr, nullptr, SW_SHOWNORMAL);
        return;
    }

    // 5. 自动隐藏开关
    if (ui::CheckToggleClick(mx, my, (int)g_itemAutoHideToggleRect.X, (int)g_itemAutoHideToggleRect.Y)) {
        g_itemHelperAutoHide = !g_itemHelperAutoHide;
        SaveEvolutionParams(); InvalidateRect(hw, nullptr, FALSE); return;
    }

    // 6. 滑块事件判定 (重新对齐绘制坐标)
    int cx = 200, cw = 0;
    RECT rc; GetClientRect(hw, &rc); cw = rc.right - rc.left - cx - 12; int slW = cw - 150; float val;

    if (ui::CheckSliderClick(mx, my, cx + 110, 198, slW, val)) { g_itemHelperX = val; SaveEvolutionParams(); InvalidateRect(hw, nullptr, FALSE); return; }
    if (ui::CheckSliderClick(mx, my, cx + 110, 236, slW, val)) { g_itemHelperY = val; SaveEvolutionParams(); InvalidateRect(hw, nullptr, FALSE); return; }
    if (ui::CheckSliderClick(mx, my, cx + 110, 274, slW, val)) { g_itemHelperOpacity = val; SaveEvolutionParams(); InvalidateRect(hw, nullptr, FALSE); return; }
    if (ui::CheckSliderClick(mx, my, cx + 110, 312, slW, val)) { g_itemHelperImgOpacity = val; SaveEvolutionParams(); InvalidateRect(hw, nullptr, FALSE); return; }

    // 7. 热键录入绑定状态判定
    if (PtInRect(mx, my, g_itemHelperHotkeyRect)) { g_isBindingItemHelperHotkey = true; InvalidateRect(hw, nullptr, FALSE); }
    else if (PtInRect(mx, my, g_itemKeyPrevRect)) { g_isBindingItemKeyPrev = true; InvalidateRect(hw, nullptr, FALSE); }
    else if (PtInRect(mx, my, g_itemKeyNextRect)) { g_isBindingItemKeyNext = true; InvalidateRect(hw, nullptr, FALSE); }
    else if (PtInRect(mx, my, g_itemKeySelRect)) { g_isBindingItemKeySelect = true; InvalidateRect(hw, nullptr, FALSE); }
    else {
        g_isBindingItemHelperHotkey = g_isBindingItemKeyPrev = g_isBindingItemKeyNext = g_isBindingItemKeySelect = false;
        InvalidateRect(hw, nullptr, FALSE);
    }
}