cmake_minimum_required(VERSION 3.15)
project(WiFiDirectCLI LANGUAGES CXX)

# Require MSVC compiler (C++/WinRT only works with MSVC)
if(NOT MSVC)
    message(FATAL_ERROR "This project requires the MSVC compiler. "
        "C++/WinRT headers are not compatible with GCC/Clang. "
        "Please use Visual Studio or MSVC Build Tools.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Executable target
add_executable(WiFiDirectCLI main.cpp)

# Preprocessor definitions
target_compile_definitions(WiFiDirectCLI PRIVATE
    WIN32_LEAN_AND_MEAN
    _UNICODE
    UNICODE
)

# Compiler options
target_compile_options(WiFiDirectCLI PRIVATE
    /EHsc   # Enable C++ exception handling
    /W3     # Warning level 3
    /O2     # Optimize for speed
)

# --- Windows SDK / C++/WinRT include paths ---
# CMake provides CMAKE_WINDOWS_KITS_10_DIR and CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION
# to help locate the Windows SDK automatically.
if(DEFINED CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION)
    set(WIN_SDK_VER ${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION})
elseif(DEFINED ENV{WindowsSDKVersion})
    # Strip trailing backslash from environment variable
    string(REGEX REPLACE "[/\\\\]$" "" WIN_SDK_VER "$ENV{WindowsSDKVersion}")
else()
    message(FATAL_ERROR "Cannot detect Windows SDK version. "
        "Please run CMake from a Visual Studio Developer Command Prompt "
        "or specify -DCMAKE_SYSTEM_VERSION=<SDK_VERSION>.")
endif()

# Determine SDK root path
if(DEFINED CMAKE_WINDOWS_KITS_10_DIR)
    set(SDK_ROOT "${CMAKE_WINDOWS_KITS_10_DIR}")
elseif(DEFINED ENV{WindowsSdkDir})
    set(SDK_ROOT "$ENV{WindowsSdkDir}")
else()
    set(SDK_ROOT "C:/Program Files (x86)/Windows Kits/10")
endif()

set(SDK_INC "${SDK_ROOT}/Include/${WIN_SDK_VER}")

# Verify C++/WinRT headers exist
if(NOT EXISTS "${SDK_INC}/cppwinrt/winrt/Windows.Foundation.h")
    message(FATAL_ERROR "C++/WinRT headers not found at: ${SDK_INC}/cppwinrt\n"
        "Please ensure your Windows SDK (${WIN_SDK_VER}) includes C++/WinRT support.\n"
        "Install via: Visual Studio Installer > Individual Components > C++/WinRT")
endif()

message(STATUS "Windows SDK version: ${WIN_SDK_VER}")
message(STATUS "Windows SDK root: ${SDK_ROOT}")
message(STATUS "C++/WinRT headers found.")

# Add include directories
target_include_directories(WiFiDirectCLI PRIVATE
    "${SDK_INC}/cppwinrt"
    "${SDK_INC}/um"
    "${SDK_INC}/ucrt"
    "${SDK_INC}/shared"
    "${SDK_INC}/winrt"
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

# Link libraries
target_link_libraries(WiFiDirectCLI PRIVATE
    windowsapp.lib
    ole32.lib
)
