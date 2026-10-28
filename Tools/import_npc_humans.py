"""Import the human NPCs (Tools/build_npc_humans.py) into /Game/Characters/Humans.

UnrealEditor-Cmd <uproject> -ExecutePythonScript=Tools/import_npc_humans.py -unattended -nosplash -NoLiveCoding

Every human shares one skeleton (SKEL_Human: MPFB's game_engine rig, Unreal
mannequin bone names, three bones per finger) and four master materials; each NPC gets a material instance per slot
from SourceAssets/NPCs/Humans/manifest.json:
  M_HumanSkin    MakeHuman skin texture; subsurface (light through ears, nose and lips) and a tiling
                 pore/fine-line normal (skin_detail_normal.png, Tools/build_skin_detail.py) since 2026-10-28
  M_HumanEye     MakeHuman eye texture, glossy
  M_HumanTeeth   teeth and tongue (slot type "teeth", MakeHuman CC0): texture x Tint, glossy
  M_HumanCard    brows, lashes, hair: masked by the texture's alpha, two-sided
  M_HumanHair    beard locks (slot type "hair"): M_HumanCard with a dithered mask
  M_HumanStubble scalp stubble (slot type "stubble"): the card graph, its mask
                 times the vertex colour (the hair's density), dithered
  M_HumanFabric  Poly Haven cloth: weave = greyed colour map x Gain (brightness
                 normalised), coloured by Tint; AO/roughness, DirectX normal;
                 the mesh UVs are already in texture repeats (real size)
Everything under /Game/Characters/Humans is generated. Meshes, clips, textures
and instances are re-imported in place and existing masters kept; for a clean
rebuild (skeleton or master graph changes) use Tools/Import-NPCHumans.ps1 -Clean. A master
alone can be rebuilt: Import-NPCHumans.ps1 -RebuildMasters M_HumanSkin deletes just that master's
file before the editor starts (clearing its graph in a running editor trips an engine assertion);
the import makes it again and re-parents the instances of the NPCs it imports, so import them all.
To re-import only some of it (leaving the rest's assets untouched), set
CHUCK_HUMANS_ONLY to a comma list of NPC names and/or "clips", "props" (all of
them) or "prop:<Name>" (just that one, e.g. prop:Tankard)
(Tools/Import-NPCHumans.ps1 -Only Blacksmith,props).
"""
from pathlib import Path
import json
import os
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
    # Re-imported when its source is newer than the asset (a regenerated beard_locks.png, 2026-10-28).
    on_disk = ROOT / 'Unreal/Chuck3D/Content' / (asset[len('/Game/'):] + '.uasset')
    stale = tex and on_disk.exists() and path.stat().st_mtime > on_disk.stat().st_mtime
    if stale: print(f'CHUCK_HUMAN_TEXTURE_REIMPORT {path.name}')
    if not tex or stale:
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


def skin_detail_texture():
    return texture(ROOT / 'SourceAssets/NPCs/Humans/Textures/skin_detail_normal.png', '/Game/Characters/Humans/Textures', 'normal')


def skin_detail(mat, x=-900, y=600):
    """The tiling skin relief (Tools/build_skin_detail.py) as a normal: UV x DetailTile, eased toward flat
    by DetailStrength. Returns the node for MP_NORMAL. Shared with M_HeroSkin (Tools/import_temple_heroes.py)."""
    M = unreal.MaterialExpressionMultiply
    uv = node(mat, unreal.MaterialExpressionTextureCoordinate, x - 400, y)
    tile = node(mat, unreal.MaterialExpressionScalarParameter, x - 400, y + 120, parameter_name='DetailTile', default_value=20.)
    scaled = node(mat, M, x - 250, y)
    LIB.connect_material_expressions(uv, '', scaled, 'A'); LIB.connect_material_expressions(tile, '', scaled, 'B')
    t = tex_param(mat, 'DetailNormal', skin_detail_texture(), unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, x - 100, y)
    LIB.connect_material_expressions(scaled, '', t, 'UVs')
    flat = node(mat, unreal.MaterialExpressionConstant3Vector, x - 100, y + 250, constant=unreal.LinearColor(0, 0, 1, 1))
    k = node(mat, unreal.MaterialExpressionScalarParameter, x - 100, y + 350, parameter_name='DetailStrength', default_value=.85)
    out = node(mat, unreal.MaterialExpressionLinearInterpolate, x + 150, y)
    LIB.connect_material_expressions(flat, '', out, 'A'); LIB.connect_material_expressions(t, 'RGB', out, 'B')
    LIB.connect_material_expressions(k, '', out, 'Alpha')
    return out


