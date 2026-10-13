"""Import the fall-to-Chult cutscene's music and jungle set (Claude, 2026-10-12).

UnrealEditor-Cmd <uproject> -ExecutePythonScript=Tools/import_fall_cutscene.py -unattended -nosplash -NoLiveCoding -AllowCommandletAudio

Sources: SourceAssets/Audio/FallToChult.wav (the user's "Fall Cutscene theme",
unmodified) and SourceAssets/Cutscenes/FallToChult (Tools/build_fall_jungle.py).
Destination /Game/Art/Cutscenes/FallToChult and /Game/Art/Audio/SW_FallToChult,
both under the /Game/Art cooking rule.

The materials here belong to this script alone, so each run rebuilds their
graphs in place (meshes keep their references):
  M_FallLeaves     masked two-sided foliage on the leaf atlas, vertex-colour
                   tint (r brightness, g deep/yellow green) and wind
                   in world position offset by vertex alpha
  M_FallCanopy     the opaque core of a crown
  M_FallBark       bark with moss by vertex green, on up-facing surfaces
  M_FallGround     world-aligned leaf litter for the landing floor
  M_FallJungleFar  the jungle seen from the sky: mottled canopy greens
  MI_FallClouds    the engine's simple volumetric cloud, as a project
                   instance so the cooker takes it with the game
"""
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'SourceAssets/Cutscenes/FallToChult'
DEST = '/Game/Art/Cutscenes/FallToChult'
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


def out(mat, expr, prop, pin=''):
    if not LIB.connect_material_property(expr, pin, prop):
        raise RuntimeError(f'Material output failed: {prop}')


def const(mat, v):
    return node(mat, unreal.MaterialExpressionConstant, r=v)


def colour(mat, r, g, b):
    return node(mat, unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(r, g, b, 1))


def op(mat, cls, a, b, pa='', pb=''):
    n = node(mat, cls)
    link(a, n, 'A', pa); link(b, n, 'B', pb)
    return n


def mul(mat, a, b, pa='', pb=''): return op(mat, unreal.MaterialExpressionMultiply, a, b, pa, pb)
def add(mat, a, b, pa='', pb=''): return op(mat, unreal.MaterialExpressionAdd, a, b, pa, pb)


def lerp(mat, a, b, alpha, pin_alpha=''):
    n = node(mat, unreal.MaterialExpressionLinearInterpolate)
    link(a, n, 'A'); link(b, n, 'B'); link(alpha, n, 'Alpha', pin_alpha)
    return n


def mask(mat, src, r=False, g=False, b=False, a=False):
    m = node(mat, unreal.MaterialExpressionComponentMask, r=r, g=g, b=b, a=a)
    link(src, m, ''); return m


def texture(name, normal=False):
    path = f'{DEST}/{name}'
    task = unreal.AssetImportTask()
    task.filename = str(SOURCE / f'{name}.png'); task.destination_path = DEST; task.destination_name = name
    task.automated = True; task.replace_existing = True; task.save = False
    TOOLS.import_asset_tasks([task])
    tex = unreal.load_asset(path)
    if not isinstance(tex, unreal.Texture2D): raise RuntimeError(f'Texture import failed: {name}')
    if normal:
        tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property('srgb', False)
    ASSETS.save_loaded_asset(tex, only_if_is_dirty=False)
    return tex


