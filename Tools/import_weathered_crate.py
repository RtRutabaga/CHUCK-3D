"""Reimport only the corrected derivative crate; do not rebuild wood/NPC materials."""
from pathlib import Path
import unreal

ROOT=Path(__file__).resolve().parents[1]
task=unreal.AssetImportTask()
task.filename=str(ROOT/'SourceAssets/Surfaces/WeatheredTimber/SM_WeatheredDockCrate.fbx')
task.destination_path='/Game/Art/Props'; task.destination_name='SM_WeatheredDockCrate'
task.automated=True; task.replace_existing=True; task.save=True
options=unreal.FbxImportUI()
options.import_mesh=True; options.import_as_skeletal=False
options.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
options.automated_import_should_detect_type=False
options.import_materials=False; options.import_textures=False
options.static_mesh_import_data.combine_meshes=True
options.static_mesh_import_data.auto_generate_collision=False
options.static_mesh_import_data.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS
task.options=options
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
mesh=unreal.load_asset('/Game/Art/Props/SM_WeatheredDockCrate')
for i,slot in enumerate(mesh.static_materials):
    material=unreal.load_asset('/Game/Art/Materials/M_'+str(slot.material_slot_name).split('.')[0])
    if not material: raise RuntimeError('Missing crate material')
    mesh.set_material(i,material)
old=unreal.load_asset('/Game/Art/Props/SM_DockCrate')
a=mesh.get_bounds(); b=old.get_bounds()
if (a.box_extent-b.box_extent).length()>.01 or (a.origin-b.origin).length()>.01:
    raise RuntimeError('Crate outer bounds changed')
unreal.EditorAssetLibrary.save_loaded_asset(mesh,only_if_is_dirty=False)
unreal.log('CHUCK_CRATE_RECESS_IMPORTED bounds_match=1 materials_unchanged=1')
