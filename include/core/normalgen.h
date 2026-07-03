#pragma once

// ============================================================
// normalgen — 通用生成器模块
// ============================================================

namespace normalgen {

void Init();
void Shutdown();
bool CheckAdminPermission();
bool IsCS2Running();

} // namespace normalgen