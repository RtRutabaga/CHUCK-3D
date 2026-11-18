"""Import the user's Phlegethos fortress encounter track (Claude, 2026-11-17; docs/regions/PHLEGETHOS.md).

The user supplied "Ritual of the Broken Sun.wav" as the encounter soundtrack; it is copied unmodified to
SourceAssets/Audio/RitualOfTheBrokenSun.wav and imported here, looping, as /Game/Art/Audio/SW_PhlegFortressBattle.

UnrealEditor-Cmd <uproject> -ExecutePythonScript=<absolute path to this file> -AllowCommandletAudio -unattended -nosplash -NoLiveCoding
"""
from pathlib import Path
import unreal

root = Path(__file__).resolve().parents[1]
task = unreal.AssetImportTask()
task.filename = str(root / 'SourceAssets/Audio/RitualOfTheBrokenSun.wav')
task.destination_path = '/Game/Art/Audio'
task.destination_name = 'SW_PhlegFortressBattle'
task.automated = True
task.replace_existing = True
task.save = True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
sound = unreal.load_asset('/Game/Art/Audio/SW_PhlegFortressBattle')
if not isinstance(sound, unreal.SoundWave):
    raise RuntimeError('Soundtrack import failed')
sound.set_editor_property('looping', True)
sound.set_editor_property('volume', 1.0)
if not unreal.EditorAssetLibrary.save_loaded_asset(sound, only_if_is_dirty=False):
    raise RuntimeError('Soundtrack save failed')
unreal.log(f'CHUCK_PHLEG_BATTLE_MUSIC_IMPORTED looping=true duration={sound.get_editor_property("duration"):.2f}')
