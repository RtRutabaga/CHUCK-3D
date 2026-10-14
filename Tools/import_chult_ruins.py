"""Verified CC0 ruin-only import/material pass; never rebuild shared assets."""
from pathlib import Path
import hashlib, json
import unreal
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'SourceAssets/Setting/ChultRuins'
DEST='/Game/Art/ChultRuins'
TOOLS=unreal.AssetToolsHelpers.get_asset_tools()
LIB=unreal.MaterialEditingLibrary
ASSETS=unreal.EditorAssetLibrary
helper=Path(__file__).with_name('import_fall_cutscene.py')
scope={'__file__':str(helper),'__name__':'ruin_material_helpers'}
exec(compile(helper.read_text(encoding='utf-8').split('# Music:')[0],str(helper),'exec'),scope)
node,const,colour,mul,lerp,sample,out,link,mask=[scope[k] for k in ('node','const','colour','mul','lerp','sample','out','link','mask')]
textures={}
for rec in json.loads((SOURCE/'manifest.json').read_text(encoding='utf-8-sig'))['assets']:
    file=SOURCE/rec['file']
    if hashlib.sha256(file.read_bytes()).hexdigest().upper()!=rec['sha256']: raise RuntimeError('Hash '+file.name)
    task=unreal.AssetImportTask();task.filename=str(file);task.destination_path=DEST
    task.destination_name='T_'+file.stem;task.automated=True;task.replace_existing=True;task.save=False
    TOOLS.import_asset_tasks([task]);tex=unreal.load_asset(DEST+'/'+task.destination_name)
    role=rec['role'];tex.set_editor_property('srgb',role=='Diffuse')
    tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP if role=='nor_dx' else unreal.TextureCompressionSettings.TC_MASKS if role=='arm' else unreal.TextureCompressionSettings.TC_DEFAULT)
    # Saving waits for the asynchronous rebuild triggered by compression changes.
    ASSETS.save_loaded_asset(tex,only_if_is_dirty=False)
    if tex.blueprint_get_size_x()!=2048 or tex.blueprint_get_size_y()!=2048:raise RuntimeError('Expected 2K '+file.name+' got '+str((tex.blueprint_get_size_x(),tex.blueprint_get_size_y())))
    textures[rec['asset'],role]=tex

def custom(m,code,inputs,kind=unreal.CustomMaterialOutputType.CMOT_FLOAT3):
    args=[]
    for name in inputs:
        i=unreal.CustomInput();i.set_editor_property('input_name',name);args.append(i)
    c=node(m,unreal.MaterialExpressionCustom,code=code,inputs=args,output_type=kind)
    for name,expr in inputs.items():link(expr,c,name)
    return c

