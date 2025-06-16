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

# Clean existing libwebsockets directory to force fresh build
if (Test-Path $LwsDir) {
    Write-Host "Removing existing libwebsockets directory..."
    Remove-Item -Path $LwsDir -Recurse -Force
}

Invoke-WebRequest -Uri $LwsUrl -OutFile $LwsZip
Expand-Archive -Path $LwsZip -DestinationPath $DepsDir -Force
Move-Item -Path (Join-Path $DepsDir "libwebsockets-${LwsVersion}") -Destination $LwsDir -Force
Remove-Item $LwsZip

# Build libwebsockets for Windows
Write-Host "Building libwebsockets..."
$LwsBuildDir = Join-Path $LwsDir "build"

# Clean build directory if it exists
if (Test-Path $LwsBuildDir) {
    Write-Host "Cleaning existing build directory..."
    Remove-Item -Path $LwsBuildDir -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $LwsBuildDir | Out-Null

Push-Location $LwsBuildDir
try {
    # Configure with minimal options
    # Force clean configure to ensure no cached values
    cmake .. -G "Visual Studio 17 2022" -A x64 `
        -DLWS_WITH_SSL=OFF `
        -DLWS_WITHOUT_TESTAPPS=ON `
        -DLWS_WITHOUT_TEST_SERVER=ON `
        -DLWS_WITHOUT_TEST_CLIENT=ON `
        -DLWS_WITHOUT_EXTENSIONS=ON `
        -DLWS_WITH_SHARED=OFF `
        -DLWS_WITH_STATIC=ON `
        -DLWS_WITH_STRUCT=OFF `
        -DLWS_WITH_STRUCT_JSON=OFF `
        -DLWS_WITH_STRUCT_SQLITE3=OFF `
        -DLWS_WITH_SQLITE3=OFF `
        -DLWS_WITHOUT_BUILTIN_GETIFADDRS=ON `
        -DLWS_WITH_EXTERNAL_POLL=OFF `
        -DLWS_WITH_LIBUV=OFF `
        -DLWS_WITH_CUSTOM_HEADERS=OFF `
        -DLWS_WITH_STRUCT=OFF `
        -DLWS_WITH_PLUGINS=OFF `
        -DLWS_WITH_LWSAC=OFF
    
    # Build Release configuration
    cmake --build . --config Release
    
    # Debug: Check what was actually built
    Write-Host "Checking build output..."
    $LibPath = Join-Path $LwsBuildDir "lib/Release"
    if (Test-Path $LibPath) {
        Write-Host "Contents of lib/Release:"
        Get-ChildItem $LibPath | ForEach-Object { Write-Host "  $_" }
    }
    
    $LibPath2 = Join-Path $LwsBuildDir "Release"
    if (Test-Path $LibPath2) {
        Write-Host "Contents of Release:"
        Get-ChildItem $LibPath2 | ForEach-Object { Write-Host "  $_" }
    }
    
    # Try to find the library anywhere in the build directory
    Write-Host "Searching for libwebsockets library files..."
    $LibFiles = Get-ChildItem -Path $LwsBuildDir -Recurse -Include "*.lib", "*.a" -ErrorAction SilentlyContinue
    if ($LibFiles) {
        Write-Host "Found library files:"
        $LibFiles | ForEach-Object { Write-Host "  $($_.FullName)" }
    } else {
        Write-Host "WARNING: No library files found in build directory!"
    }
    
    # Verify that SQLite support is disabled in the generated config
    $ConfigFile = Join-Path $LwsBuildDir "lws_config.h"
    if (Test-Path $ConfigFile) {
        Write-Host "Checking generated lws_config.h for SQLite defines..."
        $SqliteDefines = Select-String -Path $ConfigFile -Pattern "LWS_WITH_STRUCT_SQLITE3|LWS_WITH_SQLITE3"
        if ($SqliteDefines) {
            Write-Host "WARNING: Found SQLite defines in lws_config.h:"
            $SqliteDefines | ForEach-Object { Write-Host "  $_" }
        } else {
            Write-Host "Good: No SQLite defines found in lws_config.h"
        }
    }
    
    Write-Host "libwebsockets built successfully!"
    
    # Also check the installed headers
    $InstalledConfigFile = Join-Path $LwsDir "include/lws_config.h"
    if (Test-Path $InstalledConfigFile) {
        Write-Host "Checking installed lws_config.h..."
        $InstalledSqliteDefines = Select-String -Path $InstalledConfigFile -Pattern "LWS_WITH_STRUCT_SQLITE3|LWS_WITH_SQLITE3"
        if ($InstalledSqliteDefines) {
            Write-Host "WARNING: Found SQLite defines in installed lws_config.h:"
            $InstalledSqliteDefines | ForEach-Object { Write-Host "  $_" }
        }
    }
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