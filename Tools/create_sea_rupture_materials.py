"""Materials for the ruptures in the sea in the fore topsail cutscene (Claude, 2026-11-09; ShipEndingScene.cpp).

UnrealEditor-Cmd <uproject> -ExecutePythonScript=Tools/create_sea_rupture_materials.py -unattended -nullrhi -nosplash -NoLiveCoding

Each rupture is a flat ragged disc on the sea, but its material traces the view ray down a well under it, so it reads
as a hole: the sea pouring over its lip and down its sides, and at the bottom the plane it opens onto.
  M_SeaRuptureAstral      the Astral: a silver-violet nebula turning far below, with stars; the falling water lit violet
  M_SeaRupturePhlegethos  Phlegethos: cracked basalt over molten rock far below; the falling water flashing to steam
  M_SeaRuptureFoam        translucent: the white water drawn round and into a rupture over the sea about its lip
Mesh contract (ShipEndingScene.cpp BuildRuptures): UV0 = offset from the centre / (radius * (1.25, .85)), so the well's
wall is the unit circle; UV1 = (radius cm, open 0-1); UV2 = (seed, 0). The foam ring: UV0 = (angle 0-1, 0 at the lip
to 1 outside), UV1 = (kind: 0 Astral, 1 Phlegethos, open).
"""
import unreal

LIB = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
DEST = '/Game/Art/Materials'

NOISE = r'''
struct FN {
  float h(float2 p) { return frac(sin(dot(p, float2(127.1, 311.7))) * 43758.5453); }
  float n(float2 p) { float2 i = floor(p), f = frac(p); f = f * f * (3. - 2. * f);
    return lerp(lerp(h(i), h(i + float2(1, 0)), f.x), lerp(h(i + float2(0, 1)), h(i + 1.), f.x), f.y); }
  float fbm(float2 p) { float s = 0., a = .5; for (int k = 0; k < 5; k++) { s += a * n(p); p = p * 2.03 + 17.1; a *= .5; } return s; }
  float2 vor(float2 p) { float2 i = floor(p), f = frac(p); float d1 = 8., d2 = 8.;
    for (int y = -1; y <= 1; y++) for (int x = -1; x <= 1; x++) {
      float2 g = float2(x, y), o = float2(h(i + g), h(i + g + 31.7));
      float d = length(g + o - f); if (d < d1) { d2 = d1; d1 = d; } else if (d < d2) d2 = d; }
    return float2(d1, d2); }
};
FN z;
'''

# The well: shared by both holes. Sets Hit (0 wall, 1 floor), Zw (depth hit, in radii), Q (where, in the unit circle).
WELL = r'''
float R = max(Info.x, 1.), Open = saturate(Info.y), Seed = Extra.x;
float3 d = -normalize(Dir); d.z = min(d.z, -.06);
float3 dv = float3(d.x / (1.25 * R), d.y / (.85 * R), d.z / R);
float2 p0 = UV;
float A = dot(dv.xy, dv.xy) + 1e-7, B = 2. * dot(p0, dv.xy), C = dot(p0, p0) - 1.;
float tw = C >= 0. ? 0. : (-B + sqrt(max(B * B - 4. * A * C, 0.))) / (2. * A);
float H = DEPTH * (.35 + .65 * Open);
float tf = H / -dv.z;
float Hit = tw < tf ? 0. : 1.;
float t = min(tw, tf);
float Zw = t * -dv.z;
float2 Q = p0 + t * dv.xy;
float ang = atan2(Q.y, Q.x);
// The sea falling down the wall: streaks running down, white at the lip.
float2 wc = float2(ang * 7.64 + Seed, Zw * 2.2 - T * 1.7);
float streak = z.fbm(float2(wc.x * 2.6, wc.y * .35)) ;
float foam = saturate(1. - Zw / .06) * smoothstep(.45, .8, z.fbm(wc * 3.)) * .8;
float3 water = lerp(float3(.006, .011, .015), float3(.035, .05, .06), smoothstep(.35, .75, streak));
water = lerp(water, float3(.3, .33, .36), saturate(foam));
// Beyond the lip (its ragged edge), still the sea, streaked with foam running in.
float outside = step(0., C);
float2 sc0 = float2(atan2(p0.y, p0.x) * 9. + Seed, length(p0) * 6. + T * .8);
water = lerp(water, lerp(float3(.014, .02, .024), float3(.22, .24, .26), smoothstep(.55, .85, z.fbm(sc0))), outside);
float below = exp(-(H - Zw) * GLOWFALL);
'''

