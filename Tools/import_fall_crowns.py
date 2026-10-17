"""The jungle trees' new crowns (Claude, 2026-10-17).

blender --background --python Tools/build_crown_leaves.py
blender --background --python Tools/build_fall_jungle.py -- --only SM_FallTree_A,SM_FallTree_B,SM_FallCrown
UnrealEditor-Cmd <uproject> -ExecutePythonScript=Tools/import_fall_crowns.py -unattended -nosplash -NoLiveCoding

Imports T_FallCrownLeaves (+_N), builds M_FallCrownLeaves, rebuilds the matte
M_FallCanopy core and (through create_chult_wet_materials.py, that material
only) the less glossy M_FallLeaves, then reimports SM_FallTree_A/B and
SM_FallCrown with their slots assigned by name. No other mesh, texture or
material is touched.
"""
from pathlib import Path
import unreal

source = Path(__file__).with_name('import_fall_cutscene.py')
scope = {'__file__': str(source), '__name__': 'fall_crown_helpers'}
helpers = source.read_text(encoding='utf-8')
if '# Music:' not in helpers: raise RuntimeError('Import helper boundary changed')
exec(compile(helpers.split('# Music:')[0], str(source), 'exec'), scope)
DEST, SOURCE, TOOLS, ASSETS = scope['DEST'], scope['SOURCE'], scope['TOOLS'], scope['ASSETS']
material, texture = scope['material'], scope['texture']

material('M_FallCrownLeaves', scope['crown_leaves'](texture('T_FallCrownLeaves'), texture('T_FallCrownLeaves_N', True)))
material('M_FallCanopy', scope['canopy'])
wet = Path(__file__).with_name('create_chult_wet_materials.py')
exec(compile(wet.read_text(encoding='utf-8'), str(wet), 'exec'),
     {'__file__': str(wet), '__name__': '__main__', 'CHULT_MATERIAL_NAMES': {'M_FallLeaves'}})

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
for name in ('SM_FallTree_A', 'SM_FallTree_B', 'SM_FallCrown'):
    task = unreal.AssetImportTask()
    task.filename = str(SOURCE / f'{name}.fbx'); task.destination_path = DEST; task.destination_name = name
    task.automated = True; task.replace_existing = True; task.save = False; task.options = options
    TOOLS.import_asset_tasks([task])
    mesh = unreal.load_asset(f'{DEST}/{name}')
    if not isinstance(mesh, unreal.StaticMesh): raise RuntimeError(f'Missing mesh {name}')
    slots = mesh.get_editor_property('static_materials')
    for i, slot in enumerate(slots):
        slot_name = str(slot.get_editor_property('material_slot_name'))
        # Longest name first, so a prefix never takes another slot.
        key = next((k for k in sorted(mats, key=len, reverse=True) if slot_name.startswith(k)), None)
        if key is None: raise RuntimeError(f'{name}: unexpected slot {slot_name}')
        mesh.set_material(i, mats[key])
    ASSETS.save_loaded_asset(mesh, only_if_is_dirty=False)
    box = mesh.get_bounding_box()
    unreal.log(f'CHUCK_FALL_CROWN_IMPORT {name} slots={len(slots)} height_cm={box.max.z - box.min.z:.0f}')
unreal.log('CHUCK_FALL_CROWNS_IMPORT_READY')
