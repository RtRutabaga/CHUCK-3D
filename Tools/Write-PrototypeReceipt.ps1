param([string]$PackageRoot,[Parameter(Mandatory=$true)][string]$VerificationLog,[switch]$AstralCameraReview)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
if(!$PackageRoot) { $PackageRoot=Join-Path $projectRoot 'Builds\Windows' }
$game=Join-Path $PackageRoot 'Chuck3D\Binaries\Win64\Chuck3D.exe'
$completion=if($AstralCameraReview) {'CHUCK_ASTRAL_CAMERA_CHECK failures=0 walls=2 camera_samples=6'} else {'CHUCK_TEST_COMPLETE failures=0'}
if(!(Select-String -LiteralPath $VerificationLog -Pattern $completion -Quiet) -or
    (Select-String -LiteralPath $VerificationLog -Pattern 'CHUCK_TEST FAIL|Fatal error:' -Quiet)) {
    throw 'A successful package verification log is required.'
}
$revision=(& git -C $projectRoot rev-parse HEAD).Trim()
if($LASTEXITCODE) { throw 'Cannot identify source revision.' }
$runtimeRevision=(& git -C $projectRoot log -1 --format=%H -- Unreal/Chuck3D).Trim()
$hashAlgorithm=[Security.Cryptography.SHA256]::Create()
$stream=[IO.File]::OpenRead($game)
try { $executableHash=[BitConverter]::ToString($hashAlgorithm.ComputeHash($stream)).Replace('-','') }
finally { $stream.Dispose(); $hashAlgorithm.Dispose() }
$receipt=[ordered]@{
    sourceRevision=$revision
    runtimeRevision=$runtimeRevision
    verifiedAtUtc=(Get-Date).ToUniversalTime().ToString('o')
    executableSha256=$executableHash
    executableBuiltUtc=(Get-Item -LiteralPath $game).LastWriteTimeUtc.ToString('o')
    verificationLog=[IO.Path]::GetFullPath($VerificationLog)
    verificationScope=$(if($AstralCameraReview) {'Focused Astral camera collision and visual review; full traversal suite not rerun at user request'} else {'Full package verification'})
}
$receipt | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $PackageRoot 'prototype-build.json') -Encoding UTF8
Write-Output "CHUCK_BUILD_RECEIPT $revision"
