"""Import the Chult raptor (Tools/build_raptor.py) into /Game/Characters/Raptor (Claude, 2026-10-14).

UnrealEditor-Cmd <uproject> -ExecutePythonScript=Tools/import_raptor.py -unattended -nosplash -NoLiveCoding

Skinned mesh SK_Raptor, its skeleton, six clips AS_Raptor_<Clip> (no root
motion: the runtime moves it), the generated textures and M_Raptor: a hide
with dark dorsal bands and a pale belly (T_RaptorPattern on the body-following
UVs) and tiled scales (T_RaptorScales, _N). One instance per source region
slot. Only raptor assets are created or replaced.
"""
from pathlib import Path
import json
import unreal

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'SourceAssets/Enemies/Raptor'
DEST = '/Game/Characters/Raptor'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
LIB = unreal.MaterialEditingLibrary
ASSETS = unreal.EditorAssetLibrary
CLIPS = json.loads((SOURCE / 'raptor-clips.json').read_text())['clips']


def node(mat, cls, x=0, y=0, **props):
    n = LIB.create_material_expression(mat, cls, x, y)
    for k, v in props.items(): n.set_editor_property(k, v)
    return n


def link(a, b, pin_b, pin_a=''):
    if not LIB.connect_material_expressions(a, pin_a, b, pin_b):
        raise RuntimeError(f'Material link failed: {pin_a} -> {pin_b}')


def out(expr, prop, pin=''):
    if not LIB.connect_material_property(expr, pin, prop):
        raise RuntimeError(f'Material output failed: {prop}')


def texture(name, normal=False, srgb=True):
    task = unreal.AssetImportTask()
    task.filename = str(SOURCE / f'{name}.png'); task.destination_path = DEST; task.destination_name = name
    task.automated = True; task.replace_existing = True; task.save = False
    TOOLS.import_asset_tasks([task])
    tex = unreal.load_asset(f'{DEST}/{name}')
    if not isinstance(tex, unreal.Texture2D): raise RuntimeError(f'Texture import failed: {name}')
    if normal:
        tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
    tex.set_editor_property('srgb', srgb and not normal)
    ASSETS.save_loaded_asset(tex, only_if_is_dirty=False)
    return tex


scales = texture('T_RaptorScales')
scales_n = texture('T_RaptorScales_N', normal=True)
pattern = texture('T_RaptorPattern', srgb=False)


def vector_param(mat, name, rgb, x, y):
    return node(mat, unreal.MaterialExpressionVectorParameter, x, y, parameter_name=name, default_value=unreal.LinearColor(*rgb, 1))


def scalar_param(mat, name, v, x, y):
    return node(mat, unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=v)


path = f'{DEST}/M_Raptor'
# Built once (rebuilding an existing graph trips an engine assertion in
# DeleteAllMaterialExpressions); later runs only update its settings. Delete
# M_Raptor to rebuild the graph.
mat = unreal.load_asset(path)
fresh = mat is None
if fresh:
    mat = TOOLS.create_asset('M_Raptor', DEST, unreal.Material, unreal.MaterialFactoryNew())
