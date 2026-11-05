"""Unreal 5.7 editor Python: the heroes' generated LODs draw with the material of the LOD 0 section each section came
from (Claude, 2026-11-04; UChuckReviewLibrary::FixLODMaterials). Their LOD material maps had gone stale, and from
about 15 m the wizard's skirt drew white and the fighter's beard shade grey.

UnrealEditor-Cmd <uproject> -run=pythonscript -script=Tools/fix_hero_lod_materials.py -unattended -nullrhi -nosplash
CHUCK_LOD_FIX=1 repairs and saves; without it, it only reports. Tools/import_temple_heroes.py runs the same repair after
it regenerates the LODs.
"""
import os
import unreal

FIX = os.environ.get('CHUCK_LOD_FIX') == '1'
for hero in ('Fighter', 'Wizard', 'Ranger'):
    mesh = unreal.load_asset(f'/Game/Characters/Heroes/{hero}/SK_Hero{hero}')
    report = unreal.ChuckReviewLibrary.fix_lod_materials(mesh, FIX)
    unreal.log_warning(f'CHUCK_HERO_LOD_MATERIALS {hero} {report}')
    if FIX and not unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False): raise RuntimeError(hero)
unreal.log_warning(f'CHUCK_HERO_LOD_MATERIALS_READY fix={int(FIX)}')
