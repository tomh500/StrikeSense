#pragma once

#include <windows.h>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <streambuf>

// ============================================================
// Console — Debugger 调试输出窗口
//
// 工作原理（无管道，纯内存缓冲）:
//   1. 替换 cout/cerr/wcout 的 streambuf 为自定义 BufferStreambuf
//   2. 所有 << 操作直接追加到内存中的 vector<wstring>
//   3. 没有管道、没有 fd、没有 _read 阻塞，完全可靠
//   4. Debugger 窗口打开时刷入所有已累积的输出
//   5. 后续输出通过 PostMessage 实时推送到编辑框
// ============================================================

// ----------------------------------------------------------
// 自定义窄字符 streambuf — 直接写入内存缓冲
// ----------------------------------------------------------
class CoutStreambuf : public std::streambuf
{
public:
    CoutStreambuf() : m_buffer(nullptr) {}

    // 由 Console 设置共享的缓冲目标
    void SetTarget(std::vector<std::wstring>* vec, std::mutex* mtx, HWND* hDlg)
    {
        m_buffer = vec;
        m_mutex = mtx;
        m_hDlg = hDlg;
    }

protected:
    int_type overflow(int_type c) override
    {
        if (c != EOF)
        {
            char ch = static_cast<char>(c);
            AppendUtf8(&ch, 1);
        }
        return c;
    }

    int sync() override { return 0; }

    std::streamsize xsputn(const char* s, std::streamsize n) override
    {
        if (n > 0)
        {
            AppendUtf8(s, static_cast<size_t>(n));
        }
        return n;
    }

private:
    void AppendUtf8(const char* data, size_t len);

    std::vector<std::wstring>* m_buffer;
    std::mutex* m_mutex;
    HWND* m_hDlg;
};

// ----------------------------------------------------------
// 自定义宽字符 streambuf — 直接写入内存缓冲
// ----------------------------------------------------------
class WcoutStreambuf : public std::wstreambuf
{
public:
    WcoutStreambuf() : m_buffer(nullptr) {}

    void SetTarget(std::vector<std::wstring>* vec, std::mutex* mtx, HWND* hDlg)
    {
        m_buffer = vec;
        m_mutex = mtx;
        m_hDlg = hDlg;
    }

protected:
    int_type overflow(int_type c) override
    {
        if (c != EOF)
        {
            wchar_t wc = static_cast<wchar_t>(c);
            Append(std::wstring(1, wc));
        }
        return c;
    }

    int sync() override { return 0; }

    std::streamsize xsputn(const wchar_t* s, std::streamsize n) override
    {
        if (n > 0)
        {
            Append(std::wstring(s, static_cast<size_t>(n)));
        }
        return n;
    }

private:
    void Append(const std::wstring& text);

    std::vector<std::wstring>* m_buffer;
    std::mutex* m_mutex;
    HWND* m_hDlg;
};

// ============================================================
// Console 主类
// ============================================================
class Console
{
public:
    Console();
    ~Console();

    // 初始化重定向 — 在 wWinMain 最开始调用（任何 cout 使用之前）
    bool InitRedirection();

    // 创建 / 显示 Debugger 窗口
    bool ShowDebugger(HINSTANCE hInstance, HWND hParentWnd);

    // 销毁 Debugger 窗口
    void HideDebugger();

    // 对话框过程
    static INT_PTR CALLBACK DebuggerDlgProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    // 将已累积的输出刷入编辑框
    void FlushPendingOutput(HWND hEdit, bool clearAfter = false);

private:
    HWND        m_hDlg;
    HWND        m_hParentWnd;
    HINSTANCE   m_hInst;

    // 原始 streambuf
    std::streambuf*   m_oldCoutBuf;
    std::streambuf*   m_oldCerrBuf;
    std::wstreambuf*  m_oldWcoutBuf;

    // 自定义 streambuf（内存成员，不是 new 出来的）
    CoutStreambuf    m_coutBuf;
    CoutStreambuf    m_cerrBuf;
    WcoutStreambuf   m_wcoutBuf;

    // 输出缓冲（所有 cout/cerr/wcout 都写到这里）
    std::vector<std::wstring>   m_outputLog;
    std::mutex                  m_outputMutex;

    bool                    m_redirected;

    static Console*         s_pThis;
public:
    static const UINT       WM_APPEND_TEXT = WM_APP + 100;
private:
};