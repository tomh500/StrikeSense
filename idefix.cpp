// 这是一个快速修复文件，修复语言切换和持久化
// 修改后合并到对应的源文件

// ===== 修复1: evolution.json 保存 g_langCN =====
// 在 evolution_page.cpp 的 SaveEvolutionParams() 中添加:
// j["langCN"] = g_langCN;

// 在 LoadEvolutionParams() 中添加:
// if (j.contains("langCN") && j["langCN"].is_boolean()) g_langCN = j["langCN"];

// ===== 修复2: 让侧边栏使用 i18n::T() =====
// sidebar.cpp 中 items[] 改为:
// {i18n::T(i18n::Keys::SIDEBAR_FILE), PAGE_SOUNDS, 52},
// {i18n::T(i18n::Keys::SIDEBAR_SETTINGS), PAGE_SETTINGS, 82},
// ...

// ===== 修复3: 合法配置页面使用 i18n::T() =====
// legalcfg_page.cpp 中所有 DrawString 改用 i18n::T("LEGAL_xxx")