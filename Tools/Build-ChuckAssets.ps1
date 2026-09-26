param(
    [string]$Blender = "$env:LOCALAPPDATA\Programs\CHUCK-Tools\blender-4.5.14-windows-x64\blender.exe",
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.7'
)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
if (!(Test-Path -LiteralPath $Blender)) { throw 'Install Blender 4.5.14 LTS outside this repository or pass -Blender.' }
& $Blender --background --python (Join-Path $PSScriptRoot 'build_chuck_model.py')
if ($LASTEXITCODE) { throw 'Blender asset generation failed.' }
$editor=Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$project=Join-Path $projectRoot 'Unreal\Chuck3D\Chuck3D.uproject'
$script=Join-Path $PSScriptRoot 'import_chuck_model.py'
$log=Join-Path $projectRoot 'Unreal\Chuck3D\Saved\Logs\ChuckImport.log'
# Full editor scripting initializes the UI services needed by Interchange import.
# The commandlet-only Python mode asserts in UE 5.7.4's Interchange completion UI.
& $editor $project "-ExecutePythonScript=$script" -unattended -nullrhi -nosplash -NoLiveCoding "-abslog=$log"
if ($LASTEXITCODE -or !(Select-String -LiteralPath $log -Pattern 'CHUCK_CUSTOM_ASSETS_READY' -Quiet)) {
    throw "Character import failed; inspect $log"
}
$rigScript=Join-Path $PSScriptRoot 'import_chuck_rig.py'
$rigLog=Join-Path $projectRoot 'Unreal\Chuck3D\Saved\Logs\ChuckRigImport.log'
& $editor $project "-ExecutePythonScript=$rigScript" -unattended -nullrhi -nosplash -NoLiveCoding "-abslog=$rigLog"
if ($LASTEXITCODE -or !(Select-String -LiteralPath $rigLog -Pattern 'CHUCK_RIG_READY' -Quiet)) {
    throw "Character rig import failed; inspect $rigLog"
}
