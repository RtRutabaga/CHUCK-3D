"""The beholder's attack effects (Claude, 2026-10-25; docs/CHULT-TEMPLE-BEHOLDER.md#the-fight).

UnrealEditor-Cmd <uproject> -ExecutePythonScript=Tools/create_beholder_fx.py -unattended -nosplash -NoLiveCoding

Writes /Game/Art/ChultTemple/BeholderFX (always cooked with /Game/Art):
  M_BeholderBeam   an eye ray on the camera-facing ribbons TempleBeholderFx.cpp builds each frame: vertex colour is
                   the ray's colour and fade, UV.x runs across the ribbon (a white-hot core, a coloured sheath), UV.y
                   along it in metres (energy flowing out from the eye). Unlit, additive.
  M_BeholderWave   the central eye's cone: a shell of light, brightest where it is seen edge-on, with a rippling
                   pattern streaming out along it. Vertex colour is its colour and strength. Unlit, additive.
  M_BeholderSpark  a soft point of light on an instanced billboard (custom data R, G, B, A): ray motes, sparks, embers.
  M_BeholderFlare  Unity Labs' CC0 fireball flipbook (T_JetFireBall, Tools/import_temple_fire.py) tinted per instance
                   (custom data frame, R, G, B, A): a ray's burst of energy where it strikes, coloured as the ray.
The game holds these assets (the battle streams them in): an existing one is kept; pass -Clean as a script
argument (CHUCK_BEHOLDER_FX_CLEAN=1) to rebuild them, with no game running.
"""
import os
import unreal

FX = '/Game/Art/ChultTemple/BeholderFX'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
LIB = unreal.MaterialEditingLibrary
ASSETS = unreal.EditorAssetLibrary
MP = unreal.MaterialProperty
CLEAN = os.environ.get('CHUCK_BEHOLDER_FX_CLEAN') == '1'


def node(mat, cls, x=-400, y=0, **props):
    n = LIB.create_material_expression(mat, cls, x, y)
    for k, v in props.items(): n.set_editor_property(k, v)
    return n


def fresh(name):
    path = f'{FX}/{name}'
    if ASSETS.does_asset_exist(path):
        if not CLEAN:
            unreal.log(f'CHUCK_BEHOLDER_FX {name} kept'); return None
        ASSETS.delete_asset(path)
    mat = TOOLS.create_asset(name, FX, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ADDITIVE)
    mat.set_editor_property('two_sided', True)
    return mat


def custom(mat, code, inputs, defines=(), out=unreal.CustomMaterialOutputType.CMOT_FLOAT3):
    field = node(mat, unreal.MaterialExpressionCustom, -400, 0)
    items = []
    for key, *_ in inputs:
        item = unreal.CustomInput(); item.set_editor_property('input_name', key); items.append(item)
    field.set_editor_property('inputs', items)
    field.set_editor_property('output_type', out)
    ds = []
    for k, v in defines:
        d = unreal.CustomDefine(); d.set_editor_property('define_name', k); d.set_editor_property('define_value', v); ds.append(d)
    field.set_editor_property('additional_defines', ds)
    field.set_editor_property('code', code)
    for key, src, *pin in inputs: LIB.connect_material_expressions(src, pin[0] if pin else '', field, key)
    LIB.connect_material_property(field, '', MP.MP_EMISSIVE_COLOR)
    return field


def finish(mat, name):
    LIB.recompile_material(mat); ASSETS.save_loaded_asset(mat, only_if_is_dirty=False)
    unreal.log(f'CHUCK_BEHOLDER_FX {name}')


BEAM = r"""
float4 VC = float4(VCol, VA);
float a = abs(UV.x * 2. - 1.);
float core = exp(-a * a * 26.);
float sheath = exp(-a * a * 4.5) * (1. - a);
float s = UV.y;
// Energy streaming out of the eye, in knots that travel and pulse.
float flow = .5 + .5 * sin(s * 5.1 - Time * 33. + 2.2 * sin(s * 1.7 + Time * 6.));
float knot = pow(saturate(.5 + .5 * sin(s * 1.3 - Time * 19.)), 6.);
float flick = .78 + .22 * sin(Time * 61. + s * .9);
float3 hot = lerp(VC.rgb, float3(1., 1., 1.), .6);
return (VC.rgb * sheath * (.25 + .75 * flow + 1.5 * knot) * flick + hot * core * (.9 + knot)) * VC.a * GLOW;
"""

