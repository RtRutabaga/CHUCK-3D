"""Rebuild only three materials owned by the sewer cave pass; no shared art edits."""
import unreal

lib = unreal.MaterialEditingLibrary
dest = '/Game/Art/Materials'

NOISE = r'''
struct Field {
 float hash(float3 p) { return frac(sin(dot(p,float3(127.1,311.7,74.7)))*43758.5453); }
 float noise(float3 p) {
  float3 i=floor(p),f=frac(p); f=f*f*(3-2*f);
  return lerp(lerp(lerp(hash(i),hash(i+float3(1,0,0)),f.x),lerp(hash(i+float3(0,1,0)),hash(i+float3(1,1,0)),f.x),f.y),
              lerp(lerp(hash(i+float3(0,0,1)),hash(i+float3(1,0,1)),f.x),lerp(hash(i+float3(0,1,1)),hash(i+float3(1,1,1)),f.x),f.y),f.z);
 }
 float fbm(float3 p) { return .53*noise(p)+.27*noise(p*2.03+7.3)+.13*noise(p*4.17+19.1)+.07*noise(p*8.23+31.7); }
 float stars(float2 p,float seed) {
  float2 c=floor(p),f=frac(p);
  float h=hash(float3(c,seed));
  float2 at=float2(hash(float3(c,seed+2)),hash(float3(c,seed+7)))*.7+.15;
  float d=length(f-at),aa=max(length(fwidth(p))*.65,.008);
  return step(.96,h)*(1-smoothstep(.012,.012+aa,d))*(.35+.65*h);
 }
};
Field n;
'''

ROCK = r'''
float3 p=P*.017;
float broad=n.fbm(p*.24),fine=n.fbm(p*3.8);
float layer=P.z*.043+n.fbm(p*.47)*4.8+P.x*.0018;
float bedding=abs(sin(layer));
float seam=1-smoothstep(.018,.09,bedding);
float fissure=(1-smoothstep(.008,.022,abs(n.fbm(p*.72)-.49)))*smoothstep(.38,.62,broad);
float damp=1-smoothstep(-897,-775,P.z);
float3 col=lerp(float3(.10,.112,.13),float3(.22,.205,.18),broad);
col*=lerp(.65,1.12,fine)*(1-seam*.18)*(1-fissure*.14)*(1-damp*.24);
float relief=fine*.4+broad*.6-seam*.05-fissure*.025;
return float4(col,relief);
'''
SPACE = r'''
// Layered cloud fields and sparse jittered stars, not periodic bands or a portal.
float2 q=P.xy*.009+P.z*float2(.0031,.0047);
float3 a=float3(q+V.xy*.35,T*.006);
float w=n.fbm(a*.54);
float cloud=n.fbm(a*1.15+float3(w*2.4,w*1.7,8));
float ridge=smoothstep(.34,.75,cloud);
float dust=smoothstep(.36,.58,n.fbm(a*2.6+21));
dust=lerp(dust,.6,smoothstep(.08,.35,length(fwidth(a.xy*2.6))));   // too fine for the screen far off: no blocky aliasing
float3 col=float3(.003,.006,.021);
col+=ridge*float3(.045,.07,.20);
col+=pow(saturate(cloud*1.4-.35),2)*float3(.34,.055,.29);
col+=smoothstep(.55,.79,n.fbm(a*.8+float3(31,5,9)))*float3(.08,.16,.23);
col*=lerp(.38,1,dust)*1.15;
float2 sp=(q+V.xy*.7)*21,sp2=(q+V.xy*.2)*39;
// Seen from far (a broad rupture from a gallery), star cells shrink to a few pixels and crowd into speckle; there they
// hand over to cells four times larger, so the star field keeps the spacing it has close up (user, 2026-10-24).
float fw=length(fwidth(sp)),near=1-smoothstep(.06,.16,fw),mid=1-smoothstep(.3,.65,fw*.25);
float stars=near*(n.stars(sp,4)+n.stars(sp2,18)*.35)+(1-near)*mid*(n.stars(sp*.25,41)+n.stars(sp2*.25,57)*.35);
col+=stars*float3(.75,.83,1.0);
return float4(col,1);
'''
WATER = r'''
float u=UV.x,v=UV.y-T*.24;
float ripple=sin(v*25+sin(u*11+v*4)*1.7)*.55+sin(v*43-u*17+T*.17)*.22;
float broken=n.noise(float3(u*17,v*5,0));
float glint=pow(saturate(ripple*.5+.5),8)*(.3+.7*broken);
float edge=smoothstep(.28,.49,abs(u-.5));
float3 col=lerp(float3(.018,.034,.048),float3(.046,.085,.115),.5+ripple*.3);
col+=glint*float3(.028,.054,.072)+edge*broken*.012;
return float4(col,ripple*.25+.5);
'''

# Chult's Astral brightness against the eye's adaptation (see AstralDepth below); 1 = the sewer's fixed exposure.
ASTRAL_EXPOSURE = '1.0'

def node(mat, cls, **properties):
    result=lib.create_material_expression(mat,cls)
    for key,value in properties.items(): result.set_editor_property(key,value)
    return result

def custom(mat, code, sources, kind=unreal.CustomMaterialOutputType.CMOT_FLOAT4):
    inputs=[]
    for name in sources:
        item=unreal.CustomInput(); item.set_editor_property('input_name',name); inputs.append(item)
    result=node(mat,unreal.MaterialExpressionCustom,inputs=inputs,code=code,output_type=kind)
    for name,source in sources.items(): lib.connect_material_expressions(source,'',result,name)
    return result

def property_constant(mat, prop, value):
    expression=node(mat,unreal.MaterialExpressionConstant,r=value)
    lib.connect_material_property(expression,'',prop)

