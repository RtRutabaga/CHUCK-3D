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

def chain_tube(name,points,radius_at,mat,fold=0.,caps=(1.,1.),segments=32,rings=24,subdiv=1,squash=1.):
    """Continuous tube along a polyline (e.g. shoulder-elbow-wrist), with ring
    frames blended over +/-2 cm at each joint and rounded (domed) ends instead
    of flat caps. radius_at(t) takes t in 0..1 along the polyline. squash
    scales the tube's second cross-section axis (flattened feet/toes)."""
    points=[Vector(p) for p in points]
    lengths=[(b-a).length for a,b in zip(points,points[1:])]
    dirs=[(b-a).normalized() for a,b in zip(points,points[1:])]
    total=sum(lengths)
    reference=Vector((0,0,1)) if abs(dirs[0].z)<.9 else Vector((1,0,0))
    def frame(s):
        acc=0.
        for i,(length,d) in enumerate(zip(lengths,dirs)):
            if s<=acc+length or i==len(lengths)-1:
                center=points[i]+d*(s-acc)
                direction=d
                if i+1<len(dirs):
                    direction=(d*(1-max(0.,min(1.,(s-acc-length+2.)/4.)))+dirs[i+1]*max(0.,min(1.,(s-acc-length+2.)/4.))).normalized()
                if i>0:
                    w=max(0.,min(1.,(s-acc+2.)/4.))
                    direction=(dirs[i-1]*(1-w)+direction*w).normalized()
                return center,direction
            acc+=length
    cap0=radius_at(0.)*caps[0]; cap1=radius_at(1.)*caps[1]
    stations=[]
    for k in range(4,0,-1):  # start dome
        f=k/4.; stations.append((-cap0*f, math.sqrt(max(0.,1-f*f))*.98+.02, 0.))
    for r in range(rings):
        stations.append((total*r/(rings-1),1.,r/(rings-1)))
    for k in range(1,5):  # end dome
        f=k/4.; stations.append((total+cap1*f, math.sqrt(max(0.,1-f*f))*.98+.02, 1.))
    verts,faces=[],[]
    for s_along,scale,t in stations:
        center,direction=frame(min(max(s_along,0.),total))
        if s_along<0: center=points[0]+dirs[0]*s_along
        if s_along>total: center=points[-1]+dirs[-1]*(s_along-total)
        u=direction.cross(reference).normalized(); v=direction.cross(u)
        radius=radius_at(t)*scale
        for j in range(segments):
            angle=j*math.tau/segments
            wrinkle=fold*scale*(math.sin(t*math.pi*9+angle*2)+.55*math.sin(angle*5+t*3))
            verts.append(center+(radius+wrinkle)*(u*math.cos(angle)+v*math.sin(angle)*squash))
    n=len(stations)
    for row in range(n-1):
        for j in range(segments):
            k=row*segments+j; nxt=row*segments+(j+1)%segments
            faces.append((k,nxt,nxt+segments,k+segments))
    faces.extend([tuple(reversed(range(segments))),tuple((n-1)*segments+j for j in range(segments))])
    return mesh(name,verts,faces,mat,subdiv)

def smoothstep(x,lo,hi):
    f=max(0.,min(1.,(x-lo)/(hi-lo)))
    return f*f*(3-2*f)

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
    # One continuous haunch/thigh/shin per leg on the runtime IK chain
    # (hip -> knee -> ankle, ChuckCharacter::SolveLeg). The domed end tucks
    # into the paw's heel so the ankle join stays covered as the foot turns.
    def leg_radius(t):
        haunch=4.3-1.6*smoothstep(t,.08,.55)
        return haunch-.75*smoothstep(t,.6,1.)
    chain_tube('Leg',[(-2,side*6,21.5),(-4,side*7.2,11),(2,side*7,5),(2.6,side*7,3.6)],leg_radius,'Fur',caps=(.9,.6),rings=26)

