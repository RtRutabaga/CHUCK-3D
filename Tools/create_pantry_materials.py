"""Create the pantry's sky, cloud and cheese materials (user 2026-10-03).

UnrealEditor-Cmd <uproject> -ExecutePythonScript=Tools/create_pantry_materials.py -unattended -nosplash -NoLiveCoding

M_PantrySky   the open sky seen down through the pantry's floor (the 2D game's
              teal sky tiles): unlit, a blue that pales toward the horizon, with
              slow drifting cloud from procedural noise in world space.
M_PantryCloud soft white cloud puffs drifting in it: unlit translucent, fading at
              their silhouettes.
M_Cheese      the wheel of cheese on the crate it can't be reached on: waxy yellow.
Each is kept if it exists (delete it to rebuild).
"""
import unreal

DEST = '/Game/Art/Materials'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
LIB = unreal.MaterialEditingLibrary
ASSETS = unreal.EditorAssetLibrary


def node(mat, cls, **props):
    n = LIB.create_material_expression(mat, cls)
    for k, v in props.items(): n.set_editor_property(k, v)
    return n


def link(a, b, pin_b, pin_a=''):
    if not LIB.connect_material_expressions(a, pin_a, b, pin_b):
        raise RuntimeError(f'Material link failed: {pin_a} -> {pin_b}')


def colour(mat, r, g, b):
    return node(mat, unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(r, g, b, 1))


def make(name, build):
    path = f'{DEST}/{name}'
    if unreal.load_asset(path):
        unreal.log(f'CHUCK_PANTRY_MATERIAL_KEPT {name}'); return
    mat = TOOLS.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    try:
        mat.set_editor_property('used_with_instanced_static_meshes', True)   # the pantry draws its shapes instanced
        build(mat)
    except Exception:
        ASSETS.delete_loaded_asset(mat); raise
    LIB.recompile_material(mat)
    ASSETS.save_loaded_asset(mat, only_if_is_dirty=False)
    unreal.log(f'CHUCK_PANTRY_MATERIAL_CREATED {name}')


def sky(mat):
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('two_sided', True)
    # Drifting world-space cloud: nested sines of (position + time * wind),
    # a soft blobby field, thresholded into separate clouds with clear sky between.
    pos = node(mat, unreal.MaterialExpressionWorldPosition)
    t = node(mat, unreal.MaterialExpressionTime)
    wind = node(mat, unreal.MaterialExpressionMultiply)
    link(t, wind, 'A'); link(colour(mat, 14., 6., 0.), wind, 'B')
    moved = node(mat, unreal.MaterialExpressionAdd); link(pos, moved, 'A'); link(wind, moved, 'B')
    scaled = node(mat, unreal.MaterialExpressionMultiply, const_b=.0045); link(moved, scaled, 'A')
    def comp(src, r, g, b):
        m = node(mat, unreal.MaterialExpressionComponentMask, r=r, g=g, b=b, a=False); link(src, m, ''); return m
    def mul(src, k):
        m = node(mat, unreal.MaterialExpressionMultiply, const_b=k); link(src, m, 'A'); return m
    def add(a, b):
        m = node(mat, unreal.MaterialExpressionAdd); link(a, m, 'A'); link(b, m, 'B'); return m
    def sine(src):
        m = node(mat, unreal.MaterialExpressionSine, period=6.2831853); link(src, m, ''); return m
    x = comp(scaled, True, False, False); y = comp(scaled, False, True, False); z = comp(scaled, False, False, True)
    a = sine(add(add(x, mul(sine(mul(y, 1.7)), 1.2)), mul(z, .6)))
    b = sine(add(add(mul(y, 1.3), mul(sine(mul(x, .8)), 1.5)), mul(z, .9)))
    c = sine(add(mul(x, 2.9), mul(y, 2.3)))
    field = add(a, add(b, mul(c, .35)))
    shifted = node(mat, unreal.MaterialExpressionAdd, const_b=-.75); link(field, shifted, 'A')
    sharp = mul(shifted, 1.4)
    clamp = node(mat, unreal.MaterialExpressionClamp); link(sharp, clamp, '')
    # Deeper blue at the bottom of the view, paler toward the sides.
    depth = node(mat, unreal.MaterialExpressionVertexNormalWS)
    up = node(mat, unreal.MaterialExpressionComponentMask, r=False, g=False, b=True, a=False); link(depth, up, '')
    absup = node(mat, unreal.MaterialExpressionAbs); link(up, absup, '')
    blue = node(mat, unreal.MaterialExpressionLinearInterpolate)
    link(colour(mat, .42, .66, .80), blue, 'A'); link(colour(mat, .16, .40, .70), blue, 'B'); link(absup, blue, 'Alpha')
    sky_col = node(mat, unreal.MaterialExpressionLinearInterpolate)
    link(blue, sky_col, 'A'); link(colour(mat, .93, .95, .97), sky_col, 'B'); link(clamp, sky_col, 'Alpha')
    bright = node(mat, unreal.MaterialExpressionMultiply, const_b=.75); link(sky_col, bright, 'A')
    LIB.connect_material_property(bright, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)


def cloud(mat):
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property('two_sided', True)
    LIB.connect_material_property(colour(mat, 1.5, 1.52, 1.55), '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    # Soft edges: opaque facing the eye, gone at the silhouette.
    fres = node(mat, unreal.MaterialExpressionFresnel, exponent=1.6)
    inv = node(mat, unreal.MaterialExpressionOneMinus); link(fres, inv, '')
    op = node(mat, unreal.MaterialExpressionMultiply, const_b=.75); link(inv, op, 'A')
    LIB.connect_material_property(op, '', unreal.MaterialProperty.MP_OPACITY)


def cheese(mat):
    LIB.connect_material_property(colour(mat, .80, .52, .12), '', unreal.MaterialProperty.MP_BASE_COLOR)
    LIB.connect_material_property(node(mat, unreal.MaterialExpressionConstant, r=.42), '', unreal.MaterialProperty.MP_ROUGHNESS)
    LIB.connect_material_property(node(mat, unreal.MaterialExpressionConstant, r=.3), '', unreal.MaterialProperty.MP_SPECULAR)


make('M_PantrySky', sky)
make('M_PantryCloud', cloud)
make('M_Cheese', cheese)
print('CHUCK_PANTRY_MATERIALS_READY')
