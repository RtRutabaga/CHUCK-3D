"""Original moored working boat and rope coil, Blender 4.5, centimetres."""
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
for name,color in [('Wood',(.13,.068,.029,1)),('WoodLight',(.225,.135,.062,1)),('Dark',(.045,.052,.05,1)),('Plaster',(.5,.46,.34,1))]:
    mat=bpy.data.materials.new(name); mat.diffuse_color=color; materials[name]=mat

def finish(obj,name,material):
    obj.name=name; obj.data.materials.append(materials[material]); return obj

def mesh(name,verts,faces,material='Wood'):
    data=bpy.data.meshes.new(name); data.from_pydata(verts,[],faces); data.update()
    obj=bpy.data.objects.new(name,data); bpy.context.collection.objects.link(obj)
    return finish(obj,name,material)

def box(name,loc,size,material='Wood',bevel=.4):
    bpy.ops.mesh.primitive_cube_add(size=1,location=loc)
    obj=bpy.context.object; obj.scale=size
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
    mod=obj.modifiers.new('Worn edges','BEVEL'); mod.width=bevel; mod.segments=2
    mod=obj.modifiers.new('Face normals','WEIGHTED_NORMAL'); mod.keep_sharp=True
    return finish(obj,name,material)

def tube(name,points,radius,material='Wood',sides=8):
    curve=bpy.data.curves.new(name,'CURVE'); curve.dimensions='3D'
    curve.bevel_depth=radius; curve.bevel_resolution=1; curve.resolution_u=1
    spline=curve.splines.new('POLY'); spline.points.add(len(points)-1)
    for point,co in zip(spline.points,points): point.co=(*co,1)
    obj=bpy.data.objects.new(name,curve); bpy.context.collection.objects.link(obj)
    return finish(obj,name,material)

def export(parts,name):
    bpy.ops.object.select_all(action='DESELECT')
    for part in parts: part.select_set(True)
    bpy.context.view_layer.objects.active=parts[0]
    bpy.ops.object.convert(target='MESH'); bpy.ops.object.join()
    obj=bpy.context.object; obj.name=name
    scene.cursor.location=(0,0,0); bpy.ops.object.origin_set(type='ORIGIN_CURSOR')
    bpy.ops.object.transform_apply(location=False,rotation=True,scale=True)
    # Recalculate normals after joining planks, ribs and curves.
    bpy.ops.object.mode_set(mode='EDIT'); bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.normals_make_consistent(inside=False); bpy.ops.object.mode_set(mode='OBJECT')
    bpy.ops.export_scene.fbx(filepath=str(OUT/f'{name}.fbx'),use_selection=True,
        object_types={'MESH'},apply_unit_scale=True,axis_forward='-Y',axis_up='Z',bake_anim=False,mesh_smooth_type='FACE')
    return obj

# A six-metre open cargo boat, with pointed bow and narrow stern. Z=0 is waterline.
# Station: fore/aft X, half beam, gunwale height. No invented cabin or playable deck.
stations=[(-300,10,85),(-260,50,68),(-200,80,57),(-120,98,50),(0,108,47),(120,97,50),(200,72,59),(260,37,77),(310,1.5,102)]
def surface(station,t,side,offset=0):
    x,width,top=station
    return (x,side*(width*math.sin(t*math.pi/2)+offset),-48+(top+48)*(1-math.cos(t*math.pi/2)))

parts=[]
for side in (-1,1):
    # Eight real strakes per side; narrow seams and thickness survive side views.
    for row in range(8):
        lo=row/8+.001; hi=(row+1)/8-.001
        verts=[]
        for station in stations:
            verts.extend([surface(station,lo,side),surface(station,hi,side)])
        faces=[(i*2,i*2+1,i*2+3,i*2+2) for i in range(len(stations)-1)]
        obj=mesh('Hull strake',verts,faces,'WoodLight' if row==7 else 'Wood')
        solid=obj.modifiers.new('Plank thickness','SOLIDIFY'); solid.thickness=2
        bevel=obj.modifiers.new('Plank edge','BEVEL'); bevel.width=.25; bevel.segments=2
        parts.append(obj)
    parts.append(tube('Gunwale',[(x,side*w,z+1) for x,w,z in stations],3.2,'WoodLight'))
    for t in (.55,.83): parts.append(tube('Rubbing strake',[surface(st,t,side,1.5) for st in stations],1.6))
    # Standing rigging is intentionally quiet, with the sail furled at the yard.
    for x in (-170,150): parts.append(tube('Stay',[(x,side*75,55),(15,side*5,435)],.65,'Dark'))
