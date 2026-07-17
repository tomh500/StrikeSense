#include "pages.h"
#include "steam_helper.h"
#include "i18n.h"
#include "textgui_overlay.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <Shellapi.h>

namespace fs = std::filesystem;

// ==================== 【新增】切刀声音替换 (SRP) 全局变量 ====================
int g_srpModeIdx = 0;              // 当前选中的切刀声音索引 (0=不替换, 1=短剑...)
bool g_srpDropdownOpen = false;    // 下拉栏展开状态
static Gdiplus::RectF g_btnSrpAddRect; // 写入按钮区域
static Gdiplus::RectF g_btnSrpRemRect; // 移除按钮区域
static Gdiplus::RectF g_ddSrpRect;     // 下拉栏点击区域

// 替代原先的 SRP_MODES 数组
const wchar_t* GetSrpModeName(int idx) {
    static const char* keys[8] = {
        "LEGAL_SRP_NONE",
        "LEGAL_SRP_STILLETTO",
        "LEGAL_SRP_GYPSY",
        "LEGAL_SRP_PUSH",
        "LEGAL_SRP_WIDOW",
        "LEGAL_SRP_FALCHION",
        "LEGAL_SRP_URSUS",
        "LEGAL_SRP_BUTTERFLY"
    };
    if (idx < 0 || idx >= 8) return L"";
    return i18n::T(keys[idx]); // 这里调用你的翻译函数
}

static std::wstring g_autoexecPath;
static std::wstring g_autoexecContent;
static std::wstring g_editBuffer;
static Gdiplus::RectF g_srpDropdownRects[8]; // 用于存储下拉菜单中8个选项的点击区域
static std::wstring BuildSRP() {
    std::wstring r = L"\n//--StrikeSense SRP--\n";
    r += L"alias SRP_play_0 \"\"\n";
    r += L"alias SRP_play_1 \"play sounds/weapons/knife_stilleto/stilletto_draw_01.vsnd\"\n";
    r += L"alias SRP_play_2 \"play sounds/weapons/knife_gypsy/gypsy_draw_01.vsnd\"\n";
    r += L"alias SRP_play_3 \"play sounds/weapons/knife_push/knife_push_draw.vsnd\"\n";
    r += L"alias SRP_play_4 \"play sounds/weapons/knife_widow/widow_deploy_01.vsnd\"\n";
    r += L"alias SRP_play_5 \"play sounds/weapons/knife_falchion/knife_falchion_draw.vsnd\"\n";
    r += L"alias SRP_play_6 \"play sounds/weapons/knife_ursus/ursus_catch_01.vsnd\"\n";
    r += L"alias SRP_play_7 \"play sounds/weapons/bknife/bknife_draw02.vsnd\"\n";

    // 根据下拉选项拼接对应的声音编号
    r += L"alias +ss_slot3 \"SRP_play_" + std::to_wstring(g_srpModeIdx) + L";slot3\"\n";
    r += L"alias -ss_slot3 \"\"\n";
    r += L"bind 3 +ss_slot3\n";
    r += L"//--StrikeSense SRP END--\n"; // 添加END标识以配合Remove功能
    return r;
}

// 回归 wstring，但存储的是通过 _() 翻译后的动态文本
static std::wstring g_autoexecStatus;
// 增加一个标记，用来画图时判断状态颜色（是否为绿色“已保存”）
static bool g_isStatusSaved = false;

