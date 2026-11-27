@echo off
setlocal DisableDelayedExpansion
"%~dp0runtime\python.exe" "%~dp0start-native.py" --cli %*
exit /b %ERRORLEVEL%
