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
$sideGateCompleted=Select-String -LiteralPath $log -Pattern 'CHUCK_SIDEGATE_CHECK failures=0 gate_closed=1 grate_open=1 approach_clear=1'
$courtPierCompleted=Select-String -LiteralPath $log -Pattern 'CHUCK_COURTPIER_CHECK failures=0 floors=10 routes=9'
$dockPropsCompleted=Select-String -LiteralPath $log -Pattern 'CHUCK_DOCKPROPS_CHECK failures=0 barrels=5 planks=4 boundaries=2'
$plazaCompleted=Select-String -LiteralPath $log -Pattern 'CHUCK_PLAZA_CHECK failures=0 floor_samples=14 capsule_routes=11 sewer_closed=1'
$harborWaterCompleted=Select-String -LiteralPath $log -Pattern 'CHUCK_HARBOR_WATER_CHECK failures=0 sheets=7 collision=0 open_shafts=2 single_surface_samples=6'
$surfaceWaterCompleted=Select-String -LiteralPath $log -Pattern 'CHUCK_SURFACE_WATER_CHECK failures=0 materials=3 jet_drops=24 splash_drops=48'
$musicCompleted=Select-String -LiteralPath $log -Pattern 'CHUCK_MUSIC_CHECK failures=0 looping=1 playing_after_boundary=1'
$chimneyCompleted=Select-String -LiteralPath $log -Pattern 'CHUCK_CHIMNEY_COLLISION failures=0 checked=[1-9]'
$hatchCompleted=Select-String -LiteralPath $log -Pattern 'CHUCK_OPENHATCH_CHECK failures=0 shaft=3 surrounds=3 raised_lid=1'
$sewerCompleted=Select-String -LiteralPath $log -Pattern 'CHUCK_SEWER_GEOMETRY failures=0 samples=[1-9]'
$astralCompleted=Select-String -LiteralPath $log -Pattern 'CHUCK_ASTRAL_HAZARDS failures=0 holes=32 purple_lights=32 torches=0 large=12 small=20'
$nightMusicCompleted=Select-String -LiteralPath $log -Pattern 'CHUCK_NIGHT_MUSIC_CHECK failures=0 looping=1 playing_after_boundary=1'
$smallStreamCompleted=Select-String -LiteralPath $log -Pattern 'CHUCK_SMALL_RIFT_STREAM failures=0 checked=20 clearance_cm=10'
$caveCompleted=Select-String -LiteralPath $log -Pattern 'CHUCK_CAVE_CHECK failures=0 wall_traces=[1-9]'
$tavernCompleted=Select-String -LiteralPath $log -Pattern 'CHUCK_TAVERN_CHECK failures=0 floors=5 routes=3 furniture=2 roof=1 doorway_initially_closed=1'
$pantryCompleted=Select-String -LiteralPath $log -Pattern 'CHUCK_PANTRY_CHECK failures=0 floors=5 routes=4 shaft=1 walls=4 open_holes=4'
$streamAudio=Select-String -LiteralPath $log -Pattern 'CHUCK_STREAM_AUDIO loaded=6'
$fireCompleted=Select-String -LiteralPath $log -Pattern 'CHUCK_FIRE_CHECK failures=0 flames=[1-9][0-9]* flicker_lights=[1-9][0-9]* legacy_primitives=0'
$narrowCompleted=Select-String -LiteralPath $log -Pattern 'CHUCK_SEWER_NARROW failures=0 samples=11 nominal_width_cm=140'
$wallRiftCompleted=Select-String -LiteralPath $log -Pattern 'CHUCK_WALLRIFT_GEOMETRY failures=0 after_chamber=1 floor_holes=6 length_cm=195 width_cm=140'
$rubbleCompleted=Select-String -LiteralPath $log -Pattern 'CHUCK_SLIDE_RUBBLE rocks=44 collision=1'
$forgeCompleted=Select-String -LiteralPath $log -Pattern 'CHUCK_FORGE_DRESSING failures=0 materials=4 solid_samples=4 clear_routes=3'
$smithCompleted=Select-String -LiteralPath $log -Pattern 'CHUCK_SMITH_SPAWNED anvil=1 hammer=1 tongs=1 sounds=12'
$keeperCompleted=Select-String -LiteralPath $log -Pattern 'CHUCK_KEEPER_SPAWNED tankard=1 rag=1'
$sailorCompleted=Select-String -LiteralPath $log -Pattern 'CHUCK_SAILOR_SPAWNED pipe=1 smoke=1 puff=1'
$dwarfVoice=Select-String -LiteralPath $log -Pattern 'CHUCK_NPC_VOICE Dwarf lines=1 sounds=1'
$guardVoice=Select-String -LiteralPath $log -Pattern 'CHUCK_NPC_VOICE GuardWoman lines=1 sounds=1'
$plazaGuardVoice=Select-String -LiteralPath $log -Pattern 'CHUCK_NPC_VOICE Guard lines=1 sounds=1'
$sideGuardVoice=Select-String -LiteralPath $log -Pattern 'CHUCK_NPC_VOICE SideGuard lines=1 sounds=1'
$sailorVoice=Select-String -LiteralPath $log -Pattern 'CHUCK_NPC_VOICE Sailor lines=1 sounds=1'
$workerVoice=Select-String -LiteralPath $log -Pattern 'CHUCK_NPC_VOICE Worker lines=1 sounds=1'
$elfVoice=Select-String -LiteralPath $log -Pattern 'CHUCK_NPC_VOICE ElfElder lines=1 sounds=1'
$marketVoice=Select-String -LiteralPath $log -Pattern 'CHUCK_NPC_VOICE MarketWoman lines=1 sounds=1'
$alchemistVoice=Select-String -LiteralPath $log -Pattern 'CHUCK_NPC_VOICE GnomeAlchemist lines=1 sounds=1'
$smithVoice=Select-String -LiteralPath $log -Pattern 'CHUCK_NPC_VOICE Blacksmith lines=1 sounds=1'
$bobertCompleted=Select-String -LiteralPath $log -Pattern 'CHUCK_BOBERT_SPAWNED body=1 barrel=1'
$passed=@(Select-String -LiteralPath $log -Pattern 'CHUCK_TEST PASS:').Count
$expected=if($NoCapture) {161} else {162}
if ($process.ExitCode -or $failed -or !$completed -or !$worldCompleted -or !$streetCompleted -or !$sideGateCompleted -or !$courtPierCompleted -or !$dockPropsCompleted -or !$plazaCompleted -or !$harborWaterCompleted -or !$surfaceWaterCompleted -or !$musicCompleted -or !$nightMusicCompleted -or !$chimneyCompleted -or !$hatchCompleted -or !$sewerCompleted -or !$astralCompleted -or !$smallStreamCompleted -or !$caveCompleted -or !$tavernCompleted -or !$pantryCompleted -or !$streamAudio -or !$fireCompleted -or !$narrowCompleted -or !$wallRiftCompleted -or !$rubbleCompleted -or !$forgeCompleted -or !$smithCompleted -or !$dwarfVoice -or !$guardVoice -or !$plazaGuardVoice -or !$sideGuardVoice -or !$sailorVoice -or !$workerVoice -or !$elfVoice -or !$marketVoice -or !$alchemistVoice -or !$smithVoice -or !$keeperCompleted -or !$sailorCompleted -or !$bobertCompleted -or $passed -lt $expected) {
    if($failed) { $failed | ForEach-Object {$_.Line} }
    throw "Package verification failed (exit $($process.ExitCode), $passed passes; expected at least $expected). Inspect $log"
}
Write-Output "CHUCK_PACKAGE_VERIFIED passes=$passed log=$log"
if (!$NoCapture) { Write-Output ('Captures: '+(Join-Path $PackageRoot 'Chuck3D\Saved\Screenshots\Windows')) }
$global:ChuckLastVerificationLog=$log
