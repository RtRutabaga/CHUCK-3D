"""Blender 4.5 LTS: lit preview of Chuck v1 using only the baked textures.

blender --background SourceAssets/Chuck/V1/Chuck_V1.blend --python SourceAssets/Chuck/V1/preview_textured.py -- <out_dir>

Replaces every material (in memory, never saved) with the same simple setup
an Unreal material would use: T_Chuck_BaseColor, T_Chuck_ORM (R AO, G rough,
B metal) and T_Chuck_Normal (DirectX, green flipped back for Blender). Renders
EEVEE views under warm daylight so the bakes can be judged, not the
procedural source shaders.
"""
import math
import sys
from pathlib import Path
import bpy
from mathutils import Vector

out = Path(sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else 'TexturedPreview')
out.mkdir(parents=True, exist_ok=True)
V1 = Path(bpy.data.filepath).parent
scene = bpy.context.scene
body = bpy.data.objects['SK_Chuck']

def load(name, colorspace):
    img = bpy.data.images.load(str(V1 / 'Textures' / f'{name}.png'))
    img.colorspace_settings.name = colorspace
    return img

base, orm, nrm = load('T_Chuck_BaseColor', 'sRGB'), load('T_Chuck_ORM', 'Non-Color'), load('T_Chuck_Normal', 'Non-Color')
for mat in body.data.materials:
    mat.use_nodes = True
    nt = mat.node_tree; nt.nodes.clear()
    o = nt.nodes.new('ShaderNodeOutputMaterial'); b = nt.nodes.new('ShaderNodeBsdfPrincipled')
    nt.links.new(b.outputs[0], o.inputs[0])
    tb = nt.nodes.new('ShaderNodeTexImage'); tb.image = base
    to = nt.nodes.new('ShaderNodeTexImage'); to.image = orm
    tn = nt.nodes.new('ShaderNodeTexImage'); tn.image = nrm
    sep = nt.nodes.new('ShaderNodeSeparateColor'); nt.links.new(to.outputs[0], sep.inputs[0])
    ao = nt.nodes.new('ShaderNodeMix'); ao.data_type = 'RGBA'; ao.blend_type = 'MULTIPLY'
    ao.inputs[0].default_value = 1.
    nt.links.new(tb.outputs[0], ao.inputs[6]); nt.links.new(sep.outputs[0], ao.inputs[7])
    nt.links.new(ao.outputs[2], b.inputs['Base Color'])
    nt.links.new(sep.outputs[1], b.inputs['Roughness']); nt.links.new(sep.outputs[2], b.inputs['Metallic'])
    # DirectX -> OpenGL: invert green before the normal map node.
    sn = nt.nodes.new('ShaderNodeSeparateColor'); nt.links.new(tn.outputs[0], sn.inputs[0])
    inv = nt.nodes.new('ShaderNodeMath'); inv.operation = 'SUBTRACT'; inv.inputs[0].default_value = 1.
    nt.links.new(sn.outputs[1], inv.inputs[1])
    cn = nt.nodes.new('ShaderNodeCombineColor')
    nt.links.new(sn.outputs[0], cn.inputs[0]); nt.links.new(inv.outputs[0], cn.inputs[1]); nt.links.new(sn.outputs[2], cn.inputs[2])
    nm = nt.nodes.new('ShaderNodeNormalMap'); nt.links.new(cn.outputs[0], nm.inputs['Color'])
    nt.links.new(nm.outputs[0], b.inputs['Normal'])
    if mat.name.split('.')[0] == 'Skin':
        b.inputs['Subsurface Weight'].default_value = .15
        b.inputs['Subsurface Radius'].default_value = (1., .35, .2)

bpy.ops.mesh.primitive_plane_add(size=600, location=(0, 0, -.02))
ground = bpy.context.object
gm = bpy.data.materials.new('Ground'); gm.use_nodes = True
gm.node_tree.nodes['Principled BSDF'].inputs['Base Color'].default_value = (.23, .22, .2, 1)
gm.node_tree.nodes['Principled BSDF'].inputs['Roughness'].default_value = .85
ground.data.materials.append(gm)
bpy.ops.object.light_add(type='SUN', rotation=(math.radians(50), math.radians(10), math.radians(-35)))
sun = bpy.context.object.data; sun.energy = 4.2; sun.color = (1., .93, .82); sun.angle = math.radians(1.5)
world = scene.world or bpy.data.worlds.new('World'); scene.world = world
world.use_nodes = True
bg = world.node_tree.nodes['Background']; bg.inputs[0].default_value = (.42, .55, .75, 1); bg.inputs[1].default_value = .9
scene.render.engine = 'BLENDER_EEVEE_NEXT'
scene.eevee.taa_render_samples = 48
scene.render.resolution_x = scene.render.resolution_y = 900
scene.view_settings.view_transform = 'AgX'
cam_data = bpy.data.cameras.new('Tex'); cam = bpy.data.objects.new('Tex', cam_data)
scene.collection.objects.link(cam); scene.camera = cam
for name, eye, target, lens in (('three_quarter', (120, -95, 48), (0, 0, 33), 45),
                                ('front', (150, -10, 40), (0, 0, 33), 50),
                                ('rear', (-120, 80, 45), (0, 0, 30), 45),
                                ('close_jacket', (40, -38, 38), (3, -3, 32), 55),
                                ('close_head', (40, -32, 57), (6, 0, 53), 60),
                                ('close_paw_tail', (18, -55, 10), (-8, -4, 4), 50)):
    cam_data.lens = lens; cam.location = eye
    cam.rotation_euler = (Vector(target) - Vector(eye)).to_track_quat('-Z', 'Y').to_euler()
    scene.render.filepath = str(out / f'textured_{name}.png')
    bpy.ops.render.render(write_still=True)
print('CHUCK_TEXTURED_PREVIEW_READY', out)
