"""Import the ship's score only (Claude, 2026-11-02; docs/SHIP.md).

UnrealEditor-Cmd <uproject> -run=pythonscript -script=Tools/import_ship_music.py -AllowCommandletAudio -unattended -nosplash

  SourceAssets/Audio/Ship.wav -> /Game/Art/Audio/SW_Ship   (looping; the user's ship track, unmodified)
Played aboard the pirate ship (DockGameMode's ship branch), from the escape's arrival on.
"""
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1]
task = unreal.AssetImportTask()
task.filename = str(ROOT / 'SourceAssets/Audio/Ship.wav'); task.destination_path = '/Game/Art/Audio'; task.destination_name = 'SW_Ship'
task.automated = True; task.replace_existing = True; task.save = True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
wave = unreal.load_asset('/Game/Art/Audio/SW_Ship')
if not isinstance(wave, unreal.SoundWave):
    raise RuntimeError('SW_Ship import failed')
wave.set_editor_property('looping', True); wave.set_editor_property('volume', 1.0)
if not unreal.EditorAssetLibrary.save_loaded_asset(wave, only_if_is_dirty=False):
    raise RuntimeError('SW_Ship save failed')
unreal.log(f'CHUCK_AUDIO_IMPORTED SW_Ship looping=True duration={wave.get_editor_property("duration"):.2f}')