def build_skin(mat, default):
    """Since 2026-10-28 ("make all huminoid npc's look better, in particular faces"): subsurface, as the
    heroes' skin already was, with the skin relief; a touch glossier. Tint (the zombie's dead grey) and
    SubsurfaceColor per instance."""
    M = unreal.MaterialExpressionMultiply
    t = tex_param(mat, 'Diffuse', default, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -800, 0)
    tint = node(mat, unreal.MaterialExpressionVectorParameter, -800, 250, parameter_name='Tint', default_value=unreal.LinearColor(1, 1, 1, 1))
    base = node(mat, M, -500, 0)
    LIB.connect_material_expressions(t, 'RGB', base, 'A'); LIB.connect_material_expressions(tint, '', base, 'B')
    LIB.connect_material_property(base, '', unreal.MaterialProperty.MP_BASE_COLOR)
    # Scatter kept low and its colour deep: at the heroes' .35 and bright red the townsfolk read as pink wax in daylight.
    for name, v, prop, y in (('Roughness', .48, unreal.MaterialProperty.MP_ROUGHNESS, 200), ('Specular', .42, unreal.MaterialProperty.MP_SPECULAR, 300),
                             ('Scatter', .2, unreal.MaterialProperty.MP_OPACITY, 500)):
        LIB.connect_material_property(node(mat, unreal.MaterialExpressionScalarParameter, -500, y, parameter_name=name, default_value=v), '', prop)
    # The light through the skin is the skin's own colour, deepened toward red (a fixed red lightened the darker skins).
    sss = node(mat, M, -300, 400)
    LIB.connect_material_expressions(base, '', sss, 'A')
    LIB.connect_material_expressions(node(mat, unreal.MaterialExpressionVectorParameter, -500, 450, parameter_name='SubsurfaceColor',
                                          default_value=unreal.LinearColor(1.2, .45, .3, 1)), '', sss, 'B')
    LIB.connect_material_property(sss, '', unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
    LIB.connect_material_property(skin_detail(mat), '', unreal.MaterialProperty.MP_NORMAL)


def build_teeth(mat, default):
    t = tex_param(mat, 'Diffuse', default, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -600, 0)
    tint = node(mat, unreal.MaterialExpressionVectorParameter, -600, 250, parameter_name='Tint', default_value=unreal.LinearColor(1, 1, 1, 1))
    mul = node(mat, unreal.MaterialExpressionMultiply, -300, 0)
    LIB.connect_material_expressions(t, 'RGB', mul, 'A'); LIB.connect_material_expressions(tint, '', mul, 'B')
    LIB.connect_material_property(mul, '', unreal.MaterialProperty.MP_BASE_COLOR)
    # Inside the mouth: rough enough not to mirror the sky, little specular, shaded (the lips and cheeks hide the light).
    constant(mat, .55, unreal.MaterialProperty.MP_ROUGHNESS, 300)
    constant(mat, .25, unreal.MaterialProperty.MP_SPECULAR, 400)
    constant(mat, .45, unreal.MaterialProperty.MP_AMBIENT_OCCLUSION, 500)


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


def build_stubble(mat, default):
    """The card graph with its opacity scaled by the vertex colour: the scalp
    shell's stubble density (build_npc_humans.make_scalp), full under the hair,
    fading out past the hairline."""
    t = tex_param(mat, 'Diffuse', default, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -600, 0)
    tint = node(mat, unreal.MaterialExpressionVectorParameter, -600, 250, parameter_name='Tint', default_value=unreal.LinearColor(1, 1, 1, 1))
    mul = node(mat, unreal.MaterialExpressionMultiply, -300, 0)
    LIB.connect_material_expressions(t, 'RGB', mul, 'A'); LIB.connect_material_expressions(tint, '', mul, 'B')
    LIB.connect_material_property(mul, '', unreal.MaterialProperty.MP_BASE_COLOR)
    vc = node(mat, unreal.MaterialExpressionVertexColor, -600, 450)
    mask = node(mat, unreal.MaterialExpressionMultiply, -300, 300)
    LIB.connect_material_expressions(t, 'A', mask, 'A'); LIB.connect_material_expressions(vc, 'R', mask, 'B')
    LIB.connect_material_property(mask, '', unreal.MaterialProperty.MP_OPACITY_MASK)
    constant(mat, .8, unreal.MaterialProperty.MP_ROUGHNESS, 600)


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


def fabric_textures(name, source=CLOTH):
    folder = f'{DEST}/Fabrics'
    return (texture(source / f'{name}_diff_2k.jpg', folder),
            texture(source / f'{name}_arm_2k.jpg', folder, 'masks'),
            texture(source / f'{name}_nor_dx_2k.jpg', folder, 'normal'))


def instance(name, folder, parent, textures, tint=None, gain=None, vectors=None):
    path = f'{folder}/{name}'
    mi = unreal.load_asset(path) or TOOLS.create_asset(name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    LIB.set_material_instance_parent(mi, parent)
    for param, tex in textures.items():
        LIB.set_material_instance_texture_parameter_value(mi, param, tex)
    if tint is not None:
        LIB.set_material_instance_vector_parameter_value(mi, 'Tint', unreal.LinearColor(*tint, 1))
    if gain is not None:
        LIB.set_material_instance_scalar_parameter_value(mi, 'Gain', gain)
    for k, v in (vectors or {}).items():
        LIB.set_material_instance_vector_parameter_value(mi, k, unreal.LinearColor(*v, 1))
    ASSETS.save_loaded_asset(mi, only_if_is_dirty=False)
    return mi


def slot_material(npc, folder, slot, info):
    tex_folder = f'{DEST}/Textures'
    if info['type'] == 'fabric':
        diff, arm, nor = fabric_textures(info['fabric'], ROOT / info['folder'] if 'folder' in info else CLOTH)
        parent = master('M_HumanFabric', lambda m: build_fabric(m, diff, arm, nor))
        return instance(f'MI_{npc}_{slot}', folder, parent, {'Diffuse': diff, 'ARM': arm, 'Normal': nor}, info['tint'], info['gain'])
    tex = texture(SOURCE / info['texture'], tex_folder)
    if info['type'] == 'skin':
        parent = master('M_HumanSkin', lambda m: build_skin(m, tex), shading_model=unreal.MaterialShadingModel.MSM_SUBSURFACE)
        return instance(f'MI_{npc}_{slot}', folder, parent, {'Diffuse': tex}, info.get('tint', (1, 1, 1)),
                        vectors={'SubsurfaceColor': info.get('subsurface', (1.2, .45, .3))})
    elif info['type'] == 'teeth':
        parent = master('M_HumanTeeth', lambda m: build_teeth(m, tex))
    elif info['type'] == 'eye':
        parent = master('M_HumanEye', lambda m: build_eye(m, tex), blend_mode=unreal.BlendMode.BLEND_MASKED)
    elif info['type'] == 'card':
        parent = master('M_HumanCard', lambda m: build_card(m, tex), blend_mode=unreal.BlendMode.BLEND_MASKED, two_sided=True)
    elif info['type'] == 'hair':   # beard locks: the card graph, its alpha edges dithered (soft under TAA), not cut hard
        parent = master('M_HumanHair', lambda m: build_card(m, tex), blend_mode=unreal.BlendMode.BLEND_MASKED, two_sided=True,
                        dither_opacity_mask=True, opacity_mask_clip_value=.25)
    elif info['type'] == 'stubble':   # scalp stubble: density from the vertex colour, dithered so it fades, not cuts
        parent = master('M_HumanStubble', lambda m: build_stubble(m, tex), blend_mode=unreal.BlendMode.BLEND_MASKED,
                        dither_opacity_mask=True, opacity_mask_clip_value=.25)
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
    data.set_editor_property('vertex_color_import_option', unreal.VertexColorImportOption.REPLACE)   # the scalp's stubble density
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


def import_props(names=None):
    """What the humans carry (Tools/build_spear.py): static meshes in the same fabric materials."""
    props_dir = ROOT / 'SourceAssets/NPCs/Props'
    folder = f'{DEST}/Props'
    for prop, info in json.loads((props_dir / 'manifest.json').read_text(encoding='utf-8'))['props'].items():
        if names and prop not in names: continue
        options = unreal.FbxImportUI()
        options.import_mesh = True
        options.import_as_skeletal = False
        options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
        options.automated_import_should_detect_type = False
        options.import_materials = options.import_textures = options.import_animations = False
        options.static_mesh_import_data.set_editor_property('combine_meshes', True)
        options.static_mesh_import_data.set_editor_property('auto_generate_collision', False)
        task = unreal.AssetImportTask()
        task.filename = str(props_dir / info['fbx']); task.destination_path = folder; task.destination_name = f'SM_{prop}'
        task.automated = True; task.replace_existing = True; task.save = True; task.options = options
        TOOLS.import_asset_tasks([task])
        mesh = unreal.load_asset(f'{folder}/SM_{prop}')
        if not isinstance(mesh, unreal.StaticMesh):
            raise RuntimeError(f'{prop} did not import as a static mesh')
        materials = {slot: slot_material(prop, folder, slot, s) for slot, s in info['slots'].items()}
        mats = mesh.get_editor_property('static_materials')
        for index, m in enumerate(mats):
            slot = str(m.get_editor_property('material_slot_name')).split('.')[0]
            if slot not in materials:
                raise RuntimeError(f'Unexpected {prop} slot {slot}')
            m.set_editor_property('material_interface', materials[slot])
            mats[index] = m          # the list holds copies: write each slot back
        mesh.set_editor_property('static_materials', mats)
        ASSETS.save_loaded_asset(mesh, only_if_is_dirty=False)
        box = mesh.get_bounds().box_extent
        print(f'CHUCK_HUMAN_PROP {prop} slots={sorted(materials)} extent=({box.x:.1f},{box.y:.1f},{box.z:.1f})')


ONLY = {x.strip() for x in os.environ.get('CHUCK_HUMANS_ONLY', '').split(',') if x.strip()}
for npc, info in MANIFEST['npcs'].items():
    if not ONLY or npc in ONLY: import_mesh(npc, info)
if not ONLY or 'clips' in ONLY: import_clips()
PROPS = {x[5:] for x in ONLY if x.startswith('prop:')}
if not ONLY or 'props' in ONLY: import_props()
elif PROPS: import_props(PROPS)
print('CHUCK_HUMANS_IMPORT_READY', sorted(MANIFEST['npcs']), 'only=' + (','.join(sorted(ONLY)) or 'all'))
