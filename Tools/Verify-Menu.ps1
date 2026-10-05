param([string]$PackageRoot)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
if(!$PackageRoot) { $PackageRoot=Join-Path $projectRoot 'Builds\Windows' }
$game=Join-Path (Resolve-Path -LiteralPath $PackageRoot).Path 'Chuck3D\Binaries\Win64\Chuck3D.exe'
New-Item -ItemType Directory -Force -Path (Join-Path $projectRoot 'Local') | Out-Null
$run=Get-Date -Format 'yyyyMMdd-HHmmss'
foreach($point in @('NewGame','Waterdeep','Sewer','SewerJump','Night','Pantry')) {
    $log=Join-Path $projectRoot "Local\verify-menu-$run-$point.log"
    $arguments=@("-MenuTest=$point",'-windowed','-ResX=1280','-ResY=720',('-abslog="'+$log+'"'))
    $process=Start-Process -FilePath $game -ArgumentList $arguments -WindowStyle Hidden -PassThru
    if(!$process.WaitForExit(45000)) {
        Stop-Process -Id $process.Id
        throw "Menu test timed out: $point ($log)"
    }
    if($process.ExitCode -ne 0 -or !(Test-Path -LiteralPath $log) -or
       !(Select-String -LiteralPath $log -Pattern "CHUCK_MENU_TEST_COMPLETE failures=0 point=$point" -Quiet) -or
       (Select-String -LiteralPath $log -Pattern 'CHUCK_MENU_TEST FAIL|Fatal error:|EXCEPTION_ACCESS_VIOLATION' -Quiet)) {
        throw "Menu test failed: $point ($log)"
    }
    # The only map load must be startup: button activation must use the paused world.
    $loads=@(Select-String -LiteralPath $log -Pattern 'LogLoad: LoadMap:').Count
    if($loads -ne 1) { throw "Menu selection reloaded the world: $point ($loads loads)" }
    Write-Output "CHUCK_MENU_VERIFIED point=$point log=$log"
}
Write-Output 'CHUCK_MENU_SUITE_COMPLETE passed=6'
