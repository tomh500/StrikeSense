#pragma once

// ============================================================
// anti-stupid — Steam 用户封禁名单检查
// ============================================================

namespace antistupid {

// 检查当前登录的 Steam 用户是否在封禁名单中
// 返回 true 表示应阻止程序启动
bool CheckAndBlock();

} // namespace antistupid