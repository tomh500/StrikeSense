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
joy_response_move 1
joy_forward_sensitivity 1
joy_side_sensitivity 1
forwardback 0 0 0
rightleft 0 0 0
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
jfb_00
jrl_00
bind w +ForwardEvent
bind s +BackEvent 
bind a +LeftEvent
bind d +RightEvent
//--StrikeSense SOCD END--
)";

static const wchar_t* MWHEELJUMP_BLOCK = LR"(
//--StrikeSense MwheelJump--
bind mwheelup +jump
bind mwheeldown +jump
//--StrikeSense MwheelJump END--
)";

static std::wstring g_msNormal = L"1.0";
static std::wstring g_msAttack = L"1.25";
static bool g_editingMSnormal = false, g_editingMSattack = false;

static void ParseMSParams(const std::wstring& c) {
    size_t p = c.find(L"SSMS_S1 \"sensitivity ");
    if (p != std::string::npos) { size_t e = c.find(L"\"", p + 21); if (e != std::string::npos) g_msNormal = c.substr(p + 21, e - p - 21); }
    p = c.find(L"SSMS_S2 \"sensitivity ");
    if (p != std::string::npos) { size_t e = c.find(L"\"", p + 21); if (e != std::string::npos) g_msAttack = c.substr(p + 21, e - p - 21); }
}

static std::wstring BuildMS() {
    return L"\n//--StrikeSense MS--\nalias SSMS_S1 \"sensitivity " + g_msNormal + L"\"\nalias SSMS_S2 \"sensitivity " + g_msAttack + L"\"\nalias +SS_attack \"+attack;SSMS_S2;spec_next\"\nalias -SS_attack \"-attack;SSMS_S1\"\nbind mouse1 +SS_attack\n//--StrikeSense MS END--\n";
}

static int g_chSWMode = 0;
static bool g_chSWOpen = false;
static Gdiplus::RectF g_chSWDropRect;
static const wchar_t* CHSW_MODES[2] = { L"capslock", L"custom" };

static std::wstring BuildCHSW() {
    std::wstring r = L"\n//--StrikeSense CrosshairSW--\n";
    r += g_chSWMode == 0 ? L"bind capslock \"toggle cl_crosshair_recoil 0 1\"\n" : L"// custom\nbind F2 \"toggle cl_crosshair_recoil 0 1\"\n";
    return r + L"//--StrikeSense CrosshairSW END--\n";
}

static bool RemoveBlock(std::wstring& c, const std::wstring& start, const std::wstring& end) {
    size_t s = c.find(start); if (s == std::string::npos) return false;
    size_t e = c.find(end, s); if (e == std::string::npos) return false;
    size_t ee = e + end.length();
    while (s > 0 && (c[s-1] == L'\n' || c[s-1] == L'\r')) s--;
    while (ee < c.length() && (c[ee] == L'\n' || c[ee] == L'\r')) ee++;
    c.erase(s, ee - s); return true;
}

static std::wstring FindPath() {
    std::wstring sv = strikesense::LoadSavedCfgPath();
    if (!sv.empty()) { fs::path p(sv); p /= L"autoexec.cfg"; if (fs::exists(p)) return p.wstring(); if (fs::exists(sv)) return sv + L"\\autoexec.cfg"; }
    std::wstring sp = strikesense::GetSteamPathFromRegistry();
    if (sp.empty()) return L""; std::wstring cd = strikesense::FindCS2InstallDir(sp);
    return cd.empty() ? L"" : strikesense::GetCS2CfgPath(cd) + L"\\autoexec.cfg";
}

static std::vector<std::wstring> ToLines(const std::wstring& c) {
    std::vector<std::wstring> l; std::wstringstream ss(c); std::wstring line;
    while (std::getline(ss, line)) l.push_back(line); return l;
}

static int PosFromLC(const std::vector<std::wstring>& l, int line, int col) {
    int p = 0; for (int i = 0; i < line && i < (int)l.size(); i++) p += (int)l[i].length() + 1;
    if (col < 0) col = 0; if (line < (int)l.size() && col > (int)l[line].length()) col = (int)l[line].length();
    return p + col;
}
static void LCFromPos(const std::vector<std::wstring>& l, int pos, int& ol, int& oc) {
    ol = 0; oc = 0; int p = 0;
    for (int i = 0; i < (int)l.size(); i++) { if (pos <= p + (int)l[i].length()) { ol = i; oc = pos - p; return; } p += (int)l[i].length() + 1; }
    ol = (int)l.size() - 1; if (ol < 0) ol = 0; oc = l.empty() ? 0 : (int)l[ol].length();
}

