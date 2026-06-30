#include "pages.h"
#include "vscrpit.h"
#include "i18n.h"

#include <commdlg.h>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace {

Gdiplus::RectF g_mountRect;
Gdiplus::RectF g_openDirRect;
std::vector<Gdiplus::RectF> g_contRects;
std::vector<Gdiplus::RectF> g_runRects;
std::vector<Gdiplus::RectF> g_removeRects;

bool Hit(const Gdiplus::RectF& r, int x, int y)
{
    return x >= r.X && x <= r.X + r.Width && y >= r.Y && y <= r.Y + r.Height;
}

void DrawButton(Gdiplus::Graphics& g, const Gdiplus::RectF& r, const wchar_t* text)
{
    using namespace Gdiplus;
    SolidBrush bg(Color(255, 180, 220, 245));
    SolidBrush fg(Color(255, 20, 80, 140));
    Pen border(Color(255, 130, 190, 230), 1.0f);
    Font font(L"Microsoft YaHei", 9, FontStyleBold);
    GraphicsPath p;
    p.AddArc(r.X, r.Y, 16.f, 16.f, 180.f, 90.f);
    p.AddArc(r.X + r.Width - 16.f, r.Y, 16.f, 16.f, 270.f, 90.f);
    p.AddArc(r.X + r.Width - 16.f, r.Y + r.Height - 16.f, 16.f, 16.f, 0.f, 90.f);
    p.AddArc(r.X, r.Y + r.Height - 16.f, 16.f, 16.f, 90.f, 90.f);
    p.CloseFigure();
    g.FillPath(&bg, &p);
    g.DrawPath(&border, &p);
    g.DrawString(text, -1, &font, PointF(r.X + 8, r.Y + 5), &fg);
}

std::wstring PickScript(HWND owner)
{
    wchar_t file[MAX_PATH] = {};
    std::wstring initDir = vscrpit::GetDefaultScriptDir();
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"StrikeSense Script\0*.vscrpit;*.vscript;*.txt\0All Files\0*.*\0";
    ofn.lpstrInitialDir = initDir.c_str();
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameW(&ofn)) return file;
    return L"";
}

std::wstring CompactFileName(const std::wstring& path)
{
    fs::path p(path);
    std::wstring name = p.filename().wstring();
    if (name.size() > 34) name = name.substr(0, 31) + L"...";
    return name;
}

std::wstring DisplayName(const vscrpit::mounted_script& script)
{
    if (script.hasMetadataName) return vscrpit::GetScriptDisplayName(script);
    return CompactFileName(script.path);
}

} // namespace

