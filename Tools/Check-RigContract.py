"""Validate the accepted source-only rig contract; never imports bpy or writes assets.

Run with Python 3: Tools/Check-RigContract.py --output Local/rig-v1-review.json
This checks the proposal, not a Blender mesh, FBX, Unreal skeleton or animation.
"""
import argparse
import ast
import hashlib
import json
import math
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ACCEPTED_SHA = '49b6a885fc99940b3cbbda4c999a03664e8b6cb3bbd0b1aafaee71636e0236a6'


def require(condition, message):
    if not condition:
        raise ValueError(message)


def source_table(script):
    tree = ast.parse(script.read_text(encoding='utf-8-sig'))
    nodes = [n for n in tree.body if isinstance(n, ast.Assign)
             and any(isinstance(t, ast.Name) and t.id == 'PROPOSAL' for t in n.targets)]
    require(len(nodes) == 1, 'Expected one literal PROPOSAL table')
    proposal = ast.literal_eval(nodes[0].value)
    table = {}
    for name, (head, tail, parent, deform, note) in proposal.items():
        table[name] = dict(head=list(head), tail=list(tail), parent=parent, deform=deform, note=note)
        if name.endswith('_L'):
            right_parent = parent[:-2] + '_R' if parent and parent.endswith('_L') else parent
            mirror = lambda v: [v[0], -v[1], v[2]]
            table[name[:-2] + '_R'] = dict(head=mirror(head), tail=mirror(tail),
                                         parent=right_parent, deform=deform, note=note)
    return table


def sub(a, b):
    return [x-y for x, y in zip(a, b)]


def dot(a, b):
    return sum(x*y for x, y in zip(a, b))


def unit(v):
    length = math.sqrt(dot(v, v))
    require(length > 1e-6, 'Degenerate direction')
    return [x/length for x in v]


def unreal_position(v):
    return [v[0], -v[1], v[2]]


def validate(table, script):
    require(table == source_table(script), 'JSON differs from generator PROPOSAL; regenerate and review')
    digest = hashlib.sha256(json.dumps(table, sort_keys=True, separators=(',', ':')).encode()).hexdigest()
    require(digest == ACCEPTED_SHA, 'Table differs from the accepted v1 contract; coordinate a revision')
    require(len(table) == 41 and sum(b['deform'] for b in table.values()) == 35, 'Expected 41 bones, 35 deforming')
    require([n for n, b in table.items() if b['parent'] is None] == ['root'], 'Expected one root')
    for name, bone in table.items():
        for point in ('head', 'tail'):
            require(len(bone[point]) == 3 and all(math.isfinite(x) for x in bone[point]), f'{name}: invalid {point}')
        require(math.dist(bone['head'], bone['tail']) > 1e-6, f'{name}: zero length')
        visited = {name}
        parent = bone['parent']
        while parent is not None:
            require(parent in table and parent not in visited, f'{name}: missing/cyclic parent')
            visited.add(parent)
            parent = table[parent]['parent']

    legs = {}
    for side, sign in (('L', 1), ('R', -1)):
        hip, knee, ankle, ball = [table[f'{bone}_{side}']['head'] for bone in ('thigh', 'calf', 'foot', 'toes')]
        require(knee[0] > hip[0] and knee[0] > ankle[0], f'{side}: knee must bend forward')
        require(table[f'ik_foot_{side}']['head'] == ankle, f'{side}: goal must be at ankle, not sole')
        axis = sub(ankle, hip)
        fraction = dot(sub(knee, hip), axis) / dot(axis, axis)
        projected = [h + fraction*a for h, a in zip(hip, axis)]
        pole = unit(sub(knee, projected))
        require(pole[0] > 0, f'{side}: invalid forward pole')
        legs[side] = {
            'hip_component_cm': unreal_position(hip),
            'knee_component_cm': unreal_position(knee),
            'ankle_component_cm': unreal_position(ankle),
            'ball_component_cm': unreal_position(ball),
            'upper_length_cm': math.dist(hip, knee),
            'lower_length_cm': math.dist(knee, ankle),
            'knee_pole_component_direction': unreal_position(pole),
            'sole_markers_component_cm_pending_mesh_fit': {
                label: unreal_position([x, sign*7, 0])
                for label, x in (('heel', -4.5), ('ball', 3.4), ('toe', 8.2))
            },
        }
    cigarette = table['socket_cigarette']
    tip_vector = sub(cigarette['tail'], cigarette['head'])
    return {
        'contract': 'ChuckRigV1', 'proposal_commit': '51ff219', 'accepted_table_sha256': digest,
        'status': 'source contract only; no mesh/FBX/Unreal validation',
        'bone_count': len(table), 'deforming_bone_count': 35,
        'position_conversion': 'Blender (x,y,z) -> expected Unreal (x,-y,z); verify import',
        'rotation_conversion': 'derive from imported rest matrices; not provided by this manifest',
        'legs': legs,
        'cigarette': {
            'parent': 'jaw', 'attachment_bone': 'socket_cigarette',
            'filter_component_cm': unreal_position(cigarette['head']),
            'tip_component_cm': unreal_position(cigarette['tail']),
            'direction_component': unreal_position(unit(tip_vector)),
            'length_cm': math.sqrt(dot(tip_vector, tip_vector)),
            'tip_requires_imported_bone_relative_socket': True,
        },
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--table', type=Path, default=ROOT/'SourceAssets/Chuck/rig_proposal.json')
    parser.add_argument('--script', type=Path, default=ROOT/'SourceAssets/Chuck/rig_proposal.py')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    try:
        result = validate(json.loads(args.table.read_text(encoding='utf-8-sig')), args.script)
        if args.output:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
        print('CHUCK_RIG_CONTRACT_PASS bones=41 deforming=35 hierarchy=valid script_json=match')
        for side, leg in result['legs'].items():
            print(f"{side}: upper={leg['upper_length_cm']:.4f} lower={leg['lower_length_cm']:.4f} cm; forward pole verified")
    except (ValueError, KeyError, TypeError, OSError, SyntaxError) as exc:
        parser.exit(1, f'CHUCK_RIG_CONTRACT_FAIL: {exc}\n')


if __name__ == '__main__':
    main()