static void LoadCfg() {
    g_autoexecPath = FindPath();
    if (g_autoexecPath.empty()) { g_autoexecContent = L"// 无法找到"; g_autoexecStatus = L"路径错误"; return; }
    if (!fs::exists(g_autoexecPath)) { g_autoexecContent = L"// 不存在"; g_autoexecStatus = L"新文件"; g_lastWriteTime = 0; return; }
    auto ft = fs::last_write_time(g_autoexecPath); ULONGLONG wt = ft.time_since_epoch().count();
    if (wt == g_lastWriteTime && !g_autoexecContent.empty() && !g_editing) return;
    g_lastWriteTime = wt;
    std::ifstream in(g_autoexecPath, std::ios::binary);
    if (!in.is_open()) { g_autoexecContent = L"// 读取失败"; g_autoexecStatus = L"读取失败"; return; }
    std::stringstream ss; ss << in.rdbuf(); in.close(); std::string utf8 = ss.str();
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), 0, 0);
    if (wlen <= 0) wlen = MultiByteToWideChar(CP_ACP, 0, utf8.c_str(), (int)utf8.size(), 0, 0);
    g_autoexecContent.resize(wlen);
    MultiByteToWideChar(wlen > 0 ? CP_UTF8 : CP_ACP, 0, utf8.c_str(), (int)utf8.size(), &g_autoexecContent[0], wlen);
    g_autoexecStatus = L"已加载"; if (!g_editing) g_editBuffer = g_autoexecContent;
    ParseMSParams(g_autoexecContent);
}

static void SaveCfg(const std::wstring& c) {
    if (g_autoexecPath.empty()) return;
    int u8len = WideCharToMultiByte(CP_UTF8, 0, c.c_str(), (int)c.size(), 0, 0, 0, 0);
    if (u8len <= 0) return; std::string utf8(u8len, 0);
    WideCharToMultiByte(CP_UTF8, 0, c.c_str(), (int)c.size(), &utf8[0], u8len, 0, 0);
    std::ofstream out(g_autoexecPath, std::ios::binary);
    if (!out.is_open()) { g_autoexecStatus = L"保存失败"; return; }
    out.write(utf8.data(), utf8.size()); out.close();
    g_autoexecContent = c; g_autoexecStatus = L"已保存"; g_editBuffer = c;
    g_lastWriteTime = fs::last_write_time(g_autoexecPath).time_since_epoch().count();
}
static void Append(const std::wstring& t) { g_autoexecContent += t; g_editBuffer = g_autoexecContent; SaveCfg(g_autoexecContent); }
static bool RemoveSave(const std::wstring& s, const std::wstring& e) { std::wstring c = g_autoexecContent; bool f = RemoveBlock(c, s, e); if (f) { g_autoexecContent = c; g_editBuffer = c; SaveCfg(c); } return f; }

static void EnsureVis(const std::vector<std::wstring>& l) {
    int cl, cc; LCFromPos(l, g_cursorPos, cl, cc);
    if (cl < g_scrollOffset) g_scrollOffset = cl;
    if (cl >= g_scrollOffset + g_visibleLines) g_scrollOffset = cl - g_visibleLines + 1;
    if (g_scrollOffset < 0) g_scrollOffset = 0;
}

constexpr int LH = 18;
static Gdiplus::RectF g_er, g_sv, g_rl, g_sd, g_rsd, g_mw, g_rmw, g_msN, g_msA, g_msw, g_rmsw, g_csw, g_rcsw;

void ClearEditingFocus();

