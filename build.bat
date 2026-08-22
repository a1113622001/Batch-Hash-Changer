@echo off
setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
set "BIN_DIR=%SCRIPT_DIR%bin"
set "BUILD_DIR=%SCRIPT_DIR%build"

if not exist "%BIN_DIR%" mkdir "%BIN_DIR%"
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

rem Add companion w64devkit to PATH if present
if exist "%SCRIPT_DIR%..\w64devkit\bin\gcc.exe" (
    set "PATH=%SCRIPT_DIR%..\w64devkit\bin;%PATH%"
)

where gcc >nul 2>nul
if %errorlevel% neq 0 (
    echo [ERROR] GCC compiler not found!
    pause
    exit /b 1
)

echo [*] Compiling resource version.rc...
pushd "%SCRIPT_DIR%src"
windres version.rc -O coff -o "%BUILD_DIR%\version.res"
if %errorlevel% neq 0 (
    set "RES_OBJ="
) else (
    set "RES_OBJ=%BUILD_DIR%\version.res"
)
popd

echo [*] Compiling BatchHashChanger.exe...
gcc -O3 -s -Wall -Wextra "%SCRIPT_DIR%src\main.c" "%SCRIPT_DIR%src\hash_changer.c" %RES_OBJ% -ladvapi32 -lshell32 -o "%BIN_DIR%\BatchHashChanger.exe"
if %errorlevel% neq 0 (
    echo [ERROR] Build failed!
    pause
    exit /b 1
)

copy /y "%BIN_DIR%\BatchHashChanger.exe" "%BIN_DIR%\?????????????????exe" >nul

echo [*] Compiling test suite test_suite.exe...
gcc -O2 -Wall -Wextra "%SCRIPT_DIR%src\hash_changer.c" "%SCRIPT_DIR%tests\test_suite.c" -ladvapi32 -lshell32 -o "%BIN_DIR%\test_suite.exe"

echo =======================================================
echo   Build Successful!
echo   Output: %BIN_DIR%\BatchHashChanger.exe
echo   Output: %BIN_DIR%\?????????????????exe
echo =======================================================
