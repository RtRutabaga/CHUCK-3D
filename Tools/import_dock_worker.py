"""Import original static dock props with authored materials and no new collision."""
from pathlib import Path
import unreal
ROOT=Path(__file__).resolve().parents[1]
DEST='/Game/Art/Props'
for name,expected in [('SM_DockWorker',None)]:
    task=unreal.AssetImportTask()
    task.filename=str(ROOT/'SourceAssets'/'Docks'/f'{name}.fbx')
    task.destination_path=DEST; task.destination_name=name
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
    asset=unreal.load_asset(f'{DEST}/{name}')
    if not isinstance(asset,unreal.StaticMesh): raise RuntimeError(f'Missing mesh: {name}')
    for i,slot in enumerate(asset.static_materials):
        label=str(slot.material_slot_name).split('.')[0]
        material=unreal.load_asset(f'/Game/Art/Materials/M_{label}')
        if not material: raise RuntimeError(f'Missing art material: {label}')
        asset.set_material(i,material)
    bounds=asset.get_bounds(); extent=bounds.box_extent
    actual=(extent.x*2,extent.y*2,extent.z*2)
    if abs(actual[2]-180)>.1 or not (60 < actual[1] < 75):
        raise RuntimeError(f'Incorrect centimetre bounds: {name}: {actual} expected {expected}')
    unreal.EditorAssetLibrary.save_loaded_asset(asset,only_if_is_dirty=False)
    unreal.log(f'CHUCK_WORKER_PROP {name} dimensions={actual}')
unreal.log('CHUCK_WORKER_IMPORT_READY')