void PaintLegalCfgPage(Gdiplus::Graphics& g, int cx, int cw, int H, HWND) {
    using namespace Gdiplus;
    ui::DrawHeader(g, cx, cw, L"合法配置");
    Font sf(L"Microsoft YaHei", 9);
    SolidBrush tc(Color(255,30,60,100)), tb(Color(255,20,80,140)), td(Color(255,100,130,160));
    SolidBrush bb(Color(255,200,230,250)); Pen bp(Color(255,150,190,220));
    SolidBrush db(Color(255,220,240,255)), dh(Color(255,180,220,245));

    g.DrawString(g_autoexecPath.c_str(), -1, &sf, PointF(cx+10,50), &td);
    wchar_t sb[64]; swprintf_s(sb, L"状态: %s", g_autoexecStatus.c_str());
    g.DrawString(sb, -1, &sf, PointF(cx+10,65), g_autoexecStatus==L"已保存"?&tb:&tc);

    int av = H - 85; int eh = av / 4; if (eh < 120) eh = 120;
    g_er = RectF((REAL)(cx+10), (REAL)85, (REAL)(cw-20), (REAL)eh);
    SolidBrush ebg(Color(255,250,250,255)); Pen ep(Color(255,180,200,220));
    g.FillRectangle(&ebg, g_er); g.DrawRectangle(&ep, g_er);
    g_visibleLines = (eh - 4) / LH;
    const std::wstring& disp = g_editing ? g_editBuffer : g_autoexecContent;
    auto lines = ToLines(disp);
    if (g_editing) EnsureVis(lines);
    int ly = 89, dr = 0;
    for (int i = g_scrollOffset; i < (int)lines.size() && dr < g_visibleLines; i++, dr++) {
        g.DrawString(lines[i].c_str(), -1, &sf, PointF(cx+14, ly), &tc); ly += LH;
    }
    if (g_editing) {
        int cl, cc; LCFromPos(lines, g_cursorPos, cl, cc); int vr = cl - g_scrollOffset;
        if (vr >= 0 && vr < g_visibleLines) { SolidBrush cb(Color(255,0,0,0)); g.FillRectangle(&cb, cx+14+cc*7, 89+vr*LH, 2, LH); }
    }

    const int BW = 100, BH = 26;
    auto B = [&](RectF& r, int x, int y, const wchar_t* t) {
        r = RectF((REAL)x, (REAL)y, (REAL)BW, (REAL)BH);
        GraphicsPath p; p.AddArc(x,y,16,16,180,90); p.AddArc(x+BW-16,y,16,16,270,90);
        p.AddArc(x+BW-16,y+BH-16,16,16,0,90); p.AddArc(x,y+BH-16,16,16,90,90); p.CloseFigure();
        g.FillPath(&bb,&p); g.DrawPath(&bp,&p);
        g.DrawString(t,-1,&sf,PointF(x+6,y+6),&tb);  // 左对齐 +6px 内边距
    };

    int by = 85 + eh + 10;
    B(g_sv, cx+10, by, L"保存");
    B(g_rl, cx+10+BW+10, by, L"刷新");
    g.DrawString(L"点击编辑框编辑 | PageUp/Down翻页 | 鼠标点击移动光标", -1, &sf, PointF(cx+10+BW+10+BW+20, by+6), &td);

    int fy = by + BH + 8;
    B(g_sd, cx+10, fy, L"写入SOCD");
    B(g_rsd, cx+10+BW+10, fy, L"移除SOCD");

    int fy2 = fy + BH + 6;
    B(g_mw, cx+10, fy2, L"写入滚轮跳");
    B(g_rmw, cx+10+BW+10, fy2, L"移除滚轮跳");

    int fy3 = fy2 + BH + 6;
    B(g_msw, cx+10, fy3, L"写入混合灵敏度");
    B(g_rmsw, cx+10+BW+10, fy3, L"移除混合灵敏度");

    int ix = cx+10+BW+10+BW+20, iw = 60;
    SolidBrush ib(Color(255,250,250,255)); Pen ip(Color(255,180,200,220));
    g.DrawString(L"常规:", -1, &sf, PointF(ix, fy3+4), &tc);
    g_msN = RectF((REAL)(ix+35), (REAL)fy3, (REAL)iw, (REAL)BH);
    g.FillRectangle(&ib, g_msN); g.DrawRectangle(&ip, g_msN);
    g.DrawString(g_msNormal.c_str(), -1, &sf, PointF(ix+39, fy3+4), &tc);
    if (g_editingMSnormal) { SolidBrush cb(Color(255,0,0,0)); g.FillRectangle(&cb, ix+39+(int)g_msNormal.length()*7, fy3+2, 2, 22); }
    int ax = ix+35+iw+15;
    g.DrawString(L"开火:", -1, &sf, PointF(ax, fy3+4), &tc);
    g_msA = RectF((REAL)(ax+35), (REAL)fy3, (REAL)iw, (REAL)BH);
    g.FillRectangle(&ib, g_msA); g.DrawRectangle(&ip, g_msA);
    g.DrawString(g_msAttack.c_str(), -1, &sf, PointF(ax+39, fy3+4), &tc);
    if (g_editingMSattack) { SolidBrush cb(Color(255,0,0,0)); g.FillRectangle(&cb, ax+39+(int)g_msAttack.length()*7, fy3+2, 2, 22); }

    int fy4 = fy3 + BH + 6;
    B(g_csw, cx+10, fy4, L"写入准星跟随切换");
    B(g_rcsw, cx+10+BW+10, fy4, L"移除准星跟随切换");

    int dx = cx+10+BW+10+BW+20, dw = 90;
    g_chSWDropRect = RectF((REAL)dx, (REAL)fy4, (REAL)dw, (REAL)BH);
    SolidBrush db2(Color(255,200,230,250)); Pen dp(Color(255,150,190,220));
    g.FillRectangle(&db2, g_chSWDropRect); g.DrawRectangle(&dp, g_chSWDropRect);
    g.DrawString(CHSW_MODES[g_chSWMode], -1, &sf, PointF(dx+4, fy4+4), &tc);
    SolidBrush ar(Color(255,30,60,100));
    PointF apt[] = { PointF((REAL)(dx+dw-8),(REAL)(fy4+6)), PointF((REAL)(dx+dw),(REAL)(fy4+6)), PointF((REAL)(dx+dw-4),(REAL)(fy4+14)) };
    g.FillPolygon(&ar, apt, 3);
    if (g_chSWOpen) for (int j = 0; j < 2; ++j) {
        RectF or2((REAL)dx,(REAL)(fy4+BH+j*18),(REAL)dw,18.f);
        g_dropdownRects[j] = or2; g.FillRectangle(j==g_chSWMode?&dh:&db, or2); g.DrawRectangle(&dp, or2);
        g.DrawString(CHSW_MODES[j], -1, &sf, PointF((REAL)dx+4,(REAL)(fy4+BH+j*18)), &tc);
    }
    if (g_chSWMode == 1) g.DrawString(L"请自行修改配置文件中的绑定", -1, &sf, PointF(dx+dw+10, fy4+4), &td);
}

