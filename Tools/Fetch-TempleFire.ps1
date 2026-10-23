# The temple fire jets' flipbooks (Claude, 2026-10-22; docs/CHULT-TEMPLE-RUN.md): three CC0
# Houdini-simulated sequences from Unity Labs' free VFX image sequences
# (https://unity.com/blog/engine-platform/free-vfx-image-sequences-flipbooks). Downloads each
# zip, checks its SHA-256, and extracts only the one flipbook used. No software installation.
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$taskRoot=Split-Path $PSScriptRoot -Parent
$folder=Join-Path $taskRoot 'SourceAssets/Setting/TempleFire'
$cache=Join-Path $taskRoot 'Local/UnityLabsFlipbooks/zips'
New-Item -ItemType Directory -Force -Path $folder,$cache | Out-Null
$items=@(
    @{asset='FireBall01';file='FireBall01_8x8.tga';grid='8x8';role='the jet''s rolling fire puffs (additive)';zip='f60de3f2ce56f941d6f089ea4ea93e3ae51c63fac84c1aa0d6615022f8647010';tga='74eaf8aea246368edc30abd5fdc29db3e12b0275a0de7490020ea004d1b23d43'},
    @{asset='Flame02';file='Flame02_16x4.tga';grid='16x4';role='the pilot flame licking at the mouth (the tell)';zip='f37b12cdae090591d537ed3bb50396f03833a49864362bf665d6817b7282c954';tga='6ae2158bc06e0b035196db2695c19e7cb29a23f59b345d4c5c2764c6c10ce3a6'},
    @{asset='WispySmoke01';file='WispySmoke01_8x8.tga';grid='8x8';role='smoke left by the burnt-out puffs';zip='f9bdc88948ff18a7d3e6332b68f2f2ff17e9a64c6feb821e05388f4f23dbcff0';tga='c3f5d3be66882f05bad5b22d300fa0c9bfbac0340ed5dc48977f91d95f842143'}
)
$records=@()
foreach($i in $items) {
    $url="https://unity3d.com/files/labs/downloads/vfx/assets01/$($i.asset)/$($i.asset)-flipbooks.zip"
    $zip=Join-Path $cache "$($i.asset).zip"
    if(!(Test-Path -LiteralPath $zip) -or (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash -ne $i.zip) { Invoke-WebRequest -Uri $url -OutFile $zip }
    if((Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash -ne $i.zip) { throw "Provider hash mismatch: $($i.asset).zip" }
    $target=Join-Path $folder $i.file
    if(!(Test-Path -LiteralPath $target) -or (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $i.tga) {
        $archive=[IO.Compression.ZipFile]::OpenRead($zip)
        try {
            $entry=$archive.Entries | Where-Object { $_.FullName -eq $i.file }
            if(!$entry) { throw "No $($i.file) in $($i.asset).zip" }
            [IO.Compression.ZipFileExtensions]::ExtractToFile($entry,$target,$true)
        } finally { $archive.Dispose() }
    }
    if((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $i.tga) { throw "Flipbook hash mismatch: $($i.file)" }
    $records+=[ordered]@{asset=$i.asset;file=$i.file;grid=$i.grid;role=$i.role;source=$url;zip_sha256=$i.zip;sha256=$i.tga;license='CC0-1.0'}
    Write-Output "CHUCK_TEMPLE_FIRE_SOURCE_VERIFIED $($i.file)"
}
$manifest=[ordered]@{provider='Unity Labs (Unity Technologies), free VFX image sequences and flipbooks';page='https://unity.com/blog/engine-platform/free-vfx-image-sequences-flipbooks';license_url='https://creativecommons.org/publicdomain/zero/1.0/';assets=$records}
[IO.File]::WriteAllText((Join-Path $folder 'manifest.json'),($manifest|ConvertTo-Json -Depth 8)+[Environment]::NewLine,[Text.UTF8Encoding]::new($false))
