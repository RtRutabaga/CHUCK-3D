"""Blender 4.5 LTS: reproducible character form study, authored in centimetres.

Run blender --background --python Tools/build_chuck_model.py.
Only creates this project's named source assets; no external models/textures.
"""
from pathlib import Path
import math
import random
import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree
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

def cloth_sleeve(name,shoulder,elbow,wrist,start_radius,end_radius,mat):
    """One continuous tapered sleeve bending through the elbow, with shallow
    gathered folds. A single tube avoids the pinch of two overlapping tubes."""
    shoulder,elbow,wrist=Vector(shoulder),Vector(elbow),Vector(wrist)
    upper=(elbow-shoulder).length; lower=(wrist-elbow).length; total=upper+lower
    d_up=(elbow-shoulder).normalized(); d_lo=(wrist-elbow).normalized()
    verts,faces=[],[]
    rings,segments=22,32
    reference=Vector((1,0,0))
    for row in range(rings):
        s=(row/(rings-1))*1.04*total-.04*upper  # tuck the top into the shoulder
        center=shoulder+d_up*s if s<=upper else elbow+d_lo*(s-upper)
        blend=max(0.,min(1.,(s-upper+2.)/4.))  # rotate ring frames over +/-2 cm
        direction=(d_up*(1-blend)+d_lo*blend).normalized()
        u=direction.cross(reference).normalized(); v=direction.cross(u)
        t=s/total
        radius=(start_radius*(1-t)+end_radius*t)*(.92+.08*math.sin(max(0.,t)*math.pi))
        for j in range(segments):
            angle=j*math.tau/segments
            fold=.11*math.sin(t*math.pi*9+angle*2)+.06*math.sin(angle*5+t*3)
            verts.append(center+(radius+fold)*(u*math.cos(angle)+v*math.sin(angle)))
    for row in range(rings-1):
        for j in range(segments):
            k=row*segments+j; nxt=row*segments+(j+1)%segments
            faces.append((k,nxt,nxt+segments,k+segments))
    faces.extend([tuple(reversed(range(segments))),tuple((rings-1)*segments+j for j in range(segments))])
    return mesh(name,verts,faces,mat,1)

# Continuous tapered trunk rather than overlapping spherical jacket pieces.
verts, faces = [], []
profile = [(12,4,6,-2),(18,7,8.8,-2),(25,8.2,9.2,-1),(34,8,8.8,-.5),(42,6,7.8,0),(47,4,5,1)]
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
torso=mesh('Torso',verts,faces,'Fur',2)

def conform_patch(name,source,material,a_half,z_lo,z_hi,rise,sink,cols=24,rows=28):
    """Light fur bib that follows the evaluated torso surface: raised at its
    centre, sunk below the surface at its border so no rim or gap shows."""
    bpy.context.view_layer.update()
    tree=BVHTree.FromObject(source,bpy.context.evaluated_depsgraph_get())
    verts,faces=[],[]
    for r in range(rows):
        v=2*r/(rows-1)-1
        z=z_lo+(z_hi-z_lo)*(v+1)/2
        width=a_half*(1-v*v)**.35
        for c in range(cols):
            u=2*c/(cols-1)-1
            a=u*width
            origin=Vector((0,0,z)); direction=Vector((math.cos(a),math.sin(a),0))
            hit,normal,_,_=tree.ray_cast(origin,direction,40)
            falloff=(1-u**4)*(1-v**4)**.5
            verts.append(hit+normal*(rise*falloff-sink*(1-falloff)))
    for r in range(rows-1):
        for c in range(cols-1):
            k=r*cols+c
            faces.append((k,k+1,k+1+cols,k+cols))
    return mesh(name,verts,faces,material,1)

conform_patch('LightChest',torso,'Chest',1.3,15,45.8,.4,.25)
for side in (-1,1):
    limb('Thigh',(-2,side*6,21),(-4,side*7.2,11),4.6,'Fur')
    limb('Shin',(-4,side*7.2,12),(2,side*7,5),2.7,'Fur')

# Continuous tapered skull and muzzle. The earlier joined spheres made a blunt,
# round face; these anatomical sections narrow toward a smaller nasal pad.
verts,faces=[],[]
head_sections=[(-6,54,3.2,4.5),(-2,54,5.8,7.2),(2,53.6,6.4,7.3),
               (6,52.8,5.3,5.7),(10,51.6,4.1,3.6),(15,51,2.2,2),(17.5,51.2,1,1)]
for x,z,ry,rz in head_sections:
    for j in range(32):
        angle=j*math.tau/32
        verts.append((x,ry*math.cos(angle),z+rz*math.sin(angle)))