ASTRAL = NOISE + 'float DEPTH = 1.7; float GLOWFALL = 2.2;' + WELL + r'''
float3 glow = float3(.42, .2, 1.);
float3 wall = water + glow * (.12 + below) * (.3 + .7 * streak) * 1.2 * (1. - outside);
// The Astral far below: a silver-violet nebula turning slowly, stars, light pooling at its heart.
float r = length(Q), th = atan2(Q.y, Q.x) + T * .045 + r * 1.3;
float2 sw = float2(cos(th), sin(th)) * r * 2.2 + Seed * 3.1;
float neb = z.fbm(sw + float2(T * .03, 0.)), wisp = z.fbm(sw * 2.7 - T * .05);
float3 sky = lerp(float3(.008, .002, .03), float3(.22, .08, .5), smoothstep(.3, .75, neb));
sky = lerp(sky, float3(.78, .74, .95), pow(saturate(wisp * neb * 1.9 - .35), 2.) * 1.6);
float2 sc = Q * 34. + Seed * 11.; float star = z.h(floor(sc));
float twinkle = .6 + .4 * sin(T * 3. + star * 40.);
sky += float3(.9, .88, 1.) * step(.975, star) * pow(saturate(1. - length(frac(sc) - .5) * 2.6), 3.) * 4. * twinkle;
sky += glow * exp(-r * r * 2.) * .9;
sky *= 1.2 + .25 * sin(T * 1.1 + Seed);
float3 col = Hit > .5 ? sky : wall;
// The oil film's sheen on the water as it goes over the lip.
float3 film = .5 + .5 * cos(ang * 3. + Zw * 9. - T * .6 + float3(0, 2.1, 4.2));
col += film * float3(.25, .2, .35) * saturate(1. - Zw / .1) * (1. - Hit) * (1. - outside) * .3;
return col * (.15 + .85 * Open);
'''

PHLEGETHOS = NOISE + 'float DEPTH = 1.5; float GLOWFALL = 2.4;' + WELL + r'''
float3 fire = float3(1., .34, .06);
float steam = smoothstep(.45 * H, .95 * H, Zw) * (.45 + .7 * z.fbm(float2(ang * 3. + Seed, Zw * 1.4 + T * .9)));
float3 wall = water + fire * (.05 + below) * (.15 + .85 * streak * streak) * .8 * (1. - outside);
wall = lerp(wall, float3(.55, .2, .07) * (.25 + .9 * below), saturate(steam) * .3 * (1. - outside));
// Phlegethos far below: plates of cracked basalt over molten rock, the cracks pulsing, a lake of it at the heart.
float2 lp = Q * 3.4 + Seed * 5.;
lp += (float2(z.fbm(lp * .7), z.fbm(lp * .7 + 7.3)) - .5) * 1.1;   // (irregular plates)
float2 v = z.vor(lp);
float crack = smoothstep(.09, .0, v.y - v.x);
float2 v2 = z.vor(lp * 2.3 + 9.);
float fine = smoothstep(.07, .0, v2.y - v2.x);
float pulse = .75 + .25 * sin(T * 2.1 + z.h(floor(lp)) * 30.);
float pool = smoothstep(.4, .12, length(Q + .15 * float2(sin(T * .2), cos(T * .17))) + .25 * z.fbm(Q * 3. + T * .1));
float3 rock = float3(.006, .003, .0025) * (.6 + .8 * z.fbm(lp * 2.));
float3 molten = lerp(float3(1.1, .14, .01), float3(2.2, .6, .07), z.fbm(lp * 1.5 + T * .3));
float3 ground = lerp(rock, molten * pulse, saturate(crack + fine * .3 + pool));
ground += float3(.5, .08, .01) * .035 * (1. - pool);   // (its heat haze, lit from the cracks)
float3 col = Hit > .5 ? ground : wall;
return col * (.15 + .85 * Open);
'''

