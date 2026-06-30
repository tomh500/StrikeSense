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
#include <unordered_map> 
#include "gsi_server.h"
#include "itemhelper_page.h"
namespace fs = std::filesystem;

// 引用外部 GSI 导出的当前游戏地图变量
namespace gsi {
    extern std::string gamemap;
}

// 实例化在 itemhelper_page.h 中声明的全局 UI 状态
ItemHelperUI g_itemUI;

bool IsCS2WindowActive();

namespace itemhelper_overlay
{
    static HWND s_hwnd = nullptr;
    static HHOOK s_hKeyHook = nullptr;
    static std::unique_ptr<Gdiplus::Image> s_previewImage = nullptr;

    // ====================================================================
    // 💎 路径导航与多级光标精准记忆映射表
    // ====================================================================
    static std::unordered_map<std::wstring, fs::path> s_mapLastDir;   // 记录每个地图最后所在的物理路径
    static std::unordered_map<std::wstring, int> s_dirLastIndex;      // 💎 新增：精准记录每一个绝对路径路径下最后选中的行索引
    static fs::path s_currentDir; // 当前正在浏览的绝对路径
    static fs::path s_rootDir;    // 当前地图的根目录路径

    // 前向声明窗口回调
    static LRESULT CALLBACK OverlayProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    // 前向声明低级键盘钩子回调
    static LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wp, LPARAM lp);
    // 扫描地图文件夹下的图片与子文件夹
    static void ScanMapFiles();

    void Initialize(HINSTANCE hInst)
    {
        // 健壮性检查：如果句柄存在但已被系统意外销毁，重置它
        if (s_hwnd && !IsWindow(s_hwnd)) {
            s_hwnd = nullptr;
        }
        if (s_hwnd) return;

        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = OverlayProc;
        wc.hInstance = hInst;
        wc.lpszClassName = L"StrikeSenseItemHelperOverlay";
        RegisterClassExW(&wc);

        int sw = GetSystemMetrics(SM_CXSCREEN);
        int sh = GetSystemMetrics(SM_CYSCREEN);

        s_hwnd = CreateWindowExW(
            WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW,
            wc.lpszClassName, L"", WS_POPUP,
            0, 0, sw, sh,
            nullptr, nullptr, hInst, nullptr
        );

        std::cout << "[道具助手] 遮罩窗口底层物理创建成功，句柄: " << s_hwnd << std::endl;

        if (!s_hwnd) return;

        SetTimer(s_hwnd, 999, 200, nullptr);
        ShowWindow(s_hwnd, SW_HIDE);
    }

    static void ScanMapFiles()
    {
        g_itemUI.files.clear();
        g_itemUI.currentImage.clear();

        wchar_t profile[MAX_PATH] = {};
        GetEnvironmentVariableW(L"USERPROFILE", profile, MAX_PATH);

        fs::path rawMapPath;
        if constexpr (std::is_same_v<decltype(gsi::gamemap), std::string>) {
            rawMapPath = fs::path(gsi::gamemap);
        }
        else {
            rawMapPath = gsi::gamemap;
        }

        fs::path mapName = rawMapPath.filename();
        std::wstring mapNameStr = mapName.wstring();

        static const std::unordered_set<std::wstring> officialMaps = {
            L"de_dust2", L"de_inferno", L"de_mirage", L"de_nuke",
            L"de_overpass", L"de_vertigo", L"de_ancient", L"de_train",
            L"de_cache", L"cs_office"
        };

        if (officialMaps.count(mapNameStr) > 0) {
            s_rootDir = fs::path(profile) / L"StrikeSense" / L"itemhelper" / mapName;
        }
        else {
            s_rootDir = fs::path(profile) / L"StrikeSense" / L"itemhelper" / L"othermaps";
        }

        // 检查历史路径是否合法
        if (s_mapLastDir.count(mapNameStr) > 0 && fs::exists(s_mapLastDir[mapNameStr])) {
            s_currentDir = s_mapLastDir[mapNameStr];
        }
        else {
            s_currentDir = s_rootDir;
            s_mapLastDir[mapNameStr] = s_rootDir;
        }

        if (!fs::exists(s_currentDir)) {
            g_itemUI.needRefresh = false;
            return;
        }

        std::vector<fs::path> subDirs;
        std::vector<fs::path> subFiles;

        try {
            for (const auto& entry : fs::directory_iterator(s_currentDir)) {
                if (entry.is_directory()) {
                    subDirs.push_back(entry.path());
                }
                else if (entry.is_regular_file()) {
                    auto ext = entry.path().extension().wstring();
                    for (auto& c : ext) c = std::towlower(c);

                    if (ext == L".jpg" || ext == L".jpeg" || ext == L".png" || ext == L".bmp") {
                        subFiles.push_back(entry.path());
                    }
                }
            }
        }
        catch (const std::exception& e) {
            std::cout << "[道具助手] 目录扫描异常: " << e.what() << std::endl;
        }

        // 1. 压入返回级
        if (s_currentDir != s_rootDir) {
            g_itemUI.files.push_back(s_currentDir.parent_path());
        }
        // 2. 文件夹置顶
        g_itemUI.files.insert(g_itemUI.files.end(), subDirs.begin(), subDirs.end());
        // 3. 图片置后
        g_itemUI.files.insert(g_itemUI.files.end(), subFiles.begin(), subFiles.end());

        g_itemUI.needRefresh = false;
    }

    void Toggle(HINSTANCE hInst)
    {
        // 💎 强力初始化策略：如果窗口已经失效，就原地火化并重新建立一个
        if (s_hwnd && !IsWindow(s_hwnd)) {
            s_hwnd = nullptr;
        }
        if (!s_hwnd) {
            Initialize(hInst);
        }

        g_itemUI.showOverlay = !g_itemUI.showOverlay;

        if (g_itemUI.showOverlay) {
            std::cout << "[道具助手] 收到唤醒序列：正在初始化并重构一切状态元..." << std::endl;

            // 💎 核心修复：不管以前钩子在不在，先强行撤销，再重新向OS申请新钩子
            // 这能100%解冻被 Windows 系统因超时而偷偷注销掉的“僵尸钩子”
            if (s_hKeyHook) {
                UnhookWindowsHookEx(s_hKeyHook);
                s_hKeyHook = nullptr;
            }
            s_hKeyHook = SetWindowsHookExW(WH_KEYBOARD_LL, LowLevelKeyboardProc, GetModuleHandle(nullptr), 0);
            if (s_hKeyHook) {
                std::cout << "[道具助手] 键盘捕获流热重载成功！" << std::endl;
            }

            // 清理可能残存的预览图和状态
            ReleasePreviewImage();
            g_itemUI.state = IH_BROWSE;

            // 扫描目录
            ScanMapFiles();

            // 💎 恢复索引：提取当前绝对路径上一次停留在第几个项目
            std::wstring dirKey = s_currentDir.wstring();
            if (s_dirLastIndex.count(dirKey) > 0) {
                g_itemUI.selectedIndex = s_dirLastIndex[dirKey];
                // 防御性编程：防止磁盘文件被删导致数组越界
                if (g_itemUI.selectedIndex >= (int)g_itemUI.files.size()) {
                    g_itemUI.selectedIndex = 0;
                }
            }
            else {
                g_itemUI.selectedIndex = 0;
            }

            // 强制将层叠窗口拉到屏幕最顶层渲染
            SetWindowPos(s_hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
            ShowWindow(s_hwnd, SW_SHOWNOACTIVATE);
            Redraw();
        }
        else {
            std::cout << "[道具助手] 隐藏指令：安全隐退遮罩" << std::endl;
            g_itemUI.state = IH_IDLE;
            ShowWindow(s_hwnd, SW_HIDE);

            if (s_hKeyHook) {
                UnhookWindowsHookEx(s_hKeyHook);
                s_hKeyHook = nullptr;
            }
            ReleasePreviewImage();
        }
    }

    void LoadPreviewImage()
    {
        s_previewImage.reset();
        if (g_itemUI.state == IH_PREVIEW && !g_itemUI.currentImage.empty()) {
            auto img = std::make_unique<Gdiplus::Image>(g_itemUI.currentImage.c_str());
            if (img->GetLastStatus() == Gdiplus::Ok) {
                s_previewImage = std::move(img);
            }
        }
    }

    void ReleasePreviewImage()
    {
        if (s_previewImage) {
            s_previewImage.reset();
        }
    }

    void Redraw()
    {
        if (!s_hwnd || !g_itemUI.showOverlay) return;

        int sw = GetSystemMetrics(SM_CXSCREEN);
        int sh = GetSystemMetrics(SM_CYSCREEN);

        HDC hdcScreen = GetDC(nullptr);
        HDC hdcMem = CreateCompatibleDC(hdcScreen);

        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = sw;
        bmi.bmiHeader.biHeight = -sh;
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

            g.Clear(Color(0, 0, 0, 0));

            int panelW = 360;
            int panelH = 550;

            int panelX = (int)((sw - panelW) * g_itemHelperX);
            int panelY = (int)((sh - panelH) * g_itemHelperY);

            float pOpacity = g_itemHelperOpacity;
            if (pOpacity < 0.0f) pOpacity = 0.0f;
            if (pOpacity > 1.0f) pOpacity = 1.0f;

            SolidBrush panelBg(Color((BYTE)(180 * pOpacity), 15, 15, 18));
            g.FillRectangle(&panelBg, (REAL)panelX, (REAL)panelY, (REAL)panelW, (REAL)panelH);

            Pen borderPen(Color((BYTE)(255 * pOpacity), 30, 60, 100), 2.f);
            g.DrawRectangle(&borderPen, (REAL)panelX, (REAL)panelY, (REAL)panelW, (REAL)panelH);

            Font titleFont(L"Microsoft YaHei", 13, FontStyleBold);
            SolidBrush textWhite(Color((BYTE)(255 * pOpacity), 255, 255, 255));

            std::wstring mapWStr = fs::path(gsi::gamemap).wstring();
            std::wstring subPathHint = L"";
            if (s_currentDir != s_rootDir) {
                try {
                    subPathHint = L" > " + fs::relative(s_currentDir, s_rootDir).wstring();
                }
                catch (...) {
                    subPathHint = L" > " + s_currentDir.filename().wstring();
                }
            }
            wchar_t headerText[256];
            swprintf_s(headerText, L"道具助手 [%s%s]", mapWStr.c_str(), subPathHint.c_str());
            g.DrawString(headerText, -1, &titleFont, PointF((REAL)(panelX + 20), (REAL)(panelY + 20)), &textWhite);

            g.DrawLine(&borderPen, (REAL)(panelX + 15), (REAL)(panelY + 55), (REAL)(panelX + panelW - 15), (REAL)(panelY + 55));

            Font itemFont(L"Microsoft YaHei", 10, FontStyleRegular);
            SolidBrush activeColor(Color((BYTE)(255 * pOpacity), 255, 215, 0));
            SolidBrush inactiveColor(Color((BYTE)(200 * pOpacity), 210, 210, 210));
            SolidBrush highlightBg(Color((BYTE)(90 * pOpacity), 255, 255, 255));

            int startY = panelY + 70;
            int itemHeight = 28;
            int maxShowCount = (panelH - 95) / itemHeight;

            if (g_itemUI.files.empty()) {
                g.DrawString(L"该目录下无有效文件或文件夹", -1, &itemFont, PointF((REAL)(panelX + 20), (REAL)startY), &inactiveColor);
            }
            else {
                for (int i = 0; i < (int)g_itemUI.files.size() && i < maxShowCount; ++i) {
                    int currentY = startY + i * itemHeight;

                    // ====================================================================
                    // 💎 彻底移除 Emoji 渲染：采用极简高兼容方括号标签，完美拒绝方块乱码
                    // ====================================================================
                    std::wstring displayName;
                    if (s_currentDir != s_rootDir && i == 0) {
                        displayName = L"[..] 返回上级目录";
                    }
                    else if (fs::is_directory(g_itemUI.files[i])) {
                        displayName = L"[DIR] " + g_itemUI.files[i].filename().wstring();
                    }
                    else {
                        displayName = L"[IMG] " + g_itemUI.files[i].filename().wstring();
                    }

                    if (i == g_itemUI.selectedIndex) {
                        g.FillRectangle(&highlightBg, (REAL)(panelX + 12), (REAL)(currentY - 2), (REAL)(panelW - 24), (REAL)(itemHeight - 2));
                        g.DrawString(displayName.c_str(), -1, &itemFont, PointF((REAL)(panelX + 22), (REAL)currentY), &activeColor);
                    }
                    else {
                        g.DrawString(displayName.c_str(), -1, &itemFont, PointF((REAL)(panelX + 22), (REAL)currentY), &inactiveColor);
                    }
                }
            }

            if (g_itemUI.state == IH_PREVIEW && s_previewImage) {
                int previewW = 520;
                int previewH = 390;
                int previewX = panelX - previewW - 40;
                int previewY = panelY + (panelH - previewH) / 2;

                float imgOpacity = g_itemHelperImgOpacity;
                if (imgOpacity < 0.0f) imgOpacity = 0.0f;
                if (imgOpacity > 1.0f) imgOpacity = 1.0f;

                SolidBrush previewBg(Color((BYTE)(230 * imgOpacity), 5, 5, 5));
                g.FillRectangle(&previewBg, (REAL)previewX, (REAL)previewY, (REAL)previewW, (REAL)previewH);

                Pen previewBorderPen(Color((BYTE)(255 * imgOpacity), 30, 60, 100), 2.f);
                g.DrawRectangle(&previewBorderPen, (REAL)previewX, (REAL)previewY, (REAL)previewW, (REAL)previewH);

                ImageAttributes imgAttr;
                ColorMatrix cm = {
                    1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                    0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
                    0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
                    0.0f, 0.0f, 0.0f, imgOpacity, 0.0f,
                    0.0f, 0.0f, 0.0f, 0.0f, 1.0f
                };
                imgAttr.SetColorMatrix(&cm, ColorMatrixFlagsDefault, ColorAdjustTypeBitmap);

                g.DrawImage(s_previewImage.get(),
                    RectF((REAL)(previewX + 6), (REAL)(previewY + 6), (REAL)(previewW - 12), (REAL)(previewH - 12)),
                    0.0f, 0.0f, (REAL)s_previewImage->GetWidth(), (REAL)s_previewImage->GetHeight(),
                    UnitPixel, &imgAttr);

                Font hintFont(L"Microsoft YaHei", 9, FontStyleRegular);
                SolidBrush hintColor(Color((BYTE)(255 * imgOpacity), 255, 215, 0));
                g.DrawString(L"[Enter] 关闭预览", -1, &hintFont, PointF((REAL)(previewX + 12), (REAL)(previewY + previewH - 24)), &hintColor);
            }
        }

        POINT ptDst = { 0, 0 };
        SIZE sizeDst = { sw, sh };
        POINT ptSrc = { 0, 0 };
        BLENDFUNCTION blend = {};
        blend.BlendOp = AC_SRC_OVER;
        blend.BlendFlags = 0;
        blend.SourceConstantAlpha = 255;
        blend.AlphaFormat = AC_SRC_ALPHA;

        UpdateLayeredWindow(s_hwnd, hdcScreen, &ptDst, &sizeDst, hdcMem, &ptSrc, 0, &blend, ULW_ALPHA);

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
                if (pKey->vkCode == g_itemHelperKeyPrev) {
                    if (!g_itemUI.files.empty()) {
                        g_itemUI.selectedIndex = (g_itemUI.selectedIndex - 1 + (int)g_itemUI.files.size()) % (int)g_itemUI.files.size();
                        // 💎 实时保存当前文件夹下选中的行数位置
                        s_dirLastIndex[s_currentDir.wstring()] = g_itemUI.selectedIndex;
                        Redraw();
                    }
                    return 1;
                }
                else if (pKey->vkCode == g_itemHelperKeyNext) {
                    if (!g_itemUI.files.empty()) {
                        g_itemUI.selectedIndex = (g_itemUI.selectedIndex + 1) % (int)g_itemUI.files.size();
                        // 💎 实时保存当前文件夹下选中的行数位置
                        s_dirLastIndex[s_currentDir.wstring()] = g_itemUI.selectedIndex;
                        Redraw();
                    }
                    return 1;
                }
                else if (pKey->vkCode == g_itemHelperKeySelect) {
                    if (g_itemUI.state == IH_BROWSE) {
                        if (!g_itemUI.files.empty() && g_itemUI.selectedIndex >= 0 && g_itemUI.selectedIndex < (int)g_itemUI.files.size()) {

                            fs::path selectedPath = g_itemUI.files[g_itemUI.selectedIndex];

                            fs::path rawMapPath;
                            if constexpr (std::is_same_v<decltype(gsi::gamemap), std::string>) {
                                rawMapPath = fs::path(gsi::gamemap);
                            }
                            else {
                                rawMapPath = gsi::gamemap;
                            }
                            std::wstring mapNameStr = rawMapPath.filename().wstring();

                            // 分支一：点击返回上级
                            if (s_currentDir != s_rootDir && g_itemUI.selectedIndex == 0) {
                                s_currentDir = selectedPath;
                                s_mapLastDir[mapNameStr] = s_currentDir;

                                ScanMapFiles();

                                // 💎 恢复上级目录的历史光标行数
                                std::wstring dirKey = s_currentDir.wstring();
                                if (s_dirLastIndex.count(dirKey) > 0) {
                                    g_itemUI.selectedIndex = s_dirLastIndex[dirKey];
                                    if (g_itemUI.selectedIndex >= (int)g_itemUI.files.size()) g_itemUI.selectedIndex = 0;
                                }
                                else {
                                    g_itemUI.selectedIndex = 0;
                                }
                            }
                            // 分支二：点击进入子文件夹
                            else if (fs::is_directory(selectedPath)) {
                                s_currentDir = selectedPath;
                                s_mapLastDir[mapNameStr] = s_currentDir;

                                ScanMapFiles();

                                // 💎 恢复或者创建该深度文件夹的历史光标行数
                                std::wstring dirKey = s_currentDir.wstring();
                                if (s_dirLastIndex.count(dirKey) > 0) {
                                    g_itemUI.selectedIndex = s_dirLastIndex[dirKey];
                                    if (g_itemUI.selectedIndex >= (int)g_itemUI.files.size()) g_itemUI.selectedIndex = 0;
                                }
                                else {
                                    g_itemUI.selectedIndex = 0;
                                }
                            }
                            // 分支三：点击展示高清图片
                            else {
                                g_itemUI.currentImage = selectedPath;
                                g_itemUI.state = IH_PREVIEW;
                                LoadPreviewImage();
                            }
                        }
                    }
                    else if (g_itemUI.state == IH_PREVIEW) {
                        g_itemUI.state = IH_BROWSE;
                        ReleasePreviewImage();
                    }
                    Redraw();
                    return 1;
                }
            }
        }
        return CallNextHookEx(nullptr, nCode, wp, lp);
    }

    static LRESULT CALLBACK OverlayProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
    {
        switch (msg) {
        case WM_TIMER: {
            if (wp == 999) {
                if (g_itemHelperAutoHide && g_itemUI.showOverlay && !IsCS2WindowActive()) {
                    HINSTANCE hInst = (HINSTANCE)GetWindowLongPtrW(hwnd, GWLP_HINSTANCE);
                    Toggle(hInst);
                    std::cout << "[道具助手] 检测到失去CS2游戏焦点，窗口自动隐退。" << std::endl;
                }
            }
            break;
        }
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
