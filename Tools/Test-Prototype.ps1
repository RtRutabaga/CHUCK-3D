param([string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.7')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$projectFile = Join-Path $projectRoot 'Unreal\Chuck3D\Chuck3D.uproject'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$testLog = Join-Path $projectRoot 'Unreal\Chuck3D\Saved\Logs\ChuckSmoke.log'
& $editor $projectFile /Game/Prototype/WaterdeepDocks -game -ChuckSmokeTest -nullrhi -unattended -nosplash "-abslog=$testLog"
if ($LASTEXITCODE -ne 0) { throw "Runtime smoke test failed; inspect $testLog" }
if (-not (Select-String -LiteralPath $testLog -Pattern 'CHUCK_TEST_COMPLETE failures=0' -Quiet)) { throw 'Runtime test did not report completion.' }
Select-String -LiteralPath $testLog -Pattern 'CHUCK_TEST' | ForEach-Object { $_.Line }
