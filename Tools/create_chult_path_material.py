"""Import only the verified CC0 ancient-road maps and its dedicated material."""
from pathlib import Path
import hashlib
import json
import unreal

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'SourceAssets/Setting/ChultPath'
DEST = '/Game/Art/ChultPath'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
LIB = unreal.MaterialEditingLibrary
ASSETS = unreal.EditorAssetLibrary
helper = Path(__file__).with_name('import_fall_cutscene.py')
scope = {'__file__': str(helper), '__name__': 'path_helpers'}
exec(compile(helper.read_text(encoding='utf-8').split('# Music:')[0], str(helper), 'exec'), scope)
node, const, colour, out, link = [scope[k] for k in ('node', 'const', 'colour', 'out', 'link')]
ASSETS.make_directory(DEST)
textures = {}
for rec in json.loads((SOURCE / 'manifest.json').read_text(encoding='utf-8-sig'))['assets']:
    file = SOURCE / rec['file']
    assert hashlib.sha256(file.read_bytes()).hexdigest().upper() == rec['sha256']
    task = unreal.AssetImportTask()
    task.filename = str(file); task.destination_path = DEST
    task.destination_name = 'T_' + file.stem
    task.automated = True; task.replace_existing = True; task.save = False
    TOOLS.import_asset_tasks([task])
    tex = unreal.load_asset(DEST + '/' + task.destination_name)
    role = rec['role'].lower()
    tex.set_editor_property('srgb', role == 'diffuse')
    tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP if role == 'nor_dx' else
                            unreal.TextureCompressionSettings.TC_DEFAULT if role == 'diffuse' else unreal.TextureCompressionSettings.TC_MASKS)
    ASSETS.save_loaded_asset(tex, only_if_is_dirty=False)
    textures[role] = tex
name = 'M_ChultAncientPath'
mat = unreal.load_asset(DEST + '/' + name)
if mat:
    LIB.delete_all_material_expressions(mat)
else:
    mat = TOOLS.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
uv = node(mat, unreal.MaterialExpressionTextureCoordinate)
samples = {}
for role, tex in textures.items():
    sample = node(mat, unreal.MaterialExpressionTextureSample, texture=tex,
                  sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if role == 'nor_dx' else
                  unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if role == 'diffuse' else unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    link(uv, sample, 'UVs'); samples[role] = sample
vc = node(mat, unreal.MaterialExpressionVertexColor)
inputs = []
for key in ('D', 'A', 'VC'):
    arg = unreal.CustomInput(); arg.set_editor_property('input_name', key); inputs.append(arg)
shader = node(mat, unreal.MaterialExpressionCustom, inputs=inputs,
              output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3,
              code='float l=dot(D,float3(.3,.59,.11)); float moss=saturate(VC.r+(A.r-.65)*1.6); return lerp(lerp(D,l*float3(.99,.96,.86),.65)*1.1,float3(.032,.059,.018)*(0.6+l*1.8),moss*.55);')
link(samples['diffuse'], shader, 'D'); link(samples['arm'], shader, 'A'); link(vc, shader, 'VC')
out(mat, shader, unreal.MaterialProperty.MP_BASE_COLOR)
out(mat, samples['nor_dx'], unreal.MaterialProperty.MP_NORMAL)
out(mat, samples['arm'], unreal.MaterialProperty.MP_AMBIENT_OCCLUSION, 'R')
out(mat, samples['arm'], unreal.MaterialProperty.MP_ROUGHNESS, 'G')
out(mat, const(mat, .2), unreal.MaterialProperty.MP_SPECULAR)
LIB.recompile_material(mat)
ASSETS.save_loaded_asset(mat, only_if_is_dirty=False)
unreal.log('CHUCK_PATH_MATERIAL_READY textures=3 materials=1')
