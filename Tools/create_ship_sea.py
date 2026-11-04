"""M_ShipSea and M_ShipSail: the open sea round the pirate ship, and its sails' canvas (at the end) (Claude, 2026-11-02; docs/SHIP.md, milestone 2).

UnrealEditor-Cmd <uproject> -run=pythonscript -script=Tools/create_ship_sea.py -unattended -nullrhi -nosplash

Drawn on the ship's sea mesh (ShipSea.inl): rings round the hull's waterline out to the horizon, in the ship's frame
(the component sits at the ship's origin, so Local Position is the ship frame). Per vertex:
  UV1 = (distance out from the hull's waterline in cm, or -100 for the curtain hidden inside the planking;
         the hull's flare there, d(half-breadth)/dZ),
  UV2 = the outward normal of the waterline there (x, y).
Its parameters: Tilt (rg: dZ/dX and dZ/dY of the sea's plane, the rocking round the still hull, set every frame),
Speed (cm/s the sea runs aft past her: she makes way) and Storm (0..1, 2026-11-03: the storm's swell, darker water,
more foam, spume streaks; ShipStorm.cpp).

- World position offset: a sum of travelling deep-water waves from about one wind (42 m down to 4.5 m; crests a little
  sharper than a sine), a little lower against the hull, gone by 60 m out (beyond that the tilt alone, which is linear and
  exact on the coarse far rings); plus the tilt. The ring on the hull moves in or out with the hull's flare as the water
  rises and falls, so the sea always meets the planking (a hidden curtain inside the planking closes any gap below).
- The surface: the same waves' slope for the normal, with two scales of the UE Water tiling normal over it (fading
  out with distance); deep blue-green, lighter through the crests; foam against the hull, a bow wave, the wake astern
  and a little on the steepest crests (the UE Water foam texture). Opaque and lit: the sky and sun reflect in it.
Creates or rebuilds only this asset in /Game/Art/Ship; imports nothing.
"""
import unreal

L = unreal.MaterialEditingLibrary
A = unreal.EditorAssetLibrary
T = unreal.AssetToolsHelpers.get_asset_tools()
DEST, NAME = '/Game/Art/Ship', 'M_ShipSea'
tex = {n: unreal.load_asset('/Game/Art/Textures/Water/' + n) for n in ('T_Water_TilingNormal_Waves_02', 'T_WaterFlow_01_Foam_Tiled')}
if not all(tex.values()):
    raise RuntimeError('Existing Water textures required in /Game/Art/Textures/Water')
if not A.does_directory_exist(DEST):
    A.make_directory(DEST)
m = unreal.load_asset(DEST + '/' + NAME) or T.create_asset(NAME, DEST, unreal.Material, unreal.MaterialFactoryNew())
L.delete_all_material_expressions(m)
m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
m.set_editor_property('two_sided', False)
m.set_editor_property('tangent_space_normal', False)


def node(cls, **props):
    n = L.create_material_expression(m, cls)
    for k, v in props.items():
        n.set_editor_property(k, v)
    return n


def link(a, b, pin='', out=''):
    if not L.connect_material_expressions(a, out, b, pin):
        raise RuntimeError('Link failed %s -> %s' % (out, pin))


def prop(n, p, out=''):
    if not L.connect_material_property(n, out, p):
        raise RuntimeError('Property link failed ' + str(p))


def const(x):
    return node(unreal.MaterialExpressionConstant, r=x)


def vec(x, y, z):
    return node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(x, y, z, 1))


def op(cls, a, b):
    n = node(cls); link(a, n, 'A'); link(b, n, 'B'); return n


def mask(src, r, g, b, a=False, out=''):
    n = node(unreal.MaterialExpressionComponentMask, r=r, g=g, b=b, a=a); link(src, n, '', out); return n


def custom(code, out_type, names):
    n = node(unreal.MaterialExpressionCustom, output_type=out_type, code=code)
    ins = []
    for key in names:
        i = unreal.CustomInput(); i.set_editor_property('input_name', key); ins.append(i)
    n.set_editor_property('inputs', ins)
    return n


pos = node(unreal.MaterialExpressionLocalPosition)
try:
    pos.set_editor_property('included_offsets', unreal.PositionIncludedOffsets.EXCLUDE_OFFSETS)
except Exception as e:  # (older names)
    raise RuntimeError('Local Position must exclude the shader offsets: %s' % e)
