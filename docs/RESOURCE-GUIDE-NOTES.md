# Applying the supplied resource guide

Read on 2026-09-26. The unedited source is `References/chuck-3d-resource-guide.md`; provenance and hash are recorded in `References/PROVENANCE.md`.

Apply its reuse-first approach: evaluate existing assets/animations before writing more procedural art generators; use Codex for integration, verification and custom movement; prioritize Blender editing and reusable source for art. Claude has already delivered a v1 rig, nine clips and baked maps through `c5d6d25`, so validate those rather than discarding them or starting another generator pipeline. For future clips, evaluate retargeted reference motion against Chuck's proportions before importing a full movement framework. Keep tasks bounded and reuse the existing project handoffs.

The supplied guide explicitly calls its advice optional. Its It Takes Two/indie-AA suggestion does not override the user's Baldur's Gate 3-style character reference. Its broad parkour/dialogue slice is a later direction: current work remains Chuck's appearance, walking/jumping, contact quality and both cameras. World expansion, combat and campaign porting remain deferred. Original CHUCK-game stays read-only; approved story references are preserved, and dialogue must never give Chuck speech.

## Resource checks affecting decisions now

- **Megascans:** do not assume the entire library is free for Unreal. Epic announced paid content from 2025, with a selected free subset. Check each listing and entitlement before acquiring an asset. [Epic announcement](https://forums.unrealengine.com/t/reminder-free-megascans-ends-soon/2203090).
- **Local image-to-3D:** the original Microsoft TRELLIS instructions require at least 16 GB GPU memory; this PC has 8 GB VRAM. Hunyuan3D-2 documents about 6 GB for shape alone and 16 GB for shape plus textures, with separate low-memory options. Neither is an automatic ready-to-use full art pipeline here. No installation or paid/cloud service is needed for the delivered rig. [TRELLIS](https://github.com/microsoft/TRELLIS/blob/main/README.md), [Hunyuan3D-2](https://github.com/Tencent-Hunyuan/Hunyuan3D-2/blob/main/README.md).
- **Groom, cloth and Control Rig:** possible later evaluations, not requirements for the current import. First verify the skinned model and existing maps on this hardware. Preserve the accepted first migration without a new Control Rig dependency; reconsider when an actual tail/authoring requirement justifies it.
- **Material wiring:** use baked base colour as Base Color and ORM.R as Ambient Occlusion, rather than unconditionally multiplying AO into albedo as the V1 readme suggests. ORM.G/B supply roughness/metallic. Inspect DirectX normal orientation on import.
- **Licences and costs:** the guide's price/license summaries are discovery pointers, not acquisition approval or redistribution evidence. Verify the specific version/asset when selected, especially before adding third-party source to this public repository. No third-party downloads or new software installs were performed for this review.

Next step: validate Claude's committed v1 source, import it beside the old skeleton with the nine clips/textures, and check the real Unreal transforms/materials before switching the playable character. No need to repeat environment research while character work is pending.
