"""Blender 4.5 LTS: bake Chuck v1 surface textures from procedural 3D shaders.

blender --background SourceAssets/Chuck/V1/Chuck_V1.blend --python SourceAssets/Chuck/V1/bake_textures.py -- [size]

Writes SourceAssets/Chuck/V1/Textures/T_Chuck_{BaseColor,Normal,ORM}.png for
the single `UVMap` atlas shared by all nine material slots. The shaders work in
object space (centimetres), so the automatic UV seams do not show as pattern
breaks. Normal map: tangent space, DirectX convention (green flipped) for
Unreal. ORM: R = ambient occlusion, G = roughness, B = metallic. Never saves
the .blend and does not touch SK_Chuck.fbx.
"""
import sys
import time
from pathlib import Path
import bpy
import numpy as np

SIZE = int(sys.argv[sys.argv.index('--') + 1]) if '--' in sys.argv and len(sys.argv) > sys.argv.index('--') + 1 else 2048
V1 = Path(bpy.data.filepath).parent
sys.path.insert(0, str(V1.parents[2] / 'Tools'))
import chuck_v1_shape as SHAPE  # noqa: E402  (whisker-pad mask follows the snout amendment)
OUT = V1 / 'Textures'; OUT.mkdir(exist_ok=True)
scene = bpy.context.scene
# Bake from the tuft-free variant: same UVs and materials, but the geometric
# tufts no longer drop dark AO dots at their roots (visible under the groom).
# Tuft islands in the parked strip are filled from material means below, so
# the maps stay valid for SK_Chuck too.
body = bpy.data.objects['SK_Chuck_Groomed']
bpy.data.objects['SK_Chuck_Rig'].data.pose_position = 'REST'  # bake the rest shape
body.hide_set(False); body.hide_render = False
bpy.data.objects['SK_Chuck'].hide_render = True

scene.render.engine = 'CYCLES'
scene.cycles.device = 'CPU'
scene.cycles.samples = 8
scene.render.bake.margin = 8
scene.render.bake.margin_type = 'EXTEND'

# ---------------------------------------------------------------- shaders
class Graph:
    def __init__(self, mat):
        mat.use_nodes = True
        self.nt = mat.node_tree; self.nt.nodes.clear()
        self.out = self.node('ShaderNodeOutputMaterial')
        self.bsdf = self.node('ShaderNodeBsdfPrincipled')
        self.link(self.bsdf.outputs['BSDF'], self.out.inputs['Surface'])
        tc = self.node('ShaderNodeTexCoord')
        self.P = tc.outputs['Object']  # object space, centimetres
        geo = self.node('ShaderNodeNewGeometry')
        self.N = geo.outputs['Normal']; self.pointiness = geo.outputs['Pointiness']

    def node(self, kind, **props):
        n = self.nt.nodes.new(kind)
        for k, v in props.items(): setattr(n, k, v)
        return n

    def link(self, a, b): self.nt.links.new(a, b)

    def noise(self, scale, detail=4., rough=.55, vec=None, stretch=None):
        n = self.node('ShaderNodeTexNoise'); n.inputs['Scale'].default_value = scale
        n.inputs['Detail'].default_value = detail; n.inputs['Roughness'].default_value = rough
        src = vec if vec is not None else self.P
        if stretch:
            m = self.node('ShaderNodeVectorMath', operation='MULTIPLY')
            m.inputs[1].default_value = stretch
            self.link(src, m.inputs[0]); src = m.outputs[0]
        self.link(src, n.inputs['Vector'])
        return n.outputs['Fac']

    def math(self, op, a, b=None, clamp=False):
        m = self.node('ShaderNodeMath', operation=op, use_clamp=clamp)
        for i, v in enumerate((a, b)):
            if v is None: continue
            if isinstance(v, (int, float)): m.inputs[i].default_value = v
            else: self.link(v, m.inputs[i])
        return m.outputs[0]

    def ramp(self, fac, stops):
        r = self.node('ShaderNodeValToRGB'); cr = r.color_ramp
        cr.elements[0].position, cr.elements[0].color = stops[0][0], (*stops[0][1], 1)
        cr.elements[1].position, cr.elements[1].color = stops[-1][0], (*stops[-1][1], 1)
        for pos, col in stops[1:-1]:
            e = cr.elements.new(pos); e.color = (*col, 1)
        self.link(fac, r.inputs['Fac'])
        return r.outputs['Color']

    def mix(self, fac, a, b):
        m = self.node('ShaderNodeMix', data_type='RGBA', blend_type='MIX')
        for sock, v in ((m.inputs[0], fac), (m.inputs[6], a), (m.inputs[7], b)):
            if isinstance(v, tuple): sock.default_value = (*v, 1) if len(v) == 3 else v
            elif isinstance(v, (int, float)): sock.default_value = v
            else: self.link(v, sock)
        return m.outputs[2]

    def axis(self, i):
        sep = self.node('ShaderNodeSeparateXYZ'); self.link(self.P, sep.inputs[0])
        return sep.outputs[i]

    def normal_axis(self, i):
        sep = self.node('ShaderNodeSeparateXYZ'); self.link(self.N, sep.inputs[0])
        return sep.outputs[i]

    def finish(self, color, rough, metal=0., height=None, strength=.2, dist=.05):
        self.link(color, self.bsdf.inputs['Base Color'])
        if isinstance(rough, (int, float)): self.bsdf.inputs['Roughness'].default_value = rough
        else: self.link(rough, self.bsdf.inputs['Roughness'])
        self.bsdf.inputs['Metallic'].default_value = metal
        if height is not None:
            b = self.node('ShaderNodeBump'); b.inputs['Strength'].default_value = strength
            b.inputs['Distance'].default_value = dist
            self.link(height, b.inputs['Height']); self.link(b.outputs['Normal'], self.bsdf.inputs['Normal'])
        self.metal = metal

