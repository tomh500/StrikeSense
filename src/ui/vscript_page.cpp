#include "pages.h"
#include "vscript.h"
#include "i18n.h"
#include "textgui_overlay.h"

#include <algorithm>
#include <commdlg.h>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace {

Gdiplus::RectF g_mountRect;
Gdiplus::RectF g_openDirRect;
Gdiplus::RectF g_previousPageRect;
Gdiplus::RectF g_nextPageRect;
std::vector<Gdiplus::RectF> g_contRects;
std::vector<Gdiplus::RectF> g_runRects;
std::vector<Gdiplus::RectF> g_removeRects;
std::vector<Gdiplus::RectF> g_noticeRects;
std::vector<size_t> g_visibleScriptIndices;
size_t g_currentScriptPage = 0;
size_t g_scriptRowsPerPage = 1;

bool Hit(const Gdiplus::RectF& r, int x, int y)
{
    return x >= r.X && x <= r.X + r.Width && y >= r.Y && y <= r.Y + r.Height;
}

void AddRoundedRect(Gdiplus::GraphicsPath& path, const Gdiplus::RectF& rect, float radius)
{
    const float d = (std::min)(radius * 2.0f, (std::min)(rect.Width, rect.Height));
    path.AddArc(rect.X, rect.Y, d, d, 180.0f, 90.0f);
    path.AddArc(rect.X + rect.Width - d, rect.Y, d, d, 270.0f, 90.0f);
    path.AddArc(rect.X + rect.Width - d, rect.Y + rect.Height - d, d, d, 0.0f, 90.0f);
    path.AddArc(rect.X, rect.Y + rect.Height - d, d, d, 90.0f, 90.0f);
    path.CloseFigure();
}

void DrawRoundedPanel(Gdiplus::Graphics& g, const Gdiplus::RectF& rect,
    Gdiplus::Brush& background, Gdiplus::Pen& border)
{
    Gdiplus::GraphicsPath path;
    AddRoundedRect(path, rect, 10.0f);
    g.FillPath(&background, &path);
    g.DrawPath(&border, &path);
}

void DrawButton(Gdiplus::Graphics& g, const Gdiplus::RectF& r, const wchar_t* text)
{
    ui::DrawRoundedButton(g, r, text);
}

