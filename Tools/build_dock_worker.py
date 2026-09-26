"""Original stationary 180 cm dock worker, Blender 4.5.14. No NPC system."""
from pathlib import Path
import math
import bpy
from mathutils import Vector

OUT=Path(__file__).resolve().parents[1]/'SourceAssets'/'Docks'
bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete(use_global=False)
scene=bpy.context.scene
scene.unit_settings.system='METRIC'; scene.unit_settings.scale_length=.01
materials={}
for name,color in [('Navy',(.035,.067,.085,1)),('Canvas',(.29,.255,.19,1)),('Leather',(.075,.035,.018,1)),('Skin',(.32,.145,.12,1)),('Eye',(.009,.006,.004,1)),('Metal',(.22,.14,.048,1))]:
    mat=bpy.data.materials.new(name); mat.diffuse_color=color; materials[name]=mat
parts=[]
def finish(obj,name,material):
    obj.name=name; obj.data.materials.append(materials[material]); parts.append(obj); return obj

def mesh(name,verts,faces,material):
    data=bpy.data.meshes.new(name); data.from_pydata(verts,[],faces); data.update()
    obj=bpy.data.objects.new(name,data); bpy.context.collection.objects.link(obj)
    for p in data.polygons: p.use_smooth=True
    return finish(obj,name,material)

def ellipsoid(name,loc,size,material):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=24,ring_count=16,radius=1,location=loc)
    obj=bpy.context.object; obj.scale=size
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    for p in obj.data.polygons: p.use_smooth=True
    return finish(obj,name,material)

def profile(name,rows,material,fold=0):
    # Rows are center X,Y,Z and depth/width radii; front is +X.
    verts=[]; count=32
    for r,(x,y,z,rx,ry) in enumerate(rows):
        for j in range(count):
            a=j*math.tau/count
            wrinkle=1+fold*math.sin(r*2.3+a*3)*math.sin(a*2+r*.7)
            verts.append((x+rx*math.cos(a)*wrinkle,y+ry*math.sin(a)*wrinkle,z))
    faces=[]
    for row in range(len(rows)-1):
        for j in range(count): faces.append((row*count+j,row*count+(j+1)%count,(row+1)*count+(j+1)%count,(row+1)*count+j))
    faces.extend([tuple(reversed(range(count))),tuple((len(rows)-1)*count+j for j in range(count))])
    obj=mesh(name,verts,faces,material)
    mod=obj.modifiers.new('Cloth continuity','SUBSURF'); mod.levels=2
    return obj

def tube(name,points,radius,material):
    curve=bpy.data.curves.new(name,'CURVE'); curve.dimensions='3D'; curve.bevel_depth=radius; curve.bevel_resolution=1
    spline=curve.splines.new('POLY'); spline.points.add(len(points)-1)
    for p,co in zip(spline.points,points): p.co=(*co,1)
    obj=bpy.data.objects.new(name,curve); bpy.context.collection.objects.link(obj)
    return finish(obj,name,material)

# Grounded rounded boots, trousers with knees at about 53 cm.
for side in (-1,1):
    y=side*11
    profile('Boot sole',[(4,y,0,13,8.5),(4,y,.3,13,8.5),(4,y,2.3,13,8.5),(4,y,2.6,13,8.5)],'Leather')
    profile('Boot upper',[(4,y,2,12.5,8),(5,y,5,12.5,8),(4,y,8,11,7.6),(1,y,11,7.8,7),(0,y,18,6.8,6.7),(0,y,21,6.8,6.7)],'Leather',.015)
    for z in (10,13,16): tube('Boot lace',[(7.2,y-3,z),(7.7,y+3,z)],.22,'Canvas')
    profile('Trouser leg',[(0,y,18,6.4,6.2),(0,y,20,6.4,6.2),(-.5,y,26,7.6,7.2),(-1,y,35,7.4,7.5),(1,y,47,7.2,7.8),(2,y,54,8.1,8.2),(0,y,60,8.7,8.6),(-1,y,75,9.5,9.5),(0,y,88,10.2,10.2),(0,y,94,10.2,10.2)],'Navy',.045)
