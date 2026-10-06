"""Derive board UVs and recess overlapping panels; preserve source mesh and outer bounds."""
from pathlib import Path
import hashlib
import json
import bpy
import numpy as np
from mathutils import Vector
from mathutils.bvhtree import BVHTree

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'SourceAssets/Docks/SM_DockCrate.fbx'
OUT = ROOT / 'SourceAssets/Surfaces/WeatheredTimber'
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
bpy.context.scene.unit_settings.system = 'METRIC'
bpy.context.scene.unit_settings.scale_length = .01
bpy.ops.import_scene.fbx(filepath=str(SOURCE))
objects = [o for o in bpy.context.scene.objects if o.type == 'MESH']
count = 0
recessed = {'front_panel':0, 'side_panel':0, 'front_rail':0, 'side_rail':0, 'lid':0}

def visible_coplanar_overlaps(obj):
    """Find same-facing coplanar polygon intersections exposed to an outside view."""
    vertices=[obj.matrix_world @ v.co for v in obj.data.vertices]
    polygons=[list(p.vertices) for p in obj.data.polygons]
    tree=BVHTree.FromPolygons(vertices,polygons)
    flat=[]
    def cross(a,b): return a[0]*b[1]-a[1]*b[0]
    def sub(a,b): return (a[0]-b[0],a[1]-b[1])
    def signed_area(points): return sum(cross(p,points[(i+1)%len(points)]) for i,p in enumerate(points))*.5
    for poly in polygons:
        points=[vertices[i] for i in poly]
        for axis in range(3):
            if max(p[axis] for p in points)-min(p[axis] for p in points)>1e-4: continue
            a,b=(axis+1)%3,(axis+2)%3
            projected=[(p[a],p[b]) for p in points]
            area=signed_area(projected)
            if abs(area)>.001: flat.append((axis,points[0][axis],1 if area>0 else -1,projected))
    overlaps=0
    for i,(axis,plane,sign,subject) in enumerate(flat):
        for other_axis,other_plane,other_sign,clip in flat[i+1:]:
            if axis!=other_axis or sign!=other_sign or abs(plane-other_plane)>1e-4: continue
            intersection=subject[:]
            for edge,p in enumerate(clip):
                q=clip[(edge+1)%len(clip)]; direction=sub(q,p)
                source=intersection; intersection=[]
                if not source: break
                for j,end in enumerate(source):
                    start=source[j-1]; ds=cross(direction,sub(start,p))*sign; de=cross(direction,sub(end,p))*sign
                    if (ds>=0)!=(de>=0):
                        t=ds/(ds-de); intersection.append((start[0]+(end[0]-start[0])*t,start[1]+(end[1]-start[1])*t))
                    if de>=0: intersection.append(end)
            if len(intersection)<3 or abs(signed_area(intersection))<.001: continue
            point=Vector((0,0,0)); point[axis]=plane+sign*.002
            point[(axis+1)%3]=sum(p[0] for p in intersection)/len(intersection)
            point[(axis+2)%3]=sum(p[1] for p in intersection)/len(intersection)
            normal=Vector((0,0,0)); normal[axis]=sign
            if tree.ray_cast(point,normal)[0] is None: overlaps+=1
    return overlaps

