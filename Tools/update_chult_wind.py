"""Update only shared Chult foliage/bark/ground materials; never import meshes/audio."""
from pathlib import Path
import unreal

source = Path(__file__).with_name('import_fall_cutscene.py')
helpers = source.read_text(encoding='utf-8').split('# Music:')[0]
scope = {'__file__': str(source), '__name__': 'chult_material_helpers'}
exec(compile(helpers, str(source), 'exec'), scope)

def existing(name):
    asset = unreal.load_asset(f"{scope['DEST']}/{name}")
    if asset is None:
        raise RuntimeError(f'Missing existing Chult asset: {name}')
    return asset

material = scope['material']
material('M_FallLeaves', scope['leaves'](existing('T_FallLeaves')))
material('M_FallCanopy', scope['canopy'])
material('M_FallBark', scope['bark'](existing('T_FallBark'), existing('T_FallBark_N')))
material('M_FallGround', scope['ground'](existing('T_FallGround'), existing('T_FallGround_N')))
unreal.log('CHUCK_CHULT_WIND leaves_cm=1.5 canopy_cm=0.75 bark_cm=0 imports=0')
