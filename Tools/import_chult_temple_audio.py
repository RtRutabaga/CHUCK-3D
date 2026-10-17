"""Import the temple approach's score only (Claude, 2026-10-17).

UnrealEditor-Cmd <uproject> -ExecutePythonScript=Tools/import_chult_temple_audio.py -unattended -nosplash -NoLiveCoding -AllowCommandletAudio

  SourceAssets/Audio/Chult3.wav -> /Game/Art/Audio/SW_Chult3 (looping; the user's "Chult 3", unmodified)
Played on the last stretch of road before the temple and round its exterior
(DockGameMode, ChultTemple.h InChultTempleMusicZone).
"""
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1]
source = ROOT / 'SourceAssets/Audio/Chult3.wav'
task = unreal.AssetImportTask()
task.filename = str(source); task.destination_path = '/Game/Art/Audio'; task.destination_name = 'SW_Chult3'
task.automated = True; task.replace_existing = True; task.save = True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
wave = unreal.load_asset('/Game/Art/Audio/SW_Chult3')
if not isinstance(wave, unreal.SoundWave): raise RuntimeError('SW_Chult3 import failed')
wave.set_editor_property('looping', True); wave.set_editor_property('volume', 1.0)
unreal.EditorAssetLibrary.save_loaded_asset(wave, only_if_is_dirty=False)
unreal.log(f'CHUCK_AUDIO_IMPORTED SW_Chult3 looping=True duration={wave.get_editor_property("duration"):.2f}')
