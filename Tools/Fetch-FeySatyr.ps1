# Marble satyr statues for the Great Hedge Maze (Claude, 2026-11-23; the user: "some statues to be detailed marble statues of
# Satyrs, feel free to download and use any free resources"); no software installation.
#  - The scan: SMK, Statens Museum for Kunst, Copenhagen: KAS460, "Satyr med fod-pauke og baekkener. Medici Faunen", a plaster
#    cast (Royal Cast Collection) of the dancing satyr in the Uffizi, 141.5 cm. SMK marks it public domain (Public Domain Mark
#    1.0, https://open.smk.dk/artwork/image/KAS460). Its 20 MB STL (the smaller of SMK's two) goes to
#    SourceAssets/Props/Feywild/Satyr/Source/ (not in Git; Tools/build_fey_satyr.py makes the committed FBX from it).
#  - Marble: Poly Haven marble_01 (CC0) at 2K: colour, normal (DirectX), roughness.
# Each file's SHA-256 goes into the manifest.
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$root = Split-Path $PSScriptRoot -Parent
$folder = Join-Path $root 'SourceAssets/Props/Feywild/Satyr'
$src = Join-Path $folder 'Source'
New-Item -ItemType Directory -Force -Path $src | Out-Null
$headers = @{ 'User-Agent' = 'asset-fetch-script' }
$files = @(
    @{ url = 'https://api.smk.dk/api/v1/download-3d/44558j590_KAS460_small.stl'; out = (Join-Path $src 'KAS460_small.stl') },
    @{ url = 'https://api.smk.dk/api/v1/download-3d/cn69m904m_KAS350_small.stl'; out = (Join-Path $src 'KAS350_small.stl') },   # Silenus with the infant Dionysus (smiling at him)
    @{ url = 'https://api.smk.dk/api/v1/download-3d/8w32rb332_KAS208_small.stl'; out = (Join-Path $src 'KAS208_small.stl') },   # the Barberini Faun (asleep, brow drawn)
    @{ url = 'https://dl.polyhaven.org/file/ph-assets/Textures/jpg/2k/marble_01/marble_01_diff_2k.jpg'; out = (Join-Path $folder 'marble_01_diff_2k.jpg') },
    @{ url = 'https://dl.polyhaven.org/file/ph-assets/Textures/jpg/2k/marble_01/marble_01_nor_dx_2k.jpg'; out = (Join-Path $folder 'marble_01_nor_dx_2k.jpg') },
    @{ url = 'https://dl.polyhaven.org/file/ph-assets/Textures/jpg/2k/marble_01/marble_01_rough_2k.jpg'; out = (Join-Path $folder 'marble_01_rough_2k.jpg') }
)
$kept = @()
foreach ($f in $files) {
    Invoke-WebRequest -Headers $headers -Uri $f.url -OutFile $f.out
    $kept += [ordered]@{ file = [IO.Path]::GetFileName($f.out); source = $f.url; bytes = (Get-Item -LiteralPath $f.out).Length; sha256 = (Get-FileHash -LiteralPath $f.out -Algorithm SHA256).Hash.ToLower() }
    Write-Output "CHUCK_FEY_SATYR_SOURCE $([IO.Path]::GetFileName($f.out)) $((Get-Item -LiteralPath $f.out).Length)"
}
$manifest = [ordered]@{
    scan = 'SMK plaster casts, Public Domain Mark 1.0: KAS460 Medici Faun (dancing satyr), KAS350 Silenus with the infant Dionysus, KAS208 Barberini Faun (https://open.smk.dk/artwork/image/KAS460 etc.)'
    marble = 'Poly Haven marble_01, CC0-1.0 (https://polyhaven.com/a/marble_01)'
    downloaded = '2026-11-23 (Claude, at the user''s request)'; files = $kept }
[IO.File]::WriteAllText((Join-Path $folder 'manifest.json'), ($manifest | ConvertTo-Json -Depth 4) + "`n")
