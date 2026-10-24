# Import the beholder into /Game/Characters/Beholder (Tools/import_beholder.py; build it first with
# Tools/build_beholder.py in Blender 4.5.14).
param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.7'
)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
if (Get-Process UnrealEditor*,blender -ErrorAction SilentlyContinue) { throw 'Another Unreal or Blender process is running (one at a time on this machine).' }
$project=Join-Path $projectRoot 'Unreal\Chuck3D\Chuck3D.uproject'
$log=Join-Path $projectRoot 'Local\beholder-import.log'
& (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe') $project "-ExecutePythonScript=$(Join-Path $PSScriptRoot 'import_beholder.py')" -unattended -nosplash -NoLiveCoding "-abslog=$log" | Out-Null
if (!(Select-String -LiteralPath $log -Pattern 'CHUCK_BEHOLDER_IMPORT_READY' -Quiet) -or (Select-String -LiteralPath $log -Pattern 'Traceback|LogPython: Error' -Quiet)) { throw "Beholder import failed; inspect $log" }
Select-String -LiteralPath $log -Pattern 'CHUCK_BEHOLDER' | ForEach-Object { $_.Line.Substring($_.Line.IndexOf('CHUCK')) }
