"""Import the human NPCs (Tools/build_npc_humans.py) into /Game/Characters/Humans.

UnrealEditor-Cmd <uproject> -ExecutePythonScript=Tools/import_npc_humans.py -unattended -nosplash -NoLiveCoding

Every human shares one skeleton (SKEL_Human: MPFB's cmu_mb rig, CMU BVH bone
names) and four master materials; each NPC gets a material instance per slot
from SourceAssets/NPCs/Humans/manifest.json:
  M_HumanSkin    MakeHuman skin texture
  M_HumanEye     MakeHuman eye texture, glossy
  M_HumanCard    brows, lashes, hair: masked by the texture's alpha, two-sided
  M_HumanFabric  Poly Haven cloth: weave = greyed colour map x Gain (brightness
                 normalised), coloured by Tint; AO/roughness, DirectX normal;
                 the mesh UVs are already in texture repeats (real size)
Everything under /Game/Characters/Humans is generated. Meshes, clips, textures
and instances are re-imported in place and existing masters kept; for a clean
rebuild (skeleton or master graph changes) use Tools/Import-NPCHumans.ps1 -Clean.
"""
from pathlib import Path
import json
import unreal

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'SourceAssets/NPCs/Humans'
CLOTH = ROOT / 'SourceAssets/Surfaces/Cloth'
DEST = '/Game/Characters/Humans'
SKELETON = f'{DEST}/SKEL_Human'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
LIB = unreal.MaterialEditingLibrary
ASSETS = unreal.EditorAssetLibrary
MANIFEST = json.loads((SOURCE / 'manifest.json').read_text(encoding='utf-8'))


def texture(path, folder, kind='color'):
    """Import (or reuse) a texture; kind: color | masks | normal."""
    path = Path(path)
    asset = f'{folder}/T_{path.stem}'
    tex = unreal.load_asset(asset)
    if not tex:
        task = unreal.AssetImportTask()
        task.filename = str(path); task.destination_path = folder; task.destination_name = f'T_{path.stem}'
        task.automated = True; task.replace_existing = True; task.save = True
        TOOLS.import_asset_tasks([task])
        tex = unreal.load_asset(asset)
        if not tex: raise RuntimeError(f'Texture import failed: {path}')
    # Compression first (it resets sRGB); import can take a pale weave for a normal map.
    compression, srgb = {'normal': ('TC_NORMALMAP', False), 'masks': ('TC_MASKS', False)}.get(kind, ('TC_DEFAULT', True))
    tex.set_editor_property('compression_settings', getattr(unreal.TextureCompressionSettings, compression))
    tex.set_editor_property('srgb', srgb)
    if tex.get_editor_property('srgb') != srgb: raise RuntimeError(f'{path.name}: sRGB did not stick')
    ASSETS.save_loaded_asset(tex, only_if_is_dirty=False)
    return tex


def node(mat, cls, x=-400, y=0, **props):
    n = LIB.create_material_expression(mat, cls, x, y)
    for k, v in props.items(): n.set_editor_property(k, v)
    return n


def tex_param(mat, name, default, sampler, x, y):
    return node(mat, unreal.MaterialExpressionTextureSampleParameter2D, x, y, parameter_name=name, texture=default, sampler_type=sampler)


def master(name, build, **flags):
    mat = unreal.load_asset(f'{DEST}/{name}')
    if mat: return mat
    mat = TOOLS.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    try:
        build(mat)
        mat.set_editor_property('used_with_skeletal_mesh', True)
        for k, v in flags.items(): mat.set_editor_property(k, v)
    except Exception:
        ASSETS.delete_loaded_asset(mat)
        raise
    LIB.recompile_material(mat)
    ASSETS.save_loaded_asset(mat, only_if_is_dirty=False)
    return mat


def constant(mat, value, prop, y):
    LIB.connect_material_property(node(mat, unreal.MaterialExpressionConstant, -250, y, r=value), '', prop)


