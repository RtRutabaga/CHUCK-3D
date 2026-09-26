"""Full-editor skeletal import; shares the form study's stable material mapping."""
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1]
DEST = '/Game/Characters/Chuck'
task = unreal.AssetImportTask()
task.filename = str(ROOT/'SourceAssets/Chuck/SK_ChuckBody.fbx')
task.destination_path = DEST
task.destination_name = 'SK_ChuckBody'
task.automated = True
task.replace_existing = True
task.save = True
options = unreal.FbxImportUI()
options.import_mesh = True
options.import_as_skeletal = True
options.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
options.automated_import_should_detect_type = False
options.import_materials = False
options.import_textures = False
options.import_animations = False
options.create_physics_asset = False
options.skeletal_mesh_import_data.normal_import_method = unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS
task.options = options
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
asset = unreal.load_asset(f'{DEST}/SK_ChuckBody')
if not isinstance(asset,unreal.SkeletalMesh):
    raise RuntimeError(f'Expected skeletal mesh; imported {task.imported_object_paths}')
mapping = {'Fur':'Gray','Chest':'Gray','Whisker':'Gray','Jacket':'Purple',
           'Seam':'Dark','Eye':'Dark','Metal':'Dark','Skin':'Pink','Claw':'Pink'}
slots = asset.get_editor_property('materials')
for index, slot in enumerate(slots):
    name = str(slot.material_slot_name).split('.')[0]
    if name not in mapping: raise RuntimeError(f'Unknown material slot {name}')
    color = mapping[name]
    material = unreal.load_asset(f'/Game/Prototype/Materials/M_{color}')
    if not material: raise RuntimeError(f'Missing material {color}')
    # Static form-study materials must also compile their skeletal permutation.
    unreal.MaterialEditingLibrary.set_material_usage(material,unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH)
    unreal.EditorAssetLibrary.save_loaded_asset(material,only_if_is_dirty=False)
    slot.material_interface = material
    slots[index] = slot
asset.set_editor_property('materials',slots)
unreal.EditorAssetLibrary.save_loaded_asset(asset,only_if_is_dirty=False)
for slot in asset.get_editor_property('materials'):
    if not slot.material_interface or not slot.material_interface.get_path_name().startswith('/Game/Prototype/Materials/'):
        raise RuntimeError(f'Material assignment did not persist: {slot.material_slot_name}')
unreal.log(f'CHUCK_RIG_READY {task.imported_object_paths}')
