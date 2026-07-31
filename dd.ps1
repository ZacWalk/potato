# Potato developer commands.
param(
    [Parameter(Position = 0)]
    [ValidateSet('run', 'clean', 'analyze-wiki-css')]
    [string] $Command = 'run'
)

$ErrorActionPreference = 'Stop'

function Invoke-Build {
    $vswhere = Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Microsoft Visual Studio\Installer\vswhere.exe'
    $msbuild = & $vswhere -latest -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' |
        Select-Object -First 1

    if (-not $msbuild) {
        throw 'MSBuild was not found by vswhere.'
    }

    & $msbuild (Join-Path $PSScriptRoot 'potato.sln') /p:Configuration=Release /p:Platform=x64 /m
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

function Invoke-Clean {
    $directories = Get-ChildItem -Path $PSScriptRoot -Directory -Recurse -Force |
        Where-Object Name -In @('Debug', 'Release', 'bin', 'obj') |
        Sort-Object { $_.FullName.Length } -Descending

    foreach ($directory in $directories) {
        Remove-Item -LiteralPath $directory.FullName -Recurse -Force
    }

    Get-ChildItem -Path $PSScriptRoot -Filter '*.sdf' -File -Force |
        Remove-Item -Force
}

function Invoke-WikiCssAnalysis {
    $url = 'https://en.wikipedia.org/w/load.php?lang=en&modules=ext.uls.interlanguage%7Cext.visualEditor.desktopArticleTarget.noscript%7Cext.wikimediamessages.styles%7Cskins.vector.icons,styles%7Cskins.vector.search.codex.styles%7Cwikibase.client.init&only=styles&skin=vector-2022'
    $css = (Invoke-WebRequest -Uri $url -UseBasicParsing).Content
    $rootIndex = $css.IndexOf(':root')

    if ($rootIndex -ge 0) {
        $braceStart = $css.IndexOf('{', $rootIndex)
        $depth = 0
        $braceEnd = -1

        for ($index = $braceStart; $index -lt $css.Length; $index++) {
            if ($css[$index] -eq '{') {
                $depth++
            }
            elseif ($css[$index] -eq '}') {
                $depth--
                if ($depth -eq 0) {
                    $braceEnd = $index
                    break
                }
            }
        }

        $rootBlock = $css.Substring($rootIndex, $braceEnd - $rootIndex + 1)
        Write-Host '=== :root block (first 2000 chars) ==='
        Write-Host $rootBlock.Substring(0, [Math]::Min($rootBlock.Length, 2000))
        Write-Host ''
        Write-Host "=== :root block length: $($rootBlock.Length) chars ==="
    }

    Write-Host ''
    Write-Host '=== First 5 @supports blocks (first 300 chars each) ==='
    $position = 0
    $count = 0

    while ($count -lt 5 -and $position -lt $css.Length) {
        $supportsIndex = $css.IndexOf('@supports', $position)
        if ($supportsIndex -lt 0) {
            break
        }

        $braceStart = $css.IndexOf('{', $supportsIndex)
        $depth = 0
        $braceEnd = -1

        for ($index = $braceStart; $index -lt $css.Length; $index++) {
            if ($css[$index] -eq '{') {
                $depth++
            }
            elseif ($css[$index] -eq '}') {
                $depth--
                if ($depth -eq 0) {
                    $braceEnd = $index
                    break
                }
            }
        }

        $block = $css.Substring($supportsIndex, $braceEnd - $supportsIndex + 1)
        Write-Host ''
        Write-Host "--- @supports #$($count + 1) ($($block.Length) chars) ---"
        Write-Host $block.Substring(0, [Math]::Min($block.Length, 300))
        $position = $braceEnd + 1
        $count++
    }

    Write-Host ''
    Write-Host '=== var() with fallback vs without ==='
    $withFallback = [regex]::Matches($css, 'var\(--[^,)]+,\s*[^)]+\)').Count
    $withoutFallback = [regex]::Matches($css, 'var\(--[^,)]+\)').Count
    Write-Host "var() with fallback: $withFallback"
    Write-Host "var() without fallback (simple): $withoutFallback"
}

switch ($Command) {
    'run' { Invoke-Build }
    'clean' { Invoke-Clean }
    'analyze-wiki-css' { Invoke-WikiCssAnalysis }
}