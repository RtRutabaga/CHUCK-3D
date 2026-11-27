# The Feywild: the Deepwood jellyfish (plan)

**Status: approved by the user 2026-11-26 with its recommendations (Redcap Hollow kept on foot; pale teal with a violet rim); built 2026-11-26 (see [What was built](#what-was-built-2026-11-26) at the end).** The Deepwood's mount: a drifting
jellyfish that Chuck hangs from by its tendrils. It was the user's idea, after the great hare
([FEYWILD-HARE.md](FEYWILD-HARE.md)) was kept out of the Deepwood. Part of [FEYWILD.md](FEYWILD.md).

## The idea

A great hare does not suit the Deepwood: it is too narrow, and roots and logs stop a ground mount dead. A jellyfish
floating at a fixed height clears that clutter, and a soft glowing bell drifting through the dark oaks fits the district.
Chuck reaches up, takes hold of its tendrils and hangs beneath it, his legs dangling, while the stick steers it.

## The user's decisions (2026-11-26)

1. **Speed:** 5 m/s, with no run toggle. The stick's push sets the speed, up to 5 m/s at full push.
2. **No up or down control.** It hovers at a fixed height above the ground.
3. **Where it goes:** anywhere on the Deepwood's ground, over its hills and rises. It is not kept off the slopes.
4. **Bucks only where the game needs them:** he is shaken off only if it is leaving the Deepwood. Elsewhere it
   drifts back, as the hare refuses.
5. **Nothing runs from him.** He can run straight up to one and take hold. The hares no longer run from him either:
   that was taken out on the same day.
6. **The Deepwood's set pieces stay on foot (2026-11-26).** The planned brachiation under limbs and slide-jumps over
   big ground roots are barriers a jellyfish cannot get round. It drifts back from them as from a threshold's mouth, and
   he must let go and cross on foot. Beyond each one, more jellyfish wait for him.
7. The rest of the outline was accepted: its height over the ground, his swing under it, letting go, redcaps,
   placement and wandering home.

## The animal

| Item | Target |
|---|---|
| Size | A bell about 1.2 m across and 80 cm tall. Eight to twelve tendrils, about 2 m long, trailing behind as it moves |
| Look | Translucent, pale teal to violet, with a soft glow in the rim and the tendrils. The glow is the Deepwood's blue, not white. Faint markings show inside the bell. It must read clearly against the dark fog |
| Motion | A slow pulse of the bell (contract, glide, relax) about once every 1.5 s, faster when he steers it at speed. The tendrils sway and trail behind it, simulated as strands in code |
| Light | A small, soft, unshadowed teal light that moves with it, so it lights him and the roots under it. Measured for frame time, because the Deepwood is the heaviest district (about 17.6 ms) |
| Build | The SDF creature kit in Blender (its own script, `Tools/build_fey_jelly.py`), on a small skeleton for the bell's pulse. The tendrils are strands drawn in code, as the storm's ropes are. The material is a translucent, emissive one |
| Sound | A faint wet pulse with each contraction, and a low airy hum. Synthesised or CC0, credited in the manifest |

## Behaviour

**Unridden.**
- It drifts slowly (about 0.5 m/s) around its home within about 20 m, pulsing, and turns slowly.
- Its tendrils' ends hang at his raised-hand height, so he can walk or run under one and take hold.
- It does not flee from him.

**Ridden.**

| Control | Hanging from the jellyfish |
|---|---|
| Move | Steers it, relative to the camera as his walking is. The push sets the speed, up to 5 m/s. No run toggle |
| Talk (F / Y) | Takes hold beneath a jellyfish within reach ("Hold on" prompt); pressed again, he lets go |
| Roll (B) | He lets go |
| Jump | Nothing (no up or down) |
| Slash, climbing, wall runs, ledges, brachiation, crawling | Not available while hanging |

- **Height:** it follows the ground with his feet about 1 to 1.5 m up, eased so it rises and sinks gently over roots and
  hills rather than following every bump. It sinks to fit under low arching roots and tunnels, keeping about 30 cm over
  his feet. Where even that will not fit, it stops and turns aside.
- **Obstacles:** trunks and anything else solid at his height or its own turn it aside, as the hare's nose sweep does.
- **His swing:** he hangs under it like a pendulum. He trails back as it speeds up and swings forward as it stops. He
  swings out on turns. On a hard stop he settles in a couple of swings.
- **Letting go:** he drops to his feet (a short fall, his ordinary landing). The jellyfish stays where it is for a few
  seconds, then drifts home.
- **The Deepwood's edge:** about 8 m from a threshold's mouth (the root tunnels to the Crossroads and Highmeadow) it
  slows and drifts back from it. It shakes him off only if it is on its way out of the district all the same: the
  hare's rule.
- **Redcaps and mites:** a bite hurts him as usual, and he keeps hold.
- **Nothing saved:** a death or a checkpoint start sends every jellyfish home, unridden.

## Chuck

- **A new clip, `HangDrift`:** both hands up gripping the tendrils, arms straight, legs loosely together and dangling,
  with a slight sway. A gentle loop, about 2 s. It is authored in `Tools/build_chuck_v1.py`, starting from his
  brachiation hang.
- **The swing** is done in code: his whole body tilts about his grip.
- **The camera:** pulled back a little and raised, as for the hare's bound, so the bell and the ground ahead both show.
- **The rig contract is unchanged.** The clip is appended to `EClip`, and a new gait, `EGait::Hang` (or the hare's
  `Ride` with a flag), is appended.

## Where they live

| Place | Jellyfish | Why |
|---|---|---|
| Just inside each of the Deepwood's ways in (two from the Crossroads, one from Highmeadow) | 1 each, about 25 m in | A mount soon after he comes in, as with the hares |
| The middle of the Deepwood, on its loop | 1 | So one is nearby wherever he lets go |

Four in all to begin with. They are kept clear of the hag's cottage and Redcap Hollow's ring of stones, so that the hollow is still
avoided, sneaked past or fought through on foot. Where each one lives is searched for at runtime, as with the herds.

**Beyond the set pieces (decision 6).** The brachiation and slide-jump crossings split the Deepwood into stretches. Each
crossing is a barrier to the jellyfish: its way is closed both ways, so it can neither cross nor drift round it. Two
jellyfish wait on the far side of each crossing, about 10 to 20 m past where he lands, so he can take hold again soon
after. They are placed from each crossing's own data, so a new crossing brings its own. Each one keeps to its own
stretch, and goes home there.

The crossings are not built yet. Until they are, the four jellyfish above are all there is. The barrier and the
far-side jellyfish come with the crossings: whoever builds a crossing adds its barrier line and landing point to the
data the jellyfish read.

## Building it

1. The mesh and material: `Tools/build_fey_jelly.py` and an import script, as for the hare.
2. `AFeyJelly`: hovering movement (a flying character, its height set from the ground each tick), steering, the
   threshold rule, the pulse, and the tendril strands.
3. Chuck: `HangDrift`, the hang gait, the input routing (as for the ride), the swing and the camera.
4. `AFeyLife`: spawning and resetting them with the Deepwood.
5. Sounds.
6. Tests and stills; then package, verify and promote.

## Checks planned

- **Jellyfish spawn:** four in the Deepwood, each within about 30 m of its way in or of the middle.
- **Hold on:** he runs up under one; the prompt shows, and the talk key takes hold.
- **Speed:** full push reaches 500 cm/s and no more.
- **Height:** his feet stay 80 to 170 cm over the ground along a ride across roots and a hill, and never touch.
- **Obstacles:** ridden straight at a trunk, it turns aside without sticking.
- **Crossings (once built):** ridden at a crossing from either side, it drifts back and never gets past. Two jellyfish
  stand on the far side of each.
- **The edge:** ridden at a root tunnel's mouth, it drifts back with no throw. Put on the tunnel itself, it shakes him
  off.
- **Bites:** a redcap's bite leaves him holding on.
- **Let go and back:** Roll drops him to his feet; the talk key takes hold again.
- **Reset:** a death sends all four home.
- **Frame time** in the Deepwood with one ridden.
- **Stills** of it unridden, him taking hold, at speed, the swing on a stop, and under a low root.

## Risks

- **Frame time.** The Deepwood is already at the edge of 60 fps. A translucent material, a moving light and simulated
  strands each cost something. Keep one light, unshadowed, and few strand segments, and measure before and after.
- **Translucency in fog.** Translucent materials and the Deepwood's dense fog and mist veils can sort badly. Test early;
  an opaque, glowing bell with soft edges is the fallback.
- **Skipping the Deepwood.** At 5 m/s it crosses the district fast. Once built, the crossings (decision 6) keep the
  climbs on foot. Until then, nothing in the Deepwood is skipped, because nothing there needs to be done on foot yet.
- **Barriers that leak.** A barrier drawn as a line can be slipped round at its ends. Each crossing's barrier must run
  wall to wall, from cliff or trunk mass to cliff. A test rides a jellyfish along each barrier and checks that it never
  gets past.

## Open for the user

1. **Redcap Hollow.** Recommendation: keep the jellyfish out of the hollow's ring, as above, so the camp is still met on
   foot. The alternative is to let him ride straight through it.
2. **Its colour.** Recommendation: pale teal with a violet rim, from the Deepwood's blue lights. Any other colour is
   possible, apart from purple sky, which the Feywild never has.

## What was built (2026-11-26)

- **Changes from the plan:**
  - **The bell is drawn in code, not in Blender.** It is a procedural mesh: a dome with a curled, scalloped rim and
    four faint horseshoe marks, pulsing each beat. This takes `Tools/build_fey_jelly.py`'s place; no FBX is imported.
  - **His hang reuses his brachiation `Swing` clip**, not a new `HangDrift`: both paws overhead, legs dangling, posed by
    the swing angle. The rig and his clips are unchanged.
  - **New gait:** `EGait::JellyHang`, appended. `IsRiding()` now means "on a mount", the hare or a jellyfish.
- **The jellyfish:** `AFeyJelly` (`FeyJelly.h/.cpp`).
  - **Movement:** kinematic, not a character.
    - It eases toward the stick's push at up to 5 m/s, then sweeps a capsule from his feet to the bell's top. Trunks
      slide it round, and a head-on push is turned to one side.
    - It follows the highest ground under him (five traces), so it rises over roots and logs.
    - A lookahead stops it where even the lowest hang will not fit under something.
  - **His swing:** a two-axis pendulum (from the bell's rim to his middle, about 1 m) driven by its acceleration.
  - **Body:** four frilled oral arms (the front two pinned at his paws while he holds on) and twelve fine tendrils, all
    simulated strands drawn as tubes.
  - **Light and material:** a small unshadowed teal light. `M_FeyJelly` is translucent and unlit, fresnel-weighted,
    pulse-driven (`Tools/create_fey_jelly_assets.py`).
  - **Sounds:** a soft wet pulse each beat and a looping airy hum, synthesised by `Tools/build_fey_jelly_sfx.py`.
- **Chuck:**
  - **Controls:** the talk key ("F   Hold on") takes hold under one. The talk key or Roll lets go. Jump and Slash do
    nothing while hanging.
  - **Bites:** a bite costs Sanity, and he holds on.
  - **Camera:** it follows the jellyfish's velocity and draws back with its speed.
- **Placement:** `AFeyLife::SpawnJellies`.
  - One about 25 m inside each of the Deepwood's three ways in, and one at the middle of its loop.
  - Homes are searched at runtime: open overhead, away from mouths and out of Redcap Hollow's clearing.
  - Two more go beyond each crossing's barrier (`AFeyJelly::Barriers()`). It is empty until the crossings are built.
- **Rules:**
  - It drifts back from threshold mouths (8 m), from Redcap Hollow (26 m from the fire pit) and from barriers.
  - It shakes him off only on a threshold's way, or off its district's land.
  - A death sends every jellyfish home.
- **Checks:** `-ChuckFeyMapTest=life` steps 101 to 108.

  | Check | Result |
  |---|---|
  | `jellies_spawned` | 4 |
  | `jelly_near_entrance` | 25.5 m from each of 3 mouths |
  | `jelly_prompt`, `jelly_hold` | Pass |
  | `jelly_lifts_him` | Feet 114 cm |
  | `jelly_speed` | Top 500 cm/s |
  | `jelly_feet_clear` | 102 to 145 cm over the Deepwood's ground |
  | `jelly_swing_on_stop` | 23 degrees forward |
  | `jelly_round_trunk` | 170 cm past a trunk's face, pushed straight at it |
  | `jelly_refuses` | Closest 6.4 m to the mouth; no shake |
  | `jelly_shakes_leaving` | Pass |
  | `jelly_holds_through_bite` | Pass |
  | `jelly_let_go` | Pass |
  | `jelly_hold_again` | Pass |
  | `jelly_reset` | Pass |

  Frame time while riding: 17.8 ms (the Deepwood alone runs about 17.6).
- **Stills:** `Life_JellyDrift`, `Life_JellyTakeHold`, `Life_JellyHeld`, `Life_JellyPlayerView`, `Life_JellySpeed`,
  `Life_JellyStopSwing`, `Life_JellyTrunk`, `Life_JellyShaken`.
- **Not done or not tested:**
  - Not played by hand.
  - Sinking under a low arch is coded but no ride met one: dips=0 in the test, and one lookahead block was logged in an
    earlier run.
  - The crossings and their barriers do not exist yet.
  - The bell is a smooth dome with marks, not a detailed sculpt.
  - The arms are thin tubes with a frill wobble, not true ribbons.

## Second look and the crossings (2026-11-26, later)

- **The look:**
  - The bell is now a rounder dome (48 cm tall) with an inner bell inside it, which carries the four horseshoe marks.
  - Eight faint radial canals run over it, with eight lit points round its rim.
  - The oral arms are twisting ribbons with ruffled edges, and the tendrils carry faint beads of light.
  - Emission and opacity were rebalanced so the detail glows and the dome stays clear.
  - A NaN at the rim (a fractional power of a cosine just below zero) once hid the whole mesh. It is clamped.
- **The crossings:** the Deepwood's crossings are built ([FEYWILD-DEEPWOOD-CROSSINGS.md](FEYWILD-DEEPWOOD-CROSSINGS.md)).
  - Their barriers are full arcs, and the jellyfish never come within 2.5 m of one.
  - Two jellyfish wait either side of each crossing, twelve in all.
