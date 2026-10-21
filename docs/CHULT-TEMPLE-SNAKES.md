# Chult — the temple snakes and the snake chamber

User request, October 21, 2026 (Claude): "Create and add the snakes enemies". The 2D game's next temple map after the dart corridor is the snake chamber (map 5), so this builds that chamber beyond the dart gallery's arch and its twenty snakes. Claude owns the creature; the chamber continues Claude's [temple interior](CHULT-TEMPLE-INTERIOR.md).

## The 2D direction

Read-only sources:
- `docs/development/PHASE-6.md`: "Create one dedicated snake chamber. Snakes are intentionally one-hit enemies. This briefly lets the player feel powerful before returning to the temple's normal difficulty". Temple Map 5 is a "broad torch-lit snake chamber with twenty one-hit snakes, looping lanes around masonry piers"; its onward route turns south.
- `src/entities/snake.py` and `src/core/config.py`:
  - Speed 32, faster than the skeleton's 25; Chuck's is 80.
  - Notice range 96 px, all round.
  - 10 sanity a bite, of 100.
  - One scratch kills.
- `assets/maps/temple_snakes.txt`: a 60 × 44 tile chamber entered through an arch, masonry blocks spread through it, serpent idols, a southward arch.

## The snake

`Tools/build_temple_snake.py` (Blender 4.5.14) builds it with no downloads: a 1.8 m jungle pit viper after the fer-de-lance, big enough to take a 65 cm rat.

**Body**
- A tube with a ring every centimetre. Its section is a squarish loaf with a flat belly.
- It swells mid-body to 6.7 cm across, narrows to a thin neck and widens into a broad triangular head with supraocular "brows".

**Mouth**
- Forward of the mouth corner the rings are cut along the mouth line on both sides. The lower part rides the jaw, so a gape opens a real mouth onto a pink palate and mouth floor.
- Two fangs fold back along the palate.
- A forked tongue lies in the floor of the mouth.
- The eyes are amber with a vertical slit pupil.

**Skin (generated textures)**
- An olive-brown body with dark, pale-edged saddles meeting along the back, small spots, and a cream belly.
- A dark stripe behind each eye and pale lips.
- Keeled, overlapping dorsal scales in 26 staggered rows and wide belly scutes, as a tiling normal map with their crevices as a cavity map.
- In Unreal (`M_SnakeSkin`) the scale relief tiles every 12 cm over the body pattern; the scales are satiny and their crevices rougher and darker.

**Rig and size:** 28 spine bones plus head, jaw and tongue (33 with the root nodes); 11,192 triangles, with LODs down to 900 vertices.

**Import:** `Tools/import_temple_snake.py`, into `/Game/Characters/Snake`.

### Motion (`ATempleSnake`, TempleSnake.h/.cpp)

No clips: the body is posed bone by bone, every frame, along the track its snout has left. This is how real snakes move, and it cannot slide.

- **Crawl (lateral undulation):** the snout weaves 34° either side of its heading over a 46 cm wavelength. Each bone sits at its own distance back along the snout's track, turned along it. Measured: the mid-body's sideways motion is about 5 % of its speed (`slip=0.05`), so it follows its own track. Starting from rest it crawls out of its own coil.
- **Rest:** a flat coil (an Archimedean spiral, the turns 7.6 cm apart), head at the outer end, the tongue flicking every few seconds.
- **The tell:** it stops. Its front 75 cm draws up into an S, a curve as long as that part of the body with its handles swung to the side. The head rises 16 cm and turns on the rat; it hisses through a parted mouth.
- **The strike:** committed to its aim, the S straightens and the head shoots out as far as he is, up to 62 cm, jaws wide (58°). Then it draws back into the S. It bites only if its snout reaches his body.
- **Death:** one scratch anywhere along it. Waves thrash down it and die away over two seconds; it rolls half over (belly up along its middle) and lies still, mouth open.

### Behaviour

