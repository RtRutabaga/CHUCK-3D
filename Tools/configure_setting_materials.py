"""Enable instanced setting geometry on existing materials; never rebuild their graphs."""
import unreal

for name in ("Stone", "Plaster", "Wood", "WoodLight", "Roof", "Dark", "Amber"):
    path = f"/Game/Art/Materials/M_{name}"
    material = unreal.load_asset(path)
    if not material:
        path = f"/Game/Prototype/Materials/M_{name}"
        material = unreal.load_asset(path)
    if not material:
        raise RuntimeError(f"Missing setting material: {name}")
    material.set_editor_property("used_with_instanced_static_meshes", True)
    unreal.MaterialEditingLibrary.recompile_material(material)
    if not unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save {path}")
    unreal.log(f"CHUCK_SETTING_MATERIAL {path}")
