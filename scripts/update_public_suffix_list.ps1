<#
.SYNOPSIS
    Regenerates src/core/net/PublicSuffixData.h from publicsuffix.org's list.

.DESCRIPTION
    The public suffix list decides where one party's domain ends and a registry
    begins. Cookie scoping and same-site (and, from M9, CORS credentials) all
    depend on it, and it cannot be derived from a hostname -- it is a curated
    file that changes as registries and hosting providers appear.

    The list is BUNDLED at build time, never fetched at runtime: a browser that
    must reach the network before it can evaluate cookie policy has a bootstrap
    problem on its first offline start, and fetching a security input over the
    network makes whoever can intercept it an author of your cookie boundaries.
    This is the model Chrome and Firefox use.

    The list is pinned to an upstream COMMIT, not to a URL, so the version in
    use has a reviewable identity and regeneration is reproducible. The pinned
    commit lives in the generated header itself, so there is one source of truth.

    Deliberately NO generation timestamp is written: -Check must produce a
    byte-identical file from the same commit, every time.

.PARAMETER Latest
    Resolve upstream HEAD and regenerate against it. Without this, regeneration
    uses the commit already recorded in the generated header.

.PARAMETER Sha
    Pin to a specific upstream commit instead of the recorded or latest one.

.PARAMETER Check
    Regenerate from the recorded commit and fail if the committed header differs.
    Catches hand-edits to a generated file, which the next regeneration would
    otherwise silently revert.

.PARAMETER CheckUpstream
    Fail if the recorded commit is not upstream HEAD. This is the release gate:
    a release and the public suffix list must be aligned.

.EXAMPLE
    scripts/update_public_suffix_list.ps1 -Latest
    scripts/update_public_suffix_list.ps1 -Check
    scripts/update_public_suffix_list.ps1 -CheckUpstream
