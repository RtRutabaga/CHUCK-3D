"""Import Claude's rest-pose groom beside the legacy playable character."""
from pathlib import Path
import json
import unreal

ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'SourceAssets/Chuck/V1'
DEST='/Game/Characters/Chuck/V1'
tools=unreal.AssetToolsHelpers.get_asset_tools()

def ingest(file,name,options,factory=None):
    task=unreal.AssetImportTask()
    task.filename=str(file)
    task.destination_path=DEST
    task.destination_name=name
    task.automated=True
    task.replace_existing=True
    task.save=True
    task.options=options
    if factory:
        task.factory=factory
    tools.import_asset_tasks([task])
    asset=unreal.load_asset(DEST+'/'+name)
    if not asset:
        raise RuntimeError('Import failed: '+name)
    return asset

options=unreal.FbxImportUI()
options.import_mesh=True
options.import_as_skeletal=True
options.mesh_type_to_import=unreal.FBXImportType.FBXIT_SKELETAL_MESH
options.automated_import_should_detect_type=False
options.import_materials=options.import_textures=options.import_animations=False
options.create_physics_asset=False
options.skeleton=unreal.load_asset(DEST+'/SK_Chuck_Skeleton')
options.skeletal_mesh_import_data.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS
options.skeletal_mesh_import_data.set_editor_property('update_skeleton_reference_pose',True)
mesh=ingest(SOURCE/'SK_Chuck_Groomed.fbx','SK_Chuck_Groomed',options)
slots=mesh.get_editor_property('materials')
for i,slot in enumerate(slots):
    slot.material_interface=unreal.load_asset(DEST+'/M_Chuck_V1')
    slots[i]=slot
mesh.set_editor_property('materials',slots)
unreal.EditorAssetLibrary.save_loaded_asset(mesh,only_if_is_dirty=False)
options=unreal.GroomImportOptions()
conversion=unreal.GroomConversionSettings()
conversion.rotation=unreal.Vector(90,0,0)
conversion.scale=unreal.Vector(1,-1,1)
options.conversion_settings=conversion
metadata=json.loads((SOURCE/'Groom/groom_metadata.json').read_text())
report=[]
lib=unreal.MaterialEditingLibrary
parent=unreal.load_asset(DEST+'/M_Fur')
if not parent:
    parent=tools.create_asset('M_Fur',DEST,unreal.Material,unreal.MaterialFactoryNew())
    parent.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_HAIR)
    parent.set_editor_property('two_sided',True)
    lib.set_material_usage(parent,unreal.MaterialUsage.MATUSAGE_HAIR_STRANDS)
    colour=lib.create_material_expression(parent,unreal.MaterialExpressionVectorParameter)
    colour.set_editor_property('parameter_name','HairColour')
    colour.set_editor_property('default_value',unreal.LinearColor(.23,.13,.07,1))
    attributes=lib.create_material_expression(parent,unreal.MaterialExpressionHairAttributes)
    vary=lib.create_material_expression(parent,unreal.MaterialExpressionLinearInterpolate)
    vary.set_editor_property('const_a',.85); vary.set_editor_property('const_b',1.15)
    assert lib.connect_material_expressions(attributes,'Seed',vary,'Alpha')
    tint=lib.create_material_expression(parent,unreal.MaterialExpressionMultiply)
    assert lib.connect_material_expressions(colour,'',tint,'A') and lib.connect_material_expressions(vary,'',tint,'B')
    lib.connect_material_property(tint,'',unreal.MaterialProperty.MP_BASE_COLOR)
    roughness=lib.create_material_expression(parent,unreal.MaterialExpressionConstant)
    roughness.set_editor_property('r',.65)
    lib.connect_material_property(roughness,'',unreal.MaterialProperty.MP_ROUGHNESS)
    lib.recompile_material(parent)
    unreal.EditorAssetLibrary.save_loaded_asset(parent,only_if_is_dirty=False)
for name,source_group in metadata['groups'].items():
    groom=ingest(ROOT/'Local/GroomGroups'/f'{name}.abc','GR_Chuck_'+name,options,unreal.HairStrandsFactory())
    groups=groom.get_editor_property('hair_groups_info')
    unreal.log('CHUCK_GROOM_COUNTS '+name+' '+str([(g.get_editor_property('num_curves'),g.get_editor_property('num_guides')) for g in groups]))
    assert len(groups)==1 and groups[0].get_editor_property('num_curves')==source_group['strands'],str(groups)
    # One parent hair material with per-strand colour variation (Hair Attributes
    # seed, +/-15%) and one instance per group carrying the group colour, so
    # re-imports only set a parameter (deleting a loaded material's expressions
    # from Python asserts !IsRooted in UE 5.7.4).
    mat=unreal.load_asset(DEST+'/M_'+name)
    if mat and not isinstance(mat,unreal.MaterialInstanceConstant):
        raise RuntimeError('M_'+name+' is an old plain material; delete it (and re-run) so it becomes an M_Fur instance')
    if not mat:
        mat=tools.create_asset('M_'+name,DEST,unreal.MaterialInstanceConstant,unreal.MaterialInstanceConstantFactoryNew())
    lib.set_material_instance_parent(mat,parent)
    lib.set_material_instance_vector_parameter_value(mat,'HairColour',unreal.LinearColor(*metadata['suggested_hair_colours_linear'][name],1))
    lib.update_material_instance(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat,only_if_is_dirty=False)
    assert unreal.ChuckReviewLibrary.configure_groom_material(groom,mat,name)
    unreal.EditorAssetLibrary.save_loaded_asset(groom,only_if_is_dirty=False)
    binding=unreal.load_asset(DEST+'/GB_Chuck_'+name)
    if binding:
        binding.set_editor_property('groom',groom)
        binding.set_editor_property('target_skeletal_mesh',mesh)
        assert unreal.ChuckReviewLibrary.rebuild_groom_binding(binding)
    else:
        binding=unreal.GroomLibrary.create_new_groom_binding_asset_with_path(DEST+'/GB_Chuck_'+name,groom,mesh,33)
        assert unreal.ChuckReviewLibrary.rebuild_groom_binding(binding)
    assert binding and binding.get_editor_property('target_skeletal_mesh')==mesh
    info=binding.get_editor_property('group_infos')
    assert len(info)==1 and info[0].get_editor_property('ren_root_count')==source_group['strands']
    unreal.EditorAssetLibrary.save_loaded_asset(binding,only_if_is_dirty=False)
    roots=unreal.ChuckReviewLibrary.sample_groom_roots(groom)
    assert len(roots)>=234
    report.append({'name':name,'strands':source_group['strands'],'binding':binding.get_path_name(),
        'sampled_roots_ue_cm':[[v.x,v.y,v.z] for v in roots]})
(ROOT/'Local/groom-import-report.json').write_text(json.dumps(report,indent=2))
unreal.log('CHUCK_GROOM_IMPORTED '+str(sum(g['strands'] for g in report)))
