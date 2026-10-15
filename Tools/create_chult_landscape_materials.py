"""Create only two dedicated landscape materials from existing scans; no imports."""
from pathlib import Path
import unreal
helper = Path(__file__).with_name('import_fall_cutscene.py')
source = helper.read_text(encoding='utf-8')
if '# Music:' not in source:
    raise RuntimeError('Helper boundary changed')
s = {'__file__': str(helper), '__name__': 'landscape_material_helpers'}
exec(compile(source.split('# Music:')[0], str(helper), 'exec'), s)
s['DEST'] = '/Game/Art/ChultTerrain'
unreal.EditorAssetLibrary.make_directory(s['DEST'])
node, const, colour, mul, lerp, sample, out, material, link, mask = [s[k] for k in ('node','const','colour','mul','lerp','sample','out','material','link','mask')]

def custom(m, code, inputs, kind=unreal.CustomMaterialOutputType.CMOT_FLOAT3):
    args=[]
    for name in inputs:
        i=unreal.CustomInput();i.set_editor_property('input_name',name);args.append(i)
    c=node(m,unreal.MaterialExpressionCustom,code=code,inputs=args,output_type=kind)
    for name, expr in inputs.items(): link(expr,c,name)
    return c

# Sample a verified intact fissured patch within the existing natural boulder atlas. Mirrored patch sampling avoids atlas padding and any masonry image.
projection = r'''
float3 n=normalize(N),sg=sign(n),w=pow(abs(n),4);w/=max(dot(w,float3(1,1,1)),.0001);
float2 ux=P.yz*float2(sg.x,1)/480.,uy=P.xz*float2(-sg.y,1)/480.,uz=P.xy*float2(sg.z,1)/480.;
ux=abs(frac(ux*.5)*2-1)*float2(.11,.08)+float2(.405,.205);
uy=abs(frac(uy*.5)*2-1)*float2(.11,.08)+float2(.405,.205);
uz=abs(frac(uz*.5)*2-1)*float2(.11,.08)+float2(.405,.205);
float3 x=Texture2DSample(Tex,TexSampler,ux).rgb,y=Texture2DSample(Tex,TexSampler,uy).rgb,z=Texture2DSample(Tex,TexSampler,uz).rgb;
if(NORMAL){float2 nx=x.rg*2-1,ny=y.rg*2-1,nz=z.rg*2-1;
float3 p=float3(0,nx.x*sg.x,nx.y)*w.x+float3(-ny.x*sg.y,0,ny.y)*w.y+float3(nz.x*sg.z,nz.y,0)*w.z;
p-=n*dot(n,p);return normalize(n+p*.65);}return x*w.x+y*w.y+z*w.z;
'''
def cliff(m):
    m.set_editor_property('tangent_space_normal',False)
    p=node(m,unreal.MaterialExpressionWorldPosition); n=node(m,unreal.MaterialExpressionVertexNormalWS)
    maps={}
    for role,typ in [('diff',unreal.MaterialSamplerType.SAMPLERTYPE_COLOR),('nor_dx',unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL),('arm',unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)]:
        tex=unreal.load_asset('/Game/Art/ChultNature/T_rock_moss_set_01_'+role+'_2k')
        if tex is None: raise RuntimeError('Missing existing rock scan '+role)
        obj=node(m,unreal.MaterialExpressionTextureObject,texture=tex,sampler_type=typ)
        maps[role]=custom(m,projection.replace('NORMAL','1' if role=='nor_dx' else '0'),{'Tex':obj,'P':p,'N':n})
    noise=node(m,unreal.MaterialExpressionNoise,scale=.0025,levels=3,quality=1,output_min=0.,output_max=1.);link(p,noise,'')
    fine=node(m,unreal.MaterialExpressionNoise,scale=.033,levels=2,quality=1,output_min=0.,output_max=1.);link(p,fine,'')
    stone=custom(m,'float l=dot(S,float3(.21,.72,.07));return lerp(S,l*float3(.88,.83,.72),.35)*lerp(.52,.95,V);',{'S':maps['diff'],'V':noise})
    moss=custom(m,'return smoothstep(.43,.78,V+max(N.z,0)*.32)*(.3+.6*F);',{'V':noise,'N':n,'F':fine},unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    green=lerp(m,colour(m,.018,.033,.009),colour(m,.105,.15,.032),fine)
    out(m,lerp(m,stone,green,moss),unreal.MaterialProperty.MP_BASE_COLOR)
    out(m,maps['nor_dx'],unreal.MaterialProperty.MP_NORMAL)
    out(m,mask(m,maps['arm'],r=True),unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    out(m,lerp(m,const(m,.64),const(m,.94),moss),unreal.MaterialProperty.MP_ROUGHNESS)
    out(m,const(m,.23),unreal.MaterialProperty.MP_SPECULAR)

def falls(m):
    m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property('translucency_lighting_mode',unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    m.set_editor_property('two_sided',True)
    uv=node(m,unreal.MaterialExpressionTextureCoordinate); t=node(m,unreal.MaterialExpressionTime)
    ribbons=custom(m,r'''float u=UV.x,v=UV.y-T*1.8;
float2 p=float2(u*13+sin(v*2)*.8,v*5),i=floor(p),f=frac(p);f=f*f*(3-2*f); float4 h=frac(sin(float4(dot(i,float2(127.1,311.7)),dot(i+float2(1,0),float2(127.1,311.7)),dot(i+float2(0,1),float2(127.1,311.7)),dot(i+1,float2(127.1,311.7))))*43758.5453);float q=lerp(lerp(h.x,h.y,f.x),lerp(h.z,h.w,f.x),f.y);
return saturate(smoothstep(0,.12,u)*smoothstep(0,.12,1-u)*(.14+q*q*.8));''',{'UV':uv,'T':t},unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    vc=node(m,unreal.MaterialExpressionVertexColor)
    out(m,mul(m,ribbons,vc,'','A'),unreal.MaterialProperty.MP_OPACITY)
    out(m,lerp(m,colour(m,.21,.29,.27),colour(m,.72,.79,.75),ribbons),unreal.MaterialProperty.MP_BASE_COLOR)
    out(m,const(m,.26),unreal.MaterialProperty.MP_ROUGHNESS)
    out(m,const(m,.4),unreal.MaterialProperty.MP_SPECULAR)
    out(m,mul(m,colour(m,.12,.15,.14),ribbons),unreal.MaterialProperty.MP_EMISSIVE_COLOR)

material('M_ChultCliff',cliff)
material('M_ChultWaterfall',falls)
unreal.log('CHUCK_LANDSCAPE_MATERIALS_READY materials=2 imports=0 natural_rock_only=1')
