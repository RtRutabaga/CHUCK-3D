"""Narrow UE 5.7.4 import of the temple snake sounds (Tools/gen_snake_sfx.py); use -AllowCommandletAudio."""
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1]
count = 0
for path in sorted((ROOT / 'SourceAssets/Audio/SFX').glob('SFX_Snake*.wav')):
    task = unreal.AssetImportTask()
    task.filename = str(path)
    task.destination_path = '/Game/Art/Audio/SFX'
    task.destination_name = path.stem
    task.automated = True
    task.replace_existing = True
    task.save = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    sound = unreal.load_asset(f'/Game/Art/Audio/SFX/{path.stem}')
    if not isinstance(sound, unreal.SoundWave):
        raise RuntimeError(f'Failed to import {path.stem}')
    sound.set_editor_property('looping', False)
    unreal.EditorAssetLibrary.save_loaded_asset(sound)
    count += 1
    unreal.log(f'CHUCK_SNAKE_AUDIO_IMPORTED {path.stem}')
unreal.log(f'CHUCK_SNAKE_AUDIO_DONE sounds={count}')
