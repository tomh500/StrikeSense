#include "console.h"
#include "resource.h"
#include <iostream>
#include <cstring>

Console* Console::s_pThis = nullptr;

// ============================================================
// CoutStreambuf 实现 — UTF-8 字节 → wstring
// ============================================================
void CoutStreambuf::AppendUtf8(const char* data, size_t len)
{
    if (!m_buffer || !m_mutex)
        return;

    // UTF-8 → UTF-16
    int wlen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                   data, (int)len, nullptr, 0);
    UINT codePage = CP_UTF8;
    if (wlen <= 0)
    {
        wlen = MultiByteToWideChar(CP_ACP, 0, data, (int)len, nullptr, 0);
        codePage = CP_ACP;
    }

    if (wlen <= 0)
        return;

    std::wstring wtext;
    wtext.resize(wlen);
    MultiByteToWideChar(codePage, 0, data, (int)len, &wtext[0], wlen);

    // 修复换行: Windows 编辑框需要 \r\n，而 std::endl 只输出 \n
    // 将所有独立的 \n（前面没有 \r 的）替换为 \r\n
    {
        std::wstring fixed;
        fixed.reserve(wtext.length());
        for (size_t i = 0; i < wtext.length(); ++i)
        {
            if (wtext[i] == L'\n')
            {
                if (i == 0 || wtext[i - 1] != L'\r')
                    fixed += L'\r';
            }
            fixed += wtext[i];
        }
        if (fixed.length() > 0)
            wtext.swap(fixed);
    }

    {
        std::lock_guard<std::mutex> lock(*m_mutex);
        m_buffer->push_back(std::move(wtext));
    }

    // 如果 Debugger 窗口已打开，实时推送
    if (m_hDlg && *m_hDlg && IsWindow(*m_hDlg))
    {
        std::wstring* pw = new std::wstring(m_buffer->back());
        ::PostMessageW(*m_hDlg, Console::WM_APPEND_TEXT, 0, (LPARAM)pw);
    }
}

// ============================================================
// WcoutStreambuf 实现
// ============================================================
void WcoutStreambuf::Append(const std::wstring& text)
{
    if (!m_buffer || !m_mutex)
        return;

    {
        std::lock_guard<std::mutex> lock(*m_mutex);
        m_buffer->push_back(text);
    }

    // 如果 Debugger 窗口已打开，实时推送
    if (m_hDlg && *m_hDlg && IsWindow(*m_hDlg))
    {
        std::wstring* pw = new std::wstring(text);
        ::PostMessageW(*m_hDlg, Console::WM_APPEND_TEXT, 0, (LPARAM)pw);
    }
}

// ============================================================
// Console
// ============================================================

Console::Console()
    : m_hDlg(nullptr)
    , m_hParentWnd(nullptr)
    , m_hInst(nullptr)
    , m_oldCoutBuf(nullptr)
    , m_oldCerrBuf(nullptr)
    , m_oldWcoutBuf(nullptr)
    , m_redirected(false)
{
}

Console::~Console()
{
    HideDebugger();

    if (m_redirected)
    {
        std::cout.rdbuf(m_oldCoutBuf);
        std::cerr.rdbuf(m_oldCerrBuf);
        std::wcout.rdbuf(m_oldWcoutBuf);
        m_redirected = false;
    }
}

// ----------------------------------------------------------
// InitRedirection: 在 wWinMain 开头调用
//
// 替换 cout/cerr/wcout 的 streambuf 为自定义 streambuf，
// 所有输出直接写入内存缓冲 m_outputLog
// ----------------------------------------------------------
bool Console::InitRedirection()
{
    if (m_redirected)
        return true;

    // 设置三个自定义 streambuf 的目标
    m_coutBuf.SetTarget(&m_outputLog, &m_outputMutex, &m_hDlg);
    m_cerrBuf.SetTarget(&m_outputLog, &m_outputMutex, &m_hDlg);
    m_wcoutBuf.SetTarget(&m_outputLog, &m_outputMutex, &m_hDlg);

    // 替换 streambuf
    m_oldCoutBuf = std::cout.rdbuf(&m_coutBuf);
    m_oldCerrBuf = std::cerr.rdbuf(&m_cerrBuf);
    m_oldWcoutBuf = std::wcout.rdbuf(&m_wcoutBuf);

    m_redirected = true;

    OutputDebugStringA("[Console::InitRedirection] SUCCESS — streambufs replaced\n");
    return true;
}

