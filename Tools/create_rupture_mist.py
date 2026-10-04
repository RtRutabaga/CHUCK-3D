"""Create only the original upright rupture-edge oil veil material."""
import unreal
lib=unreal.MaterialEditingLibrary
path='/Game/Art/Materials/M_AstralOilMist'
mat=unreal.load_asset(path)
if not mat: raise RuntimeError('Expected existing oil material')
lib.delete_all_material_expressions(mat)
mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
mat.set_editor_property('two_sided',True)
mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT)
uv=lib.create_material_expression(mat,unreal.MaterialExpressionTextureCoordinate)
clock=lib.create_material_expression(mat,unreal.MaterialExpressionTime)
field=lib.create_material_expression(mat,unreal.MaterialExpressionCustom)
inputs=[]
for key in ('UV','T'):
    item=unreal.CustomInput();item.set_editor_property('input_name',key);inputs.append(item)
field.set_editor_property('inputs',inputs)
field.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT4)
field.set_editor_property('code',r'''
float2 q=UV;float rise=q.y-T*.09;
float warp=sin(q.x*19+sin(rise*8)*1.5)+cos(rise*13-q.x*11-T*.13);
float phase=warp*1.6+q.x*9+rise*7;
float3 film=.5+.5*cos(phase+float3(0,2.1,4.2));
float ends=smoothstep(0,.12,q.x)*(1-smoothstep(.88,1,q.x));
float fade=smoothstep(0,.05,q.y)*pow(saturate(1-q.y),2.2);
float threads=pow(saturate(.5+.5*sin(phase*2.1)),2);
return float4(lerp(float3(.045,.008,.12),film*float3(.32,.22,.42),.75),ends*fade*(.10+.18*threads));
''')
lib.connect_material_expressions(uv,'',field,'UV');lib.connect_material_expressions(clock,'',field,'T')
rgb=lib.create_material_expression(mat,unreal.MaterialExpressionComponentMask)
alpha=lib.create_material_expression(mat,unreal.MaterialExpressionComponentMask)
for channel in ('r','g','b','a'):
    rgb.set_editor_property(channel,channel!='a');alpha.set_editor_property(channel,channel=='a')
lib.connect_material_expressions(field,'',rgb,'');lib.connect_material_expressions(field,'',alpha,'')
lib.connect_material_property(rgb,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
lib.connect_material_property(alpha,'',unreal.MaterialProperty.MP_OPACITY)
lib.recompile_material(mat)
if not unreal.EditorAssetLibrary.save_loaded_asset(mat,only_if_is_dirty=False): raise RuntimeError(path)
unreal.log('CHUCK_UPRIGHT_RUPTURE_MIST_READY')
