"""Materials for Phlegethos's Feywild bleedthrough and the wastes' bus stop (Claude, 2026-11-17; docs/regions/PHLEGETHOS.md).

UnrealEditor-Cmd <uproject> -ExecutePythonScript=<absolute path to this file> -unattended -nosplash -NoLiveCoding

  M_FeyRiver      the Feywild river that breaks into the fortress forecourt (PhlegethosBleed.cpp): translucent running water,
                  teal-green and lit from within, two wave normals and foam streaming down it, sparkles drifting in it.
                  Mesh contract: UV0 u across (0..1), v down the flow (1 per metre); vertex colour R foam (its front, its
                  banks, where it pours), G how much it shows (0 at a fading end).
  M_FeyTear       the tears it pours out of and into: unlit translucent light of another world turning in a ragged opening,
                  spring green and cyan with violet in the folds, white at its heart. UV0 = offset from the centre / radius
                  (its ragged edge is near the unit circle); vertex colour A its edge fade.
  M_CityPaint     painted and anodised metal and plastic of the city fragment: base colour = vertex colour RGB, metallic =
                  vertex colour A.
  M_CityConcrete  pavement and kerb concrete: vertex colour G lays 60 cm paving joints, R grime.
  M_CityAsphalt   road surface: vertex colour R white paint, G yellow paint.

The river reuses the engine Water textures already copied into /Game/Art/Textures/Water (create_sewer_water_material.py).
Reruns rebuild these materials only.
"""
import unreal

LIB = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
ASSETS = unreal.EditorAssetLibrary
DEST = '/Game/Art/Phlegethos'
WATER = '/Game/Art/Textures/Water'

NOISE = r'''
struct FN {
  float h(float2 p) { return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453); }
  float n(float2 p) { float2 i = floor(p), f = frac(p); f = f * f * (3. - 2. * f);
    return lerp(lerp(h(i), h(i + float2(1, 0)), f.x), lerp(h(i + float2(0, 1)), h(i + 1.), f.x), f.y); }
  float fbm(float2 p) { float s = 0., a = .5; for (int k = 0; k < 5; k++) { s += a * n(p); p = p * 2.03 + 17.1; a *= .5; } return s; }
};
FN N;
'''

# Sparkles in the current: points of light drifting downstream, twinkling.
SPARKLE = NOISE + r'''
float2 q = UV * float2(9., 1.6) + float2(0., -T * 1.9);
float2 i = floor(q), f = frac(q);
float2 c = float2(N.h(i), N.h(i + 7.3));
float d = length(f - c);
float tw = .5 + .5 * sin(T * (3. + 4. * N.h(i + 3.1)) + 6.28 * N.h(i + 9.2));
float s = smoothstep(.09, 0., d) * step(.72, N.h(i + 1.7)) * tw;
float2 q2 = UV * float2(17., 3.1) + float2(.3, -T * 2.6);
float2 i2 = floor(q2), f2 = frac(q2);
float d2 = length(f2 - float2(N.h(i2 + 2.2), N.h(i2 + 5.9)));
s += .6 * smoothstep(.07, 0., d2) * step(.8, N.h(i2 + 4.4)) * (.5 + .5 * sin(T * 5. + 6.28 * N.h(i2)));
return s;
'''

TEAR = NOISE + r'''
float r = length(UV);
float a = atan2(UV.y, UV.x);
float sw = a + T * .35 + 2.2 * (1. - r);
float2 p = float2(cos(sw), sin(sw)) * r * 2.4 + float2(T * .12, -T * .07);
float f = N.fbm(p * 1.6 + 3.1);
float g = N.fbm(p * 3.3 - 7.4 + f);
float3 green = float3(.35, 1., .45), cyan = float3(.2, .95, .95), violet = float3(.7, .35, 1.);
float3 col = lerp(green, cyan, smoothstep(.35, .7, f));
col = lerp(col, violet, smoothstep(.62, .85, g) * .6);
float core = smoothstep(.75, 0., r);
col = lerp(col, float3(.92, 1., .9), core * core * .8);
float rim = smoothstep(.72, .97, r) * (1. - smoothstep(.97, 1.04, r));
col += float3(.6, 1., .8) * rim * (.3 + .3 * sin(a * 9. + T * 2.));
float bright = 1.3 + 2.4 * core + 1.2 * g;   // (at 5..14 it blew out to a flat mint)
float alpha = (1. - smoothstep(.93, 1.03, r)) * Fade;
return float4(col * bright, alpha);
'''

