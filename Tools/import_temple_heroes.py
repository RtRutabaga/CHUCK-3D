"""Import the temple heroes (Tools/build_temple_heroes.py, Tools/build_hero_anims.py) into /Game/Characters/Heroes.

UnrealEditor-Cmd <uproject> -ExecutePythonScript=Tools/import_temple_heroes.py -unattended -nosplash -NoLiveCoding
(Tools/Import-TempleHeroes.ps1)

Each hero has its own skeleton (SKEL_Hero<Name>: the human rig plus its prop bones), generated LODs
and its clips (AS_Hero<Name>_<Clip>, no root motion: the game moves them). Slots:
  * the human kit's types (skin, eye, card, hair, fabric) as Tools/import_npc_humans.py builds them,
    except the skin, which is M_HeroSkin here: subsurface (light through the ears and nose), a little
    glossier where it should be;
  * 'pbr' slots (the armour, props and gear): M_HeroMetal (ambientCG colour, roughness and metal maps:
    the plate, rims, rivets, the blade, the gilt), M_HeroCloth (Poly Haven colour/ARM/normal as weave:
    leather, velvet, wool, rope, wood) and M_HeroSolid (gem, bowstring, fletching). All three read the
    vertex colour Tools/build_temple_heroes.py baked: R ambient occlusion, G edge wear (metal brighter and
    polished at its edges, leather and wood paler), B grime (darker and duller in the hollows).
Everything under /Game/Characters/Heroes is generated.
"""
from pathlib import Path
import importlib.util
import json
import os
import unreal

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'SourceAssets/Heroes'
DEST = '/Game/Characters/Heroes'
MANIFEST = json.loads((SOURCE / 'manifest.json').read_text(encoding='utf-8'))
os.environ['CHUCK_HUMANS_ONLY'] = '__none__'          # load the human importer's functions without it importing anything
spec = importlib.util.spec_from_file_location('npc_import', ROOT / 'Tools/import_npc_humans.py')
kit = importlib.util.module_from_spec(spec); spec.loader.exec_module(kit)
kit.DEST, kit.SOURCE = DEST, SOURCE
TOOLS, LIB, ASSETS = kit.TOOLS, kit.LIB, kit.ASSETS
node, tex_param, texture = kit.node, kit.tex_param, kit.texture
MP = unreal.MaterialProperty
ONLY = {x.strip() for x in os.environ.get('CHUCK_HEROES_ONLY', '').split(',') if x.strip()}
# The kit's own fabrics (Poly Haven cloth, the generated mail) are already in /Game/Characters/Humans/Fabrics:
# reuse them there rather than import second copies; the heroes' own surfaces go in Heroes/Fabrics.
_kit_fabric_textures = kit.fabric_textures
def fabric_textures(name, source=kit.CLOTH):
    if Path(source).resolve() in (kit.CLOTH.resolve(), (ROOT / 'SourceAssets/Surfaces/Armor').resolve()):
        folder = '/Game/Characters/Humans/Fabrics'
        def keep(file, kind='color'):   # loaded as they are (not re-saved: the humans own them)
            return unreal.load_asset(f'{folder}/T_{Path(file).stem}') or texture(Path(source) / file, folder, kind)
        return (keep(f'{name}_diff_2k.jpg'), keep(f'{name}_arm_2k.jpg', 'masks'), keep(f'{name}_nor_dx_2k.jpg', 'normal'))
    return _kit_fabric_textures(name, source)
kit.fabric_textures = fabric_textures


def link(a, b, pin_b, pin_a=''):
    if not LIB.connect_material_expressions(a, pin_a, b, pin_b): raise RuntimeError(f'link {pin_a}->{pin_b}')


def out(e, prop, pin=''):
    if not LIB.connect_material_property(e, pin, prop): raise RuntimeError(f'output {prop}')


def op(mat, cls, a, b, x, y, pa='', pb=''):
    n = node(mat, cls, x, y)
    link(a, n, 'A', pa); link(b, n, 'B', pb)
    return n


def const(mat, v, x, y):
    return node(mat, unreal.MaterialExpressionConstant, x, y, r=v)