# Same triplanar normal projection as the verified existing stone/timber finish.
SAMPLE=r'''
float3 n=normalize(N),s=sign(n),w=pow(abs(n),4);w/=max(dot(w,float3(1,1,1)),.0001);
float2 ux=P.yz*float2(s.x,1)/360.,uy=P.xz*float2(-s.y,1)/360.,uz=P.xy*float2(s.z,1)/360.;
float3 x=Texture2DSample(Tex,TexSampler,ux).rgb,y=Texture2DSample(Tex,TexSampler,uy).rgb,z=Texture2DSample(Tex,TexSampler,uz).rgb;
if(NORMAL) {
float2 nx=x.rg*2-1,ny=y.rg*2-1,nz=z.rg*2-1;
float3 perturb=float3(0,nx.x*s.x,nx.y)*w.x+float3(-ny.x*s.y,0,ny.y)*w.y+float3(nz.x*s.z,nz.y,0)*w.z;
perturb-=n*dot(n,perturb);return normalize(n+perturb*.5);
}return x*w.x+y*w.y+z*w.z;
'''
def finish(name,heavy):
    m=unreal.load_asset(DEST+'/'+name) or TOOLS.create_asset(name,DEST,unreal.Material,unreal.MaterialFactoryNew())
    LIB.delete_all_material_expressions(m);m.set_editor_property('tangent_space_normal',False)
    p=node(m,unreal.MaterialExpressionWorldPosition);n=node(m,unreal.MaterialExpressionVertexNormalWS)
    maps={}
    for asset in ('rock_wall_08','mossy_stone_wall','rabdentse_ruins_wall'):
        for role in ('Diffuse','arm','nor_dx'):
            typ={'Diffuse':unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,'arm':unreal.MaterialSamplerType.SAMPLERTYPE_MASKS,'nor_dx':unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL}[role]
            obj=node(m,unreal.MaterialExpressionTextureObject,texture=textures[asset,role],sampler_type=typ)
            maps[asset,role]=custom(m,SAMPLE.replace('NORMAL','1' if role=='nor_dx' else '0'),{'Tex':obj,'P':p,'N':n})
    noise=node(m,unreal.MaterialExpressionNoise,scale=.011,levels=3,quality=1,output_min=0.,output_max=1.);link(p,noise,'')
    fine=node(m,unreal.MaterialExpressionNoise,scale=.13,levels=2,quality=1,output_min=0.,output_max=1.);link(p,fine,'')
    wear=custom(m,'return smoothstep(.25,.8,V)*.42;',{'V':noise},unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    stone=lerp(m,maps['rock_wall_08','Diffuse'],maps['rabdentse_ruins_wall','Diffuse'],wear)
    stone=custom(m,'float l=dot(S,float3(.2126,.7152,.0722));return lerp(S,l*float3(.88,.94,1.0),.88)*.82;',{'S':stone})
    coverage=custom(m,'''float low=1-smoothstep(0,500,P.z);
float patch=smoothstep(.29,.69,V+low*.13+max(N.z,0)*.12+HEAVY);
return saturate(patch*(.55+F*.4));'''.replace('HEAVY','.18' if heavy else '.04'),{'V':noise,'F':fine,'N':n,'P':custom(m,'return float3(P.xy,P.z);',{'P':p})},unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    # Floor origin Z is zero; patch placement is tied to world geometry, never WPO.
    moss=custom(m,'float t=dot(S,float3(.21,.72,.07));return lerp(float3(.022,.045,.008),float3(.14,.22,.055),saturate(t*2.2));',{'S':maps['mossy_stone_wall','Diffuse']})
    out(m,lerp(m,stone,moss,coverage),unreal.MaterialProperty.MP_BASE_COLOR)
    mixedn=lerp(m,lerp(m,maps['rock_wall_08','nor_dx'],maps['rabdentse_ruins_wall','nor_dx'],wear),maps['mossy_stone_wall','nor_dx'],mul(m,coverage,const(m,.55)))
    norm=node(m,unreal.MaterialExpressionNormalize);link(mixedn,norm,'');out(m,norm,unreal.MaterialProperty.MP_NORMAL)
    arm=lerp(m,maps['rock_wall_08','arm'],maps['rabdentse_ruins_wall','arm'],wear)
    out(m,mask(m,arm,r=True),unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    rough=custom(m,'return lerp(clamp(R,.48,.86),.94,M);',{'R':mask(m,arm,g=True),'M':coverage},unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    out(m,rough,unreal.MaterialProperty.MP_ROUGHNESS);out(m,const(m,.26),unreal.MaterialProperty.MP_SPECULAR)
    LIB.recompile_material(m);ASSETS.save_loaded_asset(m,only_if_is_dirty=False)
    unreal.log('CHUCK_RUIN_MATERIAL '+name+' heavy='+str(heavy)+' gray=1 world_aligned=1 displacement=0')
finish('M_RuinStone',False);finish('M_RuinMoss',True)
unreal.log('CHUCK_RUIN_SCANS_IMPORTED textures=9 materials=2 resolution=2048')
