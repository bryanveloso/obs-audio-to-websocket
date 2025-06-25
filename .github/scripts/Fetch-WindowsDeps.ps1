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

# Download SQLite3 headers first
$SqliteDir = Join-Path $DepsDir "sqlite3"
if (-not (Test-Path $SqliteDir)) {
    Write-Host "Downloading SQLite3 headers..."
    $SqliteVersion = "3470200"  # 3.47.2
    $SqliteUrl = "https://www.sqlite.org/2024/sqlite-amalgamation-${SqliteVersion}.zip"
    $SqliteTempFile = Join-Path $DepsDir "sqlite3.zip"
    
    Invoke-WebRequest -Uri $SqliteUrl -OutFile $SqliteTempFile
    Expand-Archive -Path $SqliteTempFile -DestinationPath $DepsDir -Force
    Move-Item -Path (Join-Path $DepsDir "sqlite-amalgamation-${SqliteVersion}") -Destination $SqliteDir -Force
    Remove-Item $SqliteTempFile
}

# Download pre-built libwebsockets (using vcpkg export)
$TempFile = Join-Path $DepsDir "libwebsockets.zip"

# This would be a URL to pre-built binaries
# For now, we'll build from source
Write-Host "Building libwebsockets from source..."

$LwsVersion = "4.2.2"
$LwsUrl = "https://github.com/warmcat/libwebsockets/archive/refs/tags/v${LwsVersion}.zip"

Invoke-WebRequest -Uri $LwsUrl -OutFile $TempFile
Expand-Archive -Path $TempFile -DestinationPath $DepsDir -Force
Move-Item -Path (Join-Path $DepsDir "libwebsockets-${LwsVersion}") -Destination $LibwebsocketsDir -Force
Remove-Item $TempFile

# Build libwebsockets
Push-Location $LibwebsocketsDir
try {
    # Clean any existing build directory
    if (Test-Path "build") {
        Remove-Item -Path "build" -Recurse -Force
        Write-Host "Cleaned existing build directory"
    }
    
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
        -DLWS_STATIC_PIC=OFF `
        -DLWS_WITH_STRUCT_JSON=OFF `
        -DLWS_WITH_JSON=OFF `
        -DLWS_WITH_SQLITE3=OFF `
        -DLWS_WITH_STRUCT_SQLITE3=OFF `
        -DLWS_SQLITE3_INCLUDE_DIRS="${SqliteDir}"
    
    cmake --build . --config Release
    
    Write-Host "libwebsockets built successfully!"
    
    # Find where the library was actually built
    $LibFile = Get-ChildItem -Path . -Recurse -Include "*.lib" | Select-Object -First 1
    if ($LibFile) {
        Write-Host "Found library at: $($LibFile.FullName)"
    }
    
    # Find lws_config.h
    $ConfigFile = Get-ChildItem -Path . -Recurse -Include "lws_config.h" | Select-Object -First 1
    if ($ConfigFile) {
        Write-Host "Found lws_config.h at: $($ConfigFile.FullName)"
        Write-Host "Parent directory: $($ConfigFile.DirectoryName)"
    }
} finally {
    Pop-Location
}

# Return the path for the build script to use
Write-Host "libwebsockets installed at: $LibwebsocketsDir"
return $LibwebsocketsDir