def vec(mat, name, v, x, y):
    return node(mat, unreal.MaterialExpressionVectorParameter, x, y, parameter_name=name, default_value=unreal.LinearColor(*v, 1))


def sca(mat, name, v, x, y):
    return node(mat, unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=v)


def lerp(mat, a, b, alpha, x, y, pa='', pb='', palpha=''):
    n = node(mat, unreal.MaterialExpressionLinearInterpolate, x, y)
    link(a, n, 'A', pa); link(b, n, 'B', pb); link(alpha, n, 'Alpha', palpha)
    return n


def wear_layers(mat, base, rough, vc, edge_bright, edge_polish, grime_col, grime_amt):
    """Edge wear and grime from the vertex colour on a base colour and a roughness."""
    M, A = unreal.MaterialExpressionMultiply, unreal.MaterialExpressionAdd
    edge = op(mat, M, vc, edge_bright, -900, 700, 'G', '')
    bright = op(mat, M, base, const(mat, 1.7, -900, 600), -750, 600)
    b1 = lerp(mat, base, bright, edge, -600, 500)
    grime = op(mat, M, vc, grime_amt, -900, 850, 'B', '')
    b2 = lerp(mat, b1, grime_col, grime, -450, 500)
    polish = op(mat, M, vc, edge_polish, -900, 950, 'G', '')
    r1 = lerp(mat, rough, op(mat, M, rough, const(mat, .45, -900, 1050), -750, 1050), polish, -600, 1000)
    r2 = lerp(mat, r1, const(mat, .9, -600, 1150), grime, -450, 1000)
    return b2, r2


def build_metal(mat, d, r, m, n):
    M = unreal.MaterialExpressionMultiply
    D = tex_param(mat, 'Diffuse', d, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -1300, -200)
    R = tex_param(mat, 'Rough', r, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, -1300, 100)
    Me = tex_param(mat, 'Metal', m, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, -1300, 350)
    N = tex_param(mat, 'Normal', n, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, -1300, 600)
    vc = node(mat, unreal.MaterialExpressionVertexColor, -1300, 900)
    base = op(mat, M, D, vec(mat, 'Tint', (1, 1, 1), -1100, -300), -1000, -200, 'RGB', '')
    rough = op(mat, M, R, sca(mat, 'RoughScale', 1., -1100, 150), -1000, 100, 'R', '')
    b, ro = wear_layers(mat, base, rough, vc, sca(mat, 'EdgeBright', .6, -1100, 700), sca(mat, 'EdgePolish', .8, -1100, 950),
                        vec(mat, 'GrimeColor', (.03, .026, .02), -1100, 1100), sca(mat, 'GrimeAmount', .55, -1100, 1200))
    out(b, MP.MP_BASE_COLOR); out(ro, MP.MP_ROUGHNESS)
    metal = op(mat, M, Me, sca(mat, 'MetalScale', 1., -1100, 400), -1000, 350, 'R', '')
    out(metal, MP.MP_METALLIC)
    out(N, MP.MP_NORMAL, 'RGB')
    # The heroes stand deep in a torch-lit hall, but the project's reflections fall back to the outdoor sky
    # (r.ReflectionMethod 2): the sky's share is occluded (material AO scales the sky light's specular too),
    # so the plate shows the torches, not a blue sky (2026-10-23 in-game: it read chrome-white).
    out(op(mat, unreal.MaterialExpressionMultiply, vc, sca(mat, 'SkyOcclusion', .16, -1100, 1300), -900, 1250, 'R', ''), MP.MP_AMBIENT_OCCLUSION)