def jacket(g):
    # Oversized, worn purple canvas: blotchy dye variation, sun-faded raised
    # areas and edges (pointiness), grime toward the hem, crumpled wrinkles and
    # a fine diagonal twill/canvas grain in the normal.
    blot = g.noise(.12, 3, .5)
    # Red-violet suede (2026-09-27 turnaround). Calibrated on the Unreal render
    # against Chuck-Turnaround.png (sRGB about 86, 46, 113): Unreal reads these
    # far more saturated than the Blender AgX preview, so green is kept up.
    base = g.ramp(blot, [(.25, (.095, .043, .144)), (.5, (.146, .079, .208)), (.8, (.197, .13, .256))])
    wear = g.math('MULTIPLY', g.math('SUBTRACT', g.pointiness, .5), 7., clamp=True)
    wear = g.math('MULTIPLY', wear, g.noise(1.8, 2, .6))
    crumple = g.noise(.9, 5, .6)
    raised = g.math('MULTIPLY', g.math('SUBTRACT', crumple, .52), 3., clamp=True)
    faded = g.mix(g.math('ADD', wear, g.math('MULTIPLY', raised, .35)), base, (.22, .14, .27))
    # Grime toward the hem, at 26 cm after the v1.1 shape amendment.
    grime = g.math('SUBTRACT', 1., g.math('MULTIPLY', g.math('SUBTRACT', g.axis(2), 26.), .12, clamp=True), clamp=True)
    color = g.mix(g.math('MULTIPLY', grime, .4), faded, (.06, .03, .055))
    twill = g.node('ShaderNodeTexWave', wave_type='BANDS', bands_direction='DIAGONAL')
    twill.inputs['Scale'].default_value = 2.6; twill.inputs['Distortion'].default_value = 2.
    twill.inputs['Detail'].default_value = 1.
    g.link(g.P, twill.inputs['Vector'])
    fibre = g.noise(18, 3, .55, stretch=(1., 1., 3.))
    height = g.math('ADD', g.math('ADD', g.math('MULTIPLY', twill.outputs['Fac'], .35),
                                  g.math('MULTIPLY', fibre, .25)), g.math('MULTIPLY', crumple, 1.6))
    rough = g.math('ADD', .74, g.math('MULTIPLY', wear, .12))
    g.finish(color, rough, 0., height, .45, .05)

