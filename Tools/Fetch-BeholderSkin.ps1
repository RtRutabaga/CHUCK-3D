# The beholder's skin grain (user 2026-10-24: "use free resources where helpful").
# ambientCG Leather037 (CC0): pebbled leather grain with a fine wrinkle network. Its displacement map,
# scaled up to centimetre-sized cells, is the fine detail baked into the beholder's skin
# (Tools/build_beholder.py); its normal map is kept for reference. No installation.
# Manifest with the source URL and hashes in SourceAssets/Surfaces/Beholder/manifest.json.
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$destination=Join-Path $projectRoot 'SourceAssets\Surfaces\Beholder'
New-Item -ItemType Directory -Force -Path $destination | Out-Null
$work=Join-Path $projectRoot 'Local\BeholderSurfaces'
New-Item -ItemType Directory -Force -Path $work | Out-Null
$records=@()
foreach ($assetId in @('Leather037')) {
    $url="https://ambientcg.com/get?file=${assetId}_2K-JPG.zip"
    $zip=Join-Path $work "$assetId.zip"
    if (!(Test-Path -LiteralPath $zip)) { Invoke-WebRequest -Uri $url -OutFile $zip -UseBasicParsing }
    $unpacked=Join-Path $work $assetId
    if (!(Test-Path -LiteralPath $unpacked)) { Expand-Archive -LiteralPath $zip -DestinationPath $unpacked }
    $name=$assetId.ToLower()
    foreach ($pair in @(@('Displacement','disp'),@('NormalGL','nor_gl'))) {
        $file="${name}_$($pair[1])_2k.jpg"
        $target=Join-Path $destination $file
        Copy-Item -LiteralPath (Join-Path $unpacked "${assetId}_2K-JPG_$($pair[0]).jpg") -Destination $target -Force
        $records += [ordered]@{file=$file; asset=$name; map=$pair[1]; resolution=2048; source=$url; provider='ambientCG'; license='CC0-1.0'; sha256=(Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash; bytes=(Get-Item -LiteralPath $target).Length}
    }
}
$manifest=[ordered]@{providers=@('ambientCG (https://ambientcg.com, CC0)'); assets=$records}
[IO.File]::WriteAllText((Join-Path $destination 'manifest.json'),($manifest | ConvertTo-Json -Depth 6)+"`n")
$records | ForEach-Object { [pscustomobject]$_ } | Select-Object file,bytes
