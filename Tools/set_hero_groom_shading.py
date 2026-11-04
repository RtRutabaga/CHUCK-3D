"""Unreal 5.7 editor Python: re-apply the heroes' groom shading (colour, grey, root darkening, roughness, specular) from
SourceAssets/Heroes/manifest.json to their existing MI_<Hero>_<Group> instances, without re-importing the strands
(Claude, 2026-11-04; Tools/import_npc_grooms.py sets the same parameters on a full import).

UnrealEditor-Cmd <uproject> -run=pythonscript -script=Tools/set_hero_groom_shading.py -unattended -nullrhi -nosplash
CHUCK_GROOMS_ONLY = comma-separated hero names (default all).
"""
from pathlib import Path
import json
import os
import unreal

ROOT = Path(__file__).resolve().parents[1]
LIB = unreal.MaterialEditingLibrary
ONLY = {x.strip() for x in os.environ.get('CHUCK_GROOMS_ONLY', '').split(',') if x.strip()}
heroes = json.loads((ROOT / 'SourceAssets/Heroes/manifest.json').read_text(encoding='utf-8'))['heroes']
done = 0
for hero, info in heroes.items():
    if ONLY and hero not in ONLY: continue
    for group, g in (info.get('grooms') or {}).items():
        path = f'/Game/Characters/Heroes/{hero}/MI_{hero}_{group}'
        mi = unreal.load_asset(path)
        if not mi: raise RuntimeError(f'Missing {path}')
        LIB.set_material_instance_vector_parameter_value(mi, 'HairColour', unreal.LinearColor(*g['colour'], 1))
        LIB.set_material_instance_vector_parameter_value(mi, 'GreyColour', unreal.LinearColor(*g['grey_colour'], 1))
        LIB.set_material_instance_scalar_parameter_value(mi, 'Grey', g['grey'])
        LIB.set_material_instance_scalar_parameter_value(mi, 'RootDarken', g['root'])
        LIB.set_material_instance_scalar_parameter_value(mi, 'Roughness', g['roughness'])
        LIB.set_material_instance_scalar_parameter_value(mi, 'Specular', g.get('specular', .5))
        LIB.update_material_instance(mi)
        if not unreal.EditorAssetLibrary.save_loaded_asset(mi, only_if_is_dirty=False): raise RuntimeError(path)
        print('CHUCK_HERO_GROOM_SHADING', hero, group, g['colour'], g['specular'], g['roughness'])
        done += 1
print('CHUCK_HERO_GROOM_SHADING_READY', done)