#>
[CmdletBinding()]
param(
    [switch]$Latest,
    [string]$Sha,
    [switch]$Check,
    [switch]$CheckUpstream
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$RepoRoot = Split-Path -Parent $PSScriptRoot
$OutputPath = Join-Path $RepoRoot 'src/core/net/PublicSuffixData.h'
$VectorsPath = Join-Path $RepoRoot 'tests/fixtures/public_suffix_tests.txt'
$UpstreamRepo = 'publicsuffix/list'
$UpstreamFile = 'public_suffix_list.dat'
$UpstreamVectors = 'tests/tests.txt'

function Get-RequestHeaders {
    $headers = @{ 'Accept' = 'application/vnd.github+json'; 'User-Agent' = 'hummingbird-psl-updater' }
    # CI provides a token; using it lifts the anonymous 60-requests/hour limit.
    if ($env:GITHUB_TOKEN) { $headers['Authorization'] = "Bearer $($env:GITHUB_TOKEN)" }
    return $headers
}

function Get-UpstreamHeadSha {
    $uri = "https://api.github.com/repos/$UpstreamRepo/commits?path=$UpstreamFile&per_page=1"
    $response = Invoke-RestMethod -Uri $uri -Headers (Get-RequestHeaders)
    if (-not $response -or -not $response[0].sha) {
        throw "Could not resolve upstream HEAD for $UpstreamRepo/$UpstreamFile"
    }
    return $response[0].sha
}

function Get-RecordedSha {
    if (-not (Test-Path $OutputPath)) { return $null }
    $match = Select-String -Path $OutputPath -Pattern 'kUpstreamCommit\s*=\s*"([0-9a-f]{40})"' | Select-Object -First 1
    if (-not $match) { return $null }
    return $match.Matches[0].Groups[1].Value
}

function Get-UpstreamText([string]$commit, [string]$path) {
    $uri = "https://raw.githubusercontent.com/$UpstreamRepo/$commit/$path"
    $response = Invoke-WebRequest -Uri $uri -Headers @{ 'User-Agent' = 'hummingbird-psl-updater' }
    # Normalize line endings so parsing and comparison are identical everywhere.
    return ($response.Content -replace "`r`n", "`n")
}

function Get-ListAtSha([string]$commit) {
    return (Get-UpstreamText $commit $UpstreamFile) -split "`n"
}

# The list ships its own conformance vectors. They are vendored from the SAME
# commit as the rule data: refreshing one without the other would leave the test
# suite asserting an older list's answers against newer rules, and both files
# would still look individually valid.
function Get-VectorsAtSha([string]$commit) {
    return Get-UpstreamText $commit $UpstreamVectors
}

function Write-TextFile([string]$path, [string]$text) {
    $stream = [System.IO.StreamWriter]::new($path, $false, [System.Text.UTF8Encoding]::new($false))
    $stream.NewLine = "`n"
    $stream.Write(($text -replace "`r`n", "`n"))
    $stream.Close()
}

# publicsuffix.org publishes internationalized rules in Unicode. Emit BOTH forms:
# the punycode one for hosts that arrived encoded, and the Unicode one because
# the engine has no IDNA layer, so a hostname typed in a non-Latin script reaches
# the cookie code exactly as written. Matching only punycode would silently fall
# back to "the last label is the suffix" for those hosts — the too-permissive
# direction, on a security boundary. Costs one extra entry per IDN rule.
$script:Idn = New-Object System.Globalization.IdnMapping
function Get-RuleForms([string]$body) {
    $lower = $body.ToLowerInvariant()
    if ($lower -cmatch '^[a-z0-9.\-]+$') { return @($lower) }  # already ASCII
    try {
        $ascii = $script:Idn.GetAscii($lower).ToLowerInvariant()
    } catch {
        throw "Rule '$lower' is neither ASCII nor convertible to punycode: $_"
    }
    if ($ascii -eq $lower) { return @($lower) }
    return @($ascii, $lower)
}

# Emits a C++ string literal. Rules outside plain ASCII are escaped byte by byte
# rather than embedded as UTF-8 text, so the generated header does not depend on
# source-charset flags or a byte-order mark to compile identically everywhere.
function Format-CppLiteral([string]$rule) {
    if ($rule -cmatch '^[a-z0-9.\-]+$') { return "`"$rule`"" }
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($rule)
    $escaped = ($bytes | ForEach-Object { '\x{0:x2}' -f $_ }) -join ''
    # Every character is escaped, so no literal hex digit can extend an escape.
    return "`"$escaped`""
}

function Build-Header([string]$commit) {
    $lines = Get-ListAtSha $commit

    $exact = [System.Collections.Generic.HashSet[string]]::new()
    $wildcardParents = [System.Collections.Generic.HashSet[string]]::new()
    $exceptions = [System.Collections.Generic.HashSet[string]]::new()
    $icannCount = 0
    $privateCount = 0
    $section = 'none'

    foreach ($raw in $lines) {
        $line = $raw.Trim()
        if ($line -like '*===BEGIN ICANN DOMAINS===*') { $section = 'icann'; continue }
        if ($line -like '*===BEGIN PRIVATE DOMAINS===*') { $section = 'private'; continue }
        if ($line -like '*===END*') { $section = 'none'; continue }
        if ($line -eq '' -or $line.StartsWith('//')) { continue }

        # Both sections are kept. Cookie scoping needs PRIVATE too: github.io is
        # a PRIVATE entry, and it is exactly the "looks like an ordinary domain
        # but hands out subdomains to strangers" case the list exists for.
        if ($section -eq 'icann') { $icannCount++ } elseif ($section -eq 'private') { $privateCount++ }

        $isException = $line.StartsWith('!')
        $body = if ($isException) { $line.Substring(1) } else { $line }

        $isWildcard = $body.StartsWith('*.')
        if ($isWildcard) { $body = $body.Substring(2) }
        # A '*' anywhere but the leading label would need matching logic this
        # generator does not emit. Fail loudly rather than silently drop it.
        if ($body.Contains('*')) { throw "Unsupported wildcard position in rule: $line" }

        foreach ($form in (Get-RuleForms $body)) {
            if ($isException) { [void]$exceptions.Add($form) }
            elseif ($isWildcard) { [void]$wildcardParents.Add($form) }
            else { [void]$exact.Add($form) }
        }
    }

    if ($exact.Count -lt 5000) { throw "Only $($exact.Count) exact rules parsed; the upstream format probably changed" }

    # Ordinal sort: the C++ side binary-searches these, and every entry is
    # lowercase ASCII, so byte order and case-insensitive order agree.
    $exactSorted = [string[]]$exact; [Array]::Sort($exactSorted, [StringComparer]::Ordinal)
    $wildcardSorted = [string[]]$wildcardParents; [Array]::Sort($wildcardSorted, [StringComparer]::Ordinal)
    $exceptionSorted = [string[]]$exceptions; [Array]::Sort($exceptionSorted, [StringComparer]::Ordinal)

    $sb = [System.Text.StringBuilder]::new()
    [void]$sb.Append(@"
// GENERATED FILE -- DO NOT EDIT BY HAND.
//
// Regenerate with scripts/update_public_suffix_list.ps1 (see that script for why
// the list is bundled rather than fetched at runtime). CI checks that this file
// matches what the generator produces from the commit recorded below, so a hand
// edit here is reverted by the next refresh and will fail the build first.
//
// Source: https://github.com/$UpstreamRepo/blob/main/$UpstreamFile
// Licence: Mozilla Public License 2.0 (see THIRD_PARTY_NOTICES.md).
//
// Rules are lowercase. An internationalized rule appears TWICE, once punycode-
// encoded and once in its original Unicode form (byte-escaped): the engine has
// no IDNA layer, so a hostname typed in a non-Latin script reaches the cookie
// code exactly as written, and matching only punycode would fall back to "the
// last label is the suffix" for those hosts.
// Each array is sorted in byte order for binary search.

#pragma once

#include <array>
#include <string_view>

namespace Hummingbird::Core::PublicSuffixData {

// The upstream commit these rules were generated from. Deliberately the only
// provenance recorded: a timestamp would make regeneration non-reproducible and
// break the CI check that this file matches its source.
inline constexpr std::string_view kUpstreamCommit = "$commit";

// $icannCount ICANN rules + $privateCount PRIVATE rules, split by kind.

// Plain rules: "com", "co.uk", "github.io".
inline constexpr std::array<std::string_view, $($exactSorted.Count)> kExactRules{

"@)
    foreach ($rule in $exactSorted) { [void]$sb.Append("    std::string_view{$(Format-CppLiteral $rule)},`n") }
    [void]$sb.Append(@"
};

// Parents of wildcard rules: "*.ck" is stored as "ck", so a candidate suffix
// matches when everything after its first label is found here.
inline constexpr std::array<std::string_view, $($wildcardSorted.Count)> kWildcardParents{

"@)
    foreach ($rule in $wildcardSorted) { [void]$sb.Append("    std::string_view{$(Format-CppLiteral $rule)},`n") }
    [void]$sb.Append(@"
};

// Exception rules with their leading '!' stripped: "!www.ck" is stored as
// "www.ck". An exception beats every other match, and its public suffix is the
// rule minus its own first label.
inline constexpr std::array<std::string_view, $($exceptionSorted.Count)> kExceptionRules{

"@)
    foreach ($rule in $exceptionSorted) { [void]$sb.Append("    std::string_view{$(Format-CppLiteral $rule)},`n") }
    [void]$sb.Append(@"
};

}  // namespace Hummingbird::Core::PublicSuffixData
"@)

    return $sb.ToString()
}

# --- entry point -------------------------------------------------------------

$recorded = Get-RecordedSha

if ($CheckUpstream) {
    if (-not $recorded) { Write-Error "No commit recorded in $OutputPath; run with -Latest first."; exit 1 }
    $head = Get-UpstreamHeadSha
    if ($recorded -ne $head) {
        Write-Error @"
Public suffix list is behind upstream.
  bundled: $recorded
  upstream: $head
Run 'scripts/update_public_suffix_list.ps1 -Latest', review the diff, and merge
it before releasing. A release and the public suffix list must be aligned.
"@
        exit 1
    }
    Write-Host "Public suffix list is aligned with upstream ($head)."
    exit 0
}

$target = if ($Sha) { $Sha } elseif ($Latest) { Get-UpstreamHeadSha } elseif ($recorded) { $recorded } else { Get-UpstreamHeadSha }
$generated = Build-Header $target
$vectors = Get-VectorsAtSha $target

if ($Check) {
    # Collect every mismatch before reporting: with ErrorActionPreference=Stop a
    # Write-Error inside the loop would hide the second file's result.
    $problems = @()
    foreach ($pair in @(@{ Path = $OutputPath; Expected = $generated }, @{ Path = $VectorsPath; Expected = $vectors })) {
        if (-not (Test-Path $pair.Path)) { $problems += "$($pair.Path) is missing."; continue }
        $existing = (Get-Content -Raw -Path $pair.Path) -replace "`r`n", "`n"
        if ($existing -ne ($pair.Expected -replace "`r`n", "`n")) {
            $problems += "$($pair.Path) does not match what commit $target produces."
        }
    }
    if ($problems) {
        Write-Error @"
$($problems -join "`n")

A generated or vendored file was edited by hand -- the next automated refresh
would silently revert it. Re-run 'scripts/update_public_suffix_list.ps1' and
commit the result.
"@
        exit 1
    }
    Write-Host "Bundled public suffix data and conformance vectors match their source ($target)."
    exit 0
}

# LF endings regardless of platform, so the CI check is not fooled by CRLF.
Write-TextFile $OutputPath $generated
Write-TextFile $VectorsPath $vectors
Write-Host "Wrote $OutputPath and $VectorsPath from $UpstreamRepo@$target."
