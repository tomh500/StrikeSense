#include "pages.h"
#include "steam_helper.h"
#include <fstream>
#include <sstream>
#include <filesystem>

namespace fs = std::filesystem;

static std::wstring g_autoexecPath;
static std::wstring g_autoexecContent;
static std::wstring g_editBuffer;      // 编辑缓冲区
static std::wstring g_autoexecStatus = L"未加载";
static ULONGLONG g_lastWriteTime = 0;
static bool g_editing = false;          // 是否正在编辑
static int g_cursorPos = 0;             // 光标位置

static const wchar_t* SOCD_BLOCK = LR"(
//--StrikeSense SOCD--

alias jneutral_fb "forwardback 0 0 0"
alias jforward    " forwardback 1 0 0"
alias jback       " forwardback -1 0 0"

alias jneutral_rl "rightleft 0 0 0"
alias jright      "rightleft 1 0 0"
alias jleft       " rightleft -1 0 0"

alias jfb_00 "alias +ForwardEvent jfb_10; alias +BackEvent jfb_01; jneutral_fb"
alias jfb_10 "alias -ForwardEvent jfb_00; alias +BackEvent jfb_11_s; jforward"  
alias jfb_01 "alias -BackEvent jfb_00; alias +ForwardEvent jfb_11_w; jback"     
alias jfb_11_s "alias -BackEvent jfb_10; alias -ForwardEvent jfb_01; jback"     
alias jfb_11_w "alias -BackEvent jfb_10; alias -ForwardEvent jfb_01; jforward"    

alias jrl_00 "alias +LeftEvent jrl_10; alias +RightEvent jrl_01; jneutral_rl"  
alias jrl_10 "alias -LeftEvent jrl_00; alias +RightEvent jrl_11_d; jleft"     
alias jrl_01 "alias -RightEvent jrl_00; alias +LeftEvent jrl_11_a; jright"     
alias jrl_11_d "alias -RightEvent jrl_10; alias -LeftEvent jrl_01; jright"   
alias jrl_11_a "alias -RightEvent jrl_10; alias -LeftEvent jrl_01; jleft"      

jrl_00
jfb_00

//Edit Keybinds Here
bind w +ForwardEvent
bind s +BackEvent
bind a +LeftEvent
bind d +RightEvent

//--StrikeSense SOCD END--
)";

static std::wstring FindAutoexecPath() {
    std::wstring sv = strikesense::LoadSavedCfgPath();
    if (!sv.empty()) {
        fs::path p(sv); p /= L"autoexec.cfg";
        if (fs::exists(p)) return p.wstring();
        if (fs::exists(sv)) return sv + L"\\autoexec.cfg";
    }
    std::wstring sp = strikesense::GetSteamPathFromRegistry();
    if (sp.empty()) return L"";
    std::wstring cd = strikesense::FindCS2InstallDir(sp);
    if (cd.empty()) return L"";
    return strikesense::GetCS2CfgPath(cd) + L"\\autoexec.cfg";
}

static void LoadAutoexecContent() {
    g_autoexecPath = FindAutoexecPath();
    if (g_autoexecPath.empty()) {
        g_autoexecContent = L"// 无法找到 autoexec.cfg - 请先设置CS2配置目录";
        g_autoexecStatus = L"路径错误"; return;
    }
    if (!fs::exists(g_autoexecPath)) {
        g_autoexecContent = L"// autoexec.cfg 不存在\n// 保存时将创建新文件";
        g_autoexecStatus = L"新文件"; g_lastWriteTime = 0; return;
    }
    auto ft = fs::last_write_time(g_autoexecPath);
    ULONGLONG wt = ft.time_since_epoch().count();
    if (wt == g_lastWriteTime && !g_autoexecContent.empty() && !g_editing) return;
    g_lastWriteTime = wt;

    std::ifstream in(g_autoexecPath, std::ios::binary);
    if (!in.is_open()) { g_autoexecContent = L"// 无法读取文件"; g_autoexecStatus = L"读取失败"; return; }
    std::stringstream ss; ss << in.rdbuf(); in.close();
    std::string utf8 = ss.str();
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), nullptr, 0);
    if (wlen <= 0) wlen = MultiByteToWideChar(CP_ACP, 0, utf8.c_str(), (int)utf8.size(), nullptr, 0);
    g_autoexecContent.resize(wlen);
    MultiByteToWideChar(wlen > 0 ? CP_UTF8 : CP_ACP, 0, utf8.c_str(), (int)utf8.size(), &g_autoexecContent[0], wlen);
    g_autoexecStatus = L"已加载";
    if (!g_editing) g_editBuffer = g_autoexecContent;
}

