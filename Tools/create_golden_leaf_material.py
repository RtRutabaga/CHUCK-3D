import unreal
path='/Game/Art/Pickups/M_GoldenLeaf'
m=unreal.load_asset(path)
if m is None:
    m=unreal.AssetToolsHelpers.get_asset_tools().create_asset('M_GoldenLeaf','/Game/Art/Pickups',unreal.Material,unreal.MaterialFactoryNew())
unreal.MaterialEditingLibrary.delete_all_material_expressions(m)
def color(v,prop):
    e=unreal.MaterialEditingLibrary.create_material_expression(m,unreal.MaterialExpressionConstant3Vector)
    e.set_editor_property('constant',unreal.LinearColor(*v,1))
    unreal.MaterialEditingLibrary.connect_material_property(e,'',prop)
color((.85,.48,.045),unreal.MaterialProperty.MP_BASE_COLOR)
color((.32,.14,.008),unreal.MaterialProperty.MP_EMISSIVE_COLOR)
for value,prop in ((.65,unreal.MaterialProperty.MP_METALLIC),(.3,unreal.MaterialProperty.MP_ROUGHNESS)):
    e=unreal.MaterialEditingLibrary.create_material_expression(m,unreal.MaterialExpressionConstant)
    e.set_editor_property('r',value)
    unreal.MaterialEditingLibrary.connect_material_property(e,'',prop)
unreal.MaterialEditingLibrary.recompile_material(m)
unreal.EditorAssetLibrary.save_loaded_asset(m)
unreal.log('CHUCK_GOLDEN_MATERIAL_READY')