mat.set_editor_property('used_with_skeletal_mesh', True)
mat.set_editor_property('two_sided', True)   # safety for any open seam in the source shells
if fresh:
    uv = node(mat, unreal.MaterialExpressionTextureCoordinate, -1400, 0)
    tiles = node(mat, unreal.MaterialExpressionTextureCoordinate, -1400, 300, u_tiling=46., v_tiling=15.)
    pat = node(mat, unreal.MaterialExpressionTextureSample, -1150, 0, texture=pattern, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    link(uv, pat, 'UVs')
    det = node(mat, unreal.MaterialExpressionTextureSample, -1150, 300, texture=scales)
    link(tiles, det, 'UVs')
    nrm = node(mat, unreal.MaterialExpressionTextureSample, -1150, 600, texture=scales_n, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    link(tiles, nrm, 'UVs')
    body = vector_param(mat, 'BodyColor', (.055, .065, .032), -900, -300)
    back = vector_param(mat, 'BackColor', (.035, .042, .022), -900, -200)
    band = vector_param(mat, 'BandColor', (.022, .017, .012), -900, -100)
    belly = vector_param(mat, 'BellyColor', (.19, .16, .11), -900, 0)
    pattern_amount = scalar_param(mat, 'PatternAmount', 1., -900, 100)
    # Back over body, bands over that, belly under: all scaled by PatternAmount.
    l1 = node(mat, unreal.MaterialExpressionLinearInterpolate, -650, -250); link(body, l1, 'A'); link(back, l1, 'B')
    mb = node(mat, unreal.MaterialExpressionMultiply, -800, 150); link(pat, mb, 'A', 'B'); link(pattern_amount, mb, 'B'); link(mb, l1, 'Alpha')
    l2 = node(mat, unreal.MaterialExpressionLinearInterpolate, -450, -200); link(l1, l2, 'A'); link(band, l2, 'B')
    mr = node(mat, unreal.MaterialExpressionMultiply, -800, 250); link(pat, mr, 'A', 'R'); link(pattern_amount, mr, 'B'); link(mr, l2, 'Alpha')
    l3 = node(mat, unreal.MaterialExpressionLinearInterpolate, -250, -150); link(l2, l3, 'A'); link(belly, l3, 'B')
    mg = node(mat, unreal.MaterialExpressionMultiply, -800, 350); link(pat, mg, 'A', 'G'); link(pattern_amount, mg, 'B'); link(mg, l3, 'Alpha')
    shade = node(mat, unreal.MaterialExpressionMultiply, -50, -100); link(l3, shade, 'A'); link(det, shade, 'B', 'R')
    out(shade, unreal.MaterialProperty.MP_BASE_COLOR)
    rough = scalar_param(mat, 'Roughness', .62, -250, 450)
    crev = node(mat, unreal.MaterialExpressionOneMinus, -500, 500); link(det, crev, '', 'R')
    rmul = node(mat, unreal.MaterialExpressionMultiply, -300, 550); link(crev, rmul, 'A'); link(node(mat, unreal.MaterialExpressionConstant, -500, 600, r=.25), rmul, 'B')
    radd = node(mat, unreal.MaterialExpressionAdd, -100, 450); link(rough, radd, 'A'); link(rmul, radd, 'B')
    out(radd, unreal.MaterialProperty.MP_ROUGHNESS)
    out(nrm, unreal.MaterialProperty.MP_NORMAL)
    out(scalar_param(mat, 'Specular', .45, -100, 700), unreal.MaterialProperty.MP_SPECULAR)
LIB.recompile_material(mat)
ASSETS.save_loaded_asset(mat, only_if_is_dirty=False)

REGIONS = {   # source region -> parameters
    'RaptorHide': {},
    'RaptorBelly': {'BodyColor': (.16, .135, .095), 'BackColor': (.12, .1, .07), 'PatternAmount': .35},
    'RaptorDark': {'BodyColor': (.012, .011, .01), 'BackColor': (.012, .011, .01), 'BellyColor': (.02, .018, .016), 'PatternAmount': 0., 'Roughness': .3},
}
instances = {}
for region, params in REGIONS.items():
    name = 'MI_' + region
    mi = unreal.load_asset(f'{DEST}/{name}') or TOOLS.create_asset(name, DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    # Since 2026-10-16 the instances hang from the shared M_DinoSkin (Tools/build_dino_skin.py), when it exists.
    mi.set_editor_property('parent', unreal.load_asset('/Game/Characters/Dinos/M_DinoSkin') or mat)
    for k, v in params.items():
        if isinstance(v, tuple):
            LIB.set_material_instance_vector_parameter_value(mi, k, unreal.LinearColor(*v, 1))
        else:
            LIB.set_material_instance_scalar_parameter_value(mi, k, v)
    LIB.update_material_instance(mi)
    ASSETS.save_loaded_asset(mi, only_if_is_dirty=False)
    instances[region] = mi

options = unreal.FbxImportUI()
options.import_mesh = True
options.import_as_skeletal = True
options.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
options.automated_import_should_detect_type = False
options.import_materials = options.import_textures = options.import_animations = False
options.create_physics_asset = False
options.skeletal_mesh_import_data.set_editor_property('update_skeleton_reference_pose', True)
task = unreal.AssetImportTask()
task.filename = str(SOURCE / 'SK_Raptor.fbx'); task.destination_path = DEST; task.destination_name = 'SK_Raptor'
task.automated = True; task.replace_existing = True; task.save = True; task.options = options
TOOLS.import_asset_tasks([task])
mesh = unreal.load_asset(f'{DEST}/SK_Raptor')
if not isinstance(mesh, unreal.SkeletalMesh):
    raise RuntimeError('Raptor mesh is not skeletal')
mats = mesh.get_editor_property('materials')
for index, m in enumerate(mats):
    slot = str(m.get_editor_property('material_slot_name')).split('.')[0]
    if slot not in instances:
        raise RuntimeError(f'Unexpected raptor slot {slot}')
    m.material_interface = instances[slot]
    mats[index] = m
mesh.set_editor_property('materials', mats)
ASSETS.save_loaded_asset(mesh, only_if_is_dirty=False)
skeleton = mesh.get_editor_property('skeleton')
ASSETS.save_loaded_asset(skeleton, only_if_is_dirty=False)

for name, clip in CLIPS.items():
    o = unreal.FbxImportUI()
    o.import_mesh = False
    o.import_as_skeletal = True
    o.import_animations = True
    o.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION
    o.automated_import_should_detect_type = False
    o.skeleton = skeleton
    o.import_materials = o.import_textures = False
    o.anim_sequence_import_data.set_editor_property('use_default_sample_rate', False)
    o.anim_sequence_import_data.set_editor_property('custom_sample_rate', 24)
    t = unreal.AssetImportTask()
    t.filename = str(SOURCE / f'AS_Raptor_{name}.fbx'); t.destination_path = DEST + '/Animations'; t.destination_name = f'AS_Raptor_{name}'
    t.automated = True; t.replace_existing = True; t.save = False; t.options = o
    TOOLS.import_asset_tasks([t])
    anim = unreal.load_asset(f'{DEST}/Animations/AS_Raptor_{name}')
    if not isinstance(anim, unreal.AnimSequence) or anim.get_editor_property('skeleton') != skeleton:
        raise RuntimeError(f'Wrong animation/skeleton for {name}')
    anim.set_editor_property('enable_root_motion', False)
    ASSETS.save_loaded_asset(anim, only_if_is_dirty=False)
    unreal.log(f'CHUCK_RAPTOR_CLIP_IMPORT {name} length={anim.get_editor_property("sequence_length"):.3f} source={clip["length_s"]:.3f}')
box = mesh.get_bounds().box_extent
unreal.log(f'CHUCK_RAPTOR_IMPORTED slots={[str(m.material_slot_name) for m in mesh.get_editor_property("materials")]} extent=({box.x:.1f},{box.y:.1f},{box.z:.1f}) clips={len(CLIPS)}')
