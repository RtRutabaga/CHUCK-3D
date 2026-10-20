"""Far-canopy materials for Chult's distant jungle (Claude, 2026-10-19; docs/CHULT-UPRIVER.md).
User: "I'd like the distant jungle depth to be visible while standing on top of the pyramid
stairs and looking back where you came from ... without making the game run too heavy".

UnrealEditor-Cmd <uproject> -ExecutePythonScript=Tools/create_far_canopy_materials.py -unattended -nosplash -NoLiveCoding

The jungle's scanned trees (24k triangles, no LODs) stop drawing at 100 m. In their
place beyond that, a light canopy of SM_FallCrown (1.1k triangles) stands over each
patch of trees. These are copies of its two materials that dither in with camera
distance from 75 to 100 m, so near the player the crowns are not there at all:
  M_ChultFarCrownLeaves  (M_FallCrownLeaves: opacity mask x fade)
  M_ChultFarCanopy       (M_FallCanopy, made masked: opacity mask = fade)
"""
import unreal

ASSETS = unreal.EditorAssetLibrary
LIB = unreal.MaterialEditingLibrary
DEST = '/Game/Art/ChultNature'
NEAR, FAR = 7500.0, 10000.0
made = []


def fade(mat):
    """DitherTemporalAA(saturate((distance to camera - NEAR) / (FAR - NEAR)))."""
    world = LIB.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -900, 400)
    cam = LIB.create_material_expression(mat, unreal.MaterialExpressionCameraPositionWS, -900, 520)
    dist = LIB.create_material_expression(mat, unreal.MaterialExpressionDistance, -700, 450)
    LIB.connect_material_expressions(world, '', dist, 'A'); LIB.connect_material_expressions(cam, '', dist, 'B')
    sub = LIB.create_material_expression(mat, unreal.MaterialExpressionSubtract, -550, 450)
    sub.set_editor_property('const_b', NEAR); LIB.connect_material_expressions(dist, '', sub, 'A')
    div = LIB.create_material_expression(mat, unreal.MaterialExpressionDivide, -420, 450)
    div.set_editor_property('const_b', FAR - NEAR); LIB.connect_material_expressions(sub, '', div, 'A')
    sat = LIB.create_material_expression(mat, unreal.MaterialExpressionSaturate, -300, 450)
    LIB.connect_material_expressions(div, '', sat, '')
    dither = LIB.create_material_expression(mat, unreal.MaterialExpressionMaterialFunctionCall, -180, 450)
    dither.set_editor_property('material_function', unreal.load_asset('/Engine/Functions/Engine_MaterialFunctions02/Utility/DitherTemporalAA'))
    if not LIB.connect_material_expressions(sat, '', dither, 'Alpha Threshold'): raise RuntimeError('DitherTemporalAA input')
    return dither


for src, name, masked in (('/Game/Art/Cutscenes/FallToChult/M_FallCrownLeaves', 'M_ChultFarCrownLeaves', True),
                          ('/Game/Art/Cutscenes/FallToChult/M_FallCanopy', 'M_ChultFarCanopy', False)):
    path = f'{DEST}/{name}'
    if ASSETS.does_asset_exist(path): ASSETS.delete_asset(path)
    mat = ASSETS.duplicate_asset(src, path)
    d = fade(mat)
    if masked:
        orig = LIB.get_material_property_input_node(mat, unreal.MaterialProperty.MP_OPACITY_MASK)
        if orig is None: raise RuntimeError(f'{src} has no opacity mask input')
        mul = LIB.create_material_expression(mat, unreal.MaterialExpressionMultiply, -60, 380)
        LIB.connect_material_expressions(orig, '', mul, 'A'); LIB.connect_material_expressions(d, '', mul, 'B')
        LIB.connect_material_property(mul, '', unreal.MaterialProperty.MP_OPACITY_MASK)
    else:
        mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
        LIB.connect_material_property(d, '', unreal.MaterialProperty.MP_OPACITY_MASK)
    LIB.recompile_material(mat); ASSETS.save_loaded_asset(mat, only_if_is_dirty=False)
    made.append(f'{name}:blend={mat.get_editor_property("blend_mode")}')
print('CHUCK_FAR_CANOPY_MATERIALS ' + ' '.join(made))
