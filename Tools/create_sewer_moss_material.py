"""Create M_SewerMoss: the sewer's cigarette tufts (AGrassTuft with SetMoss).

UnrealEditor-Cmd <uproject> -ExecutePythonScript=Tools/create_sewer_moss_material.py -unattended -nosplash -NoLiveCoding

The grass tuft meshes (Tools/build_grass_tuft.py) recoloured as damp moss and
fungus: the vertex colour's brightness only (greyed, squared like M_Grass for
the FBX gamma step), times a dark olive-teal, a faint cold sheen, no wind sway
(nothing blows down there). Kept if it exists (delete to rebuild).
"""
import unreal

DEST = '/Game/Art/Props/Grass'
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


path = f'{DEST}/M_SewerMoss'
mat = unreal.load_asset(path)
if mat:
    unreal.log('CHUCK_SEWER_MOSS_MATERIAL_KEPT')
else:
    mat = TOOLS.create_asset('M_SewerMoss', DEST, unreal.Material, unreal.MaterialFactoryNew())
    try:
        vc = node(mat, unreal.MaterialExpressionVertexColor)
        sq = node(mat, unreal.MaterialExpressionMultiply)
        link(vc, sq, 'A'); link(vc, sq, 'B')
        grey = node(mat, unreal.MaterialExpressionDotProduct)
        link(sq, grey, 'A')
        link(node(mat, unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(.2126, .7152, .0722, 1)), grey, 'B')
        boost = node(mat, unreal.MaterialExpressionMultiply, const_b=3.)   # the grass greys dark; bring it back to ~1
        link(grey, boost, 'A')
        tint = node(mat, unreal.MaterialExpressionMultiply)
        link(boost, tint, 'A')
        link(node(mat, unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(.045, .075, .04, 1)), tint, 'B')
        LIB.connect_material_property(tint, '', unreal.MaterialProperty.MP_BASE_COLOR)
        LIB.connect_material_property(node(mat, unreal.MaterialExpressionConstant, r=.55), '', unreal.MaterialProperty.MP_ROUGHNESS)
        LIB.connect_material_property(node(mat, unreal.MaterialExpressionConstant, r=.4), '', unreal.MaterialProperty.MP_SPECULAR)
        mat.set_editor_property('two_sided', True)
        mat.set_editor_property('used_with_instanced_static_meshes', True)
    except Exception:
        ASSETS.delete_loaded_asset(mat)
        raise
    LIB.recompile_material(mat)
    ASSETS.save_loaded_asset(mat, only_if_is_dirty=False)
    unreal.log('CHUCK_SEWER_MOSS_MATERIAL_CREATED')
print('CHUCK_SEWER_MOSS_READY')