def lining(g):
    base = g.ramp(g.noise(.4, 3, .5), [(.3, (.16, .05, .24)), (.7, (.22, .075, .31))])
    g.finish(base, .8, 0., g.noise(30, 2, .5), .08, .02)

def fur(g):
    # Gray-brown coat: darker along the back and crown, streaks that follow the
    # hair direction (stretched vertically), small warm/cool mottling.
    back = g.math('MULTIPLY', g.math('SUBTRACT', 0., g.normal_axis(0)), .8, clamp=True)
    mott = g.noise(.35, 4, .55)
    streak = g.noise(2.2, 6, .65, stretch=(3.5, 3.5, .45))
    tone = g.math('ADD', g.math('MULTIPLY', mott, .5), g.math('MULTIPLY', streak, .5))
    base = g.ramp(tone, [(.25, (.105, .072, .045)), (.5, (.19, .133, .085)), (.75, (.275, .197, .128))])
    color = g.mix(g.math('MULTIPLY', back, .45), base, (.07, .047, .03))
    g.finish(color, g.math('ADD', .78, g.math('MULTIPLY', streak, .12)), 0., streak, .35, .06)

def chest(g):
    streak = g.noise(2.5, 6, .6, stretch=(3.5, 3.5, .45))
    base = g.ramp(g.math('ADD', g.math('MULTIPLY', g.noise(.5, 3, .5), .6), g.math('MULTIPLY', streak, .4)),
                  [(.25, (.3, .245, .18)), (.55, (.39, .33, .255)), (.8, (.46, .4, .315))])
    # Whisker-pad follicle dots (References/ArtDirection/Chuck-Snout-Fur-Target.png):
    # rows of small dark pores on the cream muzzle sides only (v1.2 object space).
    def band(v, a, b):
        return g.math('MULTIPLY', g.math('SUBTRACT', v, a), 1. / (b - a), clamp=True)
    x, z, y = g.axis(0), g.axis(2), g.math('ABSOLUTE', g.axis(1))
    pad = g.math('MULTIPLY', band(x, SHAPE.X(11.8) - .1, SHAPE.X(12.6)), g.math('SUBTRACT', 1., band(x, SHAPE.X(15.2), SHAPE.X(15.9)), clamp=True))
    pad = g.math('MULTIPLY', pad, g.math('MULTIPLY', band(z, 52.1, 52.6), g.math('SUBTRACT', 1., band(z, 54.2, 54.7), clamp=True)))
    pad = g.math('MULTIPLY', pad, band(y, .7, 1.1))
    vor = g.node('ShaderNodeTexVoronoi'); vor.inputs['Scale'].default_value = 2.6
    g.link(g.P, vor.inputs['Vector'])
    dot = g.math('SUBTRACT', 1., g.math('MULTIPLY', g.math('SUBTRACT', vor.outputs['Distance'], .07), 14., clamp=True), clamp=True)
    base = g.mix(g.math('MULTIPLY', g.math('MULTIPLY', dot, pad), .85), base, (.1, .07, .06))
    g.finish(base, .82, 0., streak, .3, .05)

def skin(g):
    # Pink-brown bare skin: mottling, darker creases, ring scales on the tail
    # (x < -8), slightly darker, rougher pads on the paw soles.
    mott = g.noise(1.4, 4, .6)
    base = g.ramp(mott, [(.3, (.4, .165, .14)), (.55, (.52, .235, .195)), (.8, (.6, .29, .24))])
    rings = g.node('ShaderNodeTexWave', wave_type='RINGS', rings_direction='X')
    rings.inputs['Scale'].default_value = .9; rings.inputs['Distortion'].default_value = .4
    g.link(g.P, rings.inputs['Vector'])
    tail = g.math('MULTIPLY', g.math('SUBTRACT', -8., g.axis(0)), .5, clamp=True)
    ring = g.math('MULTIPLY', tail, rings.outputs['Fac'])
    color = g.mix(g.math('MULTIPLY', ring, .3), base, (.3, .12, .1))
    pad = g.math('SUBTRACT', 1., g.math('MULTIPLY', g.axis(2), 2.2), clamp=True)
    color = g.mix(g.math('MULTIPLY', pad, .5), color, (.34, .14, .12))
    height = g.math('ADD', g.math('MULTIPLY', ring, .7), g.math('MULTIPLY', g.noise(9, 3, .6), .3))
    g.finish(color, g.math('ADD', .55, g.math('MULTIPLY', pad, .2)), 0., height, .25, .03)

