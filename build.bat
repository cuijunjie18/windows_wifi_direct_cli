@echo off
REM ===========================================================
REM  Wi-Fi Direct CLI Tool - Build Script (MSVC cl.exe)
REM ===========================================================
REM
REM  Prerequisites:
REM    1. Visual Studio 2019/2022 with "Desktop development with C++"
REM    2. Windows SDK 10.0.17763.0 or later (with C++/WinRT headers)
REM    3. Run from "x64 Native Tools Command Prompt for VS"
REM
REM  Usage:
REM    build.bat
REM ===========================================================

setlocal enabledelayedexpansion

echo === Wi-Fi Direct CLI Tool Build Script ===
echo.

REM Check if cl.exe is available
where cl.exe >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: cl.exe not found.
    echo Please run this script from "Developer Command Prompt for VS 2019/2022"
    echo or "x64 Native Tools Command Prompt for VS 2019/2022".
    echo.
    echo To open it:
    echo   Start Menu ^> Visual Studio 2022 ^> x64 Native Tools Command Prompt
    exit /b 1
)

REM Detect Windows SDK path
set SDK_VERSION=
for /f "tokens=*" %%i in ('dir /b /ad "E:\My_environment\windows_sdk\Include\" 2^>nul ^| findstr "10\." ^| sort /r') do (
    if not defined SDK_VERSION set SDK_VERSION=%%i
)

if not defined SDK_VERSION (
    echo ERROR: Windows SDK not found.
    echo Please install Windows 10/11 SDK via Visual Studio Installer.
    exit /b 1
)

echo Detected Windows SDK version: %SDK_VERSION%

set SDK_ROOT=E:\My_environment\windows_sdk
set SDK_INC=%SDK_ROOT%\Include\%SDK_VERSION%
set SDK_LIB=%SDK_ROOT%\Lib\%SDK_VERSION%

REM Check for C++/WinRT headers
if not exist "%SDK_INC%\cppwinrt\winrt\Windows.Foundation.h" (
    echo ERROR: C++/WinRT headers not found in SDK.
    echo Please ensure Windows SDK %SDK_VERSION% includes C++/WinRT support.
    echo.
    echo You can install C++/WinRT via:
    echo   1. Visual Studio Installer ^> Modify ^> Individual components ^> C++/WinRT
    echo   2. Or: nuget install Microsoft.Windows.CppWinRT
    exit /b 1
)

echo C++/WinRT headers found.
echo.

REM Compiler and linker flags
set CFLAGS=/nologo /EHsc /std:c++17 /W3 /O2 /DWIN32_LEAN_AND_MEAN /D_UNICODE /DUNICODE
set INCLUDES=/I"%SDK_INC%\cppwinrt" /I"%SDK_INC%\um" /I"%SDK_INC%\ucrt" /I"%SDK_INC%\shared" /I"%SDK_INC%\winrt" /I"."
set LIBS=windowsapp.lib ole32.lib
set LIBPATH=/LIBPATH:"%SDK_LIB%\um\x64" /LIBPATH:"%SDK_LIB%\ucrt\x64"

set OUTPUT=WiFiDirectCLI.exe
set SOURCE=main.cpp

echo Compiling %SOURCE%...
echo.

cl.exe %CFLAGS% %INCLUDES% %SOURCE% /Fe:%OUTPUT% /link %LIBS% %LIBPATH%

if %errorlevel% neq 0 (
    echo.
    echo Build FAILED.
    exit /b 1
)

echo.
echo ========================================
echo Build SUCCEEDED: %OUTPUT%
echo ========================================
echo.
echo Usage:
echo   %OUTPUT% advertise   - Run as Wi-Fi Direct advertiser
echo   %OUTPUT% connect     - Run as Wi-Fi Direct connector
echo   %OUTPUT% help        - Show help
echo.

endlocal
