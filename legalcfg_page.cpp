#include "pages.h"
#include "steam_helper.h"
#include <fstream>
#include <sstream>
#include <filesystem>

namespace fs = std::filesystem;

static std::wstring g_autoexecPath;
static std::wstring g_autoexecContent;
static std::wstring g_editBuffer;
static std::wstring g_autoexecStatus = L"未加载";
static ULONGLONG g_lastWriteTime = 0;
static bool g_editing = false;
static int g_cursorPos = 0;
static int g_scrollOffset = 0;
static int g_visibleLines = 0;

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

static const wchar_t* MWHEELJUMP_BLOCK = LR"(
//--StrikeSense MwheelJump--
bind mouse_wheel +jump
//--StrikeSense MwheelJump END--
)";

// ===== 混合灵敏度 =====
static std::wstring g_msNormal = L"2.5";
static std::wstring g_msAttack = L"1.0";
static bool g_editingMSnormal = false;
static bool g_editingMSattack = false;

static std::wstring BuildMSBlock() {
    std::wstring block = L"\n//--StrikeSense MS--\n";
    block += L"alias SSMS_S1 \"sensitivity " + g_msNormal + L"\"\n";
    block += L"alias SSMS_S2 \"sensitivity " + g_msAttack + L"\"\n";
    block += L"alias +SS_attack \"+attack;SSMS_S2;spec_next\"\n";
    block += L"alias -SS_attack \"-attack;SSMS_S1\"\n";
    block += L"\n//Edit Binds Here\n";
    block += L"bind mouse1 +SS_attack\n";
    block += L"//--StrikeSense MS END--\n";
    return block;
}

// ===== 移除区块并清理多余换行 =====
static bool RemoveBlock(std::wstring& content, const std::wstring& startMarker, const std::wstring& endMarker) {
    size_t start = content.find(startMarker);
    if (start == std::string::npos) return false;
    size_t end = content.find(endMarker, start);
    if (end == std::string::npos) return false;
    size_t endEnd = end + endMarker.length();
    // 向前清理多余换行
    while (start > 0 && (content[start - 1] == L'\n' || content[start - 1] == L'\r')) start--;
    // 向后清理多余换行
    while (endEnd < content.length() && (content[endEnd] == L'\n' || content[endEnd] == L'\r')) endEnd++;
    content.erase(start, endEnd - start);
    return true;
}

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

static std::vector<std::wstring> ContentToLines(const std::wstring& content) {
    std::vector<std::wstring> lines;
    std::wstringstream ss(content);
    std::wstring line;
    while (std::getline(ss, line)) lines.push_back(line);
    return lines;
}

static int PosFromLineCol(const std::vector<std::wstring>& lines, int line, int col) {
    int pos = 0;
    for (int i = 0; i < line && i < (int)lines.size(); i++) pos += (int)lines[i].length() + 1;
    if (col < 0) col = 0;
    if (line < (int)lines.size() && col > (int)lines[line].length()) col = (int)lines[line].length();
    return pos + col;
}

