#include "cscript_panel.h"

#include "cscript.h"
#include "i18n.h"
#include "pages.h"
#include "textgui_overlay.h"

#include <algorithm>
#include <commdlg.h>
#include <filesystem>
#include <iostream>
#include <optional>
#include <vector>

namespace cscriptui {
namespace {

constexpr std::size_t kScriptsPerPage = 3;
Gdiplus::RectF g_toggleRect, g_expandRect, g_mountRect, g_openDirectoryRect;
Gdiplus::RectF g_previousPageRect, g_nextPageRect;
std::vector<Gdiplus::RectF> g_bindRects, g_reloadRects, g_removeRects;
std::vector<std::size_t> g_visibleIndices;
std::optional<std::size_t> g_bindingIndex;
std::size_t g_currentPage = 0;
bool g_expanded = false;

bool Hit(const Gdiplus::RectF& rect, int x, int y)
{
    return x >= rect.X && x <= rect.X + rect.Width && y >= rect.Y && y <= rect.Y + rect.Height;
}

void DrawButton(Gdiplus::Graphics& graphics, const Gdiplus::RectF& rect, const wchar_t* label)
{
    using namespace Gdiplus;
    SolidBrush background(Color(255, 180, 220, 245));
    SolidBrush foreground(Color(255, 20, 80, 140));
    Pen border(Color(255, 130, 190, 230), 1.0f);
    Font font(L"Microsoft YaHei", 8, FontStyleBold);
    StringFormat format;
    format.SetAlignment(StringAlignmentCenter);
    format.SetLineAlignment(StringAlignmentCenter);
    graphics.FillRectangle(&background, rect);
    graphics.DrawRectangle(&border, rect);
    graphics.DrawString(label, -1, &font, rect, &format, &foreground);
}

std::filesystem::path PickScript(HWND owner)
{
    wchar_t file[MAX_PATH]{};
    const std::wstring initialDirectory = cscript::GetDefaultScriptDir();
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFile = file;
    dialog.nMaxFile = MAX_PATH;
    dialog.lpstrFilter = L"StrikeSense CScript\0*.cscript\0All Files\0*.*\0";
    dialog.lpstrInitialDir = initialDirectory.c_str();
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    return GetOpenFileNameW(&dialog) ? std::filesystem::path(file) : std::filesystem::path{};
}

std::wstring Compact(std::wstring text, std::size_t maxLength)
{
    if (text.size() > maxLength) text = text.substr(0, maxLength - 3) + L"...";
    return text;
}

void ResetDetails()
{
    g_mountRect = {};
    g_openDirectoryRect = {};
    g_previousPageRect = {};
    g_nextPageRect = {};
    g_bindRects.clear();
    g_reloadRects.clear();
    g_removeRects.clear();
    g_visibleIndices.clear();
}

} // namespace

void PaintSection(Gdiplus::Graphics& graphics, int contentX, int contentWidth, int topY, HWND)
{
    using namespace Gdiplus;
    Font titleFont(L"Microsoft YaHei", 11);
    Font textFont(L"Microsoft YaHei", 9);
    Font smallFont(L"Microsoft YaHei", 8);
    Font boldFont(L"Microsoft YaHei", 9, FontStyleBold);
    SolidBrush text(Color(255, 30, 60, 100));
    SolidBrush dim(Color(255, 90, 115, 145));
    SolidBrush ok(Color(255, 35, 135, 95));
    SolidBrush error(Color(255, 190, 65, 65));
    SolidBrush rowBackground(Color(255, 235, 247, 253));
    Pen rowBorder(Color(255, 165, 210, 235), 1.0f);

    graphics.DrawString(i18n::T("CSCRIPT_SUPPORT"), -1, &titleFont,
        PointF(static_cast<REAL>(contentX + 10), static_cast<REAL>(topY)), &text);
    g_toggleRect = RectF(static_cast<REAL>(contentX + 220), static_cast<REAL>(topY - 4), 50.f, 24.f);
    ui::DrawToggle(graphics, contentX + 220, topY - 4, cscript::IsEnabled());
    g_expandRect = RectF(static_cast<REAL>(contentX + 282), static_cast<REAL>(topY - 5), 70.f, 26.f);
    DrawButton(graphics, g_expandRect, g_expanded ? i18n::T("CSCRIPT_COLLAPSE") : i18n::T("CSCRIPT_EXPAND"));

    ResetDetails();
    if (!g_expanded) return;

    const int detailTop = topY + 70;
    g_mountRect = RectF(static_cast<REAL>(contentX + 10), static_cast<REAL>(detailTop), 112.f, 26.f);
    g_openDirectoryRect = RectF(static_cast<REAL>(contentX + 132), static_cast<REAL>(detailTop), 112.f, 26.f);
    DrawButton(graphics, g_mountRect, i18n::T("CSCRIPT_MOUNT"));
    DrawButton(graphics, g_openDirectoryRect, i18n::T("CSCRIPT_OPEN_DIR"));
    const std::wstring runtimeStatus = Compact(cscript::GetLastRuntimeStatus(), 46);
    graphics.DrawString(runtimeStatus.c_str(), -1, &smallFont,
        PointF(static_cast<REAL>(contentX + 258), static_cast<REAL>(detailTop + 5)), &dim);

    const auto& scripts = cscript::MountedScripts();
    const std::size_t pageCount = (std::max<std::size_t>)(1, (scripts.size() + kScriptsPerPage - 1) / kScriptsPerPage);
    if (g_currentPage >= pageCount) g_currentPage = pageCount - 1;
    const std::size_t first = g_currentPage * kScriptsPerPage;
    const std::size_t last = (std::min)(scripts.size(), first + kScriptsPerPage);
    int rowY = detailTop + 36;

    if (scripts.empty()) {
        graphics.DrawString(i18n::T("CSCRIPT_EMPTY"), -1, &textFont,
            PointF(static_cast<REAL>(contentX + 12), static_cast<REAL>(rowY + 8)), &dim);
    }
    for (std::size_t index = first; index < last; ++index) {
        const auto& script = scripts[index];
        RectF row(static_cast<REAL>(contentX + 8), static_cast<REAL>(rowY),
            static_cast<REAL>(contentWidth - 16), 52.f);
        graphics.FillRectangle(&rowBackground, row);
        graphics.DrawRectangle(&rowBorder, row);
        const std::wstring fileName = Compact(std::filesystem::path(script.path).filename().wstring(), 28);
        graphics.DrawString(fileName.c_str(), -1, &boldFont, PointF(row.X + 8.f, row.Y + 4.f), &text);

        const std::wstring summary = script.valid
            ? L"Pressed " + std::to_wstring(script.pressed_command_count) + L" / Released "
                + std::to_wstring(script.released_command_count)
            : Compact(script.status, 42);
        graphics.DrawString(summary.c_str(), -1, &smallFont,
            PointF(row.X + 8.f, row.Y + 27.f), script.valid ? &ok : &error);

        RectF bind(row.X + row.Width - 244.f, row.Y + 12.f, 76.f, 28.f);
        RectF reload(row.X + row.Width - 160.f, row.Y + 12.f, 68.f, 28.f);
        RectF remove(row.X + row.Width - 84.f, row.Y + 12.f, 68.f, 28.f);
        g_bindRects.push_back(bind);
        g_reloadRects.push_back(reload);
        g_removeRects.push_back(remove);
        g_visibleIndices.push_back(index);

        std::wstring bindText;
        if (g_bindingIndex.has_value() && *g_bindingIndex == index) bindText = i18n::T("CSCRIPT_PRESS_KEY");
        else if (!script.source_key.empty()) bindText = script.source_key;
        else bindText = i18n::T("CSCRIPT_BIND");
        DrawButton(graphics, bind, Compact(bindText, 10).c_str());
        DrawButton(graphics, reload, i18n::T("CSCRIPT_RELOAD"));
        DrawButton(graphics, remove, i18n::T("CSCRIPT_REMOVE"));
        rowY += 58;
    }

    const int footerY = detailTop + 218;
    wchar_t pageText[96]{};
    swprintf_s(pageText, i18n::T("CSCRIPT_PAGE"), static_cast<int>(g_currentPage + 1),
        static_cast<int>(pageCount), static_cast<int>(scripts.size()));
    graphics.DrawString(pageText, -1, &smallFont,
        PointF(static_cast<REAL>(contentX + 10), static_cast<REAL>(footerY + 6)), &dim);
    if (pageCount > 1) {
        g_previousPageRect = RectF(static_cast<REAL>(contentX + contentWidth - 210), static_cast<REAL>(footerY), 92.f, 26.f);
        g_nextPageRect = RectF(static_cast<REAL>(contentX + contentWidth - 108), static_cast<REAL>(footerY), 92.f, 26.f);
        DrawButton(graphics, g_previousPageRect, i18n::T("CSCRIPT_PREVIOUS"));
        DrawButton(graphics, g_nextPageRect, i18n::T("CSCRIPT_NEXT"));
    }
}

bool CheckClick(HWND owner, int mouseX, int mouseY)
{
    if (Hit(g_toggleRect, mouseX, mouseY)) {
        if (!cscript::IsEnabled()) {
            const int answer = MessageBoxW(owner, i18n::T("CSCRIPT_BETA_WARNING"),
                i18n::T("CSCRIPT_BETA_TITLE"), MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
            if (answer != IDYES) return true;
            if (!cscript::SetEnabled(true)) {
                const std::wstring status = cscript::GetLastRuntimeStatus();
                MessageBoxW(owner, status.c_str(), i18n::T("CSCRIPT_START_FAILED"), MB_OK | MB_ICONERROR);
            } else {
                g_expanded = true;
            }
        } else {
            cscript::SetEnabled(false);
        }
        RefreshTextguiOverlay();
        InvalidateRect(owner, nullptr, FALSE);
        return true;
    }
    if (Hit(g_expandRect, mouseX, mouseY)) {
        g_expanded = !g_expanded;
        std::cout << "[CScript界面] 子控件已" << (g_expanded ? "展开" : "折叠") << "。" << std::endl;
        InvalidateRect(owner, nullptr, FALSE);
        return true;
    }
    if (!g_expanded) return false;
    if (Hit(g_mountRect, mouseX, mouseY)) {
        const std::filesystem::path path = PickScript(owner);
        if (!path.empty()) {
            std::wstring error;
            const bool valid = cscript::AddMountedScript(path, &error);
            const auto& scripts = cscript::MountedScripts();
            if (!scripts.empty()) g_currentPage = (scripts.size() - 1) / kScriptsPerPage;
            if (!valid && !error.empty()) MessageBoxW(owner, error.c_str(), i18n::T("CSCRIPT_PARSE_FAILED"), MB_OK | MB_ICONERROR);
        }
        InvalidateRect(owner, nullptr, FALSE);
        return true;
    }
    if (Hit(g_openDirectoryRect, mouseX, mouseY)) {
        std::filesystem::create_directories(cscript::GetDefaultScriptDir());
        ShellExecuteW(owner, L"open", cscript::GetDefaultScriptDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        return true;
    }
    if (Hit(g_previousPageRect, mouseX, mouseY)) {
        if (g_currentPage > 0) --g_currentPage;
        InvalidateRect(owner, nullptr, FALSE);
        return true;
    }
    if (Hit(g_nextPageRect, mouseX, mouseY)) {
        const std::size_t count = cscript::MountedScripts().size();
        const std::size_t pages = (std::max<std::size_t>)(1, (count + kScriptsPerPage - 1) / kScriptsPerPage);
        if (g_currentPage + 1 < pages) ++g_currentPage;
        InvalidateRect(owner, nullptr, FALSE);
        return true;
    }
    for (std::size_t visible = 0; visible < g_visibleIndices.size(); ++visible) {
        const std::size_t index = g_visibleIndices[visible];
        if (Hit(g_bindRects[visible], mouseX, mouseY)) {
            g_bindingIndex = index;
            std::cout << "[CScript界面] 等待录入单按键。" << std::endl;
            InvalidateRect(owner, nullptr, FALSE);
            return true;
        }
        if (Hit(g_reloadRects[visible], mouseX, mouseY)) {
            std::wstring error;
            if (!cscript::ReloadMountedScript(index, &error) && !error.empty())
                MessageBoxW(owner, error.c_str(), i18n::T("CSCRIPT_PARSE_FAILED"), MB_OK | MB_ICONERROR);
            InvalidateRect(owner, nullptr, FALSE);
            return true;
        }
        if (Hit(g_removeRects[visible], mouseX, mouseY)) {
            cscript::RemoveMountedScript(index);
            g_bindingIndex.reset();
            InvalidateRect(owner, nullptr, FALSE);
            return true;
        }
    }
    return false;
}

bool ProcessBindingKey(HWND owner, WPARAM wParam, LPARAM lParam)
{
    if (!g_bindingIndex.has_value()) return false;
    UINT virtualKey = 0;
    bool extendedKey = false;
    if (!cscript::NormalizeWindowKey(wParam, lParam, virtualKey, extendedKey)) {
        MessageBoxW(owner, i18n::T("CSCRIPT_UNSUPPORTED_KEY"), i18n::T("CSCRIPT_BIND_FAILED"), MB_OK | MB_ICONWARNING);
        return true;
    }
    std::wstring error;
    if (!cscript::SetScriptKey(*g_bindingIndex, virtualKey, extendedKey, &error)) {
        MessageBoxW(owner, error.c_str(), i18n::T("CSCRIPT_BIND_FAILED"), MB_OK | MB_ICONWARNING);
        return true;
    }
    g_bindingIndex.reset();
    InvalidateRect(owner, nullptr, FALSE);
    return true;
}

bool IsExpanded() { return g_expanded; }
void SetExpanded(bool expanded) { g_expanded = expanded; }

} // namespace cscriptui
