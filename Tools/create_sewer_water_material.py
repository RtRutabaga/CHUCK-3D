"""Create M_SewerWater: the sewer stream as real shallow running water (user 2026-10-04).

UnrealEditor-Cmd <uproject> -EnablePlugins=Water -ExecutePythonScript=Tools/create_sewer_water_material.py -unattended -nosplash -NoLiveCoding

Replaces the painted-ripple M_SewerStream (Tools/create_sewer_cave_materials.py,
Codex's, left in place) on the stream mesh. Textures are copied once from the
engine's own Water plugin (Unreal Engine content, free for use in UE
projects) into /Game/Art/Textures/Water, so the project needs no plugin:
  T_Water_TilingNormal_Waves_02   wave normals
  T_WaterFlow_01_Foam_Tiled       flowing foam
  Caustics_Tiling_01_HDR          caustic light pattern
The stream mesh's UVs run across it (u, 0..1 over its ~83 cm) and down it
(v, 1 per metre), so the flow is along +v. The material: translucent and lit,
refracting the bed under it; two wave normals scrolling downstream at
different scales and speeds; clearer toward its shallow edges (depth fade);
flowing foam along the banks; a faint caustic shimmer moving through it.
Kept if it exists (delete it to rebuild).
"""
import unreal

DEST = '/Game/Art/Materials'
TEX = '/Game/Art/Textures/Water'
SOURCES = {'T_Water_TilingNormal_Waves_02': '/Water/Textures/Normals/T_Water_TilingNormal_Waves_02',
           'T_WaterFlow_01_Foam_Tiled': '/Water/Textures/Foam/T_WaterFlow_01_Foam_Tiled',
           'Caustics_Tiling_01_HDR': '/Water/Textures/Caustics/Caustics_Tiling_01_HDR'}
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
LIB = unreal.MaterialEditingLibrary
ASSETS = unreal.EditorAssetLibrary

textures = {}
for name, source in SOURCES.items():
    path = f'{TEX}/{name}'
    if not ASSETS.does_asset_exist(path):
        if not ASSETS.does_asset_exist(source):
            raise RuntimeError(f'{source} missing: run with -EnablePlugins=Water')
        ASSETS.duplicate_asset(source, path)
        ASSETS.save_asset(path, only_if_is_dirty=False)
        unreal.log(f'CHUCK_SEWER_WATER_TEXTURE_COPIED {name}')
    textures[name] = unreal.load_asset(path)


def node(mat, cls, **props):
    n = LIB.create_material_expression(mat, cls)
    for k, v in props.items(): n.set_editor_property(k, v)
    return n


def link(a, b, pin_b, pin_a=''):
    if not LIB.connect_material_expressions(a, pin_a, b, pin_b):
        raise RuntimeError(f'Material link failed: {pin_a} -> {pin_b}')


def const(mat, v): return node(mat, unreal.MaterialExpressionConstant, r=v)
def vec3(mat, r, g, b): return node(mat, unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(r, g, b, 1))
def mul(mat, a, b):
    n = node(mat, unreal.MaterialExpressionMultiply); link(a, n, 'A'); link(b, n, 'B'); return n
def add(mat, a, b):
    n = node(mat, unreal.MaterialExpressionAdd); link(a, n, 'A'); link(b, n, 'B'); return n
def lerp(mat, a, b, alpha):
    n = node(mat, unreal.MaterialExpressionLinearInterpolate); link(a, n, 'A'); link(b, n, 'B'); link(alpha, n, 'Alpha'); return n


def flowing(mat, uv, scale_u, scale_v, speed):
    """UV scaled, panned downstream (+v) at speed (UV units per second)."""
    scaled = mul(mat, uv, node(mat, unreal.MaterialExpressionConstant2Vector, r=scale_u, g=scale_v))
    pan = node(mat, unreal.MaterialExpressionPanner, speed_x=0., speed_y=-speed)
    link(scaled, pan, 'Coordinate')
    return pan


