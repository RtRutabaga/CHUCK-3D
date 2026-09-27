"""Import Chuck's cigarette prop and smoke wisp (SourceAssets/Chuck/V1/Cigarette).

Run by Tools/Import-ChuckV1.ps1 after import_chuck_v1.py; never runs Blender.
Materials are created once. An existing material is kept as it is, because
deleting a loaded material's expressions from Python asserts (!IsRooted) in
UE 5.7.4; delete the asset to rebuild its graph.
"""
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'SourceAssets/Chuck/V1/Cigarette'
DEST = '/Game/Characters/Chuck/V1/Cigarette'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
LIB = unreal.MaterialEditingLibrary
ASSETS = unreal.EditorAssetLibrary


def import_file(file, name, options=None):
    task = unreal.AssetImportTask()
    task.filename = str(file); task.destination_path = DEST; task.destination_name = name
    task.automated = True; task.replace_existing = True; task.save = True
    if options: task.options = options
    TOOLS.import_asset_tasks([task])
    asset = unreal.load_asset(f'{DEST}/{name}')
    if not asset:
        raise RuntimeError(f'Missing imported {name}')
    return asset


def node(mat, cls, **props):
    n = LIB.create_material_expression(mat, cls)
    for k, v in props.items(): n.set_editor_property(k, v)
    return n


def link(a, b, pin_b, pin_a=''):
    if not LIB.connect_material_expressions(a, pin_a, b, pin_b):
        raise RuntimeError(f'Material link failed: {pin_a} -> {pin_b}')


def material(name, build):
    path = f'{DEST}/{name}'
    mat = unreal.load_asset(path)
    if mat:
        unreal.log(f'CHUCK_CIGARETTE_MATERIAL_KEPT {name}')
        return mat
    mat = TOOLS.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    try:
        build(mat)
    except Exception:
        # Never leave a half-built graph behind for the keep-existing rule.
        ASSETS.delete_loaded_asset(mat)
        raise
    LIB.recompile_material(mat)
    ASSETS.save_loaded_asset(mat, only_if_is_dirty=False)
    return mat


def flat(colour, rough):
    def build(mat):
        c = node(mat, unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(*colour, 1))
        LIB.connect_material_property(c, '', unreal.MaterialProperty.MP_BASE_COLOR)
        r = node(mat, unreal.MaterialExpressionConstant, r=rough)
        LIB.connect_material_property(r, '', unreal.MaterialProperty.MP_ROUGHNESS)
    return build


def ember(mat):
    # Dim coal with a slow, restrained glow pulse (no flicker).
    base = node(mat, unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(.25, .05, .01, 1))
    LIB.connect_material_property(base, '', unreal.MaterialProperty.MP_BASE_COLOR)
    glow = node(mat, unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(6., 1.4, .25, 1))
    time = node(mat, unreal.MaterialExpressionTime)
    sine = node(mat, unreal.MaterialExpressionSine, period=3.)
    link(time, sine, '')
    swing = node(mat, unreal.MaterialExpressionMultiply, const_b=.25)
    link(sine, swing, 'A')
    level = node(mat, unreal.MaterialExpressionAdd, const_b=.75)
    link(swing, level, 'A')
    out = node(mat, unreal.MaterialExpressionMultiply)
    link(glow, out, 'A'); link(level, out, 'B')
    LIB.connect_material_property(out, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)


def smoke(mat, texture):
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('two_sided', True)
    colour = node(mat, unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(.7, .7, .72, 1))
    LIB.connect_material_property(colour, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    uv = node(mat, unreal.MaterialExpressionTextureCoordinate)
    pan = node(mat, unreal.MaterialExpressionPanner, speed_x=0., speed_y=-.18)
    link(uv, pan, 'Coordinate')
    tex = node(mat, unreal.MaterialExpressionTextureSample, texture=texture,
               sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
    link(pan, tex, 'UVs')
    fades = []
    for channel in ('r', 'g'):
        mask = node(mat, unreal.MaterialExpressionComponentMask, r=channel == 'r', g=channel == 'g', b=False, a=False)
        link(uv, mask, '')
        inv = node(mat, unreal.MaterialExpressionOneMinus)
        link(mask, inv, '')
        bell = node(mat, unreal.MaterialExpressionMultiply)
        link(mask, bell, 'A'); link(inv, bell, 'B')
        fades.append(bell)
    edge = node(mat, unreal.MaterialExpressionMultiply)
    link(fades[0], edge, 'A'); link(fades[1], edge, 'B')
    wisp = node(mat, unreal.MaterialExpressionMultiply)
    link(tex, wisp, 'A', 'R'); link(edge, wisp, 'B')
    # u(1-u) * v(1-v) peaks at 1/16: scale to a faint, thin wisp.
    opacity = node(mat, unreal.MaterialExpressionMultiply, const_b=3.2)
    link(wisp, opacity, 'A')
    LIB.connect_material_property(opacity, '', unreal.MaterialProperty.MP_OPACITY)


static = unreal.FbxImportUI()
static.import_mesh = True; static.import_as_skeletal = False
static.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
static.automated_import_should_detect_type = False
static.import_materials = static.import_textures = static.import_animations = False
static.static_mesh_import_data.set_editor_property('combine_meshes', True)
static.static_mesh_import_data.set_editor_property('auto_generate_collision', False)
cigarette = import_file(SOURCE / 'SM_Cigarette.fbx', 'SM_Cigarette', static)
wisp_mesh = import_file(SOURCE / 'SM_CigaretteSmoke.fbx', 'SM_CigaretteSmoke', static)
texture = import_file(SOURCE / 'T_CigaretteSmoke.png', 'T_CigaretteSmoke')
texture.set_editor_property('srgb', False)
texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_GRAYSCALE)
ASSETS.save_loaded_asset(texture, only_if_is_dirty=False)

mats = {
    'Paper': material('M_Cigarette_Paper', flat((.9, .88, .84), .65)),
    'Filter': material('M_Cigarette_Filter', flat((.6, .44, .26), .6)),
    'Ash': material('M_Cigarette_Ash', flat((.28, .27, .26), .9)),
    'Ember': material('M_Cigarette_Ember', ember),
    'Smoke': material('M_CigaretteSmoke', lambda m: smoke(m, texture)),
}
for mesh in (cigarette, wisp_mesh):
    slots = mesh.get_editor_property('static_materials')
    for i, slot in enumerate(slots):
        name = str(slot.get_editor_property('material_slot_name')).split('.')[0]
        if name not in mats:
            raise RuntimeError(f'Unexpected cigarette slot {name}')
        mesh.set_material(i, mats[name])
    ASSETS.save_loaded_asset(mesh, only_if_is_dirty=False)
names = [str(s.get_editor_property('material_slot_name')) for s in cigarette.get_editor_property('static_materials')]
bounds = cigarette.get_bounding_box()
unreal.log(f'CHUCK_CIGARETTE_IMPORTED slots={names} length_cm={bounds.max.x - bounds.min.x:.3f}')
