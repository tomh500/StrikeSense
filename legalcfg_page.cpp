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

static std::wstring g_msNormal = L"1.0";
static std::wstring g_msAttack = L"1.25";
static bool g_editingMSnormal = false;
static bool g_editingMSattack = false;

static void ParseMSParams(const std::wstring& content) {
    std::wstring s1 = L"SSMS_S1 \"sensitivity ";
    size_t p1 = content.find(s1);
    if (p1 != std::string::npos) {
        size_t end = content.find(L"\"", p1 + s1.length());
        if (end != std::string::npos) g_msNormal = content.substr(p1 + s1.length(), end - p1 - s1.length());
    }
    std::wstring s2 = L"SSMS_S2 \"sensitivity ";
    size_t p2 = content.find(s2);
    if (p2 != std::string::npos) {
        size_t end = content.find(L"\"", p2 + s2.length());
        if (end != std::string::npos) g_msAttack = content.substr(p2 + s2.length(), end - p2 - s2.length());
    }
}

static std::wstring BuildMSBlock() {
    std::wstring block = L"\n//--StrikeSense MS--\n";
    block += L"alias SSMS_S1 \"sensitivity " + g_msNormal + L"\"\n";
    block += L"alias SSMS_S2 \"sensitivity " + g_msAttack + L"\"\n";
    block += L"alias +SS_attack \"+attack;SSMS_S2;spec_next\"\n";
    block += L"alias -SS_attack \"-attack;SSMS_S1\"\n";
    block += L"\nbind mouse1 +SS_attack\n";
    block += L"//--StrikeSense MS END--\n";
    return block;
}

static int g_crosshairSWMode = 0;
static bool g_crosshairSWDropdownOpen = false;
static Gdiplus::RectF g_crosshairSWDropdownRect;
static const wchar_t* CROSSHAIR_SW_MODES[] = { L"capslock", L"custom" };
static const int CROSSHAIR_SW_COUNT = 2;

static std::wstring BuildCrosshairSWBlock() {
    std::wstring block = L"\n//--StrikeSense CrosshairSW--\n";
    if (g_crosshairSWMode == 0)
        block += L"bind capslock \"toggle cl_crosshair_recoil 0 1\"\n";
    else {
        block += L"// custom - 请自行修改配置文件中的绑定\n";
        block += L"bind F2 \"toggle cl_crosshair_recoil 0 1\"\n";
    }
    block += L"//--StrikeSense CrosshairSW END--\n";
    return block;
}

