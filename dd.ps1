# Potato developer commands.
param(
    [Parameter(Position = 0)]
    [ValidateSet('run', 'build', 'test', 'layout', 'clean', 'analyze-wiki-css')]
    [string] $Command = 'run',

    [ValidateSet('Debug', 'Release')]
    [string] $Config = 'Release',

    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]] $Rest
)

$ErrorActionPreference = 'Stop'

function Get-VisualStudioPath {
    $vswhere = Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) {
        throw 'vswhere.exe was not found; install Visual Studio with the C++ desktop workload.'
    }

    $path = & $vswhere -latest -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath |
        Select-Object -First 1

    if (-not $path) {
        throw 'No Visual Studio installation with the C++ desktop workload was found.'
    }

    return $path
}

# cl and rc only work inside the MSVC environment, and Ninja invokes cl directly.
function Enter-MsvcEnvironment {
    param([string] $VisualStudio)

    if ($env:VSCMD_ARG_TGT_ARCH -eq 'x64') {
        return
    }

    $vcvars = Join-Path $VisualStudio 'VC\Auxiliary\Build\vcvars64.bat'
    if (-not (Test-Path $vcvars)) {
        throw "vcvars64.bat was not found at $vcvars."
    }

    & cmd.exe /c "`"$vcvars`" >nul 2>&1 && set" | ForEach-Object {
        if ($_ -match '^([^=]+)=(.*)$') {
            Set-Item -LiteralPath "Env:$($Matches[1])" -Value $Matches[2]
        }
    }
}

# Prefer whatever is on PATH, then the copy Visual Studio ships, so neither tool
# has to be installed separately.
function Resolve-Tool {
    param([string] $Name, [string] $VisualStudio, [string] $BundledRelativePath)

    $onPath = Get-Command $Name -ErrorAction SilentlyContinue
    if ($onPath) {
        return $onPath.Source
    }

    $bundled = Join-Path $VisualStudio $BundledRelativePath
    if (Test-Path $bundled) {
        return $bundled
    }

    throw "$Name was not found on PATH or under $VisualStudio."
}

function Get-ExePath {
    $name = if ($Config -eq 'Debug') { 'potato-64d.exe' } else { 'potato-64.exe' }
    return Join-Path $PSScriptRoot "Exe\$name"
}

function Stop-RunningInstances {
    # The linker fails with LNK1168 if the previous build is still running.
    Get-Process potato-64, potato-64d -ErrorAction SilentlyContinue | Stop-Process -Force
}

function Invoke-Build {
    $vs = Get-VisualStudioPath
    Enter-MsvcEnvironment -VisualStudio $vs

    $cmake = Resolve-Tool -Name 'cmake' -VisualStudio $vs `
        -BundledRelativePath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
    $ninja = Resolve-Tool -Name 'ninja' -VisualStudio $vs `
        -BundledRelativePath 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'

    $env:PATH = "$(Split-Path $ninja);$env:PATH"

    $preset = $Config.ToLowerInvariant()

    Stop-RunningInstances

    Push-Location $PSScriptRoot
    try {
        & $cmake --preset $preset
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

        & $cmake --build --preset $preset
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
    finally {
        Pop-Location
    }
}

function Invoke-Run {
    Invoke-Build

    $exe = Get-ExePath
    Write-Host "Launching $exe"
    # The working directory must be the repo root: test-files/ is resolved
    # relative to it.
    Start-Process -FilePath $exe -WorkingDirectory $PSScriptRoot -ArgumentList $Rest
}

function Invoke-Exe {
    param([string[]] $Arguments)

    Invoke-Build

    Push-Location $PSScriptRoot
    try {
        & (Get-ExePath) @Arguments | Out-String | Write-Host
        exit $LASTEXITCODE
    }
    finally {
        Pop-Location
    }
}

function Invoke-Layout {
    if (-not $Rest) {
        throw 'Usage: dd layout <html-file> [--width:N] [--dump:N] [-v]'
    }

    Invoke-Exe -Arguments (@("--layout:$($Rest[0])") + @($Rest | Select-Object -Skip 1))
}

function Invoke-Clean {
    Stop-RunningInstances

    foreach ($path in @('build', 'Exe')) {
        $full = Join-Path $PSScriptRoot $path
        if (Test-Path $full) {
            Remove-Item -LiteralPath $full -Recurse -Force
        }
    }
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
    'run' { Invoke-Run }
    'build' { Invoke-Build }
    'test' { Invoke-Exe -Arguments @('--test') }
    'layout' { Invoke-Layout }
    'clean' { Invoke-Clean }
    'analyze-wiki-css' { Invoke-WikiCssAnalysis }
}
