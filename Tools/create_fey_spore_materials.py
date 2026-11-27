"""The Feywild's spores (Claude, 2026-11-26; the user: "implement free resource to make puff ball and etc spore clouds look
better ... make slow spores look better").

UnrealEditor-Cmd <uproject> "-ExecutePythonScript=<absolute path to this file>" -unattended -nosplash -NoLiveCoding

AFeySpores (C++) draws every spore cloud and drifting spore grain in the Feywild on instanced camera-facing quads, writing
six per-instance floats (frame, glow, opacity, red, green, blue). The cloud is Unity Labs' CC0 Houdini flipbook
WispySmoke01 (8x8), already imported for the temple's fire jets (T_JetSmoke, Tools/import_temple_fire.py), here lit from
within in the spore's colour instead of dark soot:
  M_FeySporeCloud  WispySmoke01, translucent: a soft billowing puff of spores, tinted, glowing a little at its densest
  M_FeySporeMote   procedural, additive: one grain of spore, a soft round glow that twinkles
Only these assets are created. To rebuild one, delete its .uasset first (clearing a loaded material's graph crashes UE 5.7).
"""
import unreal

FIRE = '/Game/Art/ChultTemple/Fire'
DEST = '/Game/Art/Feywild/Spores'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
LIB = unreal.MaterialEditingLibrary
ASSETS = unreal.EditorAssetLibrary
ASSETS.make_directory(DEST)
F4 = unreal.CustomMaterialOutputType.CMOT_FLOAT4


def node(mat, cls, **props):
    n = LIB.create_material_expression(mat, cls)
    for k, v in props.items(): n.set_editor_property(k, v)
    return n


def link(a, b, pin_b, pin_a=''):
    if not LIB.connect_material_expressions(a, pin_a, b, pin_b): raise RuntimeError(f'link {pin_a} -> {pin_b}')


CLOUD = r'''
float n = 64.;
float f = fmod(max(Frame, 0.), n), f0 = floor(f), b = f - f0, f1 = fmod(f0 + 1., n);
float2 c0 = float2(fmod(f0, 8.), floor(f0 / 8.)), c1 = float2(fmod(f1, 8.), floor(f1 / 8.));
float2 uv = saturate(UV);
float4 s = lerp(Texture2DSample(Tex, TexSampler, (uv + c0) / 8.), Texture2DSample(Tex, TexSampler, (uv + c1) / 8.), b);
float fade = saturate(Opacity) * Soft * saturate((Depth - 25.) / 70.);
float3 tint = float3(R, G, B);
float dens = s.a;
// lit from within: the thick of it brighter and warmer, the wisps paler and cooler
float3 col = tint * (.22 + .9 * s.r) * (.35 + Glow) * lerp(float3(.85, .9, 1.), float3(1.1, 1., .85), dens);
return float4(col, dens * fade * .75);
'''

MOTE = r'''
float2 d = UV - .5;
float r = length(d) * 2.;
float core = pow(saturate(1. - r), 2.4) + .35 * pow(saturate(1. - r), 9.);
float3 tint = float3(R, G, B);
return float4(tint * core * saturate(Opacity) * (.5 + 2.4 * Glow) * saturate((Depth - 15.) / 50.), 1.);
'''


def build(name, code, translucent, texture=None):
    path = f'{DEST}/{name}'
    if ASSETS.does_asset_exist(path):
        unreal.log(f'CHUCK_FEY_SPORE_MATERIAL kept {name}'); return
    mat = TOOLS.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT if translucent else unreal.BlendMode.BLEND_ADDITIVE)
    mat.set_editor_property('two_sided', True)
    mat.set_editor_property('used_with_instanced_static_meshes', True)
    uv = node(mat, unreal.MaterialExpressionTextureCoordinate)
    data = [node(mat, unreal.MaterialExpressionPerInstanceCustomData, data_index=i) for i in range(6)]
    depth = node(mat, unreal.MaterialExpressionPixelDepth)
    field = node(mat, unreal.MaterialExpressionCustom)
    keys = ['UV', 'Frame', 'Glow', 'Opacity', 'R', 'G', 'B', 'Depth'] + (['Tex', 'Soft'] if texture else [])
    ins = []
    for k in keys:
        i = unreal.CustomInput(); i.set_editor_property('input_name', k); ins.append(i)
    field.set_editor_property('inputs', ins)
    field.set_editor_property('output_type', F4)
    field.set_editor_property('code', code)
    srcs = {'UV': uv, 'Frame': data[0], 'Glow': data[1], 'Opacity': data[2], 'R': data[3], 'G': data[4], 'B': data[5], 'Depth': depth}
    if texture:
        srcs['Tex'] = node(mat, unreal.MaterialExpressionTextureObject, texture=texture)
        srcs['Soft'] = node(mat, unreal.MaterialExpressionDepthFade, fade_distance_default=40.)
    for k in keys: link(srcs[k], field, k)
    rgb = node(mat, unreal.MaterialExpressionComponentMask, r=True, g=True, b=True, a=False); link(field, rgb, '')
    LIB.connect_material_property(rgb, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if translucent:
        a = node(mat, unreal.MaterialExpressionComponentMask, r=False, g=False, b=False, a=True); link(field, a, '')
        LIB.connect_material_property(a, '', unreal.MaterialProperty.MP_OPACITY)
    LIB.recompile_material(mat)
    if not ASSETS.save_loaded_asset(mat, only_if_is_dirty=False): raise RuntimeError(name)
    unreal.log(f'CHUCK_FEY_SPORE_MATERIAL built {name}')


smoke = unreal.load_asset(f'{FIRE}/T_JetSmoke')
if not smoke: raise RuntimeError('T_JetSmoke missing: run Tools/import_temple_fire.py first')
build('M_FeySporeCloud', CLOUD, True, smoke)
build('M_FeySporeMote', MOTE, False)
unreal.log('CHUCK_FEY_SPORE_MATERIALS_READY')
