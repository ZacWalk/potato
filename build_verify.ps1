Get-Process potato-64,potato-64d -EA SilentlyContinue | Stop-Process -Force
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$msbuild = & $vswhere -latest -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe' | Select-Object -First 1
& $msbuild potato.sln /p:Configuration=Debug /p:Platform=x64 /m /nologo /v:minimal 2>&1 | Select-String ': error|: warning' | ForEach-Object { $_.Line } | Select-Object -Unique -First 25
Write-Output "Final Exit Code: $LASTEXITCODE"
