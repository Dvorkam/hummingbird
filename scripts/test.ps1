# Runs the Hummingbird test suite. Sets up the MSVC dev environment on Windows
# the same way scripts\build.ps1 does.
#
#   scripts\test.ps1                    # full suite, Release
#   scripts\test.ps1 -Filter FlexLayout # only matching tests
#   scripts\test.ps1 -Build             # build first, then test
param(
    [string]$Config = "Release",
    [string]$Preset = "",
    [string]$Filter = "",
    [switch]$Build
)
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot

if (-not $Preset) {
    if (Test-Path (Join-Path $repo "CMakeUserPresets.json")) {
        $Preset = "ninja-multi-vcpkg-local"
    } else {
        $Preset = "ninja-multi-vcpkg"
    }
}

$onWindows = ($null -eq $IsWindows) -or $IsWindows
if ($onWindows -and -not (Get-Command cl -ErrorAction SilentlyContinue)) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($vsPath) {
            Import-Module (Join-Path $vsPath "Common7\Tools\Microsoft.VisualStudio.DevShell.dll")
            Enter-VsDevShell -VsInstallPath $vsPath -Arch amd64 -SkipAutomaticLocation 2>$null | Out-Null
        }
    }
}

Push-Location $repo
try {
    if ($Build) {
        & (Join-Path $PSScriptRoot "build.ps1") -Config $Config -Preset $Preset
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
    $ctestArgs = @("--preset", $Preset, "-C", $Config, "--output-on-failure")
    if ($Filter) { $ctestArgs += @("-R", $Filter) }
    ctest @ctestArgs
    exit $LASTEXITCODE
} finally {
    Pop-Location
}
