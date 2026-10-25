"""M_TempleRiftSea: the open sea seen through the sanctum's sea tear (Claude, 2026-10-24; docs/CHULT-TEMPLE-WIND.md).

UnrealEditor-Cmd <uproject> -run=pythonscript -script=Tools/create_temple_rift_sea.py -unattended -nullrhi -nosplash

The tear's well is lined by a box (ChultTempleWindBuild.inl, SanctumHoles), as the cellar's sky hole is
(Tools/create_pantry_deep_sky.py). Like that sky, this graph ignores where on the box a pixel is and colours it by
the view ray alone, so the walls and floor show one continuous view: the ray meets a sea plane at SeaZ (world cm,
set per instance: 15 m under the hall's floor) and is shaded as moderate open-sea waves under a pale daylight sky -
a sum of travelling deep-water waves (3.8 m down to about 25 cm, all from about one wind so they read as crest lines, their slopes - gentle, so the water reads deep rather than as a sheet of reflected sky - making the normal), Fresnel sky reflection
over deep blue-green water, light through the crests, a little sun glint and wind-streaked foam on the steepest
crests, then haze with distance. Rays that don't reach the sea (near the rim) see the sky.
Unlit, two-sided. Creates or rebuilds only this asset in /Game/Art/ChultTemple; imports nothing.
"""
import unreal

TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
LIB = unreal.MaterialEditingLibrary
DEST = '/Game/Art/ChultTemple'
NAME = 'M_TempleRiftSea'

m = unreal.load_asset(DEST + '/' + NAME) or TOOLS.create_asset(NAME, DEST, unreal.Material, unreal.MaterialFactoryNew())
LIB.delete_all_material_expressions(m)
m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
m.set_editor_property('two_sided', True)


def node(cls, **props):
    n = LIB.create_material_expression(m, cls)
    for k, v in props.items(): n.set_editor_property(k, v)
    return n


view = node(unreal.MaterialExpressionCameraVectorWS)
eye = node(unreal.MaterialExpressionCameraPositionWS)
clock = node(unreal.MaterialExpressionTime)
sea_z = node(unreal.MaterialExpressionScalarParameter, parameter_name='SeaZ', default_value=-3000.0)
field = node(unreal.MaterialExpressionCustom)
inputs = []
for key in ('V', 'E', 'T', 'Z'):
    i = unreal.CustomInput(); i.set_editor_property('input_name', key); inputs.append(i)
field.set_editor_property('inputs', inputs)
field.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
field.set_editor_property('code', r'''
struct Sea {
  float hash(float2 p){return frac(sin(dot(p,float2(127.1,311.7)))*43758.5453);}
  float noise(float2 p){float2 i=floor(p),f=frac(p);f=f*f*(3-2*f);return lerp(lerp(hash(i),hash(i+float2(1,0)),f.x),lerp(hash(i+float2(0,1)),hash(i+1),f.x),f.y);}
  float3 sky(float3 d){float h=saturate(d.z);return lerp(float3(.66,.74,.78),float3(.24,.43,.62),pow(h,.55));}
}; Sea s;
float3 v=-normalize(V);                      // from the eye along the view
if(v.z>-.015) return s.sky(v)*.7;
float t=(Z-E.z)/v.z;                         // to the sea plane
float2 p=E.xy+v.xy*t;
// Travelling deep-water waves: height h and its slope g (cm/cm).
float h=0,amp=15,wl=380,hmax=0;float2 g=0;
for(int i=0;i<10;i++)
{
  float a=.55+.75*sin(i*2.4+.3);float2 d=float2(cos(a),sin(a));   // all from about one wind: crest lines, not blotches
  float k=6.28318/wl,w=sqrt(980*k);
  float ph=dot(d,p)*k-w*T+i*2.3;
  // a little sharper crests than a sine (Gerstner-like): lean the phase toward the crest
  float c=sin(ph),q=cos(ph);
  h+=amp*(c+.18*c*c);g+=amp*k*q*(1+.36*c)*d;hmax+=amp*1.18;
  amp*=.6;wl*=.74;
}
float3 n=normalize(float3(-g,1));
float3 r=reflect(v,n);r.z=abs(r.z);
float fres=.02+.98*pow(1-saturate(dot(-v,n)),5);
float crest=saturate(h/hmax*.5+.5);
float3 deep=float3(.006,.035,.062),lit=float3(.02,.15,.17);
float3 body=lerp(deep,lit,smoothstep(.4,.95,crest))*(.75+.35*smoothstep(.2,.8,crest));
float3 col=lerp(body,s.sky(r)*.62,fres*.75);   // a muted reflection: seen low through the hole it should still read as water
float3 sun=normalize(float3(.5,.35,.79));
col+=float3(1,.96,.88)*(pow(saturate(dot(r,sun)),70)*.35+pow(saturate(dot(r,sun)),700)*5);
// foam on the steepest crests, streaked along the wind
float steep=length(g);
float streak=s.noise(p*float2(.025,.08)+float2(T*.3,0))*.5+s.noise(p*.11+3.1)*.3+s.noise(p*.27+7.7)*.2;
float foam=smoothstep(.8,.98,crest)*smoothstep(.5,.85,streak);
col=lerp(col,float3(.8,.86,.88),foam*.5);
// haze with distance toward the horizon
float haze=1-exp(-max(t-2500,0)/60000);
col=lerp(col,s.sky(float3(v.xy,.03))*.8,saturate(haze+pow(1+v.z,10)*.5));
return col*.7;   // (the dark hall's exposure brightens a small hole)
''')
LIB.connect_material_expressions(view, '', field, 'V')
LIB.connect_material_expressions(eye, '', field, 'E')
LIB.connect_material_expressions(clock, '', field, 'T')
LIB.connect_material_expressions(sea_z, '', field, 'Z')
LIB.connect_material_property(field, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
LIB.recompile_material(m)
if not unreal.EditorAssetLibrary.save_loaded_asset(m, only_if_is_dirty=False):
    raise RuntimeError(NAME)
unreal.log('CHUCK_TEMPLE_RIFT_SEA_READY')
