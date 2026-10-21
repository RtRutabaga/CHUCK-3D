"""Import the temple interior's score only (Claude, 2026-10-20).

UnrealEditor-Cmd <uproject> -ExecutePythonScript=Tools/import_chult_temple_interior_audio.py -unattended -nosplash -NoLiveCoding -AllowCommandletAudio

  SourceAssets/Audio/TempleInterior.wav -> /Game/Art/Audio/SW_ChultTempleInterior (looping; the user's "temple", unmodified)
Played inside the temple, from the sanctuary's cella down (DockGameMode, ChultTempleInterior.h InChultTempleInterior).
"""
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1]
source = ROOT / 'SourceAssets/Audio/TempleInterior.wav'
task = unreal.AssetImportTask()
task.filename = str(source); task.destination_path = '/Game/Art/Audio'; task.destination_name = 'SW_ChultTempleInterior'
task.automated = True; task.replace_existing = True; task.save = True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
wave = unreal.load_asset('/Game/Art/Audio/SW_ChultTempleInterior')
if not isinstance(wave, unreal.SoundWave): raise RuntimeError('SW_ChultTempleInterior import failed')
wave.set_editor_property('looping', True); wave.set_editor_property('volume', 1.0)
unreal.EditorAssetLibrary.save_loaded_asset(wave, only_if_is_dirty=False)
unreal.log(f'CHUCK_AUDIO_IMPORTED SW_ChultTempleInterior looping=True duration={wave.get_editor_property("duration"):.2f}')