FOAM = NOISE + r'''
float kind = Info.x, open = saturate(Info.y);
float y = UV.y, a = UV.x * 6.2832;
// Drawn round and in toward the lip.
float2 p = float2(UV.x * 26. + y * 3. + T * .35, y * 3.4 + T * .55);
float f = z.fbm(p) * .7 + z.fbm(p * 2.7 + 5.) * .5;
float foam = smoothstep(.45, .95, f);
float edge = pow(saturate(1. - y), 1.6);
float3 glow = lerp(float3(.42, .2, 1.), float3(1., .34, .06), kind);
float3 col = float3(.2, .22, .24) * (.5 + .5 * foam) + glow * edge * edge * edge * (.25 + .35 * kind);
float alpha = saturate(foam * .7 + edge * .15) * edge * open;
return float4(col, alpha);
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


def node(mat, cls, x=0, y=0):
    return LIB.create_material_expression(mat, cls, x, y)


def custom(mat, code, out_type, inputs):
    c = node(mat, unreal.MaterialExpressionCustom, -300, 0)
    items = []
    for key in inputs:
        item = unreal.CustomInput()
        item.set_editor_property('input_name', key)
        items.append(item)
    c.set_editor_property('inputs', items)
    c.set_editor_property('output_type', out_type)
    c.set_editor_property('code', code)
    return c


def texcoord(mat, index):
    t = node(mat, unreal.MaterialExpressionTextureCoordinate, -700, 100 * index)
    t.set_editor_property('coordinate_index', index)
    return t


def hole(name, code):
    mat = material(name)
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property('two_sided', True)
    c = custom(mat, code, unreal.CustomMaterialOutputType.CMOT_FLOAT3, ('UV', 'Info', 'Extra', 'Dir', 'T'))
    LIB.connect_material_expressions(texcoord(mat, 0), '', c, 'UV')
    LIB.connect_material_expressions(texcoord(mat, 1), '', c, 'Info')
    LIB.connect_material_expressions(texcoord(mat, 2), '', c, 'Extra')
    LIB.connect_material_expressions(node(mat, unreal.MaterialExpressionCameraVectorWS, -700, 400), '', c, 'Dir')
    LIB.connect_material_expressions(node(mat, unreal.MaterialExpressionTime, -700, 500), '', c, 'T')
    LIB.connect_material_property(c, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    return mat


def foam():
    mat = material('M_SeaRuptureFoam')
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property('two_sided', True)
    c = custom(mat, FOAM, unreal.CustomMaterialOutputType.CMOT_FLOAT4, ('UV', 'Info', 'T'))
    LIB.connect_material_expressions(texcoord(mat, 0), '', c, 'UV')
    LIB.connect_material_expressions(texcoord(mat, 1), '', c, 'Info')
    LIB.connect_material_expressions(node(mat, unreal.MaterialExpressionTime, -700, 500), '', c, 'T')
    rgb = node(mat, unreal.MaterialExpressionComponentMask, 0, 0)
    alpha = node(mat, unreal.MaterialExpressionComponentMask, 0, 200)
    for ch in ('r', 'g', 'b', 'a'):
        rgb.set_editor_property(ch, ch != 'a')
        alpha.set_editor_property(ch, ch == 'a')
    LIB.connect_material_expressions(c, '', rgb, '')
    LIB.connect_material_expressions(c, '', alpha, '')
    LIB.connect_material_property(rgb, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    LIB.connect_material_property(alpha, '', unreal.MaterialProperty.MP_OPACITY)
    return mat


for mat in (hole('M_SeaRuptureAstral', ASTRAL), hole('M_SeaRupturePhlegethos', PHLEGETHOS), foam()):
    LIB.recompile_material(mat)
    if not unreal.EditorAssetLibrary.save_loaded_asset(mat, only_if_is_dirty=False):
        raise RuntimeError(mat.get_path_name())
    unreal.log(f'CHUCK_SEA_RUPTURE_MATERIAL {mat.get_name()}')
unreal.log('CHUCK_SEA_RUPTURE_MATERIALS_READY')
