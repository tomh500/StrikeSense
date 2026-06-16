#include "pages.h"
#include "steam_helper.h"
#include <fstream>
#include <sstream>
#include <filesystem>

namespace fs = std::filesystem;

static std::wstring g_autoexecPath;
static std::wstring g_autoexecContent;
static std::wstring g_autoexecStatus = L"未加载";
static ULONGLONG g_lastWriteTime = 0;

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
    if (wt == g_lastWriteTime && !g_autoexecContent.empty()) return;
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
    auto ft = fs::last_write_time(g_autoexecPath);
    g_lastWriteTime = ft.time_since_epoch().count();
}

static Gdiplus::RectF g_editRect, g_saveBtnRect, g_reloadBtnRect;

void PaintLegalCfgPage(Gdiplus::Graphics& g, int cx, int cw, int H, HWND) {
    using namespace Gdiplus;
    ui::DrawHeader(g, cx, cw, L"合法配置");
    Font rF(L"Microsoft YaHei", 11), sF(L"Microsoft YaHei", 9);
    SolidBrush tdCol(Color(255, 30, 60, 100)), tbCol(Color(255, 20, 80, 140)), tmDim(Color(255, 100, 130, 160));
    SolidBrush sFill(Color(255, 80, 180, 240)); Pen btnP(Color(255, 100, 170, 220));

    g.DrawString(g_autoexecPath.c_str(), -1, &sF, PointF(cx + 10, 50), &tmDim);
    wchar_t sb[64]; swprintf_s(sb, L"状态: %s", g_autoexecStatus.c_str());
    g.DrawString(sb, -1, &sF, PointF(cx + 10, 65), g_autoexecStatus == L"已保存" ? &tbCol : &tdCol);

    int editY = 85, editH = H - editY - 60, editW = cw - 20;
    g_editRect = RectF((REAL)(cx + 10), (REAL)editY, (REAL)editW, (REAL)editH);
    SolidBrush editBg(Color(255, 250, 250, 255)); Pen editPen(Color(255, 180, 200, 220));
    g.FillRectangle(&editBg, g_editRect); g.DrawRectangle(&editPen, g_editRect);

    std::wstringstream ss(g_autoexecContent);
    std::wstring line; int lineY = editY + 4;
    while (std::getline(ss, line)) {
        if (lineY > editY + editH - 20) break;
        g.DrawString(line.c_str(), -1, &sF, PointF(cx + 14, lineY), &tdCol); lineY += 18;
    }

    int btnY = editY + editH + 10, btnW = 80, btnH = 26;
    g_saveBtnRect = RectF((REAL)(cx + 10), (REAL)btnY, (REAL)btnW, (REAL)btnH);
    GraphicsPath sp; sp.AddArc(cx + 10, btnY, 16, 16, 180, 90);
    sp.AddArc(cx + 10 + btnW - 16, btnY, 16, 16, 270, 90);
    sp.AddArc(cx + 10 + btnW - 16, btnY + btnH - 16, 16, 16, 0, 90);
    sp.AddArc(cx + 10, btnY + btnH - 16, 16, 16, 90, 90); sp.CloseFigure();
    g.FillPath(&sFill, &sp); g.DrawPath(&btnP, &sp);
    g.DrawString(L"保存", -1, &sF, PointF(cx + 10 + 20, btnY + 6), &tbCol);

    g_reloadBtnRect = RectF((REAL)(cx + 10 + btnW + 10), (REAL)btnY, (REAL)btnW, (REAL)btnH);
    GraphicsPath rp; int rx = cx + 10 + btnW + 10;
    rp.AddArc(rx, btnY, 16, 16, 180, 90); rp.AddArc(rx + btnW - 16, btnY, 16, 16, 270, 90);
    rp.AddArc(rx + btnW - 16, btnY + btnH - 16, 16, 16, 0, 90);
    rp.AddArc(rx, btnY + btnH - 16, 16, 16, 90, 90); rp.CloseFigure();
    SolidBrush rlBg(Color(255, 140, 200, 240));
    g.FillPath(&rlBg, &rp); g.DrawPath(&btnP, &rp);
    g.DrawString(L"刷新", -1, &sF, PointF(rx + 20, btnY + 6), &tbCol);
}

void CheckLegalCfgClick(HWND hw, int mx, int my) {
    if (mx >= g_saveBtnRect.X && mx <= g_saveBtnRect.X + g_saveBtnRect.Width &&
        my >= g_saveBtnRect.Y && my <= g_saveBtnRect.Y + g_saveBtnRect.Height) {
        SaveAutoexecContent(g_autoexecContent);
        InvalidateRect(hw, nullptr, FALSE); return;
    }
    if (mx >= g_reloadBtnRect.X && mx <= g_reloadBtnRect.X + g_reloadBtnRect.Width &&
        my >= g_reloadBtnRect.Y && my <= g_reloadBtnRect.Y + g_reloadBtnRect.Height) {
        g_lastWriteTime = 0; LoadAutoexecContent();
        InvalidateRect(hw, nullptr, FALSE); return;
    }
    if (mx >= g_editRect.X && mx <= g_editRect.X + g_editRect.Width &&
        my >= g_editRect.Y && my <= g_editRect.Y + g_editRect.Height) {
        if (!g_autoexecPath.empty() && fs::exists(g_autoexecPath))
            ShellExecuteW(nullptr, L"open", L"notepad.exe", g_autoexecPath.c_str(), nullptr, SW_SHOW);
        else {
            // 文件不存在 - 创建空文件并打开
            std::ofstream create(g_autoexecPath);
            if (create.is_open()) { create.close(); g_autoexecContent = L""; g_autoexecStatus = L"已创建"; }
            if (!g_autoexecPath.empty()) ShellExecuteW(nullptr, L"open", L"notepad.exe", g_autoexecPath.c_str(), nullptr, SW_SHOW);
        }
        InvalidateRect(hw, nullptr, FALSE);
    }
}

void InitLegalCfgPage() { g_lastWriteTime = 0; LoadAutoexecContent(); }