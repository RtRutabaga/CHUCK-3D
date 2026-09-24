param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.7',
    [switch]$Package
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$projectFile = Join-Path $projectRoot 'Unreal\Chuck3D\Chuck3D.uproject'
$buildTool = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if (-not (Test-Path -LiteralPath $buildTool)) { throw "Unreal 5.7 is not installed at $EngineRoot. Pass -EngineRoot if installed elsewhere." }
& $buildTool Chuck3DEditor Win64 Development "-Project=$projectFile" -WaitMutex -MaxParallelActions=2
if ($LASTEXITCODE -ne 0) { throw 'Editor module build failed.' }
$assetScript = Join-Path $PSScriptRoot 'create_prototype_assets.py'
& $editor $projectFile -run=pythonscript "-script=$assetScript" -unattended -nullrhi -nosplash
if ($LASTEXITCODE -ne 0) { throw 'Prototype asset generation failed.' }
if (-not (Test-Path (Join-Path $projectRoot 'Unreal\Chuck3D\Content\Prototype\WaterdeepDocks.umap'))) { throw 'Expected prototype map was not generated.' }
if ($Package) {
    $automationTool = Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat'
    $output = Join-Path $projectRoot 'Builds'
    & $automationTool BuildCookRun "-project=$projectFile" -noP4 -platform=Win64 -clientconfig=Development -build -cook -stage -pak -archive "-archivedirectory=$output" -unattended -utf8output
    if ($LASTEXITCODE -ne 0) { throw 'Windows packaging failed.' }
}
