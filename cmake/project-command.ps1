#requires -Version 7.4
# dd project command backing scripts for potato.
#
# Ported from the pre-dd bespoke driver:
#   layout            - run the browser's layout engine over an HTML file and report
#   analyze-wiki-css  - fetch Wikipedia's vector-2022 stylesheet and extract :root vars
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false

$request = [Console]::In.ReadLine() | ConvertFrom-Json -AsHashtable
$root = Split-Path $PSScriptRoot
if ($request.schema -ne 1 -or $request.projectRoot -ne $root -or $request.parameters -isnot [hashtable] -or
    $request.dryRun -isnot [bool] -or $request.command -notin @('layout', 'analyze-wiki-css')) {
    throw 'Invalid project-command request.'
}
Set-Location $root

$p = $request.parameters

if ($request.command -eq 'analyze-wiki-css') {
    $url = 'https://en.wikipedia.org/w/load.php?lang=en&modules=ext.uls.interlanguage%7Cext.visualEditor.desktopArticleTarget.noscript%7Cext.wikimediamessages.styles%7Cskins.vector.icons,styles%7Cskins.vector.search.codex.styles%7Cwikibase.client.init&only=styles&skin=vector-2022'
    if ($request.dryRun) {
        @{ schema = 1; data = @{ status = 'planned'; url = $url }; files = @() } |
            ConvertTo-Json -Depth 10 -Compress | Write-Output
        exit 0
    }
    $css = (Invoke-WebRequest -Uri $url -UseBasicParsing).Content
    $variables = @{}
    $rootIndex = $css.IndexOf(':root')
    if ($rootIndex -ge 0) {
        $braceStart = $css.IndexOf('{', $rootIndex)
        $depth = 0
        $braceEnd = -1
        for ($index = $braceStart; $index -lt $css.Length; $index++) {
            if ($css[$index] -eq '{') { $depth++ }
            elseif ($css[$index] -eq '}') {
                $depth--
                if ($depth -eq 0) { $braceEnd = $index; break }
            }
        }
        if ($braceEnd -gt $braceStart) {
            $body = $css.Substring($braceStart + 1, $braceEnd - $braceStart - 1)
            foreach ($declaration in ($body -split ';')) {
                $pair = $declaration.Split(':', 2)
                if ($pair.Count -eq 2 -and $pair[0].Trim().StartsWith('--')) {
                    $variables[$pair[0].Trim()] = $pair[1].Trim()
                }
            }
        }
    }
    @{ schema = 1; data = @{ status = 'complete'; url = $url; bytes = $css.Length
            variableCount = $variables.Count; variables = $variables }; files = @() } |
        ConvertTo-Json -Depth 10 -Compress | Write-Output
    exit 0
}

# layout
if (-not $p.file) { throw 'layout requires --file <html-file>.' }
$config = if ($p.config) { [string]$p.config } else { 'release' }
$suffix = if ($config -eq 'debug') { 'd' } else { '' }
$ext = if ($IsWindows) { '.exe' } else { '' }
$app = Join-Path $root "Exe/potato-64$suffix$ext"

$file = [string]$p.file
$filePath = [IO.Path]::GetFullPath((Join-Path $root $file))
if (-not $filePath.StartsWith([IO.Path]::GetFullPath($root), $(if ($IsWindows) { 'OrdinalIgnoreCase' } else { 'Ordinal' }))) {
    throw 'Layout input must live inside the project.'
}

$arguments = @("--layout:$file")
if ($p.width) { $arguments += "--width:$([int]$p.width)" }
if ($p.dump) { $arguments += "--dump:$([int]$p.dump)" }
if ($p.trace) { $arguments += '-v' }

if ($request.dryRun) {
    @{ schema = 1; data = @{ status = 'planned'; binary = $app; arguments = $arguments }; files = @() } |
        ConvertTo-Json -Depth 10 -Compress | Write-Output
    exit 0
}

if (-not (Test-Path -LiteralPath $app)) { throw "Application not built: $app. Run dd build $config first." }
if (-not (Test-Path -LiteralPath $filePath)) { throw "Layout input not found: $file" }

$output = & $app @arguments 2>&1 | Out-String
$code = $LASTEXITCODE
@{ schema = 1; data = @{ status = 'complete'; binary = $app; arguments = $arguments
        exitCode = $code; output = $output }; files = @() } |
    ConvertTo-Json -Depth 10 -Compress | Write-Output
exit $code
