#!/bin/bash
# Local build script for testing

echo "OBS Audio to WebSocket - Local Build Script"
echo "======================================="

# Create deps directory
mkdir -p deps

# Download WebSocket++ if not present
if [ ! -d "deps/websocketpp" ]; then
    echo "Downloading WebSocket++..."
    git clone https://github.com/zaphoyd/websocketpp.git deps/websocketpp
fi

# Download nlohmann/json if not present
if [ ! -f "deps/json/include/nlohmann/json.hpp" ]; then
    echo "Downloading nlohmann/json..."
    mkdir -p deps/json/include/nlohmann
    curl -L https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp -o deps/json/include/nlohmann/json.hpp
fi

# Platform-specific build
if [[ "$OSTYPE" == "darwin"* ]]; then
    echo "Building for macOS..."
    
    # Install dependencies via Homebrew if needed
    brew list boost &>/dev/null || brew install boost
    brew list qt@6 &>/dev/null || brew install qt@6
    
    # Configure
    cmake -B build \
        -DCMAKE_BUILD_TYPE=Release \
        -DQt6_DIR="$(brew --prefix qt@6)/lib/cmake/Qt6" \
        -DCMAKE_PREFIX_PATH="$(brew --prefix)" \
        -DOBS_DIR="/Applications/OBS.app/Contents/Resources"
        
elif [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "win32" ]]; then
    echo "Building for Windows..."
    echo "Please use build-local.bat for Windows builds"
    exit 1
else
    echo "Building for Linux..."
    
    # Configure
    cmake -B build \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_PREFIX_PATH="/usr/lib/x86_64-linux-gnu/cmake"
fi

# Build
echo "Building plugin..."
cmake --build build --config Release

echo "Build complete! Plugin binary is in build/"