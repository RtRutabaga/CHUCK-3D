"""Temple trap-run material (Claude, 2026-10-22; docs/CHULT-TEMPLE-RUN.md).

M_TempleBlade: dark forged steel for the great axes' blades and the saw discs, with fine
pitting, a little rust in patches and smudged polish. Its pattern is on the mesh's own UVs
(the moving parts' generated UVs, 25 cm a tile), not world-projected, so it doesn't slide
over a swinging blade or a spinning disc. Metallic, so it catches the torchlight.

Creates or rebuilds only this asset in /Game/Art/ChultTemple; imports nothing.
UnrealEditor-Cmd <project> -run=pythonscript -script=Tools/create_temple_run_materials.py -unattended -nullrhi -nosplash
"""
import unreal

TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
LIB = unreal.MaterialEditingLibrary
DEST = '/Game/Art/ChultTemple'
NAME = 'M_TempleBlade'

m = unreal.load_asset(DEST + '/' + NAME) or TOOLS.create_asset(NAME, DEST, unreal.Material, unreal.MaterialFactoryNew())
LIB.delete_all_material_expressions(m)
m.set_editor_property('two_sided', True)
uv = LIB.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate)
field = LIB.create_material_expression(m, unreal.MaterialExpressionCustom)
arg = unreal.CustomInput(); arg.set_editor_property('input_name', 'UV')
field.set_editor_property('inputs', [arg])
field.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT4)
field.set_editor_property('code', r'''
struct Noise { float hash(float2 p){return frac(sin(dot(p,float2(127.1,311.7)))*43758.5453);}
 float noise(float2 p){float2 i=floor(p),f=frac(p);f=f*f*(3-2*f);return lerp(lerp(hash(i),hash(i+float2(1,0)),f.x),lerp(hash(i+float2(0,1)),hash(i+1),f.x),f.y);} }; Noise n;
float2 q=UV*3.;
float pit=n.noise(q*9.)*.6+n.noise(q*23.)*.4;
float smudge=n.noise(q*.7)*.7+n.noise(q*2.1)*.3;
float rust=smoothstep(.68,.9,n.noise(q*.55+7.3)*.55+n.noise(q*1.9+3.1)*.27+n.noise(q*5.3)*.18);
float3 steel=float3(.055,.058,.062)*(.75+.25*smudge)*(.85+.15*pit);
float3 c=lerp(steel,float3(.085,.042,.02)*(.7+.3*pit),rust);
float rough=lerp(.46+.2*(1-smudge)+.1*pit,.92,rust);
return float4(c,rough);''')
LIB.connect_material_expressions(uv, '', field, 'UV')
rgb = LIB.create_material_expression(m, unreal.MaterialExpressionComponentMask)
rgb.set_editor_property('r', True); rgb.set_editor_property('g', True); rgb.set_editor_property('b', True); rgb.set_editor_property('a', False)
LIB.connect_material_expressions(field, '', rgb, '')
LIB.connect_material_property(rgb, '', unreal.MaterialProperty.MP_BASE_COLOR)
rough = LIB.create_material_expression(m, unreal.MaterialExpressionComponentMask)
rough.set_editor_property('r', False); rough.set_editor_property('g', False); rough.set_editor_property('b', False); rough.set_editor_property('a', True)
LIB.connect_material_expressions(field, '', rough, '')
LIB.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
metal = LIB.create_material_expression(m, unreal.MaterialExpressionConstant)
metal.set_editor_property('r', .75)
LIB.connect_material_property(metal, '', unreal.MaterialProperty.MP_METALLIC)
LIB.recompile_material(m)
if not unreal.EditorAssetLibrary.save_loaded_asset(m, only_if_is_dirty=False):
    raise RuntimeError(NAME)
unreal.log('CHUCK_TEMPLE_RUN_MATERIAL_READY ' + NAME)
