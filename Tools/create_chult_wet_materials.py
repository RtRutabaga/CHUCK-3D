"""Chult wet-forest surface pass. Reuse existing textures; never import meshes.

Owns M_ChultHangingMoss/Rock/Puddle and refreshes only the three shared jungle
materials. Does not regenerate Blender sources, texture images or audio.
"""
from pathlib import Path
import unreal

source = Path(__file__).with_name('import_fall_cutscene.py')
scope = {'__file__': str(source), '__name__': 'chult_surface_helpers'}
helpers = source.read_text(encoding='utf-8')
if '# Music:' not in helpers:
    raise RuntimeError('Import helper boundary changed; refusing to run mesh/audio imports')
exec(compile(helpers.split('# Music:')[0], str(source), 'exec'), scope)
node, const, colour, mul, add, lerp, mask, sample, out, material = [scope[k] for k in
    ('node', 'const', 'colour', 'mul', 'add', 'lerp', 'mask', 'sample', 'out', 'material')]

def asset(path):
    a = unreal.load_asset(path)
    if a is None: raise RuntimeError('Required existing texture absent: ' + path)
    return a

def old(name): return asset(scope['DEST'] + '/' + name)

def scan(name): return asset('/Game/Art/ChultNature/T_' + name + '_2k')

def packed(m, tex, uv):
    s = sample(m, tex, uv)
    s.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    return s

def noise(m, scale, levels=2):
    n = node(m, unreal.MaterialExpressionNoise, scale=scale, levels=levels,
             quality=1, output_min=0., output_max=1.)
    scope['link'](node(m, unreal.MaterialExpressionWorldPosition), n, '')
    return n

def custom(m, code, inputs, kind=unreal.CustomMaterialOutputType.CMOT_FLOAT3):
    args = []
    for name in inputs:
        inp = unreal.CustomInput(); inp.set_editor_property('input_name', name); args.append(inp)
    c = node(m, unreal.MaterialExpressionCustom, code=code, inputs=args, output_type=kind)
    for name, expr in inputs.items(): scope['link'](expr, c, name)
    return c