- **Placement:** twenty, as in the 2D map, seeded through the chamber's lanes. Each stays clear of the piers, the idols, the first 4 m inside the way in and the way on, at least 1.8 m apart. Six of them wander their lane now and then.
- **Notice:** it feels him within 4.5 m, all round, if he is in the chamber and it can see him.
- **Chase:** 95 cm/s. That is over Chuck's walk (72) and well under his run (225); the 2D snake is 32 to his 80. It steers round piers and keeps to the chamber.
- **The strike's range:** it rears once the base of its S is within 80 cm of him. The 0.38 s tell is enough to roll or step aside: a roll goes through the bite (dodge immunity), and a step aside as it strikes leaves it biting air. A bite costs 1 sanity, with a pause of 0.7–1.3 s before the next.
- **Out of the chamber:** if he leaves, it stops at the edge, then crawls back to its place after 3 s and lies there.
- **Chuck's scratch:** snakes count as low targets, so his low rake comes out, and one scratch kills.
- **Light:** lighting channel 1.
- **Automated runs:** snakes spawn in play and in `-ChuckSnakeTest` only.

### Sounds

`Tools/gen_snake_sfx.py` (stdlib, seeded, in the traps' stone acoustic) makes 12 originals:
- the hiss;
- the strike: a whoosh, a slap and the jaws;
- scales sliding over stone;
- the death throes.

`Tools/import_snake_sfx.py` imports them.

## The snake chamber (map 5)

Built in `ChultTempleInterior.cpp`, in the same frame as the rest (S toward the temple, T across, Z above the plaza).

| Part | Where | What |
| --- | --- | --- |
| The way in | the dart gallery's arch at T −5160 | The rubble behind the arch is cleared; the gallery runs on 4 m to its open end |
| Chamber | S 65900–67900, T −5560 → −8360 (20 × 28 m), floor −1460, ceiling −660 | Five piers in lanes, each with a torch. Three beams across the vault. Torches every 5.5 m along both long walls and at the corners. Two serpent idols on plinths with a brazier between them under a glyph panel at the far end. Two fallen drums, seven pots, five old skulls |
| The way on (map 6) | an arch in the +S wall at T −7950 | The passage behind it has fallen in (rubble, a hidden wall): reserved for the Astral wind connector |

- **Spawn / Dev Checkpoint:** **Temple Snakes**, in the gallery's last 4 m between its arch and the chamber (its own space, `SnakeArch`). That is out of the snakes' sense: a first spawn 2 m inside the chamber put him in a chase at once, which the menu test caught. A death in the chamber returns him there.
- **Checks** (`CHUCK_CHULT_TEMPLE_INTERIOR_CHECK`):
  - The route continues through the gallery and the chamber to the new choke, which is closed.
  - The intrusion probes cover the three new spaces (18 in all, `intrusions=0`).
  - Six spawns.
- **Review:** the interior review's darts trial now runs on through the arch into the chamber. Its stills add the arch, the chamber and the idols.

## Tests

`-ChuckSnakeTest` (`TempleSnakeTest.cpp`), with the real character, one snake at a time (the rest held):
1. *placement:* 20 snakes coiled at their posts, on the floor, clear of the piers.
2. *notice_chase_crawl:* it notices him, uncoils, chases at 95 cm/s and rears; its body keeps to its track (slip under 0.3).
3. *strike_lands:* he stands in front of it: the tell (at least 0.3 s), the strike, the bite (1 sanity).
4. *roll_dodges:* a roll as it rears: no bite.
5. *step_aside_at_strike:* a step aside as it strikes: its snout passes 30 cm clear.
6. *one_scratch_kills:* one scratch: it dies (no other snake does), thrashes and lies still on the floor, mouth open.
7. *kept_to_the_chamber:* he leaves: it stays inside, goes back and rests.

`-ChuckSnakeCapture` also saves frames to `Saved/Screenshots/.../Snake`.

## Not done

- No person has played it. How readable the tell is from Chuck's camera height, and how the chamber feels with twenty snakes after him, are untested by a human.
- The sounds are synthetic and have not been listened to.
- Crawling out of its coil, a snake's head can pass over its own coils without rising.
- The snakes pass through one another, and through the jars at the walls.
- The model is procedural: its head is good at play distance but simpler than a real viper's close up. There are no labial or head scales modelled, only the texture.
- Map 6 (the Astral wind connector) is next, through the choked arch.
