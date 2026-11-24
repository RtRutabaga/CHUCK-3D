"""Import the Crossroads trio (Tools/build_fey_trio.py, Tools/build_fey_trio_anims.py) into /Game/Characters/FeyTrio.

UnrealEditor-Cmd <uproject> -ExecutePythonScript=Tools/import_fey_trio.py -unattended -nosplash -NoLiveCoding
(then Tools/import_npc_grooms.py with CHUCK_GROOMS_SET=fey for their strand hair and beards).

Each of the three (FeyCentaur, FeySatyr, FeyGnome) has its own skeleton (SKEL_<Name>), generated LODs and one clip,
AS_<Name>_Mill: the loop they play together (no root motion extraction: the centaur's Root bone carries all three round
their circle, so the game leaves the actor still; the meshes' bounds are widened to cover it). Materials as the temple
heroes' (Tools/import_temple_heroes.py, whose functions this uses, with its masters in /Game/Characters/Heroes): the
kit's types and the 'pbr' surfaces (coat, hoof, horn, blanket, gilt, leather, felt).
"""
from pathlib import Path
import importlib.util
import json
import os
import unreal

ROOT = Path(__file__).resolve().parents[1]
os.environ['CHUCK_HEROES_ONLY'] = '__none__'      # its functions only
_s = importlib.util.spec_from_file_location('heroes_import', ROOT / 'Tools/import_temple_heroes.py')
H = importlib.util.module_from_spec(_s); _s.loader.exec_module(H)
SOURCE = ROOT / 'SourceAssets/NPCs/Fey'
DEST = '/Game/Characters/FeyTrio'
H.SOURCE, H.DEST = SOURCE, DEST
MANIFEST = json.loads((SOURCE / 'manifest.json').read_text(encoding='utf-8'))['npcs']
TOOLS, ASSETS = H.TOOLS, H.ASSETS
ONLY = {x.strip() for x in os.environ.get('CHUCK_TRIO_ONLY', '').split(',') if x.strip()}


def import_one(name, info):
    folder = f'{DEST}/{name}'
    o = unreal.FbxImportUI()
    o.import_mesh = True; o.import_as_skeletal = True; o.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    o.automated_import_should_detect_type = False; o.import_materials = o.import_textures = o.import_animations = False; o.create_physics_asset = False
    skel_path = f'{folder}/SKEL_{name}'
    existing = unreal.load_asset(skel_path)
    if existing: o.skeleton = existing
    d = o.skeletal_mesh_import_data
    d.set_editor_property('update_skeleton_reference_pose', True)
    d.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    d.set_editor_property('vertex_color_import_option', unreal.VertexColorImportOption.REPLACE)
    old = unreal.load_asset(f'{folder}/SK_{name}')
    if old and {str(m.get_editor_property('material_slot_name')).split('.')[0] for m in old.get_editor_property('materials')} - set(info['slots']):
        ASSETS.delete_loaded_asset(old)
    t = unreal.AssetImportTask(); t.filename = str(SOURCE / info['fbx']); t.destination_path = folder; t.destination_name = f'SK_{name}'
    t.automated = True; t.replace_existing = True; t.save = True; t.options = o
    TOOLS.import_asset_tasks([t])
    mesh = unreal.load_asset(f'{folder}/SK_{name}')
    if not isinstance(mesh, unreal.SkeletalMesh): raise RuntimeError(f'{name}: not a skeletal mesh')
    if not existing:
        made = mesh.get_editor_property('skeleton')
        if not ASSETS.rename_asset(made.get_path_name().split('.')[0], skel_path): raise RuntimeError(f'{name}: skeleton rename')
        mesh = unreal.load_asset(f'{folder}/SK_{name}')
    materials = {slot: H.slot_material(name, folder, slot, s) for slot, s in info['slots'].items()}
    mats = mesh.get_editor_property('materials')
    for i, m in enumerate(mats):
        slot = str(m.get_editor_property('material_slot_name')).split('.')[0]
        if slot not in materials: raise RuntimeError(f'{name}: unexpected slot {slot}')
        m.material_interface = materials[slot]; mats[i] = m
    mesh.set_editor_property('materials', mats)
    # The centaur's Root carries them up to 4 m from the actor: bounds wide enough not to be culled away.
    mesh.set_editor_property('positive_bounds_extension', unreal.Vector(500, 500, 150))
    mesh.set_editor_property('negative_bounds_extension', unreal.Vector(500, 500, 150))
    lod_ok = False
    try:
        sub = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
        lod_ok = sub.regenerate_lod(mesh, 4, True, False)
        unreal.log(f'CHUCK_TRIO_LOD_MATERIALS {name} ' + unreal.ChuckReviewLibrary.fix_lod_materials(mesh, True))
    except Exception as e:
        unreal.log_warning(f'CHUCK_TRIO_LOD_FAILED {name} {e}')
    ASSETS.save_loaded_asset(mesh, only_if_is_dirty=False)
    skeleton = mesh.get_editor_property('skeleton'); ASSETS.save_loaded_asset(skeleton, only_if_is_dirty=False)
    a = unreal.FbxImportUI()
    a.import_mesh = False; a.import_as_skeletal = True; a.import_animations = True; a.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION
    a.automated_import_should_detect_type = False; a.skeleton = skeleton; a.import_materials = a.import_textures = False
    ad = a.anim_sequence_import_data
    ad.set_editor_property('use_default_sample_rate', False); ad.set_editor_property('custom_sample_rate', 30)
    ad.set_editor_property('remove_redundant_keys', False); ad.set_editor_property('import_bone_tracks', True)
    ad.set_editor_property('animation_length', unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
    t = unreal.AssetImportTask(); t.filename = str(SOURCE / name / 'Anim' / f'AS_{name}_Mill.fbx'); t.destination_path = f'{folder}/Anim'; t.destination_name = f'AS_{name}_Mill'
    t.automated = True; t.replace_existing = True; t.save = False; t.options = a
    TOOLS.import_asset_tasks([t])
    anim = unreal.load_asset(f'{folder}/Anim/AS_{name}_Mill')
    if not isinstance(anim, unreal.AnimSequence) or anim.get_editor_property('skeleton') != skeleton: raise RuntimeError(f'{name}: clip')
    anim.set_editor_property('enable_root_motion', False)
    ASSETS.save_loaded_asset(anim, only_if_is_dirty=False)
    box = mesh.get_bounds().box_extent
    unreal.log(f'CHUCK_TRIO_IMPORTED {name} slots={len(materials)} extent=({box.x:.0f},{box.y:.0f},{box.z:.0f}) lod_ok={lod_ok} clip={anim.get_editor_property("sequence_length"):.2f}')


for name, info in MANIFEST.items():
    if not ONLY or name in ONLY: import_one(name, info)
unreal.log('CHUCK_TRIO_IMPORT_READY')