# Continuous tapered skull and muzzle. The earlier joined spheres made a blunt,
# round face; these anatomical sections narrow toward a smaller nasal pad.
verts,faces=[],[]
# Turnaround 2026-09-27: narrow, pointed head (half-widths 0.72x the first
# study), about 8-9 cm across the cheeks seen from the front.
HEAD_NARROW=.72
# Face-proportion target (References/ArtDirection/Chuck-Face-Proportion-Target.png):
# a rat profile, with the forehead sloping almost straight from the ears into a
# short conical snout. CRANIUM lifts the skull only slightly (a strong dome read
# as a mouse); the section centre rises by half the growth, keeping the chin line.
CRANIUM={-2:1.08,2:1.08,6:1.05}
head_sections=[(x,z+rz*(CRANIUM.get(x,1.)-1)*.5,ry*HEAD_NARROW,rz*CRANIUM.get(x,1.)) for x,z,ry,rz in
               [(-6,54,3.2,4.5),(-2,54,5.8,7.2),(2,53.6,6.4,7.3),
                (6,52.8,5.4,5.8),(10,51.6,4.3,3.75),(15,50.9,2.4,2.15),(17.5,50.7,1.15,1.05)]]
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
bpy.context.view_layer.update()
HEAD_TREE=BVHTree.FromObject(head,bpy.context.evaluated_depsgraph_get())

def head_axis_z(x):
    for (x0,z0,_,_),(x1,z1,_,_) in zip(head_sections,head_sections[1:]):
        if x<=x1: return z0+(z1-z0)*max(0.,(x-x0)/(x1-x0))
    return head_sections[-1][1]

def head_surface(x,a,lift=0.):
    """Point on the evaluated head at snout station x, angle a around the
    snout axis (0 = straight down, positive toward +Y), plus lift along the
    surface normal."""
    origin=Vector((x,0,head_axis_z(x)))
    hit,normal,_,_=HEAD_TREE.ray_cast(origin,Vector((0,math.sin(a),-math.cos(a))),20)
    return hit+normal*lift

# Cream chin, lips and cheeks conform to the head (no separate lip shell);
# raised at the centre and sunk at the border so no rim shows.
verts,faces=[],[]
MU_ROWS,MU_COLS=22,20
for r in range(MU_ROWS):
    v=r/(MU_ROWS-1); x=4.2+v*12.1
    # Wraps up over the whisker pads toward the nose (snout/fur target).
    reach=1.6+.55*smoothstep(v,.5,.95)
    for c in range(MU_COLS):
        u=2*c/(MU_COLS-1)-1
        falloff=(1-u**4)*(1-(2*v-1)**6)
        verts.append(head_surface(x,u*reach,.28*falloff-.22*(1-falloff)))
for r in range(MU_ROWS-1):
    for c in range(MU_COLS-1):
        k=r*MU_COLS+c
        faces.append((k,k+1,k+1+MU_COLS,k+MU_COLS))
mesh('MuzzleLight',verts,faces,'Chest',1)
ellipsoid('Nose',(17.7,0,50.75),(1.05,1.2,.85),'Skin')

def cupped_ear(side):
    """Thin cupped ear: pinched base, pink inner face (Skin), furred back
    (Fur) via Solidify. Returned already converted to a mesh."""
    facing=Vector((.5,side*.84,.12)).normalized()
    up=Vector((0,0,1)); across=up.cross(facing).normalized(); up=facing.cross(across).normalized()
    # Square grid mapped onto a disc: no high-valence pole to dimple the cup.
    n=15
    verts=[]
    for r in range(n):
        for c in range(n):
            gx=2*c/(n-1)-1; gy=2*r/(n-1)-1
            dx=gx*math.sqrt(max(0.,1-gy*gy/2)); dy=gy*math.sqrt(max(0.,1-gx*gx/2))
            rr2=min(1.,dx*dx+dy*dy)
            width=4.0*(.62+.38*smoothstep(dy,-1.,.1))
            verts.append(across*(width*dx)+up*(4.3*dy)+facing*(1.3*rr2))
    faces=[(r*n+c,r*n+c+1,(r+1)*n+c+1,(r+1)*n+c) for r in range(n-1) for c in range(n-1)]
    # Wind faces so the base normal faces forward/outward (the pink side).
    probe=(verts[1]-verts[0]).cross(verts[n]-verts[0])
    if probe.dot(facing)<0: faces=[tuple(reversed(f)) for f in faces]
    center=Vector((-1.6,side*4.6,60.2))  # pinched base sinks into the skull
    ear=mesh('Ear',[center+v for v in verts],faces,'Skin',1)
    ear.data.materials.append(MATS['Fur'])
    sol=ear.modifiers.new('Ear thickness','SOLIDIFY')
    sol.thickness=.3; sol.offset=-1; sol.use_rim=True; sol.material_offset=0; sol.material_offset_rim=0
    bpy.ops.object.select_all(action='DESELECT'); ear.select_set(True)
    bpy.context.view_layer.objects.active=ear
    bpy.ops.object.convert(target='MESH')
    return bpy.context.object