def eye(g):
    g.finish(g.ramp(g.noise(3, 2, .5), [(0., (.012, .009, .007)), (1., (.03, .022, .016))]), .06)

def claw(g):
    streak = g.noise(3, 4, .6, stretch=(.6, 3, 3))
    g.finish(g.ramp(streak, [(.2, (.5, .46, .4)), (.8, (.72, .69, .62))]), .38, 0., streak, .15, .02)

def metal(g):
    # Worn nickel/steel zipper teeth, as in the reference (not dark brass).
    g.finish(g.ramp(g.noise(6, 3, .6), [(.2, (.42, .41, .39)), (.8, (.62, .6, .57))]),
             g.math('ADD', .32, g.math('MULTIPLY', g.noise(12, 2, .5), .2)), 1.)

def whisker(g):
    g.finish(g.ramp(g.noise(5, 2, .5), [(0., (.42, .39, .31)), (1., (.55, .52, .44))]), .5)

# Linear flat colours for materials whose geometry is entirely parked.
FLAT = {'Metal': (.5, .48, .45), 'Whisker': (.47, .44, .37)}

SHADERS = {'Jacket': jacket, 'Seam': lining, 'Fur': fur, 'Chest': chest, 'Skin': skin,
           'Eye': eye, 'Claw': claw, 'Metal': metal, 'Whisker': whisker}
graphs = {}
for mat in body.data.materials:
    key = mat.name.split('.')[0]
    g = Graph(mat); SHADERS[key](g); graphs[mat.name] = g

# ---------------------------------------------------------------- bakes
def bake(kind, colorspace='sRGB', **kw):
    # Colour bakes into an 8-bit sRGB image (Blender converts from linear);
    # data bakes use float Non-Color buffers.
    img = bpy.data.images.new(f'bake_{kind}', SIZE, SIZE, float_buffer=colorspace != 'sRGB', alpha=False)
    img.colorspace_settings.name = colorspace
    for g in graphs.values():
        n = g.node('ShaderNodeTexImage'); n.image = img
        for other in g.nt.nodes: other.select = False
        n.select = True; g.nt.nodes.active = n
    bpy.ops.object.select_all(action='DESELECT'); body.select_set(True)
    bpy.context.view_layer.objects.active = body
    t = time.time()
    bpy.ops.object.bake(type=kind, **kw)
    print('CHUCK_BAKE', kind, round(time.time() - t, 1), 's')
    px = np.empty(SIZE * SIZE * 4, dtype=np.float32); img.pixels.foreach_get(px)
    return px.reshape(SIZE, SIZE, 4)

def save(name, rgb, colorspace):
    img = bpy.data.images.new(name, SIZE, SIZE, alpha=False)
    img.colorspace_settings.name = colorspace
    rgba = np.ones((SIZE, SIZE, 4), dtype=np.float32); rgba[..., :3] = np.clip(rgb, 0, 1)
    img.pixels.foreach_set(rgba.ravel())
    img.filepath_raw = str(OUT / f'{name}.png'); img.file_format = 'PNG'
    scene.render.image_settings.compression = 90
    img.save()

# Cycles reports no diffuse colour for metallic surfaces, so bake base colour
# with metallic off (metallic goes to the ORM blue channel separately).
for g in graphs.values(): g.bsdf.inputs['Metallic'].default_value = 0.
color = bake('DIFFUSE', pass_filter={'COLOR'})
for g in graphs.values(): g.bsdf.inputs['Metallic'].default_value = g.metal

# The top strip (v > 0.985) holds one flat island per material for fur tufts,
# whiskers and zipper teeth (build_chuck_v1.py parks material index i at
# u = .004 + .008 i). Thousands of differently shaded tufts bake into those
# few texels, so fill each island with its material's mean surface colour,
# measured through a material-ID bake.
strip = int(.987 * SIZE)
mats = list(body.data.materials)
for i, mat in enumerate(mats):
    g = graphs[mat.name]
    e = g.node('ShaderNodeEmission'); e.inputs['Color'].default_value = ((i + .5) / len(mats), 0, 0, 1)
    g.id_emit = e; g.link(e.outputs[0], g.out.inputs['Surface'])
