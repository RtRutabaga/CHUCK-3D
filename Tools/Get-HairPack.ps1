# Fetches the MakeHuman community's hair editor asset pack (CC0; hair.blend and fur.blend by
# Tomas Klecer: geometry-node hair curves on the MakeHuman base mesh) into Local/HairPack, for
# Tools/npc_strands.py (the humans' and heroes' strand hair). Data only: nothing is installed.
# Listed at https://static.makehumancommunity.org/assets/assetpacks.html (Hair editor, CC0).
param([switch]$Force)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$dir = Join-Path $root 'Local\HairPack'
$zip = Join-Path $dir 'haireditor.zip'
$blend = Join-Path $dir 'hair\haireditor\hair.blend'
$zipHash = 'E47ADE8E58E31C12C1331FE96801059CB1BF0178F064B9BD4A63F343BCECBE72'
$blendHash = '8566A128ADE918C507FABDCA883C49FAC7D744FE840B9EB28A98917B5AD3F080'
if ((Test-Path $blend) -and -not $Force -and (Get-FileHash $blend -Algorithm SHA256).Hash -eq $blendHash) {
    Write-Output "CHUCK_HAIRPACK_READY $blend"; return
}
New-Item -ItemType Directory -Force $dir | Out-Null
foreach ($url in 'https://files.makehumancommunity.org/functional/haireditor.zip', 'https://files2.makehumancommunity.org/functional/haireditor.zip') {
    try { Invoke-WebRequest $url -OutFile $zip -UseBasicParsing; break } catch { Write-Warning "$url failed: $_" }
}
if ((Get-FileHash $zip -Algorithm SHA256).Hash -ne $zipHash) { throw "haireditor.zip does not match the checked pack ($zipHash); not unpacked." }
Expand-Archive $zip -DestinationPath $dir -Force
if ((Get-FileHash $blend -Algorithm SHA256).Hash -ne $blendHash) { throw 'hair.blend does not match the checked pack.' }
Write-Output "CHUCK_HAIRPACK_READY $blend"