std::wstring PickScript(HWND owner)
{
    wchar_t file[MAX_PATH] = {};
    std::wstring initDir = vscript::GetDefaultScriptDir();
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = owner;
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"StrikeSense Script\0*.vscript;*.vscrpit;*.txt\0All Files\0*.*\0";
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

std::wstring DisplayName(const vscript::mounted_script& script)
{
    if (script.hasMetadataName) return vscript::GetScriptDisplayName(script);
    return CompactFileName(script.path);
}

std::wstring SummaryText(const vscript::mounted_script& script)
{
    std::wstring out;
    if (!script.provider.empty()) out += L"Provider: " + script.provider + L"  ";
    if (!script.author.empty()) out += L"Author: " + script.author + L"  ";
    if (!script.version.empty()) out += L"Version: " + script.version;
    if (script.dangerStyle) out += L"  [高权限]";
    if (script.missing) out += L"  [文件丢失]";
    if (out.empty()) out = script.path;
    return out;
}

void DrawNoticeIcon(Gdiplus::Graphics& g, const Gdiplus::RectF& r, bool danger)
{
    using namespace Gdiplus;
    const auto& theme = uitheme::get_palette();
    SolidBrush bg(theme.notice_background);
    SolidBrush fg(danger ? theme.danger : theme.notice_text);
    Pen border(danger ? theme.danger : theme.notice_border, 1.25f);
    Font f(L"Microsoft YaHei", 9, FontStyleBold);
    g.FillEllipse(&bg, r);
    g.DrawEllipse(&border, r);
    g.DrawString(L"!", -1, &f, PointF(r.X + 5, r.Y + 1), &fg);
}

void DrawTooltip(Gdiplus::Graphics& g, const std::wstring& text, int x, int y)
{
    using namespace Gdiplus;
    const auto& theme = uitheme::get_palette();
    if (text.empty()) return;
    Font f(L"Microsoft YaHei", 9);
    SolidBrush bg(theme.tooltip_background);
    SolidBrush fg(theme.tooltip_text);
    Pen border(theme.tooltip_border, 1.0f);
    StringFormat fmt;
    fmt.SetTrimming(StringTrimmingWord);
    fmt.SetFormatFlags(StringFormatFlagsLineLimit);
    constexpr REAL kMinWidth = 180.f;
    constexpr REAL kMaxWidth = 420.f;
    constexpr REAL kPaddingX = 10.f;
    constexpr REAL kPaddingY = 8.f;

    RectF measureSingle(0.f, 0.f, 4096.f, 200.f);
    RectF singleBound;
    g.MeasureString(text.c_str(), -1, &f, measureSingle, &fmt, &singleBound);

    REAL innerWidth = std::clamp(singleBound.Width + 6.f, kMinWidth, kMaxWidth);
    RectF measureWrap(0.f, 0.f, innerWidth, 240.f);
    RectF wrapBound;
    g.MeasureString(text.c_str(), -1, &f, measureWrap, &fmt, &wrapBound);

    REAL boxWidth = innerWidth + kPaddingX * 2.f;
    REAL boxHeight = (std::max)(32.f, wrapBound.Height + kPaddingY * 2.f);
    RectF box((REAL)x + 14, (REAL)y + 16, boxWidth, boxHeight);
    GraphicsPath p;
    p.AddArc(box.X, box.Y, 10.f, 10.f, 180.f, 90.f);
    p.AddArc(box.X + box.Width - 10.f, box.Y, 10.f, 10.f, 270.f, 90.f);
    p.AddArc(box.X + box.Width - 10.f, box.Y + box.Height - 10.f, 10.f, 10.f, 0.f, 90.f);
    p.AddArc(box.X, box.Y + box.Height - 10.f, 10.f, 10.f, 90.f, 90.f);
    p.CloseFigure();
    g.FillPath(&bg, &p);
    g.DrawPath(&border, &p);
    RectF textBox(box.X + kPaddingX, box.Y + kPaddingY, box.Width - kPaddingX * 2.f, box.Height - kPaddingY * 2.f);
    g.DrawString(text.c_str(), -1, &f, textBox, &fmt, &fg);
}

} // namespace

