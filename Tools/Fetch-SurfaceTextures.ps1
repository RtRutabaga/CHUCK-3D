# Optional acquisition step. CC0 sources, exact assets only; no installation.
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$destination=Join-Path $projectRoot 'SourceAssets\Surfaces\PolyHaven'
New-Item -ItemType Directory -Force -Path $destination | Out-Null
$records=@()
foreach ($assetId in @('grey_stone_path','brown_planks_03')) {
    foreach ($map in @('diff','arm','nor_dx')) {
        $filename="${assetId}_${map}_2k.jpg"
        $url="https://dl.polyhaven.org/file/ph-assets/Textures/jpg/2k/$assetId/$filename"
        $target=Join-Path $destination $filename
        if (!(Test-Path -LiteralPath $target)) { Invoke-WebRequest -Uri $url -OutFile $target }
        $records += [ordered]@{file=$filename; asset=$assetId; map=$map; resolution=2048; source=$url; license='CC0-1.0'; sha256=(Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash; bytes=(Get-Item -LiteralPath $target).Length}
    }
}
$manifest=[ordered]@{provider='Poly Haven'; license_url='https://polyhaven.com/license'; assets=$records}
[IO.File]::WriteAllText((Join-Path $destination 'manifest.json'),($manifest | ConvertTo-Json -Depth 6)+"`n")
$records | ForEach-Object { [pscustomobject]$_ } | Select-Object file,bytes
