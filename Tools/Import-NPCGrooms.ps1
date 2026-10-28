# Import the humans' or heroes' strand grooms (Tools/import_npc_grooms.py) after their meshes:
#   Tools/Import-NPCGrooms.ps1 [-Set humans|heroes] [-Only TavernKeeper,Dwarf] [-RebuildMaster]
# -RebuildMaster deletes M_NPCHair before the editor starts, so the import makes it again
# (a loaded master's graph cannot be cleared from Python in UE 5.7: !IsRooted).
param(
    [ValidateSet('humans', 'heroes')][string]$Set = 'humans',
    [string[]]$Only = @(),
    [switch]$RebuildMaster,
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.7'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
if (Get-Process UnrealEditor*, blender -ErrorAction SilentlyContinue) { throw 'Another Unreal or Blender process is running (one at a time on this machine).' }
$project = Join-Path $projectRoot 'Unreal\Chuck3D\Chuck3D.uproject'
if ($RebuildMaster) {
    $file = Join-Path $projectRoot 'Unreal\Chuck3D\Content\Characters\Humans\M_NPCHair.uasset'
    if (Test-Path -LiteralPath $file) { Remove-Item -LiteralPath $file -Force }
}
$log = Join-Path $projectRoot "Local\grooms-import-$Set.log"
$env:CHUCK_GROOMS_SET = $Set
$env:CHUCK_GROOMS_ONLY = ($Only -join ',')
# Not NullRHI: the groom's render data is built on import.
& (Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe') $project "-ExecutePythonScript=$(Join-Path $PSScriptRoot 'import_npc_grooms.py')" -unattended -nosplash -NoLiveCoding "-abslog=$log" | Out-Null
if (!(Select-String -LiteralPath $log -Pattern 'CHUCK_GROOMS_IMPORT_READY' -Quiet) -or (Select-String -LiteralPath $log -Pattern 'Traceback|LogPython: Error' -Quiet)) { throw "Groom import failed; inspect $log" }
Select-String -LiteralPath $log -Pattern 'CHUCK_GROOM' | ForEach-Object { $_.Line.Substring($_.Line.IndexOf('CHUCK')) }
