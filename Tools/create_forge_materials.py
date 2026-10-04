"""Own only ForgeBrick, ForgeIron, ForgeAsh and ForgeSmoke; no existing graphs touched."""
import unreal
L = unreal.MaterialEditingLibrary

def node(m, cls, **props):
    n = L.create_material_expression(m, cls)
    for k, v in props.items(): n.set_editor_property(k, v)
    return n

def connect(a, b, pin=''):
    if not L.connect_material_expressions(a, '', b, pin): raise RuntimeError(pin)

def output(m, n, prop):
    if not L.connect_material_property(n, '', prop): raise RuntimeError(str(prop))

for name in ('ForgeBrick', 'ForgeIron', 'ForgeAsh', 'ForgeSmoke'):
    path = '/Game/Art/Materials/M_' + name
    m = unreal.load_asset(path) or unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        'M_' + name, '/Game/Art/Materials', unreal.Material, unreal.MaterialFactoryNew())
    L.delete_all_material_expressions(m)
    m.set_editor_property('used_with_instanced_static_meshes', True)
    m.set_editor_property('two_sided', True)
    pos = node(m, unreal.MaterialExpressionWorldPosition)
    normal = node(m, unreal.MaterialExpressionVertexNormalWS)
    uv = node(m, unreal.MaterialExpressionTextureCoordinate)
    time = node(m, unreal.MaterialExpressionTime)
    field = node(m, unreal.MaterialExpressionCustom, output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    inputs = []
    for key in ('P', 'N', 'UV', 'T'):
        i = unreal.CustomInput(); i.set_editor_property('input_name', key); inputs.append(i)
    field.set_editor_property('inputs', inputs)
    if name == 'ForgeBrick':
        code = '''float3 a=abs(N); float2 q=a.z>.65?P.xy:(a.x>a.y?P.yz:P.xz);
float row=floor(q.y/10); float2 cell=float2(q.x/26+fmod(row,2)*.5,q.y/10);
float2 f=frac(cell); float edge=min(min(f.x,1-f.x)*26,min(f.y,1-f.y)*10);
float grain=.5+.5*sin(P.x*.32+sin(P.z*.53)+P.y*.26);
float brick=frac(sin(dot(floor(cell),float2(12.9898,78.233)))*43758.5453);
float3 c=lerp(float3(.11,.064,.04),float3(.24,.15,.095),brick)*(.8+.2*grain);
float mortar=1-smoothstep(.55,1.1,edge); c=lerp(c,float3(.08,.075,.06),mortar);
float soot=saturate(1-abs(P.x+780)/125)*saturate(1-abs(P.z-150)/190)*.68;
return float4(c*(1-soot),1);'''
    elif name == 'ForgeIron':
        code = '''float grain=.5+.5*sin(P.x*1.1+sin(P.y*1.4)+P.z*.82);
float rust=pow(saturate(.5+.5*sin(P.x*.11+P.z*.17+sin(P.y*.13))),6)*.28;
return float4(lerp(float3(.033,.037,.04)*(.7+.3*grain),float3(.15,.061,.019),rust),1);'''
    elif name == 'ForgeAsh':
        code = '''float n=.5+.5*sin(P.x*.67+sin(P.y*.91)+P.z*.83);
return float4(lerp(float3(.012,.014,.015),float3(.095,.088,.075),pow(n,3)),1);'''
    else:
        m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
        m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
        code = '''float h=saturate(UV.y),x=UV.x-.5;
float bend=sin(h*5-T*.48+P.x*.02)*.10*h;
float edge=saturate(1-abs(x-bend)/(.19+h*.26));
float wisps=saturate(.5+.3*sin(h*17-T*1.15+x*15)+.2*sin(h*29-T*1.6-x*21));
float alpha=edge*edge*wisps*smoothstep(0,.08,h)*(1-smoothstep(.35,1,h))*.12;
return float4(float3(.13,.125,.12),alpha);'''
    field.set_editor_property('code', code)
    for key, source in (('P', pos), ('N', normal), ('UV', uv), ('T', time)): connect(source, field, key)
    rgb = node(m, unreal.MaterialExpressionComponentMask, r=True, g=True, b=True, a=False)
    connect(field, rgb)
    output(m, rgb, unreal.MaterialProperty.MP_EMISSIVE_COLOR if name == 'ForgeSmoke' else unreal.MaterialProperty.MP_BASE_COLOR)
    if name == 'ForgeSmoke':
        alpha = node(m, unreal.MaterialExpressionComponentMask, r=False, g=False, b=False, a=True)
        connect(field, alpha); output(m, alpha, unreal.MaterialProperty.MP_OPACITY)
    else:
        output(m, node(m, unreal.MaterialExpressionConstant, r=.63 if name == 'ForgeIron' else .94), unreal.MaterialProperty.MP_ROUGHNESS)
        output(m, node(m, unreal.MaterialExpressionConstant, r=.7 if name == 'ForgeIron' else 0), unreal.MaterialProperty.MP_METALLIC)
    L.recompile_material(m)
    if not unreal.EditorAssetLibrary.save_loaded_asset(m, only_if_is_dirty=False): raise RuntimeError(path)
    unreal.log('CHUCK_FORGE_MATERIAL_READY ' + name)
