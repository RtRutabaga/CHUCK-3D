"""Create only the low-detail coastal terrain material; no existing assets edited."""
import unreal
path = '/Game/Art/Materials/M_VistaTerrain'
mat = unreal.load_asset(path)
if not mat:
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_VistaTerrain', '/Game/Art/Materials', unreal.Material, unreal.MaterialFactoryNew())
    lib = unreal.MaterialEditingLibrary
    vertex = lib.create_material_expression(mat, unreal.MaterialExpressionVertexColor)
    lib.connect_material_property(vertex, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
    rough = lib.create_material_expression(mat, unreal.MaterialExpressionConstant)
    rough.set_editor_property('r', 1.0)
    lib.connect_material_property(rough, '', unreal.MaterialProperty.MP_ROUGHNESS)
    mat.set_editor_property('two_sided', True)
    lib.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
unreal.log('CHUCK_VISTA_MATERIAL_READY')
