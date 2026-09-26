"""Verified CC0 texture import and world-aligned stone/timber material connections."""
from pathlib import Path
import hashlib
import json
import unreal

ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'SourceAssets/Surfaces/PolyHaven'
manifest=json.loads((SOURCE/'manifest.json').read_text())
TOOLS=unreal.AssetToolsHelpers.get_asset_tools()
LIB=unreal.MaterialEditingLibrary
textures={}
for record in manifest['assets']:
    source=SOURCE/record['file']
    if hashlib.sha256(source.read_bytes()).hexdigest().upper()!=record['sha256']:
        raise RuntimeError(f'Texture source hash mismatch: {source.name}')
    name='T_'+source.stem
    task=unreal.AssetImportTask()
    task.filename=str(source); task.destination_path='/Game/Art/Textures/PolyHaven'
    task.destination_name=name; task.automated=True; task.replace_existing=True; task.save=True
    TOOLS.import_asset_tasks([task])
    texture=unreal.load_asset(task.destination_path+'/'+name)
    if not isinstance(texture,unreal.Texture2D): raise RuntimeError(f'Not a texture: {name}')
    role=record['map']
    texture.set_editor_property('srgb',role=='diff')
    texture.set_editor_property('compression_settings',{
        'diff':unreal.TextureCompressionSettings.TC_DEFAULT,
        'arm':unreal.TextureCompressionSettings.TC_MASKS,
        'nor_dx':unreal.TextureCompressionSettings.TC_NORMALMAP}[role])
    if texture.blueprint_get_size_x()!=2048 or texture.blueprint_get_size_y()!=2048:
        raise RuntimeError(f'Unexpected resolution: {name}')
    unreal.EditorAssetLibrary.save_loaded_asset(texture,only_if_is_dirty=False)
    textures[(record['asset'],role)]=texture

def node(mat,cls,**properties):
    result=LIB.create_material_expression(mat,cls)
    for key,value in properties.items(): result.set_editor_property(key,value)
    return result

def inputs(names):
    result=[]
    for name in names:
        item=unreal.CustomInput(); item.set_editor_property('input_name',name); result.append(item)
    return result

def connect(a,b,pin):
    if not LIB.connect_material_expressions(a,'',b,pin): raise RuntimeError(f'Cannot connect {pin}')

SAMPLE=r'''
float3 n=normalize(N), s=sign(n);
float3 w=pow(abs(n),4); w/=max(dot(w,float3(1,1,1)),.0001);
float2 ux=P.yz*float2(s.x,1)/TILE;
float2 uy=P.xz*float2(-s.y,1)/TILE;
float2 uz=P.xy*float2(s.z,1)/TILE;
if(WOOD) {
 // Use the interior of one scanned plank to avoid false joints crossing meshes.
 ux.x=frac(ux.x*8)*.1+.015; uy.x=frac(uy.x*8)*.1+.015; uz.x=frac(uz.x*8)*.1+.015;
}
float3 x=Texture2DSample(Tex,TexSampler,ux).rgb;
float3 y=Texture2DSample(Tex,TexSampler,uy).rgb;
float3 z=Texture2DSample(Tex,TexSampler,uz).rgb;
if(NORMAL) {
 float2 nx=x.rg*2-1, ny=y.rg*2-1, nz=z.rg*2-1;
 float3 perturb=float3(0,nx.x*s.x,nx.y)*w.x+float3(-ny.x*s.y,0,ny.y)*w.y+float3(nz.x*s.z,nz.y,0)*w.z;
 perturb-=n*dot(n,perturb);
 return normalize(n+perturb*.65);
}
return x*w.x+y*w.y+z*w.z;
'''
for name,asset,tile,wood,tint in [('Stone','grey_stone_path',180,False,(.82,.84,.83)),
                                   ('Wood','brown_planks_03',100,True,(.62,.52,.41)),
                                   ('WoodLight','brown_planks_03',100,True,(.83,.72,.58))]:
    material=unreal.load_asset(f'/Game/Art/Materials/M_{name}')
    if not material: raise RuntimeError(f'Generate art materials first: {name}')
    material.set_editor_property('tangent_space_normal',False)
    position=node(material,unreal.MaterialExpressionWorldPosition)
    normal=node(material,unreal.MaterialExpressionVertexNormalWS)
    samples={}
    for role in ('diff','arm','nor_dx'):
        obj=node(material,unreal.MaterialExpressionTextureObject,texture=textures[(asset,role)],sampler_type={
            'diff':unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,
            'arm':unreal.MaterialSamplerType.SAMPLERTYPE_MASKS,
            'nor_dx':unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL}[role])
        code=SAMPLE.replace('TILE',str(tile)).replace('WOOD','1' if wood else '0').replace('NORMAL','1' if role=='nor_dx' else '0')
        sample=node(material,unreal.MaterialExpressionCustom,code=code,output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3,inputs=inputs(('Tex','P','N')))
        connect(obj,sample,'Tex'); connect(position,sample,'P'); connect(normal,sample,'N')
        samples[role]=sample
    color=node(material,unreal.MaterialExpressionConstant3Vector,constant=unreal.LinearColor(*tint,1))
    multiply=node(material,unreal.MaterialExpressionMultiply)
    connect(samples['diff'],multiply,'A'); connect(color,multiply,'B')
    LIB.connect_material_property(multiply,'',unreal.MaterialProperty.MP_BASE_COLOR)
    for channel,prop in [('r',unreal.MaterialProperty.MP_AMBIENT_OCCLUSION),('g',unreal.MaterialProperty.MP_ROUGHNESS),('b',unreal.MaterialProperty.MP_METALLIC)]:
        mask=node(material,unreal.MaterialExpressionComponentMask,r=channel=='r',g=channel=='g',b=channel=='b',a=False)
        connect(samples['arm'],mask,'')
        LIB.connect_material_property(mask,'',prop)
    LIB.connect_material_property(samples['nor_dx'],'',unreal.MaterialProperty.MP_NORMAL)
    LIB.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material,only_if_is_dirty=False)
unreal.log('CHUCK_SCANNED_SURFACES_READY')
