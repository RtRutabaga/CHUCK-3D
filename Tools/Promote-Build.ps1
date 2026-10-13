# Promotes a verified candidate package to the root launcher build and prunes old backups.
# The current Builds\Windows becomes Builds\Windows-Previous-<date>-<Label>; only the newest -Keep backups are retained.
param(
    [Parameter(Mandatory=$true)][string]$Candidate,
    [Parameter(Mandatory=$true)][string]$Label,
    [Parameter(Mandatory=$true)][string]$VerificationLog,
    [int]$Keep=2,
    [switch]$AstralCameraReview,
    [switch]$TutorialReview,
    [string]$BuildsRoot
)
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$defaultRoot=!$BuildsRoot
if($defaultRoot) { $BuildsRoot=Join-Path $projectRoot 'Builds' }
$BuildsRoot=[IO.Path]::GetFullPath($BuildsRoot)
if($Keep -lt 1) { throw 'Keep at least one previous build so a bad promotion can be rolled back.' }
if($Label -notmatch '^[A-Za-z0-9]+$') { throw 'Label must be letters and digits only, e.g. KeeperRag.' }

# Accept either the archive directory (containing Windows) or the Windows folder itself.
$source=[IO.Path]::GetFullPath($Candidate)
if((Split-Path $source -Leaf) -ne 'Windows' -and (Test-Path -LiteralPath (Join-Path $source 'Windows'))) { $source=Join-Path $source 'Windows' }
if(!(Test-Path -LiteralPath (Join-Path $source 'Chuck3D\Binaries\Win64\Chuck3D.exe'))) { throw "No packaged Chuck3D.exe under $source." }
$target=Join-Path $BuildsRoot 'Windows'
if($source -eq $target) { throw 'The candidate is already the launcher build.' }

# Write the receipt on the candidate first: a failing log stops the promotion before anything moves.
& (Join-Path $PSScriptRoot 'Write-BuildReceipt.ps1') -PackageRoot $source -VerificationLog $VerificationLog -AstralCameraReview:$AstralCameraReview -TutorialReview:$TutorialReview

$running=Get-Process -Name 'Chuck3D*' -ErrorAction SilentlyContinue | Where-Object { $_.Path -and $_.Path.StartsWith($target,[StringComparison]::OrdinalIgnoreCase) }
if($running) { throw 'The launcher build is running. Close the game and promote again; nothing was moved.' }

$previous=$null
if(Test-Path -LiteralPath $target) {
    $name='Windows-Previous-'+(Get-Date -Format 'yyyyMMdd')+'-'+$Label
    if(Test-Path -LiteralPath (Join-Path $BuildsRoot $name)) { throw "$name already exists; choose another label." }
    Rename-Item -LiteralPath $target -NewName $name
    $previous=Join-Path $BuildsRoot $name
}
try { Move-Item -LiteralPath $source -Destination $target }
catch {
    if($previous) { Rename-Item -LiteralPath $previous -NewName 'Windows' }
    throw "Could not move the candidate; the previous launcher build was restored. $_"
}
$sourceParent=Split-Path $source -Parent
if($sourceParent -ne $BuildsRoot -and !(Get-ChildItem -LiteralPath $sourceParent -Force)) { Remove-Item -LiteralPath $sourceParent }

if($defaultRoot) { & (Join-Path $PSScriptRoot 'Launch-Chuck.ps1') -CheckOnly }

# Newest backups by when their package was built; renaming a folder keeps its creation time.
$backups=@(Get-ChildItem -LiteralPath $BuildsRoot -Directory -Filter 'Windows-Previous-*' | Sort-Object CreationTimeUtc -Descending)
foreach($old in ($backups | Select-Object -Skip $Keep)) {
    Remove-Item -LiteralPath $old.FullName -Recurse -Force
    Write-Output "CHUCK_PRUNED $($old.Name)"
}
foreach($extra in (Get-ChildItem -LiteralPath $BuildsRoot -Directory | Where-Object { $_.Name -ne 'Windows' -and $_.Name -ne 'Releases' -and $_.Name -notlike 'Windows-Previous-*' })) {
    Write-Warning "Unpromoted build folder left in place: $($extra.Name). Delete it once it is no longer needed."
}
Write-Output "CHUCK_PROMOTED $Label previous=$(if($previous){Split-Path $previous -Leaf}else{'none'}) kept=$([Math]::Min($Keep,$backups.Count))"
