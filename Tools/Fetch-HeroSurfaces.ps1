# Surfaces for the temple heroes (user 2026-10-22: "download and install whatever you see fits best").
# CC0 only, no installation. 2K colour / roughness / metal (or ARM) / DirectX normal.
#   ambientCG: Metal054A (hand-worked iron: the plate), Metal038 (dark scratched steel: mail rings, rims, blade spine)
#   Poly Haven: velour_velvet (the wizard's robe), poly_wool_herringbone (surcoat, mantle),
#               fabric_leather_02 (straps, jerkin, scabbard), scuba_suede (gloves, bracer), hessian_230 (padding)
# Manifest with source URLs, real-world sizes and hashes in SourceAssets/Surfaces/Heroes/manifest.json.
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$destination=Join-Path $projectRoot 'SourceAssets\Surfaces\Heroes'
New-Item -ItemType Directory -Force -Path $destination | Out-Null
$work=Join-Path $projectRoot 'Local\HeroSurfaces'
New-Item -ItemType Directory -Force -Path $work | Out-Null
$records=@()
function Add-Record($file,$asset,$map,$url,$sizeCm,$provider) {
    $target=Join-Path $destination $file
    $script:records += [ordered]@{file=$file; asset=$asset; map=$map; resolution=2048; source=$url; size_cm=$sizeCm; provider=$provider; license='CC0-1.0'; sha256=(Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash; bytes=(Get-Item -LiteralPath $target).Length}
}
# ambientCG: the zip's Color / Roughness / Metalness / NormalDX, renamed to the project's scheme.
# Real-world size: ambientCG metals are photographed at about 1 m a repeat (approximate; their pages give none).
foreach ($assetId in @('Metal054A','Metal038')) {
    $url="https://ambientcg.com/get?file=${assetId}_2K-JPG.zip"
    $zip=Join-Path $work "$assetId.zip"
    if (!(Test-Path -LiteralPath $zip)) { Invoke-WebRequest -Uri $url -OutFile $zip -UseBasicParsing }
    $unpacked=Join-Path $work $assetId
    if (!(Test-Path -LiteralPath $unpacked)) { Expand-Archive -LiteralPath $zip -DestinationPath $unpacked }
    $name=$assetId.ToLower()
    foreach ($pair in @(@('Color','diff'),@('Roughness','rough'),@('Metalness','metal'),@('NormalDX','nor_dx'))) {
        $file="${name}_$($pair[1])_2k.jpg"
        Copy-Item -LiteralPath (Join-Path $unpacked "${assetId}_2K-JPG_$($pair[0]).jpg") -Destination (Join-Path $destination $file) -Force
        Add-Record $file $name $pair[1] $url 100.0 'ambientCG'
    }
}
foreach ($assetId in @('velour_velvet','poly_wool_herringbone','fabric_leather_02','scuba_suede','hessian_230')) {
    $files=Invoke-RestMethod -Uri "https://api.polyhaven.com/files/$assetId" -UseBasicParsing
    $sizeCm=[Math]::Round((((Invoke-RestMethod -Uri "https://api.polyhaven.com/info/$assetId" -UseBasicParsing).dimensions | Measure-Object -Average).Average)/10,1)
    foreach ($pair in @(@('diff','Diffuse'),@('arm','arm'),@('nor_dx','nor_dx'))) {
        $file="${assetId}_$($pair[0])_2k.jpg"
        $url=$files.($pair[1]).'2k'.jpg.url
        $target=Join-Path $destination $file
        if (!(Test-Path -LiteralPath $target)) { Invoke-WebRequest -Uri $url -OutFile $target -UseBasicParsing }
        Add-Record $file $assetId $pair[0] $url $sizeCm 'Poly Haven'
    }
}
$manifest=[ordered]@{providers=@('ambientCG (https://ambientcg.com, CC0)','Poly Haven (https://polyhaven.com/license, CC0)'); assets=$records}
[IO.File]::WriteAllText((Join-Path $destination 'manifest.json'),($manifest | ConvertTo-Json -Depth 6)+"`n")
$records | ForEach-Object { [pscustomobject]$_ } | Select-Object file,bytes
