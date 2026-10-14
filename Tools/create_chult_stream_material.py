"""Create only the dedicated stream material; no imports or shared graph edits."""
from pathlib import Path
import unreal

source = Path(__file__).with_name('import_fall_cutscene.py')
scope = {'__file__': str(source), '__name__': 'chult_stream_helpers'}
helpers = source.read_text(encoding='utf-8')
if '# Music:' not in helpers:
    raise RuntimeError('Import helper boundary changed; refusing to execute mesh/audio imports')
exec(compile(helpers.split('# Music:')[0], str(source), 'exec'), scope)
scope['DEST'] = '/Game/Art/ChultTerrain'
unreal.EditorAssetLibrary.make_directory(scope['DEST'])
node, const, colour, mul, lerp, sample, out, material, link = [scope[k] for k in
    ('node', 'const', 'colour', 'mul', 'lerp', 'sample', 'out', 'material', 'link')]

def stream(m):
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property('translucency_lighting_mode', unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    m.set_editor_property('two_sided', True)
    tex = unreal.load_asset('/Game/Art/Textures/Water/T_Water_TilingNormal_Waves_02')
    if tex is None:
        raise RuntimeError('Existing water normal texture is required')
    uv = node(m, unreal.MaterialExpressionTextureCoordinate)
    # Mesh V increases upstream. Positive sample scroll moves ripples downstream.
    pan = node(m, unreal.MaterialExpressionPanner, speed_x=.008, speed_y=.12)
    link(uv, pan, 'Coordinate')
    wave = sample(m, tex, pan, normal=True)
    flat = mul(m, wave, colour(m, .14, .14, 1))
    norm = node(m, unreal.MaterialExpressionNormalize)
    link(flat, norm, '')
    out(m, norm, unreal.MaterialProperty.MP_NORMAL)
    out(m, colour(m, .027, .048, .037), unreal.MaterialProperty.MP_BASE_COLOR)
    out(m, const(m, .12), unreal.MaterialProperty.MP_ROUGHNESS)
    out(m, const(m, .5), unreal.MaterialProperty.MP_SPECULAR)
    vc = node(m, unreal.MaterialExpressionVertexColor)
    out(m, mul(m, vc, const(m, .58), 'A'), unreal.MaterialProperty.MP_OPACITY)

material('M_ChultStream', stream)
unreal.log('CHUCK_CHULT_STREAM_MATERIAL_READY materials=1 texture_imports=0 mesh_imports=0 downstream_flow=1')
