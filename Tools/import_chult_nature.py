"""Import verified 2K CC0 setting scans/derived meshes. No character imports."""
from pathlib import Path
import hashlib
import json
import unreal

ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'SourceAssets/Setting/ChultNature'
DEST='/Game/Art/ChultNature'
TOOLS=unreal.AssetToolsHelpers.get_asset_tools()
ASSETS=unreal.EditorAssetLibrary
LIB=unreal.MaterialEditingLibrary
# Definitions only; this does not run the old jungle mesh/audio importer.
helper=Path(__file__).with_name('import_fall_cutscene.py')
scope={'__file__':str(helper),'__name__':'nature_helpers'}
exec(compile(helper.read_text(encoding='utf-8').split('# Music:')[0],str(helper),'exec'),scope)
node,const,colour,mul,lerp,sample,out,link=[scope[k] for k in ('node','const','colour','mul','lerp','sample','out','link')]
textures={}
for rec in json.loads((SOURCE/'manifest.json').read_text(encoding='utf-8-sig'))['assets']:
    if rec['role']=='blend': continue
    file=SOURCE/rec['file']
    if hashlib.sha256(file.read_bytes()).hexdigest().upper()!=rec['sha256']: raise RuntimeError(file.name+' hash')
    task=unreal.AssetImportTask(); task.filename=str(file); task.destination_path=DEST
    task.destination_name='T_'+file.stem; task.automated=True; task.replace_existing=True; task.save=False
    TOOLS.import_asset_tasks([task]); tex=unreal.load_asset(DEST+'/'+task.destination_name)
    role=rec['role'].lower()
    tex.set_editor_property('srgb',role.endswith('diff') or role=='diffuse')
    tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP if role.endswith('nor_dx') else
        unreal.TextureCompressionSettings.TC_DEFAULT if role.endswith('diff') or role=='diffuse' else unreal.TextureCompressionSettings.TC_MASKS)
    if tex.blueprint_get_size_x()!=2048 or tex.blueprint_get_size_y()!=2048: raise RuntimeError('Expected 2K '+file.name)
    ASSETS.save_loaded_asset(tex,only_if_is_dirty=False); textures[rec['asset'],rec['role']]=tex

def make(name,asset,prefix='',foliage=False):
    mat=unreal.load_asset(DEST+'/'+name) or TOOLS.create_asset(name,DEST,unreal.Material,unreal.MaterialFactoryNew())
    LIB.delete_all_material_expressions(mat)
    mat.set_editor_property('used_with_instanced_static_meshes',True)
    if foliage:
        mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_MASKED)
        mat.set_editor_property('two_sided',True)
        mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
        mat.set_editor_property('opacity_mask_clip_value',.4)
    diff=textures[asset,prefix+'diff' if prefix else 'Diffuse']
    base=mul(mat,sample(mat,diff),colour(mat,.68,.74,.55) if foliage else colour(mat,.73,.77,.67),'RGB')
    out(mat,base,unreal.MaterialProperty.MP_BASE_COLOR)
    out(mat,sample(mat,textures[asset,prefix+'nor_dx'],normal=True),unreal.MaterialProperty.MP_NORMAL,'RGB')
    arm=sample(mat,textures[asset,prefix+'arm']); arm.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    out(mat,arm,unreal.MaterialProperty.MP_AMBIENT_OCCLUSION,'R')
    out(mat,lerp(mat,const(mat,.4),const(mat,.9),arm,'G'),unreal.MaterialProperty.MP_ROUGHNESS)
    out(mat,const(mat,.28),unreal.MaterialProperty.MP_SPECULAR)
    if foliage:
        alpha=textures.get((asset,prefix+'alpha' if prefix else 'Alpha'))
        opacity=sample(mat,alpha) if alpha else sample(mat,diff)
        if alpha: opacity.set_editor_property('sampler_type',unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
        out(mat,opacity,unreal.MaterialProperty.MP_OPACITY_MASK,'R' if alpha else 'A')
        out(mat,mul(mat,base,colour(mat,.22,.30,.12)),unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
    LIB.recompile_material(mat); ASSETS.save_loaded_asset(mat,only_if_is_dirty=False)
    return mat

materials={
    'fern_02':make('M_ChultFern','fern_02',foliage=True),
    'rock_moss_set_01':make('M_ChultScanRock','rock_moss_set_01'),
    'island_tree_02':make('M_ChultIslandBark','island_tree_02'),
    'island_tree_02_leaves':make('M_ChultIslandLeaves','island_tree_02','leaves_',True),
    'island_tree_02_branches':make('M_ChultIslandBranches','island_tree_02','branches_',True),
}
for rec in json.loads((SOURCE/'Derived/manifest.json').read_text())['assets']:
    file=SOURCE/'Derived'/rec['file']
    if hashlib.sha256(file.read_bytes()).hexdigest().upper()!=rec['sha256']: raise RuntimeError(file.name+' hash')
    task=unreal.AssetImportTask(); task.filename=str(file); task.destination_path=DEST; task.destination_name=file.stem
    task.automated=True; task.replace_existing=True; task.save=False
    options=unreal.FbxImportUI(); options.import_mesh=True; options.import_as_skeletal=False
    options.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH; options.automated_import_should_detect_type=False
    options.import_materials=False; options.import_textures=False
    options.static_mesh_import_data.combine_meshes=True
    options.static_mesh_import_data.auto_generate_collision=False
    options.static_mesh_import_data.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS
    task.options=options; TOOLS.import_asset_tasks([task]); mesh=unreal.load_asset(DEST+'/'+file.stem)
    for i,slot in enumerate(mesh.static_materials):
        label=str(slot.material_slot_name).split('.')[0]
        if label not in materials: raise RuntimeError('Unexpected slot '+label)
        mesh.set_material(i,materials[label])
    if rec['source']=='rock_moss_set_01' or file.stem=='SM_ChultIslandTrunkCollision':
        mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    bounds=mesh.get_bounds().box_extent*2
    expected=unreal.Vector(*rec['dimensions_cm'])
    if (bounds-expected).length()>1.: raise RuntimeError('Wrong cm scale: '+file.stem+' '+str(bounds))
    ASSETS.save_loaded_asset(mesh,only_if_is_dirty=False)
    unreal.log('CHUCK_NATURE_MESH '+file.stem+' size_cm='+str(bounds)+' triangles='+str(rec['triangles']))
# Refresh shared bark/ground without importing or regenerating existing meshes.
wet=Path(__file__).with_name('create_chult_wet_materials.py')
exec(compile(wet.read_text(encoding='utf-8'),str(wet),'exec'),{'__file__':str(wet),'__name__':'__main__',
     'CHULT_MATERIAL_NAMES':{'M_FallBark','M_FallGround'}})
unreal.log('CHUCK_NATURE_IMPORTED meshes=12 textures='+str(len(textures)))
