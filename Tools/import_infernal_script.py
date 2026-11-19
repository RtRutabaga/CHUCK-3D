"""Import the fiend's Infernal script strip (Tools/build_infernal_script.py) as /Game/Art/UI/T_InfernalScript (Claude, 2026-11-18).

UnrealEditor-Cmd <uproject> -ExecutePythonScript=<absolute path to this file> -unattended -nosplash -NoLiveCoding
"""
from pathlib import Path
import unreal

root = Path(__file__).resolve().parents[1]
task = unreal.AssetImportTask()
task.filename = str(root / 'SourceAssets/UI/InfernalScript/T_InfernalScript.png')
task.destination_path = '/Game/Art/UI'
task.destination_name = 'T_InfernalScript'
task.automated = True
task.replace_existing = True
task.save = True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
tex = unreal.load_asset('/Game/Art/UI/T_InfernalScript')
if not isinstance(tex, unreal.Texture2D):
    raise RuntimeError('Infernal script import failed')
# A HUD image: no mips, no streaming, full alpha.
tex.set_editor_property('compression_settings', unreal.TextureCompressionSettings.TC_EDITOR_ICON)
tex.set_editor_property('lod_group', unreal.TextureGroup.TEXTUREGROUP_UI)
tex.set_editor_property('mip_gen_settings', unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
tex.set_editor_property('never_stream', True)
tex.set_editor_property('srgb', True)
if not unreal.EditorAssetLibrary.save_loaded_asset(tex, only_if_is_dirty=False):
    raise RuntimeError('Infernal script save failed')
unreal.log(f'CHUCK_INFERNAL_SCRIPT_IMPORTED {tex.blueprint_get_size_x()}x{tex.blueprint_get_size_y()}')
