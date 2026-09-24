## Established Project Decisions

- CHUCK is a top-down action-adventure RPG inspired by 16-bit adventure games.
- Chuck is approximately one foot tall.
- The D&D source stat block's “Giant Rat” creature name does not define his visual scale in the game.
- The Game Bible is the core creative authority.
- Topic-specific supplements are authoritative within their defined topics.
- Campaign notes are historical source material and reference pools, not automatic implementation requirements.
- Development is incremental: one coherent feature or vertical slice at a time.
- The game should remain runnable after each completed pass.
- Claude Code and Codex use the same Git repository.
- Only one coding agent edits at a time.
- Each completed pass should be tested, handed off, and committed before switching agents.
- ~~Each newly authored gameplay map receives one physical Ashtray/checkpoint,
  registered through the shared save/checkpoint loader rather than a separate
  map-specific or development teleport path.~~ **Superseded** — see the save
  and browser decisions at the end of this file. Do not add Ashtrays.
- Phase 5 and Phase 6 are complete. Phase 6's implemented scope remains
  documented in `PHASE-6.md`.
- Phase 6 begins at the jungle-temple interior and ends when Chuck reaches the
  ship; ship gameplay remains Phase 7 content.
- The first Phase 6 room is a dedicated, reversible temple entrance hall with