for row in range(len(head_sections)-1):
    for j in range(32):
        k=row*32+j; nxt=row*32+(j+1)%32
        faces.append((k,nxt,nxt+32,k+32))
faces += [tuple(reversed(range(32))),tuple((len(head_sections)-1)*32+j for j in range(32))]
head=mesh('Head',verts,faces,'Fur',2)
ellipsoid('MuzzleLight',(12,0,49.6),(4.3,3.1,1.8),'Chest')
ellipsoid('Nose',(17.8,0,51.2),(1.1,1.3,.9),'Skin')
for side in (-1,1):
    # Top of ears is exactly 65 cm.
    ear=ellipsoid('Ear',(-1,side*6.1,60.8),(1.1,3.7,4.2),'Fur')
    ear.rotation_euler.z=side*math.radians(18)
    inner=ellipsoid('EarInner',(-.1,side*6.4,60.8),(.4,3.1,3.5),'Skin')
    inner.rotation_euler.z=ear.rotation_euler.z
    ellipsoid('EyeLid',(6.5,side*5.2,54.4),(2,.85,1.5),'Fur')
    ellipsoid('Eye',(7.1,side*5.45,54.6),(1.45,.75,1),'Eye')
    tube('Brow',[(4.8,side*5.6,56),(6.5,side*6.0,56.2),(8.3,side*5.4,55.7)],.35,'Fur')
    tube('Mouth',[(16.8,0,50.1),(14,side*1.7,49.2),(10,side*3.1,48.8)],.05,'Fur')
    for i in range(4):
        tube('Whisker',[(13+i*.6,side*3.7,50),(15+i*.6,side*9,50.8-i*.7),(12+i*2,side*(16+i),52-i*1.4)],.028,'Whisker',1)

# Open jacket: one continuous garment surface. Every body row, the collar stand,
# the fold and the collar/lapel fall share one grid and one front-edge function,
# so plackets, lapels and stitching cannot drift away from the shell edge.
# Solidify gives real cloth thickness; its rim closes every boundary and the
# inner shell carries the lining (Seam slot). The chest opening stays open.
JACKET_PROFILE=[(18.5,8.9,10.7,-1),(21,9.1,10.6,-1),(30,9.6,10.4,-.5),(38,9.2,10.3,0),
                (42,8.0,9.9,0),(44.5,6.6,8.8,0),(46.5,5.4,7.2,0),(47.8,5.1,6.7,-.2)]
JACKET_THICKNESS=.4

def jacket_radii(z):
    rows=JACKET_PROFILE
    if z<=rows[0][0]: return rows[0][1:]
    for lo,hi in zip(rows,rows[1:]):
        if z<=hi[0]:
            f=(z-lo[0])/(hi[0]-lo[0])
            return tuple(a*(1-f)+b*f for a,b in zip(lo[1:],hi[1:]))
    return rows[-1][1:]

def jacket_edge_angle(z):
    """Half-angle of the open front, measured from +X; widens into the collar."""
    f=max(0.,min(1.,(z-34)/13.))
    return .58+.47*f*f

def jacket_point(t,z,lift=0.,fold=0.):
    """t runs 0..1 from the +Y front edge around the back to the -Y front edge."""
    edge=jacket_edge_angle(z)
    a=edge+(2*math.pi-2*edge)*t
    rx,ry,cx=jacket_radii(z)
    return Vector((cx+(rx+lift+fold)*math.cos(a),(ry+lift+fold)*math.sin(a),z))

def collar_bottom(t):
    # Collar sits at 44.6 cm behind the neck; near each front edge the fall
    # continues down the chest as the rolled lapel.
    e=min(t,1-t)
    return 44.6-7.4*max(0.,1-e/.1)**1.5

J=56
rows=[]  # (kind, per-column (z, lift, cloth fold amplitude))
body_z=[18.5+i*1.15 for i in range(26)]+[48.3]
for z in body_z:
    rows.append(('body',[(z,0.,.22*max(0.,min(1.,(46-z)/6))) for _ in range(J)]))
rows.append(('fall',[(48.8,.45,0.) for _ in range(J)]))
FALL=8
for i in range(1,FALL+1):
    s=i/FALL
    rows.append(('fall',[(48.8+(collar_bottom(j/(J-1))-48.8)*s,.6+1.1*s,0.) for j in range(J)]))
verts,faces,face_kind=[],[],[]
for r,(kind,cols) in enumerate(rows):
    for j,(z,lift,amp) in enumerate(cols):
        t=j/(J-1)
        edge_calm=min(1.,min(t,1-t)/.06)  # keep the placket edge straight
        fold=amp*edge_calm*math.sin(t*math.tau*4.5+z*.55)
        verts.append(jacket_point(t,z,lift,fold))
