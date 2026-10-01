# Import the human NPCs and their motion-capture clips into /Game/Characters/Humans
# (Tools/import_npc_humans.py). -Clean deletes the generated folder on disk first:
# needed when the shared skeleton or a master material changes, and it has to
# happen before the editor starts, because the game module hard-references the
# humans (ADockNPC) and keeps them loaded.
param(
    [switch]$Clean,
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.7'
)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
if (Get-Process UnrealEditor*,blender -ErrorAction SilentlyContinue) { throw 'Another Unreal or Blender process is running (one at a time on this machine).' }
$content=Join-Path $projectRoot 'Unreal\Chuck3D\Content\Characters\Humans'
if ($Clean -and (Test-Path -LiteralPath $content)) { Remove-Item -LiteralPath $content -Recurse -Force }
$project=Join-Path $projectRoot 'Unreal\Chuck3D\Chuck3D.uproject'
$log=Join-Path $projectRoot 'Local\humans-import.log'
& (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe') $project "-ExecutePythonScript=$(Join-Path $PSScriptRoot 'import_npc_humans.py')" -unattended -nosplash -NoLiveCoding "-abslog=$log" | Out-Null
if (!(Select-String -LiteralPath $log -Pattern 'CHUCK_HUMANS_IMPORT_READY' -Quiet) -or (Select-String -LiteralPath $log -Pattern 'Traceback' -Quiet)) { throw "Human import failed; inspect $log" }
Select-String -LiteralPath $log -Pattern 'CHUCK_HUMAN' | ForEach-Object { $_.Line.Substring($_.Line.IndexOf('CHUCK')) }