ears=[cupped_ear(side) for side in (-1,1)]
top=max((e.matrix_world @ v.co).z for e in ears for v in e.data.vertices)
for e in ears:
    # Contract: ear top exactly 65 cm.
    for v in e.data.vertices: v.co.z+=65.-top
for side in (-1,1):
    # Small, high rat eyes (face-proportion target).
    ellipsoid('EyeLid',(6.7,side*3.6,54.9),(1.45,.62,1.1),'Fur')
    ellipsoid('Eye',(7.15,side*3.8,55.0),(1.15,.62,.82),'Eye')
    tube('Mouth',[head_surface(x,side*a,.3) for x,a in ((16.6,.35),(14.5,.8),(12,1.05),(9.8,1.15))],.05,'Fur')
    for i in range(7):
        # Rows on the whisker pad, fanning up/back to down/forward.
        root=(12.6+i*.45,side*(2.95-.17*i),51.0-i*.12)  # on the pad surface
        tube('Whisker',[root,(15+i*.5,side*(6.4-i*.15),51.6-i*.6),(14+i*1.4,side*(11+i*.3),52.6-i*1.1)],.028,'Whisker',1)

# Open jacket: one continuous garment surface. Every body row, the collar stand,
# the fold and the collar/lapel fall share one grid and one front-edge function,
# so plackets, lapels and stitching cannot drift away from the shell edge.
# Solidify gives real cloth thickness; its rim closes every boundary and the
# inner shell carries the lining (Seam slot). The chest opening stays open.
JACKET_PROFILE=[(23,9.3,10.8,-1),(25,9.3,10.7,-1),(30,9.6,10.4,-.5),(37,9.3,10.6,0),
                (41,8.6,11.4,0),(43,7.7,10.9,0),(44.5,6.6,8.8,0),(46.5,5.4,7.2,0),(47.8,5.1,6.7,-.2)]
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
    return 44.6-3.6*max(0.,1-abs(e-.075)/.075)**1.3

J=56
rows=[]  # (kind, per-column (z, lift, cloth fold amplitude))
body_z=[23+i*.975 for i in range(26)]+[48.3]
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
    for i in range(26):
        z=23.6+i*.5
        c=jacket_point(side_t,z,.16)
        n=Vector((c.x,c.y,0)).normalized(); w=Vector((0,0,1)); u=w.cross(n)
        c=c+u*(.12 if i%2 else -.12)
        teeth.append((c,n,u,w,(.12,.26,.13)))
    tube('ZipperTape',[jacket_point(side_t,z,.05) for z in (23.4,27,31,34,36.2)],.2,'Seam',1)
