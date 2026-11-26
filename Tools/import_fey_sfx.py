"""Import the Feywild's creature and mechanic sounds (Claude, 2026-11-22; Tools/build_fey_sfx.py).

UnrealEditor-Cmd <uproject> "-ExecutePythonScript=<absolute path to this file>" -unattended -nosplash -NoLiveCoding -AllowCommandletAudio

  SourceAssets/Audio/SFX/<the sounds listed in fey-sfx-manifest.json>.wav -> /Game/Art/Audio/SFX ("loop": true ones loop).
Nothing else is imported or changed.
"""
from pathlib import Path
import json, os
import unreal

ROOT = Path(__file__).resolve().parents[1]
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
ASSETS = unreal.EditorAssetLibrary
DEST = '/Game/Art/Audio/SFX'
SOUNDS = json.loads((ROOT / 'SourceAssets/Audio/SFX/fey-sfx-manifest.json').read_text())['sounds']
# (2026-11-24) and the small delights' synthesised sounds (Tools/gen_fey_delights_sfx.py)
SOUNDS.update(json.loads((ROOT / 'SourceAssets/Audio/SFX/fey-delights-sfx-manifest.json').read_text())['sounds'])
# (2026-11-25) and the great hare's (Tools/build_fey_hare_sfx.py)
SOUNDS.update(json.loads((ROOT / 'SourceAssets/Audio/SFX/fey-hare-sfx-manifest.json').read_text())['sounds'])
ONLY = [n for n in os.environ.get('CHUCK_FEY_SFX_ONLY', '').split(',') if n]   # (a comma list: import only these, so the rest do not churn)
for name, info in sorted(SOUNDS.items()):
    if ONLY and name not in ONLY: continue
    task = unreal.AssetImportTask()
    task.filename = str(ROOT / f'SourceAssets/Audio/SFX/{name}.wav'); task.destination_path = DEST; task.destination_name = name
    task.automated = True; task.replace_existing = True; task.save = True
    TOOLS.import_asset_tasks([task])
    wave = unreal.load_asset(f'{DEST}/{name}')
    if not isinstance(wave, unreal.SoundWave): raise RuntimeError(f'{name} import failed')
    wave.set_editor_property('looping', bool(info['loop']))
    ASSETS.save_loaded_asset(wave, only_if_is_dirty=False)
    unreal.log(f'CHUCK_FEY_AUDIO_IMPORTED {name} duration={wave.get_editor_property("duration"):.2f} loop={info["loop"]}')
unreal.log(f'CHUCK_FEY_AUDIO_READY sounds={len(SOUNDS)}')
