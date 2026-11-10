"""Unreal 5.7 editor Python: M_ShipComber, the big sea that knocks the brig over in the storm ending's cutscene
(user 2026-11-09: "a big wave shown as knocking the ship"; ShipEndingScene.cpp builds its mesh).

UnrealEditor-Cmd <uproject> -ExecutePythonScript=Tools/create_ship_comber_material.py -unattended -nosplash -NoLiveCoding

Lit, opaque, two-sided water in the storm sea's own colours (create_ship_sea.py: slate grey-green, the crests a dull
green), the sea's wave normal and foam textures running with the wave. Vertex colour R: foam (its crest, the white water
streaming down its face and boiling at its foot); G: thin water (light through the crest, a faint green).
Creates or rebuilds only this asset in /Game/Art/Ship; imports nothing.
"""
import unreal

L = unreal.MaterialEditingLibrary
A = unreal.EditorAssetLibrary
T = unreal.AssetToolsHelpers.get_asset_tools()
DEST = '/Game/Art/Ship'
NAME = 'M_ShipComber'

m = unreal.load_asset(DEST + '/' + NAME) or T.create_asset(NAME, DEST, unreal.Material, unreal.MaterialFactoryNew())
L.delete_all_material_expressions(m)
m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
m.set_editor_property('two_sided', True)


def node(cls, **props):
    n = L.create_material_expression(m, cls)
    for k, v in props.items():
        n.set_editor_property(k, v)
    return n


def link(a, b, pin='', out=''):
    if not L.connect_material_expressions(a, out, b, pin):
        raise RuntimeError('Link failed %s -> %s' % (out, pin))


def custom(code, kind, inputs):
    c = node(unreal.MaterialExpressionCustom, code=code, output_type=kind)
    ins = []
    for k in inputs:
        i = unreal.CustomInput(); i.set_editor_property('input_name', k); ins.append(i)
    c.set_editor_property('inputs', ins)
    for k, src in inputs.items():
        link(src, c, k)
    return c


normal_tex = unreal.load_asset('/Game/Art/Textures/Water/T_Water_TilingNormal_Waves_02')
foam_tex = unreal.load_asset('/Game/Art/Textures/Water/T_WaterFlow_01_Foam_Tiled')
if not normal_tex or not foam_tex:
    raise RuntimeError('the sea textures are missing (Tools/create_ship_sea.py uses them)')
vc = node(unreal.MaterialExpressionVertexColor)
wp = node(unreal.MaterialExpressionWorldPosition)
tm = node(unreal.MaterialExpressionTime)
# (Texture coordinates from the world, running down the face as the water pours off the crest.)
uv_n = custom('return float2(W.x / 380. + W.y / 1100., W.z / 320. + W.y / 900.) + float2(.02, .35) * Tm;',
              unreal.CustomMaterialOutputType.CMOT_FLOAT2, {'W': wp, 'Tm': tm})
uv_f = custom('return float2(W.x / 520. + W.y / 1400., W.z / 900. + W.y / 1200.) + float2(-.02, .55) * Tm;',
              unreal.CustomMaterialOutputType.CMOT_FLOAT2, {'W': wp, 'Tm': tm})
nrm = node(unreal.MaterialExpressionTextureSample, texture=normal_tex, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
link(uv_n, nrm, 'UVs')
fsm = node(unreal.MaterialExpressionTextureSample, texture=foam_tex)
link(uv_f, fsm, 'UVs')
look = custom(r'''
float tex = dot(F.rgb, float3(.33, .33, .34));
// (Foam only where the mesh puts it, streaked by the texture; never spotting the clear water.)
float foam = smoothstep(.3, .8, C.r + (tex - .5) * .7 * C.r);
float3 water = lerp(float3(.006, .013, .016), float3(.02, .045, .045), saturate(C.g));
return float4(lerp(water, float3(.6, .64, .65), foam), foam);
''', unreal.CustomMaterialOutputType.CMOT_FLOAT4, {'C': vc, 'F': fsm})
rgb = node(unreal.MaterialExpressionComponentMask, r=True, g=True, b=True, a=False)
link(look, rgb)
foam_a = node(unreal.MaterialExpressionComponentMask, r=False, g=False, b=False, a=True)
link(look, foam_a)
rough = custom('return lerp(.24, .7, Fm);', unreal.CustomMaterialOutputType.CMOT_FLOAT1, {'Fm': foam_a})
flat = custom('return normalize(lerp(float3(0, 0, 1), N.xyz * float3(1.1, 1.1, 1), 1 - .7 * Fm));',
              unreal.CustomMaterialOutputType.CMOT_FLOAT3, {'N': nrm, 'Fm': foam_a})
# Light through the thin crest: a faint green of its own (the storm light is dim and grey).
glow = custom('return float3(.004, .016, .013) * saturate(C.g) * (1 - Fm);',
              unreal.CustomMaterialOutputType.CMOT_FLOAT3, {'C': vc, 'Fm': foam_a})
L.connect_material_property(rgb, '', unreal.MaterialProperty.MP_BASE_COLOR)
L.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
L.connect_material_property(flat, '', unreal.MaterialProperty.MP_NORMAL)
L.connect_material_property(glow, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
spec = node(unreal.MaterialExpressionConstant, r=.35)
L.connect_material_property(spec, '', unreal.MaterialProperty.MP_SPECULAR)
L.recompile_material(m)
A.save_asset(DEST + '/' + NAME)
unreal.log('CHUCK_SHIP_COMBER_MATERIAL_READY')
