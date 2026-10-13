# Chult canopy grove — setting contract

User-authorized October 13, 2026: the original grove and later revision to make
its trees more natural, rounded, rooted and suitable for jumps between separate
branches. This contract supersedes the first straight branch/static-vine layout.
Codex owns setting and verification; Claude owns actual cat-to-cat climbing,
brachiation, stamina and camera integration. This remains an inactive traversal
setting preview, not a completed brachiation milestone.

## Location and access

Coordinates are centimetres relative to `AChultLanding::FloorOrigin()`:
(400000,2500000,0). The grove connects to the original landing loop at
(4200,500,0), approaches fork 0 through (5300,900,0), and returns through
(4600,1350,0). The floor, cinematic clearing and recovery point remain intact.

`ChultCanopy.cpp` authors the grove. Random trunks and colliding understory are
excluded from X=5000–8800, Y=250–3000. Imported island trees remain outside this
reserve with a 650 cm margin; noncolliding scanned ferns dress its ground.
Preserve the ground lanes when expanding roots or foliage. The Poly Haven kit
and existing bark/leaf meshes/materials are reused; no binary source regenerated.

| Fork marker | Local standing reference | Route |
| --- | --- | --- |
| `ChultCanopyFork0` | (5600,900,650) | First entry and two branch choices |
| `ChultCanopyFork1` | (7200,900,650) | Standing pause and onward branch |
| `ChultCanopyFork2` | (8200,1800,760) | Higher overlook |
| `ChultCanopyFork3` | (6800,2400,650) | Return loop and recovery |

The adjacent stone causeway joins fork 0 from the south. Its 360 x 360 cm landing
is centred at (5530,820), top Z=670, 20 cm above the wood. Approach along X=5460
to Y=900, then east, around the south trunk. See [CHULT-RUINS.md](CHULT-RUINS.md).
The user explicitly deferred impassable boundary cliffs.

## Rounded trunks, standing wood and roots

Each fork has paired trunks with centres Y +/-160 from the hub. Their inner
climbing strips remain at Y +/-60: **120 cm clear separation**. Trunk exteriors
now use 32-sided, 24-ring tapered/flared bark. The inner strip is intentionally
flat; its exact width varies with the rounded section, superseding the original
fixed 140 cm face width. Continuation boughs curve outward from the tops rather
than beginning as thin poles on square stumps.

Broad round limbs cross and extend beside the tops for resting/dismounts. The
standing marker names the highest limb surface, not a guaranteed resting capsule
transform. Rendered solid bark supplies query/physics collision; there are no
hidden rectangular platforms. Claude still needs actual climbing/standing tests.

Outward buttress ribs broaden from the bases, taper along curved 3.5–5.3 m
surface roots, and split into smaller toes. Root tips sink into the ground.
Candidate roots near the ground recovery route are omitted. Inner climbing gaps
stay open. The visible root surface supplies collision.

## Separate reaching branches and paw proposals

`ChultCanopyChains()` returns four named chains with local paw positions,
`bVine=false` and `BranchBreak`, the first paw on the neighbouring tree. Each
chain comprises **two independently supported curved boughs**; wood never spans
the branch break. Chain 2 is now a branch pair, replacing the earlier static vine.

| Chain | Direction |
| --- | --- |
| `ChultCanopyChain0` | Fork 0 → 1 |
| `ChultCanopyChain1` | Fork 1 → 2 |
| `ChultCanopyChain2` | Fork 2 → 3 |
| `ChultCanopyChain3` | Fork 3 → 0 |

Endpoint paws remain 300 cm out and 115 cm above their standing fork. Proposals
arc up 45 cm and bend sideways 35 cm. Along each bough they are spaced at most
about 140 cm apart; the two jump paws are **180 cm apart**, leaving nominal
**140 cm of air** between tapered wooden tips. Boughs taper from 67 cm radius
at a supporting crotch, through 28 cm at the initial paw, to about 15 cm near the
jump and a 2 cm rounded tip. Swept curves create continuous surfaces within each
individual branch, avoiding a series of capped straight tubes.

`BuildChultCanopy()` creates `<ChainName>_PawNN` scene components with forward
horizontal tangent and `ChultCanopyGripProposal`. The first paw after each jump
also has `ChultCanopyBranchJump`. **No `AddChuckSwingGrip` calls occur.** These
branches cannot be caught yet. High hand boughs stay noncolliding until actual
leap/contact checks; solid trunks, roots and standing limbs collide.

Claude must integrate the revised coordinates, audit registry lifetime/local
respawn/re-entry, test forward/reverse leaps and catches, and adjust contact as
needed. Old cached paw arrays are stale. Fork choice/dismount requires explicit
verification; chain-end no-input selection can otherwise choose the previous
grip. The 300 cm fork transitions are standing breaks, not automatic grip links.
No character/controller, control mapping, stamina or camera code changed here.

## Leaf crowns and camera ownership

Eight components keep `ChultCanopyCrown<I>_<Side>` names and the
`ChultCanopyFoliage` tag. They now contain hierarchical instances of the existing
individual leaf mesh, with five asymmetric lobes on curved smaller boughs per
crown. Air between leaves replaces the old opaque mushroom core. Crowns are
noncolliding and sit above/to the sides of the hand routes.

If camera play needs partial fading, handle these instanced components and
restore their material instances on exit. No fade is implemented. Manual orbit,
elevated framing, paw visibility and camera comfort must be checked in play.

The grove still uses procedural junctions, paired climbing strips and the flat
forest floor. It does not reach the supplied art references. No raptors,
encounter territory, raptor-safety claim, buff-only teaching tree or golden pickup
is added. Ordinary paired climbs should work without a buff, subject to Claude's
integration and tests.

## Verification boundaries

`-ChuckChultTest` retains 11 real-capsule grove ground sweeps and metre-spaced
floor samples, the original landing checks and five real-character ruin trials,
including the stone-to-fork join. The layout marker checks consecutive proposed
reach against 40–240 cm horizontal and at most 70 cm vertical in either direction;
asset inventory checks bark and the reused leaf mesh. Cooked review captures are
`Canopy_Entry`, `Canopy_Overview`, `Canopy_Fork`, `Canopy_BranchGap`, `Canopy_Roots`.

These prove setting access/layout and visual evidence only. Actual cat-to-cat
climbing, brachiation catches, fork choices, dismounts, missed catches, empty
stamina, physical Xbox and camera/fading remain Claude's next work. Update the
verifier's `registered_grips=0` expectation when activating the chains.

Next part of the work can only be done by Claude: integrate and test canopy
traversal on the revised setting, then assess selective crown fading.