parts.append(tube('Keel',[(-300,0,85),(-300,0,-36),(0,0,-51),(270,0,-38),(310,0,102)],3.2))
for x in (-210,-120,0,120,210):
    st=min(stations,key=lambda s:abs(s[0]-x))
    pts=[surface((x,st[1]-3,st[2]),i/12,-1) for i in reversed(range(13))]
    pts += [surface((x,st[1]-3,st[2]),i/12,1) for i in range(1,13)]
    parts.append(tube('Inner rib',pts,2.2,'WoodLight'))
for x,width in [(-175,148),(-60,192),(90,182),(185,127)]:
    parts.append(box('Thwart',(x,0,29),(25,width,5),'WoodLight'))
# Raised bow deck closes the fine point; a working floor sits above the bilge.
for y in range(-48,49,12): parts.append(box('Floor board',(0,y,-7),(350,11.5,3),'WoodLight'))
parts.append(tube('Mast',[(15,0,-9),(15,0,440)],5))
parts.append(tube('Yard',[(-110,0,338),(135,0,350)],3.2,'WoodLight'))
parts.append(tube('Boom',[(-180,0,85),(100,0,85)],2.8))
for end in (-280,285): parts.append(tube('Fore and aft stay',[(end,0,80),(15,0,435)],.65,'Dark'))
# Gathered sail is a restrained bundle of cloth rather than a flat primitive sphere.
verts=[]; faces=[]
for i in range(61):
    x=-106+i*4
    z=332+(x+110)*12/245
    radius=7.5+1.2*math.sin(i*1.8)+.6*math.sin(i*4.3)
    for j in range(12):
        a=j*math.tau/12; verts.append((x,math.cos(a)*radius,z+math.sin(a)*radius))
for i in range(60):
    for j in range(12): faces.append((i*12+j,i*12+(j+1)%12,(i+1)*12+(j+1)%12,(i+1)*12+j))
faces.extend([tuple(reversed(range(12))),tuple(60*12+j for j in range(12))])
parts.append(mesh('Furled canvas',verts,faces,'Plaster'))
for x in range(-90,131,36):
    z=332+(x+110)*12/245
    parts.append(tube('Sail tie',[(x,math.cos(a*math.tau/16)*9.2,z+math.sin(a*math.tau/16)*9.2) for a in range(17)],.55,'Dark'))
parts.append(box('Rudder',(-303,0,-3),(24,5,66)))
parts.append(tube('Tiller',[(-303,0,33),(-244,0,46)],2.3,'WoodLight'))
boat=export(parts,'SM_HarborBoat')

# Low coil beside existing bollards. Decorative, no extra navigation obstruction.
parts=[]
pts=[]
for i in range(481):
    t=i/480; a=t*math.tau*4; r=7+14*t
    pts.append((r*math.cos(a),r*math.sin(a),1.3+.2*math.sin(a*2)))
parts.append(tube('Coiled rope',pts,1.25,'Plaster'))
parts.append(tube('Rope end',[(21,0,1.3),(25,5,1.3),(30,7,1.3),(34,6,1.3)],1.25,'Plaster'))
coil=export(parts,'SM_RopeCoil')
bpy.ops.wm.save_as_mainfile(filepath=str(OUT/'HarborBoat.blend'))
print('CHUCK_HARBOR_BOAT_READY',[(o.name,len(o.data.polygons)) for o in (boat,coil)])