void CheckLegalCfgClick(HWND hw, int mx, int my) {
    auto cf = [&]() { g_editing = false; g_editingMSnormal = false; g_editingMSattack = false; };
    #define R(r) (mx >= r.X && mx <= r.X+r.Width && my >= r.Y && my <= r.Y+r.Height)
    if (g_chSWOpen) {
        for (int j = 0; j < 2; ++j) if R(g_dropdownRects[j]) { g_chSWMode = j; g_chSWOpen = false; InvalidateRect(hw,0,0); return; }
        g_chSWOpen = false; InvalidateRect(hw,0,0); return;
    }
    if R(g_chSWDropRect) { g_chSWOpen = !g_chSWOpen; cf(); InvalidateRect(hw,0,0); return; }
    if R(g_sv) { if (g_editing) { g_autoexecContent = g_editBuffer; cf(); } SaveCfg(g_autoexecContent); InvalidateRect(hw,0,0); return; }
    if R(g_rl) { g_lastWriteTime = 0; LoadCfg(); cf(); InvalidateRect(hw,0,0); return; }
    if R(g_sd) { Append(SOCD_BLOCK); cf(); InvalidateRect(hw,0,0); return; }
    if R(g_rsd) { g_autoexecStatus = RemoveSave(L"//--StrikeSense SOCD--",L"//--StrikeSense SOCD END--")?L"已移除SOCD":L"未找到SOCD"; cf(); InvalidateRect(hw,0,0); return; }
    if R(g_mw) { Append(MWHEELJUMP_BLOCK); cf(); InvalidateRect(hw,0,0); return; }
    if R(g_rmw) { g_autoexecStatus = RemoveSave(L"//--StrikeSense MwheelJump--",L"//--StrikeSense MwheelJump END--")?L"已移除滚轮跳":L"未找到滚轮跳"; cf(); InvalidateRect(hw,0,0); return; }
    if R(g_msw) { Append(BuildMS()); cf(); InvalidateRect(hw,0,0); return; }
    if R(g_rmsw) { g_autoexecStatus = RemoveSave(L"//--StrikeSense MS--",L"//--StrikeSense MS END--")?L"已移除混合灵敏度":L"未找到混合灵敏度"; cf(); InvalidateRect(hw,0,0); return; }
    if R(g_csw) { Append(BuildCHSW()); cf(); InvalidateRect(hw,0,0); return; }
    if R(g_rcsw) { g_autoexecStatus = RemoveSave(L"//--StrikeSense CrosshairSW--",L"//--StrikeSense CrosshairSW END--")?L"已移除准星跟随切换":L"未找到准星跟随切换"; cf(); InvalidateRect(hw,0,0); return; }
    if R(g_msN) { cf(); g_editingMSnormal = true; SetFocus(hw); InvalidateRect(hw,0,0); return; }
    if R(g_msA) { cf(); g_editingMSattack = true; SetFocus(hw); InvalidateRect(hw,0,0); return; }
    if R(g_er) {
        cf(); g_editing = true; g_editBuffer = g_autoexecContent;
        int ry = my - (int)g_er.Y - 4, rx = mx - (int)g_er.X - 14;
        int cl = ry / LH + g_scrollOffset, cc = (rx / 7 > 0) ? rx / 7 : 0;
        auto l = ToLines(g_editBuffer); if (cl < 0) cl = 0;
        if (cl >= (int)l.size()) { cl = (int)l.size()-1; cc = (int)l[cl].length(); }
        g_cursorPos = PosFromLC(l, cl, cc); SetFocus(hw); InvalidateRect(hw,0,0); return;
    }
    cf(); InvalidateRect(hw,0,0);
    #undef R
}

