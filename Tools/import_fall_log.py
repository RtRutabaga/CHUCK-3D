"""Re-import only SM_FallLog (Claude, 2026-10-27), keeping the existing materials.

blender --background --python Tools/build_fall_jungle.py -- --only SM_FallLog
UnrealEditor-Cmd <uproject> -ExecutePythonScript=Tools/import_fall_log.py -unattended -nosplash -NoLiveCoding

import_fall_cutscene.py rebuilds the base materials (and overwrites the wet-forest finish from
create_chult_wet_materials.py), so it is not used for this one mesh. Same FBX options as there.
"""
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'SourceAssets/Cutscenes/FallToChult'
DEST = '/Game/Art/Cutscenes/FallToChult'
NAME = 'SM_FallLog'

mats = {k: unreal.load_asset(f'{DEST}/{k}') for k in ('M_FallLeaves', 'M_FallCanopy', 'M_FallBark', 'M_FallCrownLeaves')}
if not all(mats.values()):
    raise RuntimeError('Missing a FallToChult material: ' + ', '.join(k for k, v in mats.items() if not v))

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

task = unreal.AssetImportTask()
task.filename = str(SOURCE / f'{NAME}.fbx'); task.destination_path = DEST; task.destination_name = NAME
task.automated = True; task.replace_existing = True; task.save = False; task.options = options
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
mesh = unreal.load_asset(f'{DEST}/{NAME}')
if not isinstance(mesh, unreal.StaticMesh):
    raise RuntimeError(f'Missing mesh {NAME}')
slots = mesh.get_editor_property('static_materials')
for i, slot in enumerate(slots):
    slot_name = str(slot.get_editor_property('material_slot_name'))
    key = next((k for k in sorted(mats, key=len, reverse=True) if slot_name.startswith(k)), None)
    if key is None:
        raise RuntimeError(f'{NAME}: unexpected slot {slot_name}')
    mesh.set_material(i, mats[key])
unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False)
box = mesh.get_bounding_box()
unreal.log(f'CHUCK_FALL_LOG_IMPORT slots={len(slots)} triangles={mesh.get_num_triangles(0)} min_z={box.min.z:.1f} max_z={box.max.z:.1f}')
