#include "pages.h"
#include "vscrpit.h"

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
    SolidBrush bg(Color(255, 80, 180, 240));
    SolidBrush fg(Color(255, 255, 255, 255));
    Pen border(Color(255, 45, 130, 205), 1.0f);
    Font font(L"Microsoft YaHei", 10, FontStyleBold);
    GraphicsPath path;
    path.AddArc(r.X, r.Y, 8.f, 8.f, 180.f, 90.f);
    path.AddArc(r.X + r.Width - 8.f, r.Y, 8.f, 8.f, 270.f, 90.f);
    path.AddArc(r.X + r.Width - 8.f, r.Y + r.Height - 8.f, 8.f, 8.f, 0.f, 90.f);
    path.AddArc(r.X, r.Y + r.Height - 8.f, 8.f, 8.f, 90.f, 90.f);
    path.CloseFigure();
    g.FillPath(&bg, &path);
    g.DrawPath(&border, &path);
    g.DrawString(text, -1, &font, PointF(r.X + 12, r.Y + 6), &fg);
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
    ofn.lpstrFilter = L"StrikeSense 脚本\0*.vscrpit;*.vscript;*.txt\0所有文件\0*.*\0";
    ofn.lpstrInitialDir = initDir.c_str();
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameW(&ofn)) return file;
    return L"";
}

std::wstring CompactName(const std::wstring& path)
{
    fs::path p(path);
    std::wstring name = p.filename().wstring();
    if (name.size() > 36) name = name.substr(0, 33) + L"...";
    return name;
}

} // namespace

void PaintVscriptPage(Gdiplus::Graphics& g, int cx, int cw, int H, HWND)
{
    using namespace Gdiplus;
    ui::DrawHeader(g, cx, cw, L"自定脚本");

    g_contRects.clear();
    g_runRects.clear();
    g_removeRects.clear();

    Font textFont(L"Microsoft YaHei", 10);
    Font smallFont(L"Microsoft YaHei", 9);
    Font boldFont(L"Microsoft YaHei", 10, FontStyleBold);
    SolidBrush text(Color(255, 30, 60, 100));
    SolidBrush dim(Color(255, 90, 115, 145));
    SolidBrush rowBg(Color(255, 232, 244, 252));
    SolidBrush warn(Color(255, 170, 80, 50));
    Pen rowPen(Color(255, 170, 210, 235));

    g_mountRect = RectF((REAL)cx + 10, 56, 120, 30);
    g_openDirRect = RectF((REAL)cx + 140, 56, 120, 30);
    DrawButton(g, g_mountRect, L"挂载脚本");
    DrawButton(g, g_openDirRect, L"打开目录");

    wchar_t cap[128]{};
    swprintf_s(cap, L"构建等级: %d    运行能力: %d    OEM: %s",
        (int)vscrpit::GetBuildCode(),
        (int)vscrpit::GetRuntimeCapability(),
        vscrpit::IsOemUnlockValid() ? L"有效" : L"无效");
    g.DrawString(cap, -1, &smallFont, PointF((REAL)cx + 280, 62), &dim);

    g.DrawString(L"已挂载脚本", -1, &boldFont, PointF((REAL)cx + 10, 106), &text);
    g.DrawString(L"持续", -1, &smallFont, PointF((REAL)cx + cw - 230, 106), &dim);
    g.DrawString(L"执行", -1, &smallFont, PointF((REAL)cx + cw - 155, 106), &dim);
    g.DrawString(L"卸载", -1, &smallFont, PointF((REAL)cx + cw - 80, 106), &dim);

    const auto& scripts = vscrpit::MountedScripts();
    if (scripts.empty()) {
        g.DrawString(L"暂无挂载脚本。示范脚本已生成在脚本目录，可以先挂载 death_douyin.vscrpit。", -1, &textFont, PointF((REAL)cx + 10, 140), &dim);
        return;
    }

    int y = 132;
    for (size_t i = 0; i < scripts.size(); ++i) {
        RectF row((REAL)cx + 8, (REAL)y, (REAL)cw - 16, 42);
        g.FillRectangle(&rowBg, row);
        g.DrawRectangle(&rowPen, row);

        std::wstring name = CompactName(scripts[i].path);
        g.DrawString(name.c_str(), -1, &textFont, PointF((REAL)cx + 18, (REAL)y + 6), &text);
        g.DrawString(scripts[i].path.c_str(), -1, &smallFont, PointF((REAL)cx + 18, (REAL)y + 24), &dim);

        RectF cont((REAL)cx + cw - 230, (REAL)y + 9, 50, 24);
        RectF run((REAL)cx + cw - 160, (REAL)y + 7, 56, 28);
        RectF remove((REAL)cx + cw - 86, (REAL)y + 7, 56, 28);
        g_contRects.push_back(cont);
        g_runRects.push_back(run);
        g_removeRects.push_back(remove);

        ui::DrawToggle(g, (int)cont.X, (int)cont.Y, scripts[i].continuous);
        if (!scripts[i].continuous) DrawButton(g, run, L"执行");
        else g.DrawString(L"轮询中", -1, &smallFont, PointF(run.X + 8, run.Y + 7), &warn);
        DrawButton(g, remove, L"卸载");

        y += 50;
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
