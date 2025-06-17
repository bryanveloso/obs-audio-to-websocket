#!/usr/bin/env pwsh

# Fetch pre-built libwebsockets for Windows CI

$ErrorActionPreference = 'Stop'

$DepsDir = Join-Path $PSScriptRoot "../../.deps"
$LibwebsocketsDir = Join-Path $DepsDir "libwebsockets"

if (Test-Path $LibwebsocketsDir) {
    Write-Host "libwebsockets already present"
    exit 0
}

Write-Host "Fetching pre-built libwebsockets..."

# Create deps directory
New-Item -ItemType Directory -Force -Path $DepsDir | Out-Null

# Download pre-built libwebsockets (using vcpkg export)
$TempFile = Join-Path $DepsDir "libwebsockets.zip"

# This would be a URL to pre-built binaries
# For now, we'll build from source
Write-Host "Building libwebsockets from source..."

$LwsVersion = "4.3.3"
$LwsUrl = "https://github.com/warmcat/libwebsockets/archive/refs/tags/v${LwsVersion}.zip"

Invoke-WebRequest -Uri $LwsUrl -OutFile $TempFile
Expand-Archive -Path $TempFile -DestinationPath $DepsDir -Force
Move-Item -Path (Join-Path $DepsDir "libwebsockets-${LwsVersion}") -Destination $LibwebsocketsDir -Force
Remove-Item $TempFile

# Build libwebsockets
Push-Location $LibwebsocketsDir
try {
    New-Item -ItemType Directory -Force -Path "build" | Out-Null
    Set-Location "build"
    
    cmake .. -G "Visual Studio 17 2022" -A x64 `
        -DCMAKE_BUILD_TYPE=Release `
        -DLWS_WITH_SSL=OFF `
        -DLWS_WITHOUT_TESTAPPS=ON `
        -DLWS_WITHOUT_TEST_SERVER=ON `
        -DLWS_WITHOUT_TEST_CLIENT=ON `
        -DLWS_WITHOUT_EXTENSIONS=ON `
        -DLWS_WITH_SHARED=OFF `
        -DLWS_WITH_STATIC=ON `
        -DLWS_STATIC_PIC=OFF
    
    cmake --build . --config Release
    
    Write-Host "libwebsockets built successfully!"
} finally {
    Pop-Location
}

# Set environment variable for CMake
$env:CMAKE_PREFIX_PATH = "${env:CMAKE_PREFIX_PATH};${LibwebsocketsDir}"
Write-Host "CMAKE_PREFIX_PATH updated"