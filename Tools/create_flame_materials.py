"""Materials for the game's torches, braziers, fireplaces and lamps (Claude, 2026-10-27; docs/FIRE.md).

UnrealEditor-Cmd <uproject> -ExecutePythonScript=Tools/create_flame_materials.py -unattended -nosplash -NoLiveCoding

AChuckFlames (C++) draws every fire placed through AddDockFlame on instanced camera-facing quads, writing three
per-instance floats (frame, heat, opacity) as the temple's fire jets do (import_temple_fire.py, whose flipbook
shader this reuses unchanged). The textures are the CC0 Unity Labs flipbooks already imported there:
  M_ChuckFlame   Flame02 16x4 (T_JetPilotFlame), additive: the flame itself, rolling and licking upward
  M_ChuckEmber   procedural, additive: a spark that cools from yellow-white to a dull red as it rises
The smoke is the jets' own M_TempleJetSmoke (WispySmoke01).
"""
from pathlib import Path
import unreal

source = Path(__file__).with_name('import_temple_fire.py')
helpers = source.read_text(encoding='utf-8')
marker = "fire = import_texture("
if marker not in helpers:
    raise RuntimeError('import_temple_fire.py changed: its definitions end marker is gone')
scope = {'__file__': str(source), '__name__': 'temple_fire_helpers'}
exec(compile(helpers.split(marker)[0], str(source), 'exec'), scope)
build, node, link, LIB, TOOLS, ASSETS = (scope[k] for k in ('build', 'node', 'link', 'LIB', 'TOOLS', 'ASSETS'))

FIRE = '/Game/Art/ChultTemple/Fire'
DEST = '/Game/Art/Materials/Fire'
flame = unreal.load_asset(f'{FIRE}/T_JetPilotFlame')
if not flame or not unreal.load_asset(f'{FIRE}/M_TempleJetSmoke'):
    raise RuntimeError('Run Tools/import_temple_fire.py first (T_JetPilotFlame, M_TempleJetSmoke)')

# The temple helper writes into its own folder; build there under a new name, then move it.
scope['DEST'] = DEST
build('M_ChuckFlame', flame, 16, 4, 1, 2.6)

EMBER = r'''
float2 d = UV - .5;
float r = length(d) * 2.;
float core = pow(saturate(1. - r), 2.2);
float h = saturate(Heat);
float3 col = lerp(float3(1., .16, .02), float3(1., .72, .32), h) * (.6 + 3.4 * h * h);
return col * core * saturate(Opacity) * saturate((Depth - 20.) / 60.);
'''
name = 'M_ChuckEmber'
mat = unreal.load_asset(f'{DEST}/{name}') or TOOLS.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
LIB.delete_all_material_expressions(mat)
mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_ADDITIVE)
mat.set_editor_property('two_sided', True)
mat.set_editor_property('used_with_instanced_static_meshes', True)
uv = node(mat, unreal.MaterialExpressionTextureCoordinate)
data = [node(mat, unreal.MaterialExpressionPerInstanceCustomData, data_index=i) for i in range(3)]
depth = node(mat, unreal.MaterialExpressionPixelDepth)
field = node(mat, unreal.MaterialExpressionCustom)
inputs = []
for key in ('UV', 'Heat', 'Opacity', 'Depth'):
    item = unreal.CustomInput(); item.set_editor_property('input_name', key); inputs.append(item)
field.set_editor_property('inputs', inputs)
field.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
field.set_editor_property('code', EMBER)
for key, src in (('UV', uv), ('Heat', data[1]), ('Opacity', data[2]), ('Depth', depth)):
    link(src, field, key)
LIB.connect_material_property(field, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
LIB.recompile_material(mat)
if not ASSETS.save_loaded_asset(mat, only_if_is_dirty=False):
    raise RuntimeError(name)
unreal.log('CHUCK_FLAME_MATERIALS_READY')