void PaintVscriptPage(Gdiplus::Graphics& g, int cx, int cw, int H, HWND hw)
{
    using namespace Gdiplus;
    const auto& theme = uitheme::get_palette();
    ui::DrawHeader(g, cx, cw, i18n::T("VSCRIPT_TITLE"));

    g_contRects.clear();
    g_runRects.clear();
    g_removeRects.clear();
    g_noticeRects.clear();
    g_visibleScriptIndices.clear();

    Font textFont(L"Microsoft YaHei", 10);
    Font smallFont(L"Microsoft YaHei", 9);
    Font boldFont(L"Microsoft YaHei", 10, FontStyleBold);
    SolidBrush text(theme.text);
    SolidBrush dim(theme.dim);
    SolidBrush meta(theme.accent_strong);
    SolidBrush rowBg(theme.card_background);
    SolidBrush rowDangerBg(theme.warning);
    SolidBrush warn(theme.warning);
    SolidBrush dangerText(theme.danger);
    Pen rowPen(theme.card_border);
    Pen rowDangerPen(theme.danger);

    g_mountRect = RectF((REAL)cx + 10, 56, 116, 28);
    g_openDirRect = RectF((REAL)cx + 136, 56, 116, 28);
    DrawButton(g, g_mountRect, i18n::T("VSCRIPT_MOUNT"));
    DrawButton(g, g_openDirRect, i18n::T("VSCRIPT_OPEN_DIR"));

    wchar_t cap[128]{};
    swprintf_s(cap, L"%s: %d    %s: %d    OEM: %s",
        i18n::T("VSCRIPT_BUILD"),
        (int)vscript::GetBuildCode(),
        i18n::T("VSCRIPT_CAPABILITY"),
        (int)vscript::GetRuntimeCapability(),
        vscript::IsOemUnlockValid() ? i18n::T("SOUNDS_ENABLED") : i18n::T("SOUNDS_DISABLED"));
    g.DrawString(cap, -1, &smallFont, PointF((REAL)cx + 270, 62), &dim);

    g.DrawString(i18n::T("VSCRIPT_MOUNTED"), -1, &boldFont, PointF((REAL)cx + 10, 106), &text);
    g.DrawString(i18n::T("VSCRIPT_CONTINUOUS"), -1, &smallFont, PointF((REAL)cx + cw - 230, 106), &dim);
    g.DrawString(i18n::T("VSCRIPT_RUN"), -1, &smallFont, PointF((REAL)cx + cw - 155, 106), &dim);
    g.DrawString(i18n::T("VSCRIPT_REMOVE"), -1, &smallFont, PointF((REAL)cx + cw - 80, 106), &dim);

    const auto& scripts = vscript::MountedScripts();
    if (scripts.empty()) {
        g_currentScriptPage = 0;
        g_previousPageRect = RectF{};
        g_nextPageRect = RectF{};
        g.DrawString(i18n::T("VSCRIPT_EMPTY"), -1, &textFont, PointF((REAL)cx + 10, 140), &dim);
        return;
    }

    constexpr int kListTop = 132;
    constexpr int kRowStep = 56;
    constexpr int kFooterHeight = 54;
    g_scriptRowsPerPage = static_cast<size_t>((std::max)(1, (H - kListTop - kFooterHeight) / kRowStep));
    const size_t pageCount = (scripts.size() + g_scriptRowsPerPage - 1) / g_scriptRowsPerPage;
    if (g_currentScriptPage >= pageCount) g_currentScriptPage = pageCount - 1;
    const size_t firstScript = g_currentScriptPage * g_scriptRowsPerPage;
    const size_t lastScript = (std::min)(scripts.size(), firstScript + g_scriptRowsPerPage);

    int y = kListTop;
    for (size_t scriptIndex = firstScript; scriptIndex < lastScript; ++scriptIndex) {
        RectF row((REAL)cx + 8, (REAL)y, (REAL)cw - 16, 48);
        const bool danger = scripts[scriptIndex].dangerStyle;
        DrawRoundedPanel(g, row, danger ? static_cast<Brush&>(rowDangerBg) : static_cast<Brush&>(rowBg),
            danger ? rowDangerPen : rowPen);

        std::wstring name = DisplayName(scripts[scriptIndex]);
        g.DrawString(name.c_str(), -1, &textFont, PointF((REAL)cx + 18, (REAL)y + 5), danger ? &dangerText : (scripts[scriptIndex].hasMetadataName ? &meta : &text));
        std::wstring sub = scripts[scriptIndex].hasMetadataName ? SummaryText(scripts[scriptIndex]) : scripts[scriptIndex].path;
        g.DrawString(sub.c_str(), -1, &smallFont, PointF((REAL)cx + 18, (REAL)y + 25), &dim);

        RectF notice((REAL)cx + 260, (REAL)y + 7, 18, 18);
        if (!scripts[scriptIndex].notice.empty()) DrawNoticeIcon(g, notice, danger);
        g_noticeRects.push_back(notice);
        g_visibleScriptIndices.push_back(scriptIndex);

        RectF cont((REAL)cx + cw - 230, (REAL)y + 12, 50, 24);
        RectF run((REAL)cx + cw - 160, (REAL)y + 10, 56, 28);
        RectF remove((REAL)cx + cw - 86, (REAL)y + 10, 56, 28);
        g_contRects.push_back(cont);
        g_runRects.push_back(run);
        g_removeRects.push_back(remove);

        ui::DrawToggle(g, (int)cont.X, (int)cont.Y, scripts[scriptIndex].continuous);
        if (!scripts[scriptIndex].continuous) DrawButton(g, run, i18n::T("VSCRIPT_RUN"));
        else g.DrawString(i18n::T("VSCRIPT_POLLING"), -1, &smallFont, PointF(run.X + 8, run.Y + 7), &warn);
        DrawButton(g, remove, i18n::T("VSCRIPT_REMOVE"));

        y += kRowStep;
    }

    const int footerY = H - 42;
    wchar_t pageText[96]{};
    swprintf_s(pageText, i18n::T("VSCRIPT_PAGE"),
        static_cast<int>(g_currentScriptPage + 1), static_cast<int>(pageCount), static_cast<int>(scripts.size()));
    g.DrawString(pageText, -1, &smallFont, PointF((REAL)cx + 12, (REAL)footerY + 7), &dim);

    if (pageCount > 1) {
        g_previousPageRect = RectF((REAL)cx + cw - 222, (REAL)footerY, 96, 28);
        g_nextPageRect = RectF((REAL)cx + cw - 116, (REAL)footerY, 96, 28);
        DrawButton(g, g_previousPageRect, i18n::T("VSCRIPT_PREVIOUS"));
        DrawButton(g, g_nextPageRect, i18n::T("VSCRIPT_NEXT"));
    } else {
        g_previousPageRect = RectF{};
        g_nextPageRect = RectF{};
    }

    if (hw) {
        POINT pt{};
        GetCursorPos(&pt);
        ScreenToClient(hw, &pt);
        for (size_t i = 0; i < g_noticeRects.size() && i < g_visibleScriptIndices.size(); ++i) {
            if (Hit(g_noticeRects[i], pt.x, pt.y)) {
                const size_t scriptIndex = g_visibleScriptIndices[i];
                if (scriptIndex < scripts.size()) DrawTooltip(g, scripts[scriptIndex].notice, pt.x, pt.y);
                break;
            }
        }
    }
}

