@echo off
setlocal

set "ROOT=%~dp0"

echo [build] Configuring project...
cmake -S "%ROOT%." -B "%ROOT%."
if errorlevel 1 (
    echo [build] Configure failed.
    exit /b 1
)

echo [build] Building Release (user)...
cmake --build "%ROOT%." --config Release
if errorlevel 1 (
    echo [build] Release failed.
    exit /b 1
)

echo [build] Building Nightly (eng)...
cmake --build "%ROOT%." --config Nightly
if errorlevel 1 (
    echo [build] Nightly failed.
    exit /b 1
)

echo [build] Release and Nightly completed.
endlocal
