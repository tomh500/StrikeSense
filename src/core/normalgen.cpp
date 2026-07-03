#include "normalgen.h"
#include <windows.h>
#include <tlhelp32.h>
#include <string>
namespace normalgen {

void Init() {}
void Shutdown() {}
// 辅助函数：检查当前程序是否拥有管理员权限
bool CheckAdminPermission() {
    bool isElevated = false;
    HANDLE hToken = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION te;
        DWORD size = sizeof(te);
        if (GetTokenInformation(hToken, TokenElevation, &te, size, &size)) {
            isElevated = te.TokenIsElevated;
        }
        CloseHandle(hToken);
    }
    return isElevated;
}



// 使用 Windows Toolhelp32 API 获取进程状态
bool IsCS2Running() {
    bool exists = false;
    // 拍摄所有进程的快照
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(PROCESSENTRY32W);

    // 遍历快照中的进程
    if (Process32FirstW(hSnapshot, &pe)) {
        do {
            // 检查进程名是否为 cs2.exe
            if (std::wstring(pe.szExeFile) == L"cs2.exe") {
                exists = true;
                break;
            }
        } while (Process32NextW(hSnapshot, &pe));
    }

    CloseHandle(hSnapshot);
    return exists;
}

} // namespace normalgen