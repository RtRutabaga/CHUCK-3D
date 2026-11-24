"""Import the maze's marble satyr (Claude, 2026-11-23; Tools/Fetch-FeySatyr.ps1, Tools/build_fey_satyr.py) into
/Game/Art/Feywild/Satyr.

UnrealEditor-Cmd <uproject> "-ExecutePythonScript=<absolute path to this file>" -unattended -nosplash -NoLiveCoding

  SM_FeySatyr   SMK's public-domain scan of the Medici Faun (KAS460), laughing, 70,000 triangles, 141.5 cm, no collision
                (FeyMap gives each a box), with generated LODs.
  SM_FeySatyrFrown  the same faun with his face re-carved to a scowl (Tools/build_fey_satyr.py), for the dead ends.
  M_FeyMarble   Poly Haven marble_01 (CC0) projected from three sides in the statue's own space (the scan has no UVs), a
                little weathered: grey in the hollows that face down, a faint green on the tops of the lower parts.
Only these assets are created or replaced.
"""
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'SourceAssets/Props/Feywild/Satyr'
DEST = '/Game/Art/Feywild/Satyr'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
ASSETS = unreal.EditorAssetLibrary
LIB = unreal.MaterialEditingLibrary
F1, F3 = unreal.CustomMaterialOutputType.CMOT_FLOAT1, unreal.CustomMaterialOutputType.CMOT_FLOAT3
ASSETS.make_directory(DEST)


def texture(name, file, kind):
    t = unreal.AssetImportTask()
    t.filename = str(SOURCE / file); t.destination_path = DEST; t.destination_name = name
    t.automated = True; t.replace_existing = True; t.save = False
    TOOLS.import_asset_tasks([t])
    tx = unreal.load_asset(f'{DEST}/{name}')
    if kind == 'normal':
        tx.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP); tx.set_editor_property('srgb', False)
    elif kind == 'mask':
        tx.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_MASKS); tx.set_editor_property('srgb', False)
    ASSETS.save_loaded_asset(tx, only_if_is_dirty=False)
    return tx


C = texture('T_Marble01_Diff', 'marble_01_diff_2k.jpg', 'color')
N = texture('T_Marble01_NorDX', 'marble_01_nor_dx_2k.jpg', 'normal')
R = texture('T_Marble01_Rough', 'marble_01_rough_2k.jpg', 'mask')

MARBLE = r"""
float3 n=normalize(LN);float3 w=pow(abs(n),4.);w/=dot(w,1);
float2 ua=P.yz/Scale,ub=P.xz/Scale,uc=P.xy/Scale;
float3 c=Texture2DSample(C,CSampler,ua).rgb*w.x+Texture2DSample(C,CSampler,ub).rgb*w.y+Texture2DSample(C,CSampler,uc).rgb*w.z;
float r=Texture2DSample(R,RSampler,ua).r*w.x+Texture2DSample(R,RSampler,ub).r*w.y+Texture2DSample(R,RSampler,uc).r*w.z;
float2 nx=Texture2DSample(NM,NMSampler,ua).rg*2-1,ny=Texture2DSample(NM,NMSampler,ub).rg*2-1,nz=Texture2DSample(NM,NMSampler,uc).rg*2-1;
float3 dn=float3(0,nx.x,nx.y)*w.x+float3(ny.x,0,ny.y)*w.y+float3(nz.x,nz.y,0)*w.z;
float low=saturate(1.-P.z/90.);
float grime=saturate(-n.z)*.35+low*.12;
float moss=saturate(n.z-.55)*low*.8;
float3 base=c*Tint;
base=lerp(base,base*float3(.62,.62,.6),grime);
base=lerp(base,float3(.16,.22,.08),moss);
Rough=lerp(saturate(r*.7+.08),.85,max(grime,moss));
WN=normalize(n+dn*.6);
return base;
"""


