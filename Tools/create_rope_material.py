"""The rope material (2026-11-07): ambientCG Rope001 (CC0) imported and M_Rope built from it, mapped on a rope's own UVs
(Source/Chuck3D/ShipRope.h: U along the rope, 26 cm a repeat; 3/7 of V round it), and MI_RopeTarred, its instance for
the ship's standing rigging (2026-11-08: tarred hemp, dark and a little glossy). Touches only these assets.
Run: UnrealEditor-Cmd Chuck3D.uproject -ExecutePythonScript=Tools/create_rope_material.py -unattended -nosplash"""
from pathlib import Path
import hashlib
import json
import unreal

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'SourceAssets/Surfaces/Rope'
DEST = '/Game/Art/Textures/Rope'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
LIB = unreal.MaterialEditingLibrary

textures = {}
for record in json.loads((SOURCE / 'manifest.json').read_text(encoding='utf-8-sig'))['assets']:
    file = SOURCE / record['file']
    if hashlib.sha256(file.read_bytes()).hexdigest().upper() != record['sha256']:
        raise RuntimeError('Source hash mismatch: ' + file.name)
    task = unreal.AssetImportTask()
    task.filename = str(file); task.destination_path = DEST
    task.destination_name = 'T_Rope001_' + record['map']
    task.automated = True; task.replace_existing = True; task.save = True
    TOOLS.import_asset_tasks([task])
    tex = unreal.load_asset(DEST + '/' + task.destination_name)
    if not tex: raise RuntimeError('Import failed: ' + file.name)
    role = record['map']
    tex.set_editor_property('srgb', role == 'color')
    tex.set_editor_property('compression_settings', {'color': unreal.TextureCompressionSettings.TC_DEFAULT,
        'roughness': unreal.TextureCompressionSettings.TC_GRAYSCALE, 'normal_dx': unreal.TextureCompressionSettings.TC_NORMALMAP}[role])
    if tex.blueprint_get_size_x() != 1024 or tex.blueprint_get_size_y() != 512:
        raise RuntimeError('Unexpected resolution: ' + file.name)
    unreal.EditorAssetLibrary.save_loaded_asset(tex, only_if_is_dirty=False)
    textures[role] = tex


def node(mat, cls, x, y, **props):
    item = LIB.create_material_expression(mat, cls, x, y)
    for key, value in props.items(): item.set_editor_property(key, value)
    return item


def connect(a, b, pin, output=''):
    if not LIB.connect_material_expressions(a, output, b, pin): raise RuntimeError('Cannot connect ' + pin)


path = '/Game/Art/Materials/M_Rope'
mat = unreal.load_asset(path) or TOOLS.create_asset('M_Rope', '/Game/Art/Materials', unreal.Material, unreal.MaterialFactoryNew())
LIB.delete_all_material_expressions(mat)
mat.set_editor_property('tangent_space_normal', True)
uv = node(mat, unreal.MaterialExpressionTextureCoordinate, -900, 0)
samples = {}
for i, (role, typ) in enumerate([('color', unreal.MaterialSamplerType.SAMPLERTYPE_COLOR),
                                 ('roughness', unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE),
                                 ('normal_dx', unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)]):
    s = node(mat, unreal.MaterialExpressionTextureSample, -600, i * 260, texture=textures[role], sampler_type=typ)
    connect(uv, s, 'UVs')
    samples[role] = s
# Old hemp, weathered and wet in the storm: the scan's light beige darkened and greyed toward brown.
tint = node(mat, unreal.MaterialExpressionVectorParameter, -600, -200, parameter_name='Tint', default_value=unreal.LinearColor(.60, .53, .43, 1))
color = node(mat, unreal.MaterialExpressionMultiply, -300, -60)
connect(samples['color'], color, 'A', 'RGB'); connect(tint, color, 'B')
LIB.connect_material_property(color, '', unreal.MaterialProperty.MP_BASE_COLOR)
# Rough fibre; the wet strands' crowns a little smoother than the dry scan.
rough = node(mat, unreal.MaterialExpressionLinearInterpolate, -300, 260)
connect(node(mat, unreal.MaterialExpressionScalarParameter, -450, 200, parameter_name='RoughLow', default_value=.52), rough, 'A')
connect(node(mat, unreal.MaterialExpressionScalarParameter, -450, 240, parameter_name='RoughHigh', default_value=.95), rough, 'B')
connect(samples['roughness'], rough, 'Alpha', 'R')
LIB.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
LIB.connect_material_property(node(mat, unreal.MaterialExpressionConstant, -300, 400, r=.3), '', unreal.MaterialProperty.MP_SPECULAR)
LIB.connect_material_property(samples['normal_dx'], 'RGB', unreal.MaterialProperty.MP_NORMAL)
LIB.recompile_material(mat)
if not unreal.EditorAssetLibrary.save_loaded_asset(mat, only_if_is_dirty=False): raise RuntimeError(path)
unreal.log('CHUCK_ROPE_MATERIAL_READY ' + path)

# The standing rigging: the same hemp soaked in Stockholm tar, near black-brown, the tar's sheen on the strands' crowns.
ipath = '/Game/Art/Materials/MI_RopeTarred'
inst = unreal.load_asset(ipath) or TOOLS.create_asset('MI_RopeTarred', '/Game/Art/Materials', unreal.MaterialInstanceConstant,
                                                     unreal.MaterialInstanceConstantFactoryNew())
inst.set_editor_property('parent', mat)
MIL = unreal.MaterialEditingLibrary
MIL.set_material_instance_vector_parameter_value(inst, 'Tint', unreal.LinearColor(.34, .27, .19, 1))
MIL.set_material_instance_scalar_parameter_value(inst, 'RoughLow', .38)
MIL.set_material_instance_scalar_parameter_value(inst, 'RoughHigh', .8)
MIL.update_material_instance(inst)
if not unreal.EditorAssetLibrary.save_loaded_asset(inst, only_if_is_dirty=False): raise RuntimeError(ipath)
unreal.log('CHUCK_ROPE_MATERIAL_READY ' + ipath)
