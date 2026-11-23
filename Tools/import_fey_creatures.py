"""Import the Feywild's creatures (Tools/build_fey_creatures.py) into /Game/Art/Feywild/Creatures (Claude, 2026-11-22;
docs/regions/FEYWILD.md#stage-5-part-two).

UnrealEditor-Cmd <uproject> "-ExecutePythonScript=<absolute path to this file>" -unattended -nosplash -NoLiveCoding

  Material: the Phlegethos creatures' master (/Game/Art/Phlegethos/Creatures/M_PhlegCreature, built by
  Tools/import_phleg_creatures.py and only loaded here): vertex colour RGB the albedo, A the glow, a tiling detail normal.
  SK_Redcap      a skeletal mesh on the temple skeleton's own skeleton (/Game/Characters/Skeleton), so its clips play on it.
  SM_Displacer*, SM_ThornMite*   static parts, posed in code (FeyCreatures.cpp).
  M_FeyFur (2026-11-22, the user: free resources for a better displacer beast): ambientCG Fabric084's CC0 fur
                 (Tools/Fetch-FeyFur.ps1) projected from three sides in each part's own space over its vertex colours
                 (which keep the eyes' glow, the fangs and spikes); a Shimmer parameter dithers it out in flickering
                 patches with a blue sparkle when its image slips from one side of the beast to the other.
Only these assets and their instances are created or replaced.
"""
from pathlib import Path
import json
import unreal

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'SourceAssets/Enemies/Feywild'
DEST = '/Game/Art/Feywild/Creatures'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
ASSETS = unreal.EditorAssetLibrary
LIB = unreal.MaterialEditingLibrary
UV = json.loads((SOURCE / 'creatures.json').read_text())
ASSETS.make_directory(DEST)
MASTER = unreal.load_asset('/Game/Art/Phlegethos/Creatures/M_PhlegCreature')
if not MASTER: raise RuntimeError('M_PhlegCreature missing: run Tools/import_phleg_creatures.py first')


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


FUR_SRC = ROOT / 'SourceAssets/Enemies/Feywild/Fur'
F1, F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT1, unreal.CustomMaterialOutputType.CMOT_FLOAT3


def texture(name, file, kind):
    t = unreal.AssetImportTask()
    t.filename = str(FUR_SRC / file); t.destination_path = DEST; t.destination_name = name
    t.automated = True; t.replace_existing = True; t.save = False
    TOOLS.import_asset_tasks([t])
    tx = unreal.load_asset(f'{DEST}/{name}')
    if kind == 'normal':
        tx.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP); tx.set_editor_property('srgb', False)
    elif kind == 'mask':
        tx.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_MASKS); tx.set_editor_property('srgb', False)
    ASSETS.save_loaded_asset(tx, only_if_is_dirty=False)
    return tx


FUR = r"""
float3 n=normalize(LN);float3 w=pow(abs(n),4.);w/=dot(w,1);
float2 ua=P.yz/Scale,ub=P.xz/Scale,uc=P.xy/Scale;
float3 c=Texture2DSample(C,CSampler,ua).rgb*w.x+Texture2DSample(C,CSampler,ub).rgb*w.y+Texture2DSample(C,CSampler,uc).rgb*w.z;
float r=Texture2DSample(R,RSampler,ua).r*w.x+Texture2DSample(R,RSampler,ub).r*w.y+Texture2DSample(R,RSampler,uc).r*w.z;
float o=Texture2DSample(O,OSampler,ua).r*w.x+Texture2DSample(O,OSampler,ub).r*w.y+Texture2DSample(O,OSampler,uc).r*w.z;
float2 nx=Texture2DSample(NM,NMSampler,ua).rg*2-1,ny=Texture2DSample(NM,NMSampler,ub).rg*2-1,nz=Texture2DSample(NM,NMSampler,uc).rg*2-1;
float3 dn=float3(0,nx.x,nx.y)*w.x+float3(ny.x,0,ny.y)*w.y+float3(nz.x,nz.y,0)*w.z;
float lum=dot(c,float3(.3,.59,.11));lum=lerp(.42,lum,.6);   // (its tile-to-tile shading evened out: it showed as squares)
float3 vc=pow(max(VC.rgb,.0001),2.2);
float fur=1.-saturate(VA*3.);
float3 base=vc*lerp(1.,saturate(lum/.42)*1.3,fur)*Tint;
Rough=lerp(.45,saturate(r*.8),fur);   // (a sleek coat)AO=lerp(1.,o,fur);
WN=normalize(n+dn*NStr*fur);
float2 px=SP.xy*VS;   // (ScreenPosition is the viewport UV here)
float h=frac(52.9829189*frac(dot(px,float2(.06711056,.00583715))+View.StateFrameIndexMod8*.618));
float3 cell=floor(P/2.5);float sn=frac(sin(dot(cell,float3(12.9898,78.233,37.719))+floor(T*12.)*.37)*43758.5453);
Op=h>Shimmer*(.25+.75*sn)?1.:0.;
Glow=vc*VA*GlowStrength*GlowColor+Shimmer*float3(.12,.3,1.)*sn*1.6;
return base;
"""


