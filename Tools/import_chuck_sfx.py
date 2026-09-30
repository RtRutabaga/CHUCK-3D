"""Import Chuck's movement SFX (Tools/gen_chuck_sfx.py output) into /Game/Art/Audio/SFX.

UnrealEditor-Cmd <uproject> -run=pythonscript -script=Tools/import_chuck_sfx.py -AllowCommandletAudio
(-AllowCommandletAudio initializes the audio decoder, as for the soundtrack.)
"""
import json
from pathlib import Path
import unreal

root = Path(__file__).resolve().parents[1]
sfx = root / 'SourceAssets/Audio/SFX'
manifest = json.loads((sfx / 'manifest.json').read_text(encoding='utf-8'))
tasks = []
for entry in manifest['sounds']:
    task = unreal.AssetImportTask()
    task.filename = str(sfx / entry['file'])
    task.destination_path = '/Game/Art/Audio/SFX'
    task.destination_name = entry['name']
    task.automated = True
    task.replace_existing = True
    task.save = True
    tasks.append(task)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
count = 0
for entry in manifest['sounds']:
    sound = unreal.load_asset(f"/Game/Art/Audio/SFX/{entry['name']}")
    if not isinstance(sound, unreal.SoundWave):
        raise RuntimeError(f"SFX import failed: {entry['name']}")
    sound.set_editor_property('looping', False)
    if not unreal.EditorAssetLibrary.save_loaded_asset(sound, only_if_is_dirty=False):
        raise RuntimeError(f"SFX save failed: {entry['name']}")
    count += 1
unreal.log(f'CHUCK_SFX_IMPORTED {count}')