scene.cycles.samples = 1
# Exact IDs: no padding and no pixel filter, so island borders never blend
# two IDs into a third material's value.
scene.render.bake.margin = 0; width = scene.cycles.filter_width; scene.cycles.filter_width = .01
ids = bake('EMIT', 'Non-Color')
scene.render.bake.margin = 8; scene.cycles.filter_width = width
for g in graphs.values(): g.link(g.bsdf.outputs['BSDF'], g.out.inputs['Surface'])
def srgb(c):
    c = np.asarray(c, dtype=np.float32)
    return np.where(c <= .0031308, c * 12.92, 1.055 * np.power(c, 1 / 2.4) - .055)
ISLANDS = []  # (material index, surface mask or None, x0, x1) for the strip fills
for i, mat in enumerate(mats):
    mask = np.abs(ids[..., 0] - (i + .5) / len(mats)) < .1 / len(mats); mask[strip:] = False
    real = mask.sum() > 2e-4 * SIZE * SIZE  # a real surface, not stray border texels
    if real:
        mean = color[mask][:, :3].mean(axis=0)
    else:
        # All of this material's faces are parked (whiskers, zipper teeth): use
        # the shader's intended flat colour (the legacy slot colour is stale).
        mean = srgb(FLAT.get(mat.name.split('.')[0], mat.diffuse_color[:3]))
    x0 = int((.004 + .008 * i - .002) * SIZE); x1 = int((.004 + .008 * i + .007) * SIZE)
    color[strip:, x0:x1, :3] = mean
    ISLANDS.append((i, mask if real else None, x0, x1))
    print('CHUCK_STRAND_COLOUR', mat.name, [round(float(c), 3) for c in mean])
save('T_Chuck_BaseColor', color[..., :3], 'sRGB')
scene.cycles.samples = 4
normal = bake('NORMAL', 'Non-Color', normal_space='TANGENT')
normal[..., 1] = 1 - normal[..., 1]  # OpenGL (Blender) -> DirectX (Unreal) green channel
normal[strip:, :, :3] = (.5, .5, 1.)  # parked strands/teeth: flat tangent normal
save('T_Chuck_Normal', normal[..., :3], 'Non-Color')
rough = bake('ROUGHNESS', 'Non-Color')
scene.cycles.samples = 24
scene.world = scene.world or bpy.data.worlds.new('World')
ao = bake('AO', 'Non-Color')
# Metallic has no bake pass: emit the per-material metallic value and bake EMIT.
for g in graphs.values():
    e = g.node('ShaderNodeEmission'); e.inputs['Color'].default_value = (g.metal,) * 3 + (1,)
    g.link(e.outputs[0], g.out.inputs['Surface'])
scene.cycles.samples = 1
metal = bake('EMIT', 'Non-Color')  # (after the ID bake, emission is rewired to metallic)
# The top strip (v > 0.985) holds the flat per-material islands of fur tufts,
# whiskers and zipper teeth (see build_chuck_v1.py). Their packed geometry
# self-occludes, so baked AO there would blacken them: force it to 1.
ao[strip:, :, :3] = 1.
# The parked islands have no geometry in the tuft-free bake mesh (and would be
# a jumble of thousands of tufts otherwise): give each its material's mean
# roughness and its metallic value.
for i, mask, x0, x1 in ISLANDS:
    g = graphs[mats[i].name]
    rough[strip:, x0:x1, :3] = rough[mask][:, 0].mean() if mask is not None else .45
    metal[strip:, x0:x1, :3] = g.metal
orm = np.stack([ao[..., 0], rough[..., 0], metal[..., 0]], axis=-1)
save('T_Chuck_ORM', orm, 'Non-Color')
print('CHUCK_TEXTURES_READY', SIZE, [p.name for p in sorted(OUT.glob('*.png'))])
