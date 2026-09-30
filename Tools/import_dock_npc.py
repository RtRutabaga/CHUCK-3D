"""Import the dock worker NPC (Tools/build_dock_npc.py) into /Game/Characters/DockWorker.

UnrealEditor-Cmd <uproject> -ExecutePythonScript=Tools/import_dock_npc.py -unattended -nosplash -NoLiveCoding

A skinned mesh with its own small skeleton and no clips: the runtime poses
the bones procedurally (ADockNPC: breathing, weight shift, glances, watching
Chuck). One flat material per clothing/skin slot, as for the rat. Existing
materials are kept (delete to rebuild).
"""
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'SourceAssets/NPCs/DockWorker'
DEST = '/Game/Characters/DockWorker'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
LIB = unreal.MaterialEditingLibrary
ASSETS = unreal.EditorAssetLibrary


def node(mat, cls, **props):
    n = LIB.create_material_expression(mat, cls)
    for k, v in props.items(): n.set_editor_property(k, v)
    return n


# Per-region materials (the mesh's slots): fur, belly, pink skin, eyes, tail.
# Linear colours; matte fur, the skin and tail a little smoother, glossy eyes.
REGIONS = {
    'Skin': ((.36, .2, .15), .6, .35),
    'Shirt': ((.55, .5, .4), .9, .25),
    'Jerkin': ((.12, .07, .035), .55, .35),
    'Belt': ((.07, .04, .02), .45, .4),
    'Trousers': ((.035, .067, .085), .9, .25),
    'Boots': ((.05, .03, .02), .5, .35),
    'Cap': ((.16, .06, .04), .95, .2),
    'Stubble': ((.12, .08, .06), .85, .25),
    'Brow': ((.05, .035, .025), .85, .25),
    'Eye': ((.012, .01, .01), .15, .6),
}


def region_material(name, colour, rough, spec):
    mat = unreal.load_asset(DEST + '/M_Worker' + name)
    if mat:
        return mat
    mat = TOOLS.create_asset('M_Worker' + name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    try:
        LIB.connect_material_property(node(mat, unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(*colour, 1)), '', unreal.MaterialProperty.MP_BASE_COLOR)
        LIB.connect_material_property(node(mat, unreal.MaterialExpressionConstant, r=rough), '', unreal.MaterialProperty.MP_ROUGHNESS)
        LIB.connect_material_property(node(mat, unreal.MaterialExpressionConstant, r=spec), '', unreal.MaterialProperty.MP_SPECULAR)
        mat.set_editor_property('used_with_skeletal_mesh', True)
    except Exception:
        ASSETS.delete_loaded_asset(mat)
        raise
    LIB.recompile_material(mat)
    ASSETS.save_loaded_asset(mat, only_if_is_dirty=False)
    return mat

options = unreal.FbxImportUI()
options.import_mesh = True
options.import_as_skeletal = True
options.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
options.automated_import_should_detect_type = False
options.import_materials = options.import_textures = options.import_animations = False
options.create_physics_asset = False
options.skeletal_mesh_import_data.set_editor_property('vertex_color_import_option', unreal.VertexColorImportOption.REPLACE)
options.skeletal_mesh_import_data.set_editor_property('update_skeleton_reference_pose', True)
materials = {name: region_material(name, *spec) for name, spec in REGIONS.items()}
task = unreal.AssetImportTask()
task.filename = str(SOURCE / 'SK_DockWorker.fbx'); task.destination_path = DEST; task.destination_name = 'SK_DockWorker'
task.automated = True; task.replace_existing = True; task.save = True; task.options = options
TOOLS.import_asset_tasks([task])
mesh = unreal.load_asset(f'{DEST}/SK_DockWorker')
if not isinstance(mesh, unreal.SkeletalMesh):
    raise RuntimeError('Worker mesh is not skeletal')
mats = mesh.get_editor_property('materials')
for index, m in enumerate(mats):
    slot = str(m.get_editor_property('material_slot_name')).split('.')[0]
    if slot not in materials:
        raise RuntimeError(f'Unexpected worker slot {slot}')
    m.material_interface = materials[slot]
    mats[index] = m          # the list holds copies: write each slot back
mesh.set_editor_property('materials', mats)
ASSETS.save_loaded_asset(mesh, only_if_is_dirty=False)
mats = mesh.get_editor_property('materials')
if any(m.material_interface != materials[str(m.material_slot_name).split('.')[0]] for m in mats):
    raise RuntimeError('Worker material assignments did not persist')
skeleton = mesh.get_editor_property('skeleton')
ASSETS.save_loaded_asset(skeleton, only_if_is_dirty=False)
box = mesh.get_bounds().box_extent
print(f'CHUCK_WORKER_IMPORTED slots={[str(m.get_editor_property("material_slot_name")) for m in mats]} extent=({box.x:.1f},{box.y:.1f},{box.z:.1f})')
