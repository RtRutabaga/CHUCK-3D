# The Feywild: the way on through the Deepwood (plan)

**Status: approved by the user 2026-11-27 with both recommendations (nothing left where the road was; the hidden way plainly findable). Not built yet.** Part of [FEYWILD.md](FEYWILD.md); written
separately so as not to collide with work on that file.

## The idea

The Crossroads has a direct road to Highmeadow (threshold 8, a rock cutting). It lets a player skip the Deepwood and
go straight on to the maze. The user wants more of the Feywild seen before the maze, with no hidden moves. So that road
is removed. The only way on is then through the Deepwood and its hidden way into Highmeadow.

The Lily Mere stays an optional loop off the Crossroads, as now.

## The user's decisions (2026-11-27)

1. **The Crossroads' direct road to Highmeadow is removed.** The Deepwood becomes the only way on; the Lily Mere stays
   optional.
2. **Nothing is left where the road was.** No stub, no arch, no clearing: the ring has six roads.
3. **The hidden way is made plainly findable.** It keeps its name in the docs only.

## The map now and after

Now (from `FeyMap.cpp`'s `Ways`):

```
Riverbank -0-> Orchard =1,2=> Crossroads <=3,4=> Lily Mere            (loop)
                              Crossroads <=5,6=> Deepwood -7-> Highmeadow -9-> Maze
                              Crossroads ---8---------------> Highmeadow   (removed)
```

After:

```
Riverbank -0-> Orchard =1,2=> Crossroads <=3,4=> Lily Mere            (optional loop)
                              Crossroads <=5,6=> Deepwood -7-> Highmeadow -8-> Maze
```

The way forward is fixed: the Riverbank, the Orchard, the Crossroads, the Deepwood, Highmeadow and the maze. Six
districts out of seven must be seen. The two loops (the Orchard's two roads, the Deepwood's two) still return to the
Crossroads.

## What changes

**The road and its threshold.**

- Threshold 8 is deleted from `Ways`, and `Thresholds` drops from 10 to 9. The maze's stone stair becomes threshold 8.
- Everything that names a threshold by number is renumbered:
  - `FeyMap::Checkpoint`'s entry table (`{0, 0, 1, 3, 5, 8, 9}` becomes `{0, 0, 1, 3, 5, 7, 8}`, see below);
  - the route walker's legs in `FeyMapTest.cpp`;
  - any test still or shot list keyed to a threshold's number.
- The gap between the two districts' rectangles is closed by the painted distance's forest. The 48 m kept clear for the
  cutting goes.

**The Crossroads end.**

- The road from the ring to that mouth, its rune arch and the notch in the district's rim are removed. The Crossroads
  has six roads instead of seven.
- The land and planting fill the place (decision 2): grass, foxgloves, ferns and flowers, the purple-moss oaks and fly
  agarics of its edges, and the district's wooded rise running on unbroken where the cutting opened, with the painted
  forest beyond. No sealed arch or dead end is left behind: a road that ends at a wall would look like a locked door and
  send players looking for a key.
- **It must look as if nothing was ever there.** No widened paving where the road met the ring, no straight clearing
  through the trees along its line, no signpost arrow pointing at nothing. Anything planted or kept clear because of
  that road (`FeyLifeClearings`, the threshold-mouth margins used by the hares and the planting) goes with it.
- **The tower still pulls that way.** Highmeadow and the maze lie beyond that side, so Zephyros' tower stands over the
  trees there with no road to it. A player who walks toward it meets a natural wooded slope (ordinary ground and trees,
  not an invisible wall), and turning back from it shows the Deepwood's roads from the ring.
- Its signposts lose their arrow to Highmeadow and point to the Deepwood instead, as the way on.

**The Highmeadow end.**

- The road from that mouth to the hub, and the rim's notch, are removed. The land and planting fill the place.
- Highmeadow's checkpoint now stands on the road in from the Deepwood's hidden way (threshold 7). It used to be on the
  road from the Crossroads. Its name and save point stay "Feywild Highmeadow"; an old save there starts at the new place.
- The herd of hares just inside the Crossroads mouth goes away of itself, since that mouth no longer exists. The herd
  inside the Deepwood mouth and the two home herds stay.
- The displacer beasts' rounds are checked so that none of them now crosses the removed road's ground.

**The Deepwood's hidden way.**

It is now the only way forward, so it must not be hidden from someone who goes looking for it (decision 3). With no
marker and no minimap, it should be findable by landmarks alone. Its mouth is in the district's north-west corner:

- The Deepwood's blue lanterns line the road to it the whole way from the ring, as they line its other paths.
- Its signposts point to it as plainly as to the Crossroads.
- At its mouth, a gap in the canopy shows the low gold light of Highmeadow beyond and, where the overcast allows, a
  glimpse of Zephyros' tower.

The road keeps its name, but nothing about it is hidden.

**The Deepwood's climbs become compulsory.** Both crossings ([FEYWILD-DEEPWOOD-CROSSINGS.md](FEYWILD-DEEPWOOD-CROSSINGS.md),
built 2026-11-26) are arcs round the hidden way's mouth, so every player now climbs them: the root crossing (speed vault,
four-legged sprint leap over a 1.6 m strip of thorns, speed vault) and the thorn crossing (five ivy hoops). The sprint
leap needs full stamina and a straight run-up.

