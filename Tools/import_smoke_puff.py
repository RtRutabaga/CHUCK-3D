"""Create M_SmokePuff for Chuck's exhaled smoke (user 2026-09-30: "since Chuck is
always smoking, have him exhale smoke periodically").

UnrealEditor-Cmd <uproject> -ExecutePythonScript=Tools/import_smoke_puff.py -unattended -nosplash -NoLiveCoding

Translucent, unlit, a pale grey that doesn't glow. Drawn on instanced spheres
(/Engine/BasicShapes/Sphere): each puff's opacity comes from per-instance
custom data 0 (so every puff fades on its own), softened toward the silhouette
(1 - Fresnel) and broken up by the cigarette's existing smoke texture panning
slowly upward. An existing material is kept (delete to rebuild).
"""
import unreal

DEST = '/Game/Characters/Chuck/V1/Cigarette'
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


mat = unreal.load_asset(f'{DEST}/M_SmokePuff')
if mat:
    print('CHUCK_SMOKE_PUFF_KEPT')
else:
    texture = unreal.load_asset(f'{DEST}/T_CigaretteSmoke')
    if not texture:
        raise RuntimeError('T_CigaretteSmoke missing (import_chuck_cigarette.py first)')
    mat = TOOLS.create_asset('M_SmokePuff', DEST, unreal.Material, unreal.MaterialFactoryNew())
    try:
        mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
        mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
        mat.set_editor_property('used_with_instanced_static_meshes', True)
        colour = node(mat, unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(.42, .42, .44, 1))
        LIB.connect_material_property(colour, '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        fade = node(mat, unreal.MaterialExpressionPerInstanceCustomData, data_index=0)
        edge = node(mat, unreal.MaterialExpressionFresnel, exponent=1.5, base_reflect_fraction=0.)
        core = node(mat, unreal.MaterialExpressionOneMinus); link(edge, core, '')
        soft = node(mat, unreal.MaterialExpressionPower, const_exponent=1.6); link(core, soft, 'Base')
        uv = node(mat, unreal.MaterialExpressionTextureCoordinate, u_tiling=1.5, v_tiling=1.5)
        pan = node(mat, unreal.MaterialExpressionPanner, speed_x=.02, speed_y=-.12)
        link(uv, pan, 'Coordinate')
        tex = node(mat, unreal.MaterialExpressionTextureSample, texture=texture, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_GRAYSCALE)
        link(pan, tex, 'UVs')
        grain = node(mat, unreal.MaterialExpressionLinearInterpolate, const_a=.45, const_b=1.)   # never fully holed
        link(tex, grain, 'Alpha', 'R')
        a = node(mat, unreal.MaterialExpressionMultiply); link(fade, a, 'A'); link(soft, a, 'B')
        b = node(mat, unreal.MaterialExpressionMultiply); link(a, b, 'A'); link(grain, b, 'B')
        LIB.connect_material_property(b, '', unreal.MaterialProperty.MP_OPACITY)
    except Exception:
        ASSETS.delete_loaded_asset(mat)
        raise
    LIB.recompile_material(mat)
    ASSETS.save_loaded_asset(mat, only_if_is_dirty=False)
print('CHUCK_SMOKE_PUFF_READY', mat.get_path_name())
