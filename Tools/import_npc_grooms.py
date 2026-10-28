"""Unreal 5.7 editor Python: the humans' and heroes' strand grooms (Tools/npc_strands.py output,
listed under each NPC's `grooms` in SourceAssets/NPCs/Humans/manifest.json or
SourceAssets/Heroes/manifest.json) into each NPC's folder as GR_<Name>_<Group>, with a material
instance MI_<Name>_<Group> of M_NPCHair (/Game/Characters/Humans: the hair shading model,
the group's colour varied per strand, darker toward the roots, a share of grey strands).

CHUCK_GROOMS_SET = humans | heroes; CHUCK_GROOMS_ONLY = comma-separated names (default all).
The runtime attaches each groom rigidly to its bone (Source/Chuck3D/NPCGroom.cpp): no binding.
"""
from pathlib import Path
import json
import os
import unreal

ROOT = Path(__file__).resolve().parents[1]
SET = os.environ.get('CHUCK_GROOMS_SET', 'humans')
SOURCE = ROOT / ('SourceAssets/Heroes' if SET == 'heroes' else 'SourceAssets/NPCs/Humans')
DEST = '/Game/Characters/Heroes' if SET == 'heroes' else '/Game/Characters/Humans'
MASTER = '/Game/Characters/Humans/M_NPCHair'
ONLY = {x.strip() for x in os.environ.get('CHUCK_GROOMS_ONLY', '').split(',') if x.strip()}
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
LIB = unreal.MaterialEditingLibrary
ASSETS = unreal.EditorAssetLibrary
manifest = json.loads((SOURCE / 'manifest.json').read_text(encoding='utf-8'))
entries = manifest.get('npcs') or manifest.get('heroes') or {}


def master():
    mat = unreal.load_asset(MASTER)
    if mat: return mat
    mat = TOOLS.create_asset('M_NPCHair', '/Game/Characters/Humans', unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_HAIR)
    mat.set_editor_property('two_sided', True)
    LIB.set_material_usage(mat, unreal.MaterialUsage.MATUSAGE_HAIR_STRANDS)
    n = lambda cls, x, y, **p: (lambda e: ([e.set_editor_property(k, v) for k, v in p.items()], e)[1])(LIB.create_material_expression(mat, cls, x, y))
    link = lambda a, b, pin, pa='': LIB.connect_material_expressions(a, pa, b, pin) or (_ for _ in ()).throw(RuntimeError(f'link {pa}->{pin}'))
    vec = lambda name, v, x, y: n(unreal.MaterialExpressionVectorParameter, x, y, parameter_name=name, default_value=unreal.LinearColor(*v, 1))
    sca = lambda name, v, x, y: n(unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=v)
    attrs = n(unreal.MaterialExpressionHairAttributes, -1400, 0)
    colour, grey_col = vec('HairColour', (.06, .045, .033), -1100, -300), vec('GreyColour', (.34, .32, .3), -1100, -150)
    # Each strand its own shade: x0.75..1.25 by its seed.
    vary = n(unreal.MaterialExpressionLinearInterpolate, -1100, 0, const_a=.75, const_b=1.25); link(attrs, vary, 'Alpha', 'Seed')
    shade = n(unreal.MaterialExpressionMultiply, -900, -250); link(colour, shade, 'A'); link(vary, shade, 'B')
    # Grey strands: those whose second random (frac(seed*7.31)) falls under Grey.
    s7 = n(unreal.MaterialExpressionMultiply, -1100, 150, const_b=7.31); link(attrs, s7, 'A', 'Seed')
    fr = n(unreal.MaterialExpressionFrac, -950, 150); link(s7, fr, '')
    grey = sca('Grey', 0., -1100, 300)
    diff = n(unreal.MaterialExpressionSubtract, -800, 200); link(grey, diff, 'A'); link(fr, diff, 'B')
    hard = n(unreal.MaterialExpressionMultiply, -650, 200, const_b=60.); link(diff, hard, 'A')
    mask = n(unreal.MaterialExpressionSaturate, -500, 200); link(hard, mask, '')
    mixed = n(unreal.MaterialExpressionLinearInterpolate, -450, -150); link(shade, mixed, 'A'); link(grey_col, mixed, 'B'); link(mask, mixed, 'Alpha')
    # Darker at the root: x(1 - RootDarken) there, full colour from a quarter of the way out.
    u4 = n(unreal.MaterialExpressionMultiply, -1100, 450, const_b=4.); link(attrs, u4, 'A', 'U')
    us = n(unreal.MaterialExpressionSaturate, -950, 450); link(u4, us, '')
    rd = sca('RootDarken', .6, -950, 550)
    one_minus = n(unreal.MaterialExpressionOneMinus, -800, 550); link(rd, one_minus, '')
    root = n(unreal.MaterialExpressionLinearInterpolate, -650, 450, const_b=1.); link(one_minus, root, 'A'); link(us, root, 'Alpha')
    final = n(unreal.MaterialExpressionMultiply, -300, 0); link(mixed, final, 'A'); link(root, final, 'B')
    LIB.connect_material_property(final, '', unreal.MaterialProperty.MP_BASE_COLOR)
    LIB.connect_material_property(sca('Roughness', .5, -300, 250), '', unreal.MaterialProperty.MP_ROUGHNESS)
    LIB.recompile_material(mat)
    ASSETS.save_loaded_asset(mat, only_if_is_dirty=False)
    return mat


