"""Full-editor Python import using the project's stable flat-color materials."""
from pathlib import Path
import unreal
ROOT=Path(__file__).resolve().parents[1]
DEST='/Game/Characters/Chuck'
tools=unreal.AssetToolsHelpers.get_asset_tools()
materials={
    'Fur':unreal.load_asset('/Game/Prototype/Materials/M_Gray.M_Gray'),
    'Chest':unreal.load_asset('/Game/Prototype/Materials/M_Gray.M_Gray'),
    'Jacket':unreal.load_asset('/Game/Prototype/Materials/M_Purple.M_Purple'),
    'Seam':unreal.load_asset('/Game/Prototype/Materials/M_Dark.M_Dark'),
    'Skin':unreal.load_asset('/Game/Prototype/Materials/M_Pink.M_Pink'),
    'Eye':unreal.load_asset('/Game/Prototype/Materials/M_Dark.M_Dark'),
    'Claw':unreal.load_asset('/Game/Prototype/Materials/M_Pink.M_Pink'),
    'Metal':unreal.load_asset('/Game/Prototype/Materials/M_Dark.M_Dark'),
    'Whisker':unreal.load_asset('/Game/Prototype/Materials/M_Gray.M_Gray')}
if not all(materials.values()):
    raise RuntimeError('Prototype materials are missing; run the normal asset generator first.')
for name in ('SM_ChuckBody','SM_ChuckFoot'):
    task=unreal.AssetImportTask()
    task.filename=str(ROOT/'SourceAssets'/'Chuck'/f'{name}.fbx')
    task.destination_path=DEST
    task.destination_name=name
    task.automated=True
    task.replace_existing=True
    task.save=True
    options=unreal.FbxImportUI()
    options.import_mesh=True
    options.import_as_skeletal=False
    options.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
    options.automated_import_should_detect_type=False
    options.import_materials=False
    options.import_textures=False
    options.static_mesh_import_data.combine_meshes=True
    options.static_mesh_import_data.auto_generate_collision=False
    options.static_mesh_import_data.generate_lightmap_u_vs=False
    options.static_mesh_import_data.vertex_color_import_option=unreal.VertexColorImportOption.REPLACE
    options.static_mesh_import_data.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS
    task.options=options
    tools.import_asset_tasks([task])
    asset=unreal.load_asset(f'{DEST}/{name}')
    if not isinstance(asset,unreal.StaticMesh):
        raise RuntimeError(f'Expected static mesh: {name}; imported {task.imported_object_paths}')
    for i,slot in enumerate(asset.static_materials):
        label=str(slot.material_slot_name)
        key=label.split('.')[0]
        if key not in materials:
            raise RuntimeError(f'Unknown material slot {label}')
        asset.set_material(i,materials[key])
    unreal.EditorAssetLibrary.save_loaded_asset(asset)
    bounds=asset.get_bounds()
    unreal.log(f'CHUCK_IMPORT {name}: origin={bounds.origin} extent={bounds.box_extent}')
    if name=='SM_ChuckBody':
        top=bounds.origin.z+bounds.box_extent.z
        if abs(top-65)>1:
            raise RuntimeError(f'Incorrect import scale: expected top at 65 cm, got {top}')
unreal.log('CHUCK_CUSTOM_ASSETS_READY')