def build_cloth(mat, d, arm, n):
    M = unreal.MaterialExpressionMultiply
    D = tex_param(mat, 'Diffuse', d, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -1300, -200)
    A = tex_param(mat, 'ARM', arm, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, -1300, 100)
    N = tex_param(mat, 'Normal', n, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, -1300, 400)
    vc = node(mat, unreal.MaterialExpressionVertexColor, -1300, 900)
    grey = node(mat, unreal.MaterialExpressionDotProduct, -1050, -250)
    link(D, grey, 'A', 'RGB'); link(node(mat, unreal.MaterialExpressionConstant3Vector, -1200, -350, constant=unreal.LinearColor(.2126, .7152, .0722, 1)), grey, 'B')
    colour = op(mat, M, vec(mat, 'Tint', (.5, .5, .5), -1200, -500), sca(mat, 'Gain', 1., -1200, -600), -1050, -500)
    base = op(mat, M, grey, colour, -900, -300)
    rough = op(mat, M, A, sca(mat, 'RoughScale', 1., -1100, 200), -1000, 150, 'G', '')
    b, ro = wear_layers(mat, base, rough, vc, sca(mat, 'EdgeBright', .35, -1100, 700), sca(mat, 'EdgePolish', .25, -1100, 950),
                        vec(mat, 'GrimeColor', (.02, .016, .012), -1100, 1100), sca(mat, 'GrimeAmount', .45, -1100, 1200))
    out(b, MP.MP_BASE_COLOR); out(ro, MP.MP_ROUGHNESS)
    out(op(mat, M, op(mat, M, A, vc, -900, 300, 'R', 'R'), sca(mat, 'SkyOcclusion', .45, -1100, 1300), -750, 300), MP.MP_AMBIENT_OCCLUSION)
    out(N, MP.MP_NORMAL, 'RGB')
    out(const(mat, .35, -300, 600), MP.MP_SPECULAR)


def build_solid(mat):
    vc = node(mat, unreal.MaterialExpressionVertexColor, -900, 400)
    out(vec(mat, 'Tint', (.5, .5, .5), -600, -100), MP.MP_BASE_COLOR)
    out(sca(mat, 'Roughness', .6, -600, 50), MP.MP_ROUGHNESS)
    out(sca(mat, 'Metallic', 0., -600, 150), MP.MP_METALLIC)
    out(vec(mat, 'Emissive', (0, 0, 0), -600, 250), MP.MP_EMISSIVE_COLOR)
    out(vc, MP.MP_AMBIENT_OCCLUSION, 'R')


def build_skin(mat, default):
    M = unreal.MaterialExpressionMultiply
    t = tex_param(mat, 'Diffuse', default, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -800, 0)
    out(op(mat, M, t, vec(mat, 'Tint', (1, 1, 1), -800, 250), -500, 0, 'RGB', ''), MP.MP_BASE_COLOR)
    out(sca(mat, 'Roughness', .48, -500, 200), MP.MP_ROUGHNESS)
    out(sca(mat, 'Specular', .42, -500, 300), MP.MP_SPECULAR)
    out(vec(mat, 'SubsurfaceColor', (.72, .2, .12), -500, 400), MP.MP_SUBSURFACE_COLOR)
    out(sca(mat, 'Scatter', .35, -500, 500), MP.MP_OPACITY)


def master(name, build, **flags):
    mat = kit.master(name, build, **flags)
    # The flying arrows are instanced static meshes with the ranger's materials (ATempleSanctumBattle).
    if not mat.get_editor_property('used_with_instanced_static_meshes'):
        mat.set_editor_property('used_with_instanced_static_meshes', True)
        LIB.recompile_material(mat); ASSETS.save_loaded_asset(mat, only_if_is_dirty=False)
    return mat


# ---- The fight's effects (ATempleSanctumBattle, ATempleHero): drawn on instanced camera-facing quads like the
# temple fire (Tools/import_temple_fire.py: per-instance frame, heat, opacity), with its CC0 Unity Labs flipbooks.
FX = f'{DEST}/FX'
FLIP = r"""
float n = COLS * ROWS;
float f = fmod(max(Frame, 0.), n), f0 = floor(f), b = f - f0, f1 = fmod(f0 + 1., n);
float2 c0 = float2(fmod(f0, COLS), floor(f0 / COLS)), c1 = float2(fmod(f1, COLS), floor(f1 / COLS));
float2 uv = saturate(UV);
float4 s = lerp(Texture2DSample(Tex, TexSampler, (uv + c0) / float2(COLS, ROWS)), Texture2DSample(Tex, TexSampler, (uv + c1) / float2(COLS, ROWS)), b);
float fade = saturate(Opacity) * Soft * saturate((Depth - 20.) / 60.);
#if MODE == 0
  // Bone dust: pale, dry, lit a little by the torches.
  float l = dot(s.rgb, float3(.3, .59, .11));
  return float4(float3(.16, .14, .11) * (.55 + .6 * l), s.a * fade * .8);
#else
  // The wizard's fire: sea-green, white at its hot heart, the thin edges darkening to a deep teal.
  float l = dot(s.rgb, float3(.3, .59, .11));
  float h = saturate(Heat);
  float3 col = lerp(float3(.02, .35, .28) * l, float3(.75, 1., .92) * l, saturate(h * (.4 + .8 * l)));
  return float4(col * GLOW * (.2 + 1.1 * h) * fade, 1.);
#endif
"""