def build_fur():
    path = f'{DEST}/M_FeyFur'
    mat = unreal.load_asset(path)
    if mat: LIB.delete_all_material_expressions(mat)
    else: mat = TOOLS.create_asset('M_FeyFur', DEST, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED); mat.set_editor_property('opacity_mask_clip_value', .5)
    mat.set_editor_property('tangent_space_normal', False)
    def node(cls, **props):
        e = LIB.create_material_expression(mat, cls)
        for k, v in props.items(): e.set_editor_property(k, v)
        return e
    def sca(name, v): return node(unreal.MaterialExpressionScalarParameter, parameter_name=name, default_value=v)
    def vec(name, rgb): return node(unreal.MaterialExpressionVectorParameter, parameter_name=name, default_value=unreal.LinearColor(*rgb, 1))
    def tob(t, normal=False): return node(unreal.MaterialExpressionTextureObject, texture=t, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if normal else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR if t == C else unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    ln = node(unreal.MaterialExpressionTransform, transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_WORLD, transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_LOCAL)
    LIB.connect_material_expressions(node(unreal.MaterialExpressionVertexNormalWS), '', ln, '')
    vc = node(unreal.MaterialExpressionVertexColor)
    inputs = {'P': node(unreal.MaterialExpressionLocalPosition), 'LN': ln, 'VC': vc, 'VA': (vc, 'A'), 'C': tob(C), 'R': tob(R), 'O': tob(O), 'NM': tob(N, True),
              'SP': node(unreal.MaterialExpressionScreenPosition), 'VS': node(unreal.MaterialExpressionViewSize), 'T': node(unreal.MaterialExpressionTime),
              'Scale': sca('Scale', 80.), 'NStr': sca('NormalStrength', .4), 'Tint': vec('Tint', (.27, .3, .4)), 'Shimmer': sca('Shimmer', 0.),
              'GlowColor': vec('GlowColor', (.45, 1., .55)), 'GlowStrength': sca('GlowStrength', 16.)}
    args = []
    for name in inputs:
        i = unreal.CustomInput(); i.set_editor_property('input_name', name); args.append(i)
    outs = []
    for name, typ in (('Rough', F1), ('AO', F1), ('WN', F3), ('Op', F1), ('Glow', F3)):
        o = unreal.CustomOutput(); o.set_editor_property('output_name', name); o.set_editor_property('output_type', typ); outs.append(o)
    c = node(unreal.MaterialExpressionCustom, code=FUR, inputs=args, output_type=F3, additional_outputs=outs)
    for name, expr in inputs.items():
        ok = LIB.connect_material_expressions(expr[0], expr[1], c, name) if isinstance(expr, tuple) else LIB.connect_material_expressions(expr, '', c, name)
        if not ok: raise RuntimeError(f'M_FeyFur: link {name}')
    wn = node(unreal.MaterialExpressionTransform, transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL, transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    LIB.connect_material_expressions(c, 'WN', wn, '')
    P_ = unreal.MaterialProperty
    for expr, pin, prop in ((c, '', P_.MP_BASE_COLOR), (c, 'Rough', P_.MP_ROUGHNESS), (c, 'AO', P_.MP_AMBIENT_OCCLUSION), (wn, '', P_.MP_NORMAL),
                            (c, 'Op', P_.MP_OPACITY_MASK), (c, 'Glow', P_.MP_EMISSIVE_COLOR)):
        if not LIB.connect_material_property(expr, pin, prop): raise RuntimeError(f'M_FeyFur: output {prop}')
    LIB.connect_material_property(node(unreal.MaterialExpressionConstant, r=.35), '', P_.MP_SPECULAR)
    LIB.recompile_material(mat)
    ASSETS.save_loaded_asset(mat, only_if_is_dirty=False)
    return mat


C = texture('T_FeyFur_C', 'Fabric084_2K-JPG_Color.jpg', 'color')
N = texture('T_FeyFur_N', 'Fabric084_2K-JPG_NormalDX.jpg', 'normal')
R = texture('T_FeyFur_R', 'Fabric084_2K-JPG_Roughness.jpg', 'mask')
O = texture('T_FeyFur_AO', 'Fabric084_2K-JPG_AmbientOcclusion.jpg', 'mask')
FUR_MAT = build_fur()
BONE_N = '/Game/Characters/Skeleton/T_SkeletonBone_N'
SCALES_N = '/Game/Characters/Snake/T_SnakeScales_N'
MI = {'SK_Redcap': instance('MI_Redcap', 'SK_Redcap', BONE_N, 6., {'Gamma': 2.2, 'Roughness': .62, 'DetailStrength': .35, 'GlowColor': (1., .78, .2), 'GlowStrength': 14.})}
for part in ('Body', 'Head', 'Upper', 'Lower', 'Tail', 'Tentacle', 'Pad'):
    MI[f'SM_Displacer{part}'] = FUR_MAT   # (2026-11-22: its fur, M_FeyFur; the C++ gives each part its own instance for the shimmer)
for part in ('Body', 'Leg'):
    MI[f'SM_ThornMite{part}'] = instance(f'MI_ThornMite{part}', f'SM_ThornMite{part}', BONE_N, 1.2,
                                         {'Gamma': 2.2, 'Roughness': .7, 'DetailStrength': .5, 'GlowColor': (1., .45, .12), 'GlowStrength': 18., 'Tint': (2.2, 2.1, 1.8)})   # (stage 8: they read near black)


def set_slots(mesh, mat):
    key = 'materials' if isinstance(mesh, unreal.SkeletalMesh) else 'static_materials'
    mats = mesh.get_editor_property(key)
    for i, m in enumerate(mats):
        m.material_interface = mat; mats[i] = m
    mesh.set_editor_property(key, mats)


skeleton = unreal.load_asset('/Game/Characters/Skeleton/SK_TempleSkeleton').get_editor_property('skeleton')
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
t.filename = str(SOURCE / 'SK_Redcap.fbx'); t.destination_path = DEST; t.destination_name = 'SK_Redcap'
t.automated = True; t.replace_existing = True; t.save = False; t.options = o
TOOLS.import_asset_tasks([t])
mesh = unreal.load_asset(f'{DEST}/SK_Redcap')
if not isinstance(mesh, unreal.SkeletalMesh) or mesh.get_editor_property('skeleton') != skeleton:
    raise RuntimeError('SK_Redcap: not a skeletal mesh on the temple skeleton')
set_slots(mesh, MI['SK_Redcap'])
lod_ok = False
try:
    lod_ok = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem).regenerate_lod(mesh, 3, True, False)
except Exception as e:
    unreal.log_warning(f'CHUCK_FEY_CREATURE_LOD_FAILED SK_Redcap {e}')
ASSETS.save_loaded_asset(mesh, only_if_is_dirty=False)
box = mesh.get_bounds().box_extent
unreal.log(f'CHUCK_FEY_CREATURE_IMPORTED SK_Redcap extent=({box.x:.0f},{box.y:.0f},{box.z:.0f}) lod_ok={lod_ok}')

for name in [n for n in MI if n.startswith('SM_')]:
    o = unreal.FbxImportUI()
    o.import_mesh = True; o.import_as_skeletal = False
    o.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    o.automated_import_should_detect_type = False
    o.import_materials = o.import_textures = o.import_animations = False
    d = o.static_mesh_import_data
    d.set_editor_property('combine_meshes', True)
    d.set_editor_property('auto_generate_collision', False)
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
    unreal.log(f'CHUCK_FEY_CREATURE_IMPORTED {name} extent=({box.x:.0f},{box.y:.0f},{box.z:.0f})')

unreal.log('CHUCK_FEY_CREATURES_READY')
