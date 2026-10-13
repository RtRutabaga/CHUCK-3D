# Exact CC0 asset acquisition; no software installation.
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$folder=Join-Path $root 'SourceAssets/Setting/ChultNature'
New-Item -ItemType Directory -Force -Path $folder | Out-Null
$manifestPath=Join-Path $folder 'manifest.json'
if(Test-Path -LiteralPath $manifestPath) {
    $records=(Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json).assets
} else {
    $records=@()
    foreach($id in @('tree_bark_03','forest_ground_04','fern_02','rock_moss_set_01','island_tree_02')) {
        $files=Invoke-RestMethod -Uri "https://api.polyhaven.com/files/$id"
        $roles=@('Diffuse','arm','nor_dx')
        if($id -eq 'fern_02') { $roles+=@('Alpha','blend') }
        if($id -eq 'rock_moss_set_01') { $roles+='blend' }
        if($id -eq 'island_tree_02') {
            $roles+=@('blend','branches_diff','branches_arm','branches_nor_dx','leaves_diff','leaves_arm','leaves_nor_dx','leaves_alpha')
        }
        foreach($role in $roles) {
            $format=if($role -eq 'blend'){'blend'} elseif($role -match 'alpha|Alpha' -or $role -eq 'branches_diff'){'png'} else {'jpg'}
            $item=$files.$role.'2k'.$format
            if(!$item.url) { throw "No 2K $format for $id/$role" }
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
    Write-Output "CHUCK_NATURE_SOURCE_VERIFIED $($record.file) bytes=$($record.bytes)"
}
$manifest=[ordered]@{provider='Poly Haven'; license_url='https://polyhaven.com/license'; resolution=2048; assets=$records}
[IO.File]::WriteAllText($manifestPath,($manifest | ConvertTo-Json -Depth 8)+"`n")
