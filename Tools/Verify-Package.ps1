param([switch]$NoCapture,[switch]$MotionCapture,[switch]$NoGroom,[string]$PackageRoot)
$ErrorActionPreference='Stop'
if($NoCapture -and $MotionCapture) { throw 'MotionCapture requires captures; omit NoCapture.' }
$projectRoot=Split-Path $PSScriptRoot -Parent
if(!$PackageRoot) { $PackageRoot=Join-Path $projectRoot 'Builds\Windows' }
$PackageRoot=(Resolve-Path -LiteralPath $PackageRoot).Path
$game=Join-Path $PackageRoot 'Chuck3D\Binaries\Win64\Chuck3D.exe'
if (!(Test-Path -LiteralPath $game)) { throw 'No packaged game. Build Windows first; see docs/PLAYTEST.md.' }
$evidence=Join-Path $projectRoot 'Local'
New-Item -ItemType Directory -Force -Path $evidence | Out-Null
$log=Join-Path $evidence ('verify-package-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'.log')
$arguments=@('-ChuckSmokeTest','-windowed','-ResX=1280','-ResY=720',('-abslog="'+$log+'"'))
if (!$NoCapture) { $arguments+='-ChuckCapture' }
if ($NoGroom) { $arguments+='-ChuckNoGroom' }
if ($MotionCapture) {
    $arguments+='-ChuckMotionCapture'
    # Capture speed can change the final frame count. Remove only old generated
    # frame PNGs so a shorter run cannot leave stale frames in the review.
    foreach($view in 0..3) {
        $frames=Join-Path $PackageRoot "Chuck3D\Saved\Screenshots\Windows\Motion\View$view"
        if(Test-Path -LiteralPath $frames) {
            Get-ChildItem -LiteralPath $frames -Filter 'frame*.png' -File | ForEach-Object { Remove-Item -LiteralPath $_.FullName }
        }
    }
}
$process=Start-Process -FilePath $game -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru
if (!(Test-Path -LiteralPath $log)) { throw "Game did not produce its test log: $log" }
$failed=Select-String -LiteralPath $log -Pattern 'CHUCK_TEST FAIL|Failed to compile Material|valid ShaderMap|Default Material will be used in game|Fatal error:'
$completed=Select-String -LiteralPath $log -Pattern 'CHUCK_TEST_COMPLETE failures=0'
$worldCompleted=Select-String -LiteralPath $log -Pattern 'CHUCK_WORLD_CHECK_COMPLETE failures=0 '
$streetCompleted=Select-String -LiteralPath $log -Pattern 'CHUCK_DOCKSTREET_CHECK failures=0 floors=10 routes=9 buildings=8 boundary=1'
$sideGateCompleted=Select-String -LiteralPath $log -Pattern 'CHUCK_SIDEGATE_CHECK failures=0 gate_closed=1 grate_ground=1 approach_clear=1'
$courtPierCompleted=Select-String -LiteralPath $log -Pattern 'CHUCK_COURTPIER_CHECK failures=0 floors=10 routes=9'
$dockPropsCompleted=Select-String -LiteralPath $log -Pattern 'CHUCK_DOCKPROPS_CHECK failures=0 barrels=5 planks=4 boundaries=2'
$plazaCompleted=Select-String -LiteralPath $log -Pattern 'CHUCK_PLAZA_CHECK failures=0 floor_samples=11 capsule_routes=8 sewer_closed=1'
$musicCompleted=Select-String -LiteralPath $log -Pattern 'CHUCK_MUSIC_CHECK failures=0 looping=1 playing_after_boundary=1'
$passed=@(Select-String -LiteralPath $log -Pattern 'CHUCK_TEST PASS:').Count
$expected=if($NoCapture) {124} else {125}
if ($process.ExitCode -or $failed -or !$completed -or !$worldCompleted -or !$streetCompleted -or !$sideGateCompleted -or !$courtPierCompleted -or !$dockPropsCompleted -or !$plazaCompleted -or !$musicCompleted -or $passed -lt $expected) {
    if($failed) { $failed | ForEach-Object {$_.Line} }
    throw "Package verification failed (exit $($process.ExitCode), $passed passes; expected at least $expected). Inspect $log"
}
Write-Output "CHUCK_PACKAGE_VERIFIED passes=$passed log=$log"
if (!$NoCapture) { Write-Output ('Captures: '+(Join-Path $PackageRoot 'Chuck3D\Saved\Screenshots\Windows')) }
$global:ChuckLastVerificationLog=$log
