"""Import only the six new stream sounds; preserve all existing audio assets."""
import json
from pathlib import Path
import unreal
root = Path(__file__).resolve().parents[1] / 'SourceAssets/Audio/SFX'
entries = json.loads((root / 'stream-manifest.json').read_text())['sounds']
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
        raise RuntimeError('Stream import failed: ' + entry['name'])
    sound.set_editor_property('looping', False)
    if not unreal.EditorAssetLibrary.save_loaded_asset(sound, only_if_is_dirty=False):
        raise RuntimeError('Stream save failed')
unreal.log('CHUCK_STREAM_IMPORTED 6')
