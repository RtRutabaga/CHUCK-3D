"""Rebuild M_PantrySky as an open sky seen along the view (Claude, 2026-10-13).

UnrealEditor-Cmd <uproject> -ExecutePythonScript=Tools/create_pantry_deep_sky.py -unattended -nosplash -NoLiveCoding

The cellar's sky hole is lined by a box (DockPantry.cpp). With the old
surface-space sky (Tools/create_pantry_materials.py) its walls and corners
showed as a square of different sky as he fell, before the cutscene's real
sky. This graph ignores where on the box a pixel is and colours it by the
view ray alone, as a sky dome would: teal haze toward the horizon and, 2.5 km
below, a sunlit cloud sea (the fall cutscene's deck, 2.0-3.1 km over the
jungle) with dark jungle-green gaps, drifting slowly. Every wall and the floor
then show one continuous sky, and the cutscene's first sky shot follows on.
Unlit, two-sided, usable on instances. Only this material is touched.
"""
import unreal

PATH = '/Game/Art/Materials/M_PantrySky'
LIB = unreal.MaterialEditingLibrary
ASSETS = unreal.EditorAssetLibrary


def node(mat, cls, **props):
    n = LIB.create_material_expression(mat, cls)
    for k, v in props.items(): n.set_editor_property(k, v)
    return n


def link(a, b, pin_b, pin_a=''):
    if not LIB.connect_material_expressions(a, pin_a, b, pin_b):
        raise RuntimeError(f'Material link failed: {pin_a} -> {pin_b}')


def const(mat, v): return node(mat, unreal.MaterialExpressionConstant, r=v)
def colour(mat, r, g, b): return node(mat, unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(r, g, b, 1))


def op(mat, cls, a, b, pa='', pb=''):
    n = node(mat, cls); link(a, n, 'A', pa); link(b, n, 'B', pb); return n


def mul(mat, a, b, pa='', pb=''): return op(mat, unreal.MaterialExpressionMultiply, a, b, pa, pb)
def add(mat, a, b, pa='', pb=''): return op(mat, unreal.MaterialExpressionAdd, a, b, pa, pb)


def lerp(mat, a, b, alpha):
    n = node(mat, unreal.MaterialExpressionLinearInterpolate)
    link(a, n, 'A'); link(b, n, 'B'); link(alpha, n, 'Alpha'); return n


def mask(mat, src, r=False, g=False, b=False):
    m = node(mat, unreal.MaterialExpressionComponentMask, r=r, g=g, b=b, a=False); link(src, m, ''); return m


def clamp(mat, src, lo=0., hi=1.):
    c = node(mat, unreal.MaterialExpressionClamp, min_default=lo, max_default=hi); link(src, c, ''); return c


mat = unreal.load_asset(PATH)
if not mat: raise RuntimeError('M_PantrySky missing: run Tools/create_pantry_materials.py first')
LIB.delete_all_material_expressions(mat)
mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property('two_sided', True)
mat.set_editor_property('used_with_instanced_static_meshes', True)

# The view ray (CameraVector points from the surface to the eye).
view = mul(mat, node(mat, unreal.MaterialExpressionCameraVectorWS), const(mat, -1.))
vz = mask(mat, view, b=True)
down = clamp(mat, mul(mat, vz, const(mat, -1.)), .02, 1.)
# Where it meets the cloud sea 2.5 km below the eye, drifting with time.
cam = node(mat, unreal.MaterialExpressionCameraPositionWS)
reach = node(mat, unreal.MaterialExpressionDivide); link(const(mat, 250000.), reach, 'A'); link(down, reach, 'B')
hit = add(mat, mask(mat, cam, r=True, g=True), mul(mat, mask(mat, view, r=True, g=True), reach))
drift = mul(mat, node(mat, unreal.MaterialExpressionTime), colour(mat, 900., 350., 0.))
hit3 = node(mat, unreal.MaterialExpressionAppendVector); link(hit, hit3, 'A'); link(const(mat, 0.), hit3, 'B')
moved = add(mat, hit3, drift)
cells = node(mat, unreal.MaterialExpressionNoise, scale=.000011, levels=3, output_min=0., output_max=1., quality=2)
link(moved, cells, '')
fine = node(mat, unreal.MaterialExpressionNoise, scale=.00009, levels=3, output_min=0., output_max=1., quality=1)
link(moved, fine, '')
puffs = add(mat, cells, mul(mat, add(mat, fine, const(mat, -.5)), const(mat, .25)))
cover = clamp(mat, mul(mat, add(mat, puffs, const(mat, -.5)), const(mat, 3.5)))
# Sunlit tops with soft grey shading where the fine noise is low; deep green
# haze of the jungle far below in the gaps.
cloud = lerp(mat, colour(mat, .58, .64, .72), colour(mat, 1., .99, .96), clamp(mat, mul(mat, fine, const(mat, 1.6))))
below = lerp(mat, colour(mat, .12, .19, .23), cloud, cover)
horizon = colour(mat, .62, .78, .84)
haze = node(mat, unreal.MaterialExpressionPower); link(clamp(mat, add(mat, const(mat, 1.), mul(mat, down, const(mat, -1.)))), haze, 'Base'); link(const(mat, 5.), haze, 'Exp')
seen_below = lerp(mat, below, horizon, haze)
# Above the horizon (only at the hole's rim): teal toward the zenith.
seen_above = lerp(mat, horizon, colour(mat, .13, .43, .62), clamp(mat, vz))
sky = lerp(mat, seen_below, seen_above, clamp(mat, add(mat, mul(mat, vz, const(mat, 40.)), const(mat, .5))))
LIB.connect_material_property(mul(mat, sky, const(mat, .85)), '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
LIB.recompile_material(mat)
ASSETS.save_loaded_asset(mat, only_if_is_dirty=False)
unreal.log('CHUCK_PANTRY_DEEP_SKY_READY')
