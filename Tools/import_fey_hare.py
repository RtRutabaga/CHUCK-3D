"""Import the Feywild's great hare (Tools/build_fey_hare.py) into /Game/Art/Feywild/Creatures/Hare (Claude, 2026-11-25;
docs/regions/FEYWILD-HARE.md).

UnrealEditor-Cmd <uproject> "-ExecutePythonScript=<absolute path to this file>" -unattended -nosplash -NoLiveCoding

  SK_FeyHare     a skeletal mesh on a skeleton of its own (SKEL_FeyHare); posed in code (FeyHare.cpp, a poseable mesh).
  M_FeyHareFur   its coat: the displacer's CC0 fur (ambientCG Fabric084, already imported as T_FeyFur_* by
                 Tools/import_fey_creatures.py; only loaded here) projected from three sides in the mesh's own rest
                 space (pre-skinned, so it does not swim as the hare moves) over its vertex colours; vertex alpha is
                 the sleek mask (eyes, nose, claws: no fur there, glossy). A Tint parameter colours each hare (the game
                 gives each its own instance: moss-grey to dusk-gold across the herd). Opaque, no glow.
Only these assets are created or replaced.
"""
from pathlib import Path
import json
import unreal

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'SourceAssets/Enemies/Feywild/Hare'
DEST = '/Game/Art/Feywild/Creatures/Hare'
FUR = '/Game/Art/Feywild/Creatures'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
ASSETS = unreal.EditorAssetLibrary
LIB = unreal.MaterialEditingLibrary
INFO = json.loads((SOURCE / 'hare.json').read_text())
ASSETS.make_directory(DEST)
F1, F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT1, unreal.CustomMaterialOutputType.CMOT_FLOAT3

CODE = r"""
float3 n=normalize(LN);float3 w=pow(abs(n),4.);w/=dot(w,1);
float2 ua=P.yz/Scale,ub=P.xz/Scale,uc=P.xy/Scale;
float3 c=Texture2DSample(C,CSampler,ua).rgb*w.x+Texture2DSample(C,CSampler,ub).rgb*w.y+Texture2DSample(C,CSampler,uc).rgb*w.z;
float r=Texture2DSample(R,RSampler,ua).r*w.x+Texture2DSample(R,RSampler,ub).r*w.y+Texture2DSample(R,RSampler,uc).r*w.z;
float o=Texture2DSample(O,OSampler,ua).r*w.x+Texture2DSample(O,OSampler,ub).r*w.y+Texture2DSample(O,OSampler,uc).r*w.z;
float2 nx=Texture2DSample(NM,NMSampler,ua).rg*2-1,ny=Texture2DSample(NM,NMSampler,ub).rg*2-1,nz=Texture2DSample(NM,NMSampler,uc).rg*2-1;
float3 dn=float3(0,nx.x,nx.y)*w.x+float3(ny.x,0,ny.y)*w.y+float3(nz.x,nz.y,0)*w.z;
float lum=dot(c,float3(.3,.59,.11));lum=lerp(.42,lum,.55);
float3 vc=VC.rgb;   // (the build writes linear colours)
float fur=1.-saturate(VA*1.2);
float3 base=vc*lerp(1.,saturate(lum/.42)*1.25,fur)*lerp(float3(1,1,1),Tint.rgb,fur);
base=lerp(base,dot(base,float3(.3,.59,.11)),.18*fur)*.9;   // (a wild hare's coat: less saturated than the painted tawny reads in low sun)
Rough=lerp(.18,saturate(.55+r*.4),fur);
AO=lerp(1.,o,fur*.8);
WN=normalize(n+dn*NStr*fur);
return base;
"""