for r in range(len(rows)-1):
    for j in range(J-1):
        k=r*J+j
        faces.append((k,k+1,k+1+J,k+J)); face_kind.append(rows[r+1][0])
jacket=mesh('OpenJacket',verts,faces,'Jacket',1)
jacket.data.materials.append(MATS['Seam']); jacket.data.materials.append(MATS['Jacket'])
for face,kind in zip(jacket.data.polygons,face_kind):
    # Body faces: outer 0 (Jacket), solidified inner 1 (lining). The collar
    # fall is folded over, so its visible face is the solidified one.
    face.material_index=0 if kind=='body' else 1
solid=jacket.modifiers.new('Fabric thickness','SOLIDIFY')
solid.thickness=JACKET_THICKNESS; solid.offset=-1; solid.use_even_offset=True
solid.use_rim=True; solid.material_offset=1; solid.material_offset_rim=0

def box_mesh(name,items,mat):
    """items: (center, axis_u, axis_v, axis_w, half-sizes) boxes in one mesh."""
    verts,faces=[],[]
    for c,u,v,w,(hu,hv,hw) in items:
        base=len(verts)
        for su in (-1,1):
            for sv in (-1,1):
                for sw in (-1,1):
                    verts.append(c+u*hu*su+v*hv*sv+w*hw*sw)
        faces+=[(base+q[0],base+q[1],base+q[2],base+q[3]) for q in
                ((0,1,3,2),(4,6,7,5),(0,4,5,1),(2,3,7,6),(0,2,6,4),(1,5,7,3))]
    return mesh(name,verts,faces,mat)

teeth=[]
for side_t in (.004,.996):
    for i in range(34):
        z=19.2+i*.5
        c=jacket_point(side_t,z,.16)
        n=Vector((c.x,c.y,0)).normalized(); w=Vector((0,0,1)); u=w.cross(n)
        c=c+u*(.12 if i%2 else -.12)
        teeth.append((c,n,u,w,(.12,.26,.13)))
    tube('ZipperTape',[jacket_point(side_t,z,.05) for z in (19,24,29,34,36.2)],.2,'Seam',1)
box_mesh('Zipper',teeth,'Metal')
for side in (-1,1):
    cloth_sleeve('Sleeve',(0,side*10.4,41),(-1,side*14,29),(3,side*14,22),4.4,3.4,'Jacket')
    limb('Cuff',(2.5,side*14,24),(3.8,side*14,21),3.75,'Seam')
    ellipsoid('Hand',(4.8,side*14,19.2),(3,2.7,3.6),'Skin')
    for finger in range(3):
        limb('Finger',(6,side*(12.4+finger*1.2),19),(7,side*(12.4+finger*1.2),16.5),.75,'Skin')
for t0 in (.075,.925):
    # Slanted welt pocket stitched on the shell surface.
    d=-1 if t0<.5 else 1
    tube('Pocket',[jacket_point(t0,28,.08),jacket_point(t0+d*.02,25.5,.08),jacket_point(t0+d*.035,23,.08)],.22,'Seam')
for z in (19.4,20.4):
    tube('HemStitch',[jacket_point(i/40*.99+.005,z,.06) for i in range(41)],.09,'Seam',1)
for t0 in (.41,.59):
    tube('BackSeam',[jacket_point(t0,z,.06) for z in (19.6,27,35,42.5)],.12,'Seam')

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

# Short directional geometric tufts. Sample evaluated surface area so modifiers,
# limb rotation and nonuniform authored topology do not bias coverage.
def fur_surface(source,name,material,count,seed,lengths,groom,accept):
    rng=random.Random(seed)
    bpy.context.view_layer.update()
    evaluated=source.evaluated_get(bpy.context.evaluated_depsgraph_get())
    surface=evaluated.to_mesh(); surface.calc_loop_triangles()
    triangles=list(surface.loop_triangles)
    transform=source.matrix_world
    normals=transform.to_3x3().inverted().transposed()
    verts,faces=[],[]
    for tri in rng.choices(triangles,weights=[t.area for t in triangles],k=count):
        a,b,c=[surface.vertices[i] for i in tri.vertices]
        u=rng.random(); v=rng.random()
        if u+v>1: u,v=1-u,1-v
        point=transform @ (a.co*(1-u-v)+b.co*u+c.co*v)
        normal=(normals @ (a.normal*(1-u-v)+b.normal*u+c.normal*v)).normalized()
        if not accept(point,normal): continue
        tangent=normal.cross(Vector((0,0,1)))
        if tangent.length<.01: tangent=normal.cross(Vector((0,1,0)))
        tangent.normalize(); bitangent=normal.cross(tangent).normalized()
        root=point-normal*.035
        length=rng.uniform(*lengths)
        # Shorter, narrower facial hairs preserve the muzzle rather than obscuring it.
        if name=='CheekFur' and point.x>6: length*=.45
        width=rng.uniform(.045,.095)
        direction=Vector(groom); direction-=normal*direction.dot(normal)
        tip=point+normal*length+direction*length
        start=len(verts)
        for j in range(3):
            angle=j*math.tau/3
            verts.append(root+width*(tangent*math.cos(angle)+bitangent*math.sin(angle)))
        verts.append(tip)
        faces += [(start+j,start+(j+1)%3,start+3) for j in range(3)]
    mesh(name,verts,faces,material)
    evaluated.to_mesh_clear()

