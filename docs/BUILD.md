# Wi-Fi Direct CLI Tool - Build Guide

## Overview

This is a command-line version of the Windows UWP Wi-Fi Direct sample.
It provides the same core functionality (advertising and connecting Wi-Fi Direct devices)
without requiring a graphical user interface or the Visual Studio IDE project system.

The code has been converted from **C++/CX (UWP)** to **C++/WinRT (standard C++17)**,
which allows building with the MSVC `cl.exe` compiler directly from the command line.

## Project Structure

```
transform/
├── main.cpp              # Main CLI entry point (advertiser + connector modes)
├── pch.h                 # Precompiled header (WinRT + standard includes)
├── SocketReaderWriter.h  # Socket R/W helper, data structures, constants
├── CMakeLists.txt        # CMake build configuration
├── build.bat             # Legacy build script for cl.exe (deprecated)
├── BUILD.md              # This file (build instructions)
└── USAGE.md              # Usage documentation
```

## Prerequisites

1. **Windows 10/11** (Build 17763 or later)
2. **Visual Studio Build Tools** (2019 or 2022) with:
   - "Desktop development with C++" workload
   - Windows 10/11 SDK (10.0.17763.0 or later)
   - C++/WinRT support (included in modern Windows SDKs)

> **Note**: The full Visual Studio IDE is NOT required. You can install just the
> [Build Tools for Visual Studio](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022)
> as a lightweight alternative.

## Build Instructions (CMake)

### Step 1: Open a Developer Command Prompt

You need a command prompt with the MSVC `cl.exe` toolchain configured.

**Option A** - From Start Menu:
```
Start Menu > Visual Studio 2022 > x64 Native Tools Command Prompt for VS 2022
```

**Option B** - Manually initialize the environment:
```cmd
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
```

### Step 2: Navigate to the project directory

```cmd
cd path\to\windows_wifi_direct_cli
```

### Step 3: Configure and build with CMake

```cmd
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

On success, this produces `WiFiDirectCLI.exe` in `build/Release/`.

#### Alternative: Using Ninja generator (faster builds)

```cmd
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Legacy Build (build.bat - deprecated)

The original `build.bat` script is still available for reference but is deprecated.
Please use CMake for new builds.

## Why Not g++?

**g++ (MinGW/MSYS2) is NOT supported** because:
- The code uses Windows Runtime (WinRT) APIs via C++/WinRT projection headers
- C++/WinRT requires `windowsapp.lib` (MSVC import library)
- C++/WinRT headers depend on MSVC-specific features and compiler support

The MSVC `cl.exe` compiler is the **only** supported compiler for WinRT-based applications.
However, no Visual Studio IDE or `.sln`/`.vcxproj` files are needed — everything builds
from a single `cl.exe` command on the command line.

## Troubleshooting

| Issue | Solution |
|-------|----------|
| `cl.exe not found` | Run from "x64 Native Tools Command Prompt" |
| `Windows SDK not found` | Install Windows SDK via Visual Studio Installer |
| `C++/WinRT headers not found` | Update SDK or install `Microsoft.Windows.CppWinRT` NuGet |
| `windowsapp.lib not found` | Ensure SDK lib path is correct for your architecture (x64) |
