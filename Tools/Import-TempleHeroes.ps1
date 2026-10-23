# Import the temple heroes and their clips into /Game/Characters/Heroes (Tools/import_temple_heroes.py).
# -Clean deletes the generated folder on disk first (needed after a master material or skeleton change,
# before the editor starts). -Only Fighter,Wizard re-imports just those heroes.
param(
    [switch]$Clean,
    [string[]]$Only = @(),
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.7'
)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
if (Get-Process UnrealEditor*,blender -ErrorAction SilentlyContinue) { throw 'Another Unreal or Blender process is running (one at a time on this machine).' }
$content=Join-Path $projectRoot 'Unreal\Chuck3D\Content\Characters\Heroes'
if ($Clean -and $Only) { throw '-Clean rebuilds everything; it cannot be combined with -Only.' }
if ($Clean -and (Test-Path -LiteralPath $content)) { Remove-Item -LiteralPath $content -Recurse -Force }
$project=Join-Path $projectRoot 'Unreal\Chuck3D\Chuck3D.uproject'
$log=Join-Path $projectRoot 'Local\heroes-import.log'
$env:CHUCK_HEROES_ONLY=($Only -join ',')
& (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe') $project "-ExecutePythonScript=$(Join-Path $PSScriptRoot 'import_temple_heroes.py')" -unattended -nosplash -NoLiveCoding "-abslog=$log" | Out-Null
if (!(Select-String -LiteralPath $log -Pattern 'CHUCK_HEROES_IMPORT_READY' -Quiet) -or (Select-String -LiteralPath $log -Pattern 'Traceback|LogPython: Error' -Quiet)) { throw "Hero import failed; inspect $log" }
Select-String -LiteralPath $log -Pattern 'CHUCK_HERO' | ForEach-Object { $_.Line.Substring($_.Line.IndexOf('CHUCK')) }
