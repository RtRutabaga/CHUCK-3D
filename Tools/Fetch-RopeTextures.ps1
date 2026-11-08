param()
# The CC0 rope textures (ambientCG Rope001, 1K JPG): downloads the archive only when a file is missing or differs, keeps the
# three maps the rope material uses and checks every hash in SourceAssets/Surfaces/Rope/manifest.json.
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$folder=Join-Path $root 'SourceAssets/Surfaces/Rope'
$manifest=Get-Content -LiteralPath (Join-Path $folder 'manifest.json') -Raw | ConvertFrom-Json
$stale=@($manifest.assets | Where-Object {
    $t=Join-Path $folder $_.file
    !(Test-Path -LiteralPath $t) -or (Get-FileHash -LiteralPath $t -Algorithm SHA256).Hash -ne $_.sha256 })
if($stale.Count) {
    $work=Join-Path ([IO.Path]::GetTempPath()) ('chuck-rope-'+[guid]::NewGuid())
    New-Item -ItemType Directory -Path $work | Out-Null
    try {
        $zip=Join-Path $work 'Rope001.zip'
        Invoke-WebRequest -Uri $manifest.archive -OutFile $zip
        if((Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash -ne $manifest.archive_sha256) { throw 'Rope archive hash mismatch' }
        Expand-Archive -LiteralPath $zip -DestinationPath (Join-Path $work 'x')
        foreach($record in $stale) { Copy-Item -LiteralPath (Join-Path $work "x/$($record.file)") -Destination (Join-Path $folder $record.file) -Force }
    } finally { Remove-Item -LiteralPath $work -Recurse -Force }
}
foreach($record in $manifest.assets) {
    $target=Join-Path $folder $record.file
    if((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $record.sha256) { throw "Texture hash mismatch: $($record.file)" }
    Write-Output "CHUCK_ROPE_SOURCE_VERIFIED $($record.file)"
}
