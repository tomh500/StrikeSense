#include "cscript_panel.h"

#include "cscript.h"
#include "i18n.h"
#include "pages.h"
#include "Resource.h"
#include "textgui_overlay.h"

#include <algorithm>
#include <commdlg.h>
#include <filesystem>
#include <optional>
#include <thread>
#include <vector>

namespace cscriptui {
namespace {

constexpr std::size_t kScriptsPerPage = 3;

enum class BindingTarget {
    None,
    Ticker,
    ScriptKey,
    ScriptTriggerKey
};

struct InputDialogContext {
    const wchar_t* label = nullptr;
    std::wstring initial;
    std::wstring result;
    bool accepted = false;
};

Gdiplus::RectF g_toggleRect, g_expandRect, g_mountRect, g_openDirectoryRect;
Gdiplus::RectF g_tickerBindRect, g_performanceToggleRect;
Gdiplus::RectF g_previousPageRect, g_nextPageRect;
std::vector<Gdiplus::RectF> g_bindRects, g_triggerBindRects, g_triggerFileRects, g_reloadRects, g_removeRects;
std::vector<std::size_t> g_visibleIndices;
std::optional<std::size_t> g_bindingIndex;
BindingTarget g_bindingTarget = BindingTarget::None;
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
    GraphicsPath path;
    constexpr REAL diameter = 16.f;
    path.AddArc(rect.X, rect.Y, diameter, diameter, 180.f, 90.f);
    path.AddArc(rect.X + rect.Width - diameter, rect.Y, diameter, diameter, 270.f, 90.f);
    path.AddArc(rect.X + rect.Width - diameter, rect.Y + rect.Height - diameter, diameter, diameter, 0.f, 90.f);
    path.AddArc(rect.X, rect.Y + rect.Height - diameter, diameter, diameter, 90.f, 90.f);
    path.CloseFigure();
    graphics.FillPath(&background, &path);
    graphics.DrawPath(&border, &path);
    graphics.DrawString(label, -1, &font, rect, &format, &foreground);
}

void OpenDirectoryAsync(std::wstring directory)
{
    std::thread([directory = std::move(directory)] {
        const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        ShellExecuteW(nullptr, L"open", directory.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        if (SUCCEEDED(initialized)) CoUninitialize();
    }).detach();
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
    g_tickerBindRect = {};
    g_performanceToggleRect = {};
    g_previousPageRect = {};
    g_nextPageRect = {};
    g_bindRects.clear();
    g_triggerBindRects.clear();
    g_triggerFileRects.clear();
    g_reloadRects.clear();
    g_removeRects.clear();
    g_visibleIndices.clear();
}

INT_PTR CALLBACK InputDialogProc(HWND dialog, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* context = reinterpret_cast<InputDialogContext*>(GetWindowLongPtrW(dialog, DWLP_USER));
    if (message == WM_INITDIALOG) {
        context = reinterpret_cast<InputDialogContext*>(lParam);
        SetWindowLongPtrW(dialog, DWLP_USER, reinterpret_cast<LONG_PTR>(context));
        SetDlgItemTextW(dialog, IDC_EVOLUTION_INPUT_LABEL, context->label);
        SetDlgItemTextW(dialog, IDC_EVOLUTION_INPUT_EDIT, context->initial.c_str());
        SendDlgItemMessageW(dialog, IDC_EVOLUTION_INPUT_EDIT, EM_SETSEL, 0, -1);
        SetFocus(GetDlgItem(dialog, IDC_EVOLUTION_INPUT_EDIT));
        return FALSE;
    }
    if (message != WM_COMMAND || !context) return FALSE;
    if (LOWORD(wParam) == IDOK) {
        wchar_t text[260]{};
        GetDlgItemTextW(dialog, IDC_EVOLUTION_INPUT_EDIT, text, static_cast<int>(std::size(text)));
        context->result = text;
        context->accepted = true;
        EndDialog(dialog, IDOK);
        return TRUE;
    }
    if (LOWORD(wParam) == IDCANCEL) {
        EndDialog(dialog, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}

bool PromptTextValue(HWND owner, const wchar_t* label, const std::wstring& initial, std::wstring& result)
{
    InputDialogContext context{ label, initial, initial, false };
    DialogBoxParamW(hInst, MAKEINTRESOURCEW(IDD_EVOLUTION_INPUT), owner,
        InputDialogProc, reinterpret_cast<LPARAM>(&context));
    if (!context.accepted) return false;
    result = context.result;
    return true;
}

} // namespace

int PaintSection(Gdiplus::Graphics& graphics, int contentX, int contentWidth, int topY, HWND)
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
    if (cscript::IsEnabled()) {
        g_expandRect = RectF(static_cast<REAL>(contentX + 286), static_cast<REAL>(topY - 4), 24.f, 24.f);
        SolidBrush foldBackground(Color(255, 231, 244, 252));
        Pen foldBorder(Color(255, 150, 200, 230));
        graphics.FillRectangle(&foldBackground, g_expandRect);
        graphics.DrawRectangle(&foldBorder, g_expandRect);
        graphics.DrawString(g_expanded ? L"v" : L">", -1, &smallFont,
            PointF(g_expandRect.X + 8.f, g_expandRect.Y + 4.f), &text);
    } else {
        g_expandRect = {};
        g_expanded = false;
    }

    ResetDetails();
    if (!cscript::IsEnabled() || !g_expanded) return topY + 34;

    const int detailTop = topY + 34;
    graphics.DrawString(L"全局触发键", -1, &textFont,
        PointF(static_cast<REAL>(contentX + 14), static_cast<REAL>(detailTop + 5)), &text);
    g_tickerBindRect = RectF(static_cast<REAL>(contentX + 118), static_cast<REAL>(detailTop), 90.f, 26.f);
    const std::wstring tickerText = g_bindingTarget == BindingTarget::Ticker
        ? i18n::T("CSCRIPT_PRESS_KEY") : cscript::GetTickerSourceKey();
    DrawButton(graphics, g_tickerBindRect, Compact(tickerText, 12).c_str());

    graphics.DrawString(L"性能优化", -1, &textFont,
        PointF(static_cast<REAL>(contentX + 222), static_cast<REAL>(detailTop + 5)), &text);
    g_performanceToggleRect = RectF(static_cast<REAL>(contentX + 286), static_cast<REAL>(detailTop - 4), 50.f, 24.f);
    ui::DrawToggle(graphics, contentX + 286, detailTop - 4, cscript::IsPerformanceMode());

    const std::wstring runtimeStatus = Compact(cscript::GetLastRuntimeStatus(), 38);
    graphics.DrawString(runtimeStatus.c_str(), -1, &smallFont,
        PointF(static_cast<REAL>(contentX + 350), static_cast<REAL>(detailTop + 5)), &dim);

    const int toolbarY = detailTop + 34;
    g_mountRect = RectF(static_cast<REAL>(contentX + 10), static_cast<REAL>(toolbarY), 112.f, 26.f);
    g_openDirectoryRect = RectF(static_cast<REAL>(contentX + 132), static_cast<REAL>(toolbarY), 112.f, 26.f);
    DrawButton(graphics, g_mountRect, i18n::T("CSCRIPT_MOUNT"));
    DrawButton(graphics, g_openDirectoryRect, i18n::T("CSCRIPT_OPEN_DIR"));

    const auto& scripts = cscript::MountedScripts();
    const std::size_t pageCount = (std::max<std::size_t>)(1, (scripts.size() + kScriptsPerPage - 1) / kScriptsPerPage);
    if (g_currentPage >= pageCount) g_currentPage = pageCount - 1;
    const std::size_t first = g_currentPage * kScriptsPerPage;
    const std::size_t last = (std::min)(scripts.size(), first + kScriptsPerPage);
    int rowY = toolbarY + 36;

    if (scripts.empty()) {
        graphics.DrawString(i18n::T("CSCRIPT_EMPTY"), -1, &textFont,
            PointF(static_cast<REAL>(contentX + 12), static_cast<REAL>(rowY + 8)), &dim);
    }
    for (std::size_t index = first; index < last; ++index) {
        const auto& script = scripts[index];
        RectF row(static_cast<REAL>(contentX + 8), static_cast<REAL>(rowY),
            static_cast<REAL>(contentWidth - 16), 84.f);
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

        graphics.DrawString(L"监听键", -1, &smallFont, PointF(row.X + 8.f, row.Y + 50.f), &dim);
        const std::wstring triggerSummary = script.trigger_file_name.empty()
            ? L"默认全局 StrikeTicker.cfg"
            : (L"触发文件: CustomTicker/" + script.trigger_file_name + L".cfg");
        graphics.DrawString(Compact(triggerSummary, 36).c_str(), -1, &smallFont,
            PointF(row.X + 76.f, row.Y + 50.f), &dim);

        RectF bind(row.X + row.Width - 396.f, row.Y + 10.f, 76.f, 28.f);
        RectF triggerBind(row.X + row.Width - 312.f, row.Y + 10.f, 76.f, 28.f);
        RectF triggerFile(row.X + row.Width - 228.f, row.Y + 10.f, 76.f, 28.f);
        RectF reload(row.X + row.Width - 144.f, row.Y + 10.f, 60.f, 28.f);
        RectF remove(row.X + row.Width - 76.f, row.Y + 10.f, 60.f, 28.f);
        g_bindRects.push_back(bind);
        g_triggerBindRects.push_back(triggerBind);
        g_triggerFileRects.push_back(triggerFile);
        g_reloadRects.push_back(reload);
        g_removeRects.push_back(remove);
        g_visibleIndices.push_back(index);

        std::wstring bindText;
        if (g_bindingTarget == BindingTarget::ScriptKey && g_bindingIndex.has_value() && *g_bindingIndex == index)
            bindText = i18n::T("CSCRIPT_PRESS_KEY");
        else if (!script.source_key.empty()) bindText = script.source_key;
        else bindText = L"未绑定";

        std::wstring triggerBindText;
        if (g_bindingTarget == BindingTarget::ScriptTriggerKey && g_bindingIndex.has_value() && *g_bindingIndex == index)
            triggerBindText = i18n::T("CSCRIPT_PRESS_KEY");
        else if (!script.trigger_source_key.empty()) triggerBindText = script.trigger_source_key;
        else triggerBindText = L"自定义触发键";

        const std::wstring triggerFileText = script.trigger_file_name.empty()
            ? L"自定义触发名" : script.trigger_file_name;

        DrawButton(graphics, bind, Compact(bindText, 10).c_str());
        DrawButton(graphics, triggerBind, Compact(triggerBindText, 10).c_str());
        DrawButton(graphics, triggerFile, Compact(triggerFileText, 10).c_str());
        DrawButton(graphics, reload, i18n::T("CSCRIPT_RELOAD"));
        DrawButton(graphics, remove, i18n::T("CSCRIPT_REMOVE"));
        rowY += 90;
    }

    const int footerY = rowY + 8;
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
    return footerY + 34;
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
            }
        } else {
            cscript::SetEnabled(false);
            g_expanded = false;
        }
        RefreshTextguiOverlay();
        InvalidateRect(owner, nullptr, FALSE);
        return true;
    }
    if (Hit(g_expandRect, mouseX, mouseY)) {
        g_expanded = !g_expanded;
        InvalidateRect(owner, nullptr, FALSE);
        return true;
    }
    if (!g_expanded) return false;
    if (Hit(g_performanceToggleRect, mouseX, mouseY)) {
        cscript::SetPerformanceMode(!cscript::IsPerformanceMode());
        InvalidateRect(owner, nullptr, FALSE);
        return true;
    }
    if (Hit(g_tickerBindRect, mouseX, mouseY)) {
        g_bindingTarget = BindingTarget::Ticker;
        g_bindingIndex.reset();
        InvalidateRect(owner, nullptr, FALSE);
        return true;
    }
    if (Hit(g_mountRect, mouseX, mouseY)) {
        const std::filesystem::path path = PickScript(owner);
        if (!path.empty()) {
            std::wstring error;
            const bool valid = cscript::AddMountedScript(path, &error);
            const auto& scripts = cscript::MountedScripts();
            if (!scripts.empty()) g_currentPage = (scripts.size() - 1) / kScriptsPerPage;
            if (!valid && !error.empty())
                MessageBoxW(owner, error.c_str(), i18n::T("CSCRIPT_PARSE_FAILED"), MB_OK | MB_ICONERROR);
        }
        InvalidateRect(owner, nullptr, FALSE);
        return true;
    }
    if (Hit(g_openDirectoryRect, mouseX, mouseY)) {
        std::filesystem::create_directories(cscript::GetDefaultScriptDir());
        OpenDirectoryAsync(cscript::GetDefaultScriptDir());
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
            g_bindingTarget = BindingTarget::ScriptKey;
            InvalidateRect(owner, nullptr, FALSE);
            return true;
        }
        if (Hit(g_triggerBindRects[visible], mouseX, mouseY)) {
            g_bindingIndex = index;
            g_bindingTarget = BindingTarget::ScriptTriggerKey;
            InvalidateRect(owner, nullptr, FALSE);
            return true;
        }
        if (Hit(g_triggerFileRects[visible], mouseX, mouseY)) {
            const auto& scripts = cscript::MountedScripts();
            if (index < scripts.size()) {
                std::wstring next;
                if (PromptTextValue(owner, L"输入触发文件名，留空恢复全局设置", scripts[index].trigger_file_name, next)) {
                    std::wstring error;
                    if (!cscript::SetScriptTriggerFileName(index, next, &error) && !error.empty())
                        MessageBoxW(owner, error.c_str(), i18n::T("CSCRIPT_BIND_FAILED"), MB_OK | MB_ICONWARNING);
                }
            }
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
            g_bindingTarget = BindingTarget::None;
            InvalidateRect(owner, nullptr, FALSE);
            return true;
        }
    }
    return false;
}

