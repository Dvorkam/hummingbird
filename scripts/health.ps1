# Codebase health snapshot: per-function complexity/size + copy-paste duplication,
# via lizard (a pure-Python analyzer — no compiler or compile_commands.json needed).
# Track the trend across milestones, not just absolute numbers.
#
#   scripts\health.ps1                    # summary + top offenders over src\
#   scripts\health.ps1 -Top 25
#   scripts\health.ps1 -Path src\style    # scope to a subtree
#   scripts\health.ps1 -CcnWarn 20        # tune the "too complex" line
#
# One-time setup:  python -m pip install lizard
param(
    [string]$Path = "src",
    [int]$Top = 15,
    [int]$CcnWarn = 15,   # a function above this cyclomatic complexity is a hotspot
    [int]$NlocWarn = 70   # ...or above this many lines
)
$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $PSScriptRoot

python -m lizard --version *> $null 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Error "lizard not found. Install it with:  python -m pip install lizard"
    exit 1
}

Push-Location $repo
try {
    # lizard --csv columns (no header): nloc, ccn, token, param, length, location, ...
    $cols = "nloc", "ccn", "token", "param", "length", "location"
    $rows = python -m lizard $Path -l cpp --csv 2>$null | ConvertFrom-Csv -Header $cols
    $funcs = foreach ($r in $rows) {
        [pscustomobject]@{
            CCN      = [int]$r.ccn
            NLOC     = [int]$r.nloc
            PARAM    = [int]$r.param
            Location = $r.location
        }
    }
    if (-not $funcs) { Write-Error "lizard returned no functions for '$Path'."; exit 1 }

    Write-Host "== Codebase health: $Path ($($funcs.Count) functions) ==" -ForegroundColor Cyan

    Write-Host "`n-- Top $Top by cyclomatic complexity (ladder-of-ifs / god-function risk) --" -ForegroundColor Yellow
    $funcs | Sort-Object CCN, NLOC -Descending | Select-Object -First $Top |
        Format-Table CCN, NLOC, PARAM, Location -AutoSize

    Write-Host "-- Top $Top by function length (NLOC) --" -ForegroundColor Yellow
    $funcs | Sort-Object NLOC, CCN -Descending | Select-Object -First $Top |
        Format-Table NLOC, CCN, PARAM, Location -AutoSize

    # Aggregates: the trend line to watch milestone-over-milestone.
    $avgCcn = [math]::Round(($funcs | Measure-Object CCN -Average).Average, 2)
    $avgNloc = [math]::Round(($funcs | Measure-Object NLOC -Average).Average, 1)
    $maxCcn = ($funcs | Measure-Object CCN -Maximum).Maximum
    $hotCcn = ($funcs | Where-Object CCN -gt $CcnWarn).Count
    $hotNloc = ($funcs | Where-Object NLOC -gt $NlocWarn).Count
    Write-Host "-- Aggregates --" -ForegroundColor Yellow
    Write-Host ("  functions        : {0}" -f $funcs.Count)
    Write-Host ("  avg CCN / max CCN : {0} / {1}" -f $avgCcn, $maxCcn)
    Write-Host ("  avg NLOC          : {0}" -f $avgNloc)
    Write-Host ("  hotspots          : {0} fn > CCN {1},  {2} fn > {3} NLOC" -f $hotCcn, $CcnWarn, $hotNloc, $NlocWarn)

    # Copy-paste duplication (the "forgotten twin" detector).
    Write-Host "`n-- Duplication --" -ForegroundColor Yellow
    python -m lizard $Path -l cpp -Eduplicate 2>$null |
        Select-String -Pattern 'duplicate rate|unique rate' | ForEach-Object { "  $($_.Line)" }

    # lizard exits non-zero as its warning count; a completed snapshot is a success.
    exit 0
}
finally {
    Pop-Location
}