void PaintVscriptPage(Gdiplus::Graphics& g, int cx, int cw, int H, HWND)
{
    using namespace Gdiplus;
    ui::DrawHeader(g, cx, cw, i18n::T("VSCRIPT_TITLE"));

    g_contRects.clear();
    g_runRects.clear();
    g_removeRects.clear();

    Font textFont(L"Microsoft YaHei", 10);
    Font smallFont(L"Microsoft YaHei", 9);
    Font boldFont(L"Microsoft YaHei", 10, FontStyleBold);
    SolidBrush text(Color(255, 30, 60, 100));
    SolidBrush dim(Color(255, 90, 115, 145));
    SolidBrush meta(Color(255, 35, 125, 170));
    SolidBrush rowBg(Color(255, 232, 244, 252));
    SolidBrush warn(Color(255, 170, 80, 50));
    Pen rowPen(Color(255, 170, 210, 235));

    g_mountRect = RectF((REAL)cx + 10, 56, 116, 28);
    g_openDirRect = RectF((REAL)cx + 136, 56, 116, 28);
    DrawButton(g, g_mountRect, i18n::T("VSCRIPT_MOUNT"));
    DrawButton(g, g_openDirRect, i18n::T("VSCRIPT_OPEN_DIR"));

    wchar_t cap[128]{};
    swprintf_s(cap, L"%s: %d    %s: %d    OEM: %s",
        i18n::T("VSCRIPT_BUILD"),
        (int)vscrpit::GetBuildCode(),
        i18n::T("VSCRIPT_CAPABILITY"),
        (int)vscrpit::GetRuntimeCapability(),
        vscrpit::IsOemUnlockValid() ? i18n::T("SOUNDS_ENABLED") : i18n::T("SOUNDS_DISABLED"));
    g.DrawString(cap, -1, &smallFont, PointF((REAL)cx + 270, 62), &dim);

    g.DrawString(i18n::T("VSCRIPT_MOUNTED"), -1, &boldFont, PointF((REAL)cx + 10, 106), &text);
    g.DrawString(i18n::T("VSCRIPT_CONTINUOUS"), -1, &smallFont, PointF((REAL)cx + cw - 230, 106), &dim);
    g.DrawString(i18n::T("VSCRIPT_RUN"), -1, &smallFont, PointF((REAL)cx + cw - 155, 106), &dim);
    g.DrawString(i18n::T("VSCRIPT_REMOVE"), -1, &smallFont, PointF((REAL)cx + cw - 80, 106), &dim);

    const auto& scripts = vscrpit::MountedScripts();
    if (scripts.empty()) {
        g.DrawString(i18n::T("VSCRIPT_EMPTY"), -1, &textFont, PointF((REAL)cx + 10, 140), &dim);
        return;
    }

    int y = 132;
    for (size_t i = 0; i < scripts.size(); ++i) {
        RectF row((REAL)cx + 8, (REAL)y, (REAL)cw - 16, 48);
        g.FillRectangle(&rowBg, row);
        g.DrawRectangle(&rowPen, row);

        std::wstring name = DisplayName(scripts[i]);
        g.DrawString(name.c_str(), -1, &textFont, PointF((REAL)cx + 18, (REAL)y + 5), scripts[i].hasMetadataName ? &meta : &text);
        std::wstring sub = scripts[i].hasMetadataName ? vscrpit::GetScriptNotice(scripts[i]) : scripts[i].path;
        if (sub.empty()) sub = scripts[i].path;
        g.DrawString(sub.c_str(), -1, &smallFont, PointF((REAL)cx + 18, (REAL)y + 25), &dim);

        RectF cont((REAL)cx + cw - 230, (REAL)y + 12, 50, 24);
        RectF run((REAL)cx + cw - 160, (REAL)y + 10, 56, 28);
        RectF remove((REAL)cx + cw - 86, (REAL)y + 10, 56, 28);
        g_contRects.push_back(cont);
        g_runRects.push_back(run);
        g_removeRects.push_back(remove);

        ui::DrawToggle(g, (int)cont.X, (int)cont.Y, scripts[i].continuous);
        if (!scripts[i].continuous) DrawButton(g, run, i18n::T("VSCRIPT_RUN"));
        else g.DrawString(i18n::T("VSCRIPT_POLLING"), -1, &smallFont, PointF(run.X + 8, run.Y + 7), &warn);
        DrawButton(g, remove, i18n::T("VSCRIPT_REMOVE"));

        y += 56;
        if (y > H - 60) break;
    }
}

void CheckVscriptClick(HWND hw, int mx, int my)
{
    if (Hit(g_mountRect, mx, my)) {
        std::wstring path = PickScript(hw);
        if (!path.empty()) {
            vscrpit::AddMountedScript(path);
            std::wcout << L"[脚本页面] 已挂载脚本: " << path << std::endl;
        }
        InvalidateRect(hw, nullptr, FALSE);
        return;
    }
    if (Hit(g_openDirRect, mx, my)) {
        ShellExecuteW(hw, L"open", vscrpit::GetDefaultScriptDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        return;
    }
    for (size_t i = 0; i < g_contRects.size(); ++i) {
        if (Hit(g_contRects[i], mx, my)) {
            vscrpit::ToggleContinuous(i);
            InvalidateRect(hw, nullptr, FALSE);
            return;
        }
        if (Hit(g_runRects[i], mx, my)) {
            auto& scripts = vscrpit::MountedScripts();
            if (i < scripts.size() && !scripts[i].continuous) vscrpit::ExecuteScriptFile(scripts[i].path);
            InvalidateRect(hw, nullptr, FALSE);
            return;
        }
        if (Hit(g_removeRects[i], mx, my)) {
            vscrpit::RemoveMountedScript(i);
            InvalidateRect(hw, nullptr, FALSE);
            return;
        }
    }
}
