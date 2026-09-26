param([string]$BaseRef='HEAD')
$ErrorActionPreference='Stop'
$projectRoot=[IO.Path]::GetFullPath((Split-Path $PSScriptRoot -Parent))
$gitRoot=& git -C $projectRoot rev-parse --show-toplevel
if($LASTEXITCODE -or [IO.Path]::GetFullPath($gitRoot) -ne $projectRoot) { throw 'Run from the CHUCK-3D main checkout, not a copied Tools folder.' }
$remote=& git -C $projectRoot remote get-url origin
if($LASTEXITCODE -or $remote -notmatch 'github\.com[:/]RtRutabaga/CHUCK-3D(?:\.git)?$') { throw 'Unexpected remote. This helper only prepares the separate CHUCK-3D repository.' }
$dirty=& git -C $projectRoot status --porcelain
if($LASTEXITCODE -or $dirty) { throw 'Commit or finish existing work before preparing agent worktrees. Nothing was changed.' }
$baseCommit=& git -C $projectRoot rev-parse --verify "$BaseRef^{commit}"
if($LASTEXITCODE) { throw "Cannot resolve base commit: $BaseRef" }
$common=& git -C $projectRoot rev-parse --path-format=absolute --git-common-dir
& git -C $projectRoot check-ignore --quiet -- Local/AgentWorktrees/
if($LASTEXITCODE) { throw 'Local/AgentWorktrees must be ignored before creating nested worktrees.' }
$parent=Join-Path $projectRoot 'Local\AgentWorktrees'
New-Item -ItemType Directory -Force -Path $parent | Out-Null
$results=@()
foreach($item in @(
    @{Folder='codex-movement'; Branch='codex/movement-foundation'},
    @{Folder='claude-character'; Branch='codex/claude-character'}
)) {
    $target=[IO.Path]::GetFullPath((Join-Path $parent $item.Folder))
    if(!$target.StartsWith($parent+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)) { throw 'Worktree target escaped the intended directory.' }
    if(Test-Path -LiteralPath $target) {
        if(!(Test-Path -LiteralPath (Join-Path $target '.git'))) { throw "Existing directory is not a worktree; it was not overwritten: $target" }
        $targetCommon=& git -C $target rev-parse --path-format=absolute --git-common-dir
        if($LASTEXITCODE -or $targetCommon -ne $common) { throw "Existing checkout belongs to another repository: $target" }
        $branch=& git -C $target branch --show-current
        if($LASTEXITCODE -or $branch -ne $item.Branch) { throw "Existing checkout is on another branch; no reset performed: $target" }
    } else {
        & git -C $projectRoot show-ref --verify --quiet ('refs/heads/'+$item.Branch)
        if($LASTEXITCODE -eq 0) {
            & git -C $projectRoot worktree add $target $item.Branch
        } else {
            & git -C $projectRoot worktree add -b $item.Branch $target $baseCommit
        }
        if($LASTEXITCODE) { throw "Worktree creation failed; inspect git worktree list before retrying: $target" }
    }
    $head=& git -C $target rev-parse HEAD
    if($LASTEXITCODE) { throw "Cannot inspect worktree: $target" }
    $state=@(& git -C $target status --porcelain)
    $results += [pscustomobject]@{Branch=$item.Branch;Path=$target;Head=$head;Dirty=($state.Count -gt 0)}
}
$results | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $parent 'worktrees.json')
$results | Format-Table -AutoSize
Write-Output 'Existing branches/checkouts were preserved. No agent, build, installer or login was started.'
