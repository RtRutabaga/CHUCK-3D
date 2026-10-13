"""Derive a small cm-scale setting kit from verified CC0 sources, Blender 4.5.14.

Only writes ChultNature/Derived. Never rebuilds the authored jungle or character.
"""
import hashlib
import json
import math
from pathlib import Path
import bpy
import bmesh
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'SourceAssets/Setting/ChultNature'
OUT = SOURCE / 'Derived'
OUT.mkdir(exist_ok=True)
manifest = json.loads((SOURCE / 'manifest.json').read_text(encoding='utf-8-sig'))
for rec in manifest['assets']:
    if hashlib.sha256((SOURCE / rec['file']).read_bytes()).hexdigest().upper() != rec['sha256']:
        raise RuntimeError('Source hash mismatch: ' + rec['file'])
report = []
for asset in ('fern_02', 'rock_moss_set_01', 'island_tree_02'):
    bpy.ops.wm.open_mainfile(filepath=str(SOURCE / (asset + '_2k.blend')))
    # Disable inherited rendering nodes and take only complete mesh variants.
    objects = sorted([o for o in bpy.data.objects if o.type == 'MESH' and
        (asset != 'island_tree_02' or o.name == 'island_tree_02_LOD1')], key=lambda o: o.name)
    for index, original in enumerate(objects):
        ob = original.copy(); ob.data = original.data.copy(); bpy.context.collection.objects.link(ob)
        bpy.ops.object.select_all(action='DESELECT')
        ob.hide_set(False); ob.hide_viewport=False; ob.select_set(True); bpy.context.view_layer.objects.active=ob
        # Preserve rotation/scale, remove the source gallery arrangement.
        ob.location=Vector((0,0,0))
        bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
        if asset == 'island_tree_02':
            dec = ob.modifiers.new('BackgroundBudget', 'DECIMATE'); dec.ratio=.075
            bpy.ops.object.modifier_apply(modifier=dec.name)
        elif asset == 'rock_moss_set_01':
            dec = ob.modifiers.new('RockBudget', 'DECIMATE'); dec.ratio=.45
            bpy.ops.object.modifier_apply(modifier=dec.name)
        # Centre footprint, put the bottom at ground level. Keep UVs/materials.
        pts=[v.co.copy() for v in ob.data.vertices]
        shift=Vector(((min(p.x for p in pts)+max(p.x for p in pts))*.5,
                      (min(p.y for p in pts)+max(p.y for p in pts))*.5, min(p.z for p in pts)))
        for v in ob.data.vertices: v.co=(v.co-shift)*100
        bpy.context.scene.unit_settings.system='METRIC'; bpy.context.scene.unit_settings.scale_length=.01
        name = ('SM_ChultFern' if asset=='fern_02' else 'SM_ChultRock' if asset=='rock_moss_set_01' else 'SM_ChultIslandTree') + (str(index) if asset!='island_tree_02' else '')
        ob.name=name; ob.data.name=name
        bpy.ops.export_scene.fbx(filepath=str(OUT/(name+'.fbx')), use_selection=True,
            object_types={'MESH'}, apply_unit_scale=True, axis_forward='-Y', axis_up='Z',
            bake_anim=False, mesh_smooth_type='FACE', add_leaf_bones=False, path_mode='STRIP')
        ob.data.calc_loop_triangles()
        dims=[round(float(d),3) for d in ob.dimensions]
        record=dict(file=name+'.fbx', source=asset, source_object=original.name,
                    triangles=len(ob.data.loop_triangles), dimensions_cm=dims,
                    sha256=hashlib.sha256((OUT/(name+'.fbx')).read_bytes()).hexdigest().upper())
        report.append(record); print('CHUCK_NATURE_DERIVED',record)
        if asset=='island_tree_02':
            # The query surface is the visible reduced bark geometry alone;
            # leaf and branch cards never become camera/character blockers.
            collision=ob.copy(); collision.data=ob.data.copy(); bpy.context.collection.objects.link(collision)
            bm=bmesh.new(); bm.from_mesh(collision.data)
            bmesh.ops.delete(bm,geom=[f for f in bm.faces if f.material_index!=0],context='FACES')
            bmesh.ops.delete(bm,geom=[v for v in bm.verts if not v.link_faces],context='VERTS')
            bm.to_mesh(collision.data); bm.free()
            bpy.context.view_layer.update()
            bpy.ops.object.select_all(action='DESELECT'); collision.select_set(True)
            bpy.context.view_layer.objects.active=collision
            collision.name='SM_ChultIslandTrunkCollision'
            file=OUT/(collision.name+'.fbx')
            bpy.ops.export_scene.fbx(filepath=str(file),use_selection=True,object_types={'MESH'},
                apply_unit_scale=True,axis_forward='-Y',axis_up='Z',bake_anim=False,mesh_smooth_type='FACE',path_mode='STRIP')
            collision.data.calc_loop_triangles()
            report.append(dict(file=file.name,source=asset,source_object=original.name+' bark faces only',
                triangles=len(collision.data.loop_triangles),dimensions_cm=[round(float(d),3) for d in collision.dimensions],
                sha256=hashlib.sha256(file.read_bytes()).hexdigest().upper()))
            bpy.data.objects.remove(collision,do_unlink=True)
        bpy.data.objects.remove(ob, do_unlink=True)
(OUT/'manifest.json').write_text(json.dumps(dict(blender='4.5.14 / 62c1db4208e8', assets=report),indent=2)+'\n')
print('CHUCK_NATURE_PREPARED meshes=',len(report))
