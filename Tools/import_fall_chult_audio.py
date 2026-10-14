"""Import the Chult score and the fall's recorded sounds only (Claude, 2026-10-13).

UnrealEditor-Cmd <uproject> -ExecutePythonScript=Tools/import_fall_chult_audio.py -unattended -nosplash -NoLiveCoding -AllowCommandletAudio

  SourceAssets/Audio/Chult.wav        -> /Game/Art/Audio/SW_Chult (looping; the user's Chult track, unmodified)
  SourceAssets/Audio/SFX/SFX_Fall*.wav -> /Game/Art/Audio/SFX (Tools/make_fall_sounds.py; loops loop, the thud doesn't)
Narrow on purpose: no meshes or materials (Tools/import_fall_cutscene.py reimports the
whole fall set, whose finish create_chult_wet_materials.py owns).
"""
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1]
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
ASSETS = unreal.EditorAssetLibrary


def sound(source, dest, name, looping):
    if not source.exists():
        unreal.log(f'CHUCK_AUDIO_SKIPPED {name} (no {source.name})'); return
    task = unreal.AssetImportTask()
    task.filename = str(source); task.destination_path = dest; task.destination_name = name
    task.automated = True; task.replace_existing = True; task.save = True
    TOOLS.import_asset_tasks([task])
    wave = unreal.load_asset(f'{dest}/{name}')
    if not isinstance(wave, unreal.SoundWave): raise RuntimeError(f'{name} import failed')
    wave.set_editor_property('looping', looping); wave.set_editor_property('volume', 1.0)
    ASSETS.save_loaded_asset(wave, only_if_is_dirty=False)
    unreal.log(f'CHUCK_AUDIO_IMPORTED {name} looping={looping} duration={wave.get_editor_property("duration"):.2f}')


sound(ROOT / 'SourceAssets/Audio/Chult.wav', '/Game/Art/Audio', 'SW_Chult', True)
for name, looping in (('SFX_FallRush_00', True), ('SFX_FallFlap_00', True), ('SFX_FallWhistle_00', True), ('SFX_FallThud_00', False)):
    sound(ROOT / f'SourceAssets/Audio/SFX/{name}.wav', '/Game/Art/Audio/SFX', name, looping)
unreal.log('CHUCK_FALL_CHULT_AUDIO_READY')