profile('Trouser seat',[(0,0,83,8,17),(0,0,88,11.5,19),(0,0,99,11,18),(0,0,101,11,18)],'Navy',.02)
profile('Work shirt',[(0,0,97,11.8,19),(0,0,99,11.8,19),(0,0,108,12.5,19.5),(0,0,119,13.5,21),(0,0,134,12.4,23),(0,0,142,10.5,23),(0,0,146,8,19),(0,0,148,6,8)],'Canvas',.025)
profile('Belt',[(0,0,99,12,19.3),(0,0,99.5,12,19.3),(0,0,103,12,19.3),(0,0,103.5,12,19.3)],'Leather')
tube('Buckle',[(12.6,-2.8,99.7),(12.6,2.8,99.7),(12.6,2.8,103),(12.6,-2.8,103),(12.6,-2.8,99.7)],.45,'Metal')
for side in (-1,1):
    # Narrow vest fronts leave a strip of shirt showing down the centre.
    verts=[]
    for z,depth,width in [(103,12.2,18.5),(116,13.8,20),(132,13,21.5),(142,11,19.5)]:
        for fraction in (.16,.55,.89):
            yy=width*fraction*side
            verts.append((depth*math.sqrt(1-fraction*fraction)+.7,yy,z))
    faces=[(r*3+j,r*3+j+1,(r+1)*3+j+1,(r+1)*3+j) for r in range(3) for j in range(2)]
    obj=mesh('Vest front',verts,faces,'Navy')
    mod=obj.modifiers.new('Vest thickness','SOLIDIFY'); mod.thickness=.7
    mod=obj.modifiers.new('Soft vest','SUBSURF'); mod.levels=2
    profile('Shirt sleeve',[(1,side*29,92,5.7,5.7),(1,side*29,94,5.7,5.7),(0,side*29,104,6.8,6.5),(-1,side*28,116,7.5,7),(-1,side*26,128,8.5,8),(0,side*22,140,9,8.5),(0,side*20,143,8,8)],'Canvas',.055)
    profile('Rolled cuff',[(1,side*29,91,6,6),(1,side*29,92,6.5,6.5),(1,side*29,96,6.5,6.5),(1,side*29,97,6,6)],'Canvas',.025)
    ellipsoid('Wrist',(1,side*29,89),(3.6,3.7,5),'Skin')
    ellipsoid('Hand',(2,side*29,81.8),(3.5,4.1,6.3),'Skin')
    for i in range(4): ellipsoid('Resting finger',(3.3,side*(26.5+i*1.6),77.5),(2,1.1,3),'Skin')
    ellipsoid('Thumb',(5,side*25.5,83),(2.2,1.8,3.5),'Skin')
for z in (109,118,127,136): ellipsoid('Shirt button',(14,0,z),(.65,.7,.7),'Leather')
profile('Neck',[(0,0,143,5.6,5.6),(0,0,146,5.6,5.6),(0,0,153,5.2,5.2),(0,0,155,5.2,5.2)],'Skin')
for side in (-1,1):
    obj=mesh('Shirt collar',[(6.5,side*1.2,146),(4,side*6,151),(1,side*8,145),(10,side*5,140)],[(0,1,2,3)],'Canvas')
    mod=obj.modifiers.new('Collar thickness','SOLIDIFY'); mod.thickness=.5
profile('Head',[(1,0,151,3.2,4),(1,0,153,6.5,6.3),(0,0,159,9,8.2),(-.2,0,166,9.4,8.8),(-1,0,173,9,8.5),(-1.7,0,176,7,7),(-1.7,0,178,3.5,4)],'Skin')
for side in (-1,1):
    ellipsoid('Ear',(-1,side*8.7,164.5),(2.3,1.5,4.1),'Skin')
    ellipsoid('Brow',(8.1,side*4,169.7),(1.1,2.5,.7),'Leather')
    ellipsoid('Eye',(8.6,side*4,168.2),(.7,1.5,.65),'Eye')
ellipsoid('Nose bridge',(9,0,164.8),(2.5,1.5,3.9),'Skin')
ellipsoid('Nose tip',(11,0,162.5),(2.1,1.9,1.5),'Skin')
tube('Closed mouth',[(8.6,-2.6,157.7),(9.3,0,157.3),(8.6,2.6,157.7)],.18,'Leather')
profile('Wool cap',[(-1.7,0,173,9.1,8.7),(-1.7,0,173.4,9.1,8.7),(-1.7,0,177,9,8.6),(-1.7,0,179,6.5,6),(-1.7,0,180,2,2)],'Navy')
ellipsoid('Cap peak',(7,0,174),(6.6,8.2,.65),'Navy')

bpy.ops.object.select_all(action='DESELECT')
for part in parts: part.select_set(True)
bpy.context.view_layer.objects.active=parts[0]
bpy.ops.object.convert(target='MESH'); bpy.ops.object.join()
obj=bpy.context.object; obj.name='SM_DockWorker'
scene.cursor.location=(0,0,0); bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
# Normalize to exactly 180 cm after subdivision, with both soles at ground level.
zs=[v.co.z for v in obj.data.vertices]; low=min(zs); factor=180/(max(zs)-low)
for v in obj.data.vertices: v.co.z=(v.co.z-low)*factor
bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT')
bpy.ops.mesh.normals_make_consistent(inside=False); bpy.ops.object.mode_set(mode='OBJECT')
bpy.ops.export_scene.fbx(filepath=str(OUT/'SM_DockWorker.fbx'),use_selection=True,object_types={'MESH'},apply_unit_scale=True,axis_forward='-Y',axis_up='Z',bake_anim=False,mesh_smooth_type='FACE')
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'DockWorker.blend'))
print('CHUCK_DOCK_WORKER_READY',len(obj.data.polygons))