box_mesh('Zipper',teeth,'Metal')
for side in (-1,1):
    # Domed sleeve head sits under the dropped shoulder; no flat cap.
    chain_tube('Sleeve',[(0,side*10.6,40.5),(-1,side*14,29),(3,side*14,22)],
               lambda t:(3.9*(1-t)+3.2*t)*(.93+.07*math.sin(t*math.pi)),'Jacket',fold=.11,caps=(.55,.25))
    chain_tube('Cuff',[(2.25,side*14,24.4),(3.35,side*14,22.0)],lambda t:3.35,'Jacket',caps=(.12,.18),rings=4)
    # Slender palm, long relaxed fingers and a pale pointed claw on every digit
    # (2026-09-27 goal images). The thumb is its own part (skinned to thumb_*).
    ellipsoid('Hand',(4.3,side*14,19.8),(1.1,1.35,2.25),'Skin')
    for finger in range(4):
        y=side*(13.0+finger*.6)
        length=(3.3,3.9,3.7,2.9)[finger]
        base=Vector((4.9,y,18.2)); mid=base+Vector((.6,0,-length*.58)); tip=mid+Vector((-.25,0,-length*.42))
        chain_tube('Finger',[base,mid,tip],lambda t:.34-.13*t,'Skin',caps=(.5,1.),segments=8,rings=8)
        nail=(tip-mid).normalized()
        limb('FingerClaw',tip-nail*.05,tip+nail*.75,.1,'Claw')
    thumb=[Vector((4.4,side*15.2,19.4)),Vector((5.6,side*15.8,18.3)),Vector((6.5,side*15.6,17.1))]
    chain_tube('Thumb',thumb,lambda t:.36-.13*t,'Skin',caps=(.5,1.),segments=8,rings=8)
    nail=(thumb[2]-thumb[1]).normalized()
    limb('ThumbClaw',thumb[2]-nail*.05,thumb[2]+nail*.7,.1,'Claw')
for t0 in (.075,.925):
    # Slanted welt pocket stitched on the shell surface.
    d=-1 if t0<.5 else 1
    tube('Pocket',[jacket_point(t0,31,.08),jacket_point(t0+d*.02,28.8,.08),jacket_point(t0+d*.035,26.6,.08)],.22,'Seam')
for z in (23.4,24.4):
    tube('HemStitch',[jacket_point(i/40*.99+.005,z,.06) for i in range(41)],.09,'Seam',1)
for t0,t1 in ((.012,.2),(.8,.988)):
    tube('HemStitch',[jacket_point(t0+(t1-t0)*i/8,37.2-1.2*math.sin(math.pi*i/8),.06) for i in range(9)],.09,'Seam',1)
tube('BackSeam',[jacket_point(.3+.4*i/12,39.5+.8*math.sin(math.pi*i/12),.06) for i in range(13)],.1,'Seam',1)
for t0 in (.41,.59):
    tube('BackSeam',[jacket_point(t0,z,.06) for z in (23.6,29,35.5,42.5)],.12,'Seam')

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
    # The cream muzzle patch covers the lower snout and cheeks.
    if point.x>4 and point.z<head_axis_z(point.x)-.5: return False
    # Bare eyelids, nose and mouth stay legible; no exaggerated furry eyebrows.
    for side in (-1,1):
        delta=point-Vector((7.1,side*5.2,54.6))
        if (delta.x/2.4)**2+(delta.y/1.8)**2+(delta.z/1.8)**2<1: return False
    return True

fur_surface(head,'CheekFur','Fur',6500,91,(.28,.72),(-.6,0,-.2),face_fur)
chest=bpy.data.objects['LightChest']
fur_surface(chest,'ChestFur','Chest',4600,93,(.28,.68),(0,0,-.85),lambda p,n:n.x>.2)
fur_surface(torso,'BellyFur','Fur',2600,97,(.3,.7),(0,0,-.7),lambda p,n:p.z<21 and p.x>0)
for i,part in enumerate([o for o in list(scene.objects) if o.name.split('.')[0]=='Leg']):
    label=part.name.split('.')[0]
    fur_surface(part,label+'Fur','Fur',1800,101+i,(.22,.55),(0,0,-.8),lambda p,n:p.z<22)

# Graded garment weights. Shell, sleeves and the details stitched to the shell
# share one spatial field, so overlapping cloth follows the arm together instead
# of the rigid shell cutting through a rigidly weighted sleeve.
ARM_CHAIN={s:(Vector((0,y*10,41)),Vector((-1,y*14,29)),Vector((3,y*14,22))) for s,y in (('L',1),('R',-1))}
GARMENT=('OpenJacket','Zipper','ZipperTape','Pocket','HemStitch','BackSeam')
SLEEVE=('Sleeve','Cuff')
LEG=('Leg','LegFur')
# Runtime IK chain (ChuckCharacter::SolveLeg): hip, knee, rest ankle.
LEG_CHAIN={s:(Vector((-2,y*6,21)),Vector((-4,y*7.2,11)),Vector((2,y*7,5))) for s,y in (('L',1),('R',-1))}

