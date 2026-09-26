"""Original procedural surfaces, centimetre scale, no external texture dependencies.

Run with full Unreal editor -ExecutePythonScript. Rebuilds connections without
deleting expression objects that an already-loaded character may reference.
"""
import unreal
import os

DEST='/Game/Art/Materials'
LIB=unreal.MaterialEditingLibrary
tools=unreal.AssetToolsHelpers.get_asset_tools()
# color in linear space, surface family, roughness, relief in cm, metallic
SURFACES={
    'Fur':((.16,.145,.125),4,.9,.035,0),
    'Chest':((.33,.285,.22),4,.92,.03,0),
    'Jacket':((.075,.014,.145),3,.78,.018,0),
    'Seam':((.125,.033,.20),3,.83,.018,0),
    'Skin':((.32,.145,.12),5,.57,.015,0),
    'Eye':((.009,.006,.004),5,.12,0,0),
    'Claw':((.38,.32,.23),5,.38,.005,0),
    'Metal':((.22,.14,.048),6,.36,.015,.8),
    'Whisker':((.35,.32,.26),5,.8,0,0),
    'Stone':((.19,.20,.185),1,.8,.22,0),
    'Plaster':((.43,.375,.285),2,.93,.09,0),
    'Wood':((.13,.068,.029),0,.79,.035,0),
    'WoodLight':((.225,.135,.062),0,.76,.035,0),
    'Roof':((.075,.105,.12),1,.72,.09,0),
    'Dark':((.045,.052,.05),6,.46,.04,.65),
    'Water':((.017,.072,.092),7,.28,.55,.15),
    'Navy':((.035,.067,.085),3,.85,.02,0),
    'Amber':((.32,.155,.045),5,.4,0,0),
    'Sky':((.4,.55,.7),8,1,0,0),
    'Canvas':((.29,.255,.19),3,.91,.025,0),
    'Leather':((.075,.035,.018),5,.68,.025,0),
}
SHADER=r'''
struct SurfaceNoise {
 float hash(float3 p) { return frac(sin(dot(p,float3(127.1,311.7,74.7)))*43758.5453); }
 float noise(float3 p) {
  float3 i=floor(p), f=frac(p); f=f*f*(3-2*f);
  return lerp(lerp(lerp(hash(i),hash(i+float3(1,0,0)),f.x),lerp(hash(i+float3(0,1,0)),hash(i+float3(1,1,0)),f.x),f.y),
  lerp(lerp(hash(i+float3(0,0,1)),hash(i+float3(1,0,1)),f.x),lerp(hash(i+float3(0,1,1)),hash(i+float3(1,1,1)),f.x),f.y),f.z);
 }
 float fbm(float3 p) { return .57*noise(p)+.28*noise(p*2.07)+.15*noise(p*4.13); }
};
SurfaceNoise n;
float kind=KIND;
float h=n.fbm(P*.65), shade=.8+.35*h;
if(kind==0) {
 float3 q=P*float3(.48,.016,.48);
 float grain=.5+.5*sin(P.x*2.4+P.z*1.8+n.noise(q)*9);
 h=.65*n.fbm(q)+.35*grain;
 shade=.72+.30*h;
}
if(kind==1) {
 h=n.fbm(P*.36);
 float mineral=n.noise(P*4);
 shade=.68+.48*h+.08*mineral;
 // Broad dark damp patches, not an all-over gloss coating.
 shade*=lerp(.65,1,smoothstep(.28,.46,n.noise(P*.023)));
}
if(kind==2) { h=n.fbm(P*1.2); shade=.78+.32*h; }
if(kind==3) {
 float2 uv=P.xz*26;
 float attenuation=1-saturate(max(fwidth(uv.x),fwidth(uv.y))*.5);
 float weave=sin(uv.x)*sin(uv.y)*attenuation;
 h=.5+.18*weave+.2*(n.noise(P*1.5)-.5);
 shade=.8+.3*n.fbm(P*.6)+.10*weave;
}
if(kind==4) {
 float strands=.5+.5*sin(P.x*24+P.y*18+n.noise(P*float3(2,2,.18))*12);
 float aa=1-saturate(length(fwidth(P))*9);
 h=lerp(.5,strands,aa);
 shade=.65+.45*n.fbm(P*float3(1.4,1.4,.2))+.12*(h-.5);
}
if(kind==5) { h=n.noise(P*5); shade=.9+.13*n.noise(P*.8); }
if(kind==6) { h=n.fbm(P*2); shade=.7+.5*n.noise(P*.2); }
if(kind==7) {
 float warp=n.fbm(float3(P.xy*.028,T*.12))*5;
 float aa=1-saturate(length(fwidth(P.xy))*.025);
 h=.5+aa*(.18*sin(P.x*.055+P.y*.024+T*.8+warp)+.11*sin(P.y*.11-P.x*.023-T*1.1+warp));
 shade=.9+.1*n.fbm(P*.018);
}
if(kind==8) {
 float3 d=normalize(P);
 float horizon=saturate(d.z*1.8);
 float3 sky=lerp(float3(.58,.65,.66),float3(.11,.27,.44),horizon);
 float cloud=smoothstep(.54,.72,n.fbm(d*float3(5,5,18)))*smoothstep(.08,.35,d.z);
 return float4(lerp(sky,float3(.75,.76,.71),cloud*.55),.5);
}
return float4(Base*shade,h);
'''

