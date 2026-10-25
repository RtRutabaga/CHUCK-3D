"""Narrow UE 5.7.4 import of the beholder's sounds (Tools/gen_beholder_sfx.py); use -AllowCommandletAudio."""
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1]
count = 0
import os
STEM = os.environ.get('CHUCK_SFX_STEM', 'SFX_Beholder')   # SFX_Seal: the seal's (2026-10-25)
for path in sorted((ROOT / 'SourceAssets/Audio/SFX').glob(f'{STEM}*.wav')):
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
    unreal.EditorAssetLibrary.save_loaded_asset(sound)
    count += 1
    unreal.log(f'CHUCK_BEHOLDER_AUDIO_IMPORTED {path.stem}')
unreal.log(f'CHUCK_BEHOLDER_AUDIO_DONE sounds={count}')
