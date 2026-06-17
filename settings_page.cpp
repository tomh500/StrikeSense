#include "pages.h"
#include "gsi_server.h"

static bool s_toggleStates[6] = {false, true, false, false, false, false};

void PaintSettingsPage(Gdiplus::Graphics& g, int cx, int cw, int H, HWND) {
    using namespace Gdiplus;
    ui::DrawHeader(g, cx, cw, L"遗产核心");
    Font tF(L"Microsoft YaHei", 12);
    SolidBrush tdCol(Color(255, 30, 60, 100));

    // 完美：直接读取 GSI 缓存，UI 刷新零延迟
    config::Settings c = gsi::GetConfig();
    s_toggleStates[0] = c.custom_musickit;  s_toggleStates[1] = c.enable_kill_sound;
    s_toggleStates[2] = c.custom_flashbang; s_toggleStates[3] = c.low_memory;
    s_toggleStates[4] = c.show_mvp;         s_toggleStates[5] = c.ogg;

    const wchar_t* lbs[] = {L"自定义音乐包", L"击杀音效替换", L"闪光叠加", L"低内存模式", L"MVP信息板", L"OGG格式"};
    int y = 50;
    for (int i = 0; i < 6; ++i) {
        g.DrawString(lbs[i], -1, &tF, PointF(cx + 10, y), &tdCol);
        ui::DrawToggle(g, cx + cw - 60, y, s_toggleStates[i]);
        y += 36;
    }
    y += 10; int sw = cw - 80;
    g.DrawString(L"音量", -1, &tF, PointF(cx + 10, y), &tdCol);
    ui::DrawSlider(g, cx + 80, y, sw, c.volume);
    wchar_t vt[32]; swprintf_s(vt, L"%.0f%%", c.volume * 100.f);
    g.DrawString(vt, -1, &tF, PointF(cx + 80 + sw + 8, y - 4), &tdCol);
}

void CheckSettingsClick(HWND hw, int mx, int my) {
    int cx = SIDEBAR_W + 12, cw = 0;
    RECT rc; GetClientRect(hw, &rc); cw = rc.right - rc.left - cx - 12;
    
    // 1. 检查 6 个开关按钮的点击
    for (int i = 0; i < 6; ++i) {
        int tx = cx + cw - 60, ty = 50 + i * 36;
        if (ui::CheckToggleClick(mx, my, tx, ty)) {
            s_toggleStates[i] = !s_toggleStates[i];
            
            // 优化：从内存缓存拿底色配置
            config::Settings c = gsi::GetConfig();
            c.custom_musickit = s_toggleStates[0];  c.enable_kill_sound = s_toggleStates[1];
            c.custom_flashbang = s_toggleStates[2]; c.low_memory = s_toggleStates[3];
            c.show_mvp = s_toggleStates[4];         c.ogg = s_toggleStates[5];
            
            config::Save(c);           // 1. 保存到本地 json 文件
            gsi::RefreshConfig();      // 2. 刷新 GSI 内存（调整顺序，必须在 return 之前！）
            InvalidateRect(hw, nullptr, FALSE); // 3. 触发窗口重绘
            return;                    // 4. 正确退出函数
        }
    }
    
    // 2. 检查音量滑块的点击
    float val;
    int slY = 50 + 6 * 36 + 10;
    if (ui::CheckSliderClick(mx, my, cx + 80, slY, cw - 80, val)) {
        // 优化：同样改用 gsi::GetConfig() 避免读取磁盘
        config::Settings c = gsi::GetConfig(); 
        c.volume = val;
        
        config::Save(c);               // 1. 保存音量至磁盘
        gsi::RefreshConfig();          // 2. 刷新 GSI 内存，让音效音量立刻实时生效
        InvalidateRect(hw, nullptr, FALSE); // 3. 触发窗口重绘
    }
}