def face_fur(point,normal):
    if point.x>15.3 or point.z<48: return False
    # Bare eyelids, nose and mouth stay legible; no exaggerated furry eyebrows.
    for side in (-1,1):
        delta=point-Vector((7.1,side*5.45,54.6))
        if (delta.x/2.4)**2+(delta.y/1.8)**2+(delta.z/1.8)**2<1: return False
    return True

fur_surface(head,'CheekFur','Fur',6500,91,(.28,.72),(-.6,0,-.2),face_fur)
chest=bpy.data.objects['LightChest']
fur_surface(chest,'ChestFur','Chest',4600,93,(.28,.68),(0,0,-.85),lambda p,n:n.x>.2)
fur_surface(torso,'BellyFur','Fur',2600,97,(.3,.7),(0,0,-.7),lambda p,n:p.z<21 and p.x>0)
for i,part in enumerate([o for o in list(scene.objects) if o.name.split('.')[0] in ('Thigh','Shin')]):
    label=part.name.split('.')[0]
    fur_surface(part,label+'Fur','Fur',1800,101+i,(.22,.55),(0,0,-.8),lambda p,n:p.z<22)

# Graded garment weights. Shell, sleeves and the details stitched to the shell
# share one spatial field, so overlapping cloth follows the arm together instead
# of the rigid shell cutting through a rigidly weighted sleeve.
ARM_CHAIN={s:(Vector((0,y*10,41)),Vector((-1,y*14,29)),Vector((3,y*14,22))) for s,y in (('L',1),('R',-1))}
GARMENT=('OpenJacket','Zipper','ZipperTape','Pocket','HemStitch','BackSeam')
SLEEVE=('Sleeve','Cuff')

def smoothstep(x,lo,hi):
    f=max(0.,min(1.,(x-lo)/(hi-lo)))
    return f*f*(3-2*f)

def garment_weights(p,label):
    """{bone: weight} for a garment vertex in world space; totals are 1."""
    side='L' if p.y>0 else 'R'
    shoulder,elbow,wrist=ARM_CHAIN[side]
    upper=elbow-shoulder; length=upper.length; d=upper/length
    a=(p-shoulder).dot(d)
    # Upper arm owns cloth from just below the shoulder cap outward.
    along=smoothstep(a,-2.,2.5)
    if label in SLEEVE:
        chain=along
        e=smoothstep(a-length,-2.5,2.5)  # elbow blend on the sleeve only
    else:
        # Shell cloth only follows where it lies inside/next to the sleeve
        # volume of the upper arm; it never takes forearm weight.
        r=(p-(shoulder+d*max(0.,min(length,a)))).length
        # Only the armhole region follows; lower side panels stay with the body.
        chain=along*(1-smoothstep(r,3.,6.))*(1-smoothstep(a,4.,9.))
        e=0.
    weights={'root':1-chain,f'arm_{side}':chain*(1-e),f'forearm_{side}':chain*e}
    return {bone:w for bone,w in weights.items() if w>1e-4}

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
            if label in ('Thigh', 'Shin', 'ThighFur', 'ShinFur'):
                bone = ('thigh_' if label in ('Thigh','ThighFur') else 'shin_') + side
            elif label in ('Hand','Finger'):
                bone = 'forearm_' + side
            elif label in ('Head','MuzzleLight','Nose','Ear','EarInner','EyeLid','Eye','Brow','Mouth','Whisker','CheekFur'):
                bone = 'head'
            if label in GARMENT+SLEEVE:
                groups={}
                for vertex in part.data.vertices:
                    for bone,w in garment_weights(part.matrix_world @ vertex.co,label).items():
                        if bone not in groups: groups[bone]=part.vertex_groups.new(name=bone)
                        groups[bone].add([vertex.index],w,'REPLACE')
            elif label == 'Tail':
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
