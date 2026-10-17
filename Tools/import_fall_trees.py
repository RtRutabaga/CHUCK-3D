"""Reimport only the jungle trees SM_FallTree_A/B (Claude, 2026-10-13).

UnrealEditor-Cmd <uproject> -ExecutePythonScript=Tools/import_fall_trees.py -unattended -nosplash -NoLiveCoding

After `blender --background --python Tools/build_fall_jungle.py -- --only SM_FallTree_A,SM_FallTree_B`
(buttress fins that leave the trunk tangentially). Uses the same FBX settings as
import_fall_cutscene.py and reassigns the existing materials by slot name; no
material, texture or other mesh is created or changed, so Codex's wet-forest
material finish stays as it is.
"""
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'SourceAssets/Cutscenes/FallToChult'
DEST = '/Game/Art/Cutscenes/FallToChult'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
ASSETS = unreal.EditorAssetLibrary

mats = {k: unreal.load_asset(f'{DEST}/{k}') for k in ('M_FallBark', 'M_FallLeaves', 'M_FallCanopy', 'M_FallCrownLeaves')}
for k, m in mats.items():
    if not m: raise RuntimeError(f'Missing material {k}')

options = unreal.FbxImportUI()
options.import_mesh = True; options.import_as_skeletal = False
options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
options.automated_import_should_detect_type = False
options.import_materials = options.import_textures = options.import_animations = False
data = options.static_mesh_import_data
data.set_editor_property('combine_meshes', True)
data.set_editor_property('auto_generate_collision', False)
data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_COMPUTE_NORMALS)
data.set_editor_property('vertex_color_import_option', unreal.VertexColorImportOption.REPLACE)
for name in ('SM_FallTree_A', 'SM_FallTree_B'):
    task = unreal.AssetImportTask()
    task.filename = str(SOURCE / f'{name}.fbx'); task.destination_path = DEST; task.destination_name = name
    task.automated = True; task.replace_existing = True; task.save = False; task.options = options
    TOOLS.import_asset_tasks([task])
    mesh = unreal.load_asset(f'{DEST}/{name}')
    if not isinstance(mesh, unreal.StaticMesh): raise RuntimeError(f'Missing mesh {name}')
    slots = mesh.get_editor_property('static_materials')
    for i, slot in enumerate(slots):
        slot_name = str(slot.get_editor_property('material_slot_name'))
        key = next((k for k in sorted(mats, key=len, reverse=True) if slot_name.startswith(k)), None)
        if key is None: raise RuntimeError(f'{name}: unexpected slot {slot_name}')
        mesh.set_material(i, mats[key])
    ASSETS.save_loaded_asset(mesh, only_if_is_dirty=False)
    box = mesh.get_bounding_box()
    unreal.log(f'CHUCK_FALL_TREE_IMPORT {name} slots={len(slots)} height_cm={box.max.z - box.min.z:.0f}')
unreal.log('CHUCK_FALL_TREES_IMPORT_READY')
