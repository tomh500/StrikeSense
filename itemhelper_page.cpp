#include "pages.h"
#include "StrikeSense.h"
#include "i18n.h"
#include "itemhelper_page.h"
#include "Hotkey.h"
#include <shlobj.h> 
#include <filesystem> // 💎 替换为 std::filesystem 风格

namespace fs = std::filesystem;

extern HINSTANCE hInst; 
namespace itemhelper_overlay {
    void Toggle(HINSTANCE hInst); 
}

Gdiplus::RectF g_itemHelperToggleRect;
Gdiplus::RectF g_itemHelperHotkeyRect;
Gdiplus::RectF g_itemHelperDirBtnRect;

extern bool g_itemHelperEnabled;
extern bool g_isBindingItemHelperHotkey;
extern int g_itemHelperHotkeyMod;
extern int g_itemHelperHotkeyVk;

// ====== 💎 封装获取现代标准 %UserProfile%/StrikeSense/itemhelper 目录函数 💎 ======
static fs::path GetItemHelperDirectory()
{
    wchar_t userProfile[MAX_PATH] = {};
    if (GetEnvironmentVariableW(L"USERPROFILE", userProfile, MAX_PATH) > 0) {
        return fs::path(userProfile) / L"StrikeSense" / L"itemhelper";
    }
    // 兜底策略（如果环境变量获取失败，回退到当前目录）
    return fs::current_path() / L"items";
}

void PaintItemHelperPage(Gdiplus::Graphics& g, int cx, int cw, int H, HWND)
{
    using namespace Gdiplus;

    ui::DrawHeader(g, cx, cw, i18n::T("ITEM_TITLE"));

    Font rF(L"Microsoft YaHei", 11);
    Font sF(L"Microsoft YaHei", 9);
    
    // ====== 💎 完美同步自 PaintLegalCfgPage 的调色盘与画刷样式 💎 ======
    SolidBrush tc(Color(255, 30, 60, 100));     // 标题及正文深蓝色
    SolidBrush tb(Color(255, 20, 80, 140));    // 激活态蓝色（用于按钮文字）
    SolidBrush td(Color(255, 100, 130, 160));  // 辅助灰蓝色说明文
    SolidBrush bb(Color(255, 200, 230, 250));  // 按钮浅蓝填充底色
    Pen bp(Color(255, 150, 190, 220));         // 按钮高光边框
    Pen kp(Color(255, 30, 60, 100));

    // 1. 绘制开关
    g.DrawString(i18n::T("ITEM_ENABLE"), -1, &rF, PointF((float)(cx + 10), 60.f), &tc);
    g_itemHelperToggleRect = RectF((REAL)(cx + 130), 56.f, 50.f, 24.f);
    ui::DrawToggle(g, cx + 130, 56, g_itemHelperEnabled);

    if (!g_itemHelperEnabled) return;

    // 2. 绘制快捷键配置
    std::wstring keyName = Hotkey::HotkeyToString(g_itemHelperHotkeyMod, g_itemHelperHotkeyVk);
    wchar_t hs[128];
    swprintf_s(hs, i18n::T("ITEM_HOTKEY"), keyName.c_str());
    g.DrawString(hs, -1, &rF, PointF((float)(cx + 10), 92.f), &tc);

    g_itemHelperHotkeyRect = RectF((REAL)(cx + 10), 118.f, 200.f, 20.f);
    g.DrawRectangle(&kp, g_itemHelperHotkeyRect.X, g_itemHelperHotkeyRect.Y, g_itemHelperHotkeyRect.Width, g_itemHelperHotkeyRect.Height);

    const wchar_t* hintStr = g_isBindingItemHelperHotkey ? i18n::T("ITEM_BINDING") : i18n::T("ITEM_CLICK_MOD");
    g.DrawString(hintStr, -1, &sF, PointF((float)(cx + 14), 118.f), &tc);

    // 3. 绘制使用说明
    float usageY = 160.f;
    g.DrawString(i18n::T("ITEM_USAGE_TITLE"), -1, &rF, PointF((float)(cx + 10), usageY), &tc);
    g.DrawString(i18n::T("ITEM_USAGE_LINE1"), -1, &sF, PointF((float)(cx + 15), usageY + 25.f), &td);
    g.DrawString(i18n::T("ITEM_USAGE_LINE2"), -1, &sF, PointF((float)(cx + 15), usageY + 45.f), &td);
    g.DrawString(i18n::T("ITEM_USAGE_LINE3"), -1, &sF, PointF((float)(cx + 15), usageY + 65.f), &td);
    g.DrawString(i18n::T("ITEM_USAGE_LINE4"), -1, &sF, PointF((float)(cx + 15), usageY + 85.f), &td);

    // 4. 💎 重新打造：完美继承 PaintLegalCfgPage 风格的圆角高亮按钮 💎
    const int BW = 140, BH = 26;
    int bx = cx + 10;
    int by = usageY + 110.f; // 动态挂载在底部 
    
    g_itemHelperDirBtnRect = RectF((REAL)bx, (REAL)by, (REAL)BW, (REAL)BH);
    
    // 使用与 Legal 页面一模一样的 GraphicsPath 绘制平滑圆角矩形
    GraphicsPath p; 
    p.AddArc((REAL)bx, (REAL)by, 16.f, 16.f, 180.f, 90.f); 
    p.AddArc((REAL)(bx + BW - 16), (REAL)by, 16.f, 16.f, 270.f, 90.f);
    p.AddArc((REAL)(bx + BW - 16), (REAL)(by + BH - 16), 16.f, 16.f, 0.f, 90.f); 
    p.AddArc((REAL)bx, (REAL)(by + BH - 16), 16.f, 16.f, 90.f, 90.f); 
    p.CloseFigure();
    
    g.FillPath(&bb, &p); // 填充浅蓝渐变柔和底色
    g.DrawPath(&bp, &p); // 描边高光边框
    
    // 渲染文字内容并对齐位置
    g.DrawString(i18n::T("ITEM_BTN_OPEN_DIR"), -1, &sF, PointF((REAL)(bx + 12), (REAL)(by + 5)), &tb);
}