clock = node(unreal.MaterialExpressionTime)
uv1 = node(unreal.MaterialExpressionTextureCoordinate, coordinate_index=1)
uv2 = node(unreal.MaterialExpressionTextureCoordinate, coordinate_index=2)
tilt = node(unreal.MaterialExpressionVectorParameter, parameter_name='Tilt', default_value=unreal.LinearColor(0, 0, 0, 0))
tilt2 = mask(tilt, True, True, False)
speed = node(unreal.MaterialExpressionScalarParameter, parameter_name='Speed', default_value=110.0)
# The storm (2026-11-03, ShipStorm.cpp sets it 0..1): a long storm swell under a heavier wind sea, darker water, more foam
# on the crests and spume streaked down the wind.
storm = node(unreal.MaterialExpressionScalarParameter, parameter_name='Storm', default_value=0.0)

# The waves (shared by both nodes): wavelength cm, amplitude cm, heading (degrees from +X, the way they run).
WAVES = r'''
const float4 W[6] = {float4(4200,26,10,0), float4(2600,15,-25,1.7), float4(1700,10,35,3.1), float4(1100,6,-5,4.4), float4(700,3.5,55,5.2), float4(450,2,-40,0.6)};
// The storm's swell, running down the wind (from starboard to port).
const float4 SW[3] = {float4(9000,58,95,.9), float4(5600,36,72,2.3), float4(3300,22,112,4.0)};
float2 p = P.xy + float2(S*T, 0);
float dh = D.x;   // (not 'near': reserved in HLSL)
float scale = lerp(.6, 1, smoothstep(0, 400, dh)) * (1 - smoothstep(3500, 6000, dh));
// The swell keeps its shape further out (the rings are 2 m apart to 150 m), lower against the hull; its slope shades the
// sea out to 800 m.
float sscale = lerp(.45, 1, smoothstep(0, 700, dh)) * (1 - smoothstep(9000, 14000, dh));
float gscale = lerp(.45, 1, smoothstep(0, 700, dh)) * (1 - smoothstep(9000, 80000, dh));
float wamp = 1 + .9 * St;
float h = 0; float2 g = 0; float hmax = 0;
for (int i = 0; i < 6; i++)
{
  float a = radians(W[i].z); float2 dir = float2(cos(a), sin(a));
  float k = 6.28318 / W[i].x, w = sqrt(980 * k);
  float ph = dot(dir, p) * k - w * T + W[i].w;
  float c = sin(ph), q = cos(ph);
  float amp = W[i].y * wamp;
  h += amp * (c + .2 * c * c - .1);
  g += amp * k * q * (1 + .4 * c) * dir;
  hmax += amp * 1.2;
}
h *= scale; g *= scale; hmax *= scale;
float hs = 0; float2 gs = 0; float hsmax = 0;
for (int j = 0; j < 3; j++)
{
  float a = radians(SW[j].z); float2 dir = float2(cos(a), sin(a));
  float k = 6.28318 / SW[j].x, w = sqrt(980 * k);
  float ph = dot(dir, p) * k - w * T + SW[j].w;
  float c = sin(ph), q = cos(ph);
  float amp = SW[j].y * St;
  hs += amp * (c + .25 * c * c - .12);
  gs += amp * k * q * (1 + .5 * c) * dir;
  hsmax += amp * 1.25;
}
h += hs * sscale; g += gs * gscale; hmax += hsmax * sscale;
'''
wpo = custom(r'''
if (D.x < -50) return float3(0, 0, 0);   // the curtain inside the planking stays put
''' + WAVES + r'''
float z = h + dot(P.xy, G);
// The ring on the hull follows its flare as the water rises and falls (D.y: dO/dZ), easing out over the first 60 cm.
float follow = saturate(1 - dh / 60); follow *= follow;
return float3(Nh * D.y * z * follow, z);
''', unreal.CustomMaterialOutputType.CMOT_FLOAT3, ('P', 'T', 'D', 'Nh', 'G', 'S', 'St'))
for key, src in (('P', pos), ('T', clock), ('D', uv1), ('Nh', uv2), ('G', tilt2), ('S', speed), ('St', storm)):
    link(src, wpo, key)
prop(wpo, unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)

