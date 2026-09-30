"""Import the shreddable grass tufts (Tools/build_grass_tuft.py) into /Game/Art/Props/Grass.

UnrealEditor-Cmd <uproject> -run=pythonscript -script=Tools/import_grass_tuft.py

M_Grass: vertex-colour tint, rough and matte, and a gentle wind sway in world
position offset that grows with the blade height (vertex alpha = height
fraction), so roots stay planted and tips move a centimetre or so. As with the
cigarette materials, an existing material is kept (delete it to rebuild).
No collision: Chuck walks through grass, and traces for his paws, ledges and
the camera must not hit it; the slash finds tufts through ChuckBreakable.
"""
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'SourceAssets/Props/Grass'
DEST = '/Game/Art/Props/Grass'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
LIB = unreal.MaterialEditingLibrary
ASSETS = unreal.EditorAssetLibrary


def node(mat, cls, **props):
    n = LIB.create_material_expression(mat, cls)
    for k, v in props.items(): n.set_editor_property(k, v)
    return n


def link(a, b, pin_b, pin_a=''):
    if not LIB.connect_material_expressions(a, pin_a, b, pin_b):
        raise RuntimeError(f'Material link failed: {pin_a} -> {pin_b}')


def grass_material():
    path = f'{DEST}/M_Grass'
    mat = unreal.load_asset(path)
    if mat:
        unreal.log('CHUCK_GRASS_MATERIAL_KEPT')
        return mat
    mat = TOOLS.create_asset('M_Grass', DEST, unreal.Material, unreal.MaterialFactoryNew())
    try:
        vc = node(mat, unreal.MaterialExpressionVertexColor)
        # The FBX colours arrive a gamma step bright (linear written, read as sRGB):
        # squaring brings the tint back to the authored muted olive and straw.
        tint = node(mat, unreal.MaterialExpressionMultiply)
        link(vc, tint, 'A'); link(vc, tint, 'B')
        LIB.connect_material_property(tint, '', unreal.MaterialProperty.MP_BASE_COLOR)
        rough = node(mat, unreal.MaterialExpressionConstant, r=.85)
        LIB.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
        spec = node(mat, unreal.MaterialExpressionConstant, r=.25)
        LIB.connect_material_property(spec, '', unreal.MaterialProperty.MP_SPECULAR)
        # Wind: phase (cycles) = time / 4 s + a slow drift across the docks.
        pos = node(mat, unreal.MaterialExpressionWorldPosition)
        px = node(mat, unreal.MaterialExpressionComponentMask, r=True, g=False, b=False, a=False)
        py = node(mat, unreal.MaterialExpressionComponentMask, r=False, g=True, b=False, a=False)
        link(pos, px, ''); link(pos, py, '')
        across = node(mat, unreal.MaterialExpressionAdd)
        link(px, across, 'A'); link(py, across, 'B')
        drift = node(mat, unreal.MaterialExpressionMultiply, const_b=.003)
        link(across, drift, 'A')
        time = node(mat, unreal.MaterialExpressionTime)
        slow = node(mat, unreal.MaterialExpressionMultiply, const_b=.25)
        link(time, slow, 'A')
        phase = node(mat, unreal.MaterialExpressionAdd)
        link(slow, phase, 'A'); link(drift, phase, 'B')
        sx = node(mat, unreal.MaterialExpressionSine, period=1.)
        link(phase, sx, '')
        sy = node(mat, unreal.MaterialExpressionCosine, period=1.3)
        link(phase, sy, '')
        ax = node(mat, unreal.MaterialExpressionMultiply, const_b=1.2)
        link(sx, ax, 'A')
        ay = node(mat, unreal.MaterialExpressionMultiply, const_b=.8)
        link(sy, ay, 'A')
        xy = node(mat, unreal.MaterialExpressionAppendVector)
        link(ax, xy, 'A'); link(ay, xy, 'B')
        zero = node(mat, unreal.MaterialExpressionConstant, r=0.)
        xyz = node(mat, unreal.MaterialExpressionAppendVector)
        link(xy, xyz, 'A'); link(zero, xyz, 'B')
        height = node(mat, unreal.MaterialExpressionMultiply)
        link(vc, height, 'A', 'A'); link(vc, height, 'B', 'A')       # alpha squared: tips move, roots don't
        sway = node(mat, unreal.MaterialExpressionMultiply)
        link(xyz, sway, 'A'); link(height, sway, 'B')
        LIB.connect_material_property(sway, '', unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    except Exception:
        ASSETS.delete_loaded_asset(mat)
        raise
    LIB.recompile_material(mat)
    ASSETS.save_loaded_asset(mat, only_if_is_dirty=False)
    return mat


options = unreal.FbxImportUI()
options.import_mesh = True; options.import_as_skeletal = False
options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
options.automated_import_should_detect_type = False
options.import_materials = options.import_textures = options.import_animations = False
data = options.static_mesh_import_data
data.set_editor_property('combine_meshes', True)
data.set_editor_property('auto_generate_collision', False)
data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
data.set_editor_property('vertex_color_import_option', unreal.VertexColorImportOption.REPLACE)
material = grass_material()
# The shred spray draws its clippings as instances: the material must allow it.
if not material.get_editor_property('used_with_instanced_static_meshes'):
    material.set_editor_property('used_with_instanced_static_meshes', True)
    LIB.recompile_material(material)
    ASSETS.save_loaded_asset(material, only_if_is_dirty=False)
    print('CHUCK_GRASS_MATERIAL_INSTANCING_ENABLED')
names = [f'SM_GrassTuft_{k}' for k in 'ABC'] + [f'SM_GrassStub_{k}' for k in 'ABC'] + ['SM_GrassClipping']
for name in names:
    task = unreal.AssetImportTask()
    task.filename = str(SOURCE / f'{name}.fbx'); task.destination_path = DEST; task.destination_name = name
    task.automated = True; task.replace_existing = True; task.save = True; task.options = options
    TOOLS.import_asset_tasks([task])
    mesh = unreal.load_asset(f'{DEST}/{name}')
    if not isinstance(mesh, unreal.StaticMesh):
        raise RuntimeError(f'Missing grass mesh {name}')
    for i in range(len(mesh.get_editor_property('static_materials'))):
        mesh.set_material(i, material)
    ASSETS.save_loaded_asset(mesh, only_if_is_dirty=False)
    box = mesh.get_bounding_box()
    print(f'CHUCK_GRASS_MESH {name} height_cm={box.max.z - box.min.z:.1f}')
print('CHUCK_GRASS_IMPORTED', len(names))
