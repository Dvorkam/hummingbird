# Builds Hummingbird. Sets up the MSVC dev environment automatically on Windows,
# so this works from any plain PowerShell (no "x64 Native Tools" prompt needed).
#
#   scripts\build.ps1                 # Release build with the local preset
#   scripts\build.ps1 -Config Debug
#   scripts\build.ps1 -Configure      # force a fresh CMake configure first
param(
    [string]$Config = "Release",
    [string]$Preset = "",
    [switch]$Configure,
    # Compile-time log level (OFF|ERROR|WARN|INFO|DEBUG). Sticky: persists in the
    # CMake cache until changed again. Implies -Configure when set.
    [string]$LogLevel = ""
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
    if (-not (Test-Path $vswhere)) { throw "vswhere.exe not found; install Visual Studio with C++ tools." }
    $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $vsPath) { throw "No Visual Studio installation with C++ tools found." }
    Import-Module (Join-Path $vsPath "Common7\Tools\Microsoft.VisualStudio.DevShell.dll")
    Enter-VsDevShell -VsInstallPath $vsPath -Arch amd64 -SkipAutomaticLocation 2>$null | Out-Null
}

# A running Hummingbird locks the exe and the link step fails half-visibly,
# leaving a stale binary that no longer matches the sources. Fail loudly.
$exe = Join-Path $repo "build/Release/Hummingbird.exe"
$running = Get-Process -Name "Hummingbird" -ErrorAction SilentlyContinue
if ($running) {
    Write-Error "Hummingbird.exe is running (PID $($running.Id -join ', ')). Close it before building, or the new binary cannot be written."
    exit 1
}

Push-Location $repo
try {
    if ($LogLevel) {
        cmake --preset $Preset "-DHB_LOG_LEVEL=$LogLevel"
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    } elseif ($Configure -or -not (Test-Path (Join-Path $repo "build/CMakeCache.txt"))) {
        cmake --preset $Preset
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
    cmake --build --preset $Preset --config $Config
    exit $LASTEXITCODE
} finally {
    Pop-Location
}
