"""Import committed v1 FBXs/textures beside the live character; never runs Blender."""
from pathlib import Path
import json
import runpy
import unreal

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT/'SourceAssets/Chuck/V1'
DEST = '/Game/Characters/Chuck/V1'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
LIB = unreal.MaterialEditingLibrary
manifest = json.loads((SOURCE/'Animations/manifest.json').read_text())


def import_asset(file, name, folder, options=None):
    task = unreal.AssetImportTask()
    task.filename = str(file)
    task.destination_path = folder
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    if options:
        task.options = options
    TOOLS.import_asset_tasks([task])
    asset = unreal.load_asset(folder+'/'+name)
    if not asset:
        raise RuntimeError(f'Missing imported {name}: {task.imported_object_paths}')
    return asset


options = unreal.FbxImportUI()
options.import_mesh = True
options.import_as_skeletal = True
options.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
options.automated_import_should_detect_type = False
options.import_materials = options.import_textures = options.import_animations = False
options.create_physics_asset = False
options.skeletal_mesh_import_data.normal_import_method = unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS
mesh = import_asset(SOURCE/'SK_Chuck.fbx', 'SK_Chuck', DEST, options)
if not isinstance(mesh, unreal.SkeletalMesh):
    raise RuntimeError('V1 mesh is not skeletal')
skeleton = mesh.get_editor_property('skeleton')

textures = {}
for role, compression, srgb in (
        ('BaseColor', unreal.TextureCompressionSettings.TC_DEFAULT, True),
        ('Normal', unreal.TextureCompressionSettings.TC_NORMALMAP, False),
        ('ORM', unreal.TextureCompressionSettings.TC_MASKS, False)):
    tex = import_asset(SOURCE/f'Textures/T_Chuck_{role}.png', f'T_Chuck_{role}', DEST+'/Textures')
    tex.set_editor_property('srgb', srgb)
    tex.set_editor_property('compression_settings', compression)
    if tex.blueprint_get_size_x() != 2048 or tex.blueprint_get_size_y() != 2048:
        raise RuntimeError(f'Unexpected {role} dimensions')
    # Source normal is already DirectX; never flip it a second time.
    if role == 'Normal':
        tex.set_editor_property('flip_green_channel', False)
    unreal.EditorAssetLibrary.save_loaded_asset(tex, only_if_is_dirty=False)
    textures[role] = tex

material = unreal.load_asset(DEST+'/M_Chuck_V1')
# The graph only samples the three maps, which are re-imported in place, so an
# existing material is kept: deleting its expressions from Python asserts
# (!IsRooted) in UE 5.7.4 once the material has been loaded with its graph.
build_graph = not material
if build_graph:
    material = TOOLS.create_asset('M_Chuck_V1', DEST, unreal.Material, unreal.MaterialFactoryNew())
else:
    for prop in (unreal.MaterialProperty.MP_BASE_COLOR, unreal.MaterialProperty.MP_NORMAL,
                 unreal.MaterialProperty.MP_ROUGHNESS):
        node = LIB.get_material_property_input_node(material, prop)
        if not isinstance(node, unreal.MaterialExpressionTextureSample) or node.get_editor_property('texture') not in textures.values():
            raise RuntimeError(f'M_Chuck_V1 {prop} is not wired to a V1 map; delete the asset and re-run')
LIB.set_material_usage(material, unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH)
samplers = {}
for role in (textures if build_graph else ()):
    node = LIB.create_material_expression(material, unreal.MaterialExpressionTextureSample)
    node.set_editor_property('texture', textures[role])
    node.set_editor_property('sampler_type', {
        'BaseColor': unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,
        'Normal': unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
        'ORM': unreal.MaterialSamplerType.SAMPLERTYPE_MASKS}[role])
    samplers[role] = node
for role, channel, prop in (() if not build_graph else (
        ('BaseColor', 'RGB', unreal.MaterialProperty.MP_BASE_COLOR),
        ('Normal', 'RGB', unreal.MaterialProperty.MP_NORMAL),
        ('ORM', 'R', unreal.MaterialProperty.MP_AMBIENT_OCCLUSION),
        ('ORM', 'G', unreal.MaterialProperty.MP_ROUGHNESS),
        ('ORM', 'B', unreal.MaterialProperty.MP_METALLIC))):
    if not LIB.connect_material_property(samplers[role], channel, prop):
        raise RuntimeError(f'Material connection failed: {role}/{channel}')
LIB.recompile_material(material)
unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
slots = mesh.get_editor_property('materials')
expected = {'Fur','Chest','Jacket','Seam','Skin','Eye','Claw','Metal','Whisker'}
if {str(s.material_slot_name).split('.')[0] for s in slots} != expected:
    raise RuntimeError('V1 material slots differ from contract')
for index,slot in enumerate(slots):
    slot.material_interface = material
    slots[index] = slot
mesh.set_editor_property('materials', slots)
unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False)
if any(slot.material_interface != material for slot in mesh.get_editor_property('materials')):
    raise RuntimeError('V1 material assignments did not persist')

report = {'mesh': mesh.get_path_name(), 'skeleton': skeleton.get_path_name(), 'clips': []}
for clip in manifest['clips']:
    options = unreal.FbxImportUI()
    options.import_mesh = False
    options.import_as_skeletal = True
    options.import_animations = True
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION
    options.automated_import_should_detect_type = False
    options.skeleton = skeleton
    options.import_materials = options.import_textures = False
    options.anim_sequence_import_data.set_editor_property('use_default_sample_rate', False)
    options.anim_sequence_import_data.set_editor_property('custom_sample_rate', 30)
    anim = import_asset(SOURCE/clip['file'], 'AS_Chuck_'+clip['name'], DEST+'/Animations', options)
    if not isinstance(anim, unreal.AnimSequence) or anim.get_editor_property('skeleton') != skeleton:
        raise RuntimeError(f"Wrong animation/skeleton for {clip['name']}")
    anim.set_editor_property('enable_root_motion', False)
    unreal.EditorAssetLibrary.save_loaded_asset(anim, only_if_is_dirty=False)
    report['clips'].append({'name': clip['name'], 'asset': anim.get_path_name(),
                            'imported_length_s': anim.get_editor_property('sequence_length'),
                            'source_duration_s': clip['duration_s'], 'loop': clip['loop']})

(ROOT/'Local/v1-import-report.json').write_text(json.dumps(report, indent=2))
runpy.run_path(str(ROOT/'Tools/validate_chuck_v1_import.py'),run_name='__main__')
unreal.log('CHUCK_V1_IMPORT_READY '+json.dumps(report))