def node(mat, cls, **props):
    result=LIB.create_material_expression(mat,cls)
    for key,value in props.items(): result.set_editor_property(key,value)
    return result

def wire(a, output, b, pin):
    if not LIB.connect_material_expressions(a,output,b,pin):
        raise RuntimeError(f'Cannot connect {output} to {pin}')

def custom_inputs(names):
    result=[]
    for name in names:
        item=unreal.CustomInput()
        item.set_editor_property('input_name',name)
        result.append(item)
    return result

for name,(color,kind,roughness,relief,metallic) in SURFACES.items():
    selected=os.environ.get('CHUCK_ART_ONLY','')
    if selected and name not in selected.split(','): continue
    mat=unreal.load_asset(f'{DEST}/M_{name}')
    if not mat: mat=tools.create_asset(f'M_{name}',DEST,unreal.Material,unreal.MaterialFactoryNew())
    character=name in ('Fur','Chest','Jacket','Seam','Skin','Eye','Claw','Metal','Whisker')
    if character: LIB.set_material_usage(mat,unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH)
    mat.set_editor_property('tangent_space_normal',False)
    pos=node(mat,unreal.MaterialExpressionPreSkinnedPosition if character else unreal.MaterialExpressionWorldPosition)
    if character:
        interpolated=node(mat,unreal.MaterialExpressionVertexInterpolator)
        wire(pos,'',interpolated,'VS')
        pos=interpolated
    time=node(mat,unreal.MaterialExpressionTime)
    base=node(mat,unreal.MaterialExpressionConstant3Vector,constant=unreal.LinearColor(*color,1))
    surface=node(mat,unreal.MaterialExpressionCustom,code=SHADER.replace('KIND',str(kind)),output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT4,
                 inputs=custom_inputs(('P','T','Base')))
    wire(pos,'',surface,'P'); wire(time,'',surface,'T'); wire(base,'',surface,'Base')
    rgb=node(mat,unreal.MaterialExpressionComponentMask,r=True,g=True,b=True,a=False)
    height=node(mat,unreal.MaterialExpressionComponentMask,r=False,g=False,b=False,a=True)
    wire(surface,'',rgb,''); wire(surface,'',height,'')
    LIB.connect_material_property(rgb,'',unreal.MaterialProperty.MP_BASE_COLOR)
    rough=node(mat,unreal.MaterialExpressionConstant,r=roughness)
    LIB.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
    metal=node(mat,unreal.MaterialExpressionConstant,r=metallic)
    LIB.connect_material_property(metal,'',unreal.MaterialProperty.MP_METALLIC)
    if relief:
        world=node(mat,unreal.MaterialExpressionWorldPosition)
        normal=node(mat,unreal.MaterialExpressionVertexNormalWS)
        bump=node(mat,unreal.MaterialExpressionCustom,output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3,
                  inputs=custom_inputs(('P','N','H')),
                  code=f'float3 dx=ddx(P),dy=ddy(P),n=normalize(N); float3 r1=cross(dy,n),r2=cross(n,dx); float det=dot(dx,r1); float3 grad=sign(det)*(ddx(H)*r1+ddy(H)*r2)/max(abs(det),0.00001); return normalize(n-grad*{relief});')
        wire(world,'',bump,'P'); wire(normal,'',bump,'N'); wire(height,'',bump,'H')
        LIB.connect_material_property(bump,'',unreal.MaterialProperty.MP_NORMAL)
    if name=='Sky':
        mat.set_editor_property('two_sided',True)
        mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
        LIB.connect_material_property(rgb,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    LIB.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat,only_if_is_dirty=False)
unreal.log('CHUCK_ART_MATERIALS_READY')
