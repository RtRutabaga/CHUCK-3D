"""Import only the ship's storm sounds (Tools/make_storm_sounds.py) into /Game/Art/Audio/SFX; the six beds loop.

UnrealEditor-Cmd <uproject> -run=pythonscript -script=Tools/import_storm_sfx.py -AllowCommandletAudio -unattended -nosplash
"""
import json
from pathlib import Path
import unreal
root = Path(__file__).resolve().parents[1] / 'SourceAssets/Audio/SFX'
entries = json.loads((root / 'storm-recorded-manifest.json').read_text())['outputs']
LOOPS = ('SFX_StormRain_00', 'SFX_StormRainAbove_00', 'SFX_StormWind_00', 'SFX_StormWindBelow_00', 'SFX_StormFlap_00', 'SFX_StormSea_00')
tasks = []
for entry in entries:
    task = unreal.AssetImportTask()
    task.filename = str(root / entry['file'])
    task.destination_path = '/Game/Art/Audio/SFX'
    task.destination_name = entry['name']
    task.automated = True
    task.replace_existing = True
    task.save = True
    tasks.append(task)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for entry in entries:
    sound = unreal.load_asset('/Game/Art/Audio/SFX/' + entry['name'])
    if not isinstance(sound, unreal.SoundWave):
        raise RuntimeError('Storm sound import failed: ' + entry['name'])
    sound.set_editor_property('looping', entry['name'] in LOOPS)
    if not unreal.EditorAssetLibrary.save_loaded_asset(sound, only_if_is_dirty=False):
        raise RuntimeError('Storm sound save failed: ' + entry['name'])
unreal.log('CHUCK_STORM_SFX_IMPORTED %d' % len(entries))
