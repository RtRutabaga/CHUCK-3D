"""The Chult dinosaurs' shared skin material (Claude, 2026-10-16).

UnrealEditor-Cmd <uproject> -ExecutePythonScript=Tools/build_dino_skin.py -unattended -nosplash -NoLiveCoding

User request: "make both raptors and the t-rex look better, less glossy and
fake". M_Raptor and M_TRex (import_raptor.py / import_trex.py) were a
smooth, fairly glossy hide (roughness 0.62, specular 0.45, the dark parts
0.3) with one even colour per region. M_DinoSkin keeps their parameters
(BodyColor, BackColor, BandColor, BellyColor, PatternAmount, Roughness,
Specular) and each creature's own pattern and scale textures, and adds:
- uneven colour: broad blotches, mottling and speckle (T_DinoMottle), a
  warmer earth tint through the mottling, and darker creases;
- a hide of irregular plates and creases (T_DinoFolds_N) over the scales;
- dry skin: rough (0.84 base, more in the creases), little specular;
- mud and dust up the legs and under the belly, by height on the body.
Then every MI_Raptor*/MI_TRex* is re-parented to it with the creature's
textures and tiling. M_Raptor and M_TRex are left in place, unused.

Textures: Tools/build_dino_skin_textures.py (Blender) ->
SourceAssets/Enemies/Shared. M_DinoSkin is built once (rebuilding an
existing graph trips an engine assertion); later runs update the instances.
"""
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1]
SHARED = ROOT / 'SourceAssets/Enemies/Shared'
DEST = '/Game/Characters/Dinos'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
LIB = unreal.MaterialEditingLibrary
ASSETS = unreal.EditorAssetLibrary


def node(mat, cls, x=0, y=0, **props):
    n = LIB.create_material_expression(mat, cls, x, y)
    for k, v in props.items(): n.set_editor_property(k, v)
    return n


def link(a, b, pin_b, pin_a=''):
    if not LIB.connect_material_expressions(a, pin_a, b, pin_b):
        raise RuntimeError(f'Material link failed: {pin_a} -> {pin_b}')


def out(expr, prop, pin=''):
    if not LIB.connect_material_property(expr, pin, prop):
        raise RuntimeError(f'Material output failed: {prop}')


def texture(name, normal=False, srgb=True):
    task = unreal.AssetImportTask()
    task.filename = str(SHARED / f'{name}.png'); task.destination_path = DEST; task.destination_name = name
    task.automated = True; task.replace_existing = True; task.save = False
    TOOLS.import_asset_tasks([task])
    tex = unreal.load_asset(f'{DEST}/{name}')
    if not isinstance(tex, unreal.Texture2D): raise RuntimeError(f'Texture import failed: {name}')
    if normal:
        tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
    tex.set_editor_property('srgb', srgb and not normal)
    ASSETS.save_loaded_asset(tex, only_if_is_dirty=False)
    return tex


mottle = texture('T_DinoMottle', srgb=False)
folds = texture('T_DinoFolds_N', normal=True)
raptor_pattern = unreal.load_asset('/Game/Characters/Raptor/T_RaptorPattern')
raptor_scales = unreal.load_asset('/Game/Characters/Raptor/T_RaptorScales')
raptor_scales_n = unreal.load_asset('/Game/Characters/Raptor/T_RaptorScales_N')
for t in (raptor_pattern, raptor_scales, raptor_scales_n):
    if t is None: raise RuntimeError('Raptor textures missing: run Tools/import_raptor.py first')


def scalar(mat, name, v, x, y):
    return node(mat, unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=v)


def vector(mat, name, rgb, x, y):
    return node(mat, unreal.MaterialExpressionVectorParameter, x, y, parameter_name=name, default_value=unreal.LinearColor(*rgb, 1))


def tex_param(mat, name, tex, x, y, sampler):
    return node(mat, unreal.MaterialExpressionTextureSampleParameter2D, x, y, parameter_name=name, texture=tex, sampler_type=sampler)


def tiled(mat, uv, name_u, name_v, u, v, x, y):
    """UV0 times (U, V) scalar parameters."""
    pu = scalar(mat, name_u, u, x - 250, y); pv = scalar(mat, name_v, v, x - 250, y + 60)
    ap = node(mat, unreal.MaterialExpressionAppendVector, x - 100, y); link(pu, ap, 'A'); link(pv, ap, 'B')
    m = node(mat, unreal.MaterialExpressionMultiply, x, y); link(uv, m, 'A'); link(ap, m, 'B')
    return m


