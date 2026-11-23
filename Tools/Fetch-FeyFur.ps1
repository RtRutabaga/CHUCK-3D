# CC0 fur for the displacer beast (Claude, 2026-11-22; the user: "free resources to make displaced beast look better");
# no software installation. ambientCG Fabric084 (blue fur, https://ambientcg.com/view?id=Fabric084, CC0 1.0) at 2K:
# colour, DirectX normal, roughness and occlusion into SourceAssets/Enemies/Feywild/Fur/, with the zip's SHA-256 in the
# manifest. Tools/import_fey_creatures.py imports them and builds M_FeyFur on them.
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$root = Split-Path $PSScriptRoot -Parent
$folder = Join-Path $root 'SourceAssets/Enemies/Feywild/Fur'
New-Item -ItemType Directory -Force -Path $folder | Out-Null
$headers = @{ 'User-Agent' = 'asset-fetch-script' }
$zip = Join-Path ([IO.Path]::GetTempPath()) 'Fabric084_2K-JPG.zip'
$url = 'https://ambientcg.com/get?file=Fabric084_2K-JPG.zip'
Invoke-WebRequest -Headers $headers -Uri $url -OutFile $zip
$zipHash = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLower()
$unpack = Join-Path ([IO.Path]::GetTempPath()) 'Fabric084_2K'
if (Test-Path -LiteralPath $unpack) { Remove-Item -LiteralPath $unpack -Recurse -Force }
Expand-Archive -LiteralPath $zip -DestinationPath $unpack
$kept = @()
foreach ($map in 'Color', 'NormalDX', 'Roughness', 'AmbientOcclusion') {
    $src = Get-ChildItem -LiteralPath $unpack -Filter "Fabric084_2K-JPG_$map.jpg" | Select-Object -First 1
    if (!$src) { throw "Fabric084: no $map map" }
    Copy-Item -LiteralPath $src.FullName -Destination (Join-Path $folder $src.Name) -Force
    $kept += [ordered]@{ file = $src.Name; sha256 = (Get-FileHash -LiteralPath (Join-Path $folder $src.Name) -Algorithm SHA256).Hash.ToLower() }
    Write-Output "CHUCK_FEY_FUR_SOURCE $($src.Name)"
}
Remove-Item -LiteralPath $zip, $unpack -Recurse -Force
$manifest = [ordered]@{ provider = 'ambientCG (https://ambientcg.com/view?id=Fabric084)'; license = 'CC0-1.0'; source = $url; zip_sha256 = $zipHash; downloaded = '2026-11-22 (Claude, at the user''s request)'; files = $kept }
[IO.File]::WriteAllText((Join-Path $folder 'manifest.json'), ($manifest | ConvertTo-Json -Depth 4) + "`n")
