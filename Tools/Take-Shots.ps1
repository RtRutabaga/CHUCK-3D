# Takes the stills in a shot list (Unreal/Chuck3D/Source/Chuck3D/ChuckShots.h has the format) in the uncooked
# editor game, with no recompile: Tools/Take-Shots.ps1 Tools/shots/tavern-bin.txt [-ResX 1280 -ResY 720]
# The stills are copied to Local/shots/<list name>/ and the log is Local/shots/<list name>.log.
param(
    [Parameter(Mandatory)][string]$ShotList,
    [int]$ResX = 1280,
    [int]$ResY = 720,
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.7'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if (Get-Process UnrealEditor*, blender -ErrorAction SilentlyContinue) { throw 'Another Unreal or Blender process is running (one at a time on this machine).' }
$list = (Resolve-Path -LiteralPath $ShotList).Path
$name = [IO.Path]::GetFileNameWithoutExtension($list)
$out = Join-Path $root "Local\shots\$name"
$log = Join-Path $root "Local\shots\$name.log"
New-Item -ItemType Directory -Force (Split-Path $out) | Out-Null
$project = Join-Path $root 'Unreal\Chuck3D\Chuck3D.uproject'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor.exe'
& $editor $project -game -windowed "-ResX=$ResX" "-ResY=$ResY" -NoLiveCoding "-ChuckShots=$list" "-abslog=$log" | Out-Null
$done = Select-String -LiteralPath $log -Pattern 'CHUCK_SHOTS_COMPLETE' | Select-Object -Last 1
if (!$done) { throw "The shot run did not finish; inspect $log" }
$from = Join-Path $root "Unreal\Chuck3D\Saved\Screenshots\WindowsEditor\Shots\$name"
if (Test-Path -LiteralPath $out) { Remove-Item -LiteralPath $out -Recurse -Force }
Copy-Item -LiteralPath $from -Destination $out -Recurse
Select-String -LiteralPath $log -Pattern 'CHUCK_SHOT' | ForEach-Object { $_.Line.Substring($_.Line.IndexOf('CHUCK')) }
"Stills: $out"
