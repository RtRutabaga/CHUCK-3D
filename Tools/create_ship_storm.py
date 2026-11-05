"""The ship's storm materials (Claude, 2026-11-03; docs/SHIP.md, "The chest, the trigger and the storm").

UnrealEditor-Cmd <uproject> -run=pythonscript -script=Tools/create_ship_storm.py -unattended -nullrhi -nosplash

Drawn on the engine's plane (instanced: rain, splashes, spray) and on the bolt's ribbon (ShipStorm.cpp). No textures:
the shapes come from the plane's local position (it runs -50..50 cm in X and Y), so they do not depend on its UVs.
  M_ShipRain   a rain streak: lit translucent, two-sided, thin and soft across (local Y), tapered at both ends
               (local X runs along the fall); faint, so a thousand of them read as a curtain of rain.
  M_ShipSpray  a soft broken blob of thrown water or a splash: lit translucent, its opacity times the instance's custom
               data 0 (ShipStorm.cpp fades each blob), broken up by a noise on the blob.
  M_ShipBolt   a lightning channel: unlit additive, not fogged, white-violet, brightest along the ribbon's middle (UV x
               0..1 across), times the Intensity parameter (the flash, set each frame).
  M_ShipSpume  (2026-11-04) white water billowing: lit translucent, Unity Labs' CC0 smoke flipbook (T_JetSmoke, imported
               by Tools/import_temple_fire.py) as its shape; custom data 0 the frame, 2 the opacity.
  M_ShipDeckWash (2026-11-04) a sea washing across the deck: lit translucent water, vertex colour R foam, G opacity.
Creates or rebuilds only these five assets in /Game/Art/Ship; imports nothing.
"""
import unreal

L = unreal.MaterialEditingLibrary
A = unreal.EditorAssetLibrary
T = unreal.AssetToolsHelpers.get_asset_tools()
DEST = '/Game/Art/Ship'
if not A.does_directory_exist(DEST):
    A.make_directory(DEST)


def material(name, blend, shading):
    m = unreal.load_asset(DEST + '/' + name) or T.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    L.delete_all_material_expressions(m)
    m.set_editor_property('blend_mode', blend)
    m.set_editor_property('shading_model', shading)
    m.set_editor_property('two_sided', True)
    m.set_editor_property('used_with_instanced_static_meshes', True)
    if shading != unreal.MaterialShadingModel.MSM_UNLIT:
        m.set_editor_property('translucency_lighting_mode', unreal.TranslucencyLightingMode.TLM_VOLUMETRIC_NON_DIRECTIONAL)
    return m


def node(m, cls, **props):
    n = L.create_material_expression(m, cls)
    for k, v in props.items():
        n.set_editor_property(k, v)
    return n


def link(a, b, pin='', out=''):
    if not L.connect_material_expressions(a, out, b, pin):
        raise RuntimeError('Link failed %s -> %s' % (out, pin))


def prop(m, n, p, out=''):
    if not L.connect_material_property(n, out, p):
        raise RuntimeError('Property link failed ' + str(p))


def custom(m, code, out_type, inputs):
    n = node(m, unreal.MaterialExpressionCustom, output_type=out_type, code=code)
    ins = []
    for key in inputs:
        i = unreal.CustomInput(); i.set_editor_property('input_name', key); ins.append(i)
    n.set_editor_property('inputs', ins)
    for key, src in inputs.items():
        link(src, n, key)
    return n


def local_pos(m):
    n = node(m, unreal.MaterialExpressionLocalPosition)
    return n


def save(m, name):
    L.recompile_material(m)
    if not A.save_loaded_asset(m, only_if_is_dirty=False):
        raise RuntimeError('Save failed ' + name)
    unreal.log('CHUCK_SHIP_STORM_MATERIAL ' + DEST + '/' + name)


