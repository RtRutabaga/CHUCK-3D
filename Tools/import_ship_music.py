"""Import the ship's scores only (Claude, 2026-11-02; docs/SHIP.md).

UnrealEditor-Cmd <uproject> -run=pythonscript -script=Tools/import_ship_music.py -AllowCommandletAudio -unattended -nosplash

  SourceAssets/Audio/Ship.wav           -> /Game/Art/Audio/SW_Ship             (looping; the user's ship track, unmodified)
  SourceAssets/Audio/ShipConcertina.wav -> /Game/Art/Audio/SW_ShipConcertina   (looping; their concertina version of it)
Played aboard the pirate ship (DockGameMode's ship branch), from the escape's arrival on: the ship track below decks, the
concertina version on the weather deck. A track already imported is left as it is; set CHUCK_REIMPORT_SHIP_MUSIC=1 to
bring them all in again.
"""
import os
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1]
REIMPORT = os.environ.get('CHUCK_REIMPORT_SHIP_MUSIC') == '1'
for source, name in (('Ship.wav', 'SW_Ship'), ('ShipConcertina.wav', 'SW_ShipConcertina')):
    path = '/Game/Art/Audio/' + name
    if not REIMPORT and unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.log(f'CHUCK_AUDIO_KEPT {name}')
        continue
    task = unreal.AssetImportTask()
    task.filename = str(ROOT / 'SourceAssets/Audio' / source); task.destination_path = '/Game/Art/Audio'; task.destination_name = name
    task.automated = True; task.replace_existing = True; task.save = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    wave = unreal.load_asset(path)
    if not isinstance(wave, unreal.SoundWave):
        raise RuntimeError(name + ' import failed')
    wave.set_editor_property('looping', True); wave.set_editor_property('volume', 1.0)
    if not unreal.EditorAssetLibrary.save_loaded_asset(wave, only_if_is_dirty=False):
        raise RuntimeError(name + ' save failed')
    unreal.log(f'CHUCK_AUDIO_IMPORTED {name} looping=True duration={wave.get_editor_property("duration"):.2f}')
