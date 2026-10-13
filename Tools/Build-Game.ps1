param(
    [string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.7',
    [switch]$Package,
    # Source-only builds can retain the existing imported/base binary assets.
    [switch]$SkipBaseAssets,
    # Package into a candidate folder (e.g. Builds\RenameCandidate) instead of the launcher build; promote it with Promote-Build.ps1.
    [string]$ArchiveDirectory
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$projectFile = Join-Path $projectRoot 'Unreal\Chuck3D\Chuck3D.uproject'
$buildTool = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if (-not (Test-Path -LiteralPath $buildTool)) { throw "Unreal 5.7 is not installed at $EngineRoot. Pass -EngineRoot if installed elsewhere." }
# UBT makefiles restore their saved process environment, including TEMP/TMP.
# A sandbox's temporary directory can expire between runs (MSVC D8050), and
# old checkout paths can also persist there. Recalculate actions each build.
& $buildTool Chuck3DEditor Win64 Development "-Project=$projectFile" -WaitMutex -MaxParallelActions=1 -NoUBA -NoUBTMakefiles
if ($LASTEXITCODE -ne 0) { throw 'Editor module build failed.' }
$assetScript = Join-Path $PSScriptRoot 'create_base_assets.py'
if (!$SkipBaseAssets) {
    & $editor $projectFile -run=pythonscript "-script=$assetScript" -unattended -nullrhi -nosplash
    if ($LASTEXITCODE -ne 0) { throw 'Base asset generation failed.' }
}
if (-not (Test-Path (Join-Path $projectRoot 'Unreal\Chuck3D\Content\Maps\WaterdeepDocks.umap'))) { throw 'Expected Waterdeep docks map was not generated.' }
if ($Package) {
    $automationTool = Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat'
    $output = if ($ArchiveDirectory) { [IO.Path]::GetFullPath($ArchiveDirectory) } else { Join-Path $projectRoot 'Builds' }
    # The editor was compiled above. Build only the game here, with fresh UBT
    # actions so the packaging process does not inherit a stale environment.
    & $automationTool BuildCookRun "-project=$projectFile" -noP4 -platform=Win64 -clientconfig=Development -build -skipbuildeditor -cook -stage -pak -archive "-archivedirectory=$output" -unattended -utf8output '-UbtArgs=-MaxParallelActions=1 -NoUBA -NoUBTMakefiles'
    if ($LASTEXITCODE -ne 0) { throw 'Windows packaging failed.' }
    if ($ArchiveDirectory) {
        & (Join-Path $PSScriptRoot 'Verify-Package.ps1') -PackageRoot (Join-Path $output 'Windows')
        Write-Output "CHUCK_CANDIDATE_VERIFIED $output log=$global:ChuckLastVerificationLog"
    } else {
        & (Join-Path $PSScriptRoot 'Verify-Package.ps1')
        & (Join-Path $PSScriptRoot 'Write-BuildReceipt.ps1') -VerificationLog $global:ChuckLastVerificationLog
    }
}
