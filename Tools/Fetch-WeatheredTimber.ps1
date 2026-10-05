param()
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$folder=Join-Path $root 'SourceAssets/Surfaces/WeatheredTimber'
$manifest=Get-Content -LiteralPath (Join-Path $folder 'manifest.json') -Raw | ConvertFrom-Json
foreach($record in $manifest.assets) {
    $target=Join-Path $folder $record.file
    if(!(Test-Path -LiteralPath $target) -or (Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $record.sha256) {
        Invoke-WebRequest -Uri $record.source -OutFile $target
    }
    if((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $record.sha256) { throw "Texture hash mismatch: $($record.file)" }
    Write-Output "CHUCK_TIMBER_SOURCE_VERIFIED $($record.file)"
}
