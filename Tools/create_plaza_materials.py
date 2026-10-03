"""Create only the additive plaza's dawn, flame and fountain materials."""
import unreal
lib = unreal.MaterialEditingLibrary
for name in ('DawnSky', 'TorchFlame', 'FountainWater'):
    if '-ChuckSkyOnly' in unreal.SystemLibrary.get_command_line() and name != 'DawnSky':
        continue
    path = '/Game/Art/Materials/M_' + name
    mat = unreal.load_asset(path)
    if not mat:
        mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            'M_' + name, '/Game/Art/Materials', unreal.Material, unreal.MaterialFactoryNew())
    lib.delete_all_material_expressions(mat)
    mat.set_editor_property('used_with_instanced_static_meshes', True)
    mat.set_editor_property('two_sided', True)
    if name == 'DawnSky':
        mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
        pos = lib.create_material_expression(mat, unreal.MaterialExpressionWorldPosition)
        custom = lib.create_material_expression(mat, unreal.MaterialExpressionCustom)
        inp = unreal.CustomInput(); inp.set_editor_property('input_name', 'P')
        custom.set_editor_property('inputs', [inp])
        custom.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
        custom.set_editor_property('code', '''float3 d=normalize(P);
float h=saturate(d.z*1.6);
float3 sky=lerp(float3(.43,.32,.34),float3(.075,.15,.30),h);
float glow=pow(saturate(dot(normalize(d.xy+0.0001),normalize(float2(1,-.4)))),5)*exp(-abs(d.z)*6);
float cloud=pow(saturate(sin(d.x*17+d.y*11+sin(d.y*19))*.5+.5),5)*saturate(d.z*3)*.07;
return sky+glow*float3(.32,.11,.015)+cloud;''')
        lib.connect_material_expressions(pos, '', custom, 'P')
        tint = lib.create_material_expression(mat, unreal.MaterialExpressionVectorParameter)
        tint.set_editor_property('parameter_name', 'SkyTint')
        tint.set_editor_property('default_value', unreal.LinearColor(1, 1, 1, 1))
        multiply = lib.create_material_expression(mat, unreal.MaterialExpressionMultiply)
        lib.connect_material_expressions(custom, '', multiply, 'A')
        lib.connect_material_expressions(tint, '', multiply, 'B')
        lib.connect_material_property(multiply, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    else:
        color = lib.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector)
        color.set_editor_property('constant', unreal.LinearColor(7, 2.0, .16, 1) if name == 'TorchFlame' else unreal.LinearColor(.025, .21, .25, 1))
        if name == 'TorchFlame':
            mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
            lib.connect_material_property(color, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        else:
            lib.connect_material_property(color, '', unreal.MaterialProperty.MP_BASE_COLOR)
            rough = lib.create_material_expression(mat, unreal.MaterialExpressionConstant)
            rough.set_editor_property('r', .16)
            lib.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
            time = lib.create_material_expression(mat, unreal.MaterialExpressionTime)
            pos = lib.create_material_expression(mat, unreal.MaterialExpressionWorldPosition)
            wave = lib.create_material_expression(mat, unreal.MaterialExpressionCustom)
            inputs=[]
            for n in ('P','T'):
                i=unreal.CustomInput(); i.set_editor_property('input_name',n); inputs.append(i)
            wave.set_editor_property('inputs',inputs)
            wave.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
            wave.set_editor_property('code','return normalize(float3(.12*sin(P.x*.14+T*2),.12*cos(P.y*.17-T*1.7),1));')
            lib.connect_material_expressions(pos,'',wave,'P')
            lib.connect_material_expressions(time,'',wave,'T')
            lib.connect_material_property(wave,'',unreal.MaterialProperty.MP_NORMAL)
    lib.recompile_material(mat)
    if not unreal.EditorAssetLibrary.save_loaded_asset(mat, only_if_is_dirty=False):
        raise RuntimeError(path)
unreal.log('CHUCK_PLAZA_MATERIALS_READY')