CONCRETE = NOISE + r'''
float2 w = P.xy;
float base = .27 + .06 * N.fbm(w / 37.) + .03 * N.n(w / 4.1);
float2 j = abs(frac(w / 60. + .5) - .5) * 60.;
float joint = (1. - smoothstep(.4, 1.1, min(j.x, j.y))) * step(.5, C.g);
base *= 1. - .45 * joint;
base *= 1. - .55 * C.r * (.6 + .4 * N.fbm(w / 23. + 5.));
float speck = step(.93, N.h(floor(w / 1.3)));
base += .04 * speck;
return float3(base, base * .985, base * .96);
'''

ASPHALT = NOISE + r'''
float2 w = P.xy;
float base = .042 + .015 * N.fbm(w / 29.) + .02 * step(.86, N.h(floor(w / 1.1)));
float3 col = float3(base, base, base * 1.04);
float wear = .75 + .25 * N.fbm(w / 9. + 2.);
col = lerp(col, float3(.62, .62, .6) * wear, saturate(C.r));
col = lerp(col, float3(.62, .45, .06) * wear, saturate(C.g));
return col;
'''


def material(name):
    path = f'{DEST}/{name}'
    mat = unreal.load_asset(path)
    if not mat:
        mat = TOOLS.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    if not mat:
        raise RuntimeError(f'Could not create {path}')
    LIB.delete_all_material_expressions(mat)
    return mat


def node(mat, cls, x=0, y=0, **props):
    n = LIB.create_material_expression(mat, cls, x, y)
    for k, v in props.items(): n.set_editor_property(k, v)
    return n


def link(a, b, pin_b, pin_a=''):
    if not LIB.connect_material_expressions(a, pin_a, b, pin_b):
        raise RuntimeError(f'Material link failed: {pin_a} -> {pin_b}')


def custom(mat, code, out_type, inputs):
    c = node(mat, unreal.MaterialExpressionCustom, -300, 0)
    items = []
    for key in inputs:
        item = unreal.CustomInput(); item.set_editor_property('input_name', key); items.append(item)
    c.set_editor_property('inputs', items)
    c.set_editor_property('output_type', out_type)
    c.set_editor_property('code', code)
    return c


def const(mat, v): return node(mat, unreal.MaterialExpressionConstant, r=v)
def vec3(mat, r, g, b): return node(mat, unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(r, g, b, 1))
def mul(mat, a, b):
    n = node(mat, unreal.MaterialExpressionMultiply); link(a, n, 'A'); link(b, n, 'B'); return n
def add(mat, a, b):
    n = node(mat, unreal.MaterialExpressionAdd); link(a, n, 'A'); link(b, n, 'B'); return n
def lerp(mat, a, b, alpha):
    n = node(mat, unreal.MaterialExpressionLinearInterpolate); link(a, n, 'A'); link(b, n, 'B'); link(alpha, n, 'Alpha'); return n
def mask(mat, src, ch):
    n = node(mat, unreal.MaterialExpressionComponentMask, r='r' in ch, g='g' in ch, b='b' in ch, a='a' in ch); link(src, n, ''); return n


