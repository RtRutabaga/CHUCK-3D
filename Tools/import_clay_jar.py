"""Import the breakable clay jar and its shards (Tools/build_clay_jar.py) into /Game/Art/Props/Jar.

UnrealEditor-Cmd <uproject> -ExecutePythonScript=Tools/import_clay_jar.py -unattended -nosplash -NoLiveCoding
(the FBX importer needs the full editor; the -run=pythonscript commandlet asserts in Slate.)

M_ClayJar: vertex-colour tint squared (the FBX colours arrive a gamma step
bright, as for M_Grass); vertex alpha marks the glaze, which is glossier than
the bare fired clay. An existing material is kept (delete it to rebuild).
No collision on the meshes: the jar actor carries its own pawn-only blocker.

CHUCK_JAR_VARIANTS (default "jar") picks what to import, comma-separated:
jar -> /Game/Art/Props/Jar, tan -> /Game/Art/Props/TempleJar, urn ->
/Game/Art/Props/Urn (the Chult temple's, 2026-10-18), all on M_ClayJar.
"""
import json
import os
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1]
DEST = '/Game/Art/Props/Jar'
FOLDERS = {'jar': ('Jar', 'SM_ClayJar'), 'tan': ('TempleJar', 'SM_TempleJar'), 'urn': ('Urn', 'SM_ClayUrn')}
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
LIB = unreal.MaterialEditingLibrary
ASSETS = unreal.EditorAssetLibrary


def node(mat, cls, **props):
    n = LIB.create_material_expression(mat, cls)
    for k, v in props.items(): n.set_editor_property(k, v)
    return n


def link(a, b, pin_b, pin_a=''):
    if not LIB.connect_material_expressions(a, pin_a, b, pin_b):
        raise RuntimeError(f'Material link failed: {pin_a} -> {pin_b}')


def clay_material():
    mat = unreal.load_asset(f'{DEST}/M_ClayJar')
    if mat:
        print('CHUCK_JAR_MATERIAL_KEPT')
        return mat
    mat = TOOLS.create_asset('M_ClayJar', DEST, unreal.Material, unreal.MaterialFactoryNew())
    try:
        vc = node(mat, unreal.MaterialExpressionVertexColor)
        tint = node(mat, unreal.MaterialExpressionMultiply)
        link(vc, tint, 'A'); link(vc, tint, 'B')
        LIB.connect_material_property(tint, '', unreal.MaterialProperty.MP_BASE_COLOR)
        rough = node(mat, unreal.MaterialExpressionLinearInterpolate, const_a=.82, const_b=.32)
        link(vc, rough, 'Alpha', 'A')
        LIB.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
        spec = node(mat, unreal.MaterialExpressionLinearInterpolate, const_a=.3, const_b=.55)
        link(vc, spec, 'Alpha', 'A')
        LIB.connect_material_property(spec, '', unreal.MaterialProperty.MP_SPECULAR)
    except Exception:
        ASSETS.delete_loaded_asset(mat)
        raise
    LIB.recompile_material(mat)
    ASSETS.save_loaded_asset(mat, only_if_is_dirty=False)
    return mat


options = unreal.FbxImportUI()
options.import_mesh = True; options.import_as_skeletal = False
options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
options.automated_import_should_detect_type = False
options.import_materials = options.import_textures = options.import_animations = False
data = options.static_mesh_import_data
data.set_editor_property('combine_meshes', True)
data.set_editor_property('auto_generate_collision', False)
data.set_editor_property('vertex_color_import_option', unreal.VertexColorImportOption.REPLACE)
material = clay_material()
for variant in os.environ.get('CHUCK_JAR_VARIANTS', 'jar').split(','):
    folder, body = FOLDERS[variant.strip()]
    source, dest = ROOT / 'SourceAssets/Props' / folder, f'/Game/Art/Props/{folder}'
    shards = json.loads((source / 'shards.json').read_text(encoding='utf-8'))['shards']
    names = [body] + [s['name'] for s in shards]
    for name in names:
        task = unreal.AssetImportTask()
        task.filename = str(source / f'{name}.fbx'); task.destination_path = dest; task.destination_name = name
        task.automated = True; task.replace_existing = True; task.save = True; task.options = options
        TOOLS.import_asset_tasks([task])
        mesh = unreal.load_asset(f'{dest}/{name}')
        if not isinstance(mesh, unreal.StaticMesh):
            raise RuntimeError(f'Missing jar mesh {name}')
        for i in range(len(mesh.get_editor_property('static_materials'))):
            mesh.set_material(i, material)
        ASSETS.save_loaded_asset(mesh, only_if_is_dirty=False)
    box = unreal.load_asset(f'{dest}/{body}').get_bounding_box()
    print(f'CHUCK_JAR_IMPORTED variant={variant} meshes={len(names)} height_cm={box.max.z - box.min.z:.1f}')