def build_skin(mat, default):
    t = tex_param(mat, 'Diffuse', default, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -500, 0)
    LIB.connect_material_property(t, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
    constant(mat, .55, unreal.MaterialProperty.MP_ROUGHNESS, 200)
    constant(mat, .35, unreal.MaterialProperty.MP_SPECULAR, 300)


def build_eye(mat, default):
    """Eyeball and cornea share the texture; the cornea's corner of it is clear (alpha)."""
    t = tex_param(mat, 'Diffuse', default, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -500, 0)
    LIB.connect_material_property(t, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
    LIB.connect_material_property(t, 'A', unreal.MaterialProperty.MP_OPACITY_MASK)
    constant(mat, .12, unreal.MaterialProperty.MP_ROUGHNESS, 200)


def build_card(mat, default):
    t = tex_param(mat, 'Diffuse', default, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -600, 0)
    tint = node(mat, unreal.MaterialExpressionVectorParameter, -600, 250, parameter_name='Tint', default_value=unreal.LinearColor(1, 1, 1, 1))
    mul = node(mat, unreal.MaterialExpressionMultiply, -300, 0)
    LIB.connect_material_expressions(t, 'RGB', mul, 'A'); LIB.connect_material_expressions(tint, '', mul, 'B')
    LIB.connect_material_property(mul, '', unreal.MaterialProperty.MP_BASE_COLOR)
    LIB.connect_material_property(t, 'A', unreal.MaterialProperty.MP_OPACITY_MASK)
    constant(mat, .75, unreal.MaterialProperty.MP_ROUGHNESS, 300)


def build_fabric(mat, diff, arm, nor):
    d = tex_param(mat, 'Diffuse', diff, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -700, -200)
    a = tex_param(mat, 'ARM', arm, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, -700, 100)
    n = tex_param(mat, 'Normal', nor, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, -700, 400)
    # Weave only: grey the fabric, normalise its brightness (Gain), colour it (Tint).
    tint = node(mat, unreal.MaterialExpressionVectorParameter, -700, -450, parameter_name='Tint', default_value=unreal.LinearColor(.5, .5, .5, 1))
    gain = node(mat, unreal.MaterialExpressionScalarParameter, -700, -550, parameter_name='Gain', default_value=1.)
    grey = node(mat, unreal.MaterialExpressionDotProduct, -450, -250)   # linear luminance, as build_npc_humans measures it
    LIB.connect_material_expressions(d, 'RGB', grey, 'A')
    LIB.connect_material_expressions(node(mat, unreal.MaterialExpressionConstant3Vector, -600, -300, constant=unreal.LinearColor(.2126, .7152, .0722, 1)), '', grey, 'B')
    colour = node(mat, unreal.MaterialExpressionMultiply, -300, -450)
    LIB.connect_material_expressions(tint, '', colour, 'A'); LIB.connect_material_expressions(gain, '', colour, 'B')
    mul = node(mat, unreal.MaterialExpressionMultiply, -150, -250)
    LIB.connect_material_expressions(grey, '', mul, 'A'); LIB.connect_material_expressions(colour, '', mul, 'B')
    LIB.connect_material_property(mul, '', unreal.MaterialProperty.MP_BASE_COLOR)
    LIB.connect_material_property(a, 'R', unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    LIB.connect_material_property(a, 'G', unreal.MaterialProperty.MP_ROUGHNESS)
    LIB.connect_material_property(a, 'B', unreal.MaterialProperty.MP_METALLIC)   # steel helmets and plate; cloth is 0
    LIB.connect_material_property(n, 'RGB', unreal.MaterialProperty.MP_NORMAL)
    constant(mat, .3, unreal.MaterialProperty.MP_SPECULAR, 650)


def fabric_textures(name):
    folder = f'{DEST}/Fabrics'
    return (texture(CLOTH / f'{name}_diff_2k.jpg', folder),
            texture(CLOTH / f'{name}_arm_2k.jpg', folder, 'masks'),
            texture(CLOTH / f'{name}_nor_dx_2k.jpg', folder, 'normal'))


def instance(name, folder, parent, textures, tint=None, gain=None):
    path = f'{folder}/{name}'
    mi = unreal.load_asset(path) or TOOLS.create_asset(name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    LIB.set_material_instance_parent(mi, parent)
    for param, tex in textures.items():
        LIB.set_material_instance_texture_parameter_value(mi, param, tex)
    if tint is not None:
        LIB.set_material_instance_vector_parameter_value(mi, 'Tint', unreal.LinearColor(*tint, 1))
    if gain is not None:
        LIB.set_material_instance_scalar_parameter_value(mi, 'Gain', gain)
    ASSETS.save_loaded_asset(mi, only_if_is_dirty=False)
    return mi


def slot_material(npc, folder, slot, info):
    tex_folder = f'{DEST}/Textures'
    if info['type'] == 'fabric':
        diff, arm, nor = fabric_textures(info['fabric'])
        parent = master('M_HumanFabric', lambda m: build_fabric(m, diff, arm, nor))
        return instance(f'MI_{npc}_{slot}', folder, parent, {'Diffuse': diff, 'ARM': arm, 'Normal': nor}, info['tint'], info['gain'])
    tex = texture(SOURCE / info['texture'], tex_folder)
    if info['type'] == 'skin':
        parent = master('M_HumanSkin', lambda m: build_skin(m, tex))
    elif info['type'] == 'eye':
        parent = master('M_HumanEye', lambda m: build_eye(m, tex), blend_mode=unreal.BlendMode.BLEND_MASKED)
    elif info['type'] == 'card':
        parent = master('M_HumanCard', lambda m: build_card(m, tex), blend_mode=unreal.BlendMode.BLEND_MASKED, two_sided=True)
    else:
        raise RuntimeError(f'Unknown slot type {info["type"]}')
    return instance(f'MI_{npc}_{slot}', folder, parent, {'Diffuse': tex}, info.get('tint'))


def import_mesh(npc, info):
    folder = f'{DEST}/{npc}'
    skeleton = unreal.load_asset(SKELETON)
    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = True
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    options.automated_import_should_detect_type = False
    options.import_materials = options.import_textures = options.import_animations = False
    options.create_physics_asset = False
    if skeleton: options.skeleton = skeleton
    data = options.skeletal_mesh_import_data
    data.set_editor_property('update_skeleton_reference_pose', False)
    data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    task = unreal.AssetImportTask()
    task.filename = str(SOURCE / info['fbx']); task.destination_path = folder; task.destination_name = f'SK_{npc}'
    task.automated = True; task.replace_existing = True; task.save = True; task.options = options
    TOOLS.import_asset_tasks([task])
    mesh = unreal.load_asset(f'{folder}/SK_{npc}')
    if not isinstance(mesh, unreal.SkeletalMesh):
        raise RuntimeError(f'{npc} mesh is not skeletal')
    if not skeleton:   # the first human's skeleton becomes everyone's
        made = mesh.get_editor_property('skeleton')
        if not ASSETS.rename_asset(made.get_path_name().split('.')[0], SKELETON):
            raise RuntimeError('Could not move the skeleton to ' + SKELETON)
        mesh = unreal.load_asset(f'{folder}/SK_{npc}')
    materials = {slot: slot_material(npc, folder, slot, s) for slot, s in info['slots'].items()}
    mats = mesh.get_editor_property('materials')
    for index, m in enumerate(mats):
        slot = str(m.get_editor_property('material_slot_name')).split('.')[0]
        if slot not in materials:
            raise RuntimeError(f'Unexpected {npc} slot {slot}')
        m.material_interface = materials[slot]
        mats[index] = m          # the list holds copies: write each slot back
    mesh.set_editor_property('materials', mats)
    ASSETS.save_loaded_asset(mesh, only_if_is_dirty=False)
    mats = mesh.get_editor_property('materials')
    if any(m.material_interface != materials[str(m.material_slot_name).split('.')[0]] for m in mats):
        raise RuntimeError(f'{npc} material assignments did not persist')
    skel = mesh.get_editor_property('skeleton')
    if skel.get_path_name().split('.')[0] != SKELETON:
        raise RuntimeError(f'{npc} is not on the shared skeleton')
    ASSETS.save_loaded_asset(skel, only_if_is_dirty=False)
    box = mesh.get_bounds().box_extent
    print(f'CHUCK_HUMAN_IMPORTED {npc} slots={sorted(materials)} extent=({box.x:.1f},{box.y:.1f},{box.z:.1f})')


# A clean rebuild deletes the folder on disk before the editor starts: Tools/Import-NPCHumans.ps1 -Clean.
def import_clips():
    """The motion-capture clips (Tools/build_npc_mocap.py) onto the shared skeleton."""
    clips = json.loads((SOURCE / 'Anim/manifest.json').read_text(encoding='utf-8'))['clips']
    skeleton = unreal.load_asset(SKELETON)
    for clip, info in clips.items():
        options = unreal.FbxImportUI()
        options.import_mesh = False
        options.import_as_skeletal = True
        options.import_animations = True
        options.import_materials = options.import_textures = False
        options.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION
        options.automated_import_should_detect_type = False
        options.skeleton = skeleton
        data = options.anim_sequence_import_data
        data.set_editor_property('animation_length', unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
        data.set_editor_property('remove_redundant_keys', False)
        data.set_editor_property('import_bone_tracks', True)
        task = unreal.AssetImportTask()
        task.filename = str(SOURCE / info['fbx']); task.destination_path = f'{DEST}/Anim'; task.destination_name = f'AS_Human_{clip}'
        task.automated = True; task.replace_existing = True; task.save = True; task.options = options
        TOOLS.import_asset_tasks([task])
        anim = unreal.load_asset(f'{DEST}/Anim/AS_Human_{clip}')
        if not isinstance(anim, unreal.AnimSequence):
            raise RuntimeError(f'Clip {clip} did not import as an animation')
        if anim.get_editor_property('skeleton').get_path_name().split('.')[0] != SKELETON:
            raise RuntimeError(f'Clip {clip} is not on the shared skeleton')
        try: seconds = anim.get_play_length()
        except Exception: seconds = -1.
        print(f'CHUCK_HUMAN_CLIP {clip} seconds={seconds:.1f}')


for npc, info in MANIFEST['npcs'].items():
    import_mesh(npc, info)
import_clips()
print('CHUCK_HUMANS_IMPORT_READY', sorted(MANIFEST['npcs']))
