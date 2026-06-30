# ==================== 1. 配置基础路径 ====================
$srcDir = "C:\Users\jingy\source\repos\StrikeSense"
$themidaExe = "C:\Program Files\Themida v3.1.8.0\Themida64.exe"
$themidaProject = "$srcDir\release.tmd" # 确保你的 Themida 项目里设置的输入输出路径正确

$webDir = "D:\Users\user0\Downloads\Misc\strikesense.web"
$releaseTarget = "$webDir\app\StrikeSense.exe"
$nightlyDir = "$webDir\nightly"

# 获取当前时间戳 (格式: 202606240052)
$timestamp = Get-Date -Format "yyyyMMddHHmm"

# 询问用户是发布正式版还是 Nightly
$choice = Read-Host "请输入构建类型 [1] Nightly 版  [2] 正式 Release 版"

# ==================== 2. 自动编译 Release EXE ====================
Write-Host "开始使用 CMake 编译 Release 构建产物..." -ForegroundColor Cyan
cd $srcDir
# 注意：这里你可以加入自动修改 rc 文件版本号的逻辑
cmake --build bin --config Release

$builtExe = "$srcDir\bin\Release\StrikeSense.exe"
if (-not (Test-Path $builtExe)) {
    Write-Error "编译失败，未找到构建产物！"
    exit
}

# ==================== 3. 自动调用 Themida 加壳 ====================
Write-Host "正在调用 Themida 进行加壳保护..." -ForegroundColor Cyan
# Themida 支持通过 /b 参数进行后台静默命令行构建
Start-Process -FilePath $themidaExe -ArgumentList "/b", "$themidaProject" -Wait

# ==================== 4. 分流分发与自动清理 ====================
if ($choice -eq "1") {
    # ------ Nightly 分支 ------
    if (-not (Test-Path $nightlyDir)) { New-Item -ItemType Directory -Path $nightlyDir }
    
    $nightlyName = "StrikeSense-nightly-$timestamp.exe"
    $nightlyTarget = "$nightlyDir\$nightlyName"
    
    Copy-Item -Path $builtExe -Destination $nightlyTarget -Force
    Write-Host "已成功生成 Nightly 包: $nightlyName" -ForegroundColor Green
    
    # 核心魔法：自动只保留最新的 3 个构建，删除旧的
    Write-Host "正在清理旧的 Nightly 构建，只保留最新的 3 个..." -ForegroundColor Yellow
    Get-ChildItem -Path $nightlyDir -Filter "StrikeSense-nightly-*.exe" |
        Sort-Object LastWriteTime -Descending |
        Select-Object -Skip 3 |
        Remove-Item -Force
} 
else {
    # ------ 正式 Release 分支 ------
    Copy-Item -Path $builtExe -Destination $releaseTarget -Force
    Write-Host "已成功覆盖正式版 Release: $releaseTarget" -ForegroundColor Green
}

# ==================== 5. 自动准备 Git 提交 ====================
Write-Host "正在进入网站项目 Git 目录..." -ForegroundColor Cyan
cd $webDir
# 自动帮你把构建产物加进 Git 暂存区
git add app/StrikeSense.exe
git add nightly/*

Write-Host "全部完成！请进入 $webDir 确认无误后执行 git commit 和 push。" -ForegroundColor Green