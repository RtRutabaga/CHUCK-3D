"""Import Chult's macaws (Tools/build_chult_birds.py) into /Game/Art/ChultBirds (Claude, 2026-10-19).

UnrealEditor-Cmd <uproject> -ExecutePythonScript=Tools/import_chult_birds.py -unattended -nosplash -NoLiveCoding

The plumage is vertex colour: M_ChultBird takes it as base colour (a little
sheen on the feathers). Nothing collides; ChultBirds.cpp flies them.
"""
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'SourceAssets/Props/ChultBirds'
DEST = '/Game/Art/ChultBirds'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
LIB = unreal.MaterialEditingLibrary
ASSETS = unreal.EditorAssetLibrary


def feather_material():
    path = f'{DEST}/M_ChultBird'
    if ASSETS.does_asset_exist(path):
        # Rebuilt in place: the meshes keep their reference to it.
        mat = unreal.load_asset(path); LIB.delete_all_material_expressions(mat)
    else:
        mat = TOOLS.create_asset('M_ChultBird', DEST, unreal.Material, unreal.MaterialFactoryNew())
    vc = LIB.create_material_expression(mat, unreal.MaterialExpressionVertexColor)
    LIB.connect_material_property(vc, '', unreal.MaterialProperty.MP_BASE_COLOR)   # its unnamed RGB output
    r = LIB.create_material_expression(mat, unreal.MaterialExpressionConstant); r.set_editor_property('r', .48)
    LIB.connect_material_property(r, '', unreal.MaterialProperty.MP_ROUGHNESS)
    sp = LIB.create_material_expression(mat, unreal.MaterialExpressionConstant); sp.set_editor_property('r', .45)
    LIB.connect_material_property(sp, '', unreal.MaterialProperty.MP_SPECULAR)
    LIB.recompile_material(mat); ASSETS.save_loaded_asset(mat, only_if_is_dirty=False)
    return mat


feather = feather_material()
options = unreal.FbxImportUI()
options.import_mesh = True; options.import_as_skeletal = False
options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
options.automated_import_should_detect_type = False
options.import_materials = options.import_textures = options.import_animations = False
data = options.static_mesh_import_data
data.set_editor_property('combine_meshes', True)
data.set_editor_property('auto_generate_collision', False)
data.set_editor_property('generate_lightmap_u_vs', False)
data.set_editor_property('vertex_color_import_option', unreal.VertexColorImportOption.REPLACE)
made = []
for kind in ('Scarlet', 'BlueGold'):
    for part in ('Body', 'WingInL', 'WingInR', 'WingOutL', 'WingOutR'):
        name = f'SM_{kind}_{part}'
        task = unreal.AssetImportTask(); task.filename = str(SOURCE / f'{name}.fbx'); task.destination_path = DEST
        task.destination_name = name; task.automated = True; task.replace_existing = True; task.save = True; task.options = options
        TOOLS.import_asset_tasks([task])
        mesh = unreal.load_asset(f'{DEST}/{name}')
        if not isinstance(mesh, unreal.StaticMesh): raise RuntimeError(f'Missing bird mesh {name}')
        for i in range(len(mesh.get_editor_property('static_materials'))): mesh.set_material(i, feather)
        ASSETS.save_loaded_asset(mesh, only_if_is_dirty=False)
        box = mesh.get_bounding_box(); made.append(f'{name}:{box.min.x:.0f},{box.min.y:.0f},{box.min.z:.0f}..{box.max.x:.0f},{box.max.y:.0f},{box.max.z:.0f}')
print('CHUCK_BIRDS_IMPORTED ' + ' '.join(made))