static void LineColFromPos(const std::vector<std::wstring>& lines, int pos, int& outLine, int& outCol) {
    outLine = 0; outCol = 0; int p = 0;
    for (int i = 0; i < (int)lines.size(); i++) {
        if (pos <= p + (int)lines[i].length()) { outLine = i; outCol = pos - p; return; }
        p += (int)lines[i].length() + 1;
    }
    outLine = ((int)lines.size() - 1 > 0) ? (int)lines.size() - 1 : 0;
    outCol = lines.empty() ? 0 : (int)lines.back().length();
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

static bool RemoveBlockAndSave(const std::wstring& startMarker, const std::wstring& endMarker) {
    std::wstring content = g_autoexecContent;
    bool found = RemoveBlock(content, startMarker, endMarker);
    if (found) { g_autoexecContent = content; g_editBuffer = content; SaveAutoexecContent(content); }
    return found;
}

static void EnsureCursorVisible(const std::vector<std::wstring>& lines) {
    int cursorLine, cursorCol; LineColFromPos(lines, g_cursorPos, cursorLine, cursorCol);
    if (cursorLine < g_scrollOffset) g_scrollOffset = cursorLine;
    if (cursorLine >= g_scrollOffset + g_visibleLines) g_scrollOffset = cursorLine - g_visibleLines + 1;
    if (g_scrollOffset < 0) g_scrollOffset = 0;
}

constexpr int LINE_H = 18;

static Gdiplus::RectF g_editRect;
static Gdiplus::RectF g_saveBtnRect, g_reloadBtnRect;
static Gdiplus::RectF g_socdBtnRect, g_removeSocdBtnRect;
static Gdiplus::RectF g_mwheelBtnRect, g_removeMwheelBtnRect;
static Gdiplus::RectF g_msNormalRect, g_msAttackRect;
static Gdiplus::RectF g_msWriteBtnRect, g_msRemoveBtnRect;

void PaintLegalCfgPage(Gdiplus::Graphics& g, int cx, int cw, int H, HWND) {
    using namespace Gdiplus;
    ui::DrawHeader(g, cx, cw, L"合法配置");
    Font sF(L"Microsoft YaHei", 9);
    SolidBrush tdCol(Color(255, 30, 60, 100)), tbCol(Color(255, 20, 80, 140)), tmDim(Color(255, 100, 130, 160));
    SolidBrush btnB(Color(255, 200, 230, 250)); // 更浅的淡蓝色（白多蓝少）
    Pen btnP(Color(255, 150, 190, 220));

    g.DrawString(g_autoexecPath.c_str(), -1, &sF, PointF(cx + 10, 50), &tmDim);
    wchar_t sb[64]; swprintf_s(sb, L"状态: %s", g_autoexecStatus.c_str());
    g.DrawString(sb, -1, &sF, PointF(cx + 10, 65), g_autoexecStatus == L"已保存" ? &tbCol : &tdCol);

    // 编辑框 1/4
    int totalAvail = H - 85;
    int editH = totalAvail / 4;
    if (editH < 120) editH = 120;
    int editW = cw - 20, editY = 85;
    g_editRect = RectF((REAL)(cx + 10), (REAL)editY, (REAL)editW, (REAL)editH);
    SolidBrush editBg(Color(255, 250, 250, 255)); Pen editPen(Color(255, 180, 200, 220));
    g.FillRectangle(&editBg, g_editRect); g.DrawRectangle(&editPen, g_editRect);
    g_visibleLines = (editH - 4) / LINE_H;
    const std::wstring& display = g_editing ? g_editBuffer : g_autoexecContent;
    auto lines = ContentToLines(display);
    if (g_editing) EnsureCursorVisible(lines);
    int lineY = editY + 4, drawLine = 0;
    for (int i = g_scrollOffset; i < (int)lines.size() && drawLine < g_visibleLines; i++, drawLine++) {
        g.DrawString(lines[i].c_str(), -1, &sF, PointF(cx + 14, lineY), &tdCol);
        lineY += LINE_H;
    }
    if (g_editing) {
        int cursorLine, cursorCol; LineColFromPos(lines, g_cursorPos, cursorLine, cursorCol);
        int vr = cursorLine - g_scrollOffset;
        if (vr >= 0 && vr < g_visibleLines) {
            SolidBrush cursorBrush(Color(255, 0, 0, 0));
            g.FillRectangle(&cursorBrush, cx + 14 + cursorCol * 7, editY + 4 + vr * LINE_H, 2, LINE_H);
        }
    }

    // 第一排：保存 | 刷新 | 提示文字
    int btnY = editY + editH + 10, btnW = 80, btnH = 26;
    g_saveBtnRect = RectF((REAL)(cx + 10), (REAL)btnY, (REAL)btnW, (REAL)btnH);
    GraphicsPath sp; sp.AddArc(cx + 10, btnY, 16, 16, 180, 90);
    sp.AddArc(cx + 10 + btnW - 16, btnY, 16, 16, 270, 90);
    sp.AddArc(cx + 10 + btnW - 16, btnY + btnH - 16, 16, 16, 0, 90);
    sp.AddArc(cx + 10, btnY + btnH - 16, 16, 16, 90, 90); sp.CloseFigure();
    g.FillPath(&btnB, &sp); g.DrawPath(&btnP, &sp);
    g.DrawString(L"保存", -1, &sF, PointF(cx + 10 + 20, btnY + 6), &tbCol);

    g_reloadBtnRect = RectF((REAL)(cx + 10 + btnW + 10), (REAL)btnY, (REAL)btnW, (REAL)btnH);
    GraphicsPath rp; int rx = cx + 10 + btnW + 10;
    rp.AddArc(rx, btnY, 16, 16, 180, 90); rp.AddArc(rx + btnW - 16, btnY, 16, 16, 270, 90);
    rp.AddArc(rx + btnW - 16, btnY + btnH - 16, 16, 16, 0, 90);
    rp.AddArc(rx, btnY + btnH - 16, 16, 16, 90, 90); rp.CloseFigure();
    g.FillPath(&btnB, &rp); g.DrawPath(&btnP, &rp);
    g.DrawString(L"刷新", -1, &sF, PointF(rx + 20, btnY + 6), &tbCol);

    // 提示文字放在刷新按钮右边
    g.DrawString(L"点击编辑框编辑 | PageUp/Down翻页 | 鼠标点击移动光标", -1, &sF, PointF(rx + btnW + 20, btnY + 6), &tmDim);

    // ===== 功能按钮区域 =====
    int fnY = btnY + btnH + 8, fnW = 100, fnH = 26;

    // SOCD 行
    g_socdBtnRect = RectF((REAL)(cx + 10), (REAL)fnY, (REAL)fnW, (REAL)fnH);
    GraphicsPath socdP; socdP.AddArc(cx + 10, fnY, 16, 16, 180, 90);
    socdP.AddArc(cx + 10 + fnW - 16, fnY, 16, 16, 270, 90);
    socdP.AddArc(cx + 10 + fnW - 16, fnY + fnH - 16, 16, 16, 0, 90);
    socdP.AddArc(cx + 10, fnY + fnH - 16, 16, 16, 90, 90); socdP.CloseFigure();
    g.FillPath(&btnB, &socdP); g.DrawPath(&btnP, &socdP);
    g.DrawString(L"写入SOCD", -1, &sF, PointF(cx + 18, fnY + 6), &tbCol);

    g_removeSocdBtnRect = RectF((REAL)(cx + 10 + fnW + 10), (REAL)fnY, (REAL)fnW, (REAL)fnH);
    GraphicsPath rsP; int rsx = cx + 10 + fnW + 10;
    rsP.AddArc(rsx, fnY, 16, 16, 180, 90); rsP.AddArc(rsx + fnW - 16, fnY, 16, 16, 270, 90);
    rsP.AddArc(rsx + fnW - 16, fnY + fnH - 16, 16, 16, 0, 90);
    rsP.AddArc(rsx, fnY + fnH - 16, 16, 16, 90, 90); rsP.CloseFigure();
    g.FillPath(&btnB, &rsP); g.DrawPath(&btnP, &rsP);
    g.DrawString(L"移除SOCD", -1, &sF, PointF(rsx + 12, fnY + 6), &tbCol);

    // 滚轮跳行
    int fnY2 = fnY + fnH + 6;
    g_mwheelBtnRect = RectF((REAL)(cx + 10), (REAL)fnY2, (REAL)fnW, (REAL)fnH);
    GraphicsPath mwP; mwP.AddArc(cx + 10, fnY2, 16, 16, 180, 90);
    mwP.AddArc(cx + 10 + fnW - 16, fnY2, 16, 16, 270, 90);
    mwP.AddArc(cx + 10 + fnW - 16, fnY2 + fnH - 16, 16, 16, 0, 90);
    mwP.AddArc(cx + 10, fnY2 + fnH - 16, 16, 16, 90, 90); mwP.CloseFigure();
    g.FillPath(&btnB, &mwP); g.DrawPath(&btnP, &mwP);
    g.DrawString(L"写入滚轮跳", -1, &sF, PointF(cx + 12, fnY2 + 6), &tbCol);

    g_removeMwheelBtnRect = RectF((REAL)(cx + 10 + fnW + 10), (REAL)fnY2, (REAL)fnW, (REAL)fnH);
    GraphicsPath rmP; int rmx = cx + 10 + fnW + 10;
    rmP.AddArc(rmx, fnY2, 16, 16, 180, 90); rmP.AddArc(rmx + fnW - 16, fnY2, 16, 16, 270, 90);
    rmP.AddArc(rmx + fnW - 16, fnY2 + fnH - 16, 16, 16, 0, 90);
    rmP.AddArc(rmx, fnY2 + fnH - 16, 16, 16, 90, 90); rmP.CloseFigure();
    g.FillPath(&btnB, &rmP); g.DrawPath(&btnP, &rmP);
    g.DrawString(L"移除滚轮跳", -1, &sF, PointF(rmx + 8, fnY2 + 6), &tbCol);

    // 混合灵敏度行：按钮在前，输入框在后
    int fnY3 = fnY2 + fnH + 6, msBtnW = 120;

    g_msWriteBtnRect = RectF((REAL)(cx + 10), (REAL)fnY3, (REAL)msBtnW, (REAL)fnH);
    GraphicsPath msP; msP.AddArc(cx + 10, fnY3, 16, 16, 180, 90);
    msP.AddArc(cx + 10 + msBtnW - 16, fnY3, 16, 16, 270, 90);
    msP.AddArc(cx + 10 + msBtnW - 16, fnY3 + fnH - 16, 16, 16, 0, 90);
    msP.AddArc(cx + 10, fnY3 + fnH - 16, 16, 16, 90, 90); msP.CloseFigure();
    g.FillPath(&btnB, &msP); g.DrawPath(&btnP, &msP);
    g.DrawString(L"写入混合灵敏度", -1, &sF, PointF(cx + 18, fnY3 + 6), &tbCol);

    g_msRemoveBtnRect = RectF((REAL)(cx + 10 + msBtnW + 10), (REAL)fnY3, (REAL)msBtnW, (REAL)fnH);
    GraphicsPath msrP; int msrx = cx + 10 + msBtnW + 10;
    msrP.AddArc(msrx, fnY3, 16, 16, 180, 90); msrP.AddArc(msrx + msBtnW - 16, fnY3, 16, 16, 270, 90);
    msrP.AddArc(msrx + msBtnW - 16, fnY3 + fnH - 16, 16, 16, 0, 90);
    msrP.AddArc(msrx, fnY3 + fnH - 16, 16, 16, 90, 90); msrP.CloseFigure();
    g.FillPath(&btnB, &msrP); g.DrawPath(&btnP, &msrP);
    g.DrawString(L"移除混合灵敏度", -1, &sF, PointF(msrx + 8, fnY3 + 6), &tbCol);

    // 输入框在按钮后面
    int inpX = msrx + msBtnW + 20, inpW = 60;
    SolidBrush inpBg(Color(255, 250, 250, 255)); Pen inpPen(Color(255, 180, 200, 220));

    g.DrawString(L"常规:", -1, &sF, PointF(inpX, fnY3 + 4), &tdCol);
    g_msNormalRect = RectF((REAL)(inpX + 35), (REAL)fnY3, (REAL)inpW, (REAL)fnH);
    g.FillRectangle(&inpBg, g_msNormalRect); g.DrawRectangle(&inpPen, g_msNormalRect);
    g.DrawString(g_msNormal.c_str(), -1, &sF, PointF(inpX + 39, fnY3 + 4), &tdCol);
    // 常规输入框光标
    if (g_editingMSnormal) {
        SolidBrush cursorC(Color(255, 0, 0, 0));
        int cxPos = inpX + 39 + (int)g_msNormal.length() * 7;
        g.FillRectangle(&cursorC, cxPos, fnY3 + 2, 2, 22);
    }

    int atkX = inpX + 35 + inpW + 15;
    g.DrawString(L"开火:", -1, &sF, PointF(atkX, fnY3 + 4), &tdCol);
    g_msAttackRect = RectF((REAL)(atkX + 35), (REAL)fnY3, (REAL)inpW, (REAL)fnH);
    g.FillRectangle(&inpBg, g_msAttackRect); g.DrawRectangle(&inpPen, g_msAttackRect);
    g.DrawString(g_msAttack.c_str(), -1, &sF, PointF(atkX + 39, fnY3 + 4), &tdCol);
    // 开火输入框光标
    if (g_editingMSattack) {
        SolidBrush cursorC(Color(255, 0, 0, 0));
        int cxPos = atkX + 39 + (int)g_msAttack.length() * 7;
        g.FillRectangle(&cursorC, cxPos, fnY3 + 2, 2, 22);
    }
}

void CheckLegalCfgClick(HWND hw, int mx, int my) {
    if (mx >= g_saveBtnRect.X && mx <= g_saveBtnRect.X + g_saveBtnRect.Width &&
        my >= g_saveBtnRect.Y && my <= g_saveBtnRect.Y + g_saveBtnRect.Height) {
        if (g_editing) { g_autoexecContent = g_editBuffer; g_editing = false; }
        SaveAutoexecContent(g_autoexecContent); InvalidateRect(hw, nullptr, FALSE); return;
    }
    if (mx >= g_reloadBtnRect.X && mx <= g_reloadBtnRect.X + g_reloadBtnRect.Width &&
        my >= g_reloadBtnRect.Y && my <= g_reloadBtnRect.Y + g_reloadBtnRect.Height) {
        g_lastWriteTime = 0; LoadAutoexecContent(); g_editing = false; InvalidateRect(hw, nullptr, FALSE); return;
    }
    if (mx >= g_socdBtnRect.X && mx <= g_socdBtnRect.X + g_socdBtnRect.Width &&
        my >= g_socdBtnRect.Y && my <= g_socdBtnRect.Y + g_socdBtnRect.Height) {
        AppendToFile(std::wstring(SOCD_BLOCK)); g_editing = false; InvalidateRect(hw, nullptr, FALSE); return;
    }
    if (mx >= g_removeSocdBtnRect.X && mx <= g_removeSocdBtnRect.X + g_removeSocdBtnRect.Width &&
        my >= g_removeSocdBtnRect.Y && my <= g_removeSocdBtnRect.Y + g_removeSocdBtnRect.Height) {
        bool found = RemoveBlockAndSave(L"//--StrikeSense SOCD--", L"//--StrikeSense SOCD END--");
        g_autoexecStatus = found ? L"已移除SOCD" : L"未找到SOCD"; g_editing = false; InvalidateRect(hw, nullptr, FALSE); return;
    }
    if (mx >= g_mwheelBtnRect.X && mx <= g_mwheelBtnRect.X + g_mwheelBtnRect.Width &&
        my >= g_mwheelBtnRect.Y && my <= g_mwheelBtnRect.Y + g_mwheelBtnRect.Height) {
        AppendToFile(std::wstring(MWHEELJUMP_BLOCK)); g_editing = false; InvalidateRect(hw, nullptr, FALSE); return;
    }
    if (mx >= g_removeMwheelBtnRect.X && mx <= g_removeMwheelBtnRect.X + g_removeMwheelBtnRect.Width &&
        my >= g_removeMwheelBtnRect.Y && my <= g_removeMwheelBtnRect.Y + g_removeMwheelBtnRect.Height) {
        bool found = RemoveBlockAndSave(L"//--StrikeSense MwheelJump--", L"//--StrikeSense MwheelJump END--");
        g_autoexecStatus = found ? L"已移除滚轮跳" : L"未找到滚轮跳"; g_editing = false; InvalidateRect(hw, nullptr, FALSE); return;
    }
    if (mx >= g_msWriteBtnRect.X && mx <= g_msWriteBtnRect.X + g_msWriteBtnRect.Width &&
        my >= g_msWriteBtnRect.Y && my <= g_msWriteBtnRect.Y + g_msWriteBtnRect.Height) {
        AppendToFile(BuildMSBlock()); g_editing = false; InvalidateRect(hw, nullptr, FALSE); return;
    }
    if (mx >= g_msRemoveBtnRect.X && mx <= g_msRemoveBtnRect.X + g_msRemoveBtnRect.Width &&
        my >= g_msRemoveBtnRect.Y && my <= g_msRemoveBtnRect.Y + g_msRemoveBtnRect.Height) {
        bool found = RemoveBlockAndSave(L"//--StrikeSense MS--", L"//--StrikeSense MS END--");
        g_autoexecStatus = found ? L"已移除混合灵敏度" : L"未找到混合灵敏度";
        g_editing = false; InvalidateRect(hw, nullptr, FALSE); return;
    }
    // 点击常规输入框
    if (mx >= g_msNormalRect.X && mx <= g_msNormalRect.X + g_msNormalRect.Width &&
        my >= g_msNormalRect.Y && my <= g_msNormalRect.Y + g_msNormalRect.Height) {
        g_editingMSnormal = true; g_editingMSattack = false; g_editing = false; SetFocus(hw); InvalidateRect(hw, nullptr, FALSE); return;
    }
    // 点击开火输入框
    if (mx >= g_msAttackRect.X && mx <= g_msAttackRect.X + g_msAttackRect.Width &&
        my >= g_msAttackRect.Y && my <= g_msAttackRect.Y + g_msAttackRect.Height) {
        g_editingMSattack = true; g_editingMSnormal = false; g_editing = false; SetFocus(hw); InvalidateRect(hw, nullptr, FALSE); return;
    }
    // 点击编辑框
    if (mx >= g_editRect.X && mx <= g_editRect.X + g_editRect.Width &&
        my >= g_editRect.Y && my <= g_editRect.Y + g_editRect.Height) {
        g_editingMSnormal = false; g_editingMSattack = false;
        if (!g_editing) { g_editing = true; g_editBuffer = g_autoexecContent; }
        int relY = my - (int)g_editRect.Y - 4;
        int relX = mx - (int)g_editRect.X - 14;
        int clickedLine = relY / LINE_H + g_scrollOffset;
        int clickedCol = (relX / 7 > 0) ? relX / 7 : 0;
        auto lines = ContentToLines(g_editBuffer);
        if (clickedLine < 0) clickedLine = 0;
        if (clickedLine >= (int)lines.size()) { clickedLine = (int)lines.size() - 1; clickedCol = (int)lines[clickedLine].length(); }
        g_cursorPos = PosFromLineCol(lines, clickedLine, clickedCol);
        SetFocus(hw); InvalidateRect(hw, nullptr, FALSE);
    }
}

bool ProcessLegalCfgKeyInput(HWND hw, UINT msg, WPARAM wp, LPARAM lp) {
    if (g_editing) {
        auto lines = ContentToLines(g_editBuffer);
        switch (msg) {
        case WM_CHAR: {
            wchar_t ch = (wchar_t)wp;
            if (ch >= 32 && ch <= 126) { g_editBuffer.insert(g_cursorPos, 1, ch); g_cursorPos++; InvalidateRect(hw, nullptr, FALSE); return true; }
            if (ch == 13) { g_editBuffer.insert(g_cursorPos, 1, L'\n'); g_cursorPos++; InvalidateRect(hw, nullptr, FALSE); return true; }
            if (ch == 9) { g_editBuffer.insert(g_cursorPos, 4, L' '); g_cursorPos += 4; InvalidateRect(hw, nullptr, FALSE); return true; }
            return false;
        }
        case WM_KEYDOWN: {
            UINT vk = (UINT)wp;
            if (vk == VK_BACK && g_cursorPos > 0) { g_editBuffer.erase(g_cursorPos - 1, 1); g_cursorPos--; InvalidateRect(hw, nullptr, FALSE); return true; }
            if (vk == VK_DELETE && g_cursorPos < (int)g_editBuffer.length()) { g_editBuffer.erase(g_cursorPos, 1); InvalidateRect(hw, nullptr, FALSE); return true; }
            if (vk == VK_LEFT && g_cursorPos > 0) { g_cursorPos--; InvalidateRect(hw, nullptr, FALSE); return true; }
            if (vk == VK_RIGHT && g_cursorPos < (int)g_editBuffer.length()) { g_cursorPos++; InvalidateRect(hw, nullptr, FALSE); return true; }
            if (vk == VK_UP) { int cl, cc; LineColFromPos(lines, g_cursorPos, cl, cc); if (cl > 0) g_cursorPos = PosFromLineCol(lines, cl - 1, cc); InvalidateRect(hw, nullptr, FALSE); return true; }
            if (vk == VK_DOWN) { int cl, cc; LineColFromPos(lines, g_cursorPos, cl, cc); if (cl < (int)lines.size() - 1) g_cursorPos = PosFromLineCol(lines, cl + 1, cc); InvalidateRect(hw, nullptr, FALSE); return true; }
            if (vk == VK_HOME) { int cl, cc; LineColFromPos(lines, g_cursorPos, cl, cc); g_cursorPos = PosFromLineCol(lines, cl, 0); InvalidateRect(hw, nullptr, FALSE); return true; }
            if (vk == VK_END) { int cl, cc; LineColFromPos(lines, g_cursorPos, cl, cc); g_cursorPos = PosFromLineCol(lines, cl, (int)lines[cl].length()); InvalidateRect(hw, nullptr, FALSE); return true; }
            if (vk == VK_PRIOR) { g_scrollOffset -= g_visibleLines; if (g_scrollOffset < 0) g_scrollOffset = 0; int cl, cc; LineColFromPos(lines, g_cursorPos, cl, cc); cl -= g_visibleLines; if (cl < 0) cl = 0; g_cursorPos = PosFromLineCol(lines, cl, cc); InvalidateRect(hw, nullptr, FALSE); return true; }
            if (vk == VK_NEXT) { g_scrollOffset += g_visibleLines; int cl, cc; LineColFromPos(lines, g_cursorPos, cl, cc); cl += g_visibleLines; if (cl >= (int)lines.size()) cl = (int)lines.size() - 1; g_cursorPos = PosFromLineCol(lines, cl, cc); InvalidateRect(hw, nullptr, FALSE); return true; }
            return false;
        }
        }
        return false;
    }

    if (g_editingMSnormal || g_editingMSattack) {
        std::wstring* target = g_editingMSnormal ? &g_msNormal : &g_msAttack;
        if (msg == WM_CHAR) {
            wchar_t ch = (wchar_t)wp;
            if ((ch >= '0' && ch <= '9') || ch == '.') { *target += ch; InvalidateRect(hw, nullptr, FALSE); return true; }
            if (ch == 8 && !target->empty()) { target->pop_back(); InvalidateRect(hw, nullptr, FALSE); return true; }
            return false;
        }
        if (msg == WM_KEYDOWN) {
            UINT vk = (UINT)wp;
            if (vk == VK_BACK && !target->empty()) { target->pop_back(); InvalidateRect(hw, nullptr, FALSE); return true; }
            if (vk == VK_TAB) { // 切换输入框焦点
                if (g_editingMSnormal) { g_editingMSnormal = false; g_editingMSattack = true; }
                else { g_editingMSattack = false; g_editingMSnormal = true; }
                InvalidateRect(hw, nullptr, FALSE); return true;
            }
            return false;
        }
        return true;
    }
    return false;
}

void InitLegalCfgPage() {
    g_lastWriteTime = 0; g_editing = false; g_editingMSnormal = false; g_editingMSattack = false;
    LoadAutoexecContent(); g_editBuffer = g_autoexecContent;
}