def build_marble():
    path = f'{DEST}/M_FeyMarble'
    mat = unreal.load_asset(path)
    if mat: LIB.delete_all_material_expressions(mat)
    else: mat = TOOLS.create_asset('M_FeyMarble', DEST, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('tangent_space_normal', False)
    def node(cls, **props):
        e = LIB.create_material_expression(mat, cls)
        for k, v in props.items(): e.set_editor_property(k, v)
        return e
    def tob(t, kind): return node(unreal.MaterialExpressionTextureObject, texture=t, sampler_type={'color': unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, 'normal': unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, 'mask': unreal.MaterialSamplerType.SAMPLERTYPE_MASKS}[kind])
    ln = node(unreal.MaterialExpressionTransform, transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_WORLD, transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_LOCAL)
    LIB.connect_material_expressions(node(unreal.MaterialExpressionVertexNormalWS), '', ln, '')
    inputs = {'P': node(unreal.MaterialExpressionLocalPosition), 'LN': ln, 'C': tob(C, 'color'), 'R': tob(R, 'mask'), 'NM': tob(N, 'normal'),
              'Scale': node(unreal.MaterialExpressionScalarParameter, parameter_name='Scale', default_value=70.),
              'Tint': node(unreal.MaterialExpressionVectorParameter, parameter_name='Tint', default_value=unreal.LinearColor(1.05, 1.03, 1., 1))}
    args = []
    for name in inputs:
        i = unreal.CustomInput(); i.set_editor_property('input_name', name); args.append(i)
    outs = []
    for name, typ in (('Rough', F1), ('WN', F3)):
        o = unreal.CustomOutput(); o.set_editor_property('output_name', name); o.set_editor_property('output_type', typ); outs.append(o)
    c = node(unreal.MaterialExpressionCustom, code=MARBLE, inputs=args, output_type=F3, additional_outputs=outs)
    for name, expr in inputs.items():
        if not LIB.connect_material_expressions(expr, '', c, name): raise RuntimeError(f'M_FeyMarble: link {name}')
    wn = node(unreal.MaterialExpressionTransform, transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL, transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
    LIB.connect_material_expressions(c, 'WN', wn, '')
    P_ = unreal.MaterialProperty
    for expr, pin, prop in ((c, '', P_.MP_BASE_COLOR), (c, 'Rough', P_.MP_ROUGHNESS), (wn, '', P_.MP_NORMAL)):
        if not LIB.connect_material_property(expr, pin, prop): raise RuntimeError(f'M_FeyMarble: output {prop}')
    LIB.connect_material_property(node(unreal.MaterialExpressionConstant, r=.5), '', P_.MP_SPECULAR)
    LIB.recompile_material(mat)
    ASSETS.save_loaded_asset(mat, only_if_is_dirty=False)
    return mat


mat = build_marble()
for name in ('SM_FeySatyr', 'SM_FeySatyrFrown'):
    o = unreal.FbxImportUI()
    o.import_mesh = True; o.import_as_skeletal = False
    o.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    o.automated_import_should_detect_type = False
    o.import_materials = o.import_textures = o.import_animations = False
    d = o.static_mesh_import_data
    d.set_editor_property('combine_meshes', True)
    d.set_editor_property('auto_generate_collision', False)
    d.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    d.set_editor_property('generate_lightmap_u_vs', False)
    t = unreal.AssetImportTask()
    t.filename = str(SOURCE / f'{name}.fbx'); t.destination_path = DEST; t.destination_name = name
    t.automated = True; t.replace_existing = True; t.save = False; t.options = o
    TOOLS.import_asset_tasks([t])
    mesh = unreal.load_asset(f'{DEST}/{name}')
    if not isinstance(mesh, unreal.StaticMesh): raise RuntimeError(f'{name}: import failed')
    mats = mesh.get_editor_property('static_materials')
    for i, m in enumerate(mats):
        m.material_interface = mat; mats[i] = m
    mesh.set_editor_property('static_materials', mats)
    lods = 0
    try:
        opts = unreal.EditorScriptingMeshReductionOptions()
        opts.set_editor_property('reduction_settings', [unreal.EditorScriptingMeshReductionSettings(1.0, 1.0), unreal.EditorScriptingMeshReductionSettings(.35, .4),
                                                        unreal.EditorScriptingMeshReductionSettings(.1, .15), unreal.EditorScriptingMeshReductionSettings(.03, .05)])
        lods = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem).set_lods(mesh, opts)
    except Exception as e:
        unreal.log_warning(f'CHUCK_FEY_SATYR_LOD_FAILED {name} {e}')
    ASSETS.save_loaded_asset(mesh, only_if_is_dirty=False)
    box = mesh.get_bounds().box_extent
    unreal.log(f'CHUCK_FEY_SATYR_IMPORTED {name} extent=({box.x:.0f},{box.y:.0f},{box.z:.0f}) lods={lods}')
