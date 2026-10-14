"""Import the raptor's synthetic sounds only (Claude, 2026-10-14).

UnrealEditor-Cmd <uproject> -ExecutePythonScript=Tools/import_raptor_sfx.py -unattended -nosplash -NoLiveCoding -AllowCommandletAudio

  SourceAssets/Audio/SFX/SFX_Raptor*.wav (Tools/gen_raptor_sfx.py) -> /Game/Art/Audio/SFX
None loops. Nothing else is imported or changed.
"""
from pathlib import Path
import unreal

ROOT = Path(__file__).resolve().parents[1]
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
ASSETS = unreal.EditorAssetLibrary
DEST = '/Game/Art/Audio/SFX'
names = sorted(p.stem for p in (ROOT / 'SourceAssets/Audio/SFX').glob('SFX_Raptor*.wav'))
for name in names:
    task = unreal.AssetImportTask()
    task.filename = str(ROOT / f'SourceAssets/Audio/SFX/{name}.wav'); task.destination_path = DEST; task.destination_name = name
    task.automated = True; task.replace_existing = True; task.save = True
    TOOLS.import_asset_tasks([task])
    wave = unreal.load_asset(f'{DEST}/{name}')
    if not isinstance(wave, unreal.SoundWave): raise RuntimeError(f'{name} import failed')
    wave.set_editor_property('looping', False)
    ASSETS.save_loaded_asset(wave, only_if_is_dirty=False)
    unreal.log(f'CHUCK_RAPTOR_AUDIO_IMPORTED {name} duration={wave.get_editor_property("duration"):.2f}')
unreal.log(f'CHUCK_RAPTOR_AUDIO_READY sounds={len(names)}')