static ULONGLONG g_lastWriteTime = 0;
static bool g_editing = false;
static int g_cursorPos = 0;
static int g_scrollOffset = 0;
static int g_visibleLines = 0;
static bool g_autoexecManualScroll = false;

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
    const wchar_t* key = g_chSWMode == 0 ? L"capslock" : L"F2";
    if (g_chSWMode == 1) r += L"// custom\n";
    r += L"alias crosshairsw \"crosshairsw1\"\n";
    r += L"alias crosshairsw1 \"echoln /cr1;cl_crosshair_recoil 1;alias crosshairsw crosshairsw2\"\n";
    r += L"alias crosshairsw2 \"echoln /cr0;cl_crosshair_recoil 0;alias crosshairsw crosshairsw1\"\n";
    r += std::wstring(L"bind ") + key + L" \"crosshairsw\"\n";
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
    if (g_autoexecPath.empty()) { g_autoexecContent = L"// Error"; g_autoexecStatus = _(i18n::Keys::LEGAL_NOT_FOUND); g_isStatusSaved = false; return; }
    if (!fs::exists(g_autoexecPath)) { g_autoexecContent = L"// New File"; g_autoexecStatus = _(i18n::Keys::LEGAL_NOT_FOUND); g_isStatusSaved = false; g_lastWriteTime = 0; return; }
    auto ft = fs::last_write_time(g_autoexecPath); ULONGLONG wt = ft.time_since_epoch().count();
    if (wt == g_lastWriteTime && !g_autoexecContent.empty() && !g_editing) return;
    g_lastWriteTime = wt;
    std::ifstream in(g_autoexecPath, std::ios::binary);
    if (!in.is_open()) { g_autoexecContent = L"// Read Fail"; g_autoexecStatus = _(i18n::Keys::LEGAL_NOT_FOUND); g_isStatusSaved = false; return; }
    std::stringstream ss; ss << in.rdbuf(); in.close(); std::string utf8 = ss.str();
    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), 0, 0);
    if (wlen <= 0) wlen = MultiByteToWideChar(CP_ACP, 0, utf8.c_str(), (int)utf8.size(), 0, 0);
    g_autoexecContent.resize(wlen);
    MultiByteToWideChar(wlen > 0 ? CP_UTF8 : CP_ACP, 0, utf8.c_str(), (int)utf8.size(), &g_autoexecContent[0], wlen);
    g_autoexecStatus = _(i18n::Keys::LEGAL_SAVED); g_isStatusSaved = true; if (!g_editing) g_editBuffer = g_autoexecContent;
    ParseMSParams(g_autoexecContent);
}

static void SaveCfg(const std::wstring& c) {
    if (g_autoexecPath.empty()) return;
    int u8len = WideCharToMultiByte(CP_UTF8, 0, c.c_str(), (int)c.size(), 0, 0, 0, 0);
    if (u8len <= 0) return; std::string utf8(u8len, 0);
    WideCharToMultiByte(CP_UTF8, 0, c.c_str(), (int)c.size(), &utf8[0], u8len, 0, 0);
    std::ofstream out(g_autoexecPath, std::ios::binary);
    if (!out.is_open()) { g_autoexecStatus = _(i18n::Keys::LEGAL_NOT_FOUND); g_isStatusSaved = false; return; }
    out.write(utf8.data(), utf8.size()); out.close();
    g_autoexecContent = c; g_autoexecStatus = _(i18n::Keys::LEGAL_SAVED); g_isStatusSaved = true; g_editBuffer = c;
    g_lastWriteTime = fs::last_write_time(g_autoexecPath).time_since_epoch().count();
}
static void Append(const std::wstring& t) { g_autoexecContent += t; g_editBuffer = g_autoexecContent; SaveCfg(g_autoexecContent); }
static bool RemoveSave(const std::wstring& s, const std::wstring& e) { std::wstring c = g_autoexecContent; bool f = RemoveBlock(c, s, e); if (f) { g_autoexecContent = c; g_editBuffer = c; SaveCfg(c); } return f; }

static void AddRoundRect(Gdiplus::GraphicsPath& path, const Gdiplus::RectF& rect, Gdiplus::REAL radius)
{
    using namespace Gdiplus;
    const REAL d = (std::min)(radius * 2.0f, (std::min)(rect.Width, rect.Height));
    path.AddArc(rect.X, rect.Y, d, d, 180.0f, 90.0f);
    path.AddArc(rect.X + rect.Width - d, rect.Y, d, d, 270.0f, 90.0f);
    path.AddArc(rect.X + rect.Width - d, rect.Y + rect.Height - d, d, d, 0.0f, 90.0f);
    path.AddArc(rect.X, rect.Y + rect.Height - d, d, d, 90.0f, 90.0f);
    path.CloseFigure();
}

static void DrawRoundedFill(Gdiplus::Graphics& g, const Gdiplus::RectF& rect,
    Gdiplus::Brush& brush, Gdiplus::Pen& pen, float radius = 8.0f)
{
    Gdiplus::GraphicsPath path;
    AddRoundRect(path, rect, radius);
    g.FillPath(&brush, &path);
    g.DrawPath(&pen, &path);
}

