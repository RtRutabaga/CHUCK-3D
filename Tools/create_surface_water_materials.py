"""Own only fountain basin/jet and harbor water graphs; reuse existing UE Water textures.
No existing sewer, sky, fire, character or material graph is regenerated.
"""
import unreal
L=unreal.MaterialEditingLibrary
A=unreal.EditorAssetLibrary
T=unreal.AssetToolsHelpers.get_asset_tools()
textures={n:unreal.load_asset('/Game/Art/Textures/Water/'+n) for n in ('T_Water_TilingNormal_Waves_02','T_WaterFlow_01_Foam_Tiled','Caustics_Tiling_01_HDR')}
if not all(textures.values()): raise RuntimeError('Existing Water textures required; no automatic copy/download')
def node(m,c,**props):
 n=L.create_material_expression(m,c)
 for k,v in props.items(): n.set_editor_property(k,v)
 return n
def link(a,b,pin=''):
 if not L.connect_material_expressions(a,'',b,pin): raise RuntimeError('Link failed '+pin)
def c(m,x):return node(m,unreal.MaterialExpressionConstant,r=x)
def v(m,x,y,z):return node(m,unreal.MaterialExpressionConstant3Vector,constant=unreal.LinearColor(x,y,z,1))
def op(m,cls,a,b):
 n=node(m,cls);link(a,n,'A');link(b,n,'B');return n
def mul(m,a,b):return op(m,unreal.MaterialExpressionMultiply,a,b)
def add(m,a,b):return op(m,unreal.MaterialExpressionAdd,a,b)
def lerp(m,a,b,f):
 n=node(m,unreal.MaterialExpressionLinearInterpolate);link(a,n,'A');link(b,n,'B');link(f,n,'Alpha');return n
def tex(m,name,uv,normal=False):
 n=node(m,unreal.MaterialExpressionTextureSample,texture=textures[name],sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if normal else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
 if name=='Caustics_Tiling_01_HDR':n.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
 link(uv,n,'UVs');return n
def pan(m,xy,size,x,y):
 n=node(m,unreal.MaterialExpressionPanner,speed_x=x,speed_y=y);link(mul(m,xy,c(m,1/size)),n,'Coordinate');return n
def prop(m,n,p):
 if not L.connect_material_property(n,'',p):raise RuntimeError('Property connection failed '+str(p))
for kind in ('FountainBasinWater','FountainJetWater','HarborWater'):
 path='/Game/Art/Materials/M_'+kind
 m=unreal.load_asset(path) or T.create_asset('M_'+kind,'/Game/Art/Materials',unreal.Material,unreal.MaterialFactoryNew())
 L.delete_all_material_expressions(m)
 m.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
 m.set_editor_property('translucency_lighting_mode',unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
 m.set_editor_property('two_sided',True)
 m.set_editor_property('tangent_space_normal',False)
 m.set_editor_property('used_with_instanced_static_meshes',True)
 harbor=kind=='HarborWater';jet=kind=='FountainJetWater'
 m.set_editor_property('refraction_method',unreal.RefractionMode.RM_PIXEL_NORMAL_OFFSET if harbor or jet else unreal.RefractionMode.RM_INDEX_OF_REFRACTION)
 world=node(m,unreal.MaterialExpressionWorldPosition)
 xy=node(m,unreal.MaterialExpressionComponentMask,r=True,g=True,b=False,a=False);link(world,xy)
 n1=tex(m,'T_Water_TilingNormal_Waves_02',pan(m,xy,500 if harbor else 115,.025 if harbor else .04,.018),True)
 n2=tex(m,'T_Water_TilingNormal_Waves_02',pan(m,xy,1100 if harbor else 205,-.013,.009),True)
 flatten=mul(m,add(m,n1,n2),v(m,.28 if harbor else .16,.28 if harbor else .16,1))
 norm=node(m,unreal.MaterialExpressionNormalize);link(flatten,norm);prop(m,norm,unreal.MaterialProperty.MP_NORMAL)
 fres=node(m,unreal.MaterialExpressionFresnel,exponent=4.,base_reflect_fraction=.025);link(norm,fres,'Normal')
 depth=node(m,unreal.MaterialExpressionDepthFade,fade_distance_default=200. if harbor else 20.)
 shallow=node(m,unreal.MaterialExpressionDepthFade,fade_distance_default=65. if harbor else 8.)
 edge=node(m,unreal.MaterialExpressionOneMinus);link(shallow,edge)
 foamTex=tex(m,'T_WaterFlow_01_Foam_Tiled',pan(m,xy,140 if harbor else 80,.027,.02))
 foamR=node(m,unreal.MaterialExpressionComponentMask,r=True,g=False,b=False,a=False);link(foamTex,foamR)
 foam=mul(m,mul(m,foamR,edge),c(m,.24 if harbor else .12))
 if not harbor and not jet:
  ripple=node(m,unreal.MaterialExpressionCustom,output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT1,
    code='float2 P=XY-float2(260,-3320); float d=min(min(length(P-float2(154,0)),length(P+float2(154,0))),min(length(P-float2(0,154)),length(P+float2(0,154)))); return pow(saturate(1-abs(frac(d/14-Time*1.2)-.5)*2),10)*saturate(1-d/55)*.16;')
  inputs=[]
  for name in ('XY','Time'):
   inp=unreal.CustomInput();inp.set_editor_property('input_name',name);inputs.append(inp)
  ripple.set_editor_property('inputs',inputs)
  link(xy,ripple,'XY');link(node(m,unreal.MaterialExpressionTime),ripple,'Time');foam=add(m,foam,ripple)
 water=v(m,.012,.036,.044) if harbor else v(m,.025,.055,.061)
 col=lerp(m,water,v(m,.25,.29,.28),foam)
 if jet:col=v(m,.2,.27,.28)
 prop(m,col,unreal.MaterialProperty.MP_BASE_COLOR)
 prop(m,lerp(m,c(m,.14 if harbor else .12),c(m,.5),foam),unreal.MaterialProperty.MP_ROUGHNESS)
 prop(m,c(m,.4),unreal.MaterialProperty.MP_SPECULAR)
 body=mul(m,depth,c(m,.92 if harbor else .62))
 opacity=add(m,c(m,.12),body) if not jet else lerp(m,c(m,.25),c(m,.62),fres)
 prop(m,opacity,unreal.MaterialProperty.MP_OPACITY)
 prop(m,c(m,1.035 if harbor or jet else 1.33),unreal.MaterialProperty.MP_REFRACTION)
 if not harbor and not jet:
  caust=tex(m,'Caustics_Tiling_01_HDR',pan(m,xy,160,.03,.02))
  prop(m,mul(m,caust,v(m,.00012,.0002,.00022)),unreal.MaterialProperty.MP_EMISSIVE_COLOR)
 L.recompile_material(m)
 if not A.save_loaded_asset(m,only_if_is_dirty=False):raise RuntimeError('Save failed '+kind)
 unreal.log('CHUCK_SURFACE_WATER_CREATED '+kind)
