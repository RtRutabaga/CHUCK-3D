"""Import Phlegethos's lemure, Pit Fiend and spined devil (Tools/build_phleg_creatures.py) into /Game/Art/Phlegethos/Creatures
(Claude, 2026-11-16; docs/regions/PHLEGETHOS.md#creatures).

UnrealEditor-Cmd <uproject> "-ExecutePythonScript=<absolute path to this file>" -unattended -nosplash -NoLiveCoding

  M_PhlegCreature   the creatures' skin: vertex colour RGB the albedo (its occlusion baked in), A the glow (eyes, mouth, the
                    lemures' molten folds, the spines' tips), flickering; a tiling detail normal on the UVs (Tiling: the
                    UV scale from creatures.json over the detail's size). Parameters per creature in its instance.
                    Gamma 2.2: the FBX's linear vertex colours arrive gamma-encoded (at 1 the creatures read far too
                    bright and saturated, 2026-11-16).
  SK_Lemure, SK_PitFiend  skeletal meshes on the temple skeleton's own skeleton (/Game/Characters/Skeleton), so the
                    skeleton's clips play on them; LODs generated.
  SM_SpinedDevil_{Body,Head,WingL,WingR,Tail}, SM_DevilSpine  static parts; SM_DevilPerch (a basalt column, collision,
                    M_PhlegBasalt).
Only these assets are created or replaced. Add -ChuckPhlegOnly=SK_PitFiend (a comma list of mesh names) to the editor's
command line to re-import just those (the material and instances are always refreshed). (An environment variable did not
reach the editor's Python, 2026-11-16.)
"""
from pathlib import Path
import json
import unreal

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'SourceAssets/Enemies/Phlegethos'
DEST = '/Game/Art/Phlegethos/Creatures'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
ASSETS = unreal.EditorAssetLibrary
LIB = unreal.MaterialEditingLibrary
UV = json.loads((SOURCE / 'creatures.json').read_text())
ASSETS.make_directory(DEST)
ONLY = [n for a in unreal.SystemLibrary.get_command_line().split() if a.lower().startswith('-chuckphlegonly=') for n in a.split('=', 1)[1].strip('"').split(',') if n]
def wanted(name): return not ONLY or name in ONLY


def node(mat, cls, **props):
    n = LIB.create_material_expression(mat, cls)
    for k, v in props.items(): n.set_editor_property(k, v)
    return n


def link(a, b, pin_b, pin_a=''):
    if not LIB.connect_material_expressions(a, pin_a, b, pin_b):
        raise RuntimeError(f'Material link failed: {pin_a} -> {pin_b}')


def out(expr, prop, pin=''):
    if not LIB.connect_material_property(expr, pin, prop):
        raise RuntimeError(f'Material output failed: {prop}')


CODE = r'''
float3 c=pow(max(VC.rgb,0.0001),Gamma)*Tint;
Base=c;
float2 n2=Texture2DSample(D,DSampler,UV*Tiling).rg*2-1;
Nrm=normalize(float3(n2*Strength,1));
Rgh=saturate(Rough);
float fl=.78+.22*sin(T*9.+UV.x*40.)*sin(T*5.3+UV.y*31.);
return Glow*VA*GlowStrength*fl;
'''


