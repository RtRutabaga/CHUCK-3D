# Art direction

The user supplied two character images on 2026-09-24 and requested Baldur's Gate 3 quality as the long-term graphics target. These images take precedence over the procedural model for future visual development. They are reference only, not game-ready assets.

- Chuck: believable upright rat anatomy, a defined muzzle and whiskers, rounded translucent ears, small paws/claws, a long segmented tail, and layered natural gray-brown fur with a lighter chest. Preserve his quiet, observant characterization; do not derive exaggerated reactions or combat behavior from the dynamic pose.
- Jacket: oversized purple, open at the chest, with a collar, lapels, visible seams, worn fabric, pockets and convincing folds. Keep the purple silhouette readable from both cameras.
- World: grounded, richly detailed fantasy harbor; weathered timber, rope, stone, metal and cloth with physically believable surfaces. Warm daylight, soft atmospheric depth, and restrained reflective wet surfaces. The references' distant city communicates environment quality, not authorization to build a city or expand the map.
- Quality target: the material richness, character fidelity and lighting polish associated with Baldur's Gate 3. Develop original assets in Blender and Unreal; the current primitives are neither finished art nor evidence that this fidelity/performance target has been reached.
- Scale: 65 cm from feet to top of ears beside a 180 cm human. This should reach above the kneecap and replaces the initial 30.48 cm constraint. Keep human/world measurements intact; scale collision and camera framing alongside Chuck.

The cigarette is part of the supplied visual reference. Cigarette pickup/interaction remains deferred with the other gameplay interactions. No new dialogue, campaign scope, or character powers are implied.

Character-first update, 2026-09-26: stop environment improvements after the in-flight tavern pass. The user identifies primitive forms, cartoony/unnatural foot placement and gaps along the open jacket edges. Prioritize believable character construction and grounded motion over more surface/world detail. Plan the eventual rig for walk, run, roll, side-jump, climb and smoking with the cigarette retained in the mouth. See CHARACTER-PLAN.md for evidence and sequencing; existing screenshots and tests are not proof that the reference quality has been reached.

## Goal images, 2026-09-27

The user supplied four new goal images before camera or traversal work, asking to bring Chuck closer to them first. They refine, not replace, the 2026-09-24 pair:

- `Chuck-Turnaround.png` (front, 3/4, both profiles, back): the main proportion and design sheet.
- `Chuck-Standing-Smoking.png`: relaxed standing pose, 3/4 view.
- `Chuck-Run-Profile.png`: a single run pose in profile.
- `Chuck-Run-Cycle-Sheet.png`: eight views of the run cycle, the future run clip reference.

What they fix, measured on the turnaround at 65 cm feet-to-ear scale (approximate):
- **Coat:** warm taupe-brown, shaggy and clumped, not sleek grey. Darker, spiky crown and nape; short fur on the muzzle; fluffy thighs. The belly is beige-cream.
- **Head:** very large, round, thin pink ears, pink on the back as well; long tapering muzzle; pink nose; dark glossy eyes; fine whiskers.
- **Jacket:** saturated red-violet suede/brushed canvas with visible seams, pointed shirt collar, silver zippers on both open edges, cuffed sleeves reaching the wrists. The jacket is **cropped**: hem at about 25 cm, collar top at about 53 cm, so the hips and thighs show below it.
- **Limbs:** long pink hind feet with long toes and pale claws, heels planted; pink hands with long fingers and claws.
- **Tail:** long, pink and ringed, thick at the base.
- **Cigarette:** held in the mouth, with a thin smoke wisp, in every view. As before, cigarette pickup and interaction stay deferred. The prop itself is a character-art item.
- **Motion:** the run sheets show a long-striding, forward-leaning run with the arms pumping and the tail streaming. Run is a planned rig capability, not yet a gameplay feature (AGENTS.md).

## Reference provenance

- `References/ArtDirection/Chuck-Motion.jpg`: user attachment Photo 1.jpg, original filename `1-Photo-1.jpg`.
- `References/ArtDirection/Chuck-Standing.jpg`: user attachment Photo 2.jpg, original filename `2-Photo-2.jpg`.
- Both copied byte-for-byte from the attachments supplied in this conversation, attachment group `CCD8B07B-97EF-4E90-B754-1575EE2AD87B`. No original CHUCK repository assets were changed or copied.
- Combined size: 537,615 bytes. Existing .jpg Git LFS policy applies; this is less than 1 MB of new LFS storage. Prior LFS upload/download verification succeeded. No paid storage purchase is needed or authorized.
- SHA-256 Chuck-Motion.jpg: `39709538C2FCC2D538947FE97E1B70DC249F1F1F6064C22D6FD2039F9C923995`
- SHA-256 Chuck-Standing.jpg: `97D69A54A3972657092F9F7DC943A09EE90CF7E4B690815292A2AED8C94D9DEC`
- 2026-09-27 goal images, copied byte-for-byte from the user's Claude Code attachments (upload ids in parentheses). PNG, stored with Git LFS (8.6 MB in total):
  - `References/ArtDirection/Chuck-Standing-Smoking.png` (`1521ddbe`), SHA-256 `95CF5D2EB06EC4D83297B6DA41D6EDC859C4A928C335BF6A412A6F0AA9AE9DD1`
  - `References/ArtDirection/Chuck-Run-Profile.png` (`3e587028`), SHA-256 `EB5A0256679AB6C3A4E0E81F73C6DAE9D0EBD738DE99662840938C39BC824B65`
  - `References/ArtDirection/Chuck-Turnaround.png` (`1ddec5d2`), SHA-256 `63830591F3BA153BBA4DFADAF70F419D6D93E4298AB1E0162D195B4F25A5DB74`
  - `References/ArtDirection/Chuck-Run-Cycle-Sheet.png` (`c811172a`), SHA-256 `65817DE242CC12E92A5B75FEC9BEDAA04C353AA0418F8F6CC84A0AE222842B26`
