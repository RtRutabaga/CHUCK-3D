# Reference provenance

## September 30 — additive Waterdeep layout adaptation

Read the original current handoff, game bible, map comments and plaza generator as reference only; no original generators or mutations were run. Copied only these text maps from `C:/Users/ashsm/OneDrive/Documents/CHUCK-game/chuck/chuck/assets/maps/` into `References/Original/WaterdeepLayout/`, preserved byte-for-byte:

- `waterdeep_docks.txt`: SHA-256 `4AD3F64B07061C3E29B15E5DCCD84B90FC6D8A4A8320E67B638D960BAF4DEC0F`.
- `waterdeep_plaza.txt`: SHA-256 `F6538F4A8A843F4C58243B19239839E32D5A387E33F7717F6D98D74A05679014`.

These supply landmark relationships (district walls/torches, sewer grate, fountain, shops, market and closed city gate), not executable instructions. The new 3D district is rotated/rearranged and additive; no original code, sprites or campaign state was imported. The user's current request authorizes the fountain plaza despite older prototype exclusions.


## User-supplied resource guide

Added 2026-09-26: `References/chuck-3d-resource-guide.md`, copied byte-for-byte from `C:\Users\ashsm\.codex\codex-remote-attachments\01a0d449-b2e8-7fc1-9502-ac6cb557fa6d\15159EA9-DE10-4994-A871-2110A6081741\1-chuck-3d-resource-guide.md`.
SHA-256 (source and copy): `3DA84004DB51ED3CBF379940AA9DA2D05BDF7BD13A9E4157C72D812C233900C0`.
Directional suggestions, not a replacement for the user's character target or current milestone. Applicability and current-resource corrections: `docs/RESOURCE-GUIDE-NOTES.md`.

## User-supplied AI dev notes

Added 2026-09-26: `References/ai-dev-notes.md`, copied byte-for-byte from the user's upload `AI-DEV-NOTES.md` (Claude Code session attachment `9c1969f3-AI-DEV-NOTES.md`).
SHA-256 (source and copy): `2087EA97E404E02D4B956CADC4B6B293348B7F0E58378541244C0FA0A2C18BE5`.
The user describes it as suggestions, not rigid rules. Applicability, conflicts with current project rules and open decisions: `docs/AI-DEV-NOTES-REVIEW.md`.

## User-supplied goal images (2026-09-27)

Four PNG goal images were added to `References/ArtDirection/` byte-for-byte from the user's Claude Code session attachments: `Chuck-Standing-Smoking.png`, `Chuck-Run-Profile.png`, `Chuck-Turnaround.png` and `Chuck-Run-Cycle-Sheet.png`. The hashes and what each fixes are in `docs/ART-DIRECTION.md` ("Goal images, 2026-09-27"). They are reference only, not game assets.

## Original game references

Captured 2026-09-24 from read-only source: C:\Users\ashsm\OneDrive\Documents\CHUCK-game
Source HEAD: 87585dd6efb3d9fb0a44dd33549fa521f17b6701

Only two complete design documents and three short excerpts are retained. No source code, game assets, Git history or deployment files were copied. Source hashes describe the working files read, not a claim that the source worktree is clean. Historical phase instructions do not govern CHUCK 3D.

- Full copy: docs/design/GAME-BIBLE.md -> References\Original\GAME-BIBLE.md; SHA-256: CA172DF1055740AA9B7664615A738A75D7E4B00D25ED563EAD4F93AE218050F2
- Full copy: docs/development/PHASE-2.md -> References\Original\PHASE-2.md; SHA-256: 694AF70A8FE7A159863239CD232A67CB8FFA6B973BE033FD1906ACDEF048F000
- Excerpt: docs/development/HANDOFF.md, lines 8-17 -> Original/HANDOFF-status-excerpt.md; full source SHA-256: 09186240D20E9F04CF954077939CC3E25AF41C2CD5214AC7329854E25D2C3E75
- Excerpt: docs/development/PHASE-14.md, lines 1-51 -> Original/WATERDEEP-context-excerpt.md; full source SHA-256: C385E8B935375C77A86504AEC2C2B96768EB844C6A690F4A69971FA1F1547AF4
- Excerpt: docs/development/DECISIONS.md, lines 5-26 -> Original/DECISIONS-excerpt.md; full source SHA-256: 6086A8E03B667E62356C82DBDF6EE76C307F433CB3ABF481130223F1C6B200DF

Selection rationale: bible preserves identity and tone; Phase 2 supplies original movement/interaction design; Phase 14 excerpt explains established docks landmarks without importing the finale scope; handoff confirms completion; decisions preserve scale and mark obsolete ashtrays. The local project brief governs prototype scope.

Also read without copying: AGENTS.md, docs/README.md, docs/development/CURRENT-PHASE.md, remaining Phase 14 and relevant DECISIONS.md sections. The large original handoff was inspected, with its opening completion statement used as the current status.

Missing: docs/design/CHUCK-Game-Bible-Supplement-Geography.md and docs/design/CHUCK-Game-Bible-Supplement-Player-Progression.md. Both are listed in the original index but absent from the repository filename search. Their contents have not been inferred.
