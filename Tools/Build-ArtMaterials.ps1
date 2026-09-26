param([string]$EngineRoot = 'C:\Program Files\Epic Games\UE_5.7')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$editor=Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$project=Join-Path $projectRoot 'Unreal\Chuck3D\Chuck3D.uproject'
$script=Join-Path $PSScriptRoot 'create_art_materials.py'
$log=Join-Path $projectRoot 'Unreal\Chuck3D\Saved\Logs\ChuckArtMaterials.log'
& $editor $project "-ExecutePythonScript=$script" -unattended -nullrhi -nosplash -NoLiveCoding "-abslog=$log"
if ($LASTEXITCODE -or !(Select-String -LiteralPath $log -Pattern 'CHUCK_ART_MATERIALS_READY' -Quiet)) {
    throw "Art material generation failed; inspect $log"
}
if (Select-String -LiteralPath $log -Pattern 'Failed to compile Material|LogShaderCompilers: Error|LogPython: Error' -Quiet) {
    throw "Art material generation reported errors; inspect $log"
}
$surfaceManifest=Join-Path $projectRoot 'SourceAssets\Surfaces\PolyHaven\manifest.json'
if (Test-Path -LiteralPath $surfaceManifest) {
    $surfaceScript=Join-Path $PSScriptRoot 'import_surface_textures.py'
    $surfaceLog=Join-Path $projectRoot 'Unreal\Chuck3D\Saved\Logs\ChuckSurfaceTextures.log'
    & $editor $project "-ExecutePythonScript=$surfaceScript" -unattended -nullrhi -nosplash -NoLiveCoding "-abslog=$surfaceLog"
    if ($LASTEXITCODE -or !(Select-String -LiteralPath $surfaceLog -Pattern 'CHUCK_SCANNED_SURFACES_READY' -Quiet) -or (Select-String -LiteralPath $surfaceLog -Pattern 'Failed to compile Material|LogShaderCompilers: Error|LogPython: Error' -Quiet)) {
        throw "Scanned surface import failed; inspect $surfaceLog"
    }
}
