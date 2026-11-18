# CC0 scans for the Feywild (Claude, 2026-11-18; docs/regions/FEYWILD.md, decision 4); no software installation.
# Poly Haven at 2K: moss_01, grass_medium_02 and celandine_01 (models with their maps), brown_mud_leaves_01 (the path's
# ground). Bark, forest ground, fern and mossy rocks are shared with SourceAssets/Setting/ChultNature.
# The first run records each file's provider MD5 and SHA-256 in the manifest; later runs check against it.
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$root = Split-Path $PSScriptRoot -Parent
$folder = Join-Path $root 'SourceAssets/Setting/Feywild'
New-Item -ItemType Directory -Force -Path $folder | Out-Null
$manifestPath = Join-Path $folder 'manifest.json'
$headers = @{ 'User-Agent' = 'asset-fetch-script' }
$records = @()
if (Test-Path -LiteralPath $manifestPath) { $records = @((Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json).assets) }
if ($records.Count -eq 0) {
    $wanted = [ordered]@{
        moss_01             = @('Diffuse', 'nor_dx', 'arm', 'Alpha', 'blend')
        grass_medium_02     = @('Diffuse', 'nor_dx', 'arm', 'Alpha', 'blend')
        celandine_01        = @('Diffuse', 'nor_dx', 'arm', 'Alpha', 'blend')
        brown_mud_leaves_01 = @('Diffuse', 'nor_dx', 'arm')
    }
    foreach ($id in $wanted.Keys) {
        $files = Invoke-RestMethod -Headers $headers -Uri "https://api.polyhaven.com/files/$id"
        $info = Invoke-RestMethod -Headers $headers -Uri "https://api.polyhaven.com/info/$id"
        $authors = ($info.authors.PSObject.Properties.Name -join ', ')
        foreach ($role in $wanted[$id]) {
            $sizes = $files.$role.'2k'
            $item = if ($role -eq 'blend') { $sizes.blend } elseif ($role -eq 'Alpha') { if ($sizes.png) { $sizes.png } else { $sizes.jpg } } else { $sizes.jpg }
            if (!$item.url) { throw "No 2K file for $id/$role" }
            $records += [pscustomobject][ordered]@{ provider = 'Poly Haven'; asset = $id; authors = $authors; role = $role; file = ($item.url.Split('/')[-1]); source = $item.url; md5 = $item.md5; bytes = $item.size; license = 'CC0-1.0' }
        }
    }
}
foreach ($record in $records) {
    $target = Join-Path $folder $record.file
    if (!(Test-Path -LiteralPath $target) -or (Get-FileHash -LiteralPath $target -Algorithm MD5).Hash -ne $record.md5.ToUpper()) {
        Invoke-WebRequest -Headers $headers -Uri $record.source -OutFile $target
    }
    if ((Get-FileHash -LiteralPath $target -Algorithm MD5).Hash -ne $record.md5.ToUpper()) { throw "Provider hash mismatch: $($record.file)" }
    $hash = (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash
    if ($record.PSObject.Properties['sha256'] -and $record.sha256 -and $record.sha256 -ne $hash) { throw "SHA256 mismatch: $($record.file)" }
    $record | Add-Member -NotePropertyName sha256 -NotePropertyValue $hash -Force
    Write-Output "CHUCK_FEYWILD_SOURCE_VERIFIED $($record.file) bytes=$($record.bytes)"
}
$manifest = [ordered]@{ providers = @('Poly Haven (https://polyhaven.com/license)'); license = 'CC0-1.0'; resolution = 2048; assets = $records }
[IO.File]::WriteAllText($manifestPath, ($manifest | ConvertTo-Json -Depth 8) + "`n")
