"""Import the NPC voice lines (Tools/build_npc_voice.py) as SoundWaves under /Game/Art/Audio/Voice (always cooked).

UnrealEditor-Cmd <uproject> -ExecutePythonScript=Tools/import_npc_voice.py -unattended -nullrhi -nosplash -NoLiveCoding
"""
import json
from pathlib import Path
import unreal
root = Path(__file__).resolve().parents[1] / 'SourceAssets/NPCs'
lines = json.loads((root / 'Voice/manifest.json').read_text(encoding='utf-8'))['lines']
tasks = []
for key, line in lines.items():
    folder, name = line['asset'].rsplit('/', 1)
    task = unreal.AssetImportTask()
    task.filename = str(root / line['wav']); task.destination_path = folder; task.destination_name = name
    task.automated = True; task.replace_existing = True; task.save = True
    tasks.append(task)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for key, line in lines.items():
    sound = unreal.load_asset(line['asset'])
    if not isinstance(sound, unreal.SoundWave):
        raise RuntimeError('Voice import failed: ' + key)
    if not unreal.EditorAssetLibrary.save_loaded_asset(sound, only_if_is_dirty=False):
        raise RuntimeError('Voice save failed: ' + key)
    unreal.log(f'CHUCK_VOICE_IMPORTED {key} seconds={sound.get_editor_property("duration"):.2f}')
unreal.log(f'CHUCK_VOICE_IMPORT_READY {len(lines)}')
