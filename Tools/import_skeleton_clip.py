"""Import one or more of the temple skeleton's clips only (Claude, 2026-11-12), against the existing SK_TempleSkeleton.

UnrealEditor-Cmd <uproject> -ExecutePythonScript=Tools/import_skeleton_clip.py -unattended -nosplash -NoLiveCoding
  env CHUCK_SKELETON_CLIPS=Throw[,..]  (default Throw)

import_temple_skeleton.py also reimports the mesh, regenerates its LODs and sets its materials; a new clip needs none of
that. Same options as there.
"""
import json
import os
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'SourceAssets/Enemies/Skeleton'
DEST = '/Game/Characters/Skeleton'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
ASSETS = unreal.EditorAssetLibrary
CLIPS = json.loads((SOURCE / 'skeleton-clips.json').read_text())['clips']
WANT = [c for c in os.environ.get('CHUCK_SKELETON_CLIPS', 'Throw').split(',') if c]
mesh = unreal.load_asset(f'{DEST}/SK_TempleSkeleton')
skeleton = mesh.get_editor_property('skeleton')
for name in WANT:
    clip = CLIPS[name]
    o = unreal.FbxImportUI()
    o.import_mesh = False
    o.import_as_skeletal = True
    o.import_animations = True
    o.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION
    o.automated_import_should_detect_type = False
    o.skeleton = skeleton
    o.import_materials = o.import_textures = False
    o.anim_sequence_import_data.set_editor_property('use_default_sample_rate', False)
    o.anim_sequence_import_data.set_editor_property('custom_sample_rate', 30)
    t = unreal.AssetImportTask()
    t.filename = str(SOURCE / clip['fbx']); t.destination_path = DEST + '/Animations'; t.destination_name = f'AS_Skeleton_{name}'
    t.automated = True; t.replace_existing = True; t.save = False; t.options = o
    TOOLS.import_asset_tasks([t])
    anim = unreal.load_asset(f'{DEST}/Animations/AS_Skeleton_{name}')
    if not isinstance(anim, unreal.AnimSequence) or anim.get_editor_property('skeleton') != skeleton:
        raise RuntimeError(f'Wrong animation/skeleton for {name}')
    anim.set_editor_property('enable_root_motion', False)
    ASSETS.save_loaded_asset(anim, only_if_is_dirty=False)
    unreal.log(f'CHUCK_SKELETON_CLIP_IMPORT {name} length={anim.get_editor_property("sequence_length"):.3f} source={clip["frames"] / clip["fps"]:.3f}')
unreal.log('CHUCK_SKELETON_CLIPS_READY')
