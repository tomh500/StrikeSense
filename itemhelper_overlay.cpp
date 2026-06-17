#include "itemhelper_overlay.h"
#include "itemhelper_page.h"
#include <gdiplus.h>
#include <filesystem>
#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <cwctype>
#include <cstdlib>
#include <unordered_set>
#include "gsi_server.h"
#include "itemhelper_page.h"
namespace fs = std::filesystem;

// 引用外部 GSI 导出的当前游戏地图变量
namespace gsi {
    extern std::string gamemap; // 假设为 std::string，如果是 wstring 下面会自动适配
}

// 实例化在 itemhelper_page.h 中声明的全局 UI 状态
ItemHelperUI g_itemUI;

bool IsCS2WindowActive();

namespace itemhelper_overlay
{
    static HWND s_hwnd = nullptr;
    static HHOOK s_hKeyHook = nullptr;
    static std::unique_ptr<Gdiplus::Image> s_previewImage = nullptr;

    // 前向声明窗口回调
    static LRESULT CALLBACK OverlayProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    // 前向声明低级键盘钩子回调
    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wp, LPARAM lp);
    // 扫描地图文件夹下的图片
    static void ScanMapFiles();

    void Initialize(HINSTANCE hInst)
    {
        if (s_hwnd) return;

        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = OverlayProc;
        wc.hInstance = hInst;
        wc.lpszClassName = L"StrikeSenseItemHelperOverlay";
        RegisterClassExW(&wc);

        int sw = GetSystemMetrics(SM_CXSCREEN);
        int sh = GetSystemMetrics(SM_CYSCREEN);

        // 创建最高层级、层叠、鼠标穿透的无边框全屏窗口
        s_hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW,
            wc.lpszClassName, L"", WS_POPUP,
            0, 0, sw, sh,
            nullptr, nullptr, hInst, nullptr
        );

        std::cout << "[道具助手] 全屏层叠遮罩窗口初始化成功，句柄: " << s_hwnd << std::endl;

        if (!s_hwnd) return;

    // ====== 💎 加上这行：启动一个 ID 为 999 且每 200ms 触发一次的定时器 💎 ======
    SetTimer(s_hwnd, 999, 200, nullptr);
    // ====================================================================

    ShowWindow(s_hwnd, SW_HIDE); // 初始隐藏
    }

