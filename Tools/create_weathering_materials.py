"""Create only dedicated dock weathering materials; existing asset graphs are untouched."""
import unreal

lib = unreal.MaterialEditingLibrary
def node(mat, cls, **props):
    obj = lib.create_material_expression(mat, cls)
    for key, value in props.items(): obj.set_editor_property(key, value)
    return obj
def inputs(names):
    result = []
    for name in names:
        item = unreal.CustomInput(); item.set_editor_property('input_name', name); result.append(item)
    return result
for name in ('WeatheredPlaster', 'DockIvy'):
    path = '/Game/Art/Materials/M_' + name
    mat = unreal.load_asset(path)
    if not mat:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_'+name, '/Game/Art/Materials', unreal.Material, unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(mat)
    mat.set_editor_property('used_with_instanced_static_meshes', True)
    mat.set_editor_property('two_sided', True)
    rough = node(mat, unreal.MaterialExpressionConstant, r=.94 if name=='WeatheredPlaster' else .78)
    lib.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    if name == 'DockIvy':
        vertex = node(mat, unreal.MaterialExpressionVertexColor)
        uv = node(mat, unreal.MaterialExpressionTextureCoordinate)
        color = node(mat, unreal.MaterialExpressionCustom, inputs=inputs(('C','UV')), output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3,
            code='float vein=1-smoothstep(.015,.045,abs(UV.x-.5)); float ribs=pow(saturate(cos((abs(UV.x-.5)*1.7+UV.y)*48)),12); return C*(.8+.22*UV.y)+float3(.04,.055,.015)*(vein+ribs*.35);')
        if not lib.connect_material_expressions(vertex,'',color,'C'): raise RuntimeError('Missing ivy vertex color connection')
        if not lib.connect_material_expressions(uv,'',color,'UV'): raise RuntimeError('Missing ivy UV connection')
        lib.connect_material_property(color,'',unreal.MaterialProperty.MP_BASE_COLOR)
    else:
        mat.set_editor_property('tangent_space_normal',False)
        pos=node(mat,unreal.MaterialExpressionWorldPosition)
        normal=node(mat,unreal.MaterialExpressionVertexNormalWS)
        surface=node(mat,unreal.MaterialExpressionCustom,inputs=inputs(('P','N')),output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT4,
            code=r'''struct Noise {
 float hash(float2 p){return frac(sin(dot(p,float2(127.1,311.7)))*43758.5453);}
 float noise(float2 p){float2 i=floor(p),f=frac(p);f=f*f*(3-2*f);return lerp(lerp(hash(i),hash(i+float2(1,0)),f.x),lerp(hash(i+float2(0,1)),hash(i+1),f.x),f.y);}
}; Noise n;
float2 q=abs(N.x)>abs(N.y)?P.yz:P.xz;
float broad=n.noise(q*.009), medium=n.noise(q*.043), fine=n.noise(q*1.6);
float2 cell=q/95+float2(n.noise(q*.021),n.noise(q*.021+37))*.45;
float2 base=floor(cell),f=frac(cell); float nearest=100,second=100;
for(int y=-1;y<=1;y++) for(int x=-1;x<=1;x++){
 float2 o=float2(x,y);float2 seed=float2(n.hash(base+o),n.hash(base+o+83));
 float d=length(o+seed-f);if(d<nearest){second=nearest;nearest=d;}else second=min(second,d);
}
float crack=(1-smoothstep(.002,.009,second-nearest))*smoothstep(.38,.60,n.noise(q*.014+71));
float stain=(1-smoothstep(.2,.53,broad))*.45;
float baseDamp=(1-smoothstep(25,155,P.z))*(.12+.13*medium);
float3 lime=lerp(float3(.31,.285,.235),float3(.49,.425,.32),broad);
lime*=.87+.15*medium+.055*fine;
lime=lerp(lime,float3(.19,.18,.145),saturate(stain+baseDamp));
lime=lerp(lime,float3(.095,.086,.069),crack*.65);
return float4(lime,.25*fine+.12*medium-crack*.65);''')
        lib.connect_material_expressions(pos,'',surface,'P'); lib.connect_material_expressions(normal,'',surface,'N')
        rgb=node(mat,unreal.MaterialExpressionComponentMask,r=True,g=True,b=True,a=False)
        height=node(mat,unreal.MaterialExpressionComponentMask,r=False,g=False,b=False,a=True)
        lib.connect_material_expressions(surface,'',rgb,''); lib.connect_material_expressions(surface,'',height,'')
        lib.connect_material_property(rgb,'',unreal.MaterialProperty.MP_BASE_COLOR)
        bump=node(mat,unreal.MaterialExpressionCustom,inputs=inputs(('P','N','H')),output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3,
            code='float3 dx=ddx(P),dy=ddy(P),n=normalize(N);float3 r1=cross(dy,n),r2=cross(n,dx);float det=dot(dx,r1);float3 g=sign(det)*(ddx(H)*r1+ddy(H)*r2)/max(abs(det),.00001);return normalize(n-g*.28);')
        for src,pin in ((pos,'P'),(normal,'N'),(height,'H')): lib.connect_material_expressions(src,'',bump,pin)
        lib.connect_material_property(bump,'',unreal.MaterialProperty.MP_NORMAL)
    lib.recompile_material(mat)
    if not unreal.EditorAssetLibrary.save_loaded_asset(mat,only_if_is_dirty=False): raise RuntimeError(path)
    unreal.log('CHUCK_WEATHERING_MATERIAL '+path)
unreal.log('CHUCK_WEATHERING_MATERIALS_READY')