static void SaveAutoexecContent(const std::wstring& content) {
    if (g_autoexecPath.empty()) return;
    int u8len = WideCharToMultiByte(CP_UTF8, 0, content.c_str(), (int)content.size(), nullptr, 0, nullptr, nullptr);
    if (u8len <= 0) return;
    std::string utf8(u8len, 0);
    WideCharToMultiByte(CP_UTF8, 0, content.c_str(), (int)content.size(), &utf8[0], u8len, nullptr, nullptr);
    std::ofstream out(g_autoexecPath, std::ios::binary);
    if (!out.is_open()) { g_autoexecStatus = L"保存失败"; return; }
    out.write(utf8.data(), utf8.size()); out.close();
    g_autoexecContent = content; g_autoexecStatus = L"已保存";
    g_editBuffer = content;
    auto ft = fs::last_write_time(g_autoexecPath);
    g_lastWriteTime = ft.time_since_epoch().count();
}

static void AppendToFile(const std::wstring& text) {
    g_autoexecContent += text;
    g_editBuffer = g_autoexecContent;
    SaveAutoexecContent(g_autoexecContent);
}

static Gdiplus::RectF g_editRect, g_saveBtnRect, g_reloadBtnRect, g_socdBtnRect;

void PaintLegalCfgPage(Gdiplus::Graphics& g, int cx, int cw, int H, HWND) {
    using namespace Gdiplus;
    ui::DrawHeader(g, cx, cw, L"合法配置");
    Font rF(L"Microsoft YaHei", 11), sF(L"Microsoft YaHei", 9);
    SolidBrush tdCol(Color(255, 30, 60, 100)), tbCol(Color(255, 20, 80, 140)), tmDim(Color(255, 100, 130, 160));
    SolidBrush sFill(Color(255, 80, 180, 240)); Pen btnP(Color(255, 100, 170, 220));

    // 路径 + 状态 靠上
    g.DrawString(g_autoexecPath.c_str(), -1, &sF, PointF(cx + 10, 50), &tmDim);
    wchar_t sb[64]; swprintf_s(sb, L"状态: %s", g_autoexecStatus.c_str());
    g.DrawString(sb, -1, &sF, PointF(cx + 10, 65), g_autoexecStatus == L"已保存" ? &tbCol : &tdCol);

    // 编辑框占 1/4 高度
    int totalAvail = H - 85;
    int editH = totalAvail / 4;
    if (editH < 120) editH = 120;
    int editW = cw - 20;
    int editY = 85;
    g_editRect = RectF((REAL)(cx + 10), (REAL)editY, (REAL)editW, (REAL)editH);

    SolidBrush editBg(Color(255, 250, 250, 255)); Pen editPen(Color(255, 180, 200, 220));
    g.FillRectangle(&editBg, g_editRect); g.DrawRectangle(&editPen, g_editRect);

    // 绘制编辑内容
    const std::wstring& display = g_editing ? g_editBuffer : g_autoexecContent;
    std::wstringstream ss(display);
    std::wstring line; int lineY = editY + 4; int lineNum = 0;
    int cursorLine = 0, cursorCol = 0, tempPos = 0;
    while (std::getline(ss, line)) {
        if (lineY > editY + editH - 20) break;
        g.DrawString(line.c_str(), -1, &sF, PointF(cx + 14, lineY), &tdCol);
        // 追踪光标位置
        if (g_editing && tempPos <= g_cursorPos && g_cursorPos <= tempPos + (int)line.length()) {
            cursorLine = lineY; cursorCol = g_cursorPos - tempPos;
        }
        tempPos += (int)line.length() + 1;
        lineY += 18; lineNum++;
    }
    // 画光标
    if (g_editing) {
        SolidBrush cursorBrush(Color(255, 0, 0, 0));
        g.FillRectangle(&cursorBrush, cx + 14 + cursorCol * 7, cursorLine, 2, 16);
    }

    // 按钮区域在编辑框下方
    int btnY = editY + editH + 10, btnW = 80, btnH = 26;
    
    // 保存按钮
    g_saveBtnRect = RectF((REAL)(cx + 10), (REAL)btnY, (REAL)btnW, (REAL)btnH);
    GraphicsPath sp; sp.AddArc(cx + 10, btnY, 16, 16, 180, 90);
    sp.AddArc(cx + 10 + btnW - 16, btnY, 16, 16, 270, 90);
    sp.AddArc(cx + 10 + btnW - 16, btnY + btnH - 16, 16, 16, 0, 90);
    sp.AddArc(cx + 10, btnY + btnH - 16, 16, 16, 90, 90); sp.CloseFigure();
    g.FillPath(&sFill, &sp); g.DrawPath(&btnP, &sp);
    g.DrawString(L"保存", -1, &sF, PointF(cx + 10 + 20, btnY + 6), &tbCol);

    // 刷新按钮
    g_reloadBtnRect = RectF((REAL)(cx + 10 + btnW + 10), (REAL)btnY, (REAL)btnW, (REAL)btnH);
    GraphicsPath rp; int rx = cx + 10 + btnW + 10;
    rp.AddArc(rx, btnY, 16, 16, 180, 90); rp.AddArc(rx + btnW - 16, btnY, 16, 16, 270, 90);
    rp.AddArc(rx + btnW - 16, btnY + btnH - 16, 16, 16, 0, 90);
    rp.AddArc(rx, btnY + btnH - 16, 16, 16, 90, 90); rp.CloseFigure();
    SolidBrush rlBg(Color(255, 140, 200, 240));
    g.FillPath(&rlBg, &rp); g.DrawPath(&btnP, &rp);
    g.DrawString(L"刷新", -1, &sF, PointF(rx + 20, btnY + 6), &tbCol);

    // 写入SOCD按钮
    int socdX = rx + btnW + 10;
    g_socdBtnRect = RectF((REAL)socdX, (REAL)btnY, (REAL)(btnW + 30), (REAL)btnH);
    GraphicsPath socdP; 
    socdP.AddArc(socdX, btnY, 16, 16, 180, 90);
    socdP.AddArc(socdX + btnW + 30 - 16, btnY, 16, 16, 270, 90);
    socdP.AddArc(socdX + btnW + 30 - 16, btnY + btnH - 16, 16, 16, 0, 90);
    socdP.AddArc(socdX, btnY + btnH - 16, 16, 16, 90, 90); socdP.CloseFigure();
    SolidBrush socdBg(Color(255, 200, 160, 80));
    g.FillPath(&socdBg, &socdP); g.DrawPath(&btnP, &socdP);
    g.DrawString(L"写入SOCD", -1, &sF, PointF(socdX + 12, btnY + 6), &tbCol);

    // 提示：点击编辑框直接编辑
    int promptY = btnY + btnH + 10;
    g.DrawString(L"点击编辑框可直接编辑", -1, &sF, PointF(cx + 10, promptY), &tmDim);
}

