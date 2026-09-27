# Chuck v1 source and Unreal import gate

Claude's source delivery through `c5d6d25` is integrated, including the two preceding legacy-art passes, the 41-bone rig, all nine clips, ankle/ground-clearance fixes, UVs and three baked maps. The playable character still uses the older 14-bone runtime and imported assets. This milestone establishes the new assets beside it; it does not switch gameplay or certify the new controller.

## Reproduction

From the project root:

```powershell
powershell -NoProfile -File Tools/Import-ChuckV1.ps1 -Review
```

Uses installed Unreal 5.7.4, committed FBXs and PNGs. No Blender generator runs. The wrapper imports, verifies rest positions and animated clips, repairs loop endpoint intervals, and optionally builds the editor review helper and renders review views in a temporary editor world. It refuses to start while another Unreal editor or Blender process is running. The review world is not a new playable map and is not saved into project content. Evidence goes under ignored `Local/`.

For source checks, use Blender 4.5.14 LTS with `--background SourceAssets/Chuck/V1/Chuck_V1.blend --python SourceAssets/Chuck/V1/check_v1.py`. This reads the delivered blend and reimports its FBX in memory; it does not save over source.

## Verified

- Documented `Import-ChuckV1.ps1 -Review` workflow passed end to end: `Local/v1-wrapper-final.log`, import log `Local/import_chuck_v1.py-20260926-173057.log`, review log `Local/review_chuck_v1_unreal.py-20260926-173114.log`. Both required success markers were present, without Python/material/fatal errors.
- Source validation: **66 passing checks**, `Local/v1-source-check.log`; skeleton, weights, size, materials, every clip and ground clearance, exact start/stop seams, exported helpers.
- Unreal imported mesh: `/Game/Characters/Chuck/V1/SK_Chuck`; 41 required bones plus the identity `SK_Chuck_Rig` container. Maximum sampled reference-head position error against Y-reflected source: **0.0000094 cm**.
- Nine clips imported against the new skeleton. Root position remains static at sampled times; actual pelvis/foot/head motion is present. These are source/raw-pose import checks, not compressed packaged-motion certification.
- The source excludes duplicate loop endpoints. Unreal initially imported WalkLoop as 0.2667 s rather than 0.30 s. The validator appends the first pose as a closing sample, retaining the source's frame cadence: Idle 2.0 s, WalkLoop 0.30 s, JumpLoop 0.40 s. Re-running validation is idempotent. Source FBXs remain unchanged.
- Three 2048² textures imported with sRGB BaseColor, linear ORM masks and DirectX normal compression, without another green-channel flip. The material uses independent AO, roughness and metallic channels. All nine slots point to the v1 material; the importer verifies persistence. An initial struct-array assignment issue was corrected before integration.
- Source sole markers supersede the proposal estimates: heel X=-5.541, ball X=3.4, toe X=7.305 at Y=±7, Z=0. These still need bone-relative socket construction and evaluated contact telemetry in the runtime migration.

## Review and remaining work

`Local/V1UnrealReview/` contains engine-rendered material and pose review images. Poses are explicitly evaluated from imported clips into an editor-only pose component, not a test of an Animation Blueprint or movement controller. The neutral model is more coherent and materials distinguish jacket/chest/skin, but the geometric fur, stylized face and overlapping ankle construction still fall short of the supplied reference quality. No groom, cloth simulation or LODs are claimed.

The initial editor capture path rendered the reference pose despite correct animation data. `ChuckReviewLibrary::RefreshEditorPose` now explicitly refreshes bone transforms in a non-game editor world; it refuses game worlds and is disabled outside editor builds. The review checks pelvis, feet and head positions against evaluated animation data within 0.001 cm. The corrected walking and compressed landing renders were visually inspected and show distinct poses. Screenshot scheduling guards against Slate callback reentry. The editor module build passed (`Local/v1-review-module-build.log`); this does not substitute for a packaged gameplay build.

The next integration task is **SkeletalMesh + AnimInstance/AnimBP and contact migration**, as agreed in RIG-CONTRACT-V1.md: synchronize capsule travel/yaw with the manifest, derive rest-space poles and sole offsets, measure the actual deformed contacts, verify both cameras and physical collision, and only then switch the default character. The old `Build-ChuckAssets.ps1` targets the legacy generator; use the v1 importer for this delivery. Run/roll/climb/smoking gameplay and world expansion remain deferred.

The supplied resource guide is preserved in `References/chuck-3d-resource-guide.md`; `RESOURCE-GUIDE-NOTES.md` records current applicability and corrections. No external assets, local models, plugins or paid services were installed for this milestone.
