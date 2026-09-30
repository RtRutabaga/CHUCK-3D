"""Import the user-supplied docks soundtrack without altering its source WAV."""
from pathlib import Path
import unreal

root = Path(__file__).resolve().parents[1]
task = unreal.AssetImportTask()
task.filename = str(root / 'SourceAssets/Audio/waterdeep_docks.wav')
task.destination_path = '/Game/Art/Audio'
task.destination_name = 'SW_WaterdeepDocks'
task.automated = True
task.replace_existing = True
task.save = True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
sound = unreal.load_asset('/Game/Art/Audio/SW_WaterdeepDocks')
if not isinstance(sound, unreal.SoundWave):
    raise RuntimeError('Soundtrack import failed')
sound.set_editor_property('looping', True)
sound.set_editor_property('volume', 1.0)
if not unreal.EditorAssetLibrary.save_loaded_asset(sound, only_if_is_dirty=False):
    raise RuntimeError('Soundtrack save failed')
unreal.log('CHUCK_MUSIC_IMPORTED looping=true')