void CheckLegalCfgClick(HWND hw, int mx, int my) {
    // 保存按钮
    if (mx >= g_saveBtnRect.X && mx <= g_saveBtnRect.X + g_saveBtnRect.Width &&
        my >= g_saveBtnRect.Y && my <= g_saveBtnRect.Y + g_saveBtnRect.Height) {
        if (g_editing) {
            g_autoexecContent = g_editBuffer;
            g_editing = false;
        }
        SaveAutoexecContent(g_autoexecContent);
        InvalidateRect(hw, nullptr, FALSE); return;
    }
    // 刷新按钮
    if (mx >= g_reloadBtnRect.X && mx <= g_reloadBtnRect.X + g_reloadBtnRect.Width &&
        my >= g_reloadBtnRect.Y && my <= g_reloadBtnRect.Y + g_reloadBtnRect.Height) {
        g_lastWriteTime = 0; LoadAutoexecContent();
        g_editing = false; 
        InvalidateRect(hw, nullptr, FALSE); return;
    }
    // 写入SOCD按钮
    if (mx >= g_socdBtnRect.X && mx <= g_socdBtnRect.X + g_socdBtnRect.Width &&
        my >= g_socdBtnRect.Y && my <= g_socdBtnRect.Y + g_socdBtnRect.Height) {
        AppendToFile(std::wstring(SOCD_BLOCK));
        g_editing = false;
        InvalidateRect(hw, nullptr, FALSE); return;
    }
    // 点击编辑框进入编辑模式
    if (mx >= g_editRect.X && mx <= g_editRect.X + g_editRect.Width &&
        my >= g_editRect.Y && my <= g_editRect.Y + g_editRect.Height) {
        if (!g_editing) {
            g_editing = true;
            g_editBuffer = g_autoexecContent;
            g_cursorPos = (int)g_editBuffer.length();
        }
        // 请求焦点以接收键盘输入
        SetFocus(hw);
        InvalidateRect(hw, nullptr, FALSE);
    }
}

