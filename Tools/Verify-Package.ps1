param([switch]$NoCapture)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$game=Join-Path $projectRoot 'Builds\Windows\Chuck3D\Binaries\Win64\Chuck3D.exe'
if (!(Test-Path -LiteralPath $game)) { throw 'No packaged game. Build Windows first; see docs/PLAYTEST.md.' }
$evidence=Join-Path $projectRoot 'Local'
New-Item -ItemType Directory -Force -Path $evidence | Out-Null
$log=Join-Path $evidence ('verify-package-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'.log')
$arguments=@('-ChuckSmokeTest','-windowed','-ResX=1280','-ResY=720',('-abslog="'+$log+'"'))
if (!$NoCapture) { $arguments+='-ChuckCapture' }
$process=Start-Process -FilePath $game -ArgumentList $arguments -WindowStyle Normal -Wait -PassThru
if (!(Test-Path -LiteralPath $log)) { throw "Game did not produce its test log: $log" }
$failed=Select-String -LiteralPath $log -Pattern 'CHUCK_TEST FAIL|Failed to compile Material|valid ShaderMap|Fatal error:'
$completed=Select-String -LiteralPath $log -Pattern 'CHUCK_TEST_COMPLETE failures=0'
$passed=@(Select-String -LiteralPath $log -Pattern 'CHUCK_TEST PASS:').Count
$expected=if($NoCapture) {39} else {40}
if ($process.ExitCode -or $failed -or !$completed -or $passed -lt $expected) {
    if($failed) { $failed | ForEach-Object {$_.Line} }
    throw "Package verification failed (exit $($process.ExitCode), $passed passes; expected at least $expected). Inspect $log"
}
Write-Output "CHUCK_PACKAGE_VERIFIED passes=$passed log=$log"
if (!$NoCapture) { Write-Output ('Captures: '+(Join-Path $projectRoot 'Builds\Windows\Chuck3D\Saved\Screenshots\Windows')) }