def build_master():
    path = f'{DEST}/M_PhlegCreature'
    mat = unreal.load_asset(path)
    if mat: LIB.delete_all_material_expressions(mat)
    else: mat = TOOLS.create_asset('M_PhlegCreature', DEST, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('used_with_skeletal_mesh', True)
    F1, F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT1, unreal.CustomMaterialOutputType.CMOT_FLOAT3
    def sca(name, v): return node(mat, unreal.MaterialExpressionScalarParameter, parameter_name=name, default_value=v)
    def vec(name, rgb): return node(mat, unreal.MaterialExpressionVectorParameter, parameter_name=name, default_value=unreal.LinearColor(*rgb, 1))
    tex = node(mat, unreal.MaterialExpressionTextureObjectParameter, parameter_name='Detail',
               texture=unreal.load_asset('/Game/Characters/Snake/T_SnakeScales_N'), sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    vc = node(mat, unreal.MaterialExpressionVertexColor)
    inputs = {'VC': vc, 'VA': (vc, 'A'), 'UV': node(mat, unreal.MaterialExpressionTextureCoordinate),
              'T': node(mat, unreal.MaterialExpressionTime), 'D': tex, 'Gamma': sca('Gamma', 1.), 'Tint': vec('Tint', (1, 1, 1)),
              'Tiling': sca('Tiling', 30.), 'Strength': sca('DetailStrength', .6), 'Rough': sca('Roughness', .5),
              'Glow': vec('GlowColor', (1., .42, .1)), 'GlowStrength': sca('GlowStrength', 8.)}
    args = []
    for name in inputs:
        i = unreal.CustomInput(); i.set_editor_property('input_name', name); args.append(i)
    outs = []
    for name, typ in (('Base', F3), ('Nrm', F3), ('Rgh', F1)):
        o = unreal.CustomOutput(); o.set_editor_property('output_name', name); o.set_editor_property('output_type', typ); outs.append(o)
    c = node(mat, unreal.MaterialExpressionCustom, code=CODE, inputs=args, output_type=F3, additional_outputs=outs)
    for name, expr in inputs.items():
        if isinstance(expr, tuple): link(expr[0], c, name, expr[1])
        else: link(expr, c, name)
    out(c, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    out(c, unreal.MaterialProperty.MP_BASE_COLOR, 'Base')
    out(c, unreal.MaterialProperty.MP_NORMAL, 'Nrm')
    out(c, unreal.MaterialProperty.MP_ROUGHNESS, 'Rgh')
    LIB.recompile_material(mat)
    ASSETS.save_loaded_asset(mat, only_if_is_dirty=False)
    return mat


MASTER = build_master()


def instance(name, mesh_key, detail, detail_cm, params):
    mi = unreal.load_asset(f'{DEST}/{name}') or TOOLS.create_asset(name, DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property('parent', MASTER)
    LIB.set_material_instance_texture_parameter_value(mi, 'Detail', unreal.load_asset(detail))
    LIB.set_material_instance_scalar_parameter_value(mi, 'Tiling', UV[mesh_key]['uv_cm'] / detail_cm)
    for k, v in params.items():
        if isinstance(v, tuple): LIB.set_material_instance_vector_parameter_value(mi, k, unreal.LinearColor(*v, 1))
        else: LIB.set_material_instance_scalar_parameter_value(mi, k, v)
    LIB.update_material_instance(mi)
    ASSETS.save_loaded_asset(mi, only_if_is_dirty=False)
    return mi


BONE_N = '/Game/Characters/Skeleton/T_SkeletonBone_N'
SCALES_N = '/Game/Characters/Snake/T_SnakeScales_N'
MI = {
    # wet, molten flesh: a soft pitted detail, glossy; the folds glow a dull orange
    'SK_Lemure': instance('MI_Lemure', 'SK_Lemure', BONE_N, 9., {'Gamma': 2.2, 'Roughness': .32, 'DetailStrength': .45, 'GlowColor': (1., .32, .06), 'GlowStrength': 4.}),
    # smooth hide and iron (the goal's, 2026-11-16: the scales' detail kept faint); burning eyes, mouth and mace head
    'SK_PitFiend': instance('MI_PitFiend', 'SK_PitFiend', SCALES_N, 3.5, {'Gamma': 2.2, 'Roughness': .45, 'DetailStrength': .3, 'GlowColor': (1., .55, .12), 'GlowStrength': 30.}),
}
for part in ('Body', 'Head', 'WingL', 'WingR', 'Tail'):
    MI[f'SM_SpinedDevil_{part}'] = instance(f'MI_SpinedDevil_{part}', f'SM_SpinedDevil_{part}', SCALES_N, 1.6,
                                            {'Gamma': 2.2, 'Roughness': .55, 'DetailStrength': .55, 'GlowColor': (1., .5, .1), 'GlowStrength': 14.})
MI['SM_DevilSpine'] = instance('MI_DevilSpine', 'SM_DevilSpine', SCALES_N, 1., {'Gamma': 2.2, 'Roughness': .5, 'DetailStrength': .2, 'GlowColor': (1., .45, .08), 'GlowStrength': 40.})
MI['SM_DevilPerch'] = unreal.load_asset('/Game/Art/Phlegethos/M_PhlegBasalt')


def set_slots(mesh, mat):
    mats = mesh.get_editor_property('materials' if isinstance(mesh, unreal.SkeletalMesh) else 'static_materials')
    for i, m in enumerate(mats):
        m.material_interface = mat; mats[i] = m
    mesh.set_editor_property('materials' if isinstance(mesh, unreal.SkeletalMesh) else 'static_materials', mats)


skeleton = unreal.load_asset('/Game/Characters/Skeleton/SK_TempleSkeleton').get_editor_property('skeleton')
for name in ('SK_Lemure', 'SK_PitFiend'):
    if not wanted(name): continue
    o = unreal.FbxImportUI()
    o.import_mesh = True; o.import_as_skeletal = True
    o.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    o.automated_import_should_detect_type = False
    o.import_materials = o.import_textures = o.import_animations = False
    o.create_physics_asset = False
    o.skeleton = skeleton
    o.skeletal_mesh_import_data.set_editor_property('update_skeleton_reference_pose', False)
    o.skeletal_mesh_import_data.set_editor_property('vertex_color_import_option', unreal.VertexColorImportOption.REPLACE)
    o.skeletal_mesh_import_data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    t = unreal.AssetImportTask()
    t.filename = str(SOURCE / f'{name}.fbx'); t.destination_path = DEST; t.destination_name = name
    t.automated = True; t.replace_existing = True; t.save = False; t.options = o
    TOOLS.import_asset_tasks([t])
    mesh = unreal.load_asset(f'{DEST}/{name}')
    if not isinstance(mesh, unreal.SkeletalMesh) or mesh.get_editor_property('skeleton') != skeleton:
        raise RuntimeError(f'{name}: not a skeletal mesh on the temple skeleton')
    set_slots(mesh, MI[name])
    lod_ok = False
    try:
        lod_ok = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem).regenerate_lod(mesh, 3, True, False)
    except Exception as e:
        unreal.log_warning(f'CHUCK_PHLEG_CREATURE_LOD_FAILED {name} {e}')
    ASSETS.save_loaded_asset(mesh, only_if_is_dirty=False)
    # (2026-11-16: the fiend's wing and tail bones are merged into the shared skeleton by the import; keep them)
    ASSETS.save_loaded_asset(skeleton, only_if_is_dirty=True)
    box = mesh.get_bounds().box_extent
    unreal.log(f'CHUCK_PHLEG_CREATURE_IMPORTED {name} extent=({box.x:.0f},{box.y:.0f},{box.z:.0f}) lod_ok={lod_ok}')

for name in ('SM_SpinedDevil_Body', 'SM_SpinedDevil_Head', 'SM_SpinedDevil_WingL', 'SM_SpinedDevil_WingR', 'SM_SpinedDevil_Tail', 'SM_DevilSpine', 'SM_DevilPerch'):
    if not wanted(name): continue
    o = unreal.FbxImportUI()
    o.import_mesh = True; o.import_as_skeletal = False
    o.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    o.automated_import_should_detect_type = False
    o.import_materials = o.import_textures = o.import_animations = False
    d = o.static_mesh_import_data
    d.set_editor_property('combine_meshes', True)
    d.set_editor_property('auto_generate_collision', name == 'SM_DevilPerch')
    d.set_editor_property('vertex_color_import_option', unreal.VertexColorImportOption.REPLACE)
    d.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    d.set_editor_property('generate_lightmap_u_vs', False)
    t = unreal.AssetImportTask()
    t.filename = str(SOURCE / f'{name}.fbx'); t.destination_path = DEST; t.destination_name = name
    t.automated = True; t.replace_existing = True; t.save = False; t.options = o
    TOOLS.import_asset_tasks([t])
    mesh = unreal.load_asset(f'{DEST}/{name}')
    if not isinstance(mesh, unreal.StaticMesh): raise RuntimeError(f'{name}: import failed')
    set_slots(mesh, MI[name])
    ASSETS.save_loaded_asset(mesh, only_if_is_dirty=False)
    box = mesh.get_bounds().box_extent
    unreal.log(f'CHUCK_PHLEG_CREATURE_IMPORTED {name} extent=({box.x:.0f},{box.y:.0f},{box.z:.0f})')

unreal.log(f'CHUCK_PHLEG_CREATURES_READY meshes={len(ONLY) if ONLY else 9}')
