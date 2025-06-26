[CmdletBinding()]
param(
    [ValidateSet('x64')]
    [string] $Target = 'x64',
    [ValidateSet('Debug', 'RelWithDebInfo', 'Release', 'MinSizeRel')]
    [string] $Configuration = 'RelWithDebInfo'
)

$ErrorActionPreference = 'Stop'

if ( $DebugPreference -eq 'Continue' ) {
    $VerbosePreference = 'Continue'
    $InformationPreference = 'Continue'
}

if ( $env:CI -eq $null ) {
    throw "Package-Windows.ps1 requires CI environment"
}

if ( ! ( [System.Environment]::Is64BitOperatingSystem ) ) {
    throw "Packaging script requires a 64-bit system to build and run."
}

if ( $PSVersionTable.PSVersion -lt '7.2.0' ) {
    Write-Warning 'The packaging script requires PowerShell Core 7. Install or upgrade your PowerShell version: https://aka.ms/pscore6'
    exit 2
}

function Package {
    trap {
        Write-Error $_
        exit 2
    }

    $ScriptHome = $PSScriptRoot
    $ProjectRoot = Resolve-Path -Path "$PSScriptRoot/../.."
    $BuildSpecFile = "${ProjectRoot}/buildspec.json"

    $UtilityFunctions = Get-ChildItem -Path $PSScriptRoot/utils.pwsh/*.ps1 -Recurse

    foreach( $Utility in $UtilityFunctions ) {
        Write-Debug "Loading $($Utility.FullName)"
        . $Utility.FullName
    }

    $BuildSpec = Get-Content -Path ${BuildSpecFile} -Raw | ConvertFrom-Json
    $ProductName = $BuildSpec.name
    $ProductVersion = $BuildSpec.version

    $OutputName = "${ProductName}-${ProductVersion}-windows-${Target}"

    $RemoveArgs = @{
        ErrorAction = 'SilentlyContinue'
        Path = @(
            "${ProjectRoot}/release/${ProductName}-*-windows-*.zip"
        )
    }

    Remove-Item @RemoveArgs

    Log-Group "Archiving ${ProductName}..."
    
    # Check if files exist in the expected location
    if (-not (Test-Path "${ProjectRoot}/release/${Configuration}")) {
        Write-Error "Build directory not found: ${ProjectRoot}/release/${Configuration}"
        exit 1
    }
    
    # Create standard package (original structure)
    $CompressArgs = @{
        Path = (Get-ChildItem -Path "${ProjectRoot}/release/${Configuration}" -Exclude "${OutputName}*.*")
        CompressionLevel = 'Optimal'
        DestinationPath = "${ProjectRoot}/release/${OutputName}.zip"
        Verbose = ($Env:CI -ne $null)
    }
    Compress-Archive -Force @CompressArgs
    
    # Create portable package (for direct extraction to OBS directory)
    Log-Group "Creating portable package..."
    
    $PortableDir = "${ProjectRoot}/release/portable"
    if (Test-Path $PortableDir) {
        Remove-Item -Path $PortableDir -Recurse -Force
    }
    
    # Create directory structure matching OBS layout
    New-Item -ItemType Directory -Force -Path "$PortableDir/obs-plugins/64bit" | Out-Null
    New-Item -ItemType Directory -Force -Path "$PortableDir/data/obs-plugins/${ProductName}/locale" | Out-Null
    
    # The Windows build puts files in ${ProductName}/bin/64bit/ subdirectory
    $PluginBinDir = "${ProjectRoot}/release/${Configuration}/${ProductName}/bin/64bit"
    $PluginDataDir = "${ProjectRoot}/release/${Configuration}/${ProductName}/data"
    
    # Copy plugin files
    if (Test-Path "$PluginBinDir/${ProductName}.dll") {
        Copy-Item "$PluginBinDir/${ProductName}.dll" -Destination "$PortableDir/obs-plugins/64bit/"
        Copy-Item "$PluginBinDir/${ProductName}.pdb" -Destination "$PortableDir/obs-plugins/64bit/" -ErrorAction SilentlyContinue
    } else {
        Write-Error "Plugin DLL not found at: $PluginBinDir/${ProductName}.dll"
        exit 1
    }
    
    # Copy locale files
    if (Test-Path "$PluginDataDir/locale") {
        Copy-Item -Path "$PluginDataDir/locale/*" -Destination "$PortableDir/data/obs-plugins/${ProductName}/locale/" -Recurse
    }
    
    # Create portable ZIP
    $PortableCompressArgs = @{
        Path = "$PortableDir/*"
        CompressionLevel = 'Optimal'
        DestinationPath = "${ProjectRoot}/release/${OutputName}-Portable.zip"
        Force = $true
    }
    Compress-Archive @PortableCompressArgs
    
    # Clean up temp directory
    Remove-Item -Path $PortableDir -Recurse -Force
    
    Log-Group
}

Package
