@echo off
setlocal DisableDelayedExpansion
if not exist "%~dp0runtime\python.exe" (
  echo [FAIL] Incomplete CppAppLab package. Extract the entire ZIP.
  if not defined CPPAPPLAB_NO_PAUSE pause
  exit /b 2
)
"%~dp0runtime\python.exe" "%~dp0start-native.py" %*
set "RESULT=%ERRORLEVEL%"
if not "%RESULT%"=="0" if not defined CPPAPPLAB_NO_PAUSE pause
exit /b %RESULT%