void ClearEditingFocus() { g_editing = false; g_editingMSnormal = false; g_editingMSattack = false; }

bool ProcessLegalCfgKeyInput(HWND hw, UINT msg, WPARAM wp, LPARAM) {
    auto lines = ToLines(g_editBuffer);
    if (g_editing && msg == WM_CHAR) {
        wchar_t ch = (wchar_t)wp;
        if (ch >= 32 && ch <= 126) { g_editBuffer.insert(g_cursorPos, 1, ch); g_cursorPos++; InvalidateRect(hw,0,0); return true; }
        if (ch == 13) { g_editBuffer.insert(g_cursorPos, 1, L'\n'); g_cursorPos++; InvalidateRect(hw,0,0); return true; }
        return false;
    }
    if (g_editing && msg == WM_KEYDOWN) {
        UINT vk = (UINT)wp;
        if (vk == VK_BACK && g_cursorPos > 0) { g_editBuffer.erase(g_cursorPos-1,1); g_cursorPos--; InvalidateRect(hw,0,0); return true; }
        if (vk == VK_DELETE && g_cursorPos < (int)g_editBuffer.length()) { g_editBuffer.erase(g_cursorPos,1); InvalidateRect(hw,0,0); return true; }
        if (vk == VK_LEFT && g_cursorPos > 0) { g_cursorPos--; InvalidateRect(hw,0,0); return true; }
        if (vk == VK_RIGHT && g_cursorPos < (int)g_editBuffer.length()) { g_cursorPos++; InvalidateRect(hw,0,0); return true; }
        if (vk == VK_UP) { int cl,cc; LCFromPos(lines,g_cursorPos,cl,cc); if(cl>0) g_cursorPos=PosFromLC(lines,cl-1,cc); InvalidateRect(hw,0,0); return true; }
        if (vk == VK_DOWN) { int cl,cc; LCFromPos(lines,g_cursorPos,cl,cc); if(cl<(int)lines.size()-1) g_cursorPos=PosFromLC(lines,cl+1,cc); InvalidateRect(hw,0,0); return true; }
        if (vk == VK_PRIOR) { g_scrollOffset -= g_visibleLines; if(g_scrollOffset<0) g_scrollOffset=0; int cl,cc; LCFromPos(lines,g_cursorPos,cl,cc); cl-=g_visibleLines; if(cl<0)cl=0; g_cursorPos=PosFromLC(lines,cl,cc); InvalidateRect(hw,0,0); return true; }
        if (vk == VK_NEXT) { g_scrollOffset += g_visibleLines; int cl,cc; LCFromPos(lines,g_cursorPos,cl,cc); cl+=g_visibleLines; if(cl>=(int)lines.size())cl=(int)lines.size()-1; g_cursorPos=PosFromLC(lines,cl,cc); InvalidateRect(hw,0,0); return true; }
        return false;
    }
    if ((g_editingMSnormal||g_editingMSattack) && msg == WM_CHAR) {
        wchar_t ch = (wchar_t)wp;
        std::wstring* t = g_editingMSnormal ? &g_msNormal : &g_msAttack;
        if ((ch >= '0' && ch <= '9') || ch == '.') { *t += ch; InvalidateRect(hw,0,0); return true; }
        if (ch == 8 && !t->empty()) { t->pop_back(); InvalidateRect(hw,0,0); return true; }
        return false;
    }
    if ((g_editingMSnormal||g_editingMSattack) && msg == WM_KEYDOWN) {
        std::wstring* t = g_editingMSnormal ? &g_msNormal : &g_msAttack;
        UINT vk = (UINT)wp;
        if (vk == VK_BACK && !t->empty()) { t->pop_back(); InvalidateRect(hw,0,0); return true; }
        return false;
    }
    return false;
}

void InitLegalCfgPage() {
    g_lastWriteTime = 0; g_editing = false; g_editingMSnormal = false; g_editingMSattack = false; g_chSWOpen = false;
    LoadCfg(); g_editBuffer = g_autoexecContent;
}