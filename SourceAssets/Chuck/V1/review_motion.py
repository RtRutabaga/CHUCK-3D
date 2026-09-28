"""Blender 4.5 LTS: motion review sheets for body language (Idle, WalkLoop).

blender --background SourceAssets/Chuck/V1/Chuck_V1.blend --python SourceAssets/Chuck/V1/review_motion.py -- <out_dir> [Clip ...]

Renders each clip at evenly spaced frames from front, three-quarter and side
cameras (in place, root fixed) and writes one contact sheet per clip:
motion_<Clip>.png (rows = views, columns = frames). Never saves the .blend.
"""
import math
import sys
from pathlib import Path
import bpy
from mathutils import Vector

args = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
out = Path(args[0] if args else 'MotionReview')
CLIPS = args[1:] or ['WalkLoop', 'Idle']
out.mkdir(parents=True, exist_ok=True)
scene = bpy.context.scene
rig = bpy.data.objects['SK_Chuck_Rig']
rig.data.pose_position = 'POSE'
for o in scene.objects:
    if o.type == 'MESH' and o.name not in ('SK_Chuck',):
        o.hide_render = True
bpy.ops.mesh.primitive_plane_add(size=400, location=(0, 0, -.02))
bpy.ops.object.light_add(type='SUN', rotation=(math.radians(50), math.radians(10), math.radians(-35)))
bpy.context.object.data.energy = 4.
bpy.ops.object.light_add(type='SUN', rotation=(math.radians(60), 0, math.radians(100)))
bpy.context.object.data.energy = 2.  # front fill so the front view reads
world = scene.world or bpy.data.worlds.new('World'); scene.world = world
world.use_nodes = True; world.node_tree.nodes['Background'].inputs[1].default_value = .8
scene.render.engine = 'BLENDER_EEVEE_NEXT'
scene.eevee.taa_render_samples = 16
scene.render.resolution_x, scene.render.resolution_y = 360, 480
cam_data = bpy.data.cameras.new('M'); cam = bpy.data.objects.new('M', cam_data)
scene.collection.objects.link(cam); scene.camera = cam
cam_data.lens = 50
VIEWS = {'front': (125, 0, 40), 'three_quarter': (90, -90, 44), 'side': (0, -125, 36)}

def render_clip(name, columns):
    action = bpy.data.actions[f'AS_Chuck_{name}']
    rig.animation_data_create(); rig.animation_data.action = action
    first, last = [int(v) for v in action.frame_range]
    frames = [first + round(i * (last - first) / (columns - 1)) for i in range(columns)]
    tiles = []
    for view, eye in VIEWS.items():
        cam.location = eye
        cam.rotation_euler = (Vector((0, 0, 32)) - Vector(eye)).to_track_quat('-Z', 'Y').to_euler()
        for f in frames:
            scene.frame_set(f)
            path = out / f'_{name}_{view}_{f:03d}.png'
            scene.render.filepath = str(path)
            bpy.ops.render.render(write_still=True)
            tiles.append(path)
    # Compose rows = views, columns = frames.
    w, h = scene.render.resolution_x, scene.render.resolution_y
    sheet = bpy.data.images.new(f'motion_{name}', w * columns, h * len(VIEWS), alpha=False)
    px = [0.] * (w * columns * h * len(VIEWS) * 4)
    for i, tile in enumerate(tiles):
        img = bpy.data.images.load(str(tile))
        src = list(img.pixels)
        r, c = divmod(i, columns)
        oy = (len(VIEWS) - 1 - r) * h
        for y in range(h):
            row = (oy + y) * w * columns + c * w
            px[row * 4:(row + w) * 4] = src[y * w * 4:(y + 1) * w * 4]
        bpy.data.images.remove(img); tile.unlink()
    sheet.pixels = px
    sheet.filepath_raw = str(out / f'motion_{name}.png'); sheet.file_format = 'PNG'; sheet.save()
    print('CHUCK_MOTION_SHEET', name, frames)

for clip in CLIPS:
    # One-shots (roll, side jumps) get more columns to show the whole action.
    render_clip(clip, {'Idle': 5, 'WalkLoop': 6}.get(clip, 8))
rig.animation_data.action = None
print('CHUCK_MOTION_REVIEW_READY', out)
