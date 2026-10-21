"""Import the temple snake (Tools/build_temple_snake.py) into /Game/Characters/Snake (Claude, 2026-10-21).

UnrealEditor-Cmd <uproject> -ExecutePythonScript=Tools/import_temple_snake.py -unattended -nullrhi -nosplash -NoLiveCoding

SK_TempleSnake with its own small skeleton and no clips (ATempleSnake poses it
along the path its head has taken), LODs, the generated textures and five
materials on its slots:
- M_SnakeSkin: the body pattern on the body's UVs with the keeled scales' relief
  tiled over it (12 cm a tile along the body, once round it), their crevices
  darker; satiny, as a viper's scales are.
- M_SnakeMouth (wet pink), M_SnakeEye (amber, slit pupil, glossy), M_SnakeFang,
  M_SnakeTongue (blue-black).
Existing materials are kept (delete to rebuild a graph).
"""
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'SourceAssets/Enemies/Snake'
DEST = '/Game/Characters/Snake'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
LIB = unreal.MaterialEditingLibrary
ASSETS = unreal.EditorAssetLibrary


def node(mat, cls, x=0, y=0, **props):
    n = LIB.create_material_expression(mat, cls, x, y)
    for k, v in props.items(): n.set_editor_property(k, v)
    return n


def link(a, b, pin_b, pin_a=''):
    if not LIB.connect_material_expressions(a, pin_a, b, pin_b): raise RuntimeError(f'link {pin_a}->{pin_b}')


def out(e, prop, pin=''):
    if not LIB.connect_material_property(e, pin, prop): raise RuntimeError(f'output {prop}')


def texture(name, normal=False, srgb=True):
    t = unreal.AssetImportTask()
    t.filename = str(SOURCE / f'{name}.png'); t.destination_path = DEST; t.destination_name = name
    t.automated = True; t.replace_existing = True; t.save = False
    TOOLS.import_asset_tasks([t])
    tex = unreal.load_asset(f'{DEST}/{name}')
    if not isinstance(tex, unreal.Texture2D): raise RuntimeError(f'Texture import failed: {name}')
    if normal: tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
    tex.set_editor_property('srgb', srgb and not normal)
    ASSETS.save_loaded_asset(tex, only_if_is_dirty=False)
    return tex


pattern = texture('T_SnakePattern')
scales_n = texture('T_SnakeScales_N', normal=True)
scales_c = texture('T_SnakeScales_C', srgb=False)
eye = texture('T_SnakeEye')


def material(name, build):
    mat = unreal.load_asset(f'{DEST}/{name}')
    fresh = mat is None
    if fresh: mat = TOOLS.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('used_with_skeletal_mesh', True)
    if fresh: build(mat)
    LIB.recompile_material(mat)
    ASSETS.save_loaded_asset(mat, only_if_is_dirty=False)
    return mat


def skin(mat):
    uv = node(mat, unreal.MaterialExpressionTextureCoordinate, -1400, 0)
    tiled = node(mat, unreal.MaterialExpressionTextureCoordinate, -1400, 300, u_tiling=200. / 12., v_tiling=1.)
    pat = node(mat, unreal.MaterialExpressionTextureSample, -1100, -100, texture=pattern); link(uv, pat, 'UVs')
    nrm = node(mat, unreal.MaterialExpressionTextureSample, -1100, 300, texture=scales_n, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL); link(tiled, nrm, 'UVs')
    cav = node(mat, unreal.MaterialExpressionTextureSample, -1100, 600, texture=scales_c, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR); link(tiled, cav, 'UVs')
    dark = node(mat, unreal.MaterialExpressionLinearInterpolate, -700, 200)
    link(node(mat, unreal.MaterialExpressionConstant, -900, 150, r=1.), dark, 'A'); link(node(mat, unreal.MaterialExpressionConstant, -900, 220, r=.45), dark, 'B'); link(cav, dark, 'Alpha', 'R')
    col = node(mat, unreal.MaterialExpressionMultiply, -500, 0); link(pat, col, 'A'); link(dark, col, 'B')
    out(col, unreal.MaterialProperty.MP_BASE_COLOR)
    out(nrm, unreal.MaterialProperty.MP_NORMAL)
    rough = node(mat, unreal.MaterialExpressionLinearInterpolate, -500, 400)
    link(node(mat, unreal.MaterialExpressionConstant, -700, 380, r=.42), rough, 'A'); link(node(mat, unreal.MaterialExpressionConstant, -700, 450, r=.8), rough, 'B'); link(cav, rough, 'Alpha', 'R')
    out(rough, unreal.MaterialProperty.MP_ROUGHNESS)
    out(node(mat, unreal.MaterialExpressionConstant, -300, 500, r=.5), unreal.MaterialProperty.MP_SPECULAR)
    out(dark, unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)