# ---- The rain streak.
m = material('M_ShipRain', unreal.BlendMode.BLEND_TRANSLUCENT, unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
lp = local_pos(m)
op = node(m, unreal.MaterialExpressionScalarParameter, parameter_name='Opacity', default_value=.6)
rnd = node(m, unreal.MaterialExpressionPerInstanceRandom)
shape = custom(m, r'''
float across = saturate(1 - abs(P.y) / 50);
float along = saturate(1 - abs(P.x) / 50);
float a = across * across * smoothstep(0, .55, along);
return a * O * lerp(.55, 1.15, R);
''', unreal.CustomMaterialOutputType.CMOT_FLOAT1, {'P': lp, 'O': op, 'R': rnd})
prop(m, shape, unreal.MaterialProperty.MP_OPACITY)
col = node(m, unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(.85, .88, .9, 1))
prop(m, col, unreal.MaterialProperty.MP_BASE_COLOR)
# (A little light of its own, as falling water catches the sky's light from every side; a lightning flash lights it.)
glow = node(m, unreal.MaterialExpressionScalarParameter, parameter_name='Glow', default_value=.04)
prop(m, glow, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
prop(m, node(m, unreal.MaterialExpressionConstant, r=.15), unreal.MaterialProperty.MP_ROUGHNESS)
save(m, 'M_ShipRain')

# ---- Spray and splashes.
m = material('M_ShipSpray', unreal.BlendMode.BLEND_TRANSLUCENT, unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
lp = local_pos(m)
fade = node(m, unreal.MaterialExpressionPerInstanceCustomData, data_index=0, const_default_value=.5)
rnd = node(m, unreal.MaterialExpressionPerInstanceRandom)
tm = node(m, unreal.MaterialExpressionTime)
blob = custom(m, r'''
float2 q = P.xy / 50;
float r = length(q);
struct N { float h(float2 p) { return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453); }
  float v(float2 p) { float2 i = floor(p), f = frac(p); f = f * f * (3 - 2 * f);
    return lerp(lerp(h(i), h(i + float2(1, 0)), f.x), lerp(h(i + float2(0, 1)), h(i + 1), f.x), f.y); } } n;
float2 o = float2(R * 37.0, R * 91.0) + Tm * .35;
float br = n.v(q * 3 + o) * .65 + n.v(q * 7 - o) * .35;
float soft = saturate(1 - r);
float fine = n.v(q * 16 + o * 2) * .5 + .5;
return saturate(soft * soft * (.25 + br) * 1.5 * fine - .1) * F;
''', unreal.CustomMaterialOutputType.CMOT_FLOAT1, {'P': lp, 'F': fade, 'R': rnd, 'Tm': tm})
depth = node(m, unreal.MaterialExpressionDepthFade, fade_distance_default=40.)
link(blob, depth, 'Opacity')
prop(m, depth, unreal.MaterialProperty.MP_OPACITY)
prop(m, node(m, unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(.92, .95, .97, 1)), unreal.MaterialProperty.MP_BASE_COLOR)
prop(m, node(m, unreal.MaterialExpressionConstant, r=.4), unreal.MaterialProperty.MP_ROUGHNESS)
# (White water scatters light: a little of its own, so it never reads as smoke in the storm's gloom.)
prop(m, node(m, unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(.07, .075, .08, 1)), unreal.MaterialProperty.MP_EMISSIVE_COLOR)
save(m, 'M_ShipSpray')

# ---- The lightning channel.
m = material('M_ShipBolt', unreal.BlendMode.BLEND_ADDITIVE, unreal.MaterialShadingModel.MSM_UNLIT)
m.set_editor_property('use_translucency_vertex_fog', False)   # (the storm's haze would swallow it: it must cut through)
uv = node(m, unreal.MaterialExpressionTextureCoordinate)
inten = node(m, unreal.MaterialExpressionScalarParameter, parameter_name='Intensity', default_value=1.)
glow = custom(m, r'''
float x = 1 - abs(UV.x * 2 - 1);
float core = pow(x, 6) * 60 + x * x * 6;
return float3(.82, .86, 1.0) * core * I;
''', unreal.CustomMaterialOutputType.CMOT_FLOAT3, {'UV': uv, 'I': inten})
prop(m, glow, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
save(m, 'M_ShipBolt')

# ---- (2026-11-04) Spume: billows of white water thrown up where a sea breaks aboard or on the bow. Drawn from Unity Labs'
# CC0 smoke flipbook (WispySmoke01 8x8, already imported for the temple's jets as T_JetSmoke by
# Tools/import_temple_fire.py): its density as white water, lit by the storm's light. Custom data 0 the frame, 2 the opacity.
smoke = unreal.load_asset('/Game/Art/ChultTemple/Fire/T_JetSmoke')
if not smoke:
    raise RuntimeError('T_JetSmoke missing: run Tools/import_temple_fire.py first')
m = material('M_ShipSpume', unreal.BlendMode.BLEND_TRANSLUCENT, unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
tex = node(m, unreal.MaterialExpressionTextureObject, texture=smoke)
uv = node(m, unreal.MaterialExpressionTextureCoordinate)
frame = node(m, unreal.MaterialExpressionPerInstanceCustomData, data_index=0)
opac = node(m, unreal.MaterialExpressionPerInstanceCustomData, data_index=2, const_default_value=.5)
soft = node(m, unreal.MaterialExpressionDepthFade, fade_distance_default=60.)
depth = node(m, unreal.MaterialExpressionPixelDepth)
book = custom(m, r'''
float n = 64.;
float f = fmod(max(Frame, 0.), n), f0 = floor(f), b = f - f0, f1 = fmod(f0 + 1., n);
float2 c0 = float2(fmod(f0, 8.), floor(f0 / 8.)), c1 = float2(fmod(f1, 8.), floor(f1 / 8.));
float2 uv = saturate(UV);
float4 s = lerp(Texture2DSample(Tex, TexSampler, (uv + c0) / 8.), Texture2DSample(Tex, TexSampler, (uv + c1) / 8.), b);
float fade = saturate(Opacity) * Soft * saturate((Depth - 30.) / 90.);
return float4(lerp(.55, .82, saturate(s.r * 1.4)).xxx, saturate(s.a * .95) * fade);
''', unreal.CustomMaterialOutputType.CMOT_FLOAT4, {'Tex': tex, 'UV': uv, 'Frame': frame, 'Opacity': opac, 'Soft': soft, 'Depth': depth})
rgb = node(m, unreal.MaterialExpressionComponentMask, r=True, g=True, b=True, a=False); link(book, rgb)
alpha = node(m, unreal.MaterialExpressionComponentMask, r=False, g=False, b=False, a=True); link(book, alpha)
prop(m, rgb, unreal.MaterialProperty.MP_BASE_COLOR)
prop(m, alpha, unreal.MaterialProperty.MP_OPACITY)
glow = node(m, unreal.MaterialExpressionMultiply); link(rgb, glow, 'A'); link(node(m, unreal.MaterialExpressionConstant, r=.02), glow, 'B')
prop(m, glow, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
prop(m, node(m, unreal.MaterialExpressionConstant, r=.5), unreal.MaterialProperty.MP_ROUGHNESS)
save(m, 'M_ShipSpume')

# ---- (2026-11-04) The sea washing across the deck (ShipStorm.cpp's sheet): lit translucent water over the planking, its
# vertex colour R the foam (the front, the churn by the rail), G how much water (opacity); the foam broken up by a noise
# that runs with the water, the clear water dark green-grey and glossy.
m = material('M_ShipDeckWash', unreal.BlendMode.BLEND_TRANSLUCENT, unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
m.set_editor_property('two_sided', False)
m.set_editor_property('used_with_instanced_static_meshes', False)
m.set_editor_property('translucency_lighting_mode', unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
water_normal = unreal.load_asset('/Game/Art/Textures/Water/T_Water_TilingNormal_Waves_02')
water_foam = unreal.load_asset('/Game/Art/Textures/Water/T_WaterFlow_01_Foam_Tiled')
if not water_normal or not water_foam:
    raise RuntimeError('the sea textures are missing (Tools/create_ship_sea.py uses them)')
vc = node(m, unreal.MaterialExpressionVertexColor)
wp = node(m, unreal.MaterialExpressionWorldPosition)
tm = node(m, unreal.MaterialExpressionTime)
# (The water runs across the deck toward port, +Y: its ripples and foam run with it.)
uv_n = custom(m, 'return W.xy / 140. + float2(.03, -.55) * Tm;', unreal.CustomMaterialOutputType.CMOT_FLOAT2, {'W': wp, 'Tm': tm})
uv_f = custom(m, 'return W.xy / 95. + float2(-.02, -.8) * Tm;', unreal.CustomMaterialOutputType.CMOT_FLOAT2, {'W': wp, 'Tm': tm})
nrm = node(m, unreal.MaterialExpressionTextureSample, texture=water_normal, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
link(uv_n, nrm, 'UVs')
fsm = node(m, unreal.MaterialExpressionTextureSample, texture=water_foam)
link(uv_f, fsm, 'UVs')
wash = custom(m, r'''
float tex = dot(F.rgb, float3(.33, .33, .34));
float foam = saturate(C.r * 1.25 - .25 + (tex - .5) * 1.1) * saturate(C.r * 2.5);
foam = smoothstep(.2, .7, foam);
float3 col = lerp(float3(.02, .034, .033), float3(.62, .66, .67), foam);
float a = saturate(C.g) * lerp(.3, .85, foam);
return float4(col, a);
''', unreal.CustomMaterialOutputType.CMOT_FLOAT4, {'C': vc, 'F': fsm})
flat = custom(m, 'float3 n = N.xyz * float3(.55, .55, 1); return normalize(lerp(float3(0, 0, 1), n, saturate(C.g * 1.5)));', unreal.CustomMaterialOutputType.CMOT_FLOAT3, {'N': nrm, 'C': vc})
rgb = node(m, unreal.MaterialExpressionComponentMask, r=True, g=True, b=True, a=False); link(wash, rgb)
alpha = node(m, unreal.MaterialExpressionComponentMask, r=False, g=False, b=False, a=True); link(wash, alpha)
prop(m, rgb, unreal.MaterialProperty.MP_BASE_COLOR)
prop(m, alpha, unreal.MaterialProperty.MP_OPACITY)
prop(m, flat, unreal.MaterialProperty.MP_NORMAL)
prop(m, node(m, unreal.MaterialExpressionConstant, r=.3), unreal.MaterialProperty.MP_ROUGHNESS)
prop(m, node(m, unreal.MaterialExpressionConstant, r=.25), unreal.MaterialProperty.MP_SPECULAR)
save(m, 'M_ShipDeckWash')