WAVE = r"""
float4 VC = float4(VCol, VA);
float f = saturate(1. - abs(dot(normalize(N), normalize(V))));
float rim = pow(f, 3.5);
float2 p = float2(UV.x * 30., UV.y * 7. - Time * 4.);
float n = sin(p.x + 2.1 * sin(p.y * 1.6 + Time)) * sin(p.y * 2.4 + 1.3 * sin(p.x * .7 - Time * 1.3));
n = .55 + .45 * n;
return VC.rgb * VC.a * rim * (.35 + .65 * n) * GLOW;
"""

SPARK = r"""
float2 d = UV - .5;
float r = length(d) * 2.;
float g = saturate(1. - r);
g = g * g * (.35 + .65 * exp(-r * r * 12.));
return float3(R, G, B) * A * g * GLOW;
"""

FLARE = r"""
float n = COLS * ROWS;
float f = fmod(max(Frame, 0.), n), f0 = floor(f), b = f - f0, f1 = fmod(f0 + 1., n);
float2 c0 = float2(fmod(f0, COLS), floor(f0 / COLS)), c1 = float2(fmod(f1, COLS), floor(f1 / COLS));
float2 uv = saturate(UV);
float4 s = lerp(Texture2DSample(Tex, TexSampler, (uv + c0) / float2(COLS, ROWS)), Texture2DSample(Tex, TexSampler, (uv + c1) / float2(COLS, ROWS)), b);
float l = dot(s.rgb, float3(.3, .59, .11));
float3 tint = float3(R, G, B);
// The fireball's luminance as the ray's energy: the ray's colour, white where it is hottest.
float3 col = lerp(tint * l, lerp(tint, float3(1., 1., 1.), .7) * l, saturate(l * l * 1.6));
return col * saturate(A) * Soft * GLOW;
"""


def beam():
    mat = fresh('M_BeholderBeam')
    if not mat: return
    custom(mat, BEAM, [('UV', node(mat, unreal.MaterialExpressionTextureCoordinate, -800, 0)),
                       ('VCol', vc := node(mat, unreal.MaterialExpressionVertexColor, -800, 120)), ('VA', vc, 'A'),
                       ('Time', node(mat, unreal.MaterialExpressionTime, -800, 260))], [('GLOW', '11.')])
    finish(mat, 'M_BeholderBeam')


def wave():
    mat = fresh('M_BeholderWave')
    if not mat: return
    custom(mat, WAVE, [('UV', node(mat, unreal.MaterialExpressionTextureCoordinate, -800, 0)),
                       ('VCol', vc := node(mat, unreal.MaterialExpressionVertexColor, -800, 120)), ('VA', vc, 'A'),
                       ('Time', node(mat, unreal.MaterialExpressionTime, -800, 240)),
                       ('N', node(mat, unreal.MaterialExpressionVertexNormalWS, -800, 360)),
                       ('V', node(mat, unreal.MaterialExpressionCameraVectorWS, -800, 480))], [('GLOW', '5.')])
    finish(mat, 'M_BeholderWave')


def spark():
    mat = fresh('M_BeholderSpark')
    if not mat: return
    mat.set_editor_property('used_with_instanced_static_meshes', True)
    ins = [('UV', node(mat, unreal.MaterialExpressionTextureCoordinate, -800, 0))]
    for i, k in enumerate('RGBA'): ins.append((k, node(mat, unreal.MaterialExpressionPerInstanceCustomData, -800, 120 + 100 * i, data_index=i)))
    custom(mat, SPARK, ins, [('GLOW', '28.')])
    finish(mat, 'M_BeholderSpark')


def flare():
    tex = unreal.load_asset('/Game/Art/ChultTemple/Fire/T_JetFireBall')
    if not tex: raise RuntimeError('Missing /Game/Art/ChultTemple/Fire/T_JetFireBall (Tools/import_temple_fire.py)')
    mat = fresh('M_BeholderFlare')
    if not mat: return
    mat.set_editor_property('used_with_instanced_static_meshes', True)
    ins = [('Tex', node(mat, unreal.MaterialExpressionTextureObject, -900, 0, texture=tex)),
           ('UV', node(mat, unreal.MaterialExpressionTextureCoordinate, -900, 120))]
    for i, k in enumerate(('Frame', 'R', 'G', 'B', 'A')): ins.append((k, node(mat, unreal.MaterialExpressionPerInstanceCustomData, -900, 240 + 100 * i, data_index=i)))
    ins.append(('Soft', node(mat, unreal.MaterialExpressionDepthFade, -900, 800, fade_distance_default=40.)))
    custom(mat, FLARE, ins, [('COLS', '8.'), ('ROWS', '8.'), ('GLOW', '7.')])
    finish(mat, 'M_BeholderFlare')


beam(); wave(); spark(); flare()
unreal.log('CHUCK_BEHOLDER_FX_READY')
