"""Derive board-aligned UVs from the existing dock crate; never regenerate its geometry."""
from pathlib import Path
import hashlib
import json
import bpy
import numpy as np
from mathutils import Vector

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
for obj in objects:
    mesh = obj.data
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
    for material in mesh.materials:
        old = material.name.split('.')[0]
        if old in ('Wood', 'WoodLight'): material.name = 'WeatheredCrate' + ('Light' if old == 'WoodLight' else '')
bpy.ops.object.select_all(action='DESELECT')
for obj in objects: obj.select_set(True)
bpy.context.view_layer.objects.active = objects[0]
target = OUT / 'SM_WeatheredDockCrate.fbx'
bpy.ops.export_scene.fbx(filepath=str(target), use_selection=True, object_types={'MESH'},
    apply_unit_scale=True, axis_forward='-Y', axis_up='Z', bake_anim=False, mesh_smooth_type='FACE')
(OUT/'crate-derivation.json').write_text(json.dumps({
    'source':str(SOURCE.relative_to(ROOT)).replace('\\','/'),
    'source_sha256':hashlib.sha256(SOURCE.read_bytes()).hexdigest(),
    'derived':target.name, 'derived_sha256':hashlib.sha256(target.read_bytes()).hexdigest(),
    'blender':bpy.app.version_string, 'connected_pieces':count,
    'changes':'UV projection only, per connected board PCA; geometry and original FBX preserved'
}, indent=2)+'\n', encoding='utf-8')
print('CHUCK_WEATHERED_CRATE_UV_READY pieces='+str(count))
