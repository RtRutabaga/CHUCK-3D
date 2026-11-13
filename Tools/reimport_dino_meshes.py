"""Reimport only the raptor's and T-rex's skinned meshes (Claude, 2026-11-12).

UnrealEditor-Cmd <uproject> -ExecutePythonScript=Tools/reimport_dino_meshes.py -unattended -nosplash -NoLiveCoding

After Tools/build_raptor.py / build_trex.py changed only the meshes (2026-11-12: the heads' UVs, which smeared the
pattern down their faces). import_raptor.py / import_trex.py would also rebuild M_Raptor/M_TRex and their instances
(which build_dino_skin.py has since re-parented to M_DinoSkin) and reimport the clips; this keeps every slot's material
as it is and leaves the clips alone.
"""
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1]
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
ASSETS = unreal.EditorAssetLibrary

for name, src, dest in [('SK_Raptor', 'SourceAssets/Enemies/Raptor/SK_Raptor.fbx', '/Game/Characters/Raptor'),
                        ('SK_TRex', 'SourceAssets/Enemies/TRex/SK_TRex.fbx', '/Game/Characters/TRex')]:
    old = unreal.load_asset(f'{dest}/{name}')
    if not isinstance(old, unreal.SkeletalMesh):
        raise RuntimeError(f'{name} missing')
    keep = {str(m.get_editor_property('material_slot_name')): m.get_editor_property('material_interface') for m in old.get_editor_property('materials')}
    skeleton = old.get_editor_property('skeleton')
    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = True
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    options.automated_import_should_detect_type = False
    options.import_materials = options.import_textures = options.import_animations = False
    options.create_physics_asset = False
    options.skeleton = skeleton
    options.skeletal_mesh_import_data.set_editor_property('update_skeleton_reference_pose', False)
    task = unreal.AssetImportTask()
    task.filename = str(ROOT / src); task.destination_path = dest; task.destination_name = name
    task.automated = True; task.replace_existing = True; task.save = False; task.options = options
    TOOLS.import_asset_tasks([task])
    mesh = unreal.load_asset(f'{dest}/{name}')
    mats = mesh.get_editor_property('materials')
    restored = 0
    for i, m in enumerate(mats):
        slot = str(m.get_editor_property('material_slot_name'))
        if slot in keep and keep[slot]:
            m.set_editor_property('material_interface', keep[slot]); mats[i] = m; restored += 1
    mesh.set_editor_property('materials', mats)
    ASSETS.save_loaded_asset(mesh, only_if_is_dirty=False)
    unreal.log(f'CHUCK_DINO_MESH_REIMPORT {name} slots={len(mats)} restored={restored} skeleton={mesh.get_editor_property("skeleton") == skeleton}')
