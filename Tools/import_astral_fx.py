"""Import the astral summoning effect (Tools/build_astral_fx.py) into /Game/Art/Props/Astral.

UnrealEditor-Cmd <uproject> -ExecutePythonScript=Tools/import_astral_fx.py -unattended -nosplash -NoLiveCoding

Three additive, unlit starlight materials, each with an "Intensity" scalar
(default 0) that AAstralSummon fades through dynamic instances:
  M_AstralCircle  the rune texture, pale starlight, turning very slowly
  M_AstralColumn  a soft column, brightest at the foot, gone by the top
  M_AstralMote    points of starlight (instanced)
Existing materials are kept (delete to rebuild).
"""
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'SourceAssets/Props/Astral'
DEST = '/Game/Art/Props/Astral'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
LIB = unreal.MaterialEditingLibrary
ASSETS = unreal.EditorAssetLibrary
STARLIGHT = (.55, .62, 1.)


def node(mat, cls, **props):
    n = LIB.create_material_expression(mat, cls)
    for k, v in props.items(): n.set_editor_property(k, v)
    return n


def link(a, b, pin_b, pin_a=''):
    if not LIB.connect_material_expressions(a, pin_a, b, pin_b):
        raise RuntimeError(f'Material link failed: {pin_a} -> {pin_b}')


def import_file(name, options=None):
    task = unreal.AssetImportTask()
    task.filename = str(SOURCE / name); task.destination_path = DEST; task.destination_name = Path(name).stem
    task.automated = True; task.replace_existing = True; task.save = True
    if options: task.options = options
    TOOLS.import_asset_tasks([task])
    asset = unreal.load_asset(f'{DEST}/{Path(name).stem}')
    if not asset: raise RuntimeError(f'Missing {name}')
    return asset


def material(name, build):
    mat = unreal.load_asset(f'{DEST}/{name}')
    if mat:
        return mat
    mat = TOOLS.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    try:
        mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ADDITIVE)
        mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
        mat.set_editor_property('two_sided', True)
        intensity = node(mat, unreal.MaterialExpressionScalarParameter, parameter_name='Intensity', default_value=0.)
        colour = node(mat, unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(*STARLIGHT, 1))
        glow = node(mat, unreal.MaterialExpressionMultiply)
        link(colour, glow, 'A'); link(intensity, glow, 'B')
        out = build(mat, glow)
        LIB.connect_material_property(out, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    except Exception:
        ASSETS.delete_loaded_asset(mat)
        raise
    LIB.recompile_material(mat)
    ASSETS.save_loaded_asset(mat, only_if_is_dirty=False)
    return mat


static = unreal.FbxImportUI()
static.import_mesh = True; static.import_as_skeletal = False
static.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
static.automated_import_should_detect_type = False
static.import_materials = static.import_textures = static.import_animations = False
static.static_mesh_import_data.set_editor_property('combine_meshes', True)
static.static_mesh_import_data.set_editor_property('auto_generate_collision', False)
meshes = {n: import_file(f'{n}.fbx', static) for n in ('SM_AstralCircle', 'SM_AstralColumn', 'SM_AstralMote')}
texture = import_file('T_AstralCircle.png')
texture.set_editor_property('srgb', False)
texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_GRAYSCALE)
ASSETS.save_loaded_asset(texture, only_if_is_dirty=False)


def circle(mat, glow):
    uv = node(mat, unreal.MaterialExpressionTextureCoordinate)
    turn = node(mat, unreal.MaterialExpressionRotator, speed=.02, center_x=.5, center_y=.5)
    link(uv, turn, 'Coordinate')
    tex = node(mat, unreal.MaterialExpressionTextureSample, texture=texture, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
    link(turn, tex, 'UVs')
    out = node(mat, unreal.MaterialExpressionMultiply)
    link(glow, out, 'A'); link(tex, out, 'B', 'R')
    return out


def column(mat, glow):
    uv = node(mat, unreal.MaterialExpressionTextureCoordinate)
    v = node(mat, unreal.MaterialExpressionComponentMask, r=False, g=True, b=False, a=False)
    link(uv, v, '')
    # The FBX import flips V (v = 1 at the foot in Unreal): brightest there,
    # gone by the top.
    soft = node(mat, unreal.MaterialExpressionPower, const_exponent=2.)
    link(v, soft, 'Base')
    # Fade toward the silhouette: a haze of light, not a glowing tube.
    edge = node(mat, unreal.MaterialExpressionFresnel, exponent=1.2, base_reflect_fraction=0.)
    core = node(mat, unreal.MaterialExpressionOneMinus); link(edge, core, '')
    haze = node(mat, unreal.MaterialExpressionPower, const_exponent=2.5)
    link(core, haze, 'Base')
    shape = node(mat, unreal.MaterialExpressionMultiply)
    link(soft, shape, 'A'); link(haze, shape, 'B')
    out = node(mat, unreal.MaterialExpressionMultiply)
    link(glow, out, 'A'); link(shape, out, 'B')
    return out


mats = {
    'SM_AstralCircle': material('M_AstralCircle', circle),
    'SM_AstralColumn': material('M_AstralColumn', column),
    'SM_AstralMote': material('M_AstralMote', lambda mat, glow: glow),
}
mote = mats['SM_AstralMote']
if not mote.get_editor_property('used_with_instanced_static_meshes'):
    mote.set_editor_property('used_with_instanced_static_meshes', True)
    LIB.recompile_material(mote); ASSETS.save_loaded_asset(mote, only_if_is_dirty=False)
for name, mesh in meshes.items():
    for i in range(len(mesh.get_editor_property('static_materials'))):
        mesh.set_material(i, mats[name])
    ASSETS.save_loaded_asset(mesh, only_if_is_dirty=False)
print('CHUCK_ASTRAL_IMPORTED', sorted(meshes), texture.get_name())
