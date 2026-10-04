param([switch]$CheckOnly)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$package=Join-Path $projectRoot 'Builds\Windows'
$game=Join-Path $package 'Chuck3D\Binaries\Win64\Chuck3D.exe'
$receiptPath=Join-Path $package 'prototype-build.json'
if(!(Test-Path -LiteralPath $game) -or !(Test-Path -LiteralPath $receiptPath)) {
    throw 'No verified launcher build is installed. See docs/PLAYTEST.md; importing assets or building in an agent worktree does not update this launcher.'
}
$receipt=Get-Content -LiteralPath $receiptPath -Raw | ConvertFrom-Json
$hashAlgorithm=[Security.Cryptography.SHA256]::Create()
$stream=[IO.File]::OpenRead($game)
try { $executableHash=[BitConverter]::ToString($hashAlgorithm.ComputeHash($stream)).Replace('-','') }
finally { $stream.Dispose(); $hashAlgorithm.Dispose() }
if($executableHash -ne $receipt.executableSha256) {
    throw 'The executable changed since package verification. Run Tools/Verify-Package.ps1 and record a new build receipt before launching.'
}
Write-Output ('CHUCK 3D - verified build '+$receipt.sourceRevision.Substring(0,7)+' - '+$receipt.executableBuiltUtc)
if(Get-Command git -ErrorAction SilentlyContinue) {
    $runtimeRevision=(& git -C $projectRoot log -1 --format=%H -- Unreal/Chuck3D).Trim()
    if($LASTEXITCODE -eq 0 -and $runtimeRevision -ne $receipt.runtimeRevision) {
        Write-Warning 'Newer Unreal changes exist in source but are not in this packaged build. Package and verify them before expecting them in the launcher.'
    }
}
if($CheckOnly) { Write-Output "CHUCK_LAUNCH_TARGET $game"; exit 0 }
Start-Process -FilePath $game -WorkingDirectory $package -ArgumentList '-fullscreen'
