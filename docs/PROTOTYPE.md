# First milestone: Waterdeep docks

## Graybox first

After Unreal installation, create a Blueprint project at Unreal/Chuck3D/Chuck3D.uproject, no starter content required, and record its exact engine version. Use built-in cubes, cylinders and flat materials. No custom Blender models until camera and scale tests.

One map, WaterdeepDocks_Prototype: roughly 12 m by 8 m of playable dock/shore, warehouse wall as boundary, one barrel nook, crate detour, rope-scale obstruction, short low ledge and a small jumpable missing-board gap. Dimensions are test starting points, not a remake of the original map. Use quiet morning colors and plain water geometry. Include a safe debug reset if Chuck falls.

Make Chuck 30.48 cm visually tall with gray body, restrained rat silhouette and purple open-jacket proxy. Tune collision to fit him. A human reference approximately 180 cm tall, a roughly 90 cm barrel and a human doorway expose scale mistakes immediately. Do not solve small-character navigation by enlarging Chuck.

## Camera experiment (mandatory before selection)

Implement two switchable rigs on the same pawn and map. Keep movement speed, collision, route, lighting and obstacles identical. Provisional values only:

- A: elevated perspective follow, about 300 cm boom and 55-degree downward pitch; tune framing for route and landing visibility.
- B: rat-height third-person follow, lens about 25–35 cm above ground and 80–120 cm behind Chuck; collision-tested boom, restrained smoothing, optional player orbit.

Repeat the same five-minute route with each: leave barrel nook, pass under/around human-scale props, round warehouse corner, jump the gap, approach the NPC, inspect, scratch the breakable, collect the cigarette, and return. Alternate A/B order on a second run. Use the same 1080p settings on this PC.

Record failed landings, camera collisions/occlusions, occasions Chuck disappears from view, ability to locate interactables without markers, sense of smallness, comfort, average FPS and frame-time spikes. Aim for a stable 60 FPS; record actual results, not assumptions. If neither camera reads well, revise geometry/framing and repeat without changing character scale. Select only after both have evidence and the user's play feedback is recorded.

| Result | Elevated | Rat-height follow |
| --- | --- | --- |
| Build/date/settings | Not tested | Not tested |
| Navigation / landing readability | Not tested | Not tested |
| Occlusion / collision count | Not tested | Not tested |
| Scale / atmosphere / comfort | Not tested | Not tested |
| Frame-time / FPS | Not tested | Not tested |

Camera decision: OPEN. Neither camera has been built or tested yet.

## Acceptance checklist

- [ ] Walk, turn and stop reliably at rat scale; walls and props block appropriately.
- [ ] Jump the small gap with readable takeoff/landing and a nearby reset on failure.
- [ ] Scratch has a short, restrained motion and bounded forward hit area.
- [ ] One grounded NPC exchange opens/closes in range; Chuck stays silent; no quest system. Use source dialogue only after confirming it fits this contained route (do not direct players to an unbuilt sewer).
- [ ] One inspectable dock object gives brief external descriptive text and exits cleanly.
- [ ] One small breakable reacts once to scratch and stops blocking after breaking; no elaborate destruction system.
- [ ] One cigarette pickup disappears once and updates session collection state once; no inventory screen or smoking system required.
- [ ] Both camera tests recorded before choosing a view.
- [ ] Windows package starts outside the editor and the same route and interactions work.
- [ ] No campaign content, original saves, deployment or repository state changed.

Suggested implementation order: scaled pawn and primitives; both cameras with walking/jump; shared nearby-interaction handling; NPC/inspect; scratch/breakable; pickup; packaged Windows regression. Keep a single small slice throughout.
