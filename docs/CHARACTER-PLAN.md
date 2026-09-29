# Chuck first: visual and movement rework

The user's 2026-09-26 assessment is the acceptance baseline: Chuck remains far from the goal images, his movement feels cartoony, his legs land unnaturally, his appearance is primitive/simplistic, and the jacket has unnatural gaps around the open zipper/placket edges. Passing the existing smoke tests does not contradict or resolve these defects. Stop adding world detail until character work has addressed this priority.

Keep 65 cm feet-to-ear height beside the 180 cm worker, natural gray-brown rat anatomy, oversized open purple jacket and restrained silent characterization. Read ART-DIRECTION.md and inspect the References/ArtDirection images (the 2026-09-24 JPG pair and the 2026-09-27 turnaround, standing and run PNGs). Aim for credible anatomy, garment construction and motion before adding more surface detail. The latest geometric fur is still a prototype, not a production groom or a substitute for good forms.

## Source-level findings to verify visually

| Problem | Evidence in current source | Implication |
| --- | --- | --- |
| Sliding/unnatural feet | ChuckCharacter::UpdateMotion advances gait every 46 cm travelled but moves each independent foot only +/-3 cm in actor space, with sinusoidal lift. No ground trace or world-space stance lock. | At 95 cm/s, maximum backward foot speed from this cycle is about 39 cm/s: a planted-looking foot still moves forward in world space. Merely reducing bob cannot fix this. |
| Unnatural knee/ankle behavior | UpdateSkeleton solves to separate static feet using a fixed backward bend direction. Feet are outside the skeletal mesh; the rig has no foot/toe bones. | Contact, turn-in-place, slopes and transitions need a coherent rig and contact model. |
| Jacket-edge gaps | OpenJacket uses angular shell boundaries; FrontSeam, lapels and fasteners use independently typed coordinates. At the mid-body, the seam is displaced inward/forward from the shell edge. | Build plackets/lining/zipper details from the same garment boundary, not floating pieces. Confirm front and three-quarter views, including motion. |
| Primitive deformation | Thigh/shin and sleeve pieces overlap; source parts usually receive a single rigid bone weight. | Continuous joint topology and graduated skin weights are required for deeper bends and believable motion. |

These describe the pre-rework baseline. The 2026-09-26 integrated pass replaces sinusoidal foot translation with traced world-space stance/swing, and Claude rebuilt the jacket fronts and graded sleeve weights. See HANDOFF.md and both agent handoffs for measured results and remaining defects; these improvements do not establish final character quality.

## Ordered work

1. **Jacket and neutral silhouette:** eliminate unintended edge gaps, give the open fronts thickness/lining, connect collar/lapels credibly, refine muzzle/torso/limb/paw proportions against the supplied images. Keep the existing rig contract initially so this can integrate independently of movement work. Do not cover faults with more fur or accessory meshes.
2. **Grounded walking:** separate stance from swing, measure world-space foot slip, settle starts/stops and turns, and keep knees/ankles within anatomically plausible ranges. Keep collision/cameras stable. Capture actual motion from front/side/rear at rat height and elevated view; a still pose or sinusoid-amplitude assertion is not acceptance.
3. **Production rig proposal:** agree on a single deforming body, pelvis/spine/neck, appropriate limb/foot/toe/hand controls and skin weights, then migrate runtime and assets together. Do not silently rename bones in one branch while the other targets the old rig. Decide authored clips versus procedural corrections deliberately; the current PoseableMesh experiment is not a requirement for the final architecture.
4. **Reference-quality surfaces:** UVs, coherent fur solution, eyes/ears/paws, worn woven jacket and controlled material detail after forms and motion work. Track triangle/texture costs and LOD needs. Claude's current body has 159,524 triangles plus two 10,944-triangle feet, with no LODs.

Immediate art follow-up after the first integration: the open fronts are more coherent, but visible shoulder/armhole gaps and overlapping caps remain in the packaged front/rear views. The head, hands, legs and paws still read as a primitive form study. Prioritize continuous shoulder and limb construction and reference silhouette before more garment details or environment work. Codex's first long-step contact trial exposed ankle separation; the corrected short-stride pass adds a reach regression, but the separate-foot rig still needs eventual replacement.

## Eventual action requirements

| Action | Rig/animation provision to plan | Current status |
| --- | --- | --- |
| Walk | Planted stance, toe/ankle articulation, believable weight transfer and turns | Playable, visual rework required |
| Run | Separate stride/contact timing, acceleration/deceleration, tail/garment follow-through | First pass (2026-09-27): `RunLoop` (190 cm/s, 0.4 s cycle, duty 0.3, flight phase), hold-to-run, speed-blended with the saunter on a shared phase. Running jump (2026-09-27): `RunJump` split leap posed over the flight's progress, 23 cm apex and about 92 cm at run speed; lands straight into the stride with the stick held, or brakes hard into JumpLand |
| Roll | Continuous torso/hip deformation, clearance, recover-to-stance transition | First pass (2026-09-27): `Roll` clip, about 1 m in 0.8 s, from and back to the aplomb stance, ground-fitted tuck. Capsule is not shrunk, so he can't yet roll under the bench |
| Side-jump | Lateral takeoff/landing, pelvis orientation, bilateral foot placement | First pass (2026-09-27): `SideJumpLeft`/`SideJumpRight`, about 77 cm sideways with a 16 cm apex, facing kept, landing on both paws in the aplomb stance |
| Slash (claw scratch) | Readable wind-up, fast rake, follow-through, recover; works standing and on the move | First pass (2026-09-28): `SlashRight`/`SlashLeft` claw rake with a 10 cm step-in, chained by a second press; upper-body layer over walk/run/air. No hit detection or targets yet |
| Climb | Reach/grip controls, hands/fingers, feet and root alignment to climb surface | Not implemented |
| Smoke | Stable mouth/head attachment for a cigarette retained in the mouth; restrained jaw/lip pose and smoke origin; avoid face/jacket clipping | Not implemented; pickup remains deferred |

No new combat, dialogue, campaign, maps or power progression is implied. Plan these capabilities now; do not implement all actions before walking and the base model are credible.

## Review evidence

- Neutral front, side, rear and three-quarter images next to the scale worker, with the supplied reference visible separately for comparison.
- Close views of both open jacket edges, collar/shoulder and elbow bends. No unintended holes, floating plackets or exposed interior faces; the intentional open chest remains.
- Short motion captures covering start, straight walk, turn, stop, ordinary jump/landing and reset in both cameras. Inspect foot contacts frame by frame; log contact drift and gross knee flips. Do not claim physical Xbox testing unless performed.
- Record exact source/rig commit, engine/tool versions, bounds, materials, topology count, what was generated versus manually authored, and known remaining flaws.
- Retain behavioral checks for scale/collision/cameras, but replace obsolete tests tied to the old lean/bob formula as the motion implementation changes. Passing tests is necessary, not a visual quality verdict.
