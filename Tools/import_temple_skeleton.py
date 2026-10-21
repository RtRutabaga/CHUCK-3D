"""Import the temple skeleton (Tools/build_temple_skeleton.py) into /Game/Characters/Skeleton (Claude, 2026-10-21).

UnrealEditor-Cmd <uproject> -ExecutePythonScript=Tools/import_temple_skeleton.py -unattended -nullrhi -nosplash -NoLiveCoding

SK_TempleSkeleton (its own skeleton asset), LODs generated from the 104k-triangle base, the clips
AS_Skeleton_<Clip> (no root motion: the runtime moves it), the generated bone
detail textures and M_SkeletonBone: aged, dry bone, darker and dirtier in its
crevices (the baked ambient occlusion in the vertex colour), stained in broad
patches, pitted and finely cracked, worn paler on the edges. MI_SkeletonBone
and MI_SkeletonTeeth on the mesh's two slots. Only skeleton assets are created
or replaced.
"""
from pathlib import Path
import json
import unreal

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'SourceAssets/Enemies/Skeleton'
DEST = '/Game/Characters/Skeleton'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
LIB = unreal.MaterialEditingLibrary
ASSETS = unreal.EditorAssetLibrary
CLIPS = json.loads((SOURCE / 'skeleton-clips.json').read_text())['clips']


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
    if normal: tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
    tex.set_editor_property('srgb', srgb and not normal)
    ASSETS.save_loaded_asset(tex, only_if_is_dirty=False)
    return tex


def vec(mat, name, rgb, x, y):
    return node(mat, unreal.MaterialExpressionVectorParameter, x, y, parameter_name=name, default_value=unreal.LinearColor(*rgb, 1))


def sca(mat, name, v, x, y):
    return node(mat, unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=v)


def mul(mat, a, b, x, y, pa='', pb=''):
    m = node(mat, unreal.MaterialExpressionMultiply, x, y); link(a, m, 'A', pa); link(b, m, 'B', pb); return m


def lerp(mat, a, b, alpha, x, y, palpha=''):
    l = node(mat, unreal.MaterialExpressionLinearInterpolate, x, y); link(a, l, 'A'); link(b, l, 'B'); link(alpha, l, 'Alpha', palpha); return l


detail = texture('T_SkeletonBone', srgb=False)
detail_n = texture('T_SkeletonBone_N', normal=True)

path = f'{DEST}/M_SkeletonBone'
# Built once (rebuilding an existing graph trips an engine assertion); delete M_SkeletonBone to rebuild it.
mat = unreal.load_asset(path)
fresh = mat is None
if fresh:
    mat = TOOLS.create_asset('M_SkeletonBone', DEST, unreal.Material, unreal.MaterialFactoryNew())
