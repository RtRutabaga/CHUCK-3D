param(
    [switch]$Review,
    [string]$EngineRoot='C:\Program Files\Epic Games\UE_5.7',
    [string]$Blender="$env:LOCALAPPDATA\Programs\CHUCK-Tools\blender-4.5.14-windows-x64\blender.exe"
)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$project=Join-Path $projectRoot 'Unreal\Chuck3D\Chuck3D.uproject'
$editor=Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if(!(Test-Path -LiteralPath $editor) -or !(Test-Path -LiteralPath $Blender)) {
    throw 'Installed Unreal 5.7.4 and Blender 4.5.14 are required. No installation was attempted.'
}
function Assert-Idle {
    if(Get-Process -Name UnrealEditor,UnrealEditor-Cmd,blender -ErrorAction SilentlyContinue) {
        throw 'Another heavy tool is running. Finish it before continuing.'
    }
}
function Check-Step([string]$Log,[string]$Marker,[int]$Code) {
    if($Code -or !(Select-String -LiteralPath $Log -Pattern $Marker -Quiet) -or
       (Select-String -LiteralPath $Log -Pattern 'Traceback|LogPython: Error|Ensure condition failed|Fatal error:|Failed to compile Material|valid ShaderMap' -Quiet)) {
        throw "Groom step failed: $Log"
    }
    Write-Output "CHUCK_GROOM_STEP_VERIFIED $Marker $Log"
}
Assert-Idle
$stamp=Get-Date -Format 'yyyyMMdd-HHmmss'
$evidence=Join-Path $projectRoot 'Local'
New-Item -ItemType Directory -Path $evidence -Force | Out-Null
$buildLog=Join-Path $evidence "groom-build-$stamp.log"
& (Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat') Chuck3DEditor Win64 Development "-Project=$project" -WaitMutex -MaxParallelActions=1 -NoUBA *> $buildLog
Check-Step $buildLog 'Result: Succeeded|Target is up to date' $LASTEXITCODE
& (Join-Path $PSScriptRoot 'Import-ChuckV1.ps1') -EngineRoot $EngineRoot
Assert-Idle
$groupLog=Join-Path $evidence "groom-groups-$stamp.log"
& $Blender --background --python (Join-Path $PSScriptRoot 'prepare_groom_groups.py') *> $groupLog
Check-Step $groupLog 'CHUCK_GROOM_GROUPS_READY' $LASTEXITCODE
Assert-Idle
$importLog=Join-Path $evidence "groom-import-$stamp.log"
# Binding checks need completed render data; do not use NullRHI here.
& $editor $project "-ExecutePythonScript=$(Join-Path $PSScriptRoot 'import_chuck_groom.py')" -unattended -nosplash "-abslog=$importLog" *> (Join-Path $evidence "groom-console-$stamp.log")
Check-Step $importLog 'CHUCK_GROOM_IMPORTED' $LASTEXITCODE
Assert-Idle
$rootLog=Join-Path $evidence "groom-roots-$stamp.log"
& $Blender --background --python (Join-Path $PSScriptRoot 'validate_groom_roots.py') *> $rootLog
Check-Step $rootLog 'CHUCK_GROOM_ROOTS_VALIDATED' $LASTEXITCODE
if($Review) {
    Assert-Idle
    $reviewLog=Join-Path $evidence "groom-review-$stamp.log"
    & $editor $project "-ExecutePythonScript=$(Join-Path $PSScriptRoot 'review_chuck_v1_unreal.py')" -ChuckGroomReview -unattended -nosplash "-abslog=$reviewLog" *> (Join-Path $evidence "groom-review-console-$stamp.log")
    Check-Step $reviewLog 'CHUCK_V1_REVIEW_READY' $LASTEXITCODE
}
