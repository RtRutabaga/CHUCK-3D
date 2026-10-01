# Motion capture for the human NPCs (user approved downloads; source chosen by
# Claude, 2026-10-01): a few takes from the CMU Graphics Lab Motion Capture
# Database in B. Hahne's Motionbuilder-friendly BVH conversion (2010), whose
# bone names are exactly MPFB's cmu_mb rig. "Free to use worldwide for any
# purpose" (CMU / cgspeed). Individual files from the una-dinosauria/cmu-mocap
# mirror of that release; the descriptions are its index. No installation.
# Tried and dropped: 140_06/07 "Idle" (a crouched ready stance), 113_21 (head
# tipped far back), 141_20 "Waiting" (fidgety), 76_06 "avoid stepping on
# something" (a cartoonish hop); see Tools/build_npc_mocap.py.
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$destination=Join-Path $projectRoot 'SourceAssets\Mocap\CMU'
New-Item -ItemType Directory -Force -Path $destination | Out-Null
$takes=[ordered]@{
    '111_28'='Standing still'; '77_02'='standing'; '18_08'='conversation - explain with hand gestures';
    '79_73'='scared'
}
$records=@()
foreach ($take in $takes.Keys) {
    $subject=$take.Split('_')[0]
    $url="https://raw.githubusercontent.com/una-dinosauria/cmu-mocap/master/data/$('{0:d3}' -f [int]$subject)/$take.bvh"
    $target=Join-Path $destination "$take.bvh"
    if (!(Test-Path -LiteralPath $target)) {
        try { Invoke-WebRequest -Uri $url -OutFile $target -UseBasicParsing }
        catch { $url=$url.Replace("/data/$('{0:d3}' -f [int]$subject)/","/data/$subject/"); Invoke-WebRequest -Uri $url -OutFile $target -UseBasicParsing }
    }
    $records += [ordered]@{file="$take.bvh"; description=$takes[$take]; source=$url; license='CMU Graphics Lab Motion Capture Database (mocap.cs.cmu.edu): free for any use; BVH conversion by B. Hahne (cgspeed.com)'; sha256=(Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash; bytes=(Get-Item -LiteralPath $target).Length}
}
[IO.File]::WriteAllText((Join-Path $destination 'manifest.json'),([ordered]@{provider='CMU Graphics Lab Motion Capture Database, Motionbuilder-friendly BVH release 2010'; takes=$records} | ConvertTo-Json -Depth 6)+"`n")
$records | ForEach-Object { [pscustomobject]$_ } | Select-Object file,bytes,description