for name,code in [('SewerRock',ROCK),('SewerStream',WATER),('AstralDepth',SPACE)]:
    if name=='AstralDepth' and '-ChuckAmbientOnly' in unreal.SystemLibrary.get_command_line():
        continue
    if name!='AstralDepth' and '-ChuckAstralOnly' in unreal.SystemLibrary.get_command_line():
        continue
    mat=unreal.load_asset(dest+'/M_'+name)
    if mat:
        lib.delete_all_material_expressions(mat)
    else:
        mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_'+name,dest,unreal.Material,unreal.MaterialFactoryNew())
    mat.set_editor_property('two_sided',True)
    mat.set_editor_property('tangent_space_normal',False)
    mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT if name=='AstralDepth' else unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    if name=='SewerRock': lib.set_material_usage(mat,unreal.MaterialUsage.MATUSAGE_INSTANCED_STATIC_MESHES)
    world=node(mat,unreal.MaterialExpressionWorldPosition)
    normal=node(mat,unreal.MaterialExpressionVertexNormalWS)
    uv=node(mat,unreal.MaterialExpressionTextureCoordinate)
    time=node(mat,unreal.MaterialExpressionTime)
    view=node(mat,unreal.MaterialExpressionCameraVectorWS)
    source=world
    if name=='AstralDepth':
        # Chult lies 25 km from the origin (AFallCutscene::LandingSpot, 400000/2500000 cm). Fed its absolute world
        # position, the shader's 32-bit floats lose the stars there (a star cell's fraction steps by ~.03 against a
        # .012 dot) and the clouds coarsen, so the Astral looked flat next to the sewer's (user, 2026-10-24). Its
        # landing spot is taken off first, in the graph's large-world (double) maths, so Chult's ruptures see local
        # coordinates as the sewer's do; Waterdeep (near the origin) is unchanged.
        mask_y=node(mat,unreal.MaterialExpressionComponentMask,r=False,g=True,b=False,a=False)
        lib.connect_material_expressions(world,'',mask_y,'')
        chult=custom(mat,'return Y>1200000?1:0;',{'Y':mask_y},unreal.CustomMaterialOutputType.CMOT_FLOAT1)
        spot=node(mat,unreal.MaterialExpressionConstant3Vector,constant=unreal.LinearColor(400000,2500000,0,0))
        offset=node(mat,unreal.MaterialExpressionMultiply); lib.connect_material_expressions(spot,'',offset,'A'); lib.connect_material_expressions(chult,'',offset,'B')
        source=node(mat,unreal.MaterialExpressionSubtract); lib.connect_material_expressions(world,'',source,'A'); lib.connect_material_expressions(offset,'',source,'B')
    field=custom(mat,NOISE+code,{'P':source,'UV':uv,'T':time,'V':view})
    rgb=node(mat,unreal.MaterialExpressionComponentMask,r=True,g=True,b=True,a=False)
    alpha=node(mat,unreal.MaterialExpressionComponentMask,r=False,g=False,b=False,a=True)
    lib.connect_material_expressions(field,'',rgb,''); lib.connect_material_expressions(field,'',alpha,'')
    if name=='AstralDepth':
        # Waterdeep renders at a fixed exposure (r.DefaultFeature.AutoExposure=False); Chult adapts (ChultLanding's
        # histogram exposure, the temple's dark interior let it brighten a long way), so the self-lit Astral came out
        # pale lavender there. In Chult it is taken back off the eye's adaptation (EyeAdaptationInverse) to the sewer's
        # fixed level, ASTRAL_EXPOSURE times it, so it keeps the sewer's depth wherever it is seen.
        inverse=node(mat,unreal.MaterialExpressionEyeAdaptationInverse)
        level=custom(mat,'return lerp(1,'+ASTRAL_EXPOSURE+'*I,F);',{'I':inverse,'F':chult},unreal.CustomMaterialOutputType.CMOT_FLOAT1)
        held=node(mat,unreal.MaterialExpressionMultiply); lib.connect_material_expressions(rgb,'',held,'A'); lib.connect_material_expressions(level,'',held,'B')
        lib.connect_material_property(held,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    else:
        lib.connect_material_property(rgb,'',unreal.MaterialProperty.MP_BASE_COLOR)
    if name!='AstralDepth':
        # Dim grey-blue ambient floor keeps crevices legible without a cosmic wash.
        ambient=custom(mat,'return C*float3(.04,.045,.055);',{'C':rgb},unreal.CustomMaterialOutputType.CMOT_FLOAT3)
        lib.connect_material_property(ambient,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        bump=custom(mat,'float3 dx=ddx(P),dy=ddy(P),n=normalize(N);float3 r1=cross(dy,n),r2=cross(n,dx);float det=dot(dx,r1);float3 grad=sign(det)*(ddx(H)*r1+ddy(H)*r2)/max(abs(det),.00001);return normalize(n-grad*'+('1.5' if name=='SewerRock' else '1.1')+');',{'P':world,'N':normal,'H':alpha},unreal.CustomMaterialOutputType.CMOT_FLOAT3)
        lib.connect_material_property(bump,'',unreal.MaterialProperty.MP_NORMAL)
        property_constant(mat,unreal.MaterialProperty.MP_ROUGHNESS,.84 if name=='SewerRock' else .32)
        property_constant(mat,unreal.MaterialProperty.MP_SPECULAR,.12 if name=='SewerRock' else .05)
        property_constant(mat,unreal.MaterialProperty.MP_METALLIC,0)
    lib.recompile_material(mat)
    if not unreal.EditorAssetLibrary.save_loaded_asset(mat,only_if_is_dirty=False): raise RuntimeError(name)
    unreal.log('CHUCK_CAVE_MATERIAL_READY '+name)