static bool RemoveBlock(std::wstring& content, const std::wstring& startMarker, const std::wstring& endMarker) {
    size_t start = content.find(startMarker);
    if (start == std::string::npos) return false;
    size_t end = content.find(endMarker, start);
    if (end == std::string::npos) return false;
    size_t endEnd = end + endMarker.length();
    while (start > 0 && (content[start - 1] == L'\n' || content[start - 1] == L'\r')) start--;
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
    if (g_autoexecPath.empty()) { g_autoexecContent = L"// 无法找到 autoexec.cfg - 请先设置CS2配置目录"; g_autoexecStatus = L"路径错误"; return; }
    if (!fs::exists(g_autoexecPath)) { g_autoexecContent = L"// autoexec.cfg 不存在\n// 保存时将创建新文件"; g_autoexecStatus = L"新文件"; g_lastWriteTime = 0; return; }
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
    ParseMSParams(g_autoexecContent);
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
    g_autoexecContent += text; g_editBuffer = g_autoexecContent; SaveAutoexecContent(g_autoexecContent);
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
static Gdiplus::RectF g_editRect, g_saveBtnRect, g_reloadBtnRect;
static Gdiplus::RectF g_socdBtnRect, g_removeSocdBtnRect;
static Gdiplus::RectF g_mwheelBtnRect, g_removeMwheelBtnRect;
static Gdiplus::RectF g_msNormalRect, g_msAttackRect;
static Gdiplus::RectF g_msWriteBtnRect, g_msRemoveBtnRect;
static Gdiplus::RectF g_chSWWriteBtnRect, g_chSWRemoveBtnRect;

void ClearEditingFocus();

void PaintLegalCfgPage(Gdiplus::Graphics& g, int cx, int cw, int H, HWND) {
    using namespace Gdiplus;
    ui::DrawHeader(g, cx, cw, L"合法配置");
    Font sF(L"Microsoft YaHei", 9);
    SolidBrush tdCol(Color(255, 30, 60, 100)), tbCol(Color(255, 20, 80, 140)), tmDim(Color(255, 100, 130, 160));
    SolidBrush btnB(Color(255, 200, 230, 250));
    Pen btnP(Color(255, 150, 190, 220));
    SolidBrush ddBg(Color(255, 220, 240, 255)), ddHoverBg(Color(255, 180, 220, 245));

    g.DrawString(g_autoexecPath.c_str(), -1, &sF, PointF(cx + 10, 50), &tmDim);
    wchar_t sb[64]; swprintf_s(sb, L"状态: %s", g_autoexecStatus.c_str());
    g.DrawString(sb, -1, &sF, PointF(cx + 10, 65), g_autoexecStatus == L"已保存" ? &tbCol : &tdCol);

    int totalAvail = H - 85;
    int editH = totalAvail / 4; if (editH < 120) editH = 120;
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
        g.DrawString(lines[i].c_str(), -1, &sF, PointF(cx + 14, lineY), &tdCol); lineY += LINE_H;
    }
    if (g_editing) {
        int cursorLine, cursorCol; LineColFromPos(lines, g_cursorPos, cursorLine, cursorCol);
        int vr = cursorLine - g_scrollOffset;
        if (vr >= 0 && vr < g_visibleLines) {
            SolidBrush cc(Color(255, 0, 0, 0));
            g.FillRectangle(&cc, cx + 14 + cursorCol * 7, editY + 4 + vr * LINE_H, 2, LINE_H);
        }
    }

    int btnY = editY + editH + 10, btnW = 80, btnH = 26;
    auto drawBtn = [&](Gdiplus::RectF& r, int x, int y, int w, int h) {
        r = RectF((REAL)x, (REAL)y, (REAL)w, (REAL)h);
        GraphicsPath p; p.AddArc(x, y, 16, 16, 180, 90); p.AddArc(x + w - 16, y, 16, 16, 270, 90);
        p.AddArc(x + w - 16, y + h - 16, 16, 16, 0, 90); p.AddArc(x, y + h - 16, 16, 16, 90, 90); p.CloseFigure();
        g.FillPath(&btnB, &p); g.DrawPath(&btnP, &p);
    };
    drawBtn(g_saveBtnRect, cx + 10, btnY, btnW, btnH);
    g.DrawString(L"保存", -1, &sF, PointF(cx + 10 + 20, btnY + 6), &tbCol);
    drawBtn(g_reloadBtnRect, cx + 10 + btnW + 10, btnY, btnW, btnH);
    g.DrawString(L"刷新", -1, &sF, PointF(cx + 10 + btnW + 24, btnY + 6), &tbCol);
    g.DrawString(L"点击编辑框编辑 | PageUp/Down翻页 | 鼠标点击移动光标", -1, &sF, PointF(cx + 10 + btnW + 10 + btnW + 20, btnY + 6), &tmDim);

    int fnY = btnY + btnH + 8, fnW = 100, fnH = 26;
    drawBtn(g_socdBtnRect, cx + 10, fnY, fnW, fnH);
    g.DrawString(L"写入SOCD", -1, &sF, PointF(cx + 18, fnY + 6), &tbCol);
    drawBtn(g_removeSocdBtnRect, cx + 10 + fnW + 10, fnY, fnW, fnH);
    g.DrawString(L"移除SOCD", -1, &sF, PointF(cx + 10 + fnW + 22, fnY + 6), &tbCol);

    int fnY2 = fnY + fnH + 6;
    drawBtn(g_mwheelBtnRect, cx + 10, fnY2, fnW, fnH);
    g.DrawString(L"写入滚轮跳", -1, &sF, PointF(cx + 12, fnY2 + 6), &tbCol);
    drawBtn(g_removeMwheelBtnRect, cx + 10 + fnW + 10, fnY2, fnW, fnH);
    g.DrawString(L"移除滚轮跳", -1, &sF, PointF(cx + 10 + fnW + 22, fnY2 + 6), &tbCol);

    int fnY3 = fnY2 + fnH + 6, msW = 120;
    drawBtn(g_msWriteBtnRect, cx + 10, fnY3, msW, fnH);
    g.DrawString(L"写入混合灵敏度", -1, &sF, PointF(cx + 18, fnY3 + 6), &tbCol);
    drawBtn(g_msRemoveBtnRect, cx + 10 + msW + 10, fnY3, msW, fnH);
    g.DrawString(L"移除混合灵敏度", -1, &sF, PointF(cx + 10 + msW + 22, fnY3 + 6), &tbCol);

    int inpX = cx + 10 + msW + 10 + msW + 20, inpW = 60;
    SolidBrush inpBg(Color(255, 250, 250, 255)); Pen inpPen(Color(255, 180, 200, 220));
    g.DrawString(L"常规:", -1, &sF, PointF(inpX, fnY3 + 4), &tdCol);
    g_msNormalRect = RectF((REAL)(inpX + 35), (REAL)fnY3, (REAL)inpW, (REAL)fnH);
    g.FillRectangle(&inpBg, g_msNormalRect); g.DrawRectangle(&inpPen, g_msNormalRect);
    g.DrawString(g_msNormal.c_str(), -1, &sF, PointF(inpX + 39, fnY3 + 4), &tdCol);
    if (g_editingMSnormal) { SolidBrush cc(Color(255, 0, 0, 0)); g.FillRectangle(&cc, inpX + 39 + (int)g_msNormal.length() * 7, fnY3 + 2, 2, 22); }
    int atkX = inpX + 35 + inpW + 15;
    g.DrawString(L"开火:", -1, &sF, PointF(atkX, fnY3 + 4), &tdCol);
    g_msAttackRect = RectF((REAL)(atkX + 35), (REAL)fnY3, (REAL)inpW, (REAL)fnH);
    g.FillRectangle(&inpBg, g_msAttackRect); g.DrawRectangle(&inpPen, g_msAttackRect);
    g.DrawString(g_msAttack.c_str(), -1, &sF, PointF(atkX + 39, fnY3 + 4), &tdCol);
    if (g_editingMSattack) { SolidBrush cc(Color(255, 0, 0, 0)); g.FillRectangle(&cc, atkX + 39 + (int)g_msAttack.length() * 7, fnY3 + 2, 2, 22); }

    int fnY4 = fnY3 + fnH + 6, chW = 110;
    drawBtn(g_chSWWriteBtnRect, cx + 10, fnY4, chW, fnH);
    g.DrawString(L"写入准星跟随切换", -1, &sF, PointF(cx + 14, fnY4 + 6), &tbCol);
    drawBtn(g_chSWRemoveBtnRect, cx + 10 + chW + 10, fnY4, chW, fnH);
    g.DrawString(L"移除准星跟随切换", -1, &sF, PointF(cx + 10 + chW + 22, fnY4 + 6), &tbCol);

    int ddX = cx + 10 + chW + 10 + chW + 20, ddW = 90;
    g_crosshairSWDropdownRect = RectF((REAL)ddX, (REAL)fnY4, (REAL)ddW, (REAL)fnH);
    SolidBrush ddBtn(Color(255, 200, 230, 250)); Pen ddPen(Color(255, 150, 190, 220));
    g.FillRectangle(&ddBtn, g_crosshairSWDropdownRect); g.DrawRectangle(&ddPen, g_crosshairSWDropdownRect);
    g.DrawString(CROSSHAIR_SW_MODES[g_crosshairSWMode], -1, &sF, PointF(ddX + 4, fnY4 + 4), &tdCol);
    SolidBrush arr(Color(255, 30, 60, 100));
    PointF arrPts[] = { PointF((REAL)(ddX + ddW - 8), (REAL)(fnY4 + 6)), PointF((REAL)(ddX + ddW), (REAL)(fnY4 + 6)), PointF((REAL)(ddX + ddW - 4), (REAL)(fnY4 + 14)) };
    g.FillPolygon(&arr, arrPts, 3);
    if (g_crosshairSWDropdownOpen) {
        for (int j = 0; j < CROSSHAIR_SW_COUNT; ++j) {
            RectF optRect((REAL)ddX, (REAL)(fnY4 + fnH + j * 18), (REAL)ddW, 18.f);
            g_dropdownRects[j] = optRect;
            g.FillRectangle((j == g_crosshairSWMode) ? &ddHoverBg : &ddBg, optRect); g.DrawRectangle(&ddPen, optRect);
            g.DrawString(CROSSHAIR_SW_MODES[j], -1, &sF, PointF((REAL)ddX + 4, (REAL)(fnY4 + fnH + j * 18)), &tdCol);
        }
    }
    if (g_crosshairSWMode == 1)
        g.DrawString(L"请自行修改配置文件中的绑定", -1, &sF, PointF(ddX + ddW + 10, fnY4 + 4), &tmDim);
}

