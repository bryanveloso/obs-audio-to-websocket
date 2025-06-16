#!/bin/bash

# Install macOS dependencies for obs-audio-to-websocket

set -e

echo "Installing macOS dependencies..."

# Check if Homebrew is installed
if ! command -v brew &> /dev/null; then
    echo "Homebrew not found. Installing Homebrew..."
    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
fi

# Install libwebsockets
echo "Installing libwebsockets..."
brew install libwebsockets

# Install nlohmann-json (if not already available through obs-deps)
echo "Installing nlohmann-json..."
brew install nlohmann-json || true

echo "Dependencies installed successfully!"

# Export paths for CMake
echo "Setting up environment variables..."
if [[ $(arch) == "arm64" ]]; then
    export LIBWEBSOCKETS_ROOT="/opt/homebrew"
else
    export LIBWEBSOCKETS_ROOT="/usr/local"
fi

echo "LIBWEBSOCKETS_ROOT=${LIBWEBSOCKETS_ROOT}"