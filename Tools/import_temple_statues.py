"""Import the Chult temple's skull, stake and serpent statues (Tools/build_temple_statues.py)
into /Game/Art/ChultTemple (Claude, 2026-10-18).

UnrealEditor-Cmd <uproject> -ExecutePythonScript=Tools/import_temple_statues.py -unattended -nosplash -NoLiveCoding

Slots by the Blender material names: Wood -> M_ChultIslandBark (a barked pole), Cord -> M_Leather,
Stone -> M_RuinStone, Moss -> M_RuinMoss, Plaster -> M_WeatheredPlaster, Bone ->
M_TempleSkull (made here from the aged skull maps; the CC0 skull's normal map is
OpenGL-style, so its green channel is flipped). The stake, serpent head and idol
collide as their own shape; the skull does not collide.
"""
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'SourceAssets/Props/TempleStatues'
DEST = '/Game/Art/ChultTemple'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
LIB = unreal.MaterialEditingLibrary
ASSETS = unreal.EditorAssetLibrary


def texture(name, normal):
    task = unreal.AssetImportTask(); task.filename = str(SOURCE / f'{name}.png'); task.destination_path = DEST
    task.destination_name = name; task.automated = True; task.replace_existing = True; task.save = True
    TOOLS.import_asset_tasks([task])
    tex = unreal.load_asset(f'{DEST}/{name}')
    if normal:
        tex.set_editor_property('srgb', False); tex.set_editor_property('flip_green_channel', True)
        tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
    ASSETS.save_loaded_asset(tex, only_if_is_dirty=False)
    return tex


def skull_material(diffuse, normal):
    path = f'{DEST}/M_TempleSkull'
    if ASSETS.does_asset_exist(path): ASSETS.delete_asset(path)
    mat = TOOLS.create_asset('M_TempleSkull', DEST, unreal.Material, unreal.MaterialFactoryNew())
    d = LIB.create_material_expression(mat, unreal.MaterialExpressionTextureSample); d.set_editor_property('texture', diffuse)
    LIB.connect_material_property(d, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
    n = LIB.create_material_expression(mat, unreal.MaterialExpressionTextureSample); n.set_editor_property('texture', normal)
    n.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    LIB.connect_material_property(n, 'RGB', unreal.MaterialProperty.MP_NORMAL)
    r = LIB.create_material_expression(mat, unreal.MaterialExpressionConstant); r.set_editor_property('r', .72)
    LIB.connect_material_property(r, '', unreal.MaterialProperty.MP_ROUGHNESS)
    sp = LIB.create_material_expression(mat, unreal.MaterialExpressionConstant); sp.set_editor_property('r', .3)
    LIB.connect_material_property(sp, '', unreal.MaterialProperty.MP_SPECULAR)
    LIB.recompile_material(mat); ASSETS.save_loaded_asset(mat, only_if_is_dirty=False)
    return mat


skull = skull_material(texture('T_TempleSkull_D', False), texture('T_TempleSkull_N', True))
SLOTS = {'Wood': '/Game/Art/ChultNature/M_ChultIslandBark', 'Cord': '/Game/Art/Materials/M_Leather',
         'Stone': '/Game/Art/ChultRuins/M_RuinStone', 'Moss': '/Game/Art/ChultRuins/M_RuinMoss',
         'Plaster': '/Game/Art/Materials/M_WeatheredPlaster'}
options = unreal.FbxImportUI()
options.import_mesh = True; options.import_as_skeletal = False
options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
options.automated_import_should_detect_type = False
options.import_materials = options.import_textures = options.import_animations = False
data = options.static_mesh_import_data
data.set_editor_property('combine_meshes', True)
data.set_editor_property('auto_generate_collision', False)
data.set_editor_property('generate_lightmap_u_vs', False)
made = []
for name, solid in (('SM_TempleSkull', False), ('SM_SkullStake', True), ('SM_TempleSerpentHead', True), ('SM_TempleSerpentIdol', True)):
    task = unreal.AssetImportTask(); task.filename = str(SOURCE / f'{name}.fbx'); task.destination_path = DEST
    task.destination_name = name; task.automated = True; task.replace_existing = True; task.save = True; task.options = options
    TOOLS.import_asset_tasks([task])
    mesh = unreal.load_asset(f'{DEST}/{name}')
    if not isinstance(mesh, unreal.StaticMesh): raise RuntimeError(f'Missing statue mesh {name}')
    materials = mesh.get_editor_property('static_materials')
    for i, m in enumerate(materials):
        slot = str(m.get_editor_property('material_slot_name'))
        key = next((k for k in sorted(SLOTS, key=len, reverse=True) if slot.startswith(k)), None)
        mesh.set_material(i, skull if slot.startswith('Bone') else unreal.load_asset(SLOTS[key]) if key else None)
    if solid:
        mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag', unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    ASSETS.save_loaded_asset(mesh, only_if_is_dirty=False)
    box = mesh.get_bounding_box(); made.append(f'{name}:{len(materials)}slots:{box.max.z - box.min.z:.0f}cm')
print('CHUCK_STATUES_IMPORTED ' + ' '.join(made))