void CheckLegalCfgClick(HWND hw, int mx, int my) {
    if (g_crosshairSWDropdownOpen) {
        for (int j = 0; j < CROSSHAIR_SW_COUNT; ++j) {
            auto& r = g_dropdownRects[j];
            if (mx >= r.X && mx <= r.X + r.Width && my >= r.Y && my <= r.Y + r.Height) {
                g_crosshairSWMode = j; g_crosshairSWDropdownOpen = false; InvalidateRect(hw, nullptr, FALSE); return;
            }
        }
        g_crosshairSWDropdownOpen = false; InvalidateRect(hw, nullptr, FALSE); return;
    }
    if (mx >= g_crosshairSWDropdownRect.X && mx <= g_crosshairSWDropdownRect.X + g_crosshairSWDropdownRect.Width &&
        my >= g_crosshairSWDropdownRect.Y && my <= g_crosshairSWDropdownRect.Y + g_crosshairSWDropdownRect.Height) {
        g_crosshairSWDropdownOpen = !g_crosshairSWDropdownOpen; ClearEditingFocus(); InvalidateRect(hw, nullptr, FALSE); return;
    }
    auto cf = [&]() { g_editing = false; g_editingMSnormal = false; g_editingMSattack = false; };
    #define R(r) (mx >= r.X && mx <= r.X + r.Width && my >= r.Y && my <= r.Y + r.Height)
    if R(g_saveBtnRect) { if (g_editing) { g_autoexecContent = g_editBuffer; cf(); } SaveAutoexecContent(g_autoexecContent); InvalidateRect(hw, nullptr, FALSE); return; }
    if R(g_reloadBtnRect) { g_lastWriteTime = 0; LoadAutoexecContent(); cf(); InvalidateRect(hw, nullptr, FALSE); return; }
    if R(g_socdBtnRect) { AppendToFile(std::wstring(SOCD_BLOCK)); cf(); InvalidateRect(hw, nullptr, FALSE); return; }
    if R(g_removeSocdBtnRect) { bool f = RemoveBlockAndSave(L"//--StrikeSense SOCD--", L"//--StrikeSense SOCD END--"); g_autoexecStatus = f ? L"已移除SOCD" : L"未找到SOCD"; cf(); InvalidateRect(hw, nullptr, FALSE); return; }
    if R(g_mwheelBtnRect) { AppendToFile(std::wstring(MWHEELJUMP_BLOCK)); cf(); InvalidateRect(hw, nullptr, FALSE); return; }
    if R(g_removeMwheelBtnRect) { bool f = RemoveBlockAndSave(L"//--StrikeSense MwheelJump--", L"//--StrikeSense MwheelJump END--"); g_autoexecStatus = f ? L"已移除滚轮跳" : L"未找到滚轮跳"; cf(); InvalidateRect(hw, nullptr, FALSE); return; }
    if R(g_msWriteBtnRect) { AppendToFile(BuildMSBlock()); cf(); InvalidateRect(hw, nullptr, FALSE); return; }
    if R(g_msRemoveBtnRect) { bool f = RemoveBlockAndSave(L"//--StrikeSense MS--", L"//--StrikeSense MS END--"); g_autoexecStatus = f ? L"已移除混合灵敏度" : L"未找到混合灵敏度"; cf(); InvalidateRect(hw, nullptr, FALSE); return; }
    if R(g_chSWWriteBtnRect) { AppendToFile(BuildCrosshairSWBlock()); cf(); InvalidateRect(hw, nullptr, FALSE); return; }
    if R(g_chSWRemoveBtnRect) { bool f = RemoveBlockAndSave(L"//--StrikeSense CrosshairSW--", L"//--StrikeSense CrosshairSW END--"); g_autoexecStatus = f ? L"已移除准星跟随切换" : L"未找到准星跟随切换"; cf(); InvalidateRect(hw, nullptr, FALSE); return; }
    if R(g_msNormalRect) { cf(); g_editingMSnormal = true; SetFocus(hw); InvalidateRect(hw, nullptr, FALSE); return; }
    if R(g_msAttackRect) { cf(); g_editingMSattack = true; SetFocus(hw); InvalidateRect(hw, nullptr, FALSE); return; }
    if R(g_editRect) {
        cf(); g_editing = true; g_editBuffer = g_autoexecContent;
        int relY = my - (int)g_editRect.Y - 4, relX = mx - (int)g_editRect.X - 14;
        int cl = relY / LINE_H + g_scrollOffset, cc = (relX / 7 > 0) ? relX / 7 : 0;
        auto lines = ContentToLines(g_editBuffer);
        if (cl < 0) cl = 0; if (cl >= (int)lines.size()) { cl = (int)lines.size() - 1; cc = (int)lines[cl].length(); }
        g_cursorPos = PosFromLineCol(lines, cl, cc);
        SetFocus(hw); InvalidateRect(hw, nullptr, FALSE); return;
    }
    cf(); InvalidateRect(hw, nullptr, FALSE);
    #undef R
}

void ClearEditingFocus() { g_editing = false; g_editingMSnormal = false; g_editingMSattack = false; }

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
            if (vk == VK_TAB) {
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
    g_lastWriteTime = 0; g_editing = false; g_editingMSnormal = false; g_editingMSattack = false; g_crosshairSWDropdownOpen = false;
    LoadAutoexecContent(); g_editBuffer = g_autoexecContent;
}