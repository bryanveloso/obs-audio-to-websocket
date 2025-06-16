@echo off
REM Local build script for Windows

echo OBS Audio to WebSocket - Local Build Script (Windows)
echo =================================================

REM Create deps directory
if not exist deps mkdir deps

REM Download WebSocket++ if not present
if not exist deps\websocketpp (
    echo Downloading WebSocket++...
    git clone https://github.com/zaphoyd/websocketpp.git deps\websocketpp
)

REM Download nlohmann/json if not present
if not exist deps\json\include\nlohmann\json.hpp (
    echo Downloading nlohmann/json...
    if not exist deps\json\include\nlohmann mkdir deps\json\include\nlohmann
    curl -L https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp -o deps\json\include\nlohmann\json.hpp
)

REM Check for vcpkg and install dependencies
if exist C:\vcpkg\vcpkg.exe (
    echo Installing dependencies via vcpkg...
    C:\vcpkg\vcpkg.exe install boost-system:x64-windows boost-thread:x64-windows
) else (
    echo WARNING: vcpkg not found at C:\vcpkg
    echo Please install Boost manually or install vcpkg
)

REM Configure with CMake
echo Configuring with CMake...
cmake -B build ^
    -G "Visual Studio 17 2022" ^
    -A x64 ^
    -DCMAKE_BUILD_TYPE=Release

REM Build
echo Building plugin...
cmake --build build --config Release

echo Build complete! Plugin binary is in build\Release\