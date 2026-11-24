# The hag's cottage in the Deepwood (Claude, 2026-11-23; the user: "build a Hag's cottage in the deep wood just off of the
# trail ... download and use whatever free resources would be helpful"); no software installation.
#  - Poly Haven thatch_roof_angled (CC0): the roof's thatch, 2K colour, normal (DirectX), roughness.
#  - Poly Haven worn_mossy_plasterwall (CC0): the wattle-and-daub walls, the same three.
# Into SourceAssets/Props/Feywild/Hag/ with a manifest of each file's SHA-256.
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$root = Split-Path $PSScriptRoot -Parent
$folder = Join-Path $root 'SourceAssets/Props/Feywild/Hag'
New-Item -ItemType Directory -Force -Path $folder | Out-Null
$headers = @{ 'User-Agent' = 'asset-fetch-script' }
$kept = @()
foreach ($asset in 'thatch_roof_angled', 'worn_mossy_plasterwall') {
    foreach ($map in 'diff', 'nor_dx', 'rough') {
        $name = "${asset}_${map}_2k.jpg"; $url = "https://dl.polyhaven.org/file/ph-assets/Textures/jpg/2k/$asset/$name"; $out = Join-Path $folder $name
        Invoke-WebRequest -Headers $headers -Uri $url -OutFile $out
        $kept += [ordered]@{ file = $name; source = $url; bytes = (Get-Item -LiteralPath $out).Length; sha256 = (Get-FileHash -LiteralPath $out -Algorithm SHA256).Hash.ToLower() }
        Write-Output "CHUCK_FEY_HAG_SOURCE $name $((Get-Item -LiteralPath $out).Length)"
    }
}
$manifest = [ordered]@{
    textures = 'Poly Haven thatch_roof_angled and worn_mossy_plasterwall, CC0-1.0 (https://polyhaven.com/a/thatch_roof_angled, https://polyhaven.com/a/worn_mossy_plasterwall)'
    downloaded = '2026-11-23 (Claude, at the user''s request)'; files = $kept }
[IO.File]::WriteAllText((Join-Path $folder 'manifest.json'), ($manifest | ConvertTo-Json -Depth 4) + "`n")