def flowing(mat, uv, scale_u, scale_v, speed):
    scaled = mul(mat, uv, node(mat, unreal.MaterialExpressionConstant2Vector, r=scale_u, g=scale_v))
    pan = node(mat, unreal.MaterialExpressionPanner, speed_x=0., speed_y=-speed)
    link(scaled, pan, 'Coordinate')
    return pan


def sample(mat, tex, coords, normal=False):
    s = node(mat, unreal.MaterialExpressionTextureSample, texture=tex,
             sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if normal else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    if tex.get_editor_property('compression_settings') == unreal.TextureCompressionSettings.TC_HDR:
        s.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    link(coords, s, 'UVs')
    return s


def river():
    tex = {n: unreal.load_asset(f'{WATER}/{n}') for n in ('T_Water_TilingNormal_Waves_02', 'T_WaterFlow_01_Foam_Tiled', 'Caustics_Tiling_01_HDR')}
    for n, t in tex.items():
        if not t: raise RuntimeError(f'{WATER}/{n} missing: run Tools/create_sewer_water_material.py first')
    mat = material('M_FeyRiver')
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property('translucency_lighting_mode', unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    mat.set_editor_property('two_sided', True)
    mat.set_editor_property('tangent_space_normal', False)
    try: mat.set_editor_property('refraction_method', unreal.RefractionMode.RM_INDEX_OF_REFRACTION)
    except Exception: pass
    uv = node(mat, unreal.MaterialExpressionTextureCoordinate)
    vc = node(mat, unreal.MaterialExpressionVertexColor)
    time = node(mat, unreal.MaterialExpressionTime)
    across = mask(mat, uv, 'r')
    # A fast current: wave normals racing down it.
    n1 = sample(mat, tex['T_Water_TilingNormal_Waves_02'], flowing(mat, uv, 1.1, .9, 1.1), normal=True)
    n2 = sample(mat, tex['T_Water_TilingNormal_Waves_02'], flowing(mat, uv, .6, .45, .7), normal=True)
    blend = add(mat, n1, n2)
    norm = node(mat, unreal.MaterialExpressionNormalize); link(mul(mat, blend, vec3(mat, 1.15, 1.15, 1.)), norm, '')   # (rougher: it is a torrent)
    LIB.connect_material_property(norm, '', unreal.MaterialProperty.MP_NORMAL)
    centred = node(mat, unreal.MaterialExpressionSubtract, const_b=.5); link(across, centred, 'A')
    off = node(mat, unreal.MaterialExpressionAbs); link(centred, off, '')
    edge = node(mat, unreal.MaterialExpressionSmoothStep); link(const(mat, .32), edge, 'Min'); link(const(mat, .5), edge, 'Max'); link(off, edge, 'Value')
    foam_tex = sample(mat, tex['T_WaterFlow_01_Foam_Tiled'], flowing(mat, uv, .9, .7, 1.))
    foam_amt = node(mat, unreal.MaterialExpressionMax); link(mul(mat, edge, const(mat, .45)), foam_amt, 'A'); link(mask(mat, vc, 'r'), foam_amt, 'B')
    foam = node(mat, unreal.MaterialExpressionSaturate); link(mul(mat, mul(mat, mask(mat, foam_tex, 'r'), foam_amt), const(mat, 1.3)), foam, '')
    # (2026-11-17, first look: at .012/.09/.085 with its glow it read as pale lit ice; deeper, clearer, less foam)
    water = vec3(mat, .006, .05, .045)
    col = lerp(mat, water, vec3(mat, .3, .42, .4), foam)
    LIB.connect_material_property(col, '', unreal.MaterialProperty.MP_BASE_COLOR)
    LIB.connect_material_property(lerp(mat, const(mat, .06), const(mat, .5), foam), '', unreal.MaterialProperty.MP_ROUGHNESS)
    LIB.connect_material_property(const(mat, .5), '', unreal.MaterialProperty.MP_SPECULAR)
    # Lit from within (the Feywild's light comes with it), caustics running through it, sparkles, glowing foam.
    caust = mask(mat, sample(mat, tex['Caustics_Tiling_01_HDR'], flowing(mat, uv, .8, .7, .9)), 'r')
    glow = add(mat, vec3(mat, .008, .075, .06), mul(mat, caust, vec3(mat, .002, .01, .008)))
    sp = custom(mat, SPARKLE, unreal.CustomMaterialOutputType.CMOT_FLOAT1, ('UV', 'T'))
    link(uv, sp, 'UV'); link(time, sp, 'T')
    sparkle = mul(mat, sp, vec3(mat, 1.6, 2.4, 2.))
    fres = node(mat, unreal.MaterialExpressionFresnel, exponent=3., base_reflect_fraction=.05); link(norm, fres, 'Normal')
    sheen = mul(mat, fres, vec3(mat, .03, .1, .09))
    foam_glow = mul(mat, foam, vec3(mat, .08, .14, .12))
    LIB.connect_material_property(add(mat, add(mat, add(mat, glow, sparkle), sheen), foam_glow), '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    depth = node(mat, unreal.MaterialExpressionDepthFade, fade_distance_default=10.)
    body = mul(mat, depth, const(mat, .72))
    op = node(mat, unreal.MaterialExpressionMax); link(body, op, 'A'); link(foam, op, 'B')
    LIB.connect_material_property(mul(mat, op, mask(mat, vc, 'g')), '', unreal.MaterialProperty.MP_OPACITY)
    LIB.connect_material_property(const(mat, 1.33), '', unreal.MaterialProperty.MP_REFRACTION)
    return mat


def tear():
    mat = material('M_FeyTear')
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property('two_sided', True)
    c = custom(mat, TEAR, unreal.CustomMaterialOutputType.CMOT_FLOAT4, ('UV', 'T', 'Fade'))
    link(node(mat, unreal.MaterialExpressionTextureCoordinate), c, 'UV')
    link(node(mat, unreal.MaterialExpressionTime), c, 'T')
    link(mask(mat, node(mat, unreal.MaterialExpressionVertexColor), 'a'), c, 'Fade')
    LIB.connect_material_property(mask(mat, c, 'rgb'), '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    LIB.connect_material_property(mask(mat, c, 'a'), '', unreal.MaterialProperty.MP_OPACITY)
    return mat


def paint():
    mat = material('M_CityPaint')
    vc = node(mat, unreal.MaterialExpressionVertexColor)
    metal = mask(mat, vc, 'a')
    LIB.connect_material_property(mask(mat, vc, 'rgb'), '', unreal.MaterialProperty.MP_BASE_COLOR)
    LIB.connect_material_property(metal, '', unreal.MaterialProperty.MP_METALLIC)
    LIB.connect_material_property(lerp(mat, const(mat, .5), const(mat, .3), metal), '', unreal.MaterialProperty.MP_ROUGHNESS)
    return mat


def surface(name, code, rough):
    mat = material(name)
    c = custom(mat, code, unreal.CustomMaterialOutputType.CMOT_FLOAT3, ('P', 'C'))
    link(node(mat, unreal.MaterialExpressionWorldPosition), c, 'P')
    link(node(mat, unreal.MaterialExpressionVertexColor), c, 'C')
    LIB.connect_material_property(c, '', unreal.MaterialProperty.MP_BASE_COLOR)
    LIB.connect_material_property(const(mat, rough), '', unreal.MaterialProperty.MP_ROUGHNESS)
    return mat


for mat in (river(), tear(), paint(), surface('M_CityConcrete', CONCRETE, .88), surface('M_CityAsphalt', ASPHALT, .9)):
    LIB.recompile_material(mat)
    if not ASSETS.save_loaded_asset(mat, only_if_is_dirty=False):
        raise RuntimeError(mat.get_path_name())
    unreal.log(f'CHUCK_FEY_MATERIAL {mat.get_name()}')
unreal.log('CHUCK_FEY_MATERIALS_READY')
