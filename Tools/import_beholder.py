"""Import the beholder (Tools/build_beholder.py) into /Game/Characters/Beholder (Claude, 2026-10-23).

Tools/Import-Beholder.ps1, or:
UnrealEditor-Cmd <uproject> -ExecutePythonScript=Tools/import_beholder.py -unattended -nosplash -NoLiveCoding

SK_Beholder with its own skeleton and no clips (ATempleBeholder poses it), LODs, the baked textures
and four materials on its slots:
- M_BeholderSkin: the baked colour (a slightly cooled Tint parameter: the torches are warm), darkened a
  little in its creases by the baked occlusion (more turned every pebble's pit into a dark speck); the baked normal (green flipped: Blender bakes OpenGL normals);
  roughness from the ORM map. (A subsurface model turned it flat pink in the torchlight, 2026-10-23.)
- M_BeholderEye: the iris texture (green, a slit pupil), glossy; the iris (not the white) glows faintly
  by the "Glow" parameter (ATempleBeholder: a little, more in a snarl).
- M_BeholderCornea: a clear, wet shell over the iris (translucent, lit per pixel for its highlights).
- M_BeholderTeeth: the vertex colour (stained roots, ivory tips), satin.
Materials are rebuilt on every run (they are generated, not hand-edited).
"""
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'SourceAssets/Enemies/Beholder'
DEST = '/Game/Characters/Beholder'
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
LIB = unreal.MaterialEditingLibrary
ASSETS = unreal.EditorAssetLibrary
MP = unreal.MaterialProperty


def node(mat, cls, x=0, y=0, **props):
    n = LIB.create_material_expression(mat, cls, x, y)
    for k, v in props.items(): n.set_editor_property(k, v)
    return n


def link(a, b, pin_b, pin_a=''):
    if not LIB.connect_material_expressions(a, pin_a, b, pin_b): raise RuntimeError(f'link {pin_a}->{pin_b}')


def out(e, prop, pin=''):
    if not LIB.connect_material_property(e, pin, prop): raise RuntimeError(f'output {prop}')


def const(mat, v, x=-600, y=0):
    return node(mat, unreal.MaterialExpressionConstant, x, y, r=v)


def texture(name, normal=False, srgb=True):
    t = unreal.AssetImportTask()
    t.filename = str(SOURCE / f'{name}.png'); t.destination_path = DEST; t.destination_name = name
    t.automated = True; t.replace_existing = True; t.save = False
    TOOLS.import_asset_tasks([t])
    tex = unreal.load_asset(f'{DEST}/{name}')
    if not isinstance(tex, unreal.Texture2D): raise RuntimeError(f'Texture import failed: {name}')
    if normal:
        tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property('flip_green_channel', True)          # Blender bakes OpenGL (+Y) normals
    elif not srgb: tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_MASKS)
    tex.set_editor_property('srgb', srgb and not normal)
    ASSETS.save_loaded_asset(tex, only_if_is_dirty=False)
    return tex


tex_d = texture('T_Beholder_D')
tex_n = texture('T_Beholder_N', normal=True)
tex_orm = texture('T_Beholder_ORM', srgb=False)
tex_eye = texture('T_BeholderEye')


def material(name, build):
    path = f'{DEST}/{name}'
    if ASSETS.does_asset_exist(path): ASSETS.delete_asset(path)
    mat = TOOLS.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('used_with_skeletal_mesh', True)
    build(mat)
    LIB.recompile_material(mat)
    ASSETS.save_loaded_asset(mat, only_if_is_dirty=False)
    return mat


