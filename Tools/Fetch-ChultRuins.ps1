# Exact 2K CC0 acquisition for the Chult ruin finish. No software installation.
$ErrorActionPreference='Stop'
$taskRoot=Split-Path $PSScriptRoot -Parent
$folder=Join-Path $taskRoot 'SourceAssets/Setting/ChultRuins'
New-Item -ItemType Directory -Force -Path $folder | Out-Null
$records=@()
foreach($id in @('rock_wall_08','mossy_stone_wall','rabdentse_ruins_wall')) {
    $files=Invoke-RestMethod -Uri "https://api.polyhaven.com/files/$id"
    foreach($role in @('Diffuse','arm','nor_dx')) {
        $item=$files.$role.'2k'.jpg
        if(!$item.url) { throw "No 2K jpg for $id/$role" }
        $name=$item.url.Split('/')[-1]; $target=Join-Path $folder $name
        if(!(Test-Path -LiteralPath $target) -or (Get-FileHash -LiteralPath $target -Algorithm MD5).Hash -ne $item.md5) {
            Invoke-WebRequest -Uri $item.url -OutFile $target
        }
        if((Get-FileHash -LiteralPath $target -Algorithm MD5).Hash -ne $item.md5) { throw "Provider hash mismatch: $name" }
        $records+=[ordered]@{asset=$id;role=$role;file=$name;source=$item.url;bytes=$item.size;md5=$item.md5;sha256=(Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash;license='CC0-1.0'}
        Write-Output "CHUCK_RUIN_SOURCE_VERIFIED $name bytes=$($item.size)"
    }
}
$manifest=[ordered]@{provider='Poly Haven';license_url='https://polyhaven.com/license';resolution=2048;assets=$records}
[IO.File]::WriteAllText((Join-Path $folder 'manifest.json'),($manifest|ConvertTo-Json -Depth 8)+[Environment]::NewLine,[Text.UTF8Encoding]::new($false))
