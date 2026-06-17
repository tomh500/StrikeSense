#include "normalgen.h"
#include <windows.h>

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

} // namespace normalgen