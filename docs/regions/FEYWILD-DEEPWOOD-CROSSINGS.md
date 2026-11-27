# The Feywild: the Deepwood's climbs (built)

**Built 2026-11-26 by Claude**, on the user's word ("build the deepwood climb"). The region plan ([FEYWILD.md](FEYWILD.md))
gives the Deepwood "brachiation, slide-jumps over ground roots". The jellyfish plan
([FEYWILD-JELLYFISH.md](FEYWILD-JELLYFISH.md), decision 6) makes these crossings barriers the jellyfish cannot get round,
with more of them on the far side. Code: `FeyDeepCrossing.h/.cpp` (`FeyDeep::Crossings`, `AFeyDeepCrossings`).

## Where

Both crossings guard the Deepwood's hidden way to Highmeadow, whose mouth is in the district's north-west corner. Each is an
arc round that mouth from the district's west wall to its north wall, so neither can be walked round. The bounds are
already walls, and the test `deep_walls_hold` probes every 3 m of both arcs.

- **The root crossing:** the outer arc, 75 m from the mouth (162 m long).
- **The thorn crossing:** the inner arc, 45 m from the mouth (119 m long).
- **The road:** the road from the mouth crosses each arc once. That is where each one's corridor is.
- **The Deepwood North checkpoint:** it lies between the two arcs.

## How each is crossed

| Crossing | Wall | Corridor through it (on the road) | Moves |
|---|---|---|---|
| Roots | Six great ground roots per stretch, rising out of the ground and diving back in, crossing one another, with knees arching up every few metres | 3.4 m wide, 11 m long: a waist-high root (46 cm) at each end, and a 1.6 m strip of thorns between | Speed vault, then the four-legged sprint leap over the thorns (a running jump falls short), then a speed vault. Either way round |
| Thorns | A bramble thicket: a low dark core under dense thorny canes arching up to about 3 m | 3.4 m wide, 7.6 m long: a 4.8 m bed of thorns under five ivy hoops, 1.35 m apart, crowns 98 cm up (the maze's hoops) | A hop up to the first hoop, then swinging hoop to hoop, and down past the bed. Either way round |

- **Thorns:** they prick and slow him as the maze's do. Each 1.6 s among them on the ground costs a Sanity point and a
  stumble back, and he moves at 45 % speed (`FeyGroundSpeedScale`).
- **The "slide-jump":** it is his speed vault, his existing slide over a low obstacle in stride. No new move or
  animation was made.

## The jellyfish

- **The barrier:** each arc is a barrier line (`AFeyJelly::Barriers`). The jellyfish steer away from it within 9 m, and
  never come nearer than 2.5 m: a hard rule in its movement, since steering alone can carry it on at 5 m/s.
- **Who waits where:** two jellyfish wait 10 m beyond each end of each corridor, so after either climb one is near. That
  is eight more, twelve in the Deepwood in all.
- **The route-walk test:** `FeyDeep::WalkerHop` carries the map's route walk over a crossing, as a player would climb
  it. The climbs themselves are tested in life steps 109 to 112.

## Checks

`-ChuckFeyMapTest=life`, steps 109 to 112:

| Step | Check | What it shows |
|---|---|---|
| 109 | `deep_hoops_move`, `deep_hoops_clear_thorns`, `deep_hoops_past_thorns` | 5 catches, no pricks |
| 110 | `deep_roots_through`, `deep_roots_clear_thorns`, `deep_roots_moves` | 2 vaults, 1 sprint leap with a 2.1 m run-up, no pricks |
| 111 | `deep_jellies_beyond` | At least two jellyfish at each side |
| 111 | `deep_jelly_refuses` | Ridden down the road at the root crossing, it came no nearer than 7.8 m to the line |
| 112 | `deep_walls_hold` | 83 probes, no gaps |

Stills: `Life_DeepRootCrossing`, `Life_DeepRootLeap`, `Life_DeepThornCrossing`, `Life_DeepThornHoops`,
`Life_DeepJellyBack`.

## Not done

- Not played by hand.
- The thicket's core is still a dark mass in places behind the canes.
- Neither crossing has its own sound.
- The test drives his sprint with a held stick (`AChuckCharacter::SetTestHoldWorld`); its own frame-by-frame input once
  missed a frame and ended the sprint.