def fx_flipbook(name, texture_path, cols, rows, mode, glow):
    tex = unreal.load_asset(texture_path)
    if not tex: raise RuntimeError(f'Missing {texture_path} (Tools/import_temple_fire.py)')
    if unreal.load_asset(f'{FX}/{name}'): unreal.log(f'CHUCK_HERO_FX {name} kept'); return   # held by the game module: -Clean rebuilds it
    mat = TOOLS.create_asset(name, FX, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT if mode == 0 else unreal.BlendMode.BLEND_ADDITIVE)
    mat.set_editor_property('two_sided', True)
    mat.set_editor_property('used_with_instanced_static_meshes', True)
    t = node(mat, unreal.MaterialExpressionTextureObject, -900, 0, texture=tex)
    uv = node(mat, unreal.MaterialExpressionTextureCoordinate, -900, 150)
    data = [node(mat, unreal.MaterialExpressionPerInstanceCustomData, -900, 250 + 100 * i, data_index=i) for i in range(3)]
    soft = node(mat, unreal.MaterialExpressionDepthFade, -900, 600, fade_distance_default=30.)
    depth = node(mat, unreal.MaterialExpressionPixelDepth, -900, 700)
    field = node(mat, unreal.MaterialExpressionCustom, -500, 200)
    inputs = []
    for key in ('Tex', 'UV', 'Frame', 'Heat', 'Opacity', 'Soft', 'Depth'):
        item = unreal.CustomInput(); item.set_editor_property('input_name', key); inputs.append(item)
    field.set_editor_property('inputs', inputs)
    field.set_editor_property('output_type', unreal.CustomMaterialOutputType.CMOT_FLOAT4)
    defines = []
    for k, v in (('COLS', f'{cols}.'), ('ROWS', f'{rows}.'), ('MODE', str(mode)), ('GLOW', f'{glow}')):
        d = unreal.CustomDefine(); d.set_editor_property('define_name', k); d.set_editor_property('define_value', v); defines.append(d)
    field.set_editor_property('additional_defines', defines)
    field.set_editor_property('code', FLIP)
    for key, src in (('Tex', t), ('UV', uv), ('Frame', data[0]), ('Heat', data[1]), ('Opacity', data[2]), ('Soft', soft), ('Depth', depth)):
        link(src, field, key)
    rgb = node(mat, unreal.MaterialExpressionComponentMask, -250, 200, r=True, g=True, b=True, a=False); link(field, rgb, '')
    out(rgb, MP.MP_EMISSIVE_COLOR)
    if mode == 0:
        alpha = node(mat, unreal.MaterialExpressionComponentMask, -250, 350, r=False, g=False, b=False, a=True); link(field, alpha, '')
        out(alpha, MP.MP_OPACITY)
    LIB.recompile_material(mat); ASSETS.save_loaded_asset(mat, only_if_is_dirty=False)
    unreal.log(f'CHUCK_HERO_FX {name}')