void CheckItemHelperClick(HWND hw, int mx, int my)
{
    // 1. 响应总开关
    if (ui::CheckToggleClick(mx, my, (int)g_itemHelperToggleRect.X, (int)g_itemHelperToggleRect.Y))
    {
        g_itemHelperEnabled = !g_itemHelperEnabled;
        if (!g_itemHelperEnabled && g_itemUI.showOverlay) {
            itemhelper_overlay::Toggle(hInst);
        }
        SaveEvolutionParams();
        Hotkey::UpdateItemHelperHotkey(hw);
        InvalidateRect(hw, nullptr, FALSE);
        return;
    }

    // 2. 💎 响应“打开道具目录”点击（升级为 filesystem 安全路径）💎
    if (mx >= g_itemHelperDirBtnRect.X && mx <= g_itemHelperDirBtnRect.X + g_itemHelperDirBtnRect.Width &&
        my >= g_itemHelperDirBtnRect.Y && my <= g_itemHelperDirBtnRect.Y + g_itemHelperDirBtnRect.Height)
    {
        // 100% 现代 std::filesystem 风格路径控制
        fs::path targetPath = GetItemHelperDirectory();
        
        // 创建目录及其父级链式结构
        std::error_code ec;
        fs::create_directories(targetPath, ec);

        // 打开系统物理目录
        ShellExecuteW(nullptr, L"open", targetPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        return;
    }

    if (!g_itemHelperEnabled) return;

    // 3. 响应修改快捷键区域点击
    if (mx >= g_itemHelperHotkeyRect.X && mx <= g_itemHelperHotkeyRect.X + g_itemHelperHotkeyRect.Width &&
        my >= g_itemHelperHotkeyRect.Y && my <= g_itemHelperHotkeyRect.Y + g_itemHelperHotkeyRect.Height)
    {
        g_isBindingItemHelperHotkey = true;
        InvalidateRect(hw, nullptr, FALSE);
    }
}