"""The temple fire jets' flipbooks and materials (Claude, 2026-10-22; docs/CHULT-TEMPLE-RUN.md).

Imports the three CC0 Unity Labs flipbooks that Tools/Fetch-TempleFire.ps1 verified into
/Game/Art/ChultTemple/Fire and (re)builds three materials for AChultTempleTraps' fire sim.
Each is drawn on instanced camera-facing quads; the sim writes three per-instance floats:
  0  the flipbook frame (blended with the next, so the fire rolls smoothly)
  1  heat 0..1: a puff leaves the nozzle white-hot and cools to a dull red (and its smoke glows less)
  2  opacity 0..1 (fade in, burn out)
  M_TempleJetFire   FireBall01 8x8, additive: the jet's rolling puffs
  M_TempleJetPilot  Flame02 16x4, additive: the flame licking at the mouth before a burst
  M_TempleJetSmoke  WispySmoke01 8x8, translucent: dark smoke from the burnt-out puffs
All three fade where they meet a wall (depth fade) and close to the camera.

UnrealEditor-Cmd <uproject> -ExecutePythonScript=Tools/import_temple_fire.py -unattended -nullrhi -nosplash -NoLiveCoding
"""
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'SourceAssets/Setting/TempleFire'
DEST = '/Game/Art/ChultTemple/Fire'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
LIB = unreal.MaterialEditingLibrary
ASSETS = unreal.EditorAssetLibrary


def node(mat, cls, **props):
    n = LIB.create_material_expression(mat, cls)
    for k, v in props.items():
        n.set_editor_property(k, v)
    return n


def link(a, b, pin_b, pin_a=''):
    if not LIB.connect_material_expressions(a, pin_a, b, pin_b):
        raise RuntimeError(f'Material link failed: {pin_a} -> {pin_b}')


def import_texture(file, name):
    task = unreal.AssetImportTask()
    task.filename = str(SOURCE / file); task.destination_path = DEST; task.destination_name = name
    task.automated = True; task.replace_existing = True; task.save = True
    TOOLS.import_asset_tasks([task])
    tex = unreal.load_asset(f'{DEST}/{name}')
    if not tex:
        raise RuntimeError(f'Missing {file}')
    tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_EFFECTS)
    tex.set_editor_property('srgb', True)
    ASSETS.save_loaded_asset(tex, only_if_is_dirty=False)
    return tex


# The flipbook (blending frame F with the next), the heat colouring and the fades, in one node.
# Returns rgb (emissive) and a (opacity for the smoke).
CODE = r'''
float n = COLS * ROWS;
float f = fmod(max(Frame, 0.), n), f0 = floor(f), b = f - f0, f1 = fmod(f0 + 1., n);
float2 c0 = float2(fmod(f0, COLS), floor(f0 / COLS)), c1 = float2(fmod(f1, COLS), floor(f1 / COLS));
float2 uv = saturate(UV);
float4 s = lerp(Texture2DSample(Tex, TexSampler, (uv + c0) / float2(COLS, ROWS)),
                Texture2DSample(Tex, TexSampler, (uv + c1) / float2(COLS, ROWS)), b);
float fade = saturate(Opacity) * Soft * saturate((Depth - 25.) / 70.);
float h = saturate(Heat);
#if MODE == 2
  // Smoke: dark soot, lit a little from below by the fire it came from.
  float3 col = s.rgb * float3(.032, .03, .028) + float3(1., .4, .12) * .06 * h * s.a;
  return float4(col, s.a * fade * .7);
#else
  // Fire: the sim's colour is the white-hot core; cooling, it drops toward a deep red and dims
  // (a blackbody's curve, roughly), and the thinner edges go red first.
  float l = dot(s.rgb, float3(.3, .59, .11));
  float3 hot = s.rgb * float3(1., .66, .34);
  float3 cool = l * float3(1., .24, .035);
  float t = saturate(h * (.55 + .7 * l));
  float3 col = lerp(cool, hot, t) * (GLOW * (.12 + 1.1 * pow(h, 1.4)));
  return float4(col * fade * (MODE == 1 ? s.a : 1.), 1.);
#endif
'''


def build(name, texture, cols, rows, mode, glow):
    mat = unreal.load_asset(f'{DEST}/{name}') or TOOLS.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    LIB.delete_all_material_expressions(mat)
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT if mode == 2 else unreal.BlendMode.BLEND_ADDITIVE)
    mat.set_editor_property('two_sided', True)
    mat.set_editor_property('used_with_instanced_static_meshes', True)
    tex = node(mat, unreal.MaterialExpressionTextureObject, texture=texture)
    uv = node(mat, unreal.MaterialExpressionTextureCoordinate)
    data = [node(mat, unreal.MaterialExpressionPerInstanceCustomData, data_index=i) for i in range(3)]
    soft = node(mat, unreal.MaterialExpressionDepthFade, fade_distance_default=35.)
    depth = node(mat, unreal.MaterialExpressionPixelDepth)
    field = node(mat, unreal.MaterialExpressionCustom)
    inputs = []
    for key in ('Tex', 'UV', 'Frame', 'Heat', 'Opacity', 'Soft', 'Depth'):
        item = unreal.CustomInput(); item.set_editor_property('input_name', key); inputs.append(item)
    field.set_editor_property('inputs', inputs)
    field.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    defines = []
    for k, v in (('COLS', f'{cols}.'), ('ROWS', f'{rows}.'), ('MODE', str(mode)), ('GLOW', f'{glow}')):
        d = unreal.CustomDefine(); d.set_editor_property('define_name', k); d.set_editor_property('define_value', v); defines.append(d)
    field.set_editor_property('additional_defines', defines)
    field.set_editor_property('code', CODE)
    for key, src in (('Tex', tex), ('UV', uv), ('Frame', data[0]), ('Heat', data[1]), ('Opacity', data[2]), ('Soft', soft), ('Depth', depth)):
        link(src, field, key)
    rgb = node(mat, unreal.MaterialExpressionComponentMask, r=True, g=True, b=True, a=False)
    link(field, rgb, '')
    LIB.connect_material_property(rgb, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if mode == 2:
        alpha = node(mat, unreal.MaterialExpressionComponentMask, r=False, g=False, b=False, a=True)
        link(field, alpha, '')
        LIB.connect_material_property(alpha, '', unreal.MaterialProperty.MP_OPACITY)
    LIB.recompile_material(mat)
    if not ASSETS.save_loaded_asset(mat, only_if_is_dirty=False):
        raise RuntimeError(name)
    unreal.log('CHUCK_TEMPLE_FIRE_MATERIAL ' + name)


fire = import_texture('FireBall01_8x8.tga', 'T_JetFireBall')
pilot = import_texture('Flame02_16x4.tga', 'T_JetPilotFlame')
smoke = import_texture('WispySmoke01_8x8.tga', 'T_JetSmoke')
build('M_TempleJetFire', fire, 8, 8, 0, .95)
build('M_TempleJetPilot', pilot, 16, 4, 1, 3.5)
build('M_TempleJetSmoke', smoke, 8, 8, 2, 1.)
unreal.log('CHUCK_TEMPLE_FIRE_READY')
