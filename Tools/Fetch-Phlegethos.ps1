# CC0 textures for Phlegethos (Claude, 2026-11-14; docs/regions/PHLEGETHOS.md); no software installation.
# Poly Haven: dark_rock and dark_rock_02 (basalt), volcanic_rock_tiles (the worn path, the fortress paving). 2K JPG
# diffuse, normal (DirectX) and ARM. ambientCG: Lava004 (the first molten surface), 2K JPG colour, normal (DirectX) and
# roughness; Lava001 and Lava005 (the lake and the flows, 2026-11-16), colour, emission and normal (DirectX); from zips. The first run records each file's provider MD5 (Poly Haven) or SHA-256 in the manifest; later
# runs check against it.
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem
$root=Split-Path $PSScriptRoot -Parent
$folder=Join-Path $root 'SourceAssets/Setting/Phlegethos'
$cache=Join-Path $root 'Local/AmbientCG'
New-Item -ItemType Directory -Force -Path $folder,$cache | Out-Null
$manifestPath=Join-Path $folder 'manifest.json'
$records=@()
if(Test-Path -LiteralPath $manifestPath) { $records=@((Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json).assets) }
if($records.Count -eq 0) {
    foreach($id in @('dark_rock','dark_rock_02','volcanic_rock_tiles')) {
        $files=Invoke-RestMethod -Uri "https://api.polyhaven.com/files/$id"
        foreach($role in @('Diffuse','nor_dx','arm')) {
            $item=$files.$role.'2k'.jpg
            if(!$item.url) { throw "No 2K jpg for $id/$role" }
            $records+= [pscustomobject][ordered]@{provider='Poly Haven'; asset=$id; role=$role; file=($item.url.Split('/')[-1]); source=$item.url; md5=$item.md5; bytes=$item.size; license='CC0-1.0'}
        }
    }
    foreach($r in @(@{role='Diffuse';suffix='Color'},@{role='nor_dx';suffix='NormalDX'},@{role='rough';suffix='Roughness'})) {
        $records+= [pscustomobject][ordered]@{provider='ambientCG'; asset='Lava004'; role=$r.role; file="Lava004_2K-JPG_$($r.suffix).jpg"; source='https://ambientcg.com/get?file=Lava004_2K-JPG.zip'; license='CC0-1.0'}
    }
}
# (2026-11-16) ambientCG Lava001 (the lake: dark crust over molten seams) and Lava005 (streaky molten flow: rivers, falls),
# with their emission maps; added to an existing manifest.
foreach($id in @('Lava001','Lava005')) {
    if(@($records | Where-Object { $_.asset -eq $id }).Count -gt 0) { continue }
    foreach($r in @(@{role='Diffuse';suffix='Color'},@{role='emit';suffix='Emission'},@{role='nor_dx';suffix='NormalDX'})) {
        $records+= [pscustomobject][ordered]@{provider='ambientCG'; asset=$id; role=$r.role; file="${id}_2K-JPG_$($r.suffix).jpg"; source="https://ambientcg.com/get?file=${id}_2K-JPG.zip"; license='CC0-1.0'}
    }
}
foreach($record in $records) {
    $target=Join-Path $folder $record.file
    if($record.provider -eq 'Poly Haven') {
        if(!(Test-Path -LiteralPath $target) -or (Get-FileHash -LiteralPath $target -Algorithm MD5).Hash -ne $record.md5) { Invoke-WebRequest -Uri $record.source -OutFile $target }
        if((Get-FileHash -LiteralPath $target -Algorithm MD5).Hash -ne $record.md5) { throw "Provider hash mismatch: $($record.file)" }
    } else {
        if(!(Test-Path -LiteralPath $target)) {
            $zip=Join-Path $cache "$($record.asset)_2K-JPG.zip"
            if(!(Test-Path -LiteralPath $zip)) { $ProgressPreference='SilentlyContinue'; Invoke-WebRequest -Uri $record.source -OutFile $zip }
            $archive=[IO.Compression.ZipFile]::OpenRead($zip)
            try {
                $entry=$archive.Entries | Where-Object { $_.Name -eq $record.file }
                if(!$entry) { throw "No $($record.file) in $zip" }
                [IO.Compression.ZipFileExtensions]::ExtractToFile($entry,$target,$true)
            } finally { $archive.Dispose() }
        }
    }
    $hash=(Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
    if($record.PSObject.Properties['sha256'] -and $record.sha256 -and $record.sha256 -ne $hash) { throw "SHA256 mismatch: $($record.file)" }
    $record | Add-Member -NotePropertyName sha256 -NotePropertyValue $hash -Force
    if(!$record.PSObject.Properties['bytes'] -or !$record.bytes) { $record | Add-Member -NotePropertyName bytes -NotePropertyValue (Get-Item -LiteralPath $target).Length -Force }
    Write-Output "CHUCK_PHLEGETHOS_SOURCE_VERIFIED $($record.file) bytes=$($record.bytes)"
}
$manifest=[ordered]@{providers=@('Poly Haven (https://polyhaven.com/license)','ambientCG (https://docs.ambientcg.com/license/)'); license='CC0-1.0'; resolution=2048; assets=$records}
[IO.File]::WriteAllText($manifestPath,($manifest | ConvertTo-Json -Depth 8)+"`n")