void CheckVscriptClick(HWND hw, int mx, int my)
{
    if (Hit(g_mountRect, mx, my)) {
        std::wstring path = PickScript(hw);
        if (!path.empty()) {
            vscript::AddMountedScript(path);
            const auto& scripts = vscript::MountedScripts();
            if (!scripts.empty()) g_currentScriptPage = (scripts.size() - 1) / g_scriptRowsPerPage;
            std::wcout << L"[脚本页面] 已挂载脚本: " << path << std::endl;
            std::cout << "[脚本页面] 已切换到新挂载脚本所在页: " << (g_currentScriptPage + 1) << std::endl;
        }
        InvalidateRect(hw, nullptr, FALSE);
        return;
    }
    if (Hit(g_openDirRect, mx, my)) {
        ShellExecuteW(hw, L"open", vscript::GetDefaultScriptDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        return;
    }
    if (Hit(g_previousPageRect, mx, my)) {
        if (g_currentScriptPage > 0) --g_currentScriptPage;
        std::cout << "[脚本页面] 已切换到上一页: " << (g_currentScriptPage + 1) << std::endl;
        InvalidateRect(hw, nullptr, FALSE);
        return;
    }
    if (Hit(g_nextPageRect, mx, my)) {
        const auto& scripts = vscript::MountedScripts();
        const size_t pageCount = scripts.empty() ? 1 : (scripts.size() + g_scriptRowsPerPage - 1) / g_scriptRowsPerPage;
        if (g_currentScriptPage + 1 < pageCount) ++g_currentScriptPage;
        std::cout << "[脚本页面] 已切换到下一页: " << (g_currentScriptPage + 1) << std::endl;
        InvalidateRect(hw, nullptr, FALSE);
        return;
    }
    for (size_t i = 0; i < g_contRects.size(); ++i) {
        if (i >= g_visibleScriptIndices.size()) break;
        const size_t scriptIndex = g_visibleScriptIndices[i];
        if (Hit(g_contRects[i], mx, my)) {
            vscript::ToggleContinuous(scriptIndex);
            RefreshTextguiOverlay();
            InvalidateRect(hw, nullptr, FALSE);
            return;
        }
        if (Hit(g_runRects[i], mx, my)) {
            auto& scripts = vscript::MountedScripts();
            if (scriptIndex < scripts.size() && !scripts[scriptIndex].continuous)
                vscript::ExecuteScriptFile(scripts[scriptIndex].path);
            InvalidateRect(hw, nullptr, FALSE);
            return;
        }
        if (Hit(g_removeRects[i], mx, my)) {
            vscript::RemoveMountedScript(scriptIndex);
            std::cout << "[脚本页面] 已卸载第 " << (scriptIndex + 1) << " 个脚本" << std::endl;
            InvalidateRect(hw, nullptr, FALSE);
            return;
        }
    }
}
