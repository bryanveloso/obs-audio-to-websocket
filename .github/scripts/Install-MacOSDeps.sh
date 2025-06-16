#!/bin/bash

# Install macOS dependencies for obs-audio-to-websocket

set -e

echo "Installing macOS dependencies..."

# Check if Homebrew is installed
if ! command -v brew &> /dev/null; then
    echo "Homebrew not found. Installing Homebrew..."
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
fi

# Install nlohmann-json (if not already available through obs-deps)
echo "Installing nlohmann-json..."
brew install nlohmann-json || true

# Build libwebsockets as universal binary since Homebrew only provides single-arch
PROJECT_ROOT="${PROJECT_ROOT:-$(cd "$(dirname "$0")/../.." && pwd)}"
DEPS_DIR="${PROJECT_ROOT}/deps"
LWS_DIR="${DEPS_DIR}/libwebsockets"

echo "Building libwebsockets as universal binary..."
mkdir -p "$DEPS_DIR"

# Check if we need to rebuild
if [ ! -f "${LWS_DIR}/build/lib/libwebsockets.a" ]; then
    # Clean up any existing directory
    if [ -d "$LWS_DIR" ]; then
        rm -rf "$LWS_DIR"
    fi
    
    cd "$DEPS_DIR"
    
    # Download libwebsockets
    echo "Downloading libwebsockets..."
    curl -L "https://github.com/warmcat/libwebsockets/archive/refs/tags/v4.3.3.tar.gz" -o libwebsockets.tar.gz
    tar -xzf libwebsockets.tar.gz
    mv libwebsockets-4.3.3 libwebsockets
    rm libwebsockets.tar.gz
    
    cd libwebsockets
    
    # Fix the CMakeLists.txt to work with modern CMake
    echo "Patching libwebsockets CMakeLists.txt..."
    sed -i '' 's/cmake_minimum_required(VERSION 2.8.12)/cmake_minimum_required(VERSION 3.5)/' CMakeLists.txt
    
    mkdir -p build
    cd build
    
    # Configure for universal binary matching OBS requirements
    echo "Configuring libwebsockets..."
    cmake .. \
        -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
        -DLWS_WITH_SSL=OFF \
        -DLWS_WITHOUT_TESTAPPS=ON \
        -DLWS_WITHOUT_TEST_SERVER=ON \
        -DLWS_WITHOUT_TEST_CLIENT=ON \
        -DLWS_WITHOUT_EXTENSIONS=ON \
        -DLWS_WITH_SHARED=OFF \
        -DLWS_WITH_STATIC=ON \
        -DLWS_WITH_STRUCT=OFF \
        -DLWS_WITH_STRUCT_JSON=OFF \
        -DLWS_WITH_STRUCT_SQLITE3=OFF \
        -DLWS_WITH_SQLITE3=OFF
    
    # Build
    echo "Building libwebsockets..."
    cmake --build . --config Release
    
    echo "libwebsockets built successfully!"
else
    echo "libwebsockets already built, skipping..."
fi

echo "Dependencies installed successfully!"

# Export paths for CMake
export LIBWEBSOCKETS_ROOT="${LWS_DIR}"
echo "LIBWEBSOCKETS_ROOT=${LIBWEBSOCKETS_ROOT}"