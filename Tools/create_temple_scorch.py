"""M_TempleScorch: the Fireball's soot and singe, a deferred decal (Claude, 2026-11-04; docs/CHULT-TEMPLE-RUBBLE.md).

UnrealEditor-Cmd <uproject> -run=pythonscript -script=Tools/create_temple_scorch.py -unattended -nullrhi -nosplash

Projected by the rubble map (ChultTempleInterior.cpp, the fallen hall's FScorch list) onto the floor, the rubble and the
serpent idols where the Fireball's blast reached. Soot: near-black brown, matte and without sheen, its cover falling off from the middle
of the decal to nothing at its edge, broken up by layered noise so the edge reads as smoke-licked, not a disc. Streaks
run along the decal's U (laid out from the blast) so the singe reads as thrown outward. Per instance: Strength (0..1,
how much soot), Seed (varies the noise). Translucent decal: colour, roughness and specular. One custom node with its noise inline: a struct-based custom node
feeding a decal's colour drew nothing, and the standard Noise node speckles at decal scale. Creates or rebuilds only this asset
in /Game/Art/ChultTemple; imports nothing.
"""
import unreal

TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
LIB = unreal.MaterialEditingLibrary
DEST = '/Game/Art/ChultTemple'
NAME = 'M_TempleScorch'

m = unreal.load_asset(DEST + '/' + NAME) or TOOLS.create_asset(NAME, DEST, unreal.Material, unreal.MaterialFactoryNew())
LIB.delete_all_material_expressions(m)
m.set_editor_property('material_domain', unreal.MaterialDomain.MD_DEFERRED_DECAL)
m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)


def node(cls, **props):
    n = LIB.create_material_expression(m, cls)
    for k, v in props.items(): n.set_editor_property(k, v)
    return n


uv = node(unreal.MaterialExpressionTextureCoordinate)
strength = node(unreal.MaterialExpressionScalarParameter, parameter_name='Strength', default_value=1.0)
seed = node(unreal.MaterialExpressionScalarParameter, parameter_name='Seed', default_value=0.0)


# One custom node, its noise written inline (no helper struct: a struct-based custom node feeding the decal's colour
# drew nothing, and the standard Noise node speckles at decal scale). rgb the soot's colour, a its cover: the decal's
# radial falloff broken up by value noise and a streak along U, eased; brown at the fringe to near-black at the core.
field = node(unreal.MaterialExpressionCustom)
ins = []
for key in ('UV', 'S', 'K'):
    i = unreal.CustomInput(); i.set_editor_property('input_name', key); ins.append(i)
field.set_editor_property('inputs', ins)
field.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT4)
field.set_editor_property('code', r"""
float2 c=UV-.5;
float r=length(c)*2;
float f=0,st=0,amp=.5;
float2 q=UV*float2(5,9)+K*7.31, w=float2(UV.x*2.5,UV.y*22)+K*3.7;
for(int i=0;i<5;i++)
{
  float2 a=floor(q),b=frac(q);b=b*b*(3-2*b);
  float h00=frac(sin(dot(a,float2(127.1,311.7)))*43758.5453),h10=frac(sin(dot(a+float2(1,0),float2(127.1,311.7)))*43758.5453);
  float h01=frac(sin(dot(a+float2(0,1),float2(127.1,311.7)))*43758.5453),h11=frac(sin(dot(a+float2(1,1),float2(127.1,311.7)))*43758.5453);
  f+=amp*lerp(lerp(h00,h10,b.x),lerp(h01,h11,b.x),b.y);
  float2 a2=floor(w),b2=frac(w);b2=b2*b2*(3-2*b2);
  float g00=frac(sin(dot(a2,float2(269.5,183.3)))*43758.5453),g10=frac(sin(dot(a2+float2(1,0),float2(269.5,183.3)))*43758.5453);
  float g01=frac(sin(dot(a2+float2(0,1),float2(269.5,183.3)))*43758.5453),g11=frac(sin(dot(a2+float2(1,1),float2(269.5,183.3)))*43758.5453);
  st+=amp*lerp(lerp(g00,g10,b2.x),lerp(g01,g11,b2.x),b2.y);
  q*=2.03;w*=2.03;amp*=.5;
}
float cover=saturate((1-r)*1.7+(f-.5)*1.1+(st-.5)*.6);
cover=smoothstep(.12,.9,cover);
// (Near-black at the core: the temple's stone is itself dark, so soot must be much darker to read.)
float3 soot=float3(.0035,.003,.0025),brown=float3(.085,.04,.016);   // the fringe heat-tinted
return float4(lerp(brown,soot,saturate(cover*1.6+(f-.5)*.4)),saturate(cover*S));
""")
LIB.connect_material_expressions(uv, '', field, 'UV')
LIB.connect_material_expressions(strength, '', field, 'S')
LIB.connect_material_expressions(seed, '', field, 'K')
colour = node(unreal.MaterialExpressionComponentMask, r=True, g=True, b=True, a=False)
opacity = node(unreal.MaterialExpressionComponentMask, r=False, g=False, b=False, a=True)
LIB.connect_material_expressions(field, '', colour, '')
LIB.connect_material_expressions(field, '', opacity, '')
rough = node(unreal.MaterialExpressionConstant, r=1.0)
spec = node(unreal.MaterialExpressionConstant, r=0.0)   # (soot kills the stone's sheen: on this dark stone that is most of what reads)
LIB.connect_material_property(colour, '', unreal.MaterialProperty.MP_BASE_COLOR)
LIB.connect_material_property(opacity, '', unreal.MaterialProperty.MP_OPACITY)
LIB.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
LIB.connect_material_property(spec, '', unreal.MaterialProperty.MP_SPECULAR)
LIB.recompile_material(m)
if not unreal.EditorAssetLibrary.save_loaded_asset(m, only_if_is_dirty=False):
    raise RuntimeError(NAME)
unreal.log('CHUCK_TEMPLE_SCORCH_READY')
