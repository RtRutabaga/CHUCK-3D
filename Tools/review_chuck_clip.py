"""Blender 4.5: review one of Chuck's v1 clips from the saved Chuck_V1.blend.

blender --background SourceAssets/Chuck/V1/Chuck_V1.blend --python Tools/review_chuck_clip.py -- Crawl Local/CrawlReview

For each of a few frames: the deformed body's bounds (height above the floor,
lowest point), and Workbench renders from the side, front and three-quarter.
Writes <out>/<clip>_<view>_<frame>.png and prints CHUCK_CLIP_REVIEW lines.
"""
import math
import sys
from pathlib import Path
import bpy
from mathutils import Vector

args = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
clip = args[0] if args else 'Crawl'
out = Path(args[1] if len(args) > 1 else 'Local/ClipReview').resolve()
out.mkdir(parents=True, exist_ok=True)
scene = bpy.context.scene
rig = next(o for o in scene.objects if o.type == 'ARMATURE')
body = next(o for o in scene.objects if o.type == 'MESH' and o.parent == rig and o.visible_get())
action = bpy.data.actions[f'AS_Chuck_{clip}']
rig.animation_data_create(); rig.animation_data.action = action
f0, f1 = (int(x) for x in action.frame_range)
scene.render.engine = 'BLENDER_WORKBENCH'
scene.display.shading.light = 'STUDIO'
scene.display.shading.color_type = 'MATERIAL'
scene.render.resolution_x, scene.render.resolution_y = 900, 600
cam_data = bpy.data.cameras.new('ReviewCam'); cam_data.lens = 50
cam = bpy.data.objects.new('ReviewCam', cam_data); scene.collection.objects.link(cam); scene.camera = cam
# A floor so contact reads, and a 42 cm bar as the crevice's roof.
bpy.ops.mesh.primitive_plane_add(size=300, location=(0, 0, 0))
bpy.ops.mesh.primitive_cube_add(size=1, location=(0, 0, 42.5))
roof = bpy.context.object; roof.scale = (80, 80, 1); roof.display_type = 'WIRE'
views = {'side': (Vector((0, -160, 25)), Vector((0, 0, 18))), 'front': (Vector((150, 0, 22)), Vector((0, 0, 18))),
         'three': (Vector((110, -110, 60)), Vector((0, 0, 15)))}
frames = sorted({f0, f0 + (f1 - f0) // 4, f0 + (f1 - f0) // 2, f0 + 3 * (f1 - f0) // 4})
worst_top, worst_low = 0., 1e9
for f in range(f0, f1 + 1):
    scene.frame_set(f)
    deps = bpy.context.evaluated_depsgraph_get()
    ev = body.evaluated_get(deps)
    zs = [(ev.matrix_world @ v.co).z for v in ev.data.vertices]
    worst_top, worst_low = max(worst_top, max(zs)), min(worst_low, min(zs))
    print(f'CHUCK_CLIP_REVIEW clip={clip} frame={f} top={max(zs):.2f} low={min(zs):.2f}')
for f in frames:
    scene.frame_set(f)
    for name, (eye, aim) in views.items():
        cam.location = eye
        cam.rotation_euler = (aim - eye).to_track_quat('-Z', 'Y').to_euler()
        scene.render.filepath = str(out / f'{clip}_{name}_{f:02d}.png')
        bpy.ops.render.render(write_still=True)
print(f'CHUCK_CLIP_REVIEW_DONE clip={clip} frames={f1 - f0 + 1} top={worst_top:.2f} low={worst_low:.2f} out={out}')
