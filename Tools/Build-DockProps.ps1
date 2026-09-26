param(
    [string]$Blender = "$env:LOCALAPPDATA\Programs\CHUCK-Tools\blender-4.5.14-windows-x64\blender.exe",
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.7'
)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
& $Blender --background --python (Join-Path $PSScriptRoot 'build_dock_props.py')
if ($LASTEXITCODE) { throw 'Dock prop generation failed.' }
$project=Join-Path $projectRoot 'Unreal\Chuck3D\Chuck3D.uproject'
$log=Join-Path $projectRoot 'Unreal\Chuck3D\Saved\Logs\DockPropImport.log'
$script=Join-Path $PSScriptRoot 'import_dock_props.py'
& (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe') $project "-ExecutePythonScript=$script" -unattended -nullrhi -nosplash -NoLiveCoding "-abslog=$log"
if ($LASTEXITCODE -or !(Select-String -LiteralPath $log -Pattern 'CHUCK_DOCK_IMPORT_READY' -Quiet)) {
    throw "Dock prop import failed; inspect $log"
}
