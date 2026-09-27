"""Blender: verify roots read back from Unreal against delivered FBX triangles."""
from pathlib import Path
import json
import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree

ROOT=Path(__file__).resolve().parents[1]
report=json.loads((ROOT/'Local/groom-import-report.json').read_text())
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(ROOT/'SourceAssets/Chuck/V1/SK_Chuck_Groomed.fbx'))
mesh=next(o for o in bpy.context.scene.objects if o.type=='MESH')
tree=BVHTree.FromObject(mesh,bpy.context.evaluated_depsgraph_get())
worst=0
count=0
for group in report:
    for x,y,z in group['sampled_roots_ue_cm']:
        root=Vector((x,-y,z))
        hit=tree.find_nearest(root)
        assert hit[0] is not None
        worst=max(worst,(hit[0]-root).length)
        count+=1
assert count>=702 and worst<.1,(count,worst)
(ROOT/'Local/groom-root-validation.json').write_text(json.dumps({'samples':count,'max_surface_distance_cm':worst},indent=2))
print('CHUCK_GROOM_ROOTS_VALIDATED',count,worst)
