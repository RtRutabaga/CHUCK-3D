"""Blender 4.5 LTS: contract checks for SourceAssets/Chuck/Chuck.blend.

blender --background SourceAssets/Chuck/Chuck.blend --python SourceAssets/Chuck/check_model.py

Prints CHUCK_CHECK lines and exits non-zero on a contract failure. Never saves.
"""
import sys
import bmesh
import bpy

failures = []
def check(ok, label, detail=''):
    print('CHUCK_CHECK', 'PASS' if ok else 'FAIL', label, detail)
    if not ok: failures.append(label)

rig = bpy.data.objects['ChuckRig']
body = bpy.data.objects['SK_ChuckBody']
foot = bpy.data.objects['SM_ChuckFoot']
expected = {'root': None, 'head': 'root'}
for s in 'LR':
    expected.update({f'thigh_{s}': 'root', f'shin_{s}': f'thigh_{s}',
                     f'arm_{s}': 'root', f'forearm_{s}': f'arm_{s}'})
for i in range(4):
    expected[f'tail_{i}'] = 'root' if i == 0 else f'tail_{i-1}'
bones = {b.name: (b.parent.name if b.parent else None) for b in rig.data.bones}
check(bones == expected, 'bone names/parents', sorted(bones))
heads = {b.name: tuple(round(c, 3) for c in b.head_local) for b in rig.data.bones}
print('CHUCK_INFO bone heads', heads)

zs = [v.co.z for v in body.data.vertices]
check(abs(max(zs) - 65) < .05, 'ear top 65 cm', f'{max(zs):.3f}')
xs = [v.co.x for v in body.data.vertices]; ys = [v.co.y for v in body.data.vertices]
print('CHUCK_INFO body bounds', [round(min(xs), 2), round(max(xs), 2)], [round(min(ys), 2), round(max(ys), 2)], [round(min(zs), 2), round(max(zs), 2)])

slots = sorted({m.name.split('.')[0] for m in body.data.materials})
check(set(slots) <= {'Fur', 'Chest', 'Jacket', 'Seam', 'Skin', 'Eye', 'Claw', 'Metal', 'Whisker'}, 'material slot names', slots)
foot_slots = sorted({m.name.split('.')[0] for m in foot.data.materials})
fz = [v.co.z for v in foot.data.vertices]; fx = [v.co.x for v in foot.data.vertices]
print('CHUCK_INFO foot bounds x', round(min(fx), 2), round(max(fx), 2), 'z', round(min(fz), 2), round(max(fz), 2))
check(-2.3 < min(fz) < -1.6, 'foot sole near local z=-2 (runtime origin 2.5 cm above ground)', f'{min(fz):.2f}')
print('CHUCK_INFO foot slots', foot_slots)

groups = {g.index: g.name for g in body.vertex_groups}
check(set(groups.values()) <= set(expected), 'vertex groups are rig bones', sorted(set(groups.values())))
bad = [v.index for v in body.data.vertices if abs(sum(g.weight for g in v.groups) - 1) > .001]
check(not bad, 'weight totals == 1', f'{len(bad)} bad')

def tris(obj):
    return sum(len(p.vertices) - 2 for p in obj.data.polygons)
print('CHUCK_INFO triangles body', tris(body), 'foot', tris(foot))

# Jacket garment closure: every Jacket-material face (solidified shell and
# closed sleeve tubes) must have a neighbour across each edge.
jacket_slots = {i for i, m in enumerate(body.data.materials) if m.name.split('.')[0] == 'Jacket'}
lining_slots = jacket_slots | {i for i, m in enumerate(body.data.materials) if m.name.split('.')[0] == 'Seam'}
bm = bmesh.new(); bm.from_mesh(body.data)
open_edges = 0
for f in bm.faces:
    if f.material_index not in jacket_slots: continue
    for e in f.edges:
        if sum(1 for g in e.link_faces if g.material_index in lining_slots) < 2: open_edges += 1
check(open_edges == 0, 'jacket shell has no open edges', f'{open_edges} open')
bm.free()

# Garment deformation: sleeves and the armhole region carry graded arm/forearm
# weights rather than a single rigid bone.
for s in 'LR':
    arm = next((g.index for g in body.vertex_groups if g.name == f'arm_{s}'), None)
    fore = next((g.index for g in body.vertex_groups if g.name == f'forearm_{s}'), None)
    blended = sum(1 for v in body.data.vertices
                  if {g.group for g in v.groups if 0 < g.weight < 1} & {arm, fore})
    check(blended > 0, f'graded arm weights {s}', f'{blended} blended vertices')

print('CHUCK_CHECK_DONE', 'FAIL' if failures else 'PASS', failures)
sys.exit(1 if failures else 0)