def sample(mat, tex, uv=None, normal=False):
    s = node(mat, unreal.MaterialExpressionTextureSample, texture=tex,
             sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if normal else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    if uv is not None: link(uv, s, 'UVs')
    return s


def material(name, build):
    path = f'{DEST}/{name}'
    mat = unreal.load_asset(path)
    if mat:
        LIB.delete_all_material_expressions(mat)
    else:
        mat = TOOLS.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('used_with_instanced_static_meshes', True)
    build(mat)
    LIB.recompile_material(mat)
    ASSETS.save_loaded_asset(mat, only_if_is_dirty=False)
    unreal.log(f'CHUCK_FALL_MATERIAL {name}')
    return mat


def wind(mat, amount, weight):
    """A slow sway plus a leaf flutter, scaled by the vertex alpha (tips move, bases don't)."""
    pos = node(mat, unreal.MaterialExpressionWorldPosition)
    across = add(mat, mask(mat, pos, r=True), mask(mat, pos, g=True))
    drift = mul(mat, across, const(mat, .0021))
    t = node(mat, unreal.MaterialExpressionTime)
    phase = add(mat, mul(mat, t, const(mat, .22)), drift)
    sx = node(mat, unreal.MaterialExpressionSine, period=1.); link(phase, sx, '')
    sy = node(mat, unreal.MaterialExpressionCosine, period=1.4); link(phase, sy, '')
    flutter_phase = add(mat, mul(mat, t, const(mat, 2.3)), mul(mat, across, const(mat, .05)))
    fz = node(mat, unreal.MaterialExpressionSine, period=1.); link(flutter_phase, fz, '')
    xy = node(mat, unreal.MaterialExpressionAppendVector); link(sx, xy, 'A'); link(sy, xy, 'B')
    xyz = node(mat, unreal.MaterialExpressionAppendVector); link(xy, xyz, 'A'); link(mul(mat, fz, const(mat, .35)), xyz, 'B')
    return mul(mat, mul(mat, xyz, const(mat, amount)), weight)


def leaves(tex):
    def build(mat):
        mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
        mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
        mat.set_editor_property('two_sided', True)
        mat.set_editor_property('opacity_mask_clip_value', .4)
        vc = node(mat, unreal.MaterialExpressionVertexColor)
        s = sample(mat, tex)
        bright = lerp(mat, const(mat, .55), const(mat, 1.0), vc, 'R')
        hue = lerp(mat, colour(mat, .9, 1.0, .7), colour(mat, .7, .95, .8), vc, 'G')
        base = mul(mat, mul(mat, s, hue, 'RGB'), bright)
        dried = base   # (a vertex-blue dryness tint read as flat cream on some leaves; dropped)
        out(mat, dried, unreal.MaterialProperty.MP_BASE_COLOR)
        out(mat, s, unreal.MaterialProperty.MP_OPACITY_MASK, 'A')
        out(mat, mul(mat, dried, colour(mat, .25, .35, .08)), unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
        out(mat, const(mat, .7), unreal.MaterialProperty.MP_ROUGHNESS)
        out(mat, const(mat, .2), unreal.MaterialProperty.MP_SPECULAR)
        weight = mul(mat, vc, vc, 'A', 'A')
        out(mat, wind(mat, 6., weight), unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    return build


def canopy(mat):
    vc = node(mat, unreal.MaterialExpressionVertexColor)
    pos = node(mat, unreal.MaterialExpressionWorldPosition)
    n = node(mat, unreal.MaterialExpressionNoise, scale=.02, levels=3, output_min=0., output_max=1., quality=1)
    link(pos, n, '')
    g = lerp(mat, colour(mat, .035, .085, .02), colour(mat, .07, .15, .035), n)
    out(mat, mul(mat, g, lerp(mat, const(mat, .75), const(mat, 1.2), vc, 'R')), unreal.MaterialProperty.MP_BASE_COLOR)
    out(mat, const(mat, .85), unreal.MaterialProperty.MP_ROUGHNESS)
    out(mat, const(mat, .3), unreal.MaterialProperty.MP_SPECULAR)
    out(mat, wind(mat, 3., mul(mat, vc, vc, 'A', 'A')), unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)


def bark(tex, nrm):
    def build(mat):
        vc = node(mat, unreal.MaterialExpressionVertexColor)
        uv = node(mat, unreal.MaterialExpressionTextureCoordinate, u_tiling=1., v_tiling=1.)
        s = sample(mat, tex, uv)
        base = mul(mat, s, lerp(mat, const(mat, .7), const(mat, 1.2), vc, 'R'), 'RGB')
        up = mask(mat, node(mat, unreal.MaterialExpressionVertexNormalWS), b=True)
        upward = node(mat, unreal.MaterialExpressionClamp); link(add(mat, mul(mat, up, const(mat, 1.5)), const(mat, .3)), upward, '')
        moss = mul(mat, mul(mat, mul(mat, s, vc, 'A', 'G'), upward), const(mat, 1.4))
        moss_c = node(mat, unreal.MaterialExpressionClamp); link(moss, moss_c, '')
        out(mat, lerp(mat, base, colour(mat, .09, .16, .035), moss_c), unreal.MaterialProperty.MP_BASE_COLOR)
        out(mat, sample(mat, nrm, uv, normal=True), unreal.MaterialProperty.MP_NORMAL, 'RGB')
        out(mat, const(mat, .9), unreal.MaterialProperty.MP_ROUGHNESS)
        out(mat, const(mat, .25), unreal.MaterialProperty.MP_SPECULAR)
        out(mat, wind(mat, 4., mul(mat, vc, vc, 'A', 'A')), unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    return build


def ground(tex, nrm):
    def build(mat):
        pos = node(mat, unreal.MaterialExpressionWorldPosition)
        xy = mask(mat, pos, r=True, g=True)
        uv = mul(mat, xy, const(mat, 1 / 260.))
        macro_uv = mul(mat, xy, const(mat, 1 / 1900.))
        s = sample(mat, tex, uv)
        m = sample(mat, tex, macro_uv)
        base = mul(mat, mul(mat, s, add(mat, mul(mat, m, const(mat, .8), 'G'), const(mat, .65)), 'RGB'), const(mat, 1.))
        out(mat, base, unreal.MaterialProperty.MP_BASE_COLOR)
        out(mat, sample(mat, nrm, uv, normal=True), unreal.MaterialProperty.MP_NORMAL, 'RGB')
        out(mat, const(mat, .88), unreal.MaterialProperty.MP_ROUGHNESS)
        out(mat, const(mat, .3), unreal.MaterialProperty.MP_SPECULAR)
    return build


def jungle_far(mat):
    """Canopy from the air: crowns (~9 m), stands of trees (~60 m), and broad
    patches (~600 m) of darker and yellower forest."""
    pos = node(mat, unreal.MaterialExpressionWorldPosition)
    def noise(scale, levels):
        n = node(mat, unreal.MaterialExpressionNoise, scale=scale, levels=levels, output_min=0., output_max=1., quality=1)
        link(pos, n, ''); return n
    crowns, stands, broad = noise(.0011, 2), noise(.00016, 3), noise(.000016, 4)
    mix = add(mat, mul(mat, crowns, const(mat, .45)), mul(mat, stands, const(mat, .55)))
    g = lerp(mat, colour(mat, .012, .035, .01), colour(mat, .075, .15, .035), mix)
    hue = lerp(mat, colour(mat, 1.25, 1.05, .6), colour(mat, .75, 1.05, 1.05), broad)
    out(mat, mul(mat, g, hue), unreal.MaterialProperty.MP_BASE_COLOR)
    out(mat, const(mat, .9), unreal.MaterialProperty.MP_ROUGHNESS)
    out(mat, const(mat, .2), unreal.MaterialProperty.MP_SPECULAR)


def cloud_puff(mat):
    """Mist rushing past him in the deck: soft unlit puffs, brighter on the
    sun's side, thin at their edges, broken by noise, faded where they cut
    into each other and as they reach the lens."""
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    n = node(mat, unreal.MaterialExpressionVertexNormalWS)
    to_sun = node(mat, unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(-.814, -.296, .5, 1))
    lit = node(mat, unreal.MaterialExpressionDotProduct); link(n, lit, 'A'); link(to_sun, lit, 'B')
    half = add(mat, mul(mat, lit, const(mat, .5)), const(mat, .5))
    out(mat, lerp(mat, colour(mat, .42, .48, .56), colour(mat, 1.05, 1.03, 1.), half), unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    fres = node(mat, unreal.MaterialExpressionFresnel, exponent=1.)
    edge = node(mat, unreal.MaterialExpressionOneMinus); link(fres, edge, '')
    soft = mul(mat, edge, edge)
    pos = node(mat, unreal.MaterialExpressionWorldPosition)
    wisp = node(mat, unreal.MaterialExpressionNoise, scale=.0012, levels=3, output_min=.15, output_max=1., quality=1)
    link(pos, wisp, '')
    depth = node(mat, unreal.MaterialExpressionPixelDepth)
    near = node(mat, unreal.MaterialExpressionClamp); link(mul(mat, add(mat, depth, const(mat, -300.)), const(mat, 1 / 1500.)), near, '')
    alpha = mul(mat, mul(mat, mul(mat, soft, wisp), near), const(mat, .55))
    fade = node(mat, unreal.MaterialExpressionDepthFade, fade_distance_default=600.)
    link(alpha, fade, 'Opacity')
    out(mat, fade, unreal.MaterialProperty.MP_OPACITY)


def clouds():
    path = f'{DEST}/MI_FallClouds'
    # The engine's own instance carries the cloud textures (the base material's are black placeholders).
    parent = unreal.load_asset('/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst')
    if not parent: raise RuntimeError('Engine volumetric cloud material missing')
    mi = unreal.load_asset(path) or TOOLS.create_asset('MI_FallClouds', DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    LIB.set_material_instance_parent(mi, parent)
    for n in LIB.get_scalar_parameter_names(mi):
        unreal.log(f'CHUCK_FALL_CLOUD_PARAM {n}={LIB.get_material_instance_scalar_parameter_value(mi, n):.4f}')
    for n in LIB.get_texture_parameter_names(mi):
        tex = LIB.get_material_instance_texture_parameter_value(mi, n)
        unreal.log(f'CHUCK_FALL_CLOUD_PARAM {n}={tex.get_path_name() if tex else None}')
    ASSETS.save_loaded_asset(mi, only_if_is_dirty=False)
    return mi


# Music: the user's track, played once.
task = unreal.AssetImportTask()
task.filename = str(ROOT / 'SourceAssets/Audio/FallToChult.wav')
task.destination_path = '/Game/Art/Audio'; task.destination_name = 'SW_FallToChult'
task.automated = True; task.replace_existing = True; task.save = True
TOOLS.import_asset_tasks([task])
sound = unreal.load_asset('/Game/Art/Audio/SW_FallToChult')
if not isinstance(sound, unreal.SoundWave): raise RuntimeError('Fall music import failed')
sound.set_editor_property('looping', False); sound.set_editor_property('volume', 1.0)
ASSETS.save_loaded_asset(sound, only_if_is_dirty=False)
unreal.log(f'CHUCK_FALL_MUSIC duration={sound.get_editor_property("duration"):.2f}')

# Wind and leaf crashes (Tools/gen_fall_sfx.py).
for name, looping in (('SFX_FallWind_00', True), ('SFX_LeafCrash_00', False), ('SFX_LeafCrash_01', False), ('SFX_LeafCrash_02', False)):
    task = unreal.AssetImportTask()
    task.filename = str(ROOT / f'SourceAssets/Audio/SFX/{name}.wav')
    task.destination_path = '/Game/Art/Audio/SFX'; task.destination_name = name
    task.automated = True; task.replace_existing = True; task.save = True
    TOOLS.import_asset_tasks([task])
    sfx = unreal.load_asset(f'/Game/Art/Audio/SFX/{name}')
    if not isinstance(sfx, unreal.SoundWave): raise RuntimeError(f'{name} import failed')
    sfx.set_editor_property('looping', looping)
    ASSETS.save_loaded_asset(sfx, only_if_is_dirty=False)
unreal.log('CHUCK_FALL_SFX_IMPORTED 4')

leaf_tex = texture('T_FallLeaves')
bark_tex = texture('T_FallBark'); bark_n = texture('T_FallBark_N', True)
ground_tex = texture('T_FallGround'); ground_n = texture('T_FallGround_N', True)
mats = {
    'M_FallLeaves': material('M_FallLeaves', leaves(leaf_tex)),
    'M_FallCanopy': material('M_FallCanopy', canopy),
    'M_FallBark': material('M_FallBark', bark(bark_tex, bark_n)),
}
material('M_FallGround', ground(ground_tex, ground_n))
material('M_FallJungleFar', jungle_far)
material('M_FallCloudPuff', cloud_puff)
clouds()

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
names = ['SM_FallTree_A', 'SM_FallTree_B', 'SM_FallPalm', 'SM_FallFern', 'SM_FallElephantEar',
         'SM_FallShrub', 'SM_FallLog', 'SM_FallCrown', 'SM_FallLeafCard']
for name in names:
    task = unreal.AssetImportTask()
    task.filename = str(SOURCE / f'{name}.fbx'); task.destination_path = DEST; task.destination_name = name
    task.automated = True; task.replace_existing = True; task.save = False; task.options = options
    TOOLS.import_asset_tasks([task])
    mesh = unreal.load_asset(f'{DEST}/{name}')
    if not isinstance(mesh, unreal.StaticMesh): raise RuntimeError(f'Missing mesh {name}')
    slots = mesh.get_editor_property('static_materials')
    for i, slot in enumerate(slots):
        slot_name = str(slot.get_editor_property('material_slot_name'))
        key = next((k for k in mats if slot_name.startswith(k)), None)
        if key is None: raise RuntimeError(f'{name}: unexpected slot {slot_name}')
        mesh.set_material(i, mats[key])
    ASSETS.save_loaded_asset(mesh, only_if_is_dirty=False)
    box = mesh.get_bounding_box()
    unreal.log(f'CHUCK_FALL_MESH {name} slots={len(slots)} height_cm={box.max.z - box.min.z:.0f}')
unreal.log('CHUCK_FALL_CUTSCENE_IMPORT_READY')
