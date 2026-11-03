"""T_ShipFlag and M_ShipFlag: the pirate ship's flag, the user's mustachioed skull and crossed cutlasses
(SourceAssets/Setting/Ship/ShipFlag_Moustache.jpg; Claude, 2026-11-02; docs/SHIP.md).

UnrealEditor-Cmd <uproject> -ExecutePythonScript=<abs path>/Tools/create_ship_flag.py -unattended -nosplash -NoLiveCoding
(not -nullrhi: the texture import needs Slate)

The flag mesh (ShipSetting.cpp, FlagPoint in ShipDeck.inl) is a 150 x 95 cm sheet, UV0 in 150 cm units: U 0..1 from
the hoist to the fly, V 0..0.633 from the head down. The picture fills it: the hoist at its left edge. Two-sided
foliage shading as the sails (M_ShipSail), so the sun shows through the cloth from behind; both faces read the same
UV, so from the far side the picture is seen mirrored, as on a real flag.
"""
import os
import unreal

L = unreal.MaterialEditingLibrary
A = unreal.EditorAssetLibrary
T = unreal.AssetToolsHelpers.get_asset_tools()
DEST = '/Game/Art/Ship'
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
SOURCE = os.path.join(ROOT, 'SourceAssets', 'Setting', 'Ship', 'ShipFlag_Moustache.jpg')

task = unreal.AssetImportTask()
task.filename = SOURCE
task.destination_path = DEST
task.destination_name = 'T_ShipFlag'
task.replace_existing = True
task.automated = True
task.save = False
T.import_asset_tasks([task])
tex = unreal.load_asset(DEST + '/T_ShipFlag')
if not tex:
    raise RuntimeError('Flag texture import failed: ' + SOURCE)
tex.set_editor_property('srgb', True)
tex.set_editor_property('address_x', unreal.TextureAddress.TA_CLAMP)
tex.set_editor_property('address_y', unreal.TextureAddress.TA_CLAMP)
A.save_loaded_asset(tex, only_if_is_dirty=False)

NAME = 'M_ShipFlag'
m = unreal.load_asset(DEST + '/' + NAME) or T.create_asset(NAME, DEST, unreal.Material, unreal.MaterialFactoryNew())
L.delete_all_material_expressions(m)
m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
m.set_editor_property('two_sided', False)


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


uv = node(unreal.MaterialExpressionTextureCoordinate, coordinate_index=0, u_tiling=1.0, v_tiling=150.0 / 95.0)
sample = node(unreal.MaterialExpressionTextureSample, texture=tex)
link(uv, sample, 'UVs')
prop(sample, unreal.MaterialProperty.MP_BASE_COLOR, 'RGB')
through = node(unreal.MaterialExpressionMultiply, const_b=0.5)
link(sample, through, 'A', 'RGB')
prop(through, unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
rough = node(unreal.MaterialExpressionConstant, r=0.9)
prop(rough, unreal.MaterialProperty.MP_ROUGHNESS)
spec = node(unreal.MaterialExpressionConstant, r=0.2)
prop(spec, unreal.MaterialProperty.MP_SPECULAR)
L.recompile_material(m)
if not A.save_loaded_asset(m, only_if_is_dirty=False):
    raise RuntimeError('Save failed ' + NAME)
unreal.log('CHUCK_SHIP_FLAG_CREATED ' + DEST + '/' + NAME)