static void ScanMapFiles()
{
    g_itemUI.files.clear();
    g_itemUI.selectedIndex = 0;
    g_itemUI.currentImage.clear(); // 清空当前图片路径缓存

    wchar_t profile[MAX_PATH] = {};
    GetEnvironmentVariableW(L"USERPROFILE", profile, MAX_PATH);
    
    // 自动适配 gsi::gamemap 的 string 或 wstring 状态转换
    fs::path rawMapPath;
    if constexpr (std::is_same_v<decltype(gsi::gamemap), std::string>) {
        rawMapPath = fs::path(gsi::gamemap);
    } else {
        rawMapPath = gsi::gamemap;
    }

    // 1. 提取出纯粹的地图名/ID (例如 "maps/de_office" -> "de_office" 或 "workshop/1234/de_xxx" -> "de_xxx")
    fs::path mapName = rawMapPath.filename();

    // 2. 定义常规/官方地图的白名单列表
    static const std::unordered_set<std::wstring> officialMaps = {
        L"de_dust2", L"de_inferno", L"de_mirage", L"de_nuke", 
        L"de_overpass", L"de_vertigo", L"de_ancient", L"de_train", 
        L"de_cache", L"cs_office"
    };

    // 3. 决定最终扫描的目标文件夹
    fs::path mapFolder;
    
    // 将 mapName 转为 wstring 方便在集合中查找
    std::wstring mapNameStr = mapName.wstring(); 

    if (officialMaps.count(mapNameStr) > 0) {
        // 如果在白名单内，走正常文件夹：%UserProfile%\StrikeSense\itemhelper\地图名
        mapFolder = fs::path(profile) / L"StrikeSense" / L"itemhelper" / mapName;
    } else {
        // 如果是社区地图或未知地图，自动定向到 othermaps 文件夹
        mapFolder = fs::path(profile) / L"StrikeSense" / L"itemhelper" / L"othermaps";
        std::cout << "[道具助手] 检测到未知或社区地图 [" << mapName.string() << "], 自动重定向至 othermaps" << std::endl;
    }

    std::cout << "[道具助手] 开始扫描当前地图文件目录: " << mapFolder.string() << std::endl;

    if (!fs::exists(mapFolder)) {
        std::cout << "[道具助手] 警告: 地图文件夹不存在: " << mapFolder.string() << std::endl;
        g_itemUI.needRefresh = false; // 关闭刷新标志，防止死循环刷盘
        return;
    }

    try {
        for (const auto& entry : fs::directory_iterator(mapFolder)) {
            if (entry.is_regular_file()) {
                auto ext = entry.path().extension().wstring();
        for (auto& c : ext) c = std::towlower(c); // 确保转成了小写

        if (ext == L".jpg" || ext == L".jpeg" || ext == L".png" || ext == L".bmp") {
            g_itemUI.files.push_back(entry.path());
         // 加上这行打印，看看控制台有没有成功吐出 .png 的日志
         std::cout << "[道具助手] 成功载入PNG/JPG文件: " << entry.path().filename().string() << std::endl;
        }
            }
        }
    }
    catch (const std::exception& e) {
        std::cout << "[道具助手] 目录迭代异常: " << e.what() << std::endl;
    }

    // 4. 如果成功扫描到了图片，默认把第一张图赋值给当前展示图
    if (!g_itemUI.files.empty()) {
        g_itemUI.currentImage = g_itemUI.files[0];
    }

    g_itemUI.needRefresh = false; // 扫描完成后，必须关闭刷新标志
    std::cout << "[道具助手] 扫描完成。有效图片总数: " << g_itemUI.files.size() << std::endl;
}
    void Toggle(HINSTANCE hInst)
    {
        if (!s_hwnd) {
            Initialize(hInst);
        }

        g_itemUI.showOverlay = !g_itemUI.showOverlay;

        if (g_itemUI.showOverlay) {
            std::cout << "[道具助手] 收到开启指令，激活遮罩覆层" << std::endl;
            ScanMapFiles();
            g_itemUI.state = IH_BROWSE;

            // 以非激活方式显示窗口，严禁抢走游戏窗口的 Focus 焦点
            ShowWindow(s_hwnd, SW_SHOWNOACTIVATE);

            // 挂载全局低级键盘钩子，用于捕获游戏内上下箭头与回车
            if (!s_hKeyHook) {
                s_hKeyHook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(nullptr), 0);
                std::cout << "[道具助手] 全局低级键盘动作捕获钩子已挂载" << std::endl;
            }
            Redraw();
        }
        else {
            std::cout << "[道具助手] 收到关闭指令，隐藏遮罩覆层" << std::endl;
            g_itemUI.state = IH_IDLE;
            ShowWindow(s_hwnd, SW_HIDE);

            if (s_hKeyHook) {
                UnhookWindowsHookEx(s_hKeyHook);
                s_hKeyHook = nullptr;
                std::cout << "[道具助手] 全局低级键盘动作捕获钩子已安全卸载" << std::endl;
            }
            ReleasePreviewImage();
        }
    }

    void LoadPreviewImage()
    {
        s_previewImage.reset();
        if (g_itemUI.state == IH_PREVIEW && !g_itemUI.currentImage.empty()) {
            std::cout << "[道具助手] 正在加载高清 GDI+ 预览图内存: " << g_itemUI.currentImage.string() << std::endl;
            auto img = std::make_unique<Gdiplus::Image>(g_itemUI.currentImage.c_str());
            if (img->GetLastStatus() == Gdiplus::Ok) {
                s_previewImage = std::move(img);
                std::cout << "[道具助手] 内存常驻预览图片加载成功" << std::endl;
            } else {
                std::cout << "[道具助手] 错误: 图片解码失败，Gdiplus 状态码: " << img->GetLastStatus() << std::endl;
            }
        }
    }

    void ReleasePreviewImage()
    {
        if (s_previewImage) {
            s_previewImage.reset();
            std::cout << "[道具助手] 预览图内存物理释放完成" << std::endl;
        }
    }

    void Redraw()
    {
        if (!s_hwnd || !g_itemUI.showOverlay) return;

        int sw = GetSystemMetrics(SM_CXSCREEN);
        int sh = GetSystemMetrics(SM_CYSCREEN);

        // 创建双缓冲动态离屏纹理，实现无闪烁半透明渲染
        HDC hdcScreen = GetDC(nullptr);
        HDC hdcMem = CreateCompatibleDC(hdcScreen);

        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = sw;
        bmi.bmiHeader.biHeight = -sh; // 顶层自上而下像素排列
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void* pBits = nullptr;
        HBITMAP hBitmap = CreateDIBSection(hdcMem, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
        HBITMAP hOldBmp = (HBITMAP)SelectObject(hdcMem, hBitmap);

        {
            using namespace Gdiplus;
            Graphics g(hdcMem);
            g.SetSmoothingMode(SmoothingModeAntiAlias);
            g.SetTextRenderingHint(TextRenderingHintAntiAlias);

            // 清空全屏，全透明
            g.Clear(Color(0, 0, 0, 0));

            // ==== 1. 右侧列表渲染配置 ====
            int panelW = 360;
            int panelH = 550;
            int panelX = sw - panelW - 60; // 靠右贴边留置空隙
            int panelY = (sh - panelH) / 2;

            // 绘制高级毛玻璃视觉感知的半透明黑色底座 (Alpha = 180)
            SolidBrush panelBg(Color(180, 15, 15, 18));
            g.FillRectangle(&panelBg, (REAL)panelX, (REAL)panelY, (REAL)panelW, (REAL)panelH);

            // 绘制精细描框
            Pen borderPen(Color(255, 30, 60, 100), 2.f);
            g.DrawRectangle(&borderPen, (REAL)panelX, (REAL)panelY, (REAL)panelW, (REAL)panelH);

            // 标题输出
            Font titleFont(L"Microsoft YaHei", 13, FontStyleBold);
            SolidBrush textWhite(Color(255, 255, 255, 255));
            
            std::wstring mapWStr = fs::path(gsi::gamemap).wstring();
            wchar_t headerText[128];
            swprintf_s(headerText, L"道具助手清单 [%s]", mapWStr.c_str());
            g.DrawString(headerText, -1, &titleFont, PointF((REAL)(panelX + 20), (REAL)(panelY + 20)), &textWhite);

            // 分割线
            g.DrawLine(&borderPen, (REAL)(panelX + 15), (REAL)(panelY + 55), (REAL)(panelX + panelW - 15), (REAL)(panelY + 55));

            // 滚动列出文件名
            Font itemFont(L"Microsoft YaHei", 10, FontStyleRegular);
            SolidBrush activeColor(Color(255, 255, 215, 0));    // 被选中项：高亮土豪金
            SolidBrush inactiveColor(Color(200, 210, 210, 210)); // 未选中项：淡灰色
            SolidBrush highlightBg(Color(90, 255, 255, 255));   // 选中行的柔和亮色条背景

            int startY = panelY + 70;
            int itemHeight = 28;
            int maxShowCount = (panelH - 95) / itemHeight;

            if (g_itemUI.files.empty()) {
                g.DrawString(L"该地图目录下无有效图片", -1, &itemFont, PointF((REAL)(panelX + 20), (REAL)startY), &inactiveColor);
            }
            else {
                for (int i = 0; i < (int)g_itemUI.files.size() && i < maxShowCount; ++i) {
                    int currentY = startY + i * itemHeight;
                    std::wstring name = g_itemUI.files[i].filename().wstring();

                    if (i == g_itemUI.selectedIndex) {
                        // 绘制选中高亮背景条
                        g.FillRectangle(&highlightBg, (REAL)(panelX + 12), (REAL)(currentY - 2), (REAL)(panelW - 24), (REAL)(itemHeight - 2));
                        g.DrawString(name.c_str(), -1, &itemFont, PointF((REAL)(panelX + 22), (REAL)currentY), &activeColor);
                    } else {
                        g.DrawString(name.c_str(), -1, &itemFont, PointF((REAL)(panelX + 22), (REAL)currentY), &inactiveColor);
                    }
                }
            }

            // ==== 2. 左侧图片预览长方形区域渲染 ====
            if (g_itemUI.state == IH_PREVIEW && s_previewImage) {
                int previewW = 520;
                int previewH = 390; // 4:3 经典准星道具图比例
                int previewX = panelX - previewW - 40; // 恰好落在遮罩控制件的左侧
                int previewY = panelY + (panelH - previewH) / 2; // 居中垂直对齐

                // 画预览区大黑底置框
                SolidBrush previewBg(Color(230, 5, 5, 5));
                g.FillRectangle(&previewBg, (REAL)previewX, (REAL)previewY, (REAL)previewW, (REAL)previewH);
                g.DrawRectangle(&borderPen, (REAL)previewX, (REAL)previewY, (REAL)previewW, (REAL)previewH);

                // 渲染图像到全屏覆层之上
                g.DrawImage(s_previewImage.get(), (REAL)(previewX + 6), (REAL)(previewY + 6), (REAL)(previewW - 12), (REAL)(previewH - 12));
                
                // 水印或操作提示
                Font hintFont(L"Microsoft YaHei", 9, FontStyleRegular);
                g.DrawString(L"[Enter] 关闭预览", -1, &hintFont, PointF((REAL)(previewX + 12), (REAL)(previewY + previewH - 24)), &activeColor);
            }
        }

        // 核心混合层叠：使用 Windows 硬件组合器将每像素含有 ARGB 属性的数据压入显示器最前端
        POINT ptDst = { 0, 0 };
        SIZE sizeDst = { sw, sh };
        POINT ptSrc = { 0, 0 };
        BLENDFUNCTION blend = {};
        blend.BlendOp = AC_SRC_OVER;
        blend.BlendFlags = 0;
        blend.SourceConstantAlpha = 255;  // 整体控制透明度
        blend.AlphaFormat = AC_SRC_ALPHA; // 允许透明度通道起效

        UpdateLayeredWindow(s_hwnd, hdcScreen, &ptDst, &sizeDst, hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);

        // 彻底销毁 GDI 临时资源，绝不发生系统资源泄露
        SelectObject(hdcMem, hOldBmp);
        DeleteObject(hBitmap);
        DeleteDC(hdcMem);
        ReleaseDC(nullptr, hdcScreen);
    }

    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wp, LPARAM lp)
    {
        if (nCode == HC_ACTION && (wp == WM_KEYDOWN || wp == WM_SYSKEYDOWN)) {
            KBDLLHOOKSTRUCT* pKey = reinterpret_cast<KBDLLHOOKSTRUCT*>(lp);

            if (g_itemUI.showOverlay) {
                // 1. 捕获上箭头
                if (pKey->vkCode == VK_UP) {
                    if (!g_itemUI.files.empty()) {
                        g_itemUI.selectedIndex = (g_itemUI.selectedIndex - 1 + (int)g_itemUI.files.size()) % (int)g_itemUI.files.size();
                        std::cout << "[道具助手] 按键响应: UP, 当前行索引: " << g_itemUI.selectedIndex << std::endl;
                        Redraw();
                    }
                    return 1; // 吞噬按键信号，不让其传递给游戏造成视角移动
                }
                // 2. 捕获下箭头
                else if (pKey->vkCode == VK_DOWN) {
                    if (!g_itemUI.files.empty()) {
                        g_itemUI.selectedIndex = (g_itemUI.selectedIndex + 1) % (int)g_itemUI.files.size();
                        std::cout << "[道具助手] 按键响应: DOWN, 当前行索引: " << g_itemUI.selectedIndex << std::endl;
                        Redraw();
                    }
                    return 1; // 吞噬按键信号
                }
                // 3. 捕获回车键
                else if (pKey->vkCode == VK_RETURN) {
                    std::cout << "[道具助手] 按键响应: ENTER" << std::endl;
                    if (g_itemUI.state == IH_BROWSE) {
                        if (!g_itemUI.files.empty() && g_itemUI.selectedIndex >= 0 && g_itemUI.selectedIndex < (int)g_itemUI.files.size()) {
                            g_itemUI.currentImage = g_itemUI.files[g_itemUI.selectedIndex];
                            g_itemUI.state = IH_PREVIEW;
                            LoadPreviewImage();
                        }
                    } else if (g_itemUI.state == IH_PREVIEW) {
                        // 再次点击 Enter 销毁渲染的图片，回到文件浏览模式
                        g_itemUI.state = IH_BROWSE;
                        ReleasePreviewImage();
                    }
                    Redraw();
                    return 1; // 吞噬回车键信号，防止在游戏内误触发送空聊天
                }
            }
        }
        return CallNextHookEx(nullptr, nCode, wp, lp);
    }

    static LRESULT CALLBACK OverlayProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
    {
        switch (msg) {
            // ====== 💎 核心修复：处理定时器消息，自适应隐藏 💎 ======
            case WM_TIMER:{
                if (wp == 999) {
                    // 如果当前道具助手遮罩正显示在屏幕上，但检测到游戏已经不再活跃（切回桌面了）
                    if (g_itemUI.showOverlay && !IsCS2WindowActive()) {
                        // 动态获取当前的实例句柄
                        HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE);
                        // 强制安全隐藏遮罩，并卸载键盘钩子
                        Toggle(hInst); 
                        std::cout << "[道具助手] 检测到失去CS2游戏焦点，窗口自动隐退。" << std::endl;
                    }
                }
                break;}
            // =====================================================
            case WM_ERASEBKGND: return TRUE;
            case WM_DESTROY:
               { 
                KillTimer(hwnd, 999);
                break;
               }
            default: break;
        }
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    void Shutdown()
    {
        if (s_hKeyHook) {
            UnhookWindowsHookEx(s_hKeyHook);
            s_hKeyHook = nullptr;
        }
        ReleasePreviewImage();
        if (s_hwnd) {
            DestroyWindow(s_hwnd);
            s_hwnd = nullptr;
        }
    }
}