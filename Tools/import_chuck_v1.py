"""Import committed v1 FBXs/textures beside the live character; never runs Blender."""
from pathlib import Path
import json
import os
import runpy
import unreal

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT/'SourceAssets/Chuck/V1'
DEST = '/Game/Characters/Chuck/V1'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
LIB = unreal.MaterialEditingLibrary
manifest = json.loads((SOURCE/'Animations/manifest.json').read_text())
# CHUCK_V1_CLIPS_ONLY=SprintLoop,...: import just those clips against the
# existing mesh/skeleton; the mesh, textures, material and other clips stay as they are.
ONLY = [c for c in os.environ.get('CHUCK_V1_CLIPS_ONLY', '').split(',') if c]


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


def eye_material():
    """M_ChuckEye (2026-10-31, user: Chuck's eyes "more like this", a glossy black rat eye with a bright
    highlight). Near-black, wet (low roughness, strong specular), with a catchlight: a small bright spot
    where the eye's surface faces a light above and left of the camera, as a portrait lamp gives, so it
    shows whatever the scene's lights are doing. Built once; delete the asset to rebuild its graph.
    Also used by Tools/import_chuck_groom.py for SK_Chuck_Groomed."""
    path = DEST+'/M_ChuckEye'
    mat = unreal.load_asset(path)
    if mat: return mat
    mat = TOOLS.create_asset('M_ChuckEye', DEST, unreal.Material, unreal.MaterialFactoryNew())
    LIB.set_material_usage(mat, unreal.MaterialUsage.MATUSAGE_SKELETAL_MESH)
    def n(cls, x, y, **props):
        e = LIB.create_material_expression(mat, cls, x, y)
        for k, v in props.items(): e.set_editor_property(k, v)
        return e
    def link(a, b, pin, pa=''):
        if not LIB.connect_material_expressions(a, pa, b, pin): raise RuntimeError(f'M_ChuckEye link {pa}->{pin}')
    def vec3(x, y, v):
        return n(unreal.MaterialExpressionConstant3Vector, x, y, constant=unreal.LinearColor(*v, 1))
    base = n(unreal.MaterialExpressionVectorParameter, -500, -300, parameter_name='EyeColour', default_value=unreal.LinearColor(.011, .008, .006, 1))
    LIB.connect_material_property(base, '', unreal.MaterialProperty.MP_BASE_COLOR)
    LIB.connect_material_property(n(unreal.MaterialExpressionScalarParameter, -500, -150, parameter_name='Roughness', default_value=.06), '', unreal.MaterialProperty.MP_ROUGHNESS)
    LIB.connect_material_property(n(unreal.MaterialExpressionConstant, -500, -60, r=.9), '', unreal.MaterialProperty.MP_SPECULAR)
    # Catchlight direction: toward the camera, tilted up and to the left in view space.
    up = n(unreal.MaterialExpressionTransform, -1100, 100, transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_VIEW,
           transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    link(vec3(-1300, 100, (-.35, .55, 0.)), up, '')
    cam = n(unreal.MaterialExpressionCameraVectorWS, -1100, 0)
    add = n(unreal.MaterialExpressionAdd, -900, 50); link(cam, add, 'A'); link(up, add, 'B')
    light = n(unreal.MaterialExpressionNormalize, -750, 50); link(add, light, 'VectorInput')
    normal = n(unreal.MaterialExpressionVertexNormalWS, -750, 200)
    dot = n(unreal.MaterialExpressionDotProduct, -600, 100); link(normal, dot, 'A'); link(light, dot, 'B')
    sat = n(unreal.MaterialExpressionSaturate, -450, 100); link(dot, sat, '')
    sharp = n(unreal.MaterialExpressionScalarParameter, -450, 250, parameter_name='CatchSharpness', default_value=70.)
    pw = n(unreal.MaterialExpressionPower, -300, 100); link(sat, pw, 'Base'); link(sharp, pw, 'Exp')
    bright = n(unreal.MaterialExpressionScalarParameter, -300, 250, parameter_name='CatchBrightness', default_value=3.)
    em = n(unreal.MaterialExpressionMultiply, -150, 100); link(pw, em, 'A'); link(bright, em, 'B')
    LIB.connect_material_property(em, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    LIB.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat, only_if_is_dirty=False)
    return mat


def import_mesh_and_material():
    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = True
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    options.automated_import_should_detect_type = False
    options.import_materials = options.import_textures = options.import_animations = False
    options.create_physics_asset = False
    options.skeletal_mesh_import_data.normal_import_method = unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS
    # Rest-pose changes (e.g. the v1.1 shape amendment) must reach the existing
    # skeleton asset too, not just the mesh.
    options.skeletal_mesh_import_data.set_editor_property('update_skeleton_reference_pose', True)
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
    eye = eye_material()
    for index,slot in enumerate(slots):
        slot.material_interface = eye if str(slot.material_slot_name).split('.')[0] == 'Eye' else material
        slots[index] = slot
    mesh.set_editor_property('materials', slots)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False)
    if any(slot.material_interface != (eye if str(slot.material_slot_name).split('.')[0] == 'Eye' else material)
           for slot in mesh.get_editor_property('materials')):
        raise RuntimeError('V1 material assignments did not persist')
    return mesh, skeleton


if ONLY:
    mesh = unreal.load_asset(DEST+'/SK_Chuck')
    skeleton = mesh.get_editor_property('skeleton')
else:
    mesh, skeleton = import_mesh_and_material()
report = {'mesh': mesh.get_path_name(), 'skeleton': skeleton.get_path_name(), 'clips': []}
for clip in manifest['clips']:
    if ONLY and clip['name'] not in ONLY:
        continue
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
