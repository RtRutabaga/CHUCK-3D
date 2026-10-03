"""Import the supplied sewer score and create only two new astral materials."""
from pathlib import Path
import unreal

root = Path(__file__).resolve().parents[1]
task = unreal.AssetImportTask()
task.filename = str(root / 'SourceAssets/Audio/Sewer.wav')
task.destination_path = '/Game/Art/Audio'
task.destination_name = 'SW_Sewer'
task.automated = True
task.replace_existing = False
task.save = True
if not unreal.load_asset('/Game/Art/Audio/SW_Sewer'):
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
sound = unreal.load_asset('/Game/Art/Audio/SW_Sewer')
if not isinstance(sound, unreal.SoundWave): raise RuntimeError('Sewer soundtrack import failed')
sound.set_editor_property('looping', True)
sound.set_editor_property('volume', 1.0)
unreal.EditorAssetLibrary.save_loaded_asset(sound, only_if_is_dirty=False)
unreal.log('CHUCK_SEWER_AUDIO_READY duration=' + str(sound.get_editor_property('duration')))
lib = unreal.MaterialEditingLibrary
for name in ('AstralRupture', 'AstralOilMist'):
    path = '/Game/Art/Materials/M_' + name
    if unreal.load_asset(path): continue
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_' + name, '/Game/Art/Materials', unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('two_sided', True)
    if name == 'AstralOilMist': mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    uv = lib.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate)
    clock = lib.create_material_expression(mat, unreal.MaterialExpressionTime)
    custom = lib.create_material_expression(mat, unreal.MaterialExpressionCustom)
    ins = []
    for label in ('UV', 'T'):
        item = unreal.CustomInput(); item.set_editor_property('input_name', label); ins.append(item)
    custom.set_editor_property('inputs', ins)
    custom.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    common = 'float2 q=UV;float warp=sin(q.x*13+sin(q.y*10+T*.21)*2)+cos(q.y*17-q.x*7-T*.15);'
    if name == 'AstralRupture':
        code = 'float haze=.5+.5*sin(warp*1.7+q.y*6-T*.12);float star=pow(saturate(sin(q.x*143+q.y*173)*cos(q.y*117-q.x*91)),28);return float4(lerp(float3(.014,.003,.065),float3(.23,.018,.7),haze*.65)+star*float3(.5,.24,.8),1);'
    else:
        code = 'float phase=warp*2.6+q.x*8+q.y*6+T*.17;float3 film=.5+.5*cos(phase+float3(0,2.1,4.2));float edge=saturate(min(min(q.x,1-q.x),min(q.y,1-q.y))*8);float strands=pow(.5+.5*sin(phase*2),3);return float4(lerp(float3(.06,.012,.15),film*float3(.38,.26,.48),.7),edge*(.065+.13*strands));'
    custom.set_editor_property('code', common + code)
    lib.connect_material_expressions(uv, '', custom, 'UV'); lib.connect_material_expressions(clock, '', custom, 'T')
    rgb = lib.create_material_expression(mat, unreal.MaterialExpressionComponentMask)
    for channel in ('r', 'g', 'b'): rgb.set_editor_property(channel, True)
    lib.connect_material_expressions(custom, '', rgb, '')
    lib.connect_material_property(rgb, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    if name == 'AstralOilMist':
        alpha = lib.create_material_expression(mat, unreal.MaterialExpressionComponentMask); alpha.set_editor_property('a', True)
        lib.connect_material_expressions(custom, '', alpha, '')
        lib.connect_material_property(alpha, '', unreal.MaterialProperty.MP_OPACITY)
        refract = lib.create_material_expression(mat, unreal.MaterialExpressionConstant); refract.set_editor_property('r', 1.025)
        lib.connect_material_property(refract, '', unreal.MaterialProperty.MP_REFRACTION)
        offset = lib.create_material_expression(mat, unreal.MaterialExpressionCustom)
        offset.set_editor_property('inputs', ins); offset.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT3)
        offset.set_editor_property('code', 'return float3(sin(UV.y*9+T*.3)*4,cos(UV.x*12-T*.24)*4,sin(UV.x*10+UV.y*8+T*.25)*9);')
        lib.connect_material_expressions(uv, '', offset, 'UV'); lib.connect_material_expressions(clock, '', offset, 'T')
        lib.connect_material_property(offset, '', unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    lib.recompile_material(mat)
    if not unreal.EditorAssetLibrary.save_loaded_asset(mat, only_if_is_dirty=False): raise RuntimeError(path)
    unreal.log('CHUCK_ASTRAL_MATERIAL_READY ' + path)