def leaves(m):
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
    m.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)
    m.set_editor_property('two_sided', True)
    m.set_editor_property('opacity_mask_clip_value', .4)
    vc = node(m, unreal.MaterialExpressionVertexColor)
    tex = old('T_FallLeaves'); s = sample(m, tex)
    variation = noise(m, .027)
    tint = lerp(m, colour(m, .98, .68, .58), colour(m, .85, .52, .72), vc, 'G')
    base = mul(m, mul(m, s, tint, 'RGB'), lerp(m, const(m, .38), const(m, .72), vc, 'R'))
    base = mul(m, base, lerp(m, const(m, .70), const(m, 1.12), variation))
    fine = noise(m, .17, 3)
    base = mul(m, base, lerp(m, const(m, .80), const(m, 1.10), fine))
    blemish = custom(m, 'return smoothstep(.73,.89,N)*.35;', {'N': fine}, unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    base = lerp(m, base, colour(m, .045, .038, .014), blemish)
    out(m, base, unreal.MaterialProperty.MP_BASE_COLOR)
    out(m, s, unreal.MaterialProperty.MP_OPACITY_MASK, 'A')
    out(m, mul(m, base, colour(m, .18, .24, .08)), unreal.MaterialProperty.MP_SUBSURFACE_COLOR)
    # Vein relief is sampled from the existing atlas in the shader. No raster
    # or mesh source is overwritten; alpha/silhouettes remain unchanged.
    obj = node(m, unreal.MaterialExpressionTextureObject, texture=tex)
    uv = node(m, unreal.MaterialExpressionTextureCoordinate)
    norm = custom(m, '''
float2 e=float2(1.0/2048.0,0);
float dx=dot(Texture2DSample(Tex,TexSampler,UV+e).rgb-Texture2DSample(Tex,TexSampler,UV-e).rgb,float3(.25,.6,.15));
float dy=dot(Texture2DSample(Tex,TexSampler,UV+e.yx).rgb-Texture2DSample(Tex,TexSampler,UV-e.yx).rgb,float3(.25,.6,.15));
return normalize(float3(-dx*1.6,dy*1.6,1));''', {'Tex': obj, 'UV': uv})
    out(m, norm, unreal.MaterialProperty.MP_NORMAL)
    out(m, lerp(m, const(m, .28), const(m, .55), fine), unreal.MaterialProperty.MP_ROUGHNESS)
    out(m, const(m, .42), unreal.MaterialProperty.MP_SPECULAR)
    out(m, scope['wind'](m, 1.5, mul(m, vc, vc, 'A', 'A')), unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)

def ground(m):
    pos = node(m, unreal.MaterialExpressionWorldPosition)
    uv = mul(m, mask(m, pos, r=True, g=True), const(m, 1/260.))
    s = sample(m, old('T_FallGround'), uv)
    damp = noise(m, .008, 3)
    moss = custom(m, 'return smoothstep(.65,.84,N);', {'N': noise(m, .0035, 3)}, unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    scan_uv = mul(m, mask(m, pos, r=True, g=True), const(m, 1/320.))
    soil = sample(m, scan('forest_ground_04_diff'), scan_uv)
    soil_mix = custom(m, 'return smoothstep(.25,.75,N)*.65;', {'N': damp}, unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    litter = lerp(m, mul(m, s, colour(m, .37, .43, .28), 'RGB'),
                  mul(m, soil, colour(m, .60, .65, .51), 'RGB'), soil_mix)
    patch = lerp(m, colour(m, .024, .037, .011), colour(m, .065, .079, .024), noise(m, .05))
    out(m, lerp(m, litter, patch, moss), unreal.MaterialProperty.MP_BASE_COLOR)
    n = lerp(m, sample(m, old('T_FallGround_N'), uv, normal=True),
             sample(m, scan('forest_ground_04_nor_dx'), scan_uv, normal=True), soil_mix)
    normalized = node(m, unreal.MaterialExpressionNormalize); scope['link'](n, normalized, '')
    out(m, normalized, unreal.MaterialProperty.MP_NORMAL)
    out(m, packed(m, scan('forest_ground_04_arm'), scan_uv), unreal.MaterialProperty.MP_AMBIENT_OCCLUSION, 'R')
    out(m, lerp(m, lerp(m, const(m, .30), const(m, .72), damp), const(m, .87), moss), unreal.MaterialProperty.MP_ROUGHNESS)
    out(m, const(m, .38), unreal.MaterialProperty.MP_SPECULAR)

def rock(m):
    m.set_editor_property('tangent_space_normal', False)
    p = node(m, unreal.MaterialExpressionWorldPosition)
    uv = mul(m, mask(m, p, r=True, g=True), const(m, .009))
    tex = asset('/Game/Art/Textures/PolyHaven/T_grey_stone_path_diff_2k')
    detail = sample(m, tex, uv)
    up = mask(m, node(m, unreal.MaterialExpressionVertexNormalWS), b=True)
    moss = custom(m, 'return saturate((Up+.15)*smoothstep(.3,.68,N));', {'Up': up, 'N': noise(m, .016)}, unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    stone = mul(m, detail, colour(m, .23, .26, .25), 'RGB')
    green = lerp(m, colour(m, .027, .039, .012), colour(m, .105, .119, .035), noise(m, .09))
    out(m, lerp(m, stone, green, moss), unreal.MaterialProperty.MP_BASE_COLOR)
    normal = custom(m, '''float3 N=normalize(Base); float v=sin(P.x*.16+sin(P.y*.07))*sin(P.z*.12+P.y*.14);
return normalize(N+float3(sin(P.y*.17),cos(P.x*.19),v)*.13);''', {'Base': node(m, unreal.MaterialExpressionVertexNormalWS), 'P': p})
    out(m, normal, unreal.MaterialProperty.MP_NORMAL)
    out(m, lerp(m, const(m, .52), const(m, .88), moss), unreal.MaterialProperty.MP_ROUGHNESS)
    out(m, const(m, .30), unreal.MaterialProperty.MP_SPECULAR)

def bark(m):
    tex, nrm = scan('tree_bark_03_diff'), scan('tree_bark_03_nor_dx')
    vc = node(m, unreal.MaterialExpressionVertexColor)
    uv = node(m, unreal.MaterialExpressionTextureCoordinate)
    s = sample(m, tex, uv)
    variation = noise(m, .013, 3)
    wood = mul(m, s, colour(m, .34, .40, .33), 'RGB')
    wood = mul(m, wood, lerp(m, const(m, .78), const(m, 1.08), variation))
    # The scan already contains lichen/moss. A sparse, broad additional layer
    # keeps the fissures visible instead of covering them with noise blotches.
    moss = custom(m, 'return smoothstep(.52,.82,N)*(.12+.25*M);',
                  {'N': noise(m, .003, 3), 'M': mask(m, vc, g=True)}, unreal.CustomMaterialOutputType.CMOT_FLOAT1)
    col = lerp(m, colour(m, .022, .031, .010), colour(m, .051, .061, .021), noise(m, .12))
    out(m, lerp(m, wood, col, moss), unreal.MaterialProperty.MP_BASE_COLOR)
    out(m, sample(m, nrm, uv, normal=True), unreal.MaterialProperty.MP_NORMAL, 'RGB')
    arm = packed(m, scan('tree_bark_03_arm'), uv)
    out(m, arm, unreal.MaterialProperty.MP_AMBIENT_OCCLUSION, 'R')
    out(m, lerp(m, mul(m, arm, const(m, .82), 'G'), const(m, .89), moss), unreal.MaterialProperty.MP_ROUGHNESS)
    out(m, const(m, .35), unreal.MaterialProperty.MP_SPECULAR)
    out(m, colour(m, 0, 0, 0), unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)

def hanging(m):
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_MASKED)
    m.set_editor_property('two_sided', True)
    m.set_editor_property('opacity_mask_clip_value', .35)
    vc = node(m, unreal.MaterialExpressionVertexColor)
    n = noise(m, .15)
    col = lerp(m, colour(m, .035, .044, .021), colour(m, .15, .16, .069), n)
    out(m, mul(m, col, lerp(m, const(m, .65), const(m, 1.15), vc, 'R')), unreal.MaterialProperty.MP_BASE_COLOR)
    out(m, custom(m, 'return smoothstep(.18,.48,N);', {'N': n}, unreal.CustomMaterialOutputType.CMOT_FLOAT1), unreal.MaterialProperty.MP_OPACITY_MASK)
    out(m, const(m, .81), unreal.MaterialProperty.MP_ROUGHNESS)
    out(m, const(m, .23), unreal.MaterialProperty.MP_SPECULAR)
    out(m, scope['wind'](m, .4, mul(m, vc, const(m, 1), 'A')), unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)

def puddle(m):
    m.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property('translucency_lighting_mode', unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    m.set_editor_property('tangent_space_normal', False)
    m.set_editor_property('two_sided', True)
    m.set_editor_property('screen_space_reflections', True)
    pos = node(m, unreal.MaterialExpressionWorldPosition)
    xy = mask(m, pos, r=True, g=True)
    pan = node(m, unreal.MaterialExpressionPanner, speed_x=.005, speed_y=.003)
    scope['link'](mul(m, xy, const(m, 1/150.)), pan, 'Coordinate')
    water = sample(m, asset('/Game/Art/Textures/Water/T_Water_TilingNormal_Waves_02'), pan, normal=True)
    flat = mul(m, water, colour(m, .065, .065, 1.))
    norm = node(m, unreal.MaterialExpressionNormalize); scope['link'](flat, norm, '')
    out(m, norm, unreal.MaterialProperty.MP_NORMAL)
    out(m, colour(m, .022, .029, .018), unreal.MaterialProperty.MP_BASE_COLOR)
    out(m, const(m, .085), unreal.MaterialProperty.MP_ROUGHNESS)
    out(m, const(m, .48), unreal.MaterialProperty.MP_SPECULAR)
    fres = node(m, unreal.MaterialExpressionFresnel, exponent=4., base_reflect_fraction=.04)
    scope['link'](norm, fres, 'Normal')
    vc = node(m, unreal.MaterialExpressionVertexColor)
    out(m, mul(m, lerp(m, const(m, .45), const(m, .87), fres), vc, '', 'A'), unreal.MaterialProperty.MP_OPACITY)

for name, builder in [('M_FallLeaves', leaves), ('M_FallGround', ground), ('M_FallBark', bark),
                      ('M_ChultRock', rock), ('M_ChultHangingMoss', hanging), ('M_ChultPuddle', puddle)]:
    if 'CHULT_MATERIAL_NAMES' not in globals() or name in CHULT_MATERIAL_NAMES:
        material(name, builder)
unreal.log('CHUCK_CHULT_WET_MATERIALS_READY materials='+str(len(globals().get('CHULT_MATERIAL_NAMES',range(6))))+' texture_imports=0 mesh_imports=0')