surf = custom(WAVES + r'''
float crest = saturate(h / max(hmax, 1) * .5 + .5);
// Foam: against the hull, the bow wave forward, the wake astern, the steepest crests.
float hullFoam = pow(saturate(1 - dh / 75), 2) * .5;
float bow = smoothstep(1350, 1720, P.x) * pow(saturate(1 - dh / 140), 2) * .55;
float aft = -1612 - P.x;
float wake = aft > 0 ? pow(saturate(1 - abs(P.y) / (170 + aft * .1)), 2) * saturate(1 - aft / 2400) * .45 : 0;
float crestFoam = smoothstep(lerp(.86, .62, St), .99, crest) * smoothstep(.0, .06, length(g)) * lerp(.35, .85, St);
// Spume: streaks blown down the wind (toward +Y), long along it, narrow across.
float2 sq = float2(p.y / 1900, p.x / 150);
float2 si = floor(sq), sf = frac(sq); sf = sf * sf * (3 - 2 * sf);
float4 hsh = frac(sin(float4(dot(si, float2(127.1, 311.7)), dot(si + float2(1, 0), float2(127.1, 311.7)), dot(si + float2(0, 1), float2(127.1, 311.7)), dot(si + 1, float2(127.1, 311.7)))) * 43758.5453);
float sn = lerp(lerp(hsh.x, hsh.y, sf.x), lerp(hsh.z, hsh.w, sf.x), sf.y);
float spume = smoothstep(.62, .92, sn) * St * .6 * (1 - smoothstep(20000, 90000, dh));
float foam = saturate(max(max(max(hullFoam * (1 + St), bow), max(wake, crestFoam)), spume));
return float4(g + G, crest, foam);
''', unreal.CustomMaterialOutputType.CMOT_FLOAT4, ('P', 'T', 'D', 'S', 'G', 'St'))
for key, src in (('P', pos), ('T', clock), ('D', uv1), ('S', speed), ('G', tilt2), ('St', storm)):
    link(src, surf, key)
slope = mask(surf, True, True, False)
crest = mask(surf, False, False, True)
foam_mask = node(unreal.MaterialExpressionComponentMask, r=False, g=False, b=False, a=True); link(surf, foam_mask)

# Sea-frame coordinates that run aft with her way, for the textures.
xy = mask(pos, True, True, False)
way = node(unreal.MaterialExpressionAppendVector); link(op(unreal.MaterialExpressionMultiply, speed, clock), way, 'A'); link(const(0.0), way, 'B')
sea_xy = op(unreal.MaterialExpressionAdd, xy, way)


