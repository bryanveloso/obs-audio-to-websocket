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

# Download and build libwebsockets
Write-Host "Downloading libwebsockets..."
$LwsVersion = "4.3.3"
$LwsUrl = "https://github.com/warmcat/libwebsockets/archive/refs/tags/v${LwsVersion}.zip"
$LwsZip = Join-Path $DepsDir "libwebsockets.zip"
$LwsDir = Join-Path $DepsDir "libwebsockets"

Invoke-WebRequest -Uri $LwsUrl -OutFile $LwsZip
Expand-Archive -Path $LwsZip -DestinationPath $DepsDir -Force
Move-Item -Path (Join-Path $DepsDir "libwebsockets-${LwsVersion}") -Destination $LwsDir -Force -ErrorAction SilentlyContinue
Remove-Item $LwsZip

# Build libwebsockets
Write-Host "Building libwebsockets..."
$LwsBuildDir = Join-Path $LwsDir "build"
New-Item -ItemType Directory -Force -Path $LwsBuildDir | Out-Null

Push-Location $LwsBuildDir

# Configure with minimal options
cmake .. `
    -DLWS_WITH_SSL=OFF `
    -DLWS_WITHOUT_TESTAPPS=ON `
    -DLWS_WITHOUT_TEST_SERVER=ON `
    -DLWS_WITHOUT_TEST_CLIENT=ON `
    -DLWS_WITHOUT_TEST_PING=ON `
    -DLWS_WITHOUT_EXTENSIONS=ON `
    -DLWS_WITHOUT_BUILTIN_GETIFADDRS=ON `
    -DCMAKE_BUILD_TYPE=Release `
    -DCMAKE_INSTALL_PREFIX="$LwsDir/install"

# Build and install
cmake --build . --config Release
cmake --install . --config Release

Pop-Location

# Return the paths for the parent script to use
$result = @{
    CMAKE_PREFIX_PATH = Join-Path $DepsDir "json"
    LIBWEBSOCKETS_ROOT = Join-Path $LwsDir "install"
}

Write-Host "Dependencies installed successfully!"
Write-Host "CMAKE_PREFIX_PATH: $($result.CMAKE_PREFIX_PATH)"
Write-Host "LIBWEBSOCKETS_ROOT: $($result.LIBWEBSOCKETS_ROOT)"

# Return the paths
return $result