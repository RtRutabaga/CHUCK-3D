"""Original tavern window and bench studies, Blender 4.5.14, centimetres."""
from pathlib import Path
import math
import bpy
from mathutils import Vector

OUT=Path(__file__).resolve().parents[1]/'SourceAssets'/'Docks'
bpy.ops.object.select_all(action='SELECT'); bpy.ops.object.delete(use_global=False)
scene=bpy.context.scene
scene.unit_settings.system='METRIC'; scene.unit_settings.scale_length=.01
materials={}
for name,color in [('Wood',(.13,.068,.029,1)),('WoodLight',(.225,.135,.062,1)),('Dark',(.045,.052,.05,1)),('WindowGlass',(.055,.065,.059,1))]:
    mat=bpy.data.materials.new(name); mat.diffuse_color=color; materials[name]=mat

def box(name,loc,size,mat='Wood',bevel=.3):
    bpy.ops.mesh.primitive_cube_add(size=1,location=loc)
    obj=bpy.context.object; obj.name=name; obj.scale=size
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    if bevel:
        mod=obj.modifiers.new('Worn edge','BEVEL'); mod.width=bevel; mod.segments=3
        mod=obj.modifiers.new('Face normals','WEIGHTED_NORMAL'); mod.keep_sharp=True
    obj.data.materials.append(materials[mat]); return obj

def rod(name,a,b,radius,mat='Dark'):
    direction=Vector(b)-Vector(a)
    bpy.ops.mesh.primitive_cylinder_add(vertices=12,radius=radius,depth=direction.length,location=(Vector(a)+Vector(b))*.5)
    obj=bpy.context.object; obj.name=name; obj.rotation_mode='QUATERNION'; obj.rotation_quaternion=direction.to_track_quat('Z','Y')
    obj.data.materials.append(materials[mat]); return obj

def export(parts,name):
    bpy.ops.object.select_all(action='DESELECT')
    for part in parts: part.select_set(True)
    bpy.context.view_layer.objects.active=parts[0]
    bpy.ops.object.convert(target='MESH'); bpy.ops.object.join()
    obj=bpy.context.object; obj.name=name
    scene.cursor.location=(0,0,0); bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
    bpy.ops.export_scene.fbx(filepath=str(OUT/f'{name}.fbx'),use_selection=True,object_types={'MESH'},apply_unit_scale=True,axis_forward='-Y',axis_up='Z',bake_anim=False,mesh_smooth_type='FACE')
    return obj

# Bench origin is at ground level. Seat retains the original 160 x 42 x 8 bounds.
parts=[]
for i in range(4): parts.append(box('Seat board',(0,-15.75+i*10.5,45),(160,10,8),'WoodLight',.55))
for x in (-65,65):
    for y in (-12,12): parts.append(box('Bench foot',(x,y,20.5),(11,10,41),'Wood',.6))
    parts.append(box('Seat support',(x,0,39),(13,40,6),'Wood',.4))
    parts.append(box('End stretcher',(x,0,12),(8,34,7),'Wood',.3))
    for y in (-15.75,-5.25,5.25,15.75): parts.append(rod('Seat peg',(x,y,48.5),(x,y,48.9),.7,'Dark'))
parts.append(box('Long stretcher',(0,0,15),(139,8,8),'Wood',.35))
bench=export(parts,'SM_TavernBench')

# Window origin at the old frame centre, width X, front toward -Y.
# Open shutters lie against the frontage, leaving four small leaded panes visible.
parts=[]
for x in (-37,37): parts.append(box('Frame upright',(x,2,0),(6,20,95),'Wood',.45))
for z in (-44.5,44.5): parts.append(box('Frame rail',(0,2,z),(74,20,6),'Wood',.45))
parts.append(box('Central mullion',(0,-10,0),(4,6,83),'Wood',.3))
parts.append(box('Cross rail',(0,-10,0),(68,6,4),'Wood',.3))
for x in (-18,18):
    for z in (-21,21):
        parts.append(box('Small glass pane',(x,-9,z),(31,1,38),'WindowGlass',.12))
        parts.append(rod('Lead diagonal',(x-15,-10,z-18),(x+15,-10,z+18),.28))
        parts.append(rod('Lead diagonal',(x-15,-10,z+18),(x+15,-10,z-18),.28))
parts.append(box('Draining sill',(0,-3,-49),(91,29,7),'WoodLight',.6))
parts.append(box('Upper drip rail',(0,-2,49),(87,25,4),'WoodLight',.45))
for side in (-1,1):
    for i in range(4): parts.append(box('Shutter plank',(side*(43.5+i*8.2),5,0),(7.8,4,82),'WoodLight',.22))
    for z in (-31,31): parts.append(box('Shutter batten',(side*56,1.5,z),(33,4,5),'Wood',.22))
    for z in (-25,25):
        parts.append(box('Hinge strap',(side*44,-1,z),(12,1.4,3),'Dark',.16))
        parts.append(rod('Hinge pin',(side*38,-1,z-2.2),(side*38,-1,z+2.2),.65))
window=export(parts,'SM_TavernWindow')
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'TavernProps.blend'))
print('CHUCK_TAVERN_PROPS_READY',[(o.name,len(o.data.polygons)) for o in (bench,window)])
