"""Blender 4.5 LTS: reproducible character form study, authored in centimetres.

Run blender --background --python Tools/build_chuck_model.py.
Only creates this project's named source assets; no external models/textures.
"""
from pathlib import Path
import math
import random
import bpy
from mathutils import Vector
from mathutils.noise import noise

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'SourceAssets' / 'Chuck'
OUT.mkdir(parents=True, exist_ok=True)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = .01
PALETTE = {
    'Fur': ((.18, .145, .115), .85),
    'Chest': ((.38, .31, .23), .88),
    'Jacket': ((.115, .025, .215), .78),
    'Seam': ((.23, .075, .34), .82),
    'Skin': ((.32, .145, .12), .62),
    'Eye': ((.018, .012, .008), .12),
    'Claw': ((.43, .37, .28), .5),
    'Metal': ((.22, .16, .07), .36),
    'Whisker': ((.49, .45, .36), .7),
}
MATS = {}
for name, (color, rough) in PALETTE.items():
    mat = bpy.data.materials.new(name)
    mat.diffuse_color = (*color, 1)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get('Principled BSDF')
    bsdf.inputs['Base Color'].default_value = (*color, 1)
    bsdf.inputs['Roughness'].default_value = rough
    if name == 'Metal':
        bsdf.inputs['Metallic'].default_value = .7
    MATS[name] = mat

def finish(obj, name, material):
    obj.name = name
    obj.data.materials.append(MATS[material])
    if obj.type == 'MESH':
        for face in obj.data.polygons:
            face.use_smooth = True
    return obj

def ellipsoid(name, pos, radii, mat, segments=32, rings=20):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=segments, ring_count=rings, location=pos)
    obj = bpy.context.object
    obj.scale = radii
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    return finish(obj, name, mat)

def mesh(name, verts, faces, mat, subdiv=0):
    data = bpy.data.meshes.new(name)
    data.from_pydata(verts, [], faces)
    data.update()
    obj = bpy.data.objects.new(name, data)
    bpy.context.collection.objects.link(obj)
    finish(obj, name, mat)
    if subdiv:
        mod = obj.modifiers.new('Surface', 'SUBSURF')
        mod.levels = subdiv
    return obj

def tube(name, points, radius, mat, resolution=2):
    data = bpy.data.curves.new(name, 'CURVE')
    data.dimensions = '3D'
    data.resolution_u = 8
    data.bevel_depth = radius
    data.bevel_resolution = resolution
    spline = data.splines.new('BEZIER')
    spline.bezier_points.add(len(points)-1)
    for point, pos in zip(spline.bezier_points, points):
        point.co = pos
        point.handle_left_type = 'AUTO'
        point.handle_right_type = 'AUTO'
    obj = bpy.data.objects.new(name, data)
    bpy.context.collection.objects.link(obj)
    return finish(obj, name, mat)

def limb(name, a, b, radius, mat):
    midpoint = (Vector(a)+Vector(b))*.5
    obj = ellipsoid(name, midpoint, (radius, radius, (Vector(b)-Vector(a)).length*.58), mat)
    obj.rotation_mode = 'QUATERNION'
    obj.rotation_quaternion = (Vector(b)-Vector(a)).to_track_quat('Z', 'Y')
    return obj

# Continuous tapered trunk rather than overlapping spherical jacket pieces.
verts, faces = [], []
profile = [(12,4,6,-2),(18,7,9,-2),(25,8.2,10,-1),(34,8,9.5,-.5),(42,6,8,0),(47,4,5,1)]
N = 40
for z, rx, ry, cx in profile:
    for j in range(N):
        a = 2*math.pi*j/N
        verts.append((cx+rx*math.cos(a),ry*math.sin(a),z))
for i in range(len(profile)-1):
    for j in range(N):
        k=i*N+j; n=i*N+(j+1)%N
        faces.append((k,n,n+N,k+N))
faces += [tuple(reversed(range(N))), tuple((len(profile)-1)*N+j for j in range(N))]
mesh('Torso',verts,faces,'Fur',2)
ellipsoid('LightChest',(6.2,0,33),(2.5,6.3,12.5),'Chest')
for side in (-1,1):
    limb('Thigh',(-2,side*6,21),(-4,side*7.2,11),4.6,'Fur')
    limb('Shin',(-4,side*7.2,12),(2,side*7,5),2.7,'Fur')