def import_groom(abc, folder, name):
    opts = unreal.GroomImportOptions()
    conv = unreal.GroomConversionSettings()
    conv.rotation = unreal.Vector(90, 0, 0)
    conv.scale = unreal.Vector(1, -1, 1)
    opts.conversion_settings = conv
    task = unreal.AssetImportTask()
    task.filename = str(abc); task.destination_path = folder; task.destination_name = name
    task.automated = True; task.replace_existing = True; task.save = True; task.options = opts
    task.factory = unreal.HairStrandsFactory()
    TOOLS.import_asset_tasks([task])
    groom = unreal.load_asset(f'{folder}/{name}')
    if not groom: raise RuntimeError(f'Groom import failed: {abc}')
    return groom


parent = master()
total = 0
for npc, info in entries.items():
    if ONLY and npc not in ONLY: continue
    for group, g in (info.get('grooms') or {}).items():
        folder = f'{DEST}/{npc}'
        groom = import_groom(SOURCE / g['abc'], folder, f'GR_{npc}_{group}')
        groups = groom.get_editor_property('hair_groups_info')
        curves = groups[0].get_editor_property('num_curves') if groups else 0
        if len(groups) != 1 or curves != g['strands']:
            raise RuntimeError(f'{npc} {group}: {len(groups)} groups, {curves} curves (expected {g["strands"]})')
        info0 = groups[0]
        sizes = {k: round(info0.get_editor_property(k), 4) for k in ('max_curve_length',) if hasattr(info0, k)}
        print('CHUCK_GROOM_INFO', npc, group, sizes, [k for k in dir(info0) if 'width' in k.lower() or 'radius' in k.lower()])
        mi_path = f'{folder}/MI_{npc}_{group}'
        mi = unreal.load_asset(mi_path) or TOOLS.create_asset(f'MI_{npc}_{group}', folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        LIB.set_material_instance_parent(mi, parent)
        LIB.set_material_instance_vector_parameter_value(mi, 'HairColour', unreal.LinearColor(*g['colour'], 1))
        LIB.set_material_instance_vector_parameter_value(mi, 'GreyColour', unreal.LinearColor(*g['grey_colour'], 1))
        LIB.set_material_instance_scalar_parameter_value(mi, 'Grey', g['grey'])
        LIB.set_material_instance_scalar_parameter_value(mi, 'RootDarken', g['root'])
        LIB.set_material_instance_scalar_parameter_value(mi, 'Roughness', g['roughness'])
        LIB.update_material_instance(mi)
        ASSETS.save_loaded_asset(mi, only_if_is_dirty=False)
        if not unreal.ChuckReviewLibrary.configure_groom_material(groom, mi, 'Hair'):
            raise RuntimeError(f'{npc} {group}: material not set')
        ASSETS.save_loaded_asset(groom, only_if_is_dirty=False)
        # The roots where the build put them (component space, cm): the axis conversion is right. A sample
        # of them (ChuckReviewLibrary) against the build's mean and box of all of them.
        roots = unreal.ChuckReviewLibrary.sample_groom_roots(groom)
        box = g['roots_cm']
        mean = [sum(getattr(r, k) for r in roots) / max(1, len(roots)) for k in 'xyz']
        err = max(abs(m - w) for m, w in zip(mean, box['mean'])) if roots else 99.
        outside = max(max(box['min'][i] - getattr(r, k), getattr(r, k) - box['max'][i]) for r in roots for i, k in enumerate('xyz')) if roots else 99.
        print(f'CHUCK_GROOM_IMPORTED {npc} {group} bone={g["bone"]} strands={curves} mean_err_cm={err:.2f} outside_box_cm={outside:.2f}')
        if err > 1.5 or outside > .3: raise RuntimeError(f'{npc} {group}: roots off (mean {err:.2f} cm, box {outside:.2f} cm): {mean} vs {box}')
        total += curves
print('CHUCK_GROOMS_IMPORT_READY', SET, total)
