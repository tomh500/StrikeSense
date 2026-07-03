#include "pages.h"
#include "gsi_server.h"
#include "config.h" 
#include "i18n.h"
static bool s_toggleStates[6] = {false, true, false, false, false, false};

void PaintSettingsPage(Gdiplus::Graphics& g, int cx, int cw, int H, HWND) {
    using namespace Gdiplus;
    using namespace i18n; // 引入国际化命名空间

    // 1. 标题国际化
    ui::DrawHeader(g, cx, cw, T(Keys::SETTINGS_TITLE));
    SolidBrush knB(Color(255, 60, 160, 230));
    Font tF(L"Microsoft YaHei", 12);
    SolidBrush tdCol(Color(255, 30, 60, 100));

    // 完美：直接读取 GSI 缓存，UI 刷新零延迟
    // 润色：改为引用 & 避免非必要拷贝
    config::Settings& c = gsi::GetConfig();
    s_toggleStates[0] = c.custom_musickit;  s_toggleStates[1] = c.enable_kill_sound;
    s_toggleStates[2] = c.custom_flashbang; s_toggleStates[3] = c.low_memory;
    s_toggleStates[4] = c.show_mvp;         s_toggleStates[5] = c.ogg;

    // 核心修改：动态查表的国际化文本数组，不加 static 以便实时切换语言
    const wchar_t* lbs[] = {
        T(Keys::SETTINGS_CUSTOM_MUSIC), // L"自定义音乐包"
        T(Keys::SETTINGS_KILL_SOUND),   // L"击杀音效替换"
        T(Keys::SETTINGS_FLASH),        // L"闪光叠加"
        T(Keys::SETTINGS_LOWMEM),       // L"低内存模式"
        T(Keys::SETTINGS_MVP),          // L"MVP信息板"
        T(Keys::SETTINGS_OGG)           // L"OGG格式"
    };

    int y = 50;
    for (int i = 0; i < 6; ++i) {
        g.DrawString(lbs[i], -1, &tF, PointF(cx + 10, (float)y), &tdCol);
        ui::DrawToggle(g, cx + cw - 60, y, s_toggleStates[i]);
        y += 36;
    }
    y += 10; int sw = cw - 80;

// 音量文本国际化
g.DrawString(T(Keys::SETTINGS_VOL), -1, &tF,
    PointF((float)(cx + 10), (float)y), &tdCol);

// 滑条
ui::DrawSlider(g, cx + 80, y, sw, c.volume);

// 手动绘制圆形滑块（与 Evolution 页面一致）
float kx = cx + 80 + (int)(sw * c.volume) - 8.f;
g.FillEllipse(&knB, kx, y - 6.f, 16.f, 16.f);

// 百分比文本
wchar_t vt[32];
swprintf_s(vt, L"%.0f%%", c.volume * 100.f);

g.DrawString(vt, -1, &tF,
    PointF((float)(cx + 80 + sw + 8),
           (float)(y - 4)),
    &tdCol);
}

void CheckSettingsClick(HWND hw, int mx, int my) {
    int cx = SIDEBAR_W + 12, cw = 0;
    RECT rc; GetClientRect(hw, &rc); cw = rc.right - rc.left - cx - 12;
    
    // 1. 检查 6 个开关按钮的点击
    for (int i = 0; i < 6; ++i) {
        int tx = cx + cw - 60, ty = 50 + i * 36;
        if (ui::CheckToggleClick(mx, my, tx, ty)) {
            s_toggleStates[i] = !s_toggleStates[i];
            
            // 优化：使用引用 & 实时修改内存真身
            config::Settings& c = gsi::GetConfig();
            c.custom_musickit = s_toggleStates[0];  c.enable_kill_sound = s_toggleStates[1];
            c.custom_flashbang = s_toggleStates[2]; c.low_memory = s_toggleStates[3];
            c.show_mvp = s_toggleStates[4];         c.ogg = s_toggleStates[5];
            
            config::Save(c);           // 1. 保存到本地 json 文件
            gsi::RefreshConfig();      // 2. 刷新 GSI 内存
            InvalidateRect(hw, nullptr, FALSE); // 3. 触发窗口重绘
            return;                    // 4. 正确退出函数
        }
    }
    
    // 2. 检查音量滑块的点击
    float val;
    int slY = 50 + 6 * 36 + 10;
    if (ui::CheckSliderClick(mx, my, cx + 80, slY, cw - 80, val)) {
        // 优化：同样改用引用 &，防止在极度高频地拖动滑块时产生局部副本丢失
        config::Settings& c = gsi::GetConfig(); 
        c.volume = val;
        
        config::Save(c);               // 1. 保存音量至磁盘
        gsi::RefreshConfig();          // 2. 刷新 GSI 内存，让音效音量立刻实时生效
        InvalidateRect(hw, nullptr, FALSE); // 3. 触发窗口重绘
    }
}