// ----------------------------------------------------------
bool Console::ShowDebugger(HINSTANCE hInstance, HWND hParentWnd)
{
    if (m_hDlg && IsWindow(m_hDlg))
    {
        ShowWindow(m_hDlg, SW_SHOW);
        SetForegroundWindow(m_hDlg);
        return true;
    }

    m_hInst = hInstance;
    m_hParentWnd = hParentWnd;
    s_pThis = this;

    HWND hDlg = CreateDialogW(hInstance, MAKEINTRESOURCEW(IDD_DEBUGGER),
                              hParentWnd, DebuggerDlgProc);
    if (!hDlg)
    {
        OutputDebugStringA("[Console::ShowDebugger] CreateDialogW FAILED\n");
        return false;
    }

    m_hDlg = hDlg;

    // 刷入已累积的输出
    HWND hEdit = GetDlgItem(hDlg, IDC_DEBUG_EDIT);
    if (hEdit)
        FlushPendingOutput(hEdit);

    ShowWindow(hDlg, SW_SHOW);

    OutputDebugStringA("[Console::ShowDebugger] dialog created and pending output flushed\n");
    return true;
}

// ----------------------------------------------------------
void Console::HideDebugger()
{
    if (m_hDlg && IsWindow(m_hDlg))
        DestroyWindow(m_hDlg);
    m_hDlg = nullptr;
}

// ----------------------------------------------------------
void Console::FlushPendingOutput(HWND hEdit, bool clearAfter)
{
    if (!hEdit)
        return;

    std::lock_guard<std::mutex> lock(m_outputMutex);

    for (auto& line : m_outputLog)
    {
        int len = GetWindowTextLengthW(hEdit);
        SendMessageW(hEdit, EM_SETSEL, len, len);
        SendMessageW(hEdit, EM_REPLACESEL, FALSE, (LPARAM)line.c_str());
    }

    if (clearAfter)
        m_outputLog.clear();
}

// ----------------------------------------------------------
INT_PTR CALLBACK Console::DebuggerDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_INITDIALOG:
    {
        RECT rc;
        GetWindowRect(GetParent(hDlg), &rc);
        int x = rc.left + (rc.right - rc.left) / 2 - 340;
        int y = rc.top + (rc.bottom - rc.top) / 2 - 200;
        SetWindowPos(hDlg, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        return TRUE;
    }

    case WM_APPEND_TEXT:
    {
        std::wstring* pwstr = reinterpret_cast<std::wstring*>(lParam);
        if (pwstr)
        {
            HWND hEdit = GetDlgItem(hDlg, IDC_DEBUG_EDIT);
            if (hEdit)
            {
                int len = GetWindowTextLengthW(hEdit);
                SendMessageW(hEdit, EM_SETSEL, len, len);
                SendMessageW(hEdit, EM_REPLACESEL, FALSE, (LPARAM)pwstr->c_str());
                SendMessageW(hEdit, EM_SCROLLCARET, 0, 0);
            }
            delete pwstr;
        }
        return TRUE;
    }

    case WM_COMMAND:
    {
        WORD id = LOWORD(wParam);
        switch (id)
        {
        case IDC_COPYALL:
        {
            HWND hEdit = GetDlgItem(hDlg, IDC_DEBUG_EDIT);
            if (hEdit)
            {
                int len = GetWindowTextLengthW(hEdit);
                if (len > 0)
                {
                    std::wstring text(len + 1, L'\0');
                    GetWindowTextW(hEdit, &text[0], len + 1);
                    text.resize(len);

                    // 复制到剪贴板
                    if (OpenClipboard(hDlg))
                    {
                        EmptyClipboard();
                        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (len + 1) * sizeof(wchar_t));
                        if (hMem)
                        {
                            memcpy(GlobalLock(hMem), text.c_str(), (len + 1) * sizeof(wchar_t));
                            GlobalUnlock(hMem);
                            SetClipboardData(CF_UNICODETEXT, hMem);
                        }
                        CloseClipboard();
                    }
                }
            }
            return TRUE;
        }

        case IDC_CLEAR:
        {
            if (s_pThis)
            {
                {
                    std::lock_guard<std::mutex> lock(s_pThis->m_outputMutex);
                    s_pThis->m_outputLog.clear();
                }
                HWND hEdit = GetDlgItem(hDlg, IDC_DEBUG_EDIT);
                if (hEdit)
                    SetWindowTextW(hEdit, L"");
            }
            return TRUE;
        }
        case IDOK:
        case IDCANCEL:
        {
            if (s_pThis)
                s_pThis->m_hDlg = nullptr;
            DestroyWindow(hDlg);
            return TRUE;
        }
        }
        break;
    }

    case WM_DESTROY:
    {
        if (s_pThis)
            s_pThis->m_hDlg = nullptr;
        return TRUE;
    }
    }

    return FALSE;
}