def build_material():
    path = f'{DEST}/M_FeyHareFur'
    mat = unreal.load_asset(path)
    if mat:
        # (clearing a loaded material's graph crashes UE 5.7 with a !IsRooted assertion: to rebuild it, delete
        # Content/Art/Feywild/Creatures/Hare/M_FeyHareFur.uasset before starting the editor)
        unreal.log('CHUCK_FEY_HARE_MATERIAL_KEPT (delete its .uasset first to rebuild it)')
        return mat
    mat = TOOLS.create_asset('M_FeyHareFur', DEST, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('tangent_space_normal', False)
    mat.set_editor_property('used_with_skeletal_mesh', True)
    def node(cls, **props):
        e = LIB.create_material_expression(mat, cls)
        for k, v in props.items(): e.set_editor_property(k, v)
        return e
    def tex(name, kind):
        t = unreal.load_asset(f'{FUR}/{name}')
        if not t: raise RuntimeError(f'{name} missing: run Tools/import_fey_creatures.py first')
        st = {'normal': unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, 'color': unreal.MaterialSamplerType.SAMPLERTYPE_COLOR}.get(kind, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        return node(unreal.MaterialExpressionTextureObject, texture=t, sampler_type=st)
    vc = node(unreal.MaterialExpressionVertexColor)
    # (the pre-skinned position and normal are vertex-shader values: passed to the pixels through interpolators)
    def interp(e):
        v = node(unreal.MaterialExpressionVertexInterpolator); LIB.connect_material_expressions(e, '', v, ''); return v
    inputs = {'P': interp(node(unreal.MaterialExpressionPreSkinnedPosition)), 'LN': interp(node(unreal.MaterialExpressionPreSkinnedNormal)), 'VC': vc, 'VA': (vc, 'A'),
              'C': tex('T_FeyFur_C', 'color'), 'R': tex('T_FeyFur_R', 'mask'), 'O': tex('T_FeyFur_AO', 'mask'), 'NM': tex('T_FeyFur_N', 'normal'),
              'Scale': node(unreal.MaterialExpressionScalarParameter, parameter_name='Scale', default_value=38.),
              'NStr': node(unreal.MaterialExpressionScalarParameter, parameter_name='NormalStrength', default_value=.5),
              'Tint': node(unreal.MaterialExpressionVectorParameter, parameter_name='Tint', default_value=unreal.LinearColor(1, 1, 1, 1))}
    args = []
    for name in inputs:
        i = unreal.CustomInput(); i.set_editor_property('input_name', name); args.append(i)
    outs = []
    for name, typ in (('Rough', F1), ('AO', F1), ('WN', F3)):
        o = unreal.CustomOutput(); o.set_editor_property('output_name', name); o.set_editor_property('output_type', typ); outs.append(o)
    c = node(unreal.MaterialExpressionCustom, code=CODE, inputs=args, output_type=F3, additional_outputs=outs)
    for name, expr in inputs.items():
        ok = LIB.connect_material_expressions(expr[0], expr[1], c, name) if isinstance(expr, tuple) else LIB.connect_material_expressions(expr, '', c, name)
        if not ok: raise RuntimeError(f'M_FeyHareFur: link {name}')
    # the fur's normal (in the rest space) to world: through the skinned tangent basis is not available, so local -> world
    wn = node(unreal.MaterialExpressionTransform, transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL, transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    LIB.connect_material_expressions(c, 'WN', wn, '')
    # (the detail normal is a perturbation of the rest normal; blended back toward the skinned vertex normal so posed
    # limbs keep their own shading)
    vn = node(unreal.MaterialExpressionVertexNormalWS)
    d = node(unreal.MaterialExpressionSubtract)
    LIB.connect_material_expressions(wn, '', d, 'A')
    tn = node(unreal.MaterialExpressionTransform, transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL, transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    LIB.connect_material_expressions(inputs['LN'], '', tn, '')
    LIB.connect_material_expressions(tn, '', d, 'B')
    add = node(unreal.MaterialExpressionAdd)
    LIB.connect_material_expressions(vn, '', add, 'A'); LIB.connect_material_expressions(d, '', add, 'B')
    nrm = node(unreal.MaterialExpressionNormalize)
    LIB.connect_material_expressions(add, '', nrm, '')
    Pm = unreal.MaterialProperty
    for expr, pin, prop in ((c, '', Pm.MP_BASE_COLOR), (c, 'Rough', Pm.MP_ROUGHNESS), (c, 'AO', Pm.MP_AMBIENT_OCCLUSION), (nrm, '', Pm.MP_NORMAL)):
        if not LIB.connect_material_property(expr, pin, prop): raise RuntimeError(f'M_FeyHareFur: output {prop}')
    LIB.connect_material_property(node(unreal.MaterialExpressionConstant, r=.3), '', Pm.MP_SPECULAR)
    LIB.recompile_material(mat)
    ASSETS.save_loaded_asset(mat, only_if_is_dirty=False)
    return mat


mat = build_material()
o = unreal.FbxImportUI()
o.import_mesh = True; o.import_as_skeletal = True
o.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
o.automated_import_should_detect_type = False
o.import_materials = o.import_textures = o.import_animations = False
o.create_physics_asset = False
skel_path = f'{DEST}/SKEL_FeyHare'
existing = unreal.load_asset(skel_path)
if existing: o.skeleton = existing
d = o.skeletal_mesh_import_data
d.set_editor_property('update_skeleton_reference_pose', True)
d.set_editor_property('vertex_color_import_option', unreal.VertexColorImportOption.REPLACE)
d.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
t = unreal.AssetImportTask()
t.filename = str(SOURCE / 'SK_FeyHare.fbx'); t.destination_path = DEST; t.destination_name = 'SK_FeyHare'
t.automated = True; t.replace_existing = True; t.save = False; t.options = o
TOOLS.import_asset_tasks([t])
mesh = unreal.load_asset(f'{DEST}/SK_FeyHare')
if not isinstance(mesh, unreal.SkeletalMesh): raise RuntimeError('SK_FeyHare: not a skeletal mesh')
if not existing:
    made = mesh.get_editor_property('skeleton')
    if not ASSETS.rename_asset(made.get_path_name().split('.')[0], skel_path): raise RuntimeError('SK_FeyHare: skeleton rename')
    mesh = unreal.load_asset(f'{DEST}/SK_FeyHare')
mats = mesh.get_editor_property('materials')
for i, m in enumerate(mats):
    m.material_interface = mat; mats[i] = m
mesh.set_editor_property('materials', mats)
# The leap and the rear take it beyond its rest bounds.
mesh.set_editor_property('positive_bounds_extension', unreal.Vector(60, 40, 60))
mesh.set_editor_property('negative_bounds_extension', unreal.Vector(60, 40, 20))
lod_ok = False
try:
    lod_ok = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem).regenerate_lod(mesh, 3, True, False)
except Exception as e:
    unreal.log_warning(f'CHUCK_FEY_HARE_LOD_FAILED {e}')
ASSETS.save_loaded_asset(mesh, only_if_is_dirty=False)
ASSETS.save_loaded_asset(mesh.get_editor_property('skeleton'), only_if_is_dirty=False)
box = mesh.get_bounds().box_extent
unreal.log(f'CHUCK_FEY_HARE_IMPORTED extent=({box.x:.0f},{box.y:.0f},{box.z:.0f}) lod_ok={lod_ok}')
unreal.log('CHUCK_FEY_HARE_READY')