before_overlaps=sum(visible_coplanar_overlaps(obj) for obj in objects)
for obj in objects:
    mesh = obj.data
    original_bounds = np.array([list(obj.matrix_world @ vertex.co) for vertex in mesh.vertices])
    uv = mesh.uv_layers.active or mesh.uv_layers.new(name='BoardGrain')
    adjacency = [[] for _ in mesh.vertices]
    for edge in mesh.edges:
        a, b = edge.vertices
        adjacency[a].append(b); adjacency[b].append(a)
    groups = {}
    remaining = set(range(len(mesh.vertices)))
    while remaining:
        stack = [remaining.pop()]; component = []
        while stack:
            index = stack.pop(); component.append(index)
            for neighbor in adjacency[index]:
                if neighbor in remaining:
                    remaining.remove(neighbor); stack.append(neighbor)
        world_points = np.array([list(obj.matrix_world @ mesh.vertices[i].co) for i in component])
        size = world_points.max(axis=0)-world_points.min(axis=0)
        mid = (world_points.max(axis=0)+world_points.min(axis=0))*.5
        shift = Vector((0,0,0))
        kind = None
        # Old board/rail/batten outward faces shared the same plane. World-aligned
        # colour hid it; independent board UVs exposed depth-buffer fighting.
        for expected, label, axis, inset in [
            ((11.2,3.4,57),'front_panel',1,.60),
            ((3.2,11.8,57),'side_panel',0,.60),
            ((60,3.6,7),'front_rail',1,.25),
            ((3.4,58,7),'side_rail',0,.25),
            ((11.2,58,3),'lid',2,.60)]:
            if np.allclose(size,expected,atol=.01):
                kind=label; shift[axis]=-np.sign(mid[axis])*inset
                if label.endswith('_rail'): shift[2]=-np.sign(mid[2])*inset
                break
        if kind:
            local_shift = obj.matrix_world.inverted().to_3x3() @ shift
            for i in component: mesh.vertices[i].co += local_shift
            # Horizontal front-rail end caps were also flush with corner-batten
            # sides. Tuck those ends behind the battens, keeping the outer size.
            if kind=='front_rail':
                for i in component:
                    world=obj.matrix_world @ mesh.vertices[i].co
                    world.x=mid[0]+(world.x-mid[0])*(59.0/60.0)
                    mesh.vertices[i].co=obj.matrix_world.inverted() @ world
            recessed[kind] += 1
        points = np.array([mesh.vertices[i].co[:] for i in component])
        center = points.mean(axis=0)
        _, axes = np.linalg.eigh(np.cov((points-center).T))
        length, width, depth = (axes[:, i] for i in (2, 1, 0))
        # Fix PCA sign for reproducible re-runs, including the diagonal braces.
        for axis in (length, width, depth):
            if axis[np.argmax(np.abs(axis))] < 0: axis *= -1
        offset = np.array([(count * .317) % 1, (count * .173) % 1])
        record = (center, length, width, depth, offset)
        for i in component: groups[i] = record
        count += 1
    for face in mesh.polygons:
        center, length, width, depth, offset = groups[face.vertices[0]]
        normal = np.array(face.normal[:])
        # The length always runs up texture V; end faces use the other two axes.
        if abs(np.dot(normal, length)) > .8:
            u, v = width, depth
        else:
            u = depth if abs(np.dot(normal, width)) > abs(np.dot(normal, depth)) else width
            v = length
        for loop_index in face.loop_indices:
            point = np.array(mesh.vertices[mesh.loops[loop_index].vertex_index].co[:]) - center
            uv.data[loop_index].uv = (np.dot(point, u)/90+offset[0], np.dot(point, v)/90+offset[1])
    obj.name = 'SM_WeatheredDockCrate'
    mesh.update()
    bounds = np.array([list(obj.matrix_world @ vertex.co) for vertex in mesh.vertices])
    if not np.allclose(bounds.min(axis=0), original_bounds.min(axis=0), atol=.001) or not np.allclose(bounds.max(axis=0), original_bounds.max(axis=0), atol=.001):
        raise RuntimeError('Recess changed outer crate bounds')
    for material in mesh.materials:
        old = material.name.split('.')[0]
        if old in ('Wood', 'WoodLight'): material.name = 'WeatheredCrate' + ('Light' if old == 'WoodLight' else '')
bpy.ops.object.select_all(action='DESELECT')
for obj in objects: obj.select_set(True)
bpy.context.view_layer.objects.active = objects[0]
target = OUT / 'SM_WeatheredDockCrate.fbx'
if recessed != {'front_panel':10,'side_panel':10,'front_rail':4,'side_rail':4,'lid':10}:
    raise RuntimeError('Unexpected source crate components: '+str(recessed))
after_overlaps=sum(visible_coplanar_overlaps(obj) for obj in objects)
if after_overlaps: raise RuntimeError('Exposed coplanar crate overlaps remain: '+str(after_overlaps))
bpy.ops.export_scene.fbx(filepath=str(target), use_selection=True, object_types={'MESH'},
    apply_unit_scale=True, axis_forward='-Y', axis_up='Z', bake_anim=False, mesh_smooth_type='FACE')
(OUT/'crate-derivation.json').write_text(json.dumps({
    'source':str(SOURCE.relative_to(ROOT)).replace('\\','/'),
    'source_sha256':hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
    'derived':target.name, 'derived_sha256':hashlib.sha256(target.read_bytes()).hexdigest(),
    'blender':bpy.app.version_string, 'connected_pieces':count,
    'recessed_pieces':recessed,
    'visible_coplanar_overlaps_before':before_overlaps, 'visible_coplanar_overlaps_after':after_overlaps,
    'panel_inset_cm':.6, 'rail_inset_cm':.25,
    'changes':'Per-board PCA UVs; recess 38 panels/rails/lids to separate coplanar faces; outer bounds, topology, original FBX preserved'
}, indent=2)+'\n', encoding='utf-8')
print('CHUCK_WEATHERED_CRATE_UV_READY pieces='+str(count))
print('CHUCK_CRATE_RECESS_READY '+str(recessed))
print('CHUCK_CRATE_COPLANAR_CHECK before='+str(before_overlaps)+' after='+str(after_overlaps))
