"""Blender 4.5 LTS: posing helpers for Chuck rig v1 (docs/RIG-CONTRACT-V1.md).

Shared by Tools/build_chuck_v1.py (clip authoring) and
SourceAssets/Chuck/V1/review_v1.py (pose evidence). Coordinates are armature
space = source space: centimetres, Z up, nose +X, +Y is the _L side.
"""
import math
import bpy
from mathutils import Matrix, Vector


class Poser:
    def __init__(self, rig):
        self.rig = rig
        self.rest = {b.name: b.matrix_local.copy() for b in rig.data.bones}
        self.rest_head = {n: m.translation.copy() for n, m in self.rest.items()}
        self.rest_tail = {b.name: b.tail_local.copy() for b in rig.data.bones}
        for pb in rig.pose.bones:
            pb.rotation_mode = 'QUATERNION'

    def length(self, name):
        return (self.rest_tail[name] - self.rest_head[name]).length

    def reset(self):
        for pb in self.rig.pose.bones:
            pb.matrix_basis = Matrix.Identity(4)
        self.update()

    def update(self):
        bpy.context.view_layer.update()

    def rotate(self, name, axis, degrees):
        """Rotate a bone about a source-space axis ('X','Y','Z' or a vector),
        composed after any rotation already applied this pose. Negative Y
        swings a hanging limb forward (+X)."""
        pb = self.rig.pose.bones[name]
        r = self.rest[name].to_3x3()
        world = Matrix.Rotation(math.radians(degrees), 3, axis)
        local = (r.inverted() @ world @ r).to_quaternion()
        pb.rotation_quaternion = local @ pb.rotation_quaternion

    def translate(self, name, offset):
        pb = self.rig.pose.bones[name]
        pb.location = pb.location + self.rest[name].to_3x3().inverted() @ Vector(offset)

    def head(self, name):
        return self.rig.pose.bones[name].head.copy()

    def aim(self, name, head, direction):
        """Place a bone at head (armature space) pointing along direction,
        with the minimal rotation from its rest orientation (no added roll)."""
        r0 = self.rest[name].to_3x3()
        q = r0.col[1].rotation_difference(Vector(direction).normalized())
        m = (q.to_matrix() @ r0).to_4x4()
        m.translation = Vector(head)
        self.rig.pose.bones[name].matrix = m
        self.update()

    def orient(self, name, head, rotation):
        """Place a bone at head with an explicit armature-space rotation applied
        to its rest orientation (used for paws: heading then pitch, no roll)."""
        m = (rotation @ self.rest[name].to_3x3()).to_4x4()
        m.translation = Vector(head)
        self.rig.pose.bones[name].matrix = m
        self.update()

    def rotate_armature(self, name, axis, degrees):
        """Rotate a posed bone about its head around an armature-space axis."""
        pb = self.rig.pose.bones[name]
        h = pb.head.copy()
        r = Matrix.Rotation(math.radians(degrees), 4, Vector(axis))
        pb.matrix = Matrix.Translation(h) @ r @ Matrix.Translation(-h) @ pb.matrix
        self.update()

    def leg(self, side, ball, foot_pitch=0., toe_pitch=0., pole=(1, 0, 0), heading=0.):
        """Plant one leg. ball: armature-space target for the toes_ head.
        foot_pitch: degrees about Y lifting the heel (hock) around the ball joint;
        with toe_pitch = -foot_pitch the toes stay flat and planted (toe roll).
        toe_pitch: degrees the toes droop (+) or lift (-) in source space.
        heading: degrees the paw is turned about Z (turn-in-place steps).
        The knee bends toward pole (forward, turned half the paw heading).
        Returns reach ratio d/(l1+l2)."""
        s = side
        hip = self.head(f'thigh_{s}')
        l1, l2 = self.length(f'thigh_{s}'), self.length(f'calf_{s}')
        ball = Vector(ball)
        foot_vec = self.rest_head[f'toes_{s}'] - self.rest_head[f'foot_{s}']
        toe_vec = self.rest_tail[f'toes_{s}'] - self.rest_head[f'toes_{s}']
        turn = Matrix.Rotation(math.radians(heading), 3, 'Z')
        pitch = turn @ Matrix.Rotation(math.radians(foot_pitch), 3, 'Y')
        hock = ball - pitch @ foot_vec
        span = hock - hip
        d = span.length
        ratio = d / (l1 + l2)
        d = max(abs(l1 - l2) + .01, min(l1 + l2 - .01, d))
        u = span.normalized()
        hock = hip + u * d
        p = Matrix.Rotation(math.radians(heading / 2), 3, 'Z') @ Vector(pole)
        p = (p - u * p.dot(u)).normalized()
        along = (l1 * l1 - l2 * l2 + d * d) / (2 * d)
        knee = hip + u * along + p * math.sqrt(max(0., l1 * l1 - along * along))
        self.aim(f'thigh_{s}', hip, knee - hip)
        self.aim(f'calf_{s}', knee, hock - knee)
        ball = hock + pitch @ foot_vec
        # Paws get explicit heading-then-pitch rotations: no roll, so a turned
        # paw stays flat across its width.
        self.orient(f'foot_{s}', hock, pitch)
        self.orient(f'toes_{s}', ball, turn @ Matrix.Rotation(math.radians(toe_pitch), 3, 'Y'))
        # Helper goal follows the solved hock so clips carry goal data.
        goal = self.rig.pose.bones[f'ik_foot_{s}']
        goal.location = self.rest[f'ik_foot_{s}'].to_3x3().inverted() @ (hock - self.rest_head[f'ik_foot_{s}'])
        return ratio

    def clear_ground(self, chain, radius, margin=.3):
        """Lift a hanging chain (e.g. the tail) so its surface stays above Z=0:
        each bone is pitched up about its head, around a horizontal
        armature-space axis, until its tail clears radius+margin.
        radius: {bone: (head_r, tail_r)}."""
        for name in chain:
            pb = self.rig.pose.bones[name]
            target = radius[name][1] + margin
            if pb.tail.z >= target: continue
            d = pb.tail - pb.head
            axis = Vector((0, 0, 1)).cross(d)
            if axis.length < 1e-6: continue
            axis.normalize()
            # Pitch so the tail sits at the target height (clamped reach).
            want = max(-1., min(1., (target - pb.head.z) / d.length))
            now = max(-1., min(1., d.z / d.length))
            delta = math.degrees(math.asin(want) - math.asin(now))
            self.rotate_armature(name, axis, -delta)
            if pb.tail.z < target - 1e-3:  # axis sign guard
                self.rotate_armature(name, axis, 2 * delta)

    def arm(self, side, wrist, pole=None):
        """Two-bone arm IK: place the hand_ head (wrist) at an armature-space
        target with the elbow bent toward pole (default: outward and back).
        Returns reach ratio d/(l1+l2)."""
        s = side
        sign = 1 if s == 'L' else -1
        shoulder = self.head(f'upperarm_{s}')
        l1, l2 = self.length(f'upperarm_{s}'), self.length(f'lowerarm_{s}')
        span = Vector(wrist) - shoulder
        d = span.length
        ratio = d / (l1 + l2)
        d = max(abs(l1 - l2) + .01, min(l1 + l2 - .01, d))
        u = span.normalized()
        p = Vector(pole) if pole else Vector((-.35, sign, -.2))
        p = (p - u * p.dot(u)).normalized()
        along = (l1 * l1 - l2 * l2 + d * d) / (2 * d)
        elbow = shoulder + u * along + p * math.sqrt(max(0., l1 * l1 - along * along))
        wrist = shoulder + u * d
        self.aim(f'upperarm_{s}', shoulder, elbow - shoulder)
        self.aim(f'lowerarm_{s}', elbow, wrist - elbow)
        self.aim(f'hand_{s}', wrist, wrist - elbow)  # hand continues the forearm line
        return ratio

    def hand_goal(self, side):
        goal = self.rig.pose.bones[f'ik_hand_{side}']
        goal.location = self.rest[f'ik_hand_{side}'].to_3x3().inverted() @ (
            self.head(f'hand_{side}') - self.rest_head[f'ik_hand_{side}'])

    def key_all(self, frame):
        for pb in self.rig.pose.bones:
            pb.keyframe_insert('location', frame=frame)
            pb.keyframe_insert('rotation_quaternion', frame=frame)
