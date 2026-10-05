"""Import verified CC0 timber scans and update ONLY setting wood materials and derived crate."""
from pathlib import Path
import hashlib
import json
import unreal

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'SourceAssets/Surfaces/WeatheredTimber'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
LIB = unreal.MaterialEditingLibrary
textures = {}
for record in json.loads((SOURCE/'manifest.json').read_text(encoding='utf-8-sig'))['assets']:
    file = SOURCE / record['file']
    if hashlib.sha256(file.read_bytes()).hexdigest().upper() != record['sha256']:
        raise RuntimeError('Source hash mismatch: '+file.name)
    task = unreal.AssetImportTask()
    task.filename = str(file); task.destination_path = '/Game/Art/Textures/WeatheredTimber'
    task.destination_name = 'T_'+file.stem
    task.automated = True; task.replace_existing = True; task.save = True
    TOOLS.import_asset_tasks([task])
    tex = unreal.load_asset(task.destination_path+'/'+task.destination_name)
    role = record['map']
    tex.set_editor_property('srgb', role == 'diff')
    tex.set_editor_property('compression_settings', {'diff':unreal.TextureCompressionSettings.TC_DEFAULT,
        'arm':unreal.TextureCompressionSettings.TC_MASKS, 'nor_dx':unreal.TextureCompressionSettings.TC_NORMALMAP}[role])
    if tex.blueprint_get_size_x() != 2048 or tex.blueprint_get_size_y() != 2048:
        raise RuntimeError('Unexpected resolution: '+file.name)
    unreal.EditorAssetLibrary.save_loaded_asset(tex, only_if_is_dirty=False)
    textures[record['asset'], role] = tex

def node(mat, cls, **props):
    item = LIB.create_material_expression(mat, cls)
    for key, value in props.items(): item.set_editor_property(key, value)
    return item

def connect(a, b, pin, output=''):
    if not LIB.connect_material_expressions(a, output, b, pin): raise RuntimeError('Cannot connect '+pin)

def custom(mat, code, sources, kind=unreal.CustomMaterialOutputType.CMOT_FLOAT3):
    ins = []
    for label in sources:
        item = unreal.CustomInput(); item.set_editor_property('input_name', label); ins.append(item)
    result = node(mat, unreal.MaterialExpressionCustom, code=code, inputs=ins, output_type=kind)
    for label, source in sources.items(): connect(source, result, label)
    return result

def interpolated(mat, source):
    result = node(mat, unreal.MaterialExpressionVertexInterpolator); connect(source, result, ''); return result

SAMPLE = r'''
float3 S=max(float3(length(AX),length(AY),length(AZ)),.0001);
float3 Bx=AX/S.x, By=AY/S.y, Bz=AZ/S.z;
float3 Q=L*S, E=Bounds*S;
float3 n=normalize(N), localN=float3(dot(n,Bx),dot(n,By),dot(n,Bz));
float3 weights=pow(abs(localN),8); weights/=max(dot(weights,float3(1,1,1)),.0001);
float3 result=0;
for(int face=0;face<3;face++) {
    int a=(face+1)%3, b=(face+2)%3;
    int v=E[a]>E[b]?a:b, u=v==a?b:a;
    // Siding boards run vertically on walls; their scan's grain runs along U.
    if(SIDING && face!=2) {v=2;u=face==0?1:0;}
    float2 uv=float2(Q[u],Q[v])/TILE;
    if(SIDING) uv=uv.yx;
    float3 sampled=Texture2DSample(Tex,TexSampler,uv).rgb;
    if(NORMAL) {
        float3 basis[3]={Bx,By,Bz};
        float2 d=sampled.rg*2-1;
        float3 perturb=SIDING ? d.x*basis[v]+d.y*basis[u] : d.x*basis[u]+d.y*basis[v];
        perturb-=n*dot(n,perturb);
        result+=perturb*weights[face];
    } else result+=sampled*weights[face];
}
return NORMAL ? normalize(n+result*.45) : result;
'''

