"""The ship's furnishings' materials (Claude, 2026-11-11; user: an ornate rug in the captain's quarters, nicer hammocks
and sacks):

  T_ShipRug / M_ShipRug   the captain's rug: SourceAssets/Setting/Ship/ShipRug.png (Tools/make_ship_rug.py) over UV0
                          0..1 across the rug, the caban wool's normal for the pile, matte.
  M_ShipSack              burlap: the CC0 rough linen (Poly Haven, already imported for the NPCs' clothes) tinted to
                          jute, coarser, its normal at full strength. The ship's UVs are cm / 120.
  M_ShipHammock           hammock canvas: the same linen, pale and weathered, at its own scale.

UnrealEditor-Cmd <uproject> -ExecutePythonScript=<abs path>/Tools/create_ship_furnishings.py -unattended -nosplash -NoLiveCoding
(not -nullrhi: the texture import needs Slate)
"""
import os
import unreal

L = unreal.MaterialEditingLibrary
A = unreal.EditorAssetLibrary
T = unreal.AssetToolsHelpers.get_asset_tools()
DEST = '/Game/Art/Ship'
FAB = '/Game/Characters/Humans/Fabrics/'
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
RUG = os.path.join(ROOT, 'SourceAssets', 'Setting', 'Ship', 'ShipRug.png')

task = unreal.AssetImportTask()
task.filename = RUG
task.destination_path = DEST
task.destination_name = 'T_ShipRug'
task.replace_existing = True
task.automated = True
task.save = False
T.import_asset_tasks([task])
rug = unreal.load_asset(DEST + '/T_ShipRug')
if not rug:
    raise RuntimeError('Rug texture import failed: ' + RUG)
rug.set_editor_property('srgb', True)
rug.set_editor_property('address_x', unreal.TextureAddress.TA_CLAMP)
rug.set_editor_property('address_y', unreal.TextureAddress.TA_CLAMP)
A.save_loaded_asset(rug, only_if_is_dirty=False)


def tex(name):
    t = unreal.load_asset(FAB + name)
    if not t:
        raise RuntimeError('Missing texture ' + FAB + name)
    return t


def material(name, build):
    m = unreal.load_asset(DEST + '/' + name) or T.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    L.delete_all_material_expressions(m)
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    m.set_editor_property('two_sided', name == 'M_ShipHammock')

    def node(cls, **props):
        n = L.create_material_expression(m, cls)
        for k, v in props.items():
            n.set_editor_property(k, v)
        return n

    def link(a, b, pin='', out=''):
        if not L.connect_material_expressions(a, out, b, pin):
            raise RuntimeError('Link failed %s -> %s in %s' % (out, pin, name))

    def prop(n, p, out=''):
        if not L.connect_material_property(n, out, p):
            raise RuntimeError('Property link failed %s in %s' % (p, name))

    def sample(texture, uv, kind=None):
        s = node(unreal.MaterialExpressionTextureSample, texture=texture)
        if kind is not None:
            s.set_editor_property('sampler_type', kind)
        link(uv, s, 'UVs')
        return s

    def flat_normal(n, strength):
        # Lerp the sampled normal toward flat by (1 - strength).
        flat = node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(0, 0, 1, 0))
        lerp = node(unreal.MaterialExpressionLinearInterpolate, const_alpha=strength)
        link(flat, lerp, 'A'); link(n, lerp, 'B', 'RGB')
        return lerp

    build(node, link, prop, sample, flat_normal)
    L.recompile_material(m)
    if not A.save_loaded_asset(m, only_if_is_dirty=False):
        raise RuntimeError('Save failed ' + name)
    unreal.log('CHUCK_SHIP_FURNISHING_CREATED ' + DEST + '/' + name)


def build_rug(node, link, prop, sample, flat_normal):
    uv = node(unreal.MaterialExpressionTextureCoordinate, coordinate_index=0)
    col = sample(rug, uv)
    prop(col, unreal.MaterialProperty.MP_BASE_COLOR, 'RGB')
    # The pile: the caban's weave, 310 cm / 27.4 cm across the rug's length, at half strength.
    fine = node(unreal.MaterialExpressionTextureCoordinate, coordinate_index=0, u_tiling=310.0 / 14.0, v_tiling=260.0 / 14.0)
    n = sample(tex('T_caban_nor_dx_2k'), fine, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    prop(flat_normal(n, 0.55), unreal.MaterialProperty.MP_NORMAL)
    prop(node(unreal.MaterialExpressionConstant, r=0.96), unreal.MaterialProperty.MP_ROUGHNESS)
    prop(node(unreal.MaterialExpressionConstant, r=0.22), unreal.MaterialProperty.MP_SPECULAR)


def cloth(tint, tile_cm, strength, rough):
    def build(node, link, prop, sample, flat_normal):
        uv = node(unreal.MaterialExpressionTextureCoordinate, coordinate_index=0, u_tiling=120.0 / tile_cm, v_tiling=120.0 / tile_cm)
        d = sample(tex('T_rough_linen_diff_2k'), uv)
        # Toward grey first (the linen is near white), then the dye/jute colour.
        desat = node(unreal.MaterialExpressionDesaturation)
        link(d, desat, '', 'RGB')
        mul = node(unreal.MaterialExpressionMultiply)
        c = node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(tint[0], tint[1], tint[2], 1))
        link(desat, mul, 'A'); link(c, mul, 'B')
        prop(mul, unreal.MaterialProperty.MP_BASE_COLOR)
        n = sample(tex('T_rough_linen_nor_dx_2k'), uv, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        prop(flat_normal(n, strength), unreal.MaterialProperty.MP_NORMAL)
        prop(node(unreal.MaterialExpressionConstant, r=rough), unreal.MaterialProperty.MP_ROUGHNESS)
        prop(node(unreal.MaterialExpressionConstant, r=0.3), unreal.MaterialProperty.MP_SPECULAR)
    return build


material('M_ShipRug', build_rug)
material('M_ShipSack', cloth((0.62, 0.46, 0.27), 34.0, 1.0, 0.93))       # jute
material('M_ShipHammock', cloth((0.70, 0.62, 0.48), 22.0, 0.8, 0.88))   # weathered canvas
unreal.log('CHUCK_SHIP_FURNISHINGS_DONE')
