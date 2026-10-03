"""Create only the three materials for timber workshops and the open sewer hatch."""
import unreal

lib = unreal.MaterialEditingLibrary
for name in ('AgedDockTimber', 'RustIron', 'SewerVoid'):
    path = '/Game/Art/Materials/M_' + name
    if unreal.load_asset(path):
        unreal.log('CHUCK_WORKSHOP_MATERIAL_EXISTS ' + path)
        continue
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_' + name, '/Game/Art/Materials', unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('used_with_instanced_static_meshes', True)
    if name == 'SewerVoid':
        mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
        mat.set_editor_property('two_sided', True)
        black = lib.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector)
        black.set_editor_property('constant', unreal.LinearColor(0, 0, 0, 1))
        lib.connect_material_property(black, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    else:
        pos = lib.create_material_expression(mat, unreal.MaterialExpressionWorldPosition)
        normal = lib.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS)
        custom = lib.create_material_expression(mat, unreal.MaterialExpressionCustom)
        ins = []
        for label in ('P', 'N'):
            item = unreal.CustomInput(); item.set_editor_property('input_name', label); ins.append(item)
        custom.set_editor_property('inputs', ins)
        custom.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
        noise = '''struct Noise { float hash(float2 p){return frac(sin(dot(p,float2(127.1,311.7)))*43758.5453);} float noise(float2 p){float2 i=floor(p),f=frac(p);f=f*f*(3-2*f);return lerp(lerp(hash(i),hash(i+float2(1,0)),f.x),lerp(hash(i+float2(0,1)),hash(i+1),f.x),f.y);} }; Noise n; float2 q=abs(N.x)>abs(N.y)?P.yz:P.xz; if(abs(N.z)>.7)q=P.xy;'''
        if name == 'AgedDockTimber':
            code = '''float board=floor(q.x/18);float tint=n.hash(float2(board,17));float grain=n.noise(float2(q.x*.9,q.y*.035));float wave=sin(q.x*2.8+n.noise(q*.016)*3);float seam=1-smoothstep(.012,.035,min(frac(q.x/18),1-frac(q.x/18)));float crack=pow(saturate(wave),24)*smoothstep(.52,.8,n.noise(float2(q.x*.3,q.y*.018)));float damp=(1-smoothstep(15,115,P.z))*.22;float3 wood=lerp(float3(.135,.105,.072),float3(.255,.235,.185),tint);return wood*(.77+.24*grain-damp)*(1-seam*.6-crack*.42);'''
        else:
            code = '''float rust=smoothstep(.28,.67,n.noise(q*.09)+n.noise(q*.8)*.2);float pit=n.noise(q*2.3);return lerp(float3(.055,.06,.058),float3(.245,.09,.028),rust)*(.68+.32*pit);'''
        custom.set_editor_property('code', noise + code)
        lib.connect_material_expressions(pos, '', custom, 'P'); lib.connect_material_expressions(normal, '', custom, 'N')
        lib.connect_material_property(custom, '', unreal.MaterialProperty.MP_BASE_COLOR)
        rough = lib.create_material_expression(mat, unreal.MaterialExpressionConstant); rough.set_editor_property('r', .94)
        lib.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    lib.recompile_material(mat)
    if not unreal.EditorAssetLibrary.save_loaded_asset(mat, only_if_is_dirty=False): raise RuntimeError(path)
    unreal.log('CHUCK_WORKSHOP_MATERIAL_READY ' + path)
