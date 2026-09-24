"""Run once with UnrealEditor-Cmd -run=pythonscript -script=<this file>.

Creates only this project's named prototype assets. Re-running updates materials
and creates the level only if absent, preserving any subsequent map editing.
"""
import unreal

ROOT = "/Game/Prototype"
COLORS = {
    "Gray": (0.30, 0.32, 0.34), "Purple": (0.28, 0.085, 0.43),
    "Pink": (0.50, 0.29, 0.28), "Dark": (0.065, 0.075, 0.08),
    "Stone": (0.35, 0.36, 0.32), "Water": (0.085, 0.22, 0.25),
    "Wood": (0.23, 0.14, 0.08), "WoodLight": (0.38, 0.27, 0.15),
    "Plaster": (0.57, 0.52, 0.40), "Roof": (0.23, 0.23, 0.25),
    "Amber": (0.52, 0.36, 0.15), "Navy": (0.14, 0.19, 0.24),
    "Skin": (0.49, 0.35, 0.26), "Sky": (0.45, 0.57, 0.63),
}
tools = unreal.AssetToolsHelpers.get_asset_tools()
for name, color in COLORS.items():
    path = f"{ROOT}/Materials/M_{name}"
    material = unreal.load_asset(path)
    if material is None:
        material = tools.create_asset(f"M_{name}", f"{ROOT}/Materials", unreal.Material, unreal.MaterialFactoryNew())
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)
    expression = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionConstant3Vector)
    expression.set_editor_property("constant", unreal.LinearColor(*color, 1.0))
    unreal.MaterialEditingLibrary.connect_material_property(expression, "", unreal.MaterialProperty.MP_BASE_COLOR)
    roughness = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionConstant)
    roughness.set_editor_property("r", 0.85)
    unreal.MaterialEditingLibrary.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    if name == "Sky":
        material.set_editor_property("two_sided", True)
        material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
        unreal.MaterialEditingLibrary.connect_material_property(expression, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)

level_path = f"{ROOT}/WaterdeepDocks"
if not unreal.EditorAssetLibrary.does_asset_exist(level_path):
    level_system = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not level_system.new_level(level_path):
        raise RuntimeError("Could not create WaterdeepDocks level")
    level_system.save_current_level()
unreal.log("CHUCK_ASSETS_READY")