bool ProcessBindingKey(HWND owner, WPARAM wParam, LPARAM lParam)
{
    if (g_bindingTarget == BindingTarget::None) return false;
    if (g_bindingTarget != BindingTarget::Ticker && !g_bindingIndex.has_value()) return false;

    if (g_bindingTarget == BindingTarget::ScriptKey && static_cast<UINT>(wParam) == VK_ESCAPE) {
        std::wstring error;
        if (!cscript::ClearScriptKey(*g_bindingIndex, &error) && !error.empty())
            MessageBoxW(owner, error.c_str(), i18n::T("CSCRIPT_BIND_FAILED"), MB_OK | MB_ICONWARNING);
        g_bindingTarget = BindingTarget::None;
        g_bindingIndex.reset();
        InvalidateRect(owner, nullptr, FALSE);
        return true;
    }
    if (g_bindingTarget == BindingTarget::ScriptTriggerKey && static_cast<UINT>(wParam) == VK_ESCAPE) {
        std::wstring error;
        if (!cscript::ClearScriptTriggerKey(*g_bindingIndex, &error) && !error.empty())
            MessageBoxW(owner, error.c_str(), i18n::T("CSCRIPT_BIND_FAILED"), MB_OK | MB_ICONWARNING);
        g_bindingTarget = BindingTarget::None;
        g_bindingIndex.reset();
        InvalidateRect(owner, nullptr, FALSE);
        return true;
    }

    UINT virtualKey = 0;
    bool extendedKey = false;
    if (!cscript::NormalizeWindowKey(wParam, lParam, virtualKey, extendedKey)) {
        MessageBoxW(owner, i18n::T("CSCRIPT_UNSUPPORTED_KEY"), i18n::T("CSCRIPT_BIND_FAILED"), MB_OK | MB_ICONWARNING);
        return true;
    }

    std::wstring error;
    bool bound = false;
    switch (g_bindingTarget) {
    case BindingTarget::Ticker:
        bound = cscript::SetTickerKey(virtualKey, extendedKey, &error);
        break;
    case BindingTarget::ScriptKey:
        bound = cscript::SetScriptKey(*g_bindingIndex, virtualKey, extendedKey, &error);
        break;
    case BindingTarget::ScriptTriggerKey:
        bound = cscript::SetScriptTriggerKey(*g_bindingIndex, virtualKey, extendedKey, &error);
        break;
    case BindingTarget::None:
        break;
    }
    if (!bound) {
        MessageBoxW(owner, error.c_str(), i18n::T("CSCRIPT_BIND_FAILED"), MB_OK | MB_ICONWARNING);
        return true;
    }

    g_bindingTarget = BindingTarget::None;
    g_bindingIndex.reset();
    InvalidateRect(owner, nullptr, FALSE);
    return true;
}

bool IsExpanded() { return g_expanded; }
void SetExpanded(bool expanded) { g_expanded = cscript::IsEnabled() && expanded; }

} // namespace cscriptui
