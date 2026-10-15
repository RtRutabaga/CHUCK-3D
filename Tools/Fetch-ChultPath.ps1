# Exact CC0 acquisition for ancient Chult road remnants (Codex, 2026-10-15).
# Amal Kumar / Poly Haven Stone Pathway; 2K JPG diffuse/normal(DX)/ARM. No installation.
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$folder=Join-Path $root 'SourceAssets/Setting/ChultPath'
New-Item -ItemType Directory -Force -Path $folder | Out-Null
$manifestPath=Join-Path $folder 'manifest.json'
if(Test-Path -LiteralPath $manifestPath) {
    $records=(Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json).assets
} else {
    $records=@()
    foreach($id in @('stone_pathway')) {
        $files=Invoke-RestMethod -Uri "https://api.polyhaven.com/files/$id"
        foreach($role in @('Diffuse','nor_dx','arm')) {
            $item=$files.$role.'2k'.jpg
            if(!$item.url) { throw "No 2K jpg for $id/$role" }
            $records+= [ordered]@{asset=$id; role=$role; file=($item.url.Split('/')[-1]); source=$item.url; md5=$item.md5; bytes=$item.size; license='CC0-1.0'}
        }
    }
}
foreach($record in $records) {
    $target=Join-Path $folder $record.file
    if(!(Test-Path -LiteralPath $target) -or (Get-FileHash -LiteralPath $target -Algorithm MD5).Hash -ne $record.md5) {
        Invoke-WebRequest -Uri $record.source -OutFile $target
    }
    if((Get-FileHash -LiteralPath $target -Algorithm MD5).Hash -ne $record.md5) { throw "Provider hash mismatch: $($record.file)" }
    $hash=(Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
    if($record -is [System.Collections.IDictionary]) { $record.sha256=$hash }
    elseif($record.sha256 -and $record.sha256 -ne $hash) { throw "SHA256 mismatch: $($record.file)" }
    elseif(!$record.sha256) { $record | Add-Member -NotePropertyName sha256 -NotePropertyValue $hash }
    Write-Output "CHUCK_PATH_SOURCE_VERIFIED $($record.file) bytes=$($record.bytes)"
}
$manifest=[ordered]@{provider='Poly Haven'; license_url='https://polyhaven.com/license'; resolution=2048; assets=$records}
[IO.File]::WriteAllText($manifestPath,($manifest | ConvertTo-Json -Depth 8)+"`n")