# Join and voxel-remesh the head/muzzle for an organic continuous silhouette.
head = ellipsoid('Head',(2,0,53),(7.8,6.5,8.4),'Fur')
muzzle = ellipsoid('Muzzle',(10,0,50.8),(8.2,4.7,4.2),'Fur')
bpy.ops.object.select_all(action='DESELECT')
head.select_set(True); muzzle.select_set(True)
bpy.context.view_layer.objects.active = head
bpy.ops.object.join()
remesh = head.modifiers.new('Joined anatomy','REMESH')
remesh.mode = 'VOXEL'; remesh.voxel_size = .42
bpy.ops.object.modifier_apply(modifier=remesh.name)
smooth = head.modifiers.new('Soften','SMOOTH'); smooth.factor=.8; smooth.iterations=3
bpy.ops.object.modifier_apply(modifier=smooth.name)
for face in head.data.polygons: face.use_smooth=True
ellipsoid('MuzzleLight',(12,0,49.4),(5.4,4.35,2.7),'Chest')
ellipsoid('Nose',(18,0,51),(1.65,2,1.35),'Skin')
for side in (-1,1):
    # Top of ears is exactly 65 cm.
    ear=ellipsoid('Ear',(-1,side*6.1,60),(1.4,4.5,5),'Fur')
    ellipsoid('EarInner',(.18,side*6.1,60),( .45,3.7,4.25),'Skin')
    ellipsoid('EyeLid',(6.5,side*5.2,54.3),(2.55,1.3,1.7),'Chest')
    ellipsoid('Eye',(7.1,side*5.8,54.4),(1.6,.8,1.15),'Eye')
    tube('Brow',[(4.8,side*5.6,56),(6.5,side*6.0,56.2),(8.3,side*5.4,55.7)],.35,'Fur')
    tube('Mouth',[(18,0,49.5),(14,side*3,48.3),(10,side*4,48.5)],.07,'Fur')
    for i in range(4):
        tube('Whisker',[(13+i*.6,side*3.7,50),(15+i*.6,side*9,50.8-i*.7),(12+i*2,side*(16+i),52-i*1.4)],.028,'Whisker',1)

# Open jacket shell. The front opening exposes the light chest.
verts, faces = [], []
J = 44
jacket_rows = [(19,8.7,11.4,-1),(21,9.1,11.6,-1),(30,9.6,11.8,-.5),(39,9.0,11.3,0),(44,7.0,9.6,0),(46,5.5,7,0)]
for row,(z,rx,ry,cx) in enumerate(jacket_rows):
    for j in range(J):
        a = .66 + (2*math.pi-1.32)*j/(J-1)
        fold=.35*math.sin(a*9+row*.65)
        verts.append((cx+(rx+fold)*math.cos(a),(ry+fold)*math.sin(a),z+.35*math.sin(a*5)))
for i in range(len(jacket_rows)-1):
    for j in range(J-1):
        k=i*J+j
        faces.append((k,k+1,k+1+J,k+J))
jacket=mesh('OpenJacket',verts,faces,'Jacket',2)
solid=jacket.modifiers.new('Fabric thickness','SOLIDIFY'); solid.thickness=.35
for side in (-1,1):
    # Folded lapels, sleeves and visible cuffs.
    lapel=mesh('Lapel',[(6,side*4.1,46.5),(9,side*7,43),(10,side*5.5,35),(8.4,side*3.7,41)],[(0,1,2,3)],'Seam')
    sol=lapel.modifiers.new('Lapel thickness','SOLIDIFY'); sol.thickness=.35
    bevel=lapel.modifiers.new('Lapel edge','BEVEL'); bevel.width=.3; bevel.segments=3
    limb('Sleeve',(0,side*10,41),(-1,side*14,29),4.8,'Jacket')
    limb('SleeveLower',(-1,side*14,30),(3,side*14,22),4,'Jacket')
    limb('Cuff',(2.5,side*14,24),(3.8,side*14,21),4.15,'Seam')
    ellipsoid('Hand',(4.8,side*14,19.2),(3,2.7,3.6),'Skin')
    for finger in range(3):
        limb('Finger',(6,side*(12.4+finger*1.2),19),(7,side*(12.4+finger*1.2),16.5),.75,'Skin')
    tube('FrontSeam',[(7,side*5.3,21),(8.8,side*5.3,31),(8.2,side*4.5,40)],.15,'Seam')
    tube('Pocket',[(5.9,side*9.3,28),(7.2,side*8.7,25),(7.1,side*8.5,22)],.25,'Seam')
    for z in (23,29,35):
        ellipsoid('Fastener',(8,side*5.6,z),(.25,.45,.45),'Metal',16,8)
    tube('ShoulderSeam',[(-6,side*7,44),(0,side*10.4,43),(3,side*11.5,39)],.16,'Seam')
