param(
    [switch]$Review,
    # -Clips SprintLoop,...: import only these clips against the existing mesh/skeleton.
    [string[]]$Clips,
    [string]$EngineRoot='C:\Program Files\Epic Games\UE_5.7'
)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$editor=Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$project=Join-Path $projectRoot 'Unreal\Chuck3D\Chuck3D.uproject'
if(!(Test-Path -LiteralPath $editor)) { throw 'Unreal 5.7.4 is required; no installation was attempted.' }
if(Get-Process -Name UnrealEditor,UnrealEditor-Cmd,blender -ErrorAction SilentlyContinue) {
    throw 'Another heavy editor/import is running. Finish it before importing on this 16 GB machine.'
}
$evidence=Join-Path $projectRoot 'Local'
New-Item -ItemType Directory -Force -Path $evidence | Out-Null
function Invoke-V1Script([string]$Script,[string]$Marker,[bool]$Render) {
    $stamp=Get-Date -Format 'yyyyMMdd-HHmmss'
    $log=Join-Path $evidence "$Script-$stamp.log"
    $path=Join-Path $PSScriptRoot $Script
    $arguments=@($project,"-ExecutePythonScript=$path",'-unattended','-nosplash','-NoLiveCoding',"-abslog=$log")
    if(!$Render) { $arguments+='-nullrhi' }
    & $editor @arguments
    $code=$LASTEXITCODE
    if($code -or !(Test-Path -LiteralPath $log) -or
        !(Select-String -LiteralPath $log -Pattern $Marker -Quiet) -or
        (Select-String -LiteralPath $log -Pattern 'LogPython: Error|Failed to compile Material|valid ShaderMap|Fatal error:' -Quiet)) {
        throw "V1 step failed; inspect $log"
    }
    Write-Output "V1_STEP_VERIFIED $Script log=$log"
}
# Uses committed FBXs/maps; deliberately does not run either Blender generator.
if($Review) {
    # Review refresh is an editor-only C++ helper; build it before Python loads.
    $buildTool=Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
    & $buildTool Chuck3DEditor Win64 Development "-Project=$project" -WaitMutex -MaxParallelActions=1 -NoUBA
    if($LASTEXITCODE) { throw 'Editor review module build failed.' }
}
$env:CHUCK_V1_CLIPS_ONLY=($Clips -join ',')
try { Invoke-V1Script 'import_chuck_v1.py' 'CHUCK_V1_IMPORT_READY' $false }
finally { Remove-Item Env:CHUCK_V1_CLIPS_ONLY -ErrorAction SilentlyContinue }
if(!$Clips) { Invoke-V1Script 'import_chuck_cigarette.py' 'CHUCK_CIGARETTE_IMPORTED' $false }
if($Review) { Invoke-V1Script 'review_chuck_v1_unreal.py' 'CHUCK_V1_REVIEW_READY' $true }
