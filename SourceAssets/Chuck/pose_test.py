"""Blender 4.5 LTS: posed deformation renders of SourceAssets/Chuck/Chuck.blend.

blender --background SourceAssets/Chuck/Chuck.blend --python SourceAssets/Chuck/pose_test.py -- <out_dir>

Poses the existing 14-bone rig well beyond the current runtime swing
(ChuckCharacter.cpp uses about +/-7 degrees) to expose shoulder/elbow clipping
for future run/climb work. Never saves the .blend. Angles are degrees about
world axes; negative Y swings a hanging arm forward (+X); +X raises arm_L
and -X raises arm_R outward.
"""
import math
import sys
from pathlib import Path
import bpy
from mathutils import Matrix, Vector

out = Path(sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else 'PoseTest')
out.mkdir(parents=True, exist_ok=True)
scene = bpy.context.scene
rig = bpy.data.objects['ChuckRig']
for obj in scene.objects:
    obj.hide_render = obj.name != 'SK_ChuckBody'
bpy.ops.mesh.primitive_plane_add(size=400, location=(0, 0, -.05))
scene.render.engine = 'BLENDER_WORKBENCH'
shading = scene.display.shading
shading.light = 'STUDIO'; shading.color_type = 'MATERIAL'
shading.show_cavity = True; shading.cavity_type = 'BOTH'
scene.render.resolution_x = scene.render.resolution_y = 900
scene.world.color = (.55, .6, .65)
cam_data = bpy.data.cameras.new('Pose'); cam = bpy.data.objects.new('Pose', cam_data)
scene.collection.objects.link(cam); scene.camera = cam
cam_data.lens = 50

POSES = {
    'swing35': {'arm_L': ('Y', -35), 'forearm_L': ('Y', -40), 'arm_R': ('Y', 35), 'forearm_R': ('Y', -20)},
    'reach80': {'arm_L': ('Y', -80), 'forearm_L': ('Y', -30), 'arm_R': ('Y', 30), 'forearm_R': ('Y', -60)},
    'raise_side45': {'arm_L': ('X', 45), 'arm_R': ('X', -45), 'forearm_L': ('Y', -30), 'forearm_R': ('Y', -30)},
    'head_turn': {'head': ('Z', 30), 'arm_L': ('Y', -10), 'arm_R': ('Y', 10)},
}
VIEWS = {'three_quarter': ((120, -95, 50), (0, 0, 33)), 'side': ((0, -150, 35), (0, 0, 33)),
         'front': ((150, 0, 38), (0, 0, 33)), 'back_quarter': ((-110, 95, 50), (0, 0, 33)),
         'shoulder_L': ((25, 55, 45), (0, 11, 36)), 'shoulder_R': ((25, -55, 45), (0, -11, 36)),
         'shoulder_R_back': ((-30, -50, 45), (0, -11, 36))}

def pose(spec):
    for pb in rig.pose.bones:
        pb.matrix_basis = Matrix.Identity(4)
    for name, (axis, degrees) in spec.items():
        pb = rig.pose.bones[name]
        rest = pb.bone.matrix_local.to_3x3()
        world = Matrix.Rotation(math.radians(degrees), 3, axis)
        pb.matrix_basis = (rest.inverted() @ world @ rest).to_4x4()
    bpy.context.view_layer.update()

for pose_name, spec in POSES.items():
    pose(spec)
    for view, (eye, target) in VIEWS.items():
        cam.location = eye
        cam.rotation_euler = (Vector(target) - Vector(eye)).to_track_quat('-Z', 'Y').to_euler()
        scene.render.filepath = str(out / f'{pose_name}_{view}.png')
        bpy.ops.render.render(write_still=True)
print('CHUCK_POSE_READY', out)
