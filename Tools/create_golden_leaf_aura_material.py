"""The golden cigarette's aura (docs/CHULT-GOLDEN-LEAF.md).

One additive, unlit material for the pickup's halo shells and rising motes:
Color (vector) x Intensity (scalar) x a soft core that fades to nothing at the
silhouette, and to nothing where it meets the ground (depth fade), so a sphere
reads as a haze of light rather than a ball. Only this asset is written.

Run: UnrealEditor-Cmd Chuck3D.uproject -ExecutePythonScript=<this file>
     -unattended -nullrhi -nosplash -NoLiveCoding
"""
import unreal

PATH = '/Game/Art/Pickups/M_GoldenLeafAura'
LIB = unreal.MaterialEditingLibrary


def node(mat, cls, **props):
    n = LIB.create_material_expression(mat, cls)
    for k, v in props.items(): n.set_editor_property(k, v)
    return n


def link(a, b, pin_b, pin_a=''):
    if not LIB.connect_material_expressions(a, pin_a, b, pin_b):
        raise RuntimeError(f'Material link failed: {pin_a} -> {pin_b}')


mat = unreal.load_asset(PATH)
if mat is None:
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_GoldenLeafAura', '/Game/Art/Pickups', unreal.Material, unreal.MaterialFactoryNew())
LIB.delete_all_material_expressions(mat)
mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ADDITIVE)
mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property('two_sided', False)
mat.set_editor_property('used_with_instanced_static_meshes', True)   # the motes

colour = node(mat, unreal.MaterialExpressionVectorParameter, parameter_name='Color', default_value=unreal.LinearColor(1., .64, .2, 1.))
intensity = node(mat, unreal.MaterialExpressionScalarParameter, parameter_name='Intensity', default_value=1.)
glow = node(mat, unreal.MaterialExpressionMultiply)
link(colour, glow, 'A'); link(intensity, glow, 'B')
# Bright facing the eye, gone at the edge.
edge = node(mat, unreal.MaterialExpressionFresnel, exponent=1.0, base_reflect_fraction=0.)
core = node(mat, unreal.MaterialExpressionOneMinus); link(edge, core, '')
soft = node(mat, unreal.MaterialExpressionPower, const_exponent=3.)
link(core, soft, 'Base')
# No hard line where the shell passes through the leaf litter.
fade = node(mat, unreal.MaterialExpressionDepthFade, fade_distance_default=12.)
shape = node(mat, unreal.MaterialExpressionMultiply)
link(soft, shape, 'A'); link(fade, shape, 'B')
# Rise (0..1): fade toward the shell's top, so the tall wisp thins out upward.
local = node(mat, unreal.MaterialExpressionLocalPosition)
height = node(mat, unreal.MaterialExpressionComponentMask, r=False, g=False, b=True, a=False)
link(local, height, '')
scaled = node(mat, unreal.MaterialExpressionMultiply, const_b=-.01)   # the sphere's local z runs -50..50
link(height, scaled, 'A')
lower = node(mat, unreal.MaterialExpressionAdd, const_b=.5)
link(scaled, lower, 'A')
ramp = node(mat, unreal.MaterialExpressionSaturate); link(lower, ramp, '')
rise = node(mat, unreal.MaterialExpressionScalarParameter, parameter_name='Rise', default_value=0.)
taper = node(mat, unreal.MaterialExpressionLinearInterpolate, const_a=1.)
link(ramp, taper, 'B'); link(rise, taper, 'Alpha')
faded = node(mat, unreal.MaterialExpressionMultiply)
link(shape, faded, 'A'); link(taper, faded, 'B')
out = node(mat, unreal.MaterialExpressionMultiply)
link(glow, out, 'A'); link(faded, out, 'B')
LIB.connect_material_property(out, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
LIB.recompile_material(mat)
unreal.EditorAssetLibrary.save_loaded_asset(mat, only_if_is_dirty=False)
unreal.log('CHUCK_GOLDEN_AURA_MATERIAL_READY')
