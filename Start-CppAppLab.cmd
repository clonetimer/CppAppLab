@echo off
setlocal
powershell.exe -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\Start-CppAppLab.ps1" %*
set "exitCode=%ERRORLEVEL%"
if not "%exitCode%"=="0" (
    echo.
    echo CppAppLab launcher exited with code %exitCode%.
    pause
)
exit /b %exitCode%
