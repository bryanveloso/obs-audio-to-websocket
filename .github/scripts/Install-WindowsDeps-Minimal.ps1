#!/usr/bin/env pwsh

# Minimal dependency installer for Windows - libwebsockets and nlohmann/json

$ErrorActionPreference = 'Stop'

Write-Host "Installing minimal Windows dependencies for obs-audio-to-websocket..."

# Create deps directory
$DepsDir = Join-Path $PSScriptRoot "../../deps"
New-Item -ItemType Directory -Force -Path $DepsDir | Out-Null

# Download nlohmann/json (header-only)
Write-Host "Downloading nlohmann/json..."
$JsonDir = Join-Path $DepsDir "json/include/nlohmann"
New-Item -ItemType Directory -Force -Path $JsonDir | Out-Null
$JsonUrl = "https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp"
Invoke-WebRequest -Uri $JsonUrl -OutFile (Join-Path $JsonDir "json.hpp")

# For Windows, we need to build libwebsockets from source
# since there are no official Windows binaries that work well with MSVC
Write-Host "Downloading libwebsockets source..."
$LwsVersion = "4.3.3"
$LwsUrl = "https://github.com/warmcat/libwebsockets/archive/refs/tags/v${LwsVersion}.zip"
$LwsZip = Join-Path $DepsDir "libwebsockets.zip"
$LwsDir = Join-Path $DepsDir "libwebsockets"

Invoke-WebRequest -Uri $LwsUrl -OutFile $LwsZip
Expand-Archive -Path $LwsZip -DestinationPath $DepsDir -Force
Move-Item -Path (Join-Path $DepsDir "libwebsockets-${LwsVersion}") -Destination $LwsDir -Force -ErrorAction SilentlyContinue
Remove-Item $LwsZip

# Build libwebsockets for Windows
Write-Host "Building libwebsockets..."
$LwsBuildDir = Join-Path $LwsDir "build"
New-Item -ItemType Directory -Force -Path $LwsBuildDir | Out-Null

Push-Location $LwsBuildDir
try {
    # Configure with minimal options
    cmake .. -G "Visual Studio 17 2022" -A x64 `
        -DLWS_WITH_SSL=OFF `
        -DLWS_WITHOUT_TESTAPPS=ON `
        -DLWS_WITHOUT_TEST_SERVER=ON `
        -DLWS_WITHOUT_TEST_CLIENT=ON `
        -DLWS_WITHOUT_EXTENSIONS=ON `
        -DLWS_WITH_SHARED=OFF `
        -DLWS_WITH_STATIC=ON `
        -DLWS_WITH_STRUCT_JSON=OFF `
        -DLWS_WITH_STRUCT_SQLITE3=OFF `
        -DLWS_WITH_SQLITE3=OFF
    
    # Build Release configuration
    cmake --build . --config Release
    
    Write-Host "libwebsockets built successfully!"
} finally {
    Pop-Location
}

# Return the paths for the parent script to use
$result = @{
    CMAKE_PREFIX_PATH = Join-Path $DepsDir "json"
    LIBWEBSOCKETS_ROOT = $LwsDir
}

Write-Host "Dependencies installed successfully!"
Write-Host "CMAKE_PREFIX_PATH: $($result.CMAKE_PREFIX_PATH)"
Write-Host "LIBWEBSOCKETS_ROOT: $($result.LIBWEBSOCKETS_ROOT)"

# Return the paths
return $result