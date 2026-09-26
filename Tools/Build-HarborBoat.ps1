param(
    [string]$Blender = "$env:LOCALAPPDATA\Programs\CHUCK-Tools\blender-4.5.14-windows-x64\blender.exe",
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.7'
)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
& $Blender --background --python (Join-Path $PSScriptRoot 'build_harbor_boat.py')
if ($LASTEXITCODE) { throw 'Harbor boat generation failed.' }
$project=Join-Path $projectRoot 'Unreal\Chuck3D\Chuck3D.uproject'
$log=Join-Path $projectRoot 'Unreal\Chuck3D\Saved\Logs\HarborBoatImport.log'
$script=Join-Path $PSScriptRoot 'import_harbor_boat.py'
& (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe') $project "-ExecutePythonScript=$script" -unattended -nullrhi -nosplash -NoLiveCoding "-abslog=$log"
if ($LASTEXITCODE -or !(Select-String -LiteralPath $log -Pattern 'CHUCK_HARBOR_IMPORT_READY' -Quiet)) {
    throw "Harbor boat import failed; inspect $log"
}
