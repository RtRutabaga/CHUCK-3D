"""Original procedural fire/embers, no texture downloads; owns only these two materials."""
import unreal
lib = unreal.MaterialEditingLibrary
code = r'''
struct Field {
 float hash(float2 p) { return frac(sin(dot(p,float2(127.1,311.7)))*43758.5453); }
 float noise(float2 p) {
  float2 i=floor(p),f=frac(p);f=f*f*(3-2*f);
  return lerp(lerp(hash(i),hash(i+float2(1,0)),f.x),lerp(hash(i+float2(0,1)),hash(i+1),f.x),f.y);
 }
}; Field n;
float h=saturate(UV.y),x=(UV.x-.5)*2;
float phase=dot(P.xy,float2(.071,.043));
float turbulence=n.noise(float2(x*3+h*2,h*5-T*2.6+phase));
float bend=sin(h*8-T*3+phase)*.12*h+n.noise(float2(h*6,T*1.8+phase))*.17*h;
float width=pow(1-h,.72)*(.50+.14*turbulence);
float edge=saturate((width-abs(x-bend))/.16);
float pulse=.82+.18*n.noise(float2(x*7+phase,h*9-T*4));
float opacity=edge*pulse*smoothstep(0,.06,h)*(1-smoothstep(.83,1,h))*.78;
float core=saturate(1-abs(x-bend)/max(width*.62,.02))*(1-h);
float3 colour=lerp(float3(2.4,.29,.012),float3(5.2,2.6,.42),core);
return float4(colour,opacity);
'''
for name in ('TorchFlame', 'FireEmber'):
    path = '/Game/Art/Materials/M_' + name
    mat = unreal.load_asset(path)
    if not mat:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_' + name, '/Game/Art/Materials', unreal.Material, unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(mat)
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('two_sided', True)
    mat.set_editor_property('used_with_instanced_static_meshes', True)
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT if name=='TorchFlame' else unreal.BlendMode.BLEND_OPAQUE)
    time = lib.create_material_expression(mat, unreal.MaterialExpressionTime)
    pos = lib.create_material_expression(mat, unreal.MaterialExpressionWorldPosition)
    uv = lib.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate)
    field = lib.create_material_expression(mat, unreal.MaterialExpressionCustom)
    inputs=[]
    for key in ('P','T','UV'):
        item=unreal.CustomInput();item.set_editor_property('input_name',key);inputs.append(item)
    field.set_editor_property('inputs',inputs)
    field.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    field.set_editor_property('code',code if name=='TorchFlame' else 'float b=.65+.18*sin(T*1.7+P.x*.23)+.12*sin(P.y*.8+P.x*.6); return float4(float3(1.8,.17,.008)*b,1);')
    for key,source in (('P',pos),('T',time),('UV',uv)):
        lib.connect_material_expressions(source,'',field,key)
    rgb=lib.create_material_expression(mat,unreal.MaterialExpressionComponentMask)
    rgb.set_editor_property('r',True);rgb.set_editor_property('g',True);rgb.set_editor_property('b',True);rgb.set_editor_property('a',False)
    lib.connect_material_expressions(field,'',rgb,'')
    lib.connect_material_property(rgb,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if name=='TorchFlame':
        alpha=lib.create_material_expression(mat,unreal.MaterialExpressionComponentMask)
        # Component masks default to R+G. Opacity must use only the scalar alpha.
        alpha.set_editor_property('r',False);alpha.set_editor_property('g',False);alpha.set_editor_property('b',False);alpha.set_editor_property('a',True)
        lib.connect_material_expressions(field,'',alpha,'')
        lib.connect_material_property(alpha,'',unreal.MaterialProperty.MP_OPACITY)
    lib.recompile_material(mat)
    if not unreal.EditorAssetLibrary.save_loaded_asset(mat,only_if_is_dirty=False): raise RuntimeError(path)
    unreal.log('CHUCK_FIRE_MATERIAL_READY '+name)