for name, asset, siding, uv_only, tint in [
    ('Wood','rough_wood',False,False,(.64,.56,.47)),
    ('WoodLight','rough_wood',False,False,(.80,.73,.62)),
    ('AgedDockTimber','weathered_brown_planks',True,False,(.75,.70,.60)),
    ('WeatheredCrate','rough_wood',False,True,(.69,.62,.53)),
    ('WeatheredCrateLight','rough_wood',False,True,(.83,.77,.68))]:
    path = '/Game/Art/Materials/M_'+name
    mat = unreal.load_asset(path) or TOOLS.create_asset('M_'+name, '/Game/Art/Materials', unreal.Material, unreal.MaterialFactoryNew())
    # Idempotent targeted rebuild. Never call the broad character/art generator.
    LIB.delete_all_material_expressions(mat)
    mat.set_editor_property('used_with_instanced_static_meshes', True)
    mat.set_editor_property('tangent_space_normal', uv_only)
    pos = node(mat, unreal.MaterialExpressionWorldPosition)
    samples = {}
    if not uv_only:
        local = interpolated(mat, node(mat, unreal.MaterialExpressionPreSkinnedPosition))
        normal = interpolated(mat, node(mat, unreal.MaterialExpressionVertexNormalWS))
        bases = []
        for vector in ((1,0,0),(0,1,0),(0,0,1)):
            constant = node(mat, unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(*vector,1))
            transform = node(mat, unreal.MaterialExpressionTransform,
                transform_source_type=unreal.MaterialVectorCoordTransformSource.TRANSFORMSOURCE_LOCAL,
                transform_type=unreal.MaterialVectorCoordTransform.TRANSFORM_WORLD)
            connect(constant, transform, '')
            bases.append(interpolated(mat, transform))
        bounds = interpolated(mat, custom(mat, '''
        #if USE_INSTANCING || USE_INSTANCE_CULLING
            return GetPrimitiveData(Parameters).InstanceLocalBoundsExtent*2;
        #else
            return GetPrimitiveData(Parameters).LocalObjectBoundsMax-GetPrimitiveData(Parameters).LocalObjectBoundsMin;
        #endif
        ''', {'ForceVertex':node(mat, unreal.MaterialExpressionPreSkinnedPosition)}))
    else:
        uv = node(mat, unreal.MaterialExpressionTextureCoordinate)
    for role in ('diff','arm','nor_dx'):
        typ = {'diff':unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,'arm':unreal.MaterialSamplerType.SAMPLERTYPE_MASKS,
            'nor_dx':unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL}[role]
        if uv_only:
            sample = node(mat, unreal.MaterialExpressionTextureSample, texture=textures[asset,role], sampler_type=typ)
            connect(uv, sample, '')
        else:
            tex = node(mat, unreal.MaterialExpressionTextureObject, texture=textures[asset,role], sampler_type=typ)
            code = SAMPLE.replace('SIDING','1' if siding else '0').replace('TILE','130.0' if siding else '90.0').replace('NORMAL','1' if role=='nor_dx' else '0')
            sample = custom(mat, code, {'Tex':tex,'L':local,'N':normal,'Bounds':bounds,'AX':bases[0],'AY':bases[1],'AZ':bases[2]})
        samples[role] = sample
    color = custom(mat, '''
        float grainTint=lerp(.93,1.07,frac(sin(dot(floor(P.xy/23),float2(12.9898,78.233)))*43758.5453));
        float damp=(1-smoothstep(18,110,P.z))*smoothstep(-180,-100,P.z)*.16;
        return Scan*Tint*grainTint*(1-damp);
    ''', {'Scan':samples['diff'],'P':pos,'Tint':node(mat,unreal.MaterialExpressionConstant3Vector,constant=unreal.LinearColor(*tint,1))})
    LIB.connect_material_property(color,'',unreal.MaterialProperty.MP_BASE_COLOR)
    # Restrained specular: old dry grain stays matte, damp footings only slightly smoother.
    arm = samples['arm']
    for channel, prop in [('r',unreal.MaterialProperty.MP_AMBIENT_OCCLUSION),('g',unreal.MaterialProperty.MP_ROUGHNESS)]:
        mask = node(mat,unreal.MaterialExpressionComponentMask,r=channel=='r',g=channel=='g',b=False,a=False)
        connect(arm,mask,'')
        if channel=='g': mask=custom(mat,'return clamp(R*.92+.07,.65,.99);',{'R':mask},unreal.CustomMaterialOutputType.CMOT_FLOAT1)
        LIB.connect_material_property(mask,'',prop)
    spec = node(mat,unreal.MaterialExpressionConstant,r=.24)
    LIB.connect_material_property(spec,'',unreal.MaterialProperty.MP_SPECULAR)
    LIB.connect_material_property(samples['nor_dx'],'RGB' if uv_only else '',unreal.MaterialProperty.MP_NORMAL)
    LIB.recompile_material(mat)
    if not unreal.EditorAssetLibrary.save_loaded_asset(mat,only_if_is_dirty=False): raise RuntimeError(path)
    unreal.log('CHUCK_WEATHERED_MATERIAL '+path)

task = unreal.AssetImportTask()
task.filename = str(SOURCE/'SM_WeatheredDockCrate.fbx')
task.destination_path = '/Game/Art/Props'; task.destination_name = 'SM_WeatheredDockCrate'
task.automated = True; task.replace_existing = True; task.save = True
options = unreal.FbxImportUI()
options.import_mesh=True; options.import_as_skeletal=False
options.mesh_type_to_import=unreal.FBXImportType.FBXIT_STATIC_MESH
options.automated_import_should_detect_type=False
options.import_materials=False; options.import_textures=False
options.static_mesh_import_data.combine_meshes=True
options.static_mesh_import_data.auto_generate_collision=False
options.static_mesh_import_data.normal_import_method=unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS
task.options=options
TOOLS.import_asset_tasks([task])
mesh=unreal.load_asset('/Game/Art/Props/SM_WeatheredDockCrate')
for i,slot in enumerate(mesh.static_materials):
    label=str(slot.material_slot_name).split('.')[0]
    material=unreal.load_asset('/Game/Art/Materials/M_'+label)
    if not material: raise RuntimeError('Missing slot '+label)
    mesh.set_material(i,material)
old=unreal.load_asset('/Game/Art/Props/SM_DockCrate')
old_bounds=old.get_bounds(); new_bounds=mesh.get_bounds()
if (new_bounds.box_extent-old_bounds.box_extent).length()>.01 or (new_bounds.origin-old_bounds.origin).length()>.01:
    raise RuntimeError('Derived crate changed its bounds')
unreal.EditorAssetLibrary.save_loaded_asset(mesh,only_if_is_dirty=False)
unreal.log('CHUCK_WEATHERED_TIMBER_READY')
