@echo off
setlocal
rem Batch File Hash Modifier v11.1 - launcher
rem Locates the companion .ps1 script in this folder and runs it.

set "PS1="
for %%F in ("%~dp0*.ps1") do set "PS1=%%~fF"
if not defined PS1 (
  echo [ERROR] No .ps1 script found next to this launcher.
  pause
  exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%PS1%" %*
pause