def flat(colour, rough, spec, tex=None):
    def build(mat):
        if tex:
            s = node(mat, unreal.MaterialExpressionTextureSample, -500, 0, texture=tex)
            out(s, unreal.MaterialProperty.MP_BASE_COLOR)
        else:
            out(node(mat, unreal.MaterialExpressionConstant3Vector, -500, 0, constant=unreal.LinearColor(*colour, 1)), unreal.MaterialProperty.MP_BASE_COLOR)
        out(node(mat, unreal.MaterialExpressionConstant, -500, 200, r=rough), unreal.MaterialProperty.MP_ROUGHNESS)
        out(node(mat, unreal.MaterialExpressionConstant, -500, 300, r=spec), unreal.MaterialProperty.MP_SPECULAR)
    return build


materials = {
    'Skin': material('M_SnakeSkin', skin),
    'Mouth': material('M_SnakeMouth', flat((.42, .13, .13), .3, .55)),
    'Eye': material('M_SnakeEye', flat(None, .08, .8, eye)),
    'Fang': material('M_SnakeFang', flat((.72, .68, .56), .35, .5)),
    'Tongue': material('M_SnakeTongue', flat((.015, .015, .02), .3, .5)),
}

options = unreal.FbxImportUI()
options.import_mesh = True
options.import_as_skeletal = True
options.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
options.automated_import_should_detect_type = False
options.import_materials = options.import_textures = options.import_animations = False
options.create_physics_asset = False
options.skeletal_mesh_import_data.set_editor_property('update_skeleton_reference_pose', True)
task = unreal.AssetImportTask()
task.filename = str(SOURCE / 'SK_TempleSnake.fbx'); task.destination_path = DEST; task.destination_name = 'SK_TempleSnake'
task.automated = True; task.replace_existing = True; task.save = True; task.options = options
TOOLS.import_asset_tasks([task])
mesh = unreal.load_asset(f'{DEST}/SK_TempleSnake')
if not isinstance(mesh, unreal.SkeletalMesh): raise RuntimeError('Snake mesh is not skeletal')
mats = mesh.get_editor_property('materials')
for index, m in enumerate(mats):
    slot = str(m.get_editor_property('material_slot_name')).split('.')[0]
    if slot not in materials: raise RuntimeError(f'Unexpected snake slot {slot}')
    m.material_interface = materials[slot]
    mats[index] = m
mesh.set_editor_property('materials', mats)
sub = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
lod_ok = sub.regenerate_lod(mesh, 3, True, False)
ASSETS.save_loaded_asset(mesh, only_if_is_dirty=False)
ASSETS.save_loaded_asset(mesh.get_editor_property('skeleton'), only_if_is_dirty=False)
box = mesh.get_bounds().box_extent
lods = [sub.get_num_verts(mesh, i) for i in range(sub.get_lod_count(mesh))]
unreal.log(f'CHUCK_SNAKE_IMPORTED slots={[str(m.get_editor_property("material_slot_name")) for m in mesh.get_editor_property("materials")]} '
           f'extent=({box.x:.1f},{box.y:.1f},{box.z:.1f}) lod_verts={lods} lod_ok={lod_ok}')