def mul(mat, a, b, x, y, pin_a='', pin_b=''):
    m = node(mat, unreal.MaterialExpressionMultiply, x, y); link(a, m, 'A', pin_a); link(b, m, 'B', pin_b); return m


def lerp(mat, a, b, alpha, x, y, pin_alpha=''):
    l = node(mat, unreal.MaterialExpressionLinearInterpolate, x, y); link(a, l, 'A'); link(b, l, 'B'); link(alpha, l, 'Alpha', pin_alpha); return l


def lerp_const(mat, lo, hi, alpha, x, y, pin_alpha=''):
    l = node(mat, unreal.MaterialExpressionLinearInterpolate, x, y, const_a=lo, const_b=hi); link(alpha, l, 'Alpha', pin_alpha); return l


path = f'{DEST}/M_DinoSkin'
mat = unreal.load_asset(path)
fresh = mat is None
if fresh:
    mat = TOOLS.create_asset('M_DinoSkin', DEST, unreal.Material, unreal.MaterialFactoryNew())
mat.set_editor_property('used_with_skeletal_mesh', True)
mat.set_editor_property('two_sided', True)
if fresh:
    LC = unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR
    uv = node(mat, unreal.MaterialExpressionTextureCoordinate, -2600, 0)
    pat = tex_param(mat, 'Pattern', raptor_pattern, -1900, -400, LC)
    link(uv, pat, 'UVs')
    det = tex_param(mat, 'Scales', raptor_scales, -1900, 0, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    scale_uv = tiled(mat, uv, 'ScaleTileU', 'ScaleTileV', 46., 15., -2100, 100)
    link(scale_uv, det, 'UVs')
    det_n = tex_param(mat, 'ScalesN', raptor_scales_n, -1900, 300, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    link(scale_uv, det_n, 'UVs')
    mot = tex_param(mat, 'Mottle', mottle, -1900, 600, LC)
    link(tiled(mat, uv, 'MottleTileU', 'MottleTileV', 2., 1., -2100, 650), mot, 'UVs')
    fold = tex_param(mat, 'FoldsN', folds, -1900, 900, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    fold_uv = tiled(mat, uv, 'FoldTileU', 'FoldTileV', 3., 1.5, -2100, 950)
    link(fold_uv, fold, 'UVs')
    mot_c = tex_param(mat, 'MottleCavity', mottle, -1900, 1200, LC)   # the folds' cavity mask, on the folds' tiling
    link(fold_uv, mot_c, 'UVs')

    # Pattern colours, as M_Raptor: back over body, bands, belly; scaled by PatternAmount.
    body = vector(mat, 'BodyColor', (.055, .065, .032), -1500, -700)
    back = vector(mat, 'BackColor', (.035, .042, .022), -1500, -600)
    band = vector(mat, 'BandColor', (.022, .017, .012), -1500, -500)
    belly = vector(mat, 'BellyColor', (.19, .16, .11), -1500, -400)
    amount = scalar(mat, 'PatternAmount', 1., -1500, -300)
    l1 = lerp(mat, body, back, mul(mat, pat, amount, -1300, -250, 'B'), -1100, -650)
    l2 = lerp(mat, l1, band, mul(mat, pat, amount, -1300, -150, 'R'), -950, -600)
    l3 = lerp(mat, l2, belly, mul(mat, pat, amount, -1300, -50, 'G'), -800, -550)
    col = mul(mat, l3, det, -650, -500, '', 'R')                                   # scale tone
    # Uneven colour: broad blotches, mottling (warmer, earthier patches), speckle.
    blotch = lerp(mat, scalar(mat, 'BlotchDark', .62, -950, -330), scalar(mat, 'BlotchLight', 1.25, -950, -270), mot, -800, -300, 'R')
    col = mul(mat, col, blotch, -500, -450)
    tint = vector(mat, 'EarthTint', (1.25, 1.0, .7), -800, -200)
    tint_amt = scalar(mat, 'TintAmount', .55, -800, -100)
    tinted = mul(mat, col, tint, -500, -300)
    col = lerp(mat, col, tinted, mul(mat, mot, tint_amt, -650, -150, 'G'), -350, -400)
    col = mul(mat, col, lerp_const(mat, .88, 1.08, mot, -500, -100, 'B'), -200, -400)
    # Creases darker.
    cav_dark = scalar(mat, 'CreaseDarken', .5, -650, 0)
    cav = mul(mat, mot_c, cav_dark, -500, 0, 'A')
    om = node(mat, unreal.MaterialExpressionOneMinus, -350, 0); link(cav, om, '')
    col = mul(mat, col, om, -100, -350)
    # Mud and dust up the legs and under the belly, by height on the body (pre-skinned, cm).
    pos = node(mat, unreal.MaterialExpressionPreSkinnedPosition, -1300, 300)
    zv = node(mat, unreal.MaterialExpressionComponentMask, -1150, 300, r=False, g=False, b=True, a=False); link(pos, zv, '')
    z = node(mat, unreal.MaterialExpressionVertexInterpolator, -1050, 300); link(zv, z, 'VS')   # vertex-shader only
    mud_h = scalar(mat, 'MudHeight', 30., -1150, 380)
    mud_f = scalar(mat, 'MudFade', 25., -1150, 440)
    sub = node(mat, unreal.MaterialExpressionSubtract, -1000, 320); link(mud_h, sub, 'A'); link(z, sub, 'B')
    div = node(mat, unreal.MaterialExpressionDivide, -880, 340); link(sub, div, 'A'); link(mud_f, div, 'B')
    sat = node(mat, unreal.MaterialExpressionSaturate, -770, 340); link(div, sat, '')
    mud_amt = scalar(mat, 'MudAmount', .75, -770, 420)
    mud = mul(mat, mul(mat, sat, mud_amt, -650, 360), lerp_const(mat, .45, 1., mot, -650, 460, 'G'), -500, 380)
    mud_col = vector(mat, 'MudColor', (.07, .055, .04), -350, 250)
    final = lerp(mat, col, mud_col, mud, 0, -200)
    out(final, unreal.MaterialProperty.MP_BASE_COLOR)

    # Dry skin: rough, more so in creases, between scales and under mud; little specular.
    rough = scalar(mat, 'Roughness', .84, -500, 600)
    r1 = node(mat, unreal.MaterialExpressionAdd, -350, 620); link(rough, r1, 'A'); link(mul(mat, mot_c, node(mat, unreal.MaterialExpressionConstant, -650, 700, r=.12), -500, 700, 'A'), r1, 'B')
    crev = node(mat, unreal.MaterialExpressionOneMinus, -650, 780); link(det, crev, '', 'R')
    r2 = node(mat, unreal.MaterialExpressionAdd, -200, 660); link(r1, r2, 'A'); link(mul(mat, crev, node(mat, unreal.MaterialExpressionConstant, -650, 840, r=.1), -500, 800), r2, 'B')
    r3 = node(mat, unreal.MaterialExpressionAdd, -80, 700); link(r2, r3, 'A'); link(mul(mat, mud, node(mat, unreal.MaterialExpressionConstant, -350, 900, r=.2), -200, 880), r3, 'B')
    rs = node(mat, unreal.MaterialExpressionSaturate, 50, 700); link(r3, rs, '')
    out(rs, unreal.MaterialProperty.MP_ROUGHNESS)
    spec = scalar(mat, 'Specular', .28, -200, 1000)
    out(mul(mat, spec, lerp_const(mat, 1., .35, mot_c, -200, 1080, 'A'), 50, 1000), unreal.MaterialProperty.MP_SPECULAR)

    # Normal: the scales (ScaleNormal) and the plates and creases (FoldNormal), blended (whiteout).
    flat = node(mat, unreal.MaterialExpressionConstant3Vector, -1600, 1500, constant=unreal.LinearColor(0, 0, 1, 1))
    sn = lerp(mat, flat, det_n, scalar(mat, 'ScaleNormal', .55, -1600, 1580), -1450, 1450)
    fn = lerp(mat, flat, fold, scalar(mat, 'FoldNormal', .9, -1600, 1700), -1450, 1650)
    sxy = node(mat, unreal.MaterialExpressionComponentMask, -1300, 1450, r=True, g=True, b=False, a=False); link(sn, sxy, '')
    fxy = node(mat, unreal.MaterialExpressionComponentMask, -1300, 1650, r=True, g=True, b=False, a=False); link(fn, fxy, '')
    sz = node(mat, unreal.MaterialExpressionComponentMask, -1300, 1550, r=False, g=False, b=True, a=False); link(sn, sz, '')
    fz = node(mat, unreal.MaterialExpressionComponentMask, -1300, 1750, r=False, g=False, b=True, a=False); link(fn, fz, '')
    xy = node(mat, unreal.MaterialExpressionAdd, -1150, 1500); link(sxy, xy, 'A'); link(fxy, xy, 'B')
    zz = mul(mat, sz, fz, -1150, 1650)
    n = node(mat, unreal.MaterialExpressionAppendVector, -1000, 1550); link(xy, n, 'A'); link(zz, n, 'B')
    nn = node(mat, unreal.MaterialExpressionNormalize, -850, 1550); link(n, nn, '')
    out(nn, unreal.MaterialProperty.MP_NORMAL)
LIB.recompile_material(mat)
ASSETS.save_loaded_asset(mat, only_if_is_dirty=False)
unreal.log(f'CHUCK_DINO_SKIN_MATERIAL fresh={fresh} expressions={LIB.get_num_material_expressions(mat)}')

# The instances: each creature's textures, tiling and per-region settings over its old values.
CREATURES = {
    'Raptor': {
        'textures': {'Pattern': 'T_RaptorPattern', 'Scales': 'T_RaptorScales', 'ScalesN': 'T_RaptorScales_N'},
        # 4.1 m long: plates about 5 cm; mud up to the ankles and shins.
        'common': {'ScaleTileU': 46., 'ScaleTileV': 15., 'MottleTileU': 2., 'MottleTileV': 1., 'FoldTileU': 3., 'FoldTileV': 1.5,
                   'MudHeight': 30., 'MudFade': 25.},
        'regions': {
            'RaptorHide': {'Roughness': .84, 'Specular': .28},
            'RaptorBelly': {'Roughness': .86, 'Specular': .25},
            'RaptorDark': {'Roughness': .6, 'Specular': .35, 'TintAmount': .15, 'MudAmount': .3, 'CreaseDarken': .2},   # claws, eyes, nostrils
        },
    },
    'TRex': {
        'textures': {'Pattern': 'T_TRexPattern', 'Scales': 'T_TRexScales', 'ScalesN': 'T_TRexScales_N'},
        # 12 m long: bigger plates and blotches; mud up to the shins.
        'common': {'ScaleTileU': 70., 'ScaleTileV': 20., 'MottleTileU': 3., 'MottleTileV': 1.5, 'FoldTileU': 5., 'FoldTileV': 3.,
                   'MudHeight': 110., 'MudFade': 80.},
        'regions': {
            'TRexHide': {'Roughness': .85, 'Specular': .28, 'FoldNormal': .8},
            'TRexPale': {'Roughness': .85, 'Specular': .28},
            'TRexBelly': {'Roughness': .87, 'Specular': .25, 'FoldNormal': .7},
            'TRexDark': {'Roughness': .6, 'Specular': .35, 'TintAmount': .15, 'MudAmount': .3, 'CreaseDarken': .2},
            'TRexMouth': {'Roughness': .45, 'Specular': .4, 'TintAmount': 0., 'MudAmount': 0., 'CreaseDarken': .25, 'FoldNormal': .4},
        },
    },
}
for creature, spec in CREATURES.items():
    folder = f'/Game/Characters/{creature}'
    for region, params in spec['regions'].items():
        mi = unreal.load_asset(f'{folder}/MI_{region}')
        if mi is None: raise RuntimeError(f'Missing MI_{region}: run Tools/import_{creature.lower()}.py first')
        mi.set_editor_property('parent', mat)
        for k, t in spec['textures'].items():
            LIB.set_material_instance_texture_parameter_value(mi, k, unreal.load_asset(f'{folder}/{t}'))
        for k, v in {**spec['common'], **params}.items():
            LIB.set_material_instance_scalar_parameter_value(mi, k, v)
        LIB.update_material_instance(mi)
        ASSETS.save_loaded_asset(mi, only_if_is_dirty=False)
        unreal.log(f'CHUCK_DINO_SKIN_INSTANCE {region} parent={mi.get_editor_property("parent").get_name()}')
unreal.log('CHUCK_DINO_SKIN_READY')
