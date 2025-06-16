#!/bin/bash
# Script to apply WebSocket++ patches for Boost 1.87+ compatibility

echo "WebSocket++ Boost 1.87+ Compatibility Patcher"
echo "============================================"

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo "Error: Please run this script from the project root directory"
    exit 1
fi

# Create deps directory if it doesn't exist
mkdir -p deps

# Clone WebSocket++ if not already present
if [ ! -d "deps/websocketpp" ]; then
    echo "Cloning WebSocket++..."
    git clone https://github.com/zaphoyd/websocketpp.git deps/websocketpp
    cd deps/websocketpp
else
    echo "WebSocket++ already exists, updating..."
    cd deps/websocketpp
    git fetch origin
fi

# Check if PR #1164 is already applied
if git log --oneline | grep -q "Compatibility fixes for Boost 1.87"; then
    echo "Boost 1.87 compatibility patches already applied"
else
    echo "Applying Boost 1.87 compatibility patches..."
    # Fetch the PR
    git fetch origin pull/1164/head:boost-1.87-compat
    
    # Cherry-pick or merge the changes
    git checkout develop
    git merge boost-1.87-compat --no-edit
    
    echo "Patches applied successfully"
fi

cd ../..

echo ""
echo "WebSocket++ has been patched for Boost 1.87+ compatibility"
echo "Update your CMakeLists.txt to use the local WebSocket++ copy:"
echo "  set(WEBSOCKETPP_ROOT \${CMAKE_CURRENT_SOURCE_DIR}/deps/websocketpp)"