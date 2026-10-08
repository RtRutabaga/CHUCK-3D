"""Import the HUD stamina emblem (Tools/build_stamina_emblem.py output).

UnrealEditor-Cmd <uproject> -ExecutePythonScript=Tools/import_stamina_emblem.py -unattended -nosplash -NoLiveCoding -nullrhi

/Game/Art/UI/T_StaminaEmblem: a UI texture (no mips, UserInterface2D
compression, sRGB), always cooked with /Game/Art and loaded by ADockHUD.
"""
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1]
DEST = '/Game/Art/UI'
task = unreal.AssetImportTask()
task.filename = str(ROOT / 'SourceAssets/UI/T_StaminaEmblem.png')
task.destination_path = DEST
task.destination_name = 'T_StaminaEmblem'
task.automated = True
task.replace_existing = True
task.save = True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
texture = unreal.load_asset(DEST + '/T_StaminaEmblem')
if not texture:
    raise RuntimeError('T_StaminaEmblem did not import')
texture.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
texture.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
texture.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
texture.set_editor_property('srgb', True)
unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)
unreal.log(f'CHUCK_STAMINA_EMBLEM_IMPORTED {texture.get_path_name()} {texture.blueprint_get_size_x()}x{texture.blueprint_get_size_y()}')