for z in (20,21.2):
    points=[]
    for j in range(28):
        a=.66+(2*math.pi-1.32)*j/27
        points.append((-1+9*math.cos(a),11.8*math.sin(a),z))
    tube('HemStitch',points,.10,'Seam',1)
for side in (-1,1):
    tube('BackSeam',[(-8.7,side*5,22),(-9.3,side*5,33),(-7,side*4,42)],.14,'Seam')

# Tapered, curved tail with subtle ring anatomy.
points=[Vector(p) for p in [(-6,0,17),(-14,1,9),(-23,3,5),(-34,5,3),(-44,9,2),(-51,13,2.5)]]
verts,faces=[],[]
steps=45; sides=10
for i in range(steps):
    t=i/(steps-1)*(len(points)-1); k=min(int(t),len(points)-2); f=t-k
    center=points[k].lerp(points[k+1],f)
    tangent=(points[k+1]-points[k]).normalized()
    u=tangent.cross(Vector((0,0,1))).normalized(); v=tangent.cross(u)
    radius=(1.65*(1-i/(steps-1))+.13)*(1+.045*math.cos(i*math.pi))
    for j in range(sides):
        a=2*math.pi*j/sides
        verts.append(center+radius*(u*math.cos(a)+v*math.sin(a)))
for i in range(steps-1):
    for j in range(sides):
        k=i*sides+j; n=i*sides+(j+1)%sides
        faces.append((k,n,n+sides,k+sides))
mesh('Tail',verts,faces,'Skin',1)

# Sparse geometric fur tips break the cheek silhouette; no expensive hair groom.
rng=random.Random(91)
for side in (-1,1):
    for i in range(24):
        x=rng.uniform(-3,5); z=rng.uniform(48,57); y=side*rng.uniform(5,6.4)
        mesh('CheekFur',[(x,y,z),(x+.7,y,z+.9),(x-1.4,y+side*.75,z-.8)],[(0,1,2)],'Fur')

def combine(objects, name):
    if name == 'SM_ChuckBody':
        # Preserve authored part membership as weights before joining the study.
        # Curves/modifiers must be evaluated first so every exported vertex is weighted.
        for part in objects:
            label = part.name.split('.')[0]
            bpy.ops.object.select_all(action='DESELECT')
            part.select_set(True)
            bpy.context.view_layer.objects.active = part
            bpy.ops.object.convert(target='MESH')
            part = bpy.context.object
            center = sum((part.matrix_world @ v.co for v in part.data.vertices), Vector()) / len(part.data.vertices)
            # FBX -> Unreal reflects Y. Name sides by their runtime coordinates.
            side = 'L' if center.y > 0 else 'R'
            bone = 'root'
            if label in ('Thigh', 'Shin'):
                bone = ('thigh_' if label == 'Thigh' else 'shin_') + side
            elif label in ('Sleeve','SleeveLower','Cuff','Hand','Finger'):
                bone = ('arm_' if label == 'Sleeve' else 'forearm_') + side
            elif label in ('Head','MuzzleLight','Nose','Ear','EarInner','EyeLid','Eye','Brow','Mouth','Whisker','CheekFur'):
                bone = 'head'
            if label == 'Tail':
                groups = [part.vertex_groups.new(name=f'tail_{i}') for i in range(4)]
                for vertex in part.data.vertices:
                    x = (part.matrix_world @ vertex.co).x
                    t = max(0., min(3., (-x-6)/12.))
                    lo = min(2,int(t)); fraction = t-lo
                    groups[lo].add([vertex.index],1-fraction,'REPLACE')
                    groups[lo+1].add([vertex.index],fraction,'REPLACE')
            else:
                part.vertex_groups.new(name=bone).add(list(range(len(part.data.vertices))),1.,'REPLACE')
    bpy.ops.object.select_all(action='DESELECT')
    for obj in objects:
        obj.select_set(True)
    bpy.context.view_layer.objects.active=objects[0]
    bpy.ops.object.convert(target='MESH')
    bpy.ops.object.join()
    obj=bpy.context.object; obj.name=name
    scene.cursor.location=(0,0,0)
    bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    # FBX stores fully applied transforms and a 1 cm scene unit.
    bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
    colors=obj.data.color_attributes.new(name='Color',type='BYTE_COLOR',domain='CORNER')
    for polygon in obj.data.polygons:
        material=obj.data.materials[polygon.material_index]
        base=material.diffuse_color
        for loop_id in polygon.loop_indices:
            p=obj.data.vertices[obj.data.loops[loop_id].vertex_index].co
            variation=1+.12*noise(p*.9)+.05*noise(p*7)
            colors.data[loop_id].color=tuple(min(1,c*variation) for c in base[:3])+(1,)
    return obj