// 处理键盘输入（由 StrikeSense.cpp WndProc WM_CHAR/WM_KEYDOWN 调用）
bool ProcessLegalCfgKeyInput(HWND hw, UINT msg, WPARAM wp, LPARAM lp) {
    if (!g_editing) return false;
    
    switch (msg) {
    case WM_CHAR: {
        wchar_t ch = (wchar_t)wp;
        if (ch >= 32 && ch <= 126) {  // 可打印ASCII
            g_editBuffer.insert(g_cursorPos, 1, ch);
            g_cursorPos++;
            InvalidateRect(hw, nullptr, FALSE);
            return true;
        }
        if (ch == 13) { // Enter
            g_editBuffer.insert(g_cursorPos, 1, L'\n');
            g_cursorPos++;
            InvalidateRect(hw, nullptr, FALSE);
            return true;
        }
        if (ch == 9) { // Tab
            g_editBuffer.insert(g_cursorPos, 4, L' ');
            g_cursorPos += 4;
            InvalidateRect(hw, nullptr, FALSE);
            return true;
        }
        break;
    }
    case WM_KEYDOWN: {
        UINT vk = (UINT)wp;
        if (vk == VK_BACK && g_cursorPos > 0) {
            g_editBuffer.erase(g_cursorPos - 1, 1);
            g_cursorPos--;
            InvalidateRect(hw, nullptr, FALSE);
            return true;
        }
        if (vk == VK_DELETE && g_cursorPos < (int)g_editBuffer.length()) {
            g_editBuffer.erase(g_cursorPos, 1);
            InvalidateRect(hw, nullptr, FALSE);
            return true;
        }
        if (vk == VK_LEFT && g_cursorPos > 0) {
            g_cursorPos--;
            InvalidateRect(hw, nullptr, FALSE);
            return true;
        }
        if (vk == VK_RIGHT && g_cursorPos < (int)g_editBuffer.length()) {
            g_cursorPos++;
            InvalidateRect(hw, nullptr, FALSE);
            return true;
        }
        if (vk == VK_HOME) {
            g_cursorPos = 0;
            InvalidateRect(hw, nullptr, FALSE);
            return true;
        }
        if (vk == VK_END) {
            g_cursorPos = (int)g_editBuffer.length();
            InvalidateRect(hw, nullptr, FALSE);
            return true;
        }
        break;
    }
    }
    return false;
}

void InitLegalCfgPage() { 
    g_lastWriteTime = 0; 
    g_editing = false;
    LoadAutoexecContent(); 
    g_editBuffer = g_autoexecContent;
}