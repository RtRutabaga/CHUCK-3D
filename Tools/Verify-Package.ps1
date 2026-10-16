param([switch]$NoCapture,[switch]$MotionCapture,[switch]$NoGroom,[string]$PackageRoot)
$ErrorActionPreference='Stop'
if($NoCapture -and $MotionCapture) { throw 'MotionCapture requires captures; omit NoCapture.' }
$projectRoot=Split-Path $PSScriptRoot -Parent
if(!$PackageRoot) { $PackageRoot=Join-Path $projectRoot 'Builds\Windows' }
$PackageRoot=(Resolve-Path -LiteralPath $PackageRoot).Path
$game=Join-Path $PackageRoot 'Chuck3D\Binaries\Win64\Chuck3D.exe'
if (!(Test-Path -LiteralPath $game)) { throw 'No packaged game. Build Windows first; see docs/PLAYING.md.' }
$evidence=Join-Path $projectRoot 'Local'
New-Item -ItemType Directory -Force -Path $evidence | Out-Null
$log=Join-Path $evidence ('verify-package-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'.log')
$arguments=@('-ChuckSmokeTest','-windowed','-ResX=1280','-ResY=720',('-abslog="'+$log+'"'))
# The golden cigarettes: the run up the first giant, then the same up the second (trail round both feet).
foreach($goldenTree in 0,1) {
$goldenLog=Join-Path $evidence ('verify-golden-leaf-tree'+$goldenTree+'-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'.log')
$goldenProcess=Start-Process -FilePath $game -ArgumentList @('-ChuckGoldenLeafTest',('-ChuckGoldenLeafTree='+$goldenTree),'-windowed','-ResX=1280','-ResY=720',('-abslog="'+$goldenLog+'"')) -WindowStyle Hidden -Wait -PassThru
if($goldenProcess.ExitCode -or !(Select-String -LiteralPath $goldenLog -Pattern 'CHUCK_GOLDEN_TEST failures=0 duration=45' -Quiet) -or
   !(Select-String -LiteralPath $goldenLog -Pattern ('CHUCK_GOLDEN_SETUP tree='+$goldenTree+' pickups=8') -Quiet) -or
   !(Select-String -LiteralPath $goldenLog -Pattern 'CHUCK_GOLDEN_CLIMB ' -Quiet) -or
   !(Select-String -LiteralPath $goldenLog -Pattern 'CHUCK_GOLDEN_FIRST_GRIP ' -Quiet) -or
   !(Select-String -LiteralPath $goldenLog -Pattern 'CHUCK_GOLDEN_RECOLLECT ' -Quiet) -or
   (Select-String -LiteralPath $goldenLog -Pattern 'CHUCK_GOLDEN_FAIL|Fatal error:|bUsedWithInstancedStaticMeshes' -Quiet)) {
    throw "Golden cigarette verification failed. Inspect $goldenLog"
}
}
$impactLog=Join-Path $evidence ('verify-zombie-impact-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'.log')
$impactProcess=Start-Process -FilePath $game -ArgumentList @('-ChuckZombieImpactTest','-windowed','-ResX=1280','-ResY=720',('-abslog="'+$impactLog+'"')) -WindowStyle Hidden -Wait -PassThru
if($impactProcess.ExitCode -or !(Select-String -LiteralPath $impactLog -Pattern 'CHUCK_ZOMBIE_IMPACT_TEST failures=0 trials=4 sounds=12' -Quiet)) {
    throw "Zombie impact verification failed. Inspect $impactLog"
}
$chultLog=Join-Path $evidence ('verify-chult-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'.log')
$chultProcess=Start-Process -FilePath $game -ArgumentList @('-ChuckChultTest','-windowed','-ResX=1280','-ResY=720',('-abslog="'+$chultLog+'"')) -WindowStyle Hidden -Wait -PassThru
if($chultProcess.ExitCode -or !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_LANDING .* missing_assets=0' -Quiet) -or
   !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_WATERCOURSE .*missing_assets=0 falls_collision=0' -Quiet) -or
   !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_RIVER .*missing_assets=0 water_collision=0' -Quiet) -or
   !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_RIVER_CHECK failures=0 ' -Quiet) -or
   !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_PATH .*missing_material=0 route_failures=0' -Quiet) -or
   !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_PATH_CHECK failures=0 .*dry_routes=2 bridge_alignment=1 paving_collision=0' -Quiet) -or
   !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_GORGE .*missing_assets=0 falls_collision=0' -Quiet) -or
   !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_GORGE_CHECK failures=0 ' -Quiet) -or
   !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_VISTA .*missing_assets=0 far_floor_changed=0' -Quiet) -or
   !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_VISTA_CHECK failures=0 .*land_in_gorge=0 land_over_floor=0 gorge_failures=0' -Quiet) -or
   !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_VISTA_REVIEW_COMPLETE failures=0 views=7' -Quiet) -or
   !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_RIVER_TEST failures=0 trials=3' -Quiet) -or
   !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_WATERCOURSE_CHECK failures=0 ' -Quiet) -or
   !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_WATERCOURSE_TEST failures=0 trials=3' -Quiet) -or
   !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_CANYON .*missing_assets=0 deck_collision=0 rope_collision=0' -Quiet) -or
   !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_CANYON_CHECK failures=0 .*bridge_failures=0 active_crossing=1' -Quiet) -or
   !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_CANYON_TEST_COMPLETE failures=0 trials=3 gorge_return=bridge' -Quiet) -or
   !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_BRIDGE slots=166 grips=31 .*missing_assets=0 deck_collision=0 rope_collision=0' -Quiet) -or
   !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_BRIDGE_CHECK failures=0 grips=31 registered=31 ' -Quiet) -or
   !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_OVERGROWTH .*missing_assets=0 foliage_collision=0 rock_collision=1 existing_removed=0' -Quiet) -or
   !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_LOW_MIST pools=12 height_layer=1 cool_sun=1 collision=0' -Quiet) -or
   !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_CLIFFS .* missing_material=0 .*northeast_open=1' -Quiet) -or
   !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_CLIFF_CHECK failures=0 ' -Quiet) -or
   !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_CLIFF_TEST_COMPLETE failures=0 trials=3' -Quiet) -or
   !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_EXPANSION .* missing_assets=0 .*water_collision=0' -Quiet) -or
   !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_EXPANSION_CHECK failures=0 ' -Quiet) -or
   !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_EXPANSION_TEST_COMPLETE failures=0 trials=3' -Quiet) -or
   !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_GROVE_GROUND failures=0 capsule_segments=11' -Quiet) -or
   !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_CANOPY trees=8 chains=8 grips=74 spurs=14 spur_grips=53 registered_grips=127 lianas=8' -Quiet) -or
   !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_CANOPY_LAYOUT failures=0 branch_jumps=8 hand_gap=180 grip_over=98 .* missing_assets=0' -Quiet) -or
   !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_CANOPY_CHECK failures=0 grips=127 leaps=105 limb_samples=253 forks=8 lianas=8 spurs=14' -Quiet) -or
   !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_UPRIVER .*missing_assets=0 water_collision=0' -Quiet) -or
   !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_UPRIVER_CHECK failures=0 .*respawn_map=4 giant_failures=0' -Quiet) -or
   !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_GIANTS trees=12 chains=11 .*lianas=1 ' -Quiet) -or
   !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_GIANTS_LAYOUT failures=0 .* missing_assets=0' -Quiet) -or
   !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_GIANTS_CHECK failures=0 ' -Quiet) -or
   !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_TEMPLE tiers=4 .*stakes=8 serpents=2 stelae=4 .*missing_assets=0' -Quiet) -or
   !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_TEMPLE_CHECK failures=0 ' -Quiet) -or
   !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_CHECK failures=0 ' -Quiet) -or
   !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_RESPAWN failures=0 entrance=1' -Quiet) -or
   !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_TEST_COMPLETE' -Quiet) -or
   !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_MUSIC_CHECK failures=0 looping=1 playing_after_boundary=1' -Quiet) -or
   !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_WET_DRESSING materials=1 moss_strands=735 puddles=9 rocks=27 .*foliage_collision=0 water_collision=0' -Quiet) -or
   !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_NATURE ferns=[1-9][0-9]* fern_variants=4 background_trees=18 tree_collision=1 foliage_collision=0 missing_assets=0' -Quiet) -or
   !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_SCAN_ROCKS instances=27 variants=6 missing_assets=0 visible_mesh_collision=1' -Quiet) -or
   !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_RUINS .* missing_materials=0 climb_gap=120 jump_gap=100 causeway_top=650 cliffs=0' -Quiet) -or
   !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_RUINS_FINISH rounded_blocks=544 gray_scan=1 moss_layers=1 shared_materials_changed=0' -Quiet) -or
   !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_RUINS_GEOMETRY failures=0 standing_tops=12' -Quiet) -or
   !(Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_RUINS_TEST_COMPLETE failures=0 trials=5' -Quiet) -or
   (Select-String -LiteralPath $chultLog -Pattern 'Fatal error:|Failed to compile Material|Default Material will be used in game' -Quiet)) {
    throw "Chult landing verification failed. Inspect $chultLog"
}
$ruinSetup=Select-String -LiteralPath $chultLog -Pattern 'CHUCK_CHULT_RUINS_BUILD ms=([0-9.]+) vertices=109888'
if(!$ruinSetup -or [double]::Parse($ruinSetup.Matches[0].Groups[1].Value,[Globalization.CultureInfo]::InvariantCulture) -gt 10000) {
    throw "Rounded ruin setup exceeded its 10-second budget. Inspect $chultLog"
}
$pathLog=Join-Path $evidence ('verify-chult-path-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'.log')
$pathProcess=Start-Process -FilePath $game -ArgumentList @('-ChuckChultTest','-ChuckPathReview','-windowed','-ResX=1280','-ResY=720',('-abslog="'+$pathLog+'"')) -WindowStyle Hidden -Wait -PassThru
if($pathProcess.ExitCode -or !(Select-String -LiteralPath $pathLog -Pattern 'CHUCK_CHULT_PATH_CHECK failures=0 ' -Quiet) -or
   !(Select-String -LiteralPath $pathLog -Pattern 'CHUCK_CHULT_PATH_REVIEW_COMPLETE failures=0 views=6' -Quiet) -or
   !(Select-String -LiteralPath $pathLog -Pattern 'CHUCK_CHULT_PATH_TEST failures=0 trials=10' -Quiet)) {
    throw "Ancient jungle road verification failed. Inspect $pathLog"
}
# Upriver (Claude, 2026-10-15): the extended far bank, source fall, giant trees and deep jungle in cooked views.
$upriverLog=Join-Path $evidence ('verify-chult-upriver-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'.log')
$upriverProcess=Start-Process -FilePath $game -ArgumentList @('-ChuckChultTest','-ChuckUpriverReview','-windowed','-ResX=1280','-ResY=720',('-abslog="'+$upriverLog+'"')) -WindowStyle Hidden -Wait -PassThru
if($upriverProcess.ExitCode -or !(Select-String -LiteralPath $upriverLog -Pattern 'CHUCK_CHULT_UPRIVER_CHECK failures=0 ' -Quiet) -or
   !(Select-String -LiteralPath $upriverLog -Pattern 'CHUCK_CHULT_UPRIVER_REVIEW_COMPLETE failures=0 views=6' -Quiet)) {
    throw "Upriver verification failed. Inspect $upriverLog"
}
# The temple exterior (Claude, 2026-10-15): cooked views, then the real character
# runs up the pyramid's stair to the summit and steps into the doorway.
$templeLog=Join-Path $evidence ('verify-chult-temple-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'.log')
$templeProcess=Start-Process -FilePath $game -ArgumentList @('-ChuckChultTest','-ChuckTempleReview','-windowed','-ResX=1280','-ResY=720',('-abslog="'+$templeLog+'"')) -WindowStyle Hidden -Wait -PassThru
if($templeProcess.ExitCode -or !(Select-String -LiteralPath $templeLog -Pattern 'CHUCK_CHULT_TEMPLE_CHECK failures=0 ' -Quiet) -or
   !(Select-String -LiteralPath $templeLog -Pattern 'CHUCK_CHULT_TEMPLE_TEST failures=0 climb=1 legs=3' -Quiet)) {
    throw "Temple verification failed. Inspect $templeLog"
}
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
$demoLog=Join-Path $evidence ('verify-demo-end-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'.log')
$demoProcess=Start-Process -FilePath $game -ArgumentList @('-ChuckDemoEndTest','-windowed','-ResX=1280','-ResY=720',('-abslog="'+$demoLog+'"')) -WindowStyle Hidden -Wait -PassThru
if($demoProcess.ExitCode -or !(Select-String -LiteralPath $demoLog -Pattern 'CHUCK_DEMO_END_TEST failures=0 sky_ends=1 floor_and_astral_continue=1 reset_blocked=1' -Quiet) -or
   !(Select-String -LiteralPath $demoLog -Pattern 'CHUCK_DEMO_SKIP_TEST failures=0 landing=1' -Quiet) -or
   !(Select-String -LiteralPath $demoLog -Pattern 'CHUCK_CHULT_INPUT_TEST failures=0 moved=1 local_reset=1' -Quiet) -or
   (Select-String -LiteralPath $demoLog -Pattern 'CHUCK_CHULT_INPUT_TEST failures=1|Fatal error:' -Quiet)) {
    throw "Cutscene skip, playable landing or local reset verification failed. Inspect $demoLog"
}
# The fall to Chult played through (Claude, FallCutscene.h): music and cloud,
# the crash, the astral death and return, two drags, then playable Chult.
$fallLog=Join-Path $evidence ('verify-fall-cutscene-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'.log')
$fallProcess=Start-Process -FilePath $game -ArgumentList @('-ChuckFallCutsceneTest','-windowed','-ResX=1280','-ResY=720',('-abslog="'+$fallLog+'"')) -WindowStyle Hidden -Wait -PassThru
if($fallProcess.ExitCode -or !(Select-String -LiteralPath $fallLog -Pattern 'CHUCK_FALL_CUTSCENE_START .* music=1 wind=1 crashes=3 flap=1 whistle=1 thud=1' -Quiet) -or
   !(Select-String -LiteralPath $fallLog -Pattern 'CHUCK_FALL_CUTSCENE_MUSIC playing=1' -Quiet) -or
   !(Select-String -LiteralPath $fallLog -Pattern 'CHUCK_FALL_CUTSCENE_SCORE at=0\.0' -Quiet) -or
   !(Select-String -LiteralPath $fallLog -Pattern 'CHUCK_FALL_CUTSCENE_END at=4[0-9.]+ skipped=0 landed=1 vanished=1 summoned=1 exhales=2' -Quiet) -or
   !(Select-String -LiteralPath $fallLog -Pattern 'CHUCK_FALL_CUTSCENE_TEST failures=0 landing=1' -Quiet) -or
   (Select-String -LiteralPath $fallLog -Pattern 'Fatal error:' -Quiet)) {
    throw "Fall-to-Chult cutscene verification failed. Inspect $fallLog"
}
$pantryLog=Join-Path $evidence ('verify-pantry-adjustments-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'.log')
$pantryArguments=@('-ChuckPantryAdjustmentsTest','-windowed','-ResX=1280','-ResY=720',('-abslog="'+$pantryLog+'"'))
$pantryProcess=Start-Process -FilePath $game -ArgumentList $pantryArguments -WindowStyle Hidden -Wait -PassThru
if($pantryProcess.ExitCode -or !(Select-String -LiteralPath $pantryLog -Pattern 'CHUCK_PANTRY_ADJUSTMENTS_CHECK failures=0 oil_rims=6 noncolliding=1 cheese_blocks=1 cellar_talk=0 surface_talk=1' -Quiet)) {
    throw "Cellar talk, oil rims or cheese collision verification failed. Inspect $pantryLog"
}
if(!(Select-String -LiteralPath $pantryLog -Pattern 'CHUCK_PANTRY_ISLAND_CHECK failures=0 floor=1 underfloor_clear=1' -Quiet)) {
    throw "Suspended cellar floor verification failed. Inspect $pantryLog"
}
$leapLog=Join-Path $evidence ('verify-leap-rift-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'.log')
$leapArguments=@('-ChuckLeapRiftTest','-windowed','-ResX=1280','-ResY=720',('-abslog="'+$leapLog+'"'))
if (!$NoCapture) { $leapArguments+='-ChuckLeapRiftMotionCapture' }
$leapProcess=Start-Process -FilePath $game -ArgumentList $leapArguments -WindowStyle Hidden -Wait -PassThru
if ($leapProcess.ExitCode -or !(Select-String -LiteralPath $leapLog -Pattern 'CHUCK_LEAPRIFT_TEST_COMPLETE failures=0 trials=6 sprint=1 ordinary=3 wall_assisted=2 bypass_allowed=1' -Quiet)) {
    throw "Astral floor gap traversal verification failed. Inspect $leapLog"
}
# Lamp alleys (user 2026-10-09): chimney climbs caught at the eave and pulled up
# onto each alley's roof, a wall jump caught by a lantern ring, brachiation down
# the alley and back, and a kick off a ring into a chimney climb to the roof.
$swingLog=Join-Path $evidence ('verify-lamp-swing-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'.log')
$swingArguments=@('-ChuckLampSwingTest','-windowed','-ResX=1280','-ResY=720',('-abslog="'+$swingLog+'"'))
$swingProcess=Start-Process -FilePath $game -ArgumentList $swingArguments -WindowStyle Hidden -Wait -PassThru
if ($swingProcess.ExitCode -or !(Select-String -LiteralPath $swingLog -Pattern 'CHUCK_LAMPSWING_TEST_COMPLETE failures=0 trials=8 chimneys=3 catch=1 swings=2 kick=1 locked=1' -Quiet)) {
    throw "Lamp-alley climb and brachiation verification failed. Inspect $swingLog"
}
# Chult canopy (Claude, 2026-10-13): the real character climbs a liana, catches
# and brachiates all four chains tree to tree (branch-gap leaps, step-down
# dismounts, crotch crossings), reverses, lets go to the ground, climbs down,
# swings out to the tip of every side branch (spur) and back, and follows the
# line of trees into the open ground to its last tree and down its liana; dies
# in a tree, under the line and on the landing loop and comes back in the right place.
$canopyLog=Join-Path $evidence ('verify-canopy-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'.log')
$canopyProcess=Start-Process -FilePath $game -ArgumentList @('-ChuckCanopyTest','-windowed','-ResX=1280','-ResY=720',('-abslog="'+$canopyLog+'"')) -WindowStyle Hidden -Wait -PassThru
if ($canopyProcess.ExitCode -or !(Select-String -LiteralPath $canopyLog -Pattern 'CHUCK_CANOPY_TEST_COMPLETE failures=0 trials=48 static_failures=0' -Quiet)) {
    throw "Chult canopy brachiation verification failed. Inspect $canopyLog"
}
# The giant trees upriver (Claude, 2026-10-15): from the first giant's crotch along
# every chain to the last, stepping down at each, down its liana, deaths in a tree
# and under them returning to the giants' spawn, and out to every spur's tip and back.
$giantLog=Join-Path $evidence ('verify-giant-canopy-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'.log')
$giantProcess=Start-Process -FilePath $game -ArgumentList @('-ChuckGiantCanopyTest','-windowed','-ResX=1280','-ResY=720',('-abslog="'+$giantLog+'"')) -WindowStyle Hidden -Wait -PassThru
if ($giantProcess.ExitCode -or !(Select-String -LiteralPath $giantLog -Pattern 'CHUCK_GIANT_CANOPY_TEST_COMPLETE failures=0 .*static_failures=0 chains=11' -Quiet)) {
    throw "Giant canopy brachiation verification failed. Inspect $giantLog"
}
# The giants' T-rex (Claude, 2026-10-15): its loop clear of the trunks, its patrol,
# a roar, chase and the one killing bite with the return to the Giant Trees spawn,
# safe up in a giant while it stalks below, and it staying in its stretch.
$trexLog=Join-Path $evidence ('verify-trex-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'.log')
$trexProcess=Start-Process -FilePath $game -ArgumentList @('-ChuckTRexTest','-windowed','-ResX=1280','-ResY=720',('-abslog="'+$trexLog+'"')) -WindowStyle Hidden -Wait -PassThru
if ($trexProcess.ExitCode -or !(Select-String -LiteralPath $trexLog -Pattern 'CHUCK_TREX_TEST_COMPLETE failures=0 trials=4' -Quiet) -or
    !(Select-String -LiteralPath $trexLog -Pattern 'CHUCK_TREX_ROUTE failures=0 segments=24 liana_exit_safe=1' -Quiet) -or
    !(Select-String -LiteralPath $trexLog -Pattern 'CHUCK_TREX_SOUNDS steps=3 roar=2 snap=1 growl=1' -Quiet) -or
    !(Select-String -LiteralPath $trexLog -Pattern 'CHUCK_TREX_SPAWN .* clips=6 mesh=1' -Quiet)) {
    throw "Chult T-rex verification failed. Inspect $trexLog"
}
# The canopy raptor (Claude, 2026-10-14): its round, noticing him, the chase, tell
# and bite, a roll through the tell, escaping up a tree and across a chain over it,
# its territory, a death and the ruin-top return, ten scratches, and its sounds.
$raptorLog=Join-Path $evidence ('verify-raptor-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'.log')
$raptorProcess=Start-Process -FilePath $game -ArgumentList @('-ChuckRaptorTest','-windowed','-ResX=1280','-ResY=720',('-abslog="'+$raptorLog+'"')) -WindowStyle Hidden -Wait -PassThru
if ($raptorProcess.ExitCode -or !(Select-String -LiteralPath $raptorLog -Pattern 'CHUCK_RAPTOR_TEST_COMPLETE failures=0 trials=8' -Quiet) -or
    !(Select-String -LiteralPath $raptorLog -Pattern 'CHUCK_RAPTOR_ROUTE failures=0 segments=8' -Quiet) -or
    !(Select-String -LiteralPath $raptorLog -Pattern 'CHUCK_RAPTOR_SOUNDS steps=3 hiss=1 snap=2 call=1 death=1' -Quiet) -or
    !(Select-String -LiteralPath $raptorLog -Pattern 'CHUCK_RAPTOR_SPAWN .* clips=6 mesh=1' -Quiet)) {
    throw "Chult raptor verification failed. Inspect $raptorLog"
}
# The canyon's broken rope bridge (Claude, 2026-10-14): the real character catches
# the first surviving plank, brachiates plank to plank to the far bank and steps
# down, comes back steering with the stick, lets go over the gorge and returns
# to the near bank, and can't stand on a plank; its own score over the bridge.
$bridgeLog=Join-Path $evidence ('verify-bridge-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'.log')
$bridgeProcess=Start-Process -FilePath $game -ArgumentList @('-ChuckBridgeTest','-windowed','-ResX=1280','-ResY=720',('-abslog="'+$bridgeLog+'"')) -WindowStyle Hidden -Wait -PassThru
if ($bridgeProcess.ExitCode -or !(Select-String -LiteralPath $bridgeLog -Pattern 'CHUCK_BRIDGE_TEST_COMPLETE failures=0 trials=6 static_failures=0 grips=31 music_far=1 music_back=1' -Quiet) -or
    !(Select-String -LiteralPath $bridgeLog -Pattern 'CHUCK_CHULT_BRIDGE_MUSIC_CHECK failures=0 looping=1 playing_after_boundary=1' -Quiet)) {
    throw "Chult bridge brachiation verification failed. Inspect $bridgeLog"
}
# Chult's hostile life (Claude, 2026-10-14): fifteen zombies on the landing's side,
# clear of the brachiation trees, idle until he comes near; five raptors on the trees' round.
$lifeLog=Join-Path $evidence ('verify-jungle-life-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'.log')
$lifeProcess=Start-Process -FilePath $game -ArgumentList @('-ChuckJungleLifeTest','-windowed','-ResX=1280','-ResY=720',('-abslog="'+$lifeLog+'"')) -WindowStyle Hidden -Wait -PassThru
if ($lifeProcess.ExitCode -or !(Select-String -LiteralPath $lifeLog -Pattern 'CHUCK_CHULT_LIFE_TEST_COMPLETE failures=0' -Quiet) -or
    !(Select-String -LiteralPath $lifeLog -Pattern 'CHUCK_CHULT_LIFE zombies=15 ' -Quiet)) {
    throw "Chult jungle life verification failed. Inspect $lifeLog"
}
# The zombie grab (Claude, 2026-10-14; forced every time here, 1 in 4 in play): seized,
# lifted to its face with both hands on him, bitten to zero sanity, gone from its hands,
# summoned back whole at the landing, and it lets go.
$grabLog=Join-Path $evidence ('verify-zombie-grab-'+(Get-Date -Format 'yyyyMMdd-HHmmss')+'.log')
$grabProcess=Start-Process -FilePath $game -ArgumentList @('-ChuckZombieGrabTest','-windowed','-ResX=1280','-ResY=720',('-abslog="'+$grabLog+'"')) -WindowStyle Hidden -Wait -PassThru
if ($grabProcess.ExitCode -or !(Select-String -LiteralPath $grabLog -Pattern 'CHUCK_ZOMBIE_GRAB_TEST_COMPLETE failures=0' -Quiet)) {
    throw "Zombie grab verification failed. Inspect $grabLog"
}
$process=Start-Process -FilePath $game -ArgumentList $arguments -WindowStyle Hidden -Wait -PassThru
if (!(Test-Path -LiteralPath $log)) { throw "Game did not produce its test log: $log" }
$failed=Select-String -LiteralPath $log -Pattern 'CHUCK_TEST FAIL|Failed to compile Material|valid ShaderMap|Default Material will be used in game|Fatal error:'
$completed=Select-String -LiteralPath $log -Pattern 'CHUCK_TEST_COMPLETE failures=0'
$leapGeometry=Select-String -LiteralPath $log -Pattern 'CHUCK_LEAPRIFT_GEOMETRY failures=0 floor_holes=9 solid_camera_walls=26 length_cm=260 after_chamber=1'
$worldCompleted=Select-String -LiteralPath $log -Pattern 'CHUCK_WORLD_CHECK_COMPLETE failures=0 '
$wallLampsCompleted=Select-String -LiteralPath $log -Pattern 'CHUCK_WALLLAMPS_CHECK failures=0 lamps=15 grips=15 swing_grips=15 clear_swings=12 collision=0'
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
$astralCompleted=Select-String -LiteralPath $log -Pattern 'CHUCK_ASTRAL_HAZARDS failures=0 holes=33 purple_lights=33 torches=0 large=13 small=20'
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
$expected=if($NoCapture) {179} else {180}
if (!$leapGeometry) { throw "Astral floor gap geometry verification failed. Inspect $log" }
if(!(Select-String -LiteralPath $log -Pattern 'CHUCK_LEAP_OIL_CHECK failures=0 depth_cm=30.5 noncolliding=1 vertical_fade=1' -Quiet)) {
    throw "Sewer oil film verification failed. Inspect $log"
}
if (!$wallLampsCompleted) { throw "Wall lamp geometry verification failed. Inspect $log" }
if ($process.ExitCode -or $failed -or !$completed -or !$worldCompleted -or !$streetCompleted -or !$sideGateCompleted -or !$courtPierCompleted -or !$dockPropsCompleted -or !$plazaCompleted -or !$harborWaterCompleted -or !$surfaceWaterCompleted -or !$musicCompleted -or !$nightMusicCompleted -or !$chimneyCompleted -or !$hatchCompleted -or !$sewerCompleted -or !$astralCompleted -or !$smallStreamCompleted -or !$caveCompleted -or !$tavernCompleted -or !$pantryCompleted -or !$streamAudio -or !$fireCompleted -or !$narrowCompleted -or !$wallRiftCompleted -or !$rubbleCompleted -or !$forgeCompleted -or !$smithCompleted -or !$dwarfVoice -or !$guardVoice -or !$plazaGuardVoice -or !$sideGuardVoice -or !$sailorVoice -or !$workerVoice -or !$elfVoice -or !$marketVoice -or !$alchemistVoice -or !$smithVoice -or !$keeperCompleted -or !$sailorCompleted -or !$bobertCompleted -or $passed -lt $expected) {
    if($failed) { $failed | ForEach-Object {$_.Line} }
    throw "Package verification failed (exit $($process.ExitCode), $passed passes; expected at least $expected). Inspect $log"
}
Write-Output "CHUCK_PACKAGE_VERIFIED passes=$passed log=$log"
if (!$NoCapture) { Write-Output ('Captures: '+(Join-Path $PackageRoot 'Chuck3D\Saved\Screenshots\Windows')) }
$global:ChuckLastVerificationLog=$log
