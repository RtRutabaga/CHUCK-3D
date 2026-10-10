param([Parameter(Mandatory=$true)][string]$PackageRoot,[string]$Name='CHUCK-3D-Windows-Demo-2026-10-09')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$package=(Resolve-Path -LiteralPath $PackageRoot).Path
$receipt=Get-Content -LiteralPath (Join-Path $package 'prototype-build.json') -Raw | ConvertFrom-Json
$game=Join-Path $package 'Chuck3D/Binaries/Win64/Chuck3D.exe'
if((Get-FileHash -LiteralPath $game).Hash -ne $receipt.executableSha256) {throw 'Package executable does not match its verified receipt.'}
if($Name -notmatch '^[A-Za-z0-9-]+$') {throw 'Use a simple distribution name.'}
$releaseRoot=Join-Path $projectRoot 'Builds/Releases'
$folder=Join-Path $releaseRoot $Name
$zip=Join-Path $releaseRoot ($Name+'.zip')
if((Test-Path -LiteralPath $folder) -or (Test-Path -LiteralPath $zip)) {throw 'Distribution already exists; use a fresh name.'}
New-Item -ItemType Directory -Path $folder -Force | Out-Null
foreach($file in Get-ChildItem -LiteralPath $package -File -Recurse) {
    $relative=[IO.Path]::GetRelativePath($package,$file.FullName)
    if($relative -match '(^|[\\/])Saved([\\/]|$)' -or $file.Extension -in '.pdb','.lib','.exp' -or
       $file.Name -like 'Manifest_*' -or $file.Name -eq 'prototype-build.json') {continue}
    $target=Join-Path $folder $relative
    New-Item -ItemType Directory -Path (Split-Path $target -Parent) -Force | Out-Null
    Copy-Item -LiteralPath $file.FullName -Destination $target
}
Copy-Item -LiteralPath (Join-Path $projectRoot 'docs/DEMO-README.txt') -Destination (Join-Path $folder 'README.txt')
Copy-Item -LiteralPath (Join-Path $projectRoot 'docs/DEMO-CREDITS.txt') -Destination (Join-Path $folder 'CREDITS.txt')
$prerequisites=Join-Path $folder 'Prerequisites'
New-Item -ItemType Directory -Path $prerequisites -Force | Out-Null
Copy-Item -LiteralPath 'C:/Program Files/Epic Games/UE_5.7/Engine/Extras/Redist/en-us/vc_redist.x64.exe' -Destination $prerequisites
[ordered]@{name=$Name;runtimeRevision=$receipt.runtimeRevision;executableSha256=$receipt.executableSha256;platform='Windows x64';engine='Unreal Engine 5.7.4'} |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $folder 'demo-version.json') -Encoding UTF8
Add-Type -AssemblyName System.IO.Compression.FileSystem
[IO.Compression.ZipFile]::CreateFromDirectory($folder,$zip,[IO.Compression.CompressionLevel]::Optimal,$false)
$hash=(Get-FileHash -LiteralPath $zip).Hash.ToLowerInvariant()
($hash+'  '+[IO.Path]::GetFileName($zip)) | Set-Content -LiteralPath ($zip+'.sha256') -Encoding ASCII
Write-Output ('CHUCK_DEMO_ARCHIVE path='+$zip+' bytes='+(Get-Item -LiteralPath $zip).Length+' sha256='+$hash)
