"""Refresh Chult surface/wind graphs while preserving the current wet art pass.
Never import meshes/audio or restore the older flat foliage material.
"""
from pathlib import Path
import unreal

source = Path(__file__).with_name('create_chult_wet_materials.py')
scope = {'__file__': str(source), '__name__': 'chult_current_surface_pass'}
exec(compile(source.read_text(encoding='utf-8'), str(source), 'exec'), scope)
unreal.log('CHUCK_CHULT_WIND leaves_cm=1.5 canopy_cm=0.75 bark_cm=0 imports=0')