def leg_weights(p):
    """Haunch blends from the body into the thigh; thigh blends into the
    shin across the knee. Totals are 1."""
    side='L' if p.y>0 else 'R'
    hip,knee,ankle=LEG_CHAIN[side]
    thigh=knee-hip; length=thigh.length
    a=(p-hip).dot(thigh/length)
    limb=smoothstep(a,-1.5,2.5)
    e=smoothstep(a-length,-2.,2.)
    weights={'root':1-limb,f'thigh_{side}':limb*(1-e),f'shin_{side}':limb*e}
    return {bone:w for bone,w in weights.items() if w>1e-4}

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
    if label not in SLEEVE:
        # The lower jacket lifts with each thigh instead of being pierced by
        # the haunch. Both thighs blend by distance so the centre back stays
        # continuous when the legs move in opposite directions.
        pull={}
        for s,(hip,knee,_) in LEG_CHAIN.items():
            axis=(knee-hip).normalized()
            r=(p-(hip+axis*max(0.,min((knee-hip).length,(p-hip).dot(axis))))).length
            pull[s]=.75*(1-smoothstep(r,5.,11.5))*(1-smoothstep(p.z,21.,28.))
        total=sum(pull.values())
        if total>.8:
            pull={s:w*.8/total for s,w in pull.items()}; total=.8
        weights={bone:w*(1-total) for bone,w in weights.items()}
        for s,w in pull.items(): weights[f'thigh_{s}']=weights.get(f'thigh_{s}',0.)+w
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
            if label in ('Hand','Finger','Thumb','FingerClaw','ThumbClaw'):
                bone = 'forearm_' + side
            elif label in ('Head','MuzzleLight','Nose','Ear','EarInner','EyeLid','Eye','Brow','Mouth','Whisker','CheekFur'):
                bone = 'head'
            if label in GARMENT+SLEEVE+LEG:
                groups={}
                for vertex in part.data.vertices:
                    co=part.matrix_world @ vertex.co
                    field=leg_weights(co) if label in LEG else garment_weights(co,label)
                    for bone,w in field.items():
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

def paw_parts(origin=(0,0,0)):
    """Long, narrow hind paw built around origin. Legacy runtime contract:
    origin 2.5 cm above ground, ankle at local (-2,0,2.5)
    (ChuckCharacter::SolveLeg), sole close to local z=-2. Also reused by the
    v1 rig builder (Tools/build_chuck_v1.py) at its own placement."""
    o=Vector(origin)
    parts=[chain_tube('Foot',[o+Vector(p) for p in ((-4.6,0,-.55),(-1,0,-.5),(4.6,0,-.85))],
        lambda t:1.45+.35*math.sin(t*math.pi),'Skin',caps=(.9,.6),rings=14,squash=.74)]
    parts.append(ellipsoid('Heel',o+Vector((-2.4,0,1.0)),(2.0,1.6,1.9),'Skin'))
    for toe,(y,reach) in enumerate([(-2.0,2.6),(-1.0,3.6),(0.,3.9),(1.0,3.6),(2.1,2.4)]):
        base=Vector((4.2 if abs(y)<1.5 else 3.4,y*.8,-.9)); tip=base+Vector((reach,y*.35,-.35))
        mid=(base+tip)*.5+Vector((0,0,.25))
        parts.append(chain_tube('Toe',[o+base,o+mid,o+tip],lambda t:.6-.2*t,'Skin',caps=(.5,.8),segments=8,rings=6))
        direction=(tip-mid).normalized()
        parts.append(limb('Claw',o+tip+direction*.25+Vector((0,0,-.15)),o+tip+direction*1.0+Vector((0,0,-.3)),.2,'Claw'))
    return parts

# Tools/build_chuck_v1.py runs this file with CHUCK_GEOMETRY_ONLY=True to reuse
# the authored geometry; the legacy exports below are then skipped entirely.
if not globals().get('CHUCK_GEOMETRY_ONLY'):
    body=combine(list(scene.objects),'SM_ChuckBody')
    footparts=paw_parts()
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
