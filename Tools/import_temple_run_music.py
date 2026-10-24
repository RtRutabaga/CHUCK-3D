"""Import the temple run's and the sanctum's scores only (Claude, 2026-10-23).

UnrealEditor-Cmd <uproject> -ExecutePythonScript=Tools/import_temple_run_music.py -unattended -nosplash -NoLiveCoding -AllowCommandletAudio

  SourceAssets/Audio/Temple2.wav    -> /Game/Art/Audio/SW_ChultTemple2     (looping; the user's "temple 2", unmodified)
  SourceAssets/Audio/BossBattle.wav -> /Game/Art/Audio/SW_ChultTempleBoss  (looping; decoded from the user's boss_battle.mp3)
Played from the trap run's pits through the Astral wind, and in the sanctum
(DockGameMode, ChultTempleInterior.h ChultTempleInteriorMusicPart).
"""
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1]
for source, name in (('Temple2.wav', 'SW_ChultTemple2'), ('BossBattle.wav', 'SW_ChultTempleBoss')):
    task = unreal.AssetImportTask()
    task.filename = str(ROOT / 'SourceAssets/Audio' / source); task.destination_path = '/Game/Art/Audio'; task.destination_name = name
    task.automated = True; task.replace_existing = True; task.save = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    wave = unreal.load_asset(f'/Game/Art/Audio/{name}')
    if not isinstance(wave, unreal.SoundWave): raise RuntimeError(f'{name} import failed')
    wave.set_editor_property('looping', True); wave.set_editor_property('volume', 1.0)
    unreal.EditorAssetLibrary.save_loaded_asset(wave, only_if_is_dirty=False)
    unreal.log(f'CHUCK_AUDIO_IMPORTED {name} looping=True duration={wave.get_editor_property("duration"):.2f}')
