"""Blender 4.5.14: original dock props, centimetres, origin at collision centre."""
from pathlib import Path
import math
import bpy
from mathutils import Vector

OUT=Path(__file__).resolve().parents[1]/'SourceAssets'/'Docks'
OUT.mkdir(parents=True,exist_ok=True)
bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete(use_global=False)
scene=bpy.context.scene
scene.unit_settings.system='METRIC'; scene.unit_settings.scale_length=.01
materials={}
for name,color in [('Wood',(.13,.068,.029,1)),('WoodLight',(.225,.135,.062,1)),('Dark',(.045,.052,.05,1))]:
    mat=bpy.data.materials.new(name); mat.diffuse_color=color; materials[name]=mat

def finish(obj,name,material):
    obj.name=name; obj.data.materials.append(materials[material]); return obj

def box(name,loc,size,material='Wood',bevel=.16):
    bpy.ops.mesh.primitive_cube_add(size=1,location=loc)
    obj=bpy.context.object; obj.scale=size
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    if bevel:
        mod=obj.modifiers.new('Worn edges','BEVEL'); mod.width=bevel; mod.segments=2
        mod=obj.modifiers.new('Face normals','WEIGHTED_NORMAL'); mod.keep_sharp=True
    return finish(obj,name,material)

def mesh(name,verts,faces,material='Wood'):
    data=bpy.data.meshes.new(name); data.from_pydata(verts,[],faces); data.update()
    obj=bpy.data.objects.new(name,data); bpy.context.collection.objects.link(obj)
    return finish(obj,name,material)

def ring(name,z,radius,width,depth,material='Dark',segments=64):
    verts=[]
    for zz,rr in [(z-width/2,radius-depth),(z-width/2,radius),(z+width/2,radius),(z+width/2,radius-depth)]:
        for j in range(segments):
            a=j*math.tau/segments; verts.append((rr*math.cos(a),rr*math.sin(a),zz))
    faces=[]
    for row in range(4):
        for j in range(segments):
            faces.append((row*segments+j,row*segments+(j+1)%segments,((row+1)%4)*segments+(j+1)%segments,((row+1)%4)*segments+j))
    return mesh(name,verts,faces,material)

def export(parts,name):
    bpy.ops.object.select_all(action='DESELECT')
    for part in parts: part.select_set(True)
    bpy.context.view_layer.objects.active=parts[0]
    bpy.ops.object.convert(target='MESH'); bpy.ops.object.join()
    obj=bpy.context.object; obj.name=name
    scene.cursor.location=(0,0,0); bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
    bpy.ops.export_scene.fbx(filepath=str(OUT/f'{name}.fbx'),use_selection=True,
        object_types={'MESH'},apply_unit_scale=True,axis_forward='-Y',axis_up='Z',bake_anim=False,mesh_smooth_type='FACE')
    return obj

# Barrel: bowed individual staves, inset lid, four hoops and forged rivets.
parts=[]
profile=[(-45,25),(-41,26.5),(-30,28.5),(-15,30.3),(0,31),(15,30.3),(30,28.5),(41,26.5),(45,25)]
for stave in range(20):
    a0=stave*math.tau/20+.005; a1=(stave+1)*math.tau/20-.005
    verts=[]
    for z,radius in profile:
        for r,a in [(radius,a0),(radius,a1),(radius-2,a1),(radius-2,a0)]:
            verts.append((r*math.cos(a),r*math.sin(a),z))
    faces=[]
    for row in range(len(profile)-1):
        for edge in range(4): faces.append((row*4+edge,row*4+(edge+1)%4,(row+1)*4+(edge+1)%4,(row+1)*4+edge))
    faces.extend([(3,2,1,0),tuple((len(profile)-1)*4+i for i in range(4))])
    obj=mesh('Bowed stave',verts,faces,'WoodLight' if stave%5==0 else 'Wood')
    bevel=obj.modifiers.new('Stave edges','BEVEL'); bevel.width=.09; bevel.segments=2
    parts.append(obj)
for z,radius in [(-39,27.3),(-25,29.7),(25,29.7),(39,27.3)]:
    parts.append(ring('Iron hoop',z,radius+.45,3.6,.7))
    for i in range(12):
        a=i*math.tau/12
        bpy.ops.mesh.primitive_uv_sphere_add(segments=8,ring_count=4,radius=.45,location=((radius+.48)*math.cos(a),(radius+.48)*math.sin(a),z))
        parts.append(finish(bpy.context.object,'Hoop rivet','Dark'))
for lid_z in (-43.5,43.5):
    for strip in range(7):
        y0=-23.1+strip*6.6+.08; y1=y0+6.44
        ys=[y0+(y1-y0)*i/4 for i in range(5)]
        outline=[(math.sqrt(max(0,23.5**2-y*y)),y,lid_z) for y in ys]
        outline += [(-math.sqrt(max(0,23.5**2-y*y)),y,lid_z) for y in reversed(ys)]
        obj=mesh('Lid board',outline,[tuple(range(len(outline)))],'WoodLight')
        solid=obj.modifiers.new('Lid thickness','SOLIDIFY'); solid.thickness=1
        parts.append(obj)
parts.append(ring('Lid rim',44,25.2,1.5,1.7,'Wood'))
barrel=export(parts,'SM_DockBarrel')

# Crate fits the existing 60 x 65 x 60 cm collision. Real gaps and edge bevels.
parts=[]
for side in (-1,1):
    for i in range(5):
        parts.append(box('Front plank',(i*11.6-23.2,side*30.8,0),(11.2,3.4,57),'WoodLight'))
        parts.append(box('Side plank',(side*28.4,i*12.2-24.4,0),(3.2,11.8,57),'Wood'))
    for z in (-26.5,26.5):
        parts.append(box('Front rail',(0,side*30.7,z),(60,3.6,7)))
        parts.append(box('Side rail',(side*28.3,0,z),(3.4,58,7)))
    for x in (-26.5,26.5): parts.append(box('Corner batten',(x,side*30.6,0),(7,3.8,60)))
    brace=box('Diagonal brace',(0,side*30.9,0),(66,3.2,5),'Wood')
    brace.rotation_euler.y=math.radians(43)
    for x in (-26,26):
        for z in (-26,26):
            bpy.ops.mesh.primitive_uv_sphere_add(segments=8,ring_count=4,radius=.5,location=(x,side*32.4,z))
            nail=bpy.context.object; nail.scale=(1,.25,1); parts.append(finish(nail,'Crate nail','Dark'))
for z in (-28.5,28.5):
    for i in range(5): parts.append(box('Lid plank',(i*11.6-23.2,0,z),(11.2,58,3),'WoodLight'))
crate=export(parts,'SM_DockCrate')

# One bevelled full-size pier board, reused at the original placement/spacing.
parts=[box('Pier board',(0,0,0),(23,180,12),'Wood',.24)]
for y in (-73,73):
    bpy.ops.mesh.primitive_cylinder_add(vertices=12,radius=.8,depth=.3,location=(0,y,5.85))
    parts.append(finish(bpy.context.object,'Recessed nail','Dark'))
plank=export(parts,'SM_DockPlank')
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'DockProps.blend'))
print('CHUCK_DOCK_PROPS_READY',[(o.name,len(o.data.polygons)) for o in (barrel,crate,plank)])