static void ExitAutoexecEditAndSave()
{
    if (g_editing) {
        g_autoexecContent = g_editBuffer;
        SaveCfg(g_autoexecContent);
    }
    g_editing = false;
    g_editingMSnormal = false;
    g_editingMSattack = false;
    g_autoexecManualScroll = false;
}

static void OpenAutoexecFile(HWND owner)
{
    if (g_editing) {
        g_autoexecContent = g_editBuffer;
        SaveCfg(g_autoexecContent);
    }
    if (g_autoexecPath.empty()) return;
    const INT_PTR opened = reinterpret_cast<INT_PTR>(
        ShellExecuteW(owner, L"open", g_autoexecPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    if (opened <= 32) {
        ShellExecuteW(owner, L"open", L"notepad.exe", g_autoexecPath.c_str(), nullptr, SW_SHOWNORMAL);
    }
    g_editing = false;
    g_editingMSnormal = false;
    g_editingMSattack = false;
    g_autoexecManualScroll = false;
}

bool IsLegalCfgManaged()
{
    return g_autoexecContent.find(L"//--StrikeSense ") != std::wstring::npos;
}

bool HasLegalCfgSOCD()
{
    return g_autoexecContent.find(L"//--StrikeSense SOCD--") != std::wstring::npos;
}

bool SetLegalCfgSOCD(bool enabled)
{
    if (g_autoexecContent.empty()) LoadCfg();
    if (enabled) {
        if (!HasLegalCfgSOCD()) Append(SOCD_BLOCK);
        return HasLegalCfgSOCD();
    }
    RemoveSave(L"//--StrikeSense SOCD--", L"//--StrikeSense SOCD END--");
    return !HasLegalCfgSOCD();
}

bool HasLegalCfgMwheelJump()
{
    return g_autoexecContent.find(L"//--StrikeSense MwheelJump--") != std::wstring::npos;
}

bool SetLegalCfgMwheelJump(bool enabled)
{
    if (g_autoexecContent.empty()) LoadCfg();
    if (enabled) {
        if (!HasLegalCfgMwheelJump()) Append(MWHEELJUMP_BLOCK);
        return HasLegalCfgMwheelJump();
    }
    RemoveSave(L"//--StrikeSense MwheelJump--", L"//--StrikeSense MwheelJump END--");
    return !HasLegalCfgMwheelJump();
}

bool HasLegalCfgMixedSensitivity()
{
    return g_autoexecContent.find(L"//--StrikeSense MS--") != std::wstring::npos;
}

bool SetLegalCfgMixedSensitivity(bool enabled)
{
    if (g_autoexecContent.empty()) LoadCfg();
    if (enabled) {
        if (!HasLegalCfgMixedSensitivity()) Append(BuildMS());
        return HasLegalCfgMixedSensitivity();
    }
    RemoveSave(L"//--StrikeSense MS--", L"//--StrikeSense MS END--");
    return !HasLegalCfgMixedSensitivity();
}

std::pair<std::wstring, std::wstring> GetLegalCfgMixedSensitivityValues()
{
    return { g_msNormal, g_msAttack };
}

bool HasLegalCfgCrosshairSwitch()
{
    return g_autoexecContent.find(L"//--StrikeSense CrosshairSW--") != std::wstring::npos;
}

bool SetLegalCfgCrosshairSwitch(bool enabled)
{
    if (g_autoexecContent.empty()) LoadCfg();
    if (enabled) {
        if (!HasLegalCfgCrosshairSwitch()) Append(BuildCHSW());
        return HasLegalCfgCrosshairSwitch();
    }
    RemoveSave(L"//--StrikeSense CrosshairSW--", L"//--StrikeSense CrosshairSW END--");
    return !HasLegalCfgCrosshairSwitch();
}

bool HasLegalCfgSoundReplace()
{
    return g_autoexecContent.find(L"//--StrikeSense SRP--") != std::wstring::npos;
}

bool SetLegalCfgSoundReplace(bool enabled)
{
    if (g_autoexecContent.empty()) LoadCfg();
    if (enabled) {
        if (!HasLegalCfgSoundReplace()) Append(BuildSRP());
        return HasLegalCfgSoundReplace();
    }
    RemoveSave(L"//--StrikeSense SRP--", L"//--StrikeSense SRP END--");
    return !HasLegalCfgSoundReplace();
}

static void EnsureVis(const std::vector<std::wstring>& l) {
    int cl, cc; LCFromPos(l, g_cursorPos, cl, cc);
    if (cl < g_scrollOffset) g_scrollOffset = cl;
    if (cl >= g_scrollOffset + g_visibleLines) g_scrollOffset = cl - g_visibleLines + 1;
    if (g_scrollOffset < 0) g_scrollOffset = 0;
}

static Gdiplus::REAL MeasureEditorTextWidth(Gdiplus::Graphics& g, Gdiplus::Font& font, const std::wstring& text)
{
    if (text.empty()) return 0.0f;
    Gdiplus::StringFormat format;
    format.SetFormatFlags(Gdiplus::StringFormatFlagsMeasureTrailingSpaces);
    Gdiplus::RectF bounds;
    Gdiplus::RectF layout(0.0f, 0.0f, 20000.0f, 1000.0f);
    g.MeasureString(text.c_str(), static_cast<INT>(text.size()), &font, layout, &format, &bounds);
    return bounds.Width;
}

static int MeasureEditorTextWidth(HWND hw, const std::wstring& text)
{
    if (text.empty()) return 0;
    HDC dc = GetDC(hw);
    if (!dc) return static_cast<int>(text.size()) * 7;
    const int height = -MulDiv(9, GetDeviceCaps(dc, LOGPIXELSY), 72);
    HFONT font = CreateFontW(height, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");
    HGDIOBJ old = font ? SelectObject(dc, font) : nullptr;
    SIZE size{};
    GetTextExtentPoint32W(dc, text.c_str(), static_cast<int>(text.size()), &size);
    if (old) SelectObject(dc, old);
    if (font) DeleteObject(font);
    ReleaseDC(hw, dc);
    return size.cx;
}

static int EditorColumnFromX(HWND hw, const std::wstring& line, int x)
{
    if (x <= 0 || line.empty()) return 0;
    for (int col = 1; col <= static_cast<int>(line.size()); ++col) {
        const int left = MeasureEditorTextWidth(hw, line.substr(0, col - 1));
        const int right = MeasureEditorTextWidth(hw, line.substr(0, col));
        if (x < left + (right - left) / 2) return col - 1;
    }
    return static_cast<int>(line.size());
}

constexpr int LH = 18;
static Gdiplus::RectF g_er, g_sv, g_rl, g_sd, g_rsd, g_mw, g_rmw, g_msN, g_msA, g_msw, g_rmsw, g_csw, g_rcsw;

void ClearEditingFocus();

// ===== UI 绘制层 =====
void PaintLegalCfgPage(Gdiplus::Graphics& g, int cx, int cw, int H, HWND) {
    using namespace Gdiplus;
    const auto& theme = uitheme::get_palette();

    ui::DrawHeader(g, cx, cw, _(i18n::Keys::LEGAL_TITLE));
    Font sf(L"Microsoft YaHei", 9);
    SolidBrush tc(theme.text), tb(theme.accent_strong), td(theme.dim);
    SolidBrush bb(theme.button_background); Pen bp(theme.button_border);
    SolidBrush db(theme.card_background), dh(theme.card_selected_background);

    g.DrawString(g_autoexecPath.c_str(), -1, &sf, PointF((REAL)(cx + 10), 50.f), &td);

    std::wstring sb = std::wstring(_(i18n::Keys::LEGAL_STATUS_PREFIX)) + L" " + g_autoexecStatus;
    g.DrawString(sb.c_str(), -1, &sf, PointF((REAL)(cx + 10), 65.f), g_isStatusSaved ? &tb : &tc);

    int av = H - 85; int eh = av / 4; if (eh < 120) eh = 120;
    g_er = RectF((REAL)(cx + 10), (REAL)85, (REAL)(cw - 20), (REAL)eh);
    SolidBrush ebg(theme.input_background); Pen ep(theme.input_border);
    DrawRoundedFill(g, g_er, ebg, ep, 10.0f);
    g_visibleLines = (eh - 4) / LH;
    const std::wstring& disp = g_editing ? g_editBuffer : g_autoexecContent;
    auto lines = ToLines(disp);
    if (g_editing && !g_autoexecManualScroll) EnsureVis(lines);
    int ly = 89, dr = 0;
    for (int i = g_scrollOffset; i < (int)lines.size() && dr < g_visibleLines; i++, dr++) {
        g.DrawString(lines[i].c_str(), -1, &sf, PointF((REAL)(cx + 14), (REAL)ly), &tc); ly += LH;
    }
    if (g_editing) {
        int cl, cc; LCFromPos(lines, g_cursorPos, cl, cc); int vr = cl - g_scrollOffset;
        if (vr >= 0 && vr < g_visibleLines) {
            const std::wstring prefix = (cl >= 0 && cl < static_cast<int>(lines.size()))
                ? lines[cl].substr(0, (std::min)(cc, static_cast<int>(lines[cl].size())))
                : std::wstring{};
            SolidBrush cb(theme.text);
            g.FillRectangle(&cb, (REAL)(cx + 14) + MeasureEditorTextWidth(g, sf, prefix),
                (REAL)(89 + vr * LH), 2.f, (REAL)LH);
        }
    }

    const int BW = 100, BH = 26;
    auto B = [&](RectF& r, int x, int y, const wchar_t* t) {
        r = RectF((REAL)x, (REAL)y, (REAL)BW, (REAL)BH);
        ui::DrawRoundedButton(g, r, t, false, true);
        };

    int by = 85 + eh + 10;
    B(g_sv, cx + 10, by, i18n::T("LEGAL_OPEN_FILE"));
    B(g_rl, cx + 10 + BW + 10, by, _(i18n::Keys::LEGAL_REFRESH));
    g.DrawString(_(i18n::Keys::LEGAL_EDIT_HINT), -1, &sf, PointF((REAL)(cx + 10 + BW + 10 + BW + 20), (REAL)(by + 6)), &td);

    int fy = by + BH + 8;
    B(g_sd, cx + 10, fy, _(i18n::Keys::LEGAL_ADD_SOCD));
    B(g_rsd, cx + 10 + BW + 10, fy, _(i18n::Keys::LEGAL_REMOVE_SOCD));

    int fy2 = fy + BH + 6;
    B(g_mw, cx + 10, fy2, _(i18n::Keys::LEGAL_ADD_MWHEEL));
    B(g_rmw, cx + 10 + BW + 10, fy2, _(i18n::Keys::LEGAL_REMOVE_MWHEEL));

    int fy3 = fy2 + BH + 6;
    B(g_msw, cx + 10, fy3, _(i18n::Keys::LEGAL_ADD_MS));
    B(g_rmsw, cx + 10 + BW + 10, fy3, _(i18n::Keys::LEGAL_REMOVE_MS));

    int ix = cx + 10 + BW + 10 + BW + 20, iw = 60;
    SolidBrush ib(theme.input_background); Pen ip(theme.input_border);

    std::wstring normalLabel = std::wstring(_(i18n::Keys::LEGAL_NORMAL)) + L":";
    g.DrawString(normalLabel.c_str(), -1, &sf, PointF((REAL)ix, (REAL)(fy3 + 4)), &tc);
    g_msN = RectF((REAL)(ix + 35), (REAL)fy3, (REAL)iw, (REAL)BH);
    g.FillRectangle(&ib, g_msN); g.DrawRectangle(&ip, g_msN);
    g.DrawString(g_msNormal.c_str(), -1, &sf, PointF((REAL)(ix + 39), (REAL)(fy3 + 4)), &tc);
    if (g_editingMSnormal) { SolidBrush cb(Color(255, 0, 0, 0)); g.FillRectangle(&cb, (REAL)(ix + 39 + (int)g_msNormal.length() * 7), (REAL)(fy3 + 2), 2.f, 22.f); }

    int ax = ix + 35 + iw + 15;
    std::wstring attackLabel = std::wstring(_(i18n::Keys::LEGAL_ATTACK)) + L":";
    g.DrawString(attackLabel.c_str(), -1, &sf, PointF((REAL)ax, (REAL)(fy3 + 4)), &tc);
    g_msA = RectF((REAL)(ax + 35), (REAL)fy3, (REAL)iw, (REAL)BH);
    g.FillRectangle(&ib, g_msA); g.DrawRectangle(&ip, g_msA);
    g.DrawString(g_msAttack.c_str(), -1, &sf, PointF((REAL)(ax + 39), (REAL)(fy3 + 4)), &tc);
    if (g_editingMSattack) { SolidBrush cb(Color(255, 0, 0, 0)); g.FillRectangle(&cb, (REAL)(ax + 39 + (int)g_msAttack.length() * 7), (REAL)(fy3 + 2), 2.f, 22.f); }

    // ====== 1. 准星跟随 (CHSW) 静态主体层 ======
    int fy4 = fy3 + BH + 6;
    B(g_csw, cx + 10, fy4, _(i18n::Keys::LEGAL_ADD_CHSW));
    B(g_rcsw, cx + 10 + BW + 10, fy4, _(i18n::Keys::LEGAL_REMOVE_CHSW));

    int dx = cx + 10 + BW + 10 + BW + 20, dw = 90;
    g_chSWDropRect = RectF((REAL)dx, (REAL)fy4, (REAL)dw, (REAL)BH);
    SolidBrush db2(theme.button_background); Pen dp(theme.button_border);
    DrawRoundedFill(g, g_chSWDropRect, db2, dp, 8.0f);
    g.DrawString(CHSW_MODES[g_chSWMode], -1, &sf, PointF((REAL)(dx + 4), (REAL)(fy4 + 4)), &tc);
    SolidBrush ar(theme.text);
    PointF apt[] = { PointF((REAL)(dx + dw - 8),(REAL)(fy4 + 6)), PointF((REAL)(dx + dw),(REAL)(fy4 + 6)), PointF((REAL)(dx + dw - 4),(REAL)(fy4 + 14)) };
    g.FillPolygon(&ar, apt, 3);

    if (g_chSWMode == 1) g.DrawString(_(i18n::Keys::LEGAL_CUSTOM_HINT), -1, &sf, PointF((REAL)(dx + dw + 10), (REAL)(fy4 + 4)), &td);


    // ====== 2. 切刀声音 (SRP) 静态主体层 ======
    int fy5 = fy4 + BH + 6;
    B(g_btnSrpAddRect, cx + 10, fy5, i18n::T("LEGAL_ADD_SRP"));
    B(g_btnSrpRemRect, cx + 10 + BW + 10, fy5, i18n::T("LEGAL_RM_SRP"));

    int dxSrp = cx + 10 + BW + 10 + BW + 20, dwSrp = 90;
    g_ddSrpRect = RectF((REAL)dxSrp, (REAL)fy5, (REAL)dwSrp, (REAL)BH);
    DrawRoundedFill(g, g_ddSrpRect, db2, dp, 8.0f);
    g.DrawString(GetSrpModeName(g_srpModeIdx), -1, &sf, PointF((REAL)(dxSrp + 4), (REAL)(fy5 + 4)), &tc);

    PointF aptSrp[] = { PointF((REAL)(dxSrp + dwSrp - 8),(REAL)(fy5 + 6)), PointF((REAL)(dxSrp + dwSrp),(REAL)(fy5 + 6)), PointF((REAL)(dxSrp + dwSrp - 4),(REAL)(fy5 + 14)) };
    g.FillPolygon(&ar, aptSrp, 3);


    // ====== 3. 【置顶悬浮层】把所有展开菜单放在最后统一绘制 ======

    // A. 准星跟随 的下拉展开菜单 (现在可以完美盖在 SRP 按钮上方了)
    if (g_chSWOpen) {
        for (int j = 0; j < 2; ++j) {
            RectF or2((REAL)dx, (REAL)(fy4 + BH + j * 18), (REAL)dw, 18.f);
            g_dropdownRects[j] = or2;
            DrawRoundedFill(g, or2, j == g_chSWMode ? dh : db, dp, 6.0f);
            g.DrawString(CHSW_MODES[j], -1, &sf, PointF((REAL)(dx + 4), (REAL)(fy4 + BH + j * 18)), &tc);
        }
    }

    // B. 切刀声音 的下拉展开菜单
    if (g_srpDropdownOpen) {
        for (int j = 0; j < 8; ++j) {
            RectF or2((REAL)dxSrp, (REAL)(fy5 + BH + j * 18), (REAL)dwSrp, 18.f);
            g_srpDropdownRects[j] = or2;
            DrawRoundedFill(g, or2, j == g_srpModeIdx ? dh : db, dp, 6.0f);
            g.DrawString(GetSrpModeName(j), -1, &sf, PointF((REAL)(dxSrp + 4), (REAL)(fy5 + BH + j * 18)), &tc);
        }
    }
}

// ===== UI 点击事件层 =====
void CheckLegalCfgClick(HWND hw, int mx, int my) {
    auto cf = [&]() { ClearEditingFocus(); };
    #define R(r) (mx >= r.X && mx <= r.X+r.Width && my >= r.Y && my <= r.Y+r.Height)

    // 拦截 SRP 下拉菜单内的点击
    if (g_srpDropdownOpen) {
        for (int j = 0; j < 8; ++j) {
            if R(g_srpDropdownRects[j]) {
                g_srpModeIdx = j;
                g_srpDropdownOpen = false;
                InvalidateRect(hw, 0, 0);
                return;
            }
        }
        g_srpDropdownOpen = false;
        InvalidateRect(hw, 0, 0);
        return;
    }

    if (g_chSWOpen) {
        for (int j = 0; j < 2; ++j) if R(g_dropdownRects[j]) { g_chSWMode = j; g_chSWOpen = false; InvalidateRect(hw,0,0); return; }
        g_chSWOpen = false; InvalidateRect(hw,0,0); return;
    }

    // 点击 SRP 下拉框主体
    if R(g_ddSrpRect) {
        g_srpDropdownOpen = !g_srpDropdownOpen;
        g_chSWOpen = false; // 互斥关闭另一个下拉框
        cf();
        InvalidateRect(hw, 0, 0);
        return;
    }
    // 修改原有的 CHSW 下拉框点击逻辑 (加入互斥关闭)
    if R(g_chSWDropRect) {
        g_chSWOpen = !g_chSWOpen;
        g_srpDropdownOpen = false; // 互斥关闭另一个下拉框
        cf();
        InvalidateRect(hw, 0, 0);
        return;
    }
    if R(g_sv) { OpenAutoexecFile(hw); RefreshTextguiOverlay(); InvalidateRect(hw,0,0); return; }
    if R(g_rl) { g_lastWriteTime = 0; LoadCfg(); cf(); InvalidateRect(hw,0,0); return; }
    if R(g_sd) { Append(SOCD_BLOCK); cf(); RefreshTextguiOverlay(); InvalidateRect(hw,0,0); return; }
    if R(g_rsd) {
        if (RemoveSave(L"//--StrikeSense SOCD--", L"//--StrikeSense SOCD END--")) {
            g_autoexecStatus = _(i18n::Keys::LEGAL_SAVED); g_isStatusSaved = true;
        } else {
            g_autoexecStatus = _(i18n::Keys::LEGAL_NOT_FOUND); g_isStatusSaved = false;
        }
        cf(); RefreshTextguiOverlay(); InvalidateRect(hw,0,0); return;
    }
    if R(g_mw) { Append(MWHEELJUMP_BLOCK); cf(); RefreshTextguiOverlay(); InvalidateRect(hw,0,0); return; }
    if R(g_rmw) {
        if (RemoveSave(L"//--StrikeSense MwheelJump--", L"//--StrikeSense MwheelJump END--")) {
            g_autoexecStatus = _(i18n::Keys::LEGAL_SAVED); g_isStatusSaved = true;
        } else {
            g_autoexecStatus = _(i18n::Keys::LEGAL_NOT_FOUND); g_isStatusSaved = false;
        }
        cf(); RefreshTextguiOverlay(); InvalidateRect(hw,0,0); return;
    }
    if R(g_msw) { Append(BuildMS()); cf(); RefreshTextguiOverlay(); InvalidateRect(hw,0,0); return; }
    if R(g_rmsw) {
        if (RemoveSave(L"//--StrikeSense MS--", L"//--StrikeSense MS END--")) {
            g_autoexecStatus = _(i18n::Keys::LEGAL_SAVED); g_isStatusSaved = true;
        } else {
            g_autoexecStatus = _(i18n::Keys::LEGAL_NOT_FOUND); g_isStatusSaved = false;
        }
        cf(); RefreshTextguiOverlay(); InvalidateRect(hw,0,0); return;
    }
    if R(g_csw) { Append(BuildCHSW()); cf(); RefreshTextguiOverlay(); InvalidateRect(hw,0,0); return; }
    if R(g_rcsw) {
        if (RemoveSave(L"//--StrikeSense CrosshairSW--", L"//--StrikeSense CrosshairSW END--")) {
            g_autoexecStatus = _(i18n::Keys::LEGAL_SAVED); g_isStatusSaved = true;
        } else {
            g_autoexecStatus = _(i18n::Keys::LEGAL_NOT_FOUND); g_isStatusSaved = false;
        }
        cf(); RefreshTextguiOverlay(); InvalidateRect(hw,0,0); return;


    }

    // ==================== 新增：SRP 写入和移除 ====================
    if R(g_btnSrpAddRect) { Append(BuildSRP()); cf(); RefreshTextguiOverlay(); InvalidateRect(hw, 0, 0); return; }
    if R(g_btnSrpRemRect) {
        if (RemoveSave(L"//--StrikeSense SRP--", L"//--StrikeSense SRP END--")) {
            g_autoexecStatus = _(i18n::Keys::LEGAL_SAVED); g_isStatusSaved = true;
        }
        else {
            g_autoexecStatus = _(i18n::Keys::LEGAL_NOT_FOUND); g_isStatusSaved = false;
        }
        cf(); RefreshTextguiOverlay(); InvalidateRect(hw, 0, 0); return;
    }
    // ==============================================================

    if R(g_msN) { cf(); g_editingMSnormal = true; SetFocus(hw); InvalidateRect(hw,0,0); return; }
    if R(g_msA) { cf(); g_editingMSattack = true; SetFocus(hw); InvalidateRect(hw,0,0); return; }
    if R(g_er) {
        cf(); g_editing = true; g_editBuffer = g_autoexecContent;
        g_autoexecManualScroll = false;
        int ry = my - (int)g_er.Y - 4, rx = mx - (int)g_er.X - 14;
        int cl = ry / LH + g_scrollOffset, cc = 0;
        auto l = ToLines(g_editBuffer); if (l.empty()) l.push_back(L""); if (cl < 0) cl = 0;
        if (cl >= (int)l.size()) { cl = (int)l.size()-1; cc = (int)l[cl].length(); }
        else { cc = EditorColumnFromX(hw, l[cl], rx); }
        g_cursorPos = PosFromLC(l, cl, cc); SetFocus(hw); InvalidateRect(hw,0,0); return;
    }
    cf(); InvalidateRect(hw,0,0);
    #undef R
}

void ClearEditingFocus() { ExitAutoexecEditAndSave(); }

bool ProcessLegalCfgKeyInput(HWND hw, UINT msg, WPARAM wp, LPARAM) {
    auto lines = ToLines(g_editBuffer);
    if (g_editing && msg == WM_MOUSEWHEEL) {
        const int delta = GET_WHEEL_DELTA_WPARAM(wp);
        const int step = delta > 0 ? -3 : 3;
        const int maxScroll = (std::max)(0, static_cast<int>(lines.size()) - g_visibleLines);
        g_scrollOffset = std::clamp(g_scrollOffset + step, 0, maxScroll);
        g_autoexecManualScroll = true;
        InvalidateRect(hw, 0, 0);
        return true;
    }
    if (g_editing && msg == WM_CHAR) {
        g_autoexecManualScroll = false;
        wchar_t ch = (wchar_t)wp;
        if (ch >= 32 && ch <= 126) { g_editBuffer.insert(g_cursorPos, 1, ch); g_cursorPos++; InvalidateRect(hw,0,0); return true; }
        if (ch == 13) { g_editBuffer.insert(g_cursorPos, 1, L'\n'); g_cursorPos++; InvalidateRect(hw,0,0); return true; }
        return false;
    }
    if (g_editing && msg == WM_KEYDOWN) {
        g_autoexecManualScroll = false;
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
        if ((ch >= '0' && ch <= '9') || ch == '.') { *t += ch; RefreshTextguiOverlay(); InvalidateRect(hw,0,0); return true; }
        if (ch == 8 && !t->empty()) { t->pop_back(); RefreshTextguiOverlay(); InvalidateRect(hw,0,0); return true; }
        return false;
    }
    if ((g_editingMSnormal||g_editingMSattack) && msg == WM_KEYDOWN) {
        std::wstring* t = g_editingMSnormal ? &g_msNormal : &g_msAttack;
        UINT vk = (UINT)wp;
        if (vk == VK_BACK && !t->empty()) { t->pop_back(); RefreshTextguiOverlay(); InvalidateRect(hw,0,0); return true; }
        return false;
    }
    return false;
}

void InitLegalCfgPage() {
    g_lastWriteTime = 0; g_editing = false; g_editingMSnormal = false; g_editingMSattack = false; g_chSWOpen = false; g_srpDropdownOpen = false;
    LoadCfg(); g_editBuffer = g_autoexecContent;
}
