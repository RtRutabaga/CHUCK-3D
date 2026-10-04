"""Import only the WAV decoded from the user-supplied Waterdeep night MP3."""
from pathlib import Path
import unreal
root=Path(__file__).resolve().parents[1]
task=unreal.AssetImportTask()
task.filename=str(root/'SourceAssets/Audio/WaterdeepNight.wav')
task.destination_path='/Game/Art/Audio';task.destination_name='SW_WaterdeepNight'
task.automated=True;task.replace_existing=True;task.save=True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
sound=unreal.load_asset('/Game/Art/Audio/SW_WaterdeepNight')
if not isinstance(sound,unreal.SoundWave): raise RuntimeError('Night music import failed')
sound.set_editor_property('looping',True);sound.set_editor_property('volume',1.0)
if not unreal.EditorAssetLibrary.save_loaded_asset(sound,only_if_is_dirty=False): raise RuntimeError('Night music save failed')
unreal.log('CHUCK_NIGHT_MUSIC_READY duration='+str(sound.get_editor_property('duration')))