- **No player may be stuck there.** A player who misses the leap or the hoops lands in the thorns, which slow him, prick
  him and stumble him back the way he came. Check by hand and in a test that walking straight through each corridor's
  thorns, without the moves, still gets him through at a Sanity cost, and never pushes him back out for good. If it
  does, shorten the stumble inside the crossings' corridors, not the thorns elsewhere.
- **They should be readable.** Each corridor sits where the road meets its arc, so following the road leads to it. The
  stills check that the corridor reads as the way through from the road.

**The Deepwood's dangers.**

Every player now crosses the Deepwood. Its redcaps and thorn mites are checked along the road from each way in to the
hidden way: none should stand on the road itself. Redcap Hollow stays off the road, an optional camp to avoid, sneak
past or fight through as planned. The jellyfish carry on as built. The one inside the hidden way's mouth is now on
the main way.

**Music.** It is unchanged: the dark track in the Deepwood, the open track in Highmeadow.

## Checks planned

- **Route walk** (`-ChuckFeyMapTest`): every threshold in order with the new numbering. The leg from the Crossroads to
  Highmeadow is gone, so the route goes Crossroads, Deepwood, Highmeadow, maze. Results: 0 stuck, 0 late, at most two
  districts loaded.
- **See-through check** (`=cracks`): 0 leaks on every threshold, and none toward the removed road's gap from either
  district's edge.
- **Highmeadow checkpoint**: a death in Highmeadow returns him to the road in from the Deepwood, facing the hub. The
  dev menu's Highmeadow start begins there.
- **Life tests** (`=life`): the hare steps that measured from the Crossroads mouth (`hare_near_entrance`) measure from
  the Deepwood mouth instead. Every other step passes unchanged.
- **The hidden way's mouth**: stills from the Deepwood's ring and from halfway along its road, to show it can be found.
- **The old road's place**: stills of the Crossroads' west side from the ring and from the great tree, to show nothing is
  left (no paving, clearing or arrow) and the rise reads as a natural slope. A walk toward the tower from the ring meets
  ground and trees, not an invisible wall.
- **The crossings on foot without the moves**: walked straight through each corridor's thorns, he gets through (life
  steps 109 to 112 still pass as well).
- **Frame time**: unchanged in every district. The Crossroads and Highmeadow have slightly less to build.
- **Package verification**, then the user's hand play.

## Drawbacks

- **The Deepwood is compulsory.** It is the darkest and most dangerous district, and the slowest to draw. A player who
  dislikes it can no longer go round it.
- **The walk to the maze is longer.** The Deepwood replaces the direct road: at a run about two to three more minutes
  through the wood and its root tunnel, less on a jellyfish.
- **Backtracking.** A player who has already done the Lily Mere and then walks to Highmeadow goes Crossroads, Deepwood,
  Highmeadow; there is no shortcut back. This is the same as the Feywild's other one-way legs.
- **The Lily Mere can still be missed.** Only the redirect plan (not taken) would force it as well.
- **The Deepwood's two climbs are compulsory**, including a sprint leap. They were built as the way to Highmeadow, so
  this is their intent, but a player who struggles with them can no longer go round by the Crossroads' road.

## Answered by the user (2026-11-27)

1. **The old road's place at the Crossroads:** nothing left behind (the recommendation). The alternative, a ruined rune
   arch standing over grass, was not taken.
2. **The hidden way:** plainly findable in the game (the recommendation).
