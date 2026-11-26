# CC0 recordings for the Feywild's creatures and mechanics (Claude, 2026-11-22; the user: "Download and implement sounds for
# things that need sounds"); no software installation. BigSoundBank (Joseph Sardin; https://bigsoundbank.com/licenses.html,
# CC0): MP3s into SourceAssets/Audio/SFX/Feywild/Source/BigSoundBank/, each with its page, URL and SHA-256 in the manifest.
# Tools/build_fey_sfx.py cuts and mixes them into SFX_Fey*.wav; Tools/import_fey_sfx.py imports those.
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$root = Split-Path $PSScriptRoot -Parent
$folder = Join-Path $root 'SourceAssets/Audio/SFX/Feywild/Source/BigSoundBank'
New-Item -ItemType Directory -Force -Path $folder | Out-Null
$manifestPath = Join-Path (Split-Path $folder -Parent) 'manifest.json'
$headers = @{ 'User-Agent' = 'asset-fetch-script' }
$wanted = [ordered]@{
    '1881' = 'cat-roar-1'; '1882' = 'cat-roar-2'; '1883' = 'cat-roar-3'; '1884' = 'cat-roar-4'
    '1885' = 'growling-cat-1'; '1886' = 'growling-cat-2'; '1887' = 'growling-cat-3'; '1888' = 'growling-cat-4'
    '2949' = 'whip-crack-1'; '2950' = 'whip-crack-2'; '2951' = 'whip-crack-3'
    '0030' = 'fireplace-1'; '0491' = 'big-broth-in-a-pot'
    '0789' = 'chattering-teeth'; '1038' = 'tongue-clicks'; '1299' = 'broken-twigs-1'
    '2891' = 'flight-bird-1'; '0476' = 'flight-of-a-pigeon'
    '2087' = 'chimes-butterfly-1'; '2088' = 'chimes-butterfly-2'; '0137' = 'feet-in-leaves'
    '0211' = 'champagne-cork'; '1244' = 'straw-suction-1'
    '0150' = 'water-bubbles'; '1529' = 'splash-small-1'
    '2093' = 'delirious-laughter-3'; '2096' = 'delirious-laughter-6'
    '1000' = 'bumblebee-1'; '1001' = 'bumblebee-2'   # (stage 6: the bees on the maze's hedge tops)
    '1376' = 'bunny-hutch'   # (2026-11-25: rabbits stamping their feet, for the great hare's warning thump and its bound)
}
$records = @()
foreach ($id in $wanted.Keys) {
    $file = "bsb_$id.mp3"; $target = Join-Path $folder $file
    $url = "https://bigsoundbank.com/UPLOAD/mp3/$id.mp3"
    if (!(Test-Path -LiteralPath $target)) { Invoke-WebRequest -Headers $headers -Uri $url -OutFile $target }
    $hash = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash.ToLower()
    $records += [pscustomobject][ordered]@{ file = "BigSoundBank/$file"; title = $wanted[$id]; author = 'Joseph Sardin (BigSoundBank)'; license = 'CC0 1.0';
        page = "https://bigsoundbank.com/$($wanted[$id])-s$id.html"; url = $url; bytes = (Get-Item -LiteralPath $target).Length; sha256 = $hash; downloaded = '2026-11-22 (Claude, at the user''s request)' }
    Write-Output "CHUCK_FEY_SOUND_SOURCE $file bytes=$((Get-Item -LiteralPath $target).Length)"
}
[IO.File]::WriteAllText($manifestPath, ($records | ConvertTo-Json -Depth 4) + "`n")