def skin(mat):
    d0 = node(mat, unreal.MaterialExpressionTextureSample, -1300, -200, texture=tex_d)
    # cooler in the game's warm torchlight, which turned the mauve pink (a parameter, to tune per instance)
    tint = node(mat, unreal.MaterialExpressionVectorParameter, -1300, -380, parameter_name='Tint', default_value=unreal.LinearColor(.92, .84, .92, 1))
    d = node(mat, unreal.MaterialExpressionMultiply, -1100, -200); link(d0, d, 'A'); link(tint, d, 'B')
    n = node(mat, unreal.MaterialExpressionTextureSample, -1100, 150, texture=tex_n, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    orm = node(mat, unreal.MaterialExpressionTextureSample, -1100, 500, texture=tex_orm, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    # the creases darker in the colour too (Lumen's occlusion is broad; the baked one is the crease scale)
    ao = node(mat, unreal.MaterialExpressionPower, -800, 0); link(orm, ao, 'Base', 'R'); link(const(mat, 1., -950, 60), ao, 'Exp')
    lerp = node(mat, unreal.MaterialExpressionLinearInterpolate, -650, 0)
    link(const(mat, 1., -800, 100), lerp, 'A'); link(ao, lerp, 'B'); link(const(mat, .45, -800, 160), lerp, 'Alpha')
    col = node(mat, unreal.MaterialExpressionMultiply, -450, -150); link(d, col, 'A'); link(lerp, col, 'B')
    out(col, MP.MP_BASE_COLOR)
    out(n, MP.MP_NORMAL)
    out(orm, MP.MP_ROUGHNESS, 'G')
    out(orm, MP.MP_AMBIENT_OCCLUSION, 'R')
    out(const(mat, .45, -400, 400), MP.MP_SPECULAR)


def eye(mat):
    t = node(mat, unreal.MaterialExpressionTextureSample, -1100, -100, texture=tex_eye)
    out(t, MP.MP_BASE_COLOR)
    out(const(mat, .12, -400, 200), MP.MP_ROUGHNESS)
    out(const(mat, .7, -400, 260), MP.MP_SPECULAR)
    # the iris from the polar UVs: out to 0.229 (Tools/build_beholder.py: IRIS 32 of 70 degrees); the slit pupil is black in the texture
    uv = node(mat, unreal.MaterialExpressionTextureCoordinate, -1500, 400)
    c = node(mat, unreal.MaterialExpressionConstant2Vector, -1500, 500, r=.5, g=.5)
    sub = node(mat, unreal.MaterialExpressionSubtract, -1300, 450); link(uv, sub, 'A'); link(c, sub, 'B')
    ln = node(mat, unreal.MaterialExpressionLength, -1150, 450); link(sub, ln, '')
    outer = node(mat, unreal.MaterialExpressionSmoothStep, -950, 400, const_min=.212, const_max=.229); link(ln, outer, 'Value')
    ring = node(mat, unreal.MaterialExpressionOneMinus, -800, 400); link(outer, ring, '')
    glow = node(mat, unreal.MaterialExpressionScalarParameter, -650, 600, parameter_name='Glow', default_value=.3)
    k = node(mat, unreal.MaterialExpressionMultiply, -500, 500); link(ring, k, 'A'); link(glow, k, 'B')
    e = node(mat, unreal.MaterialExpressionMultiply, -350, 400); link(t, e, 'A'); link(k, e, 'B')
    e2 = node(mat, unreal.MaterialExpressionMultiply, -200, 400); link(e, e2, 'A'); link(const(mat, 1.1, -350, 520), e2, 'B')
    out(e2, MP.MP_EMISSIVE_COLOR)


def cornea(mat):
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property('translucency_lighting_mode', unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    out(node(mat, unreal.MaterialExpressionConstant3Vector, -400, 0, constant=unreal.LinearColor(0., 0., 0., 1)), MP.MP_BASE_COLOR)   # only its highlights
    out(const(mat, .03, -400, 120), MP.MP_ROUGHNESS)
    out(const(mat, 1., -400, 180), MP.MP_SPECULAR)
    fr = node(mat, unreal.MaterialExpressionFresnel, -700, 300, exponent=4.)
    op = node(mat, unreal.MaterialExpressionLinearInterpolate, -450, 300)
    link(const(mat, .06, -650, 260), op, 'A'); link(const(mat, .5, -650, 340), op, 'B'); link(fr, op, 'Alpha')
    out(op, MP.MP_OPACITY)


def teeth(mat):
    vc = node(mat, unreal.MaterialExpressionVertexColor, -600, 0)
    stain = node(mat, unreal.MaterialExpressionMultiply, -400, 0); link(vc, stain, 'A')
    link(node(mat, unreal.MaterialExpressionConstant3Vector, -600, 120, constant=unreal.LinearColor(.62, .55, .42, 1)), stain, 'B')   # old ivory, not white
    out(stain, MP.MP_BASE_COLOR)
    out(const(mat, .42, -400, 200), MP.MP_ROUGHNESS)
    out(const(mat, .5, -400, 260), MP.MP_SPECULAR)


materials = {
    'Skin': material('M_BeholderSkin', skin),
    'Eye': material('M_BeholderEye', eye),
    'Cornea': material('M_BeholderCornea', cornea),
    'Teeth': material('M_BeholderTeeth', teeth),
}

options = unreal.FbxImportUI()
options.import_mesh = True
options.import_as_skeletal = True
options.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
options.automated_import_should_detect_type = False
options.import_materials = options.import_textures = options.import_animations = False
options.create_physics_asset = False
options.skeletal_mesh_import_data.set_editor_property('update_skeleton_reference_pose', True)
options.skeletal_mesh_import_data.set_editor_property('vertex_color_import_option', unreal.VertexColorImportOption.REPLACE)
task = unreal.AssetImportTask()
task.filename = str(SOURCE / 'SK_Beholder.fbx'); task.destination_path = DEST; task.destination_name = 'SK_Beholder'
task.automated = True; task.replace_existing = True; task.save = True; task.options = options
TOOLS.import_asset_tasks([task])
mesh = unreal.load_asset(f'{DEST}/SK_Beholder')
if not isinstance(mesh, unreal.SkeletalMesh): raise RuntimeError('Beholder mesh is not skeletal')
mats = mesh.get_editor_property('materials')
for index, m in enumerate(mats):
    slot = str(m.get_editor_property('material_slot_name')).split('.')[0]
    if slot not in materials: raise RuntimeError(f'Unexpected beholder slot {slot}')
    m.material_interface = materials[slot]
    mats[index] = m
mesh.set_editor_property('materials', mats)
sub = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
lod_ok = sub.regenerate_lod(mesh, 4, True, False)
ASSETS.save_loaded_asset(mesh, only_if_is_dirty=False)
ASSETS.save_loaded_asset(mesh.get_editor_property('skeleton'), only_if_is_dirty=False)
box = mesh.get_bounds().box_extent
lods = [sub.get_num_verts(mesh, i) for i in range(sub.get_lod_count(mesh))]
skel = mesh.get_editor_property('skeleton')
unreal.log(f'CHUCK_BEHOLDER_IMPORTED slots={[str(m.get_editor_property("material_slot_name")) for m in mesh.get_editor_property("materials")]} '
           f'extent=({box.x:.1f},{box.y:.1f},{box.z:.1f}) lod_verts={lods} lod_ok={lod_ok}')
unreal.log('CHUCK_BEHOLDER_IMPORT_READY')
