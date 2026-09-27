"""Blender: split delivered Alembic objects for UE, without regenerating strands."""
from pathlib import Path
import json
import bpy

ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'SourceAssets/Chuck/V1/Groom'
OUT=ROOT/'Local/GroomGroups'
OUT.mkdir(parents=True,exist_ok=True)
metadata=json.loads((SOURCE/'groom_metadata.json').read_text())
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.wm.alembic_import(filepath=str(SOURCE/'GR_Chuck.abc'),set_frame_range=False)
groups={o.name:o for o in bpy.context.scene.objects if o.type=='CURVES'}
assert set(groups)==set(metadata['groups']),list(groups)
for name,obj in groups.items():
    assert len(obj.data.curves)==metadata['groups'][name]['strands']
    # Reimported Alembic curves carry NURBS metadata without usable knots.
    # Rebuild only the curve container, retaining every point and radius.
    old=obj.data
    points=[tuple(p.position) for p in old.points]
    radii=[p.radius for p in old.points]
    counts=[len(c.points) for c in old.curves]
    fresh=bpy.data.hair_curves.new(name+'_UE')
    fresh.add_curves(counts)
    fresh.set_types(type='POLY')  # Authored five-point strand segments, no spline knots.
    fresh.points.foreach_set('position',[v for p in points for v in p])
    fresh.points.foreach_set('radius',radii)
    obj.data=fresh
    assert all((p.position-old.points[i].position).length<1e-7 for i,p in enumerate(fresh.points))
    for other in bpy.context.scene.objects:
        other.select_set(False)
    obj.select_set(True)
    bpy.context.view_layer.objects.active=obj
    bpy.ops.wm.alembic_export(filepath=str(OUT/(name+'.abc')),selected=True,start=0,end=0,
        export_hair=True,export_particles=False,uvs=False,normals=False,face_sets=False,global_scale=1.)
    print('CHUCK_GROOM_GROUP',name,len(obj.data.curves))
print('CHUCK_GROOM_GROUPS_READY')
