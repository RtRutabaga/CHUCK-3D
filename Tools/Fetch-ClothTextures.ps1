# Clothing fabrics for the human NPCs (user-approved CC0 download, 2026-09-30).
# Same shape as Fetch-SurfaceTextures.ps1 (Codex): exact Poly Haven assets,
# 2K colour / AO-roughness-metal / DirectX normal, no installation, manifest
# with hashes. Medieval-appropriate cloth: linen shirts, wool (caban) breeches
# and tunics, a knit (boucle) for caps, brown leather for jerkins, belts and boots,
# plain worn steel plate for helmets.
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$destination=Join-Path $projectRoot 'SourceAssets\Surfaces\Cloth'
New-Item -ItemType Directory -Force -Path $destination | Out-Null
$records=@()
foreach ($assetId in @('rough_linen','caban','wool_boucle','brown_leather','metal_plate_02')) {
    # Exact file URLs from the Poly Haven API (colour maps aren't always named "diff").
    $files=Invoke-RestMethod -Uri "https://api.polyhaven.com/files/$assetId" -UseBasicParsing
    # Real-world size of one texture repeat (mm), so cloth UVs can match it.
    $sizeCm=[Math]::Round((((Invoke-RestMethod -Uri "https://api.polyhaven.com/info/$assetId" -UseBasicParsing).dimensions | Measure-Object -Average).Average)/10,1)
    foreach ($pair in @(@('diff','Diffuse'),@('arm','arm'),@('nor_dx','nor_dx'))) {
        $map=$pair[0]
        $filename="${assetId}_${map}_2k.jpg"
        $url=$files.($pair[1]).'2k'.jpg.url
        $target=Join-Path $destination $filename
        if (!(Test-Path -LiteralPath $target)) { Invoke-WebRequest -Uri $url -OutFile $target -UseBasicParsing }
        $records += [ordered]@{file=$filename; asset=$assetId; map=$map; resolution=2048; source=$url; size_cm=$sizeCm; license='CC0-1.0'; sha256=(Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash; bytes=(Get-Item -LiteralPath $target).Length}
    }
}
$manifest=[ordered]@{provider='Poly Haven'; license_url='https://polyhaven.com/license'; assets=$records}
[IO.File]::WriteAllText((Join-Path $destination 'manifest.json'),($manifest | ConvertTo-Json -Depth 6)+"`n")
$records | ForEach-Object { [pscustomobject]$_ } | Select-Object file,bytes