def fx_glow():
    if unreal.load_asset(f'{FX}/M_HeroGlow'): unreal.log('CHUCK_HERO_FX M_HeroGlow kept'); return
    mat = TOOLS.create_asset('M_HeroGlow', FX, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('used_with_instanced_static_meshes', True)
    out(node(mat, unreal.MaterialExpressionConstant3Vector, -400, 0, constant=unreal.LinearColor(3., 9., 7.5, 1)), MP.MP_EMISSIVE_COLOR)
    LIB.recompile_material(mat); ASSETS.save_loaded_asset(mat, only_if_is_dirty=False)
    unreal.log('CHUCK_HERO_FX M_HeroGlow')


def fx_trail():
    """The blade's passage: a faint pale smear on the procedural ribbon (vertex alpha: age and place along the blade)."""
    if unreal.load_asset(f'{FX}/M_HeroTrail'): unreal.log('CHUCK_HERO_FX M_HeroTrail kept'); return
    mat = TOOLS.create_asset('M_HeroTrail', FX, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property('two_sided', True)
    vc = node(mat, unreal.MaterialExpressionVertexColor, -700, 100)
    col = node(mat, unreal.MaterialExpressionConstant3Vector, -700, -100, constant=unreal.LinearColor(.55, .56, .58, 1))
    out(op(mat, unreal.MaterialExpressionMultiply, col, vc, -400, -50, '', 'A'), MP.MP_EMISSIVE_COLOR)
    out(op(mat, unreal.MaterialExpressionMultiply, vc, const(mat, .38, -700, 300), -400, 200, 'A', ''), MP.MP_OPACITY)
    LIB.recompile_material(mat); ASSETS.save_loaded_asset(mat, only_if_is_dirty=False)
    unreal.log('CHUCK_HERO_FX M_HeroTrail')


def import_arrow():
    """The loosed arrow (Props/SM_HeroArrow.fbx), with the ranger's materials."""
    info = MANIFEST['heroes'].get('Ranger', {})
    if 'arrow_fbx' not in info: return
    o = unreal.FbxImportUI()
    o.import_mesh = True; o.import_as_skeletal = False
    o.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    o.automated_import_should_detect_type = False
    o.import_materials = o.import_textures = o.import_animations = False
    o.static_mesh_import_data.set_editor_property('combine_meshes', True)
    o.static_mesh_import_data.set_editor_property('auto_generate_collision', False)
    o.static_mesh_import_data.set_editor_property('vertex_color_import_option', unreal.VertexColorImportOption.REPLACE)
    t = unreal.AssetImportTask()
    t.filename = str(SOURCE / info['arrow_fbx']); t.destination_path = f'{DEST}/Props'; t.destination_name = 'SM_HeroArrow'
    t.automated = True; t.replace_existing = True; t.save = True; t.options = o
    TOOLS.import_asset_tasks([t])
    mesh = unreal.load_asset(f'{DEST}/Props/SM_HeroArrow')
    if not isinstance(mesh, unreal.StaticMesh): raise RuntimeError('SM_HeroArrow')
    mats = mesh.get_editor_property('static_materials')
    for i, m in enumerate(mats):
        slot = str(m.get_editor_property('material_slot_name')).split('.')[0]
        mi = unreal.load_asset(f'{DEST}/Ranger/MI_Ranger_{slot}')
        if not mi: raise RuntimeError(f'SM_HeroArrow: no MI_Ranger_{slot}')
        m.set_editor_property('material_interface', mi); mats[i] = m
    mesh.set_editor_property('static_materials', mats)
    ASSETS.save_loaded_asset(mesh, only_if_is_dirty=False)
    box = mesh.get_bounds().box_extent
    unreal.log(f'CHUCK_HERO_ARROW slots={[str(m.material_slot_name) for m in mats]} extent=({box.x:.1f},{box.y:.1f},{box.z:.1f})')


def bone_for_instances():
    """The bone splinters are instances of the skeletons' bone material."""
    mat = unreal.load_asset('/Game/Characters/Skeleton/M_SkeletonBone')
    if mat and not mat.get_editor_property('used_with_instanced_static_meshes'):
        mat.set_editor_property('used_with_instanced_static_meshes', True)
        LIB.recompile_material(mat); ASSETS.save_loaded_asset(mat, only_if_is_dirty=False)
    unreal.log(f'CHUCK_HERO_FX bone_instanced={bool(mat)}')


def surf_textures(s):
    folder = f'{DEST}/Surfaces'
    src = ROOT / s['folder']
    if s['metal'] >= .5:
        return 'metal', {'Diffuse': texture(src / f"{s['set']}_diff_2k.jpg", folder),
                         'Rough': texture(src / f"{s['set']}_rough_2k.jpg", folder, 'masks'),
                         'Metal': texture(src / f"{s['set']}_metal_2k.jpg", folder, 'masks'),
                         'Normal': texture(src / f"{s['set']}_nor_dx_2k.jpg", folder, 'normal')}
    return 'cloth', {'Diffuse': texture(src / f"{s['set']}_diff_2k.jpg", folder),
                     'ARM': texture(src / f"{s['set']}_arm_2k.jpg", folder, 'masks'),
                     'Normal': texture(src / f"{s['set']}_nor_dx_2k.jpg", folder, 'normal')}


def mi(name, folder, parent, textures=None, vectors=None, scalars=None):
    path = f'{folder}/{name}'
    inst = unreal.load_asset(path) or TOOLS.create_asset(name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    LIB.set_material_instance_parent(inst, parent)
    for k, v in (textures or {}).items(): LIB.set_material_instance_texture_parameter_value(inst, k, v)
    for k, v in (vectors or {}).items(): LIB.set_material_instance_vector_parameter_value(inst, k, unreal.LinearColor(*v, 1))
    for k, v in (scalars or {}).items(): LIB.set_material_instance_scalar_parameter_value(inst, k, v)
    LIB.update_material_instance(inst)
    ASSETS.save_loaded_asset(inst, only_if_is_dirty=False)
    return inst


def slot_material(hero, folder, slot, info):
    if info['type'] == 'pbr':
        if not info.get('set'):
            parent = master('M_HeroSolid', build_solid)
            return mi(f'MI_{hero}_{slot}', folder, parent, vectors={'Tint': info['tint'], 'Emissive': info.get('emissive', (0, 0, 0))},
                      scalars={'Roughness': info['rough'], 'Metallic': info['metal']})
        kind, tex = surf_textures(info)
        if kind == 'metal':
            parent = master('M_HeroMetal', lambda m: build_metal(m, tex['Diffuse'], tex['Rough'], tex['Metal'], tex['Normal']))
            return mi(f'MI_{hero}_{slot}', folder, parent, tex, {'Tint': info['tint']}, {'RoughScale': info['rough']})
        parent = master('M_HeroCloth', lambda m: build_cloth(m, tex['Diffuse'], tex['ARM'], tex['Normal']))
        return mi(f'MI_{hero}_{slot}', folder, parent, tex, {'Tint': info['tint']}, {'Gain': info.get('gain', 1.), 'RoughScale': info['rough']})
    if info['type'] == 'skin':
        tex = texture(SOURCE / info['texture'], f'{DEST}/Textures')
        parent = master('M_HeroSkin', lambda m: build_skin(m, tex), shading_model=unreal.MaterialShadingModel.MSM_SUBSURFACE)
        return mi(f'MI_{hero}_{slot}', folder, parent, {'Diffuse': tex}, {'Tint': info.get('tint', (1, 1, 1))})
    # the kit's own generated textures (beard locks, strands) stay in the humans' folder
    kit.SOURCE = SOURCE if 'texture' not in info or (SOURCE / info['texture']).exists() else ROOT / 'SourceAssets/NPCs/Humans'
    try: return kit.slot_material(hero, folder, slot, info)
    finally: kit.SOURCE = SOURCE


def import_hero(name, info):
    folder = f'{DEST}/{name}'
    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = True
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
    options.automated_import_should_detect_type = False
    options.import_materials = options.import_textures = options.import_animations = False
    options.create_physics_asset = False
    skel_path = f'{folder}/SKEL_Hero{name}'
    existing = unreal.load_asset(skel_path)
    if existing: options.skeleton = existing
    data = options.skeletal_mesh_import_data
    data.set_editor_property('update_skeleton_reference_pose', True)
    data.set_editor_property('normal_import_method', unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)
    data.set_editor_property('vertex_color_import_option', unreal.VertexColorImportOption.REPLACE)
    task = unreal.AssetImportTask()
    task.filename = str(SOURCE / info['fbx']); task.destination_path = folder; task.destination_name = f'SK_Hero{name}'
    task.automated = True; task.replace_existing = True; task.save = True; task.options = options
    TOOLS.import_asset_tasks([task])
    mesh = unreal.load_asset(f'{folder}/SK_Hero{name}')
    if not isinstance(mesh, unreal.SkeletalMesh): raise RuntimeError(f'{name}: not a skeletal mesh')
    if not existing:
        made = mesh.get_editor_property('skeleton')
        if not ASSETS.rename_asset(made.get_path_name().split('.')[0], skel_path): raise RuntimeError(f'{name}: skeleton rename')
        mesh = unreal.load_asset(f'{folder}/SK_Hero{name}')
    materials = {slot: slot_material(name, folder, slot, s) for slot, s in info['slots'].items()}
    mats = mesh.get_editor_property('materials')
    for i, m in enumerate(mats):
        slot = str(m.get_editor_property('material_slot_name')).split('.')[0]
        if slot not in materials: raise RuntimeError(f'{name}: unexpected slot {slot}')
        m.material_interface = materials[slot]; mats[i] = m
    mesh.set_editor_property('materials', mats)
    lod_ok = False
    try:
        sub = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
        lod_ok = sub.regenerate_lod(mesh, 4, True, False)
    except Exception as e:
        unreal.log_warning(f'CHUCK_HERO_LOD_FAILED {name} {e}')
    ASSETS.save_loaded_asset(mesh, only_if_is_dirty=False)
    skeleton = mesh.get_editor_property('skeleton')
    ASSETS.save_loaded_asset(skeleton, only_if_is_dirty=False)
    clips = json.loads((SOURCE / f'Hero{name}/Anim/clips.json').read_text(encoding='utf-8'))['clips']
    for clip, c in clips.items():
        o = unreal.FbxImportUI()
        o.import_mesh = False; o.import_as_skeletal = True; o.import_animations = True
        o.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION
        o.automated_import_should_detect_type = False
        o.skeleton = skeleton
        o.import_materials = o.import_textures = False
        d = o.anim_sequence_import_data
        d.set_editor_property('use_default_sample_rate', False); d.set_editor_property('custom_sample_rate', 30)
        d.set_editor_property('remove_redundant_keys', False); d.set_editor_property('import_bone_tracks', True)
        d.set_editor_property('animation_length', unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
        t = unreal.AssetImportTask()
        t.filename = str(SOURCE / f'Hero{name}' / c['fbx']); t.destination_path = f'{folder}/Anim'; t.destination_name = f'AS_Hero{name}_{clip}'
        t.automated = True; t.replace_existing = True; t.save = False; t.options = o
        TOOLS.import_asset_tasks([t])
        anim = unreal.load_asset(f'{folder}/Anim/AS_Hero{name}_{clip}')
        if not isinstance(anim, unreal.AnimSequence) or anim.get_editor_property('skeleton') != skeleton:
            raise RuntimeError(f'{name} {clip}: wrong animation or skeleton')
        anim.set_editor_property('enable_root_motion', False)
        ASSETS.save_loaded_asset(anim, only_if_is_dirty=False)
        unreal.log(f'CHUCK_HERO_CLIP_IMPORT {name} {clip} length={anim.get_editor_property("sequence_length"):.3f} source={(c["frames"] - 1) / c["fps"]:.3f}')
    box = mesh.get_bounds().box_extent
    sub = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    lods = [sub.get_num_verts(mesh, i) for i in range(sub.get_lod_count(mesh))]
    unreal.log(f'CHUCK_HERO_IMPORTED {name} slots={sorted(materials)} extent=({box.x:.1f},{box.y:.1f},{box.z:.1f}) lod_verts={lods} lod_ok={lod_ok} clips={len(clips)}')


for hero, info in MANIFEST['heroes'].items():
    if not ONLY or hero in ONLY: import_hero(hero, info)
if not ONLY or 'fx' in ONLY:
    fx_flipbook('M_HeroDust', '/Game/Art/ChultTemple/Fire/T_JetSmoke', 8, 8, 0, 1.)
    fx_flipbook('M_HeroBoltFire', '/Game/Art/ChultTemple/Fire/T_JetFireBall', 8, 8, 1, 1.6)
    fx_glow(); fx_trail(); bone_for_instances()
    import_arrow()
unreal.log(f'CHUCK_HEROES_IMPORT_READY {sorted(MANIFEST["heroes"])}')