def sample(name, uv, normal):
    n = node(unreal.MaterialExpressionTextureSample, texture=tex[name],
             sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if normal else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    link(uv, n, 'UVs'); return n


def panned(size, sx, sy):
    n = node(unreal.MaterialExpressionPanner, speed_x=sx, speed_y=sy)
    link(op(unreal.MaterialExpressionMultiply, sea_xy, const(1.0 / size)), n, 'Coordinate'); return n


d1 = sample('T_Water_TilingNormal_Waves_02', panned(950, .012, .006), True)
d2 = sample('T_Water_TilingNormal_Waves_02', panned(2400, -.006, .009), True)
detail = mask(op(unreal.MaterialExpressionAdd, d1, d2), True, True, False)
near = mask(uv1, True, False, False)
fade = node(unreal.MaterialExpressionOneMinus); sm = node(unreal.MaterialExpressionSmoothStep)
link(near, sm, 'Value'); link(const(1000.0), sm, 'Min'); link(const(8000.0), sm, 'Max'); link(sm, fade)
detail = op(unreal.MaterialExpressionMultiply, detail, op(unreal.MaterialExpressionMultiply, fade, const(.32)))
storm_detail = node(unreal.MaterialExpressionLinearInterpolate); link(const(1.0), storm_detail, 'A'); link(const(1.8), storm_detail, 'B'); link(storm, storm_detail, 'Alpha')
detail = op(unreal.MaterialExpressionMultiply, detail, storm_detail)
grad = op(unreal.MaterialExpressionSubtract, op(unreal.MaterialExpressionMultiply, slope, const(-1.0)), detail)   # -slope + detail
nrm = node(unreal.MaterialExpressionAppendVector); link(grad, nrm, 'A'); link(const(1.0), nrm, 'B')
nrm_n = node(unreal.MaterialExpressionNormalize); link(nrm, nrm_n)
prop(nrm_n, unreal.MaterialProperty.MP_NORMAL)

foam_tex = mask(sample('T_WaterFlow_01_Foam_Tiled', panned(260, .02, .011), False), True, False, False)
foam_tex2 = mask(sample('T_WaterFlow_01_Foam_Tiled', panned(610, -.013, .007), False), True, False, False)
foam = node(unreal.MaterialExpressionSaturate)
# (The product of the two scales breaks the foam into patches rather than a band.)
link(op(unreal.MaterialExpressionMultiply, op(unreal.MaterialExpressionMultiply, foam_mask, op(unreal.MaterialExpressionMultiply, foam_tex, foam_tex2)), const(3.0)), foam)

crest2 = op(unreal.MaterialExpressionMultiply, crest, crest)
def by_storm(calm, rough):
    n = node(unreal.MaterialExpressionLinearInterpolate); link(calm, n, 'A'); link(rough, n, 'B'); link(storm, n, 'Alpha'); return n


# (In the storm: slate grey-green, the crests a dull green.)
body = node(unreal.MaterialExpressionLinearInterpolate)
link(by_storm(vec(.004, .022, .036), vec(.006, .013, .016)), body, 'A'); link(by_storm(vec(.012, .07, .078), vec(.02, .045, .045)), body, 'B'); link(crest2, body, 'Alpha')
col = node(unreal.MaterialExpressionLinearInterpolate); link(body, col, 'A'); link(vec(.62, .68, .7), col, 'B'); link(foam, col, 'Alpha')
prop(col, unreal.MaterialProperty.MP_BASE_COLOR)
far_rough = node(unreal.MaterialExpressionSmoothStep); link(near, far_rough, 'Value'); link(const(1500.0), far_rough, 'Min'); link(const(40000.0), far_rough, 'Max')
# (Far out, only a little rougher: a rough lobe there reaches the sky light's black lower half and darkens the horizon.)
rough = node(unreal.MaterialExpressionLinearInterpolate); link(by_storm(const(.06), const(.13)), rough, 'A'); link(const(.11), rough, 'B'); link(far_rough, rough, 'Alpha')
rough2 = node(unreal.MaterialExpressionLinearInterpolate); link(rough, rough2, 'A'); link(const(.6), rough2, 'B'); link(foam, rough2, 'Alpha')
prop(rough2, unreal.MaterialProperty.MP_ROUGHNESS)
prop(const(.5), unreal.MaterialProperty.MP_SPECULAR)

L.recompile_material(m)
if not A.save_loaded_asset(m, only_if_is_dirty=False):
    raise RuntimeError('Save failed ' + NAME)
unreal.log('CHUCK_SHIP_SEA_CREATED ' + DEST + '/' + NAME)

# ---- M_ShipSail: the sails' canvas (ShipSetting.cpp's sail sheets, two faces). Two-sided foliage shading, so the sun
# shows through a sail seen from its shaded side as it does through thin cloth; cloth panels seamed every 62 cm across
# (UV0 is in 150 cm units), a reinforced band every 2.5 m down.
SAIL = 'M_ShipSail'
m = unreal.load_asset(DEST + '/' + SAIL) or T.create_asset(SAIL, DEST, unreal.Material, unreal.MaterialFactoryNew())
L.delete_all_material_expressions(m)
m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
m.set_editor_property('two_sided', False)
m.set_editor_property('tangent_space_normal', True)
uv = node(unreal.MaterialExpressionTextureCoordinate, coordinate_index=0)
cloth = custom(r'''
float across = frac(UV.x * 150 / 62), down = frac(UV.y * 150 / 250);
float seam = 1 - .16 * (1 - smoothstep(0, .05, across));
seam *= 1 - .12 * saturate(1 - abs(down - .5) * 40);
float weave = .96 + .04 * frac(sin(dot(floor(UV * 150 / 9), float2(12.9898, 78.233))) * 43758.5453);
return float3(.7, .64, .53) * seam * weave;
''', unreal.CustomMaterialOutputType.CMOT_FLOAT3, ('UV',))
link(uv, cloth, 'UV')
prop(cloth, unreal.MaterialProperty.MP_BASE_COLOR)
prop(vec(.55, .47, .34), unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
prop(const(.88), unreal.MaterialProperty.MP_ROUGHNESS)
prop(const(.25), unreal.MaterialProperty.MP_SPECULAR)
L.recompile_material(m)
if not A.save_loaded_asset(m, only_if_is_dirty=False):
    raise RuntimeError('Save failed ' + SAIL)
unreal.log('CHUCK_SHIP_SAIL_CREATED ' + DEST + '/' + SAIL)