mat.set_editor_property('used_with_skeletal_mesh', True)
if fresh:
    uv = node(mat, unreal.MaterialExpressionTextureCoordinate, -1600, 200)
    tiling = sca(mat, 'DetailTiling', 3., -1600, 320)
    tuv = mul(mat, uv, tiling, -1420, 250)
    tex = node(mat, unreal.MaterialExpressionTextureSample, -1200, 100, texture=detail, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    link(tuv, tex, 'UVs')
    nrm = node(mat, unreal.MaterialExpressionTextureSample, -1200, 600, texture=detail_n, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    link(tuv, nrm, 'UVs')
    vc = node(mat, unreal.MaterialExpressionVertexColor, -1200, -300)
    bone = vec(mat, 'BoneColor', (.52, .45, .33), -1000, -600)
    stain = vec(mat, 'StainColor', (.27, .21, .13), -1000, -500)
    pale = vec(mat, 'WornColor', (.66, .6, .48), -1000, -400)
    dirt = vec(mat, 'DirtColor', (.045, .034, .022), -1000, -700)
    stain_amt = sca(mat, 'StainAmount', .6, -1000, -200)
    c1 = lerp(mat, bone, stain, mul(mat, tex, stain_amt, -800, -150, 'R'), -600, -550)
    c2 = lerp(mat, c1, pale, mul(mat, tex, node(mat, unreal.MaterialExpressionConstant, -1000, -100, r=.45), -800, -50, 'A'), -450, -500)
    pit = lerp(mat, node(mat, unreal.MaterialExpressionConstant, -700, 50, r=1.), node(mat, unreal.MaterialExpressionConstant, -700, 120, r=.45), tex, -550, 50, 'B')
    c3 = mul(mat, c2, pit, -300, -450)
    ao = node(mat, unreal.MaterialExpressionPower, -900, -300, const_exponent=1.4); link(vc, ao, 'Base', 'R')
    c4 = lerp(mat, dirt, c3, ao, -150, -450)
    out(c4, unreal.MaterialProperty.MP_BASE_COLOR)
    rough = sca(mat, 'Roughness', .68, -500, 250)
    r1 = node(mat, unreal.MaterialExpressionAdd, -300, 300); link(rough, r1, 'A'); link(mul(mat, tex, node(mat, unreal.MaterialExpressionConstant, -700, 380, r=.25), -500, 350, 'B'), r1, 'B')
    r2 = node(mat, unreal.MaterialExpressionSubtract, -150, 320); link(r1, r2, 'A'); link(mul(mat, tex, node(mat, unreal.MaterialExpressionConstant, -700, 460, r=.18), -500, 430, 'A'), r2, 'B')
    out(r2, unreal.MaterialProperty.MP_ROUGHNESS)
    out(sca(mat, 'Specular', .38, -150, 500), unreal.MaterialProperty.MP_SPECULAR)
    out(nrm, unreal.MaterialProperty.MP_NORMAL)
    out(ao, unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
LIB.recompile_material(mat)
ASSETS.save_loaded_asset(mat, only_if_is_dirty=False)

SLOTS = {
    # Old bone, after centuries in a damp temple (2026-10-21 review: the first pass read as clean, creamy and new).
    'Bone': ('MI_SkeletonBone', {'BoneColor': (.33, .27, .18), 'StainColor': (.13, .09, .05), 'WornColor': (.47, .41, .3), 'DirtColor': (.028, .021, .014), 'StainAmount': .78, 'Roughness': .74}),
    'Teeth': ('MI_SkeletonTeeth', {'BoneColor': (.34, .26, .14), 'StainColor': (.14, .09, .04), 'WornColor': (.44, .37, .24), 'DirtColor': (.028, .021, .014), 'StainAmount': .85, 'Roughness': .5, 'Specular': .45}),
}
instances = {}
for slot, (name, params) in SLOTS.items():
    mi = unreal.load_asset(f'{DEST}/{name}') or TOOLS.create_asset(name, DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property('parent', mat)
    for k, v in params.items():
        if isinstance(v, tuple): LIB.set_material_instance_vector_parameter_value(mi, k, unreal.LinearColor(*v, 1))
        else: LIB.set_material_instance_scalar_parameter_value(mi, k, v)
    LIB.update_material_instance(mi)
    ASSETS.save_loaded_asset(mi, only_if_is_dirty=False)
    instances[slot] = mi

options = unreal.FbxImportUI()
options.import_mesh = True
options.import_as_skeletal = True
options.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
options.automated_import_should_detect_type = False
options.import_materials = options.import_textures = options.import_animations = False
options.create_physics_asset = False   # it collapses by motion capture, not as a ragdoll
options.skeletal_mesh_import_data.set_editor_property('update_skeleton_reference_pose', True)
options.skeletal_mesh_import_data.set_editor_property('vertex_color_import_option', unreal.VertexColorImportOption.REPLACE)
options.skeletal_mesh_import_data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
task = unreal.AssetImportTask()
task.filename = str(SOURCE / 'SK_TempleSkeleton.fbx'); task.destination_path = DEST; task.destination_name = 'SK_TempleSkeleton'
task.automated = True; task.replace_existing = True; task.save = True; task.options = options
TOOLS.import_asset_tasks([task])
mesh = unreal.load_asset(f'{DEST}/SK_TempleSkeleton')
if not isinstance(mesh, unreal.SkeletalMesh): raise RuntimeError('Skeleton mesh is not skeletal')
mats = mesh.get_editor_property('materials')
for index, m in enumerate(mats):
    slot = str(m.get_editor_property('material_slot_name')).split('.')[0]
    if slot not in instances: raise RuntimeError(f'Unexpected skeleton slot {slot}')
    m.material_interface = instances[slot]
    mats[index] = m
mesh.set_editor_property('materials', mats)
# LODs from the base: about 50 / 22 / 9 % of its triangles.
lod_ok = False
try:
    sub = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    lod_ok = sub.regenerate_lod(mesh, 4, True, False)
except Exception as e:
    unreal.log_warning(f'CHUCK_SKELETON_LOD_FAILED {e}')
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
    o.anim_sequence_import_data.set_editor_property('custom_sample_rate', 30)
    t = unreal.AssetImportTask()
    t.filename = str(SOURCE / clip['fbx']); t.destination_path = DEST + '/Animations'; t.destination_name = f'AS_Skeleton_{name}'
    t.automated = True; t.replace_existing = True; t.save = False; t.options = o
    TOOLS.import_asset_tasks([t])
    anim = unreal.load_asset(f'{DEST}/Animations/AS_Skeleton_{name}')
    if not isinstance(anim, unreal.AnimSequence) or anim.get_editor_property('skeleton') != skeleton:
        raise RuntimeError(f'Wrong animation/skeleton for {name}')
    anim.set_editor_property('enable_root_motion', False)
    ASSETS.save_loaded_asset(anim, only_if_is_dirty=False)
    unreal.log(f'CHUCK_SKELETON_CLIP_IMPORT {name} length={anim.get_editor_property("sequence_length"):.3f} source={clip["frames"] / clip["fps"]:.3f}')
box = mesh.get_bounds().box_extent
sub = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
lods = [sub.get_num_verts(mesh, i) for i in range(sub.get_lod_count(mesh))]
unreal.log(f'CHUCK_SKELETON_IMPORTED slots={[str(m.material_slot_name) for m in mesh.get_editor_property("materials")]} '
           f'extent=({box.x:.1f},{box.y:.1f},{box.z:.1f}) lod_verts={lods} lod_ok={lod_ok} clips={len(CLIPS)}')