def sample(mat, tex, coords, normal=False):
    s = node(mat, unreal.MaterialExpressionTextureSample, texture=tex,
             sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if normal else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    if tex.get_editor_property('compression_settings') == unreal.TextureCompressionSettings.TC_HDR:
        s.set_editor_property('sampler_type', unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
    link(coords, s, 'UVs')
    return s


path = f'{DEST}/M_SewerWater'
if unreal.load_asset(path):
    unreal.log('CHUCK_SEWER_WATER_KEPT')
else:
    mat = TOOLS.create_asset('M_SewerWater', DEST, unreal.Material, unreal.MaterialFactoryNew())
    try:
        mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_TRANSLUCENT)
        mat.set_editor_property('translucency_lighting_mode', unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
        mat.set_editor_property('two_sided', True)   # the stream mesh's faces point down
        # The procedural stream has no tangents: the ripples go in as a world-space
        # normal (the water is level, so the texture's xy tilt is the world's).
        mat.set_editor_property('tangent_space_normal', False)
        mat.set_editor_property('used_with_instanced_static_meshes', False)
        try: mat.set_editor_property('refraction_method', unreal.RefractionMode.RM_INDEX_OF_REFRACTION)
        except Exception: pass
        uv = node(mat, unreal.MaterialExpressionTextureCoordinate)
        across = node(mat, unreal.MaterialExpressionComponentMask, r=True, g=False, b=False, a=False); link(uv, across, '')
        # Ripples: two wave normals running downstream, blended, a little flattened.
        n1 = sample(mat, textures['T_Water_TilingNormal_Waves_02'], flowing(mat, uv, 1.6, 2.0, .32), normal=True)
        n2 = sample(mat, textures['T_Water_TilingNormal_Waves_02'], flowing(mat, uv, .9, 1.1, .19), normal=True)
        blend = node(mat, unreal.MaterialExpressionAdd); link(n1, blend, 'A'); link(n2, blend, 'B')
        flatten = node(mat, unreal.MaterialExpressionMultiply); link(blend, flatten, 'A'); link(vec3(mat, .45, .45, 1.), flatten, 'B')
        norm = node(mat, unreal.MaterialExpressionNormalize); link(flatten, norm, '')
        LIB.connect_material_property(norm, '', unreal.MaterialProperty.MP_NORMAL)
        # Banks: 0 mid-stream .. 1 at its edges.
        centred = node(mat, unreal.MaterialExpressionSubtract, const_b=.5); link(across, centred, 'A')
        off = node(mat, unreal.MaterialExpressionAbs); link(centred, off, '')
        edge = node(mat, unreal.MaterialExpressionSmoothStep); link(const(mat, .3), edge, 'Min'); link(const(mat, .5), edge, 'Max'); link(off, edge, 'Value')
        # Foam running along the banks.
        foam_tex = sample(mat, textures['T_WaterFlow_01_Foam_Tiled'], flowing(mat, uv, 1.2, 1.6, .28))
        foam_r = node(mat, unreal.MaterialExpressionComponentMask, r=True, g=False, b=False, a=False); link(foam_tex, foam_r, '')
        foam = mul(mat, mul(mat, foam_r, edge), const(mat, .8))
        # Colour: a dark, slightly green-grey sewer water; foam greyer and lighter.
        water = vec3(mat, .016, .026, .028)
        col = lerp(mat, water, vec3(mat, .16, .17, .165), foam)
        LIB.connect_material_property(col, '', unreal.MaterialProperty.MP_BASE_COLOR)
        LIB.connect_material_property(lerp(mat, const(mat, .12), const(mat, .6), foam), '', unreal.MaterialProperty.MP_ROUGHNESS)
        LIB.connect_material_property(const(mat, .35), '', unreal.MaterialProperty.MP_SPECULAR)
        # A faint caustic shimmer drifting through it (light off the ripples).
        caust = sample(mat, textures['Caustics_Tiling_01_HDR'], flowing(mat, uv, 1.3, 1.7, .22))
        caust_r = node(mat, unreal.MaterialExpressionComponentMask, r=True, g=False, b=False, a=False); link(caust, caust_r, '')
        shimmer = mul(mat, mul(mat, caust_r, vec3(mat, .0005, .0008, .001)), lerp(mat, const(mat, 1.), const(mat, .2), edge))   # the caustics texture is HDR (values well above 1)
        # A sheen off the ripples at glancing angles, so it reads as water even
        # where the sewer's lights don't reach it; the foam faintly pale.
        fres = node(mat, unreal.MaterialExpressionFresnel, exponent=3.5, base_reflect_fraction=.04)
        link(norm, fres, 'Normal')
        sheen = mul(mat, fres, vec3(mat, .010, .015, .018))
        foam_glow = mul(mat, foam, vec3(mat, .006, .0065, .0065))
        LIB.connect_material_property(add(mat, add(mat, shimmer, sheen), foam_glow), '', unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        # Clearer where it's shallow (the edges of the bed), denser in the middle; foam is opaque.
        depth = node(mat, unreal.MaterialExpressionDepthFade, fade_distance_default=7.)
        body = mul(mat, depth, const(mat, .78))
        op = node(mat, unreal.MaterialExpressionMax); link(body, op, 'A'); link(foam, op, 'B')
        LIB.connect_material_property(op, '', unreal.MaterialProperty.MP_OPACITY)
        LIB.connect_material_property(const(mat, 1.33), '', unreal.MaterialProperty.MP_REFRACTION)
    except Exception:
        ASSETS.delete_loaded_asset(mat); raise
    LIB.recompile_material(mat)
    ASSETS.save_loaded_asset(mat, only_if_is_dirty=False)
    unreal.log('CHUCK_SEWER_WATER_CREATED')
print('CHUCK_SEWER_WATER_READY')