body=combine(list(scene.objects),'SM_ChuckBody')
footparts=[]
footparts.append(ellipsoid('Foot',(1,0,0),(6.5,3.1,2),'Skin'))
for toe in range(4):
    y=(toe-1.5)*1.4
    footparts.append(limb('Toe',(4,y,.1),(8,y,-.4),.7,'Skin'))
    footparts.append(limb('Claw',(7.5,y,-.3),(9,y,-.7),.35,'Claw'))
foot=combine(footparts,'SM_ChuckFoot')
# Only body and one reusable foot are stored/exported; rest placement is in C++.
for obj in (body,foot):
    bpy.ops.object.select_all(action='DESELECT'); obj.select_set(True)
    bpy.context.view_layer.objects.active=obj
    bpy.ops.export_scene.fbx(filepath=str(OUT/(obj.name+'.fbx')),use_selection=True,
        object_types={'MESH'},apply_unit_scale=True,axis_forward='-Y',axis_up='Z',
        bake_anim=False,add_leaf_bones=False,mesh_smooth_type='FACE')
# Keep static exports as the original form study; a separate deformable body is
# driven by a small runtime rig. Feet remain independent contact targets.
rig_data = bpy.data.armatures.new('ChuckRig')
rig = bpy.data.objects.new('ChuckRig',rig_data)
bpy.context.collection.objects.link(rig)
bpy.ops.object.select_all(action='DESELECT'); rig.select_set(True)
bpy.context.view_layer.objects.active=rig
bpy.ops.object.mode_set(mode='EDIT')
spec = [('root',(0,0,0),None),('head',(0,0,46),'root')]
for side, sign in [('L',1),('R',-1)]:
    spec += [(f'thigh_{side}',(-2,sign*6,21),'root'),
             (f'shin_{side}',(-4,sign*7.2,11),f'thigh_{side}'),
             (f'arm_{side}',(0,sign*10,41),'root'),
             (f'forearm_{side}',(-1,sign*14,29),f'arm_{side}')]
for i, point in enumerate([(-6,0,17),(-18,2,7),(-30,4,3.7),(-42,8,2.2)]):
    spec.append((f'tail_{i}',point,'root' if i == 0 else f'tail_{i-1}'))
for name, head_pos, parent in spec:
    bone=rig_data.edit_bones.new(name)
    bone.head=head_pos; bone.tail=Vector(head_pos)+Vector((0,0,5))
    if parent: bone.parent=rig_data.edit_bones[parent]
bpy.ops.object.mode_set(mode='OBJECT')
skinned=body.copy(); skinned.data=body.data.copy(); skinned.name='SK_ChuckBody'
bpy.context.collection.objects.link(skinned)
modifier=skinned.modifiers.new('Chuck skin','ARMATURE'); modifier.object=rig
skinned.parent=rig
assert all(abs(sum(g.weight for g in v.groups)-1.) < .001 for v in skinned.data.vertices), 'Unweighted body vertices'
bpy.ops.object.select_all(action='DESELECT'); rig.select_set(True); skinned.select_set(True)
bpy.context.view_layer.objects.active=rig
bpy.ops.export_scene.fbx(filepath=str(OUT/'SK_ChuckBody.fbx'),use_selection=True,
    object_types={'MESH','ARMATURE'},apply_unit_scale=True,axis_forward='-Y',axis_up='Z',
    bake_anim=False,add_leaf_bones=False,mesh_smooth_type='FACE')
body.hide_set(True); body.hide_render=True
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'Chuck.blend'))
print('CHUCK_MODEL_READY',sum(len(obj.data.polygons) for obj in (body,foot)))
