"""Blender 4.5 LTS: neutral review renders of SourceAssets/Chuck/Chuck.blend.

blender --background SourceAssets/Chuck/Chuck.blend --python SourceAssets/Chuck/review_renders.py -- <out_dir>

Never saves the .blend. Places two foot copies at an approximate neutral stance
(runtime owns real foot placement) and a 180 cm scale bar, then renders
Workbench views.
"""
import sys
from pathlib import Path
import bpy
from mathutils import Vector

out = Path(sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else 'Review')
out.mkdir(parents=True, exist_ok=True)
scene = bpy.context.scene
for obj in scene.objects:
    obj.hide_render = obj.name not in ('SK_ChuckBody', 'SM_ChuckFoot')
foot = bpy.data.objects['SM_ChuckFoot']
foot.location = (1, 7.2, 0)
twin = foot.copy(); twin.location = (1, -7.2, 0); scene.collection.objects.link(twin)
bpy.ops.mesh.primitive_cube_add(size=1, location=(45, 0, 90))
bar = bpy.context.object; bar.scale = (4, 4, 180); bar.name = 'ScaleWorker180'
bpy.ops.mesh.primitive_plane_add(size=400, location=(0, 0, -.05))

scene.render.engine = 'BLENDER_WORKBENCH'
shading = scene.display.shading
shading.light = 'STUDIO'; shading.color_type = 'MATERIAL'
shading.show_cavity = True; shading.cavity_type = 'BOTH'
shading.show_backface_culling = False
scene.render.resolution_x = scene.render.resolution_y = 1000
scene.world.color = (.55, .6, .65)

cam_data = bpy.data.cameras.new('Review'); cam = bpy.data.objects.new('Review', cam_data)
scene.collection.objects.link(cam); scene.camera = cam

def shot(name, eye, target, ortho=None, lens=50, hide_bar=True):
    bar.hide_render = hide_bar
    cam.location = eye
    cam.rotation_euler = (Vector(target) - Vector(eye)).to_track_quat('-Z', 'Y').to_euler()
    if ortho:
        cam_data.type = 'ORTHO'; cam_data.ortho_scale = ortho
    else:
        cam_data.type = 'PERSP'; cam_data.lens = lens
    scene.render.filepath = str(out / f'{name}.png')
    bpy.ops.render.render(write_still=True)

mid = (0, 0, 33)
shot('front', (200, 0, 33), mid, 78)
shot('side', (0, -200, 33), mid, 78)
shot('rear', (-200, 0, 33), mid, 78)
shot('three_quarter', (150, -120, 55), mid, 82)
shot('scale_side', (20, -400, 90), (20, 0, 90), 200, hide_bar=False)
shot('close_front_opening', (60, 0, 34), (6, 0, 32), lens=60)
shot('close_edge_left', (45, 38, 36), (7, 5, 32), lens=60)
shot('close_edge_right', (45, -38, 36), (7, -5, 32), lens=60)
shot('close_collar', (40, -25, 60), (3, 0, 44), lens=60)
shot('close_elbow', (10, -60, 28), (0, -13, 28), lens=60)
shot('close_edge_underside', (40, -20, 5), (6, -5, 22), lens=60)
print('CHUCK_REVIEW_READY', out)
