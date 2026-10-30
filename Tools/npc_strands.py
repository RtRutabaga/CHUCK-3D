"""Strand hair for the humans and heroes (user 2026-10-28: "for all haired, make hair and
beard-hair look better"; "if theres any free resources for hair use whatever looks helpful").

Imported by Tools/build_npc_humans.py (and so the heroes' builder) for a spec's `groom` list.
The styles are the MakeHuman community's hair editor pack (CC0, Tomas Klecer: hair.blend,
geometry-node hair curves authored on the MakeHuman base mesh; Tools/Get-HairPack.ps1 fetches
it into Local/HairPack). Each style's guide curves are carried from the base mesh onto this
body by their surface UV (MPFB's own Surface Deform needs the base mesh's rest shape, which
the kit bakes away), its geometry nodes (interpolation, clumping, frizz) evaluated, and the
strands then fitted to the character:
  - resampled to a few points each (UE renders every point);
  - roots kept where the spec says (a bald man's fringe, a receding hairline, never the ears);
  - beards `fill` the kit's beard volume (make_beard: the shape each NPC was given, its
    mouth already cleared): strands are lengthened, hang under gravity, rest on the chest
    or armour, and end where they leave that volume, so each beard keeps its silhouette;
  - nothing crosses the mouth (Mouth.gap: cut at the lips);
  - hair under headwear (cap, helmet, hat, kerchief) is pressed flat under it;
  - nothing inside the body or the clothes.
Each strand belongs to one bone: the jaw (a beard below the lips opens with the mouth) or the
head (braids too, 2026-10-29: on the upper spine a braid came away from a turning head). The runtime attaches each group's groom to
its bone rigidly (the NPCs are posed with a PoseableMeshComponent, which a groom binding
cannot follow; Source/Chuck3D/NPCGroom.cpp).
"""
from pathlib import Path
import math
import random
import bmesh
import bpy
import numpy as np
from mathutils import Vector, Matrix
from mathutils.bvhtree import BVHTree
from mathutils.kdtree import KDTree
from mathutils.geometry import barycentric_transform

ROOT = Path(__file__).resolve().parents[1]
PACK = ROOT / 'Local/HairPack/hair/haireditor/hair.blend'
DOWN = Vector((0., 0., -1.))
clamp = lambda x: min(1., max(0., x))
smooth = lambda a, b, x: (lambda t: t * t * (3 - 2 * t))(clamp((x - a) / (b - a)))


def store_rest(body):
    """Before the kit bakes the shape targets: the unmorphed base mesh (the Basis key) as
    `rest_position`, which carries the pack's guides from the base mesh to this body."""
    keys = body.data.shape_keys
    if not keys: return
    att = body.data.attributes.get('rest_position') or body.data.attributes.new('rest_position', 'FLOAT_VECTOR', 'POINT')
    att.data.foreach_set('vector', [c for k in keys.key_blocks[0].data for c in k.co])


def merged_bvh(objs):
    """One BVH (world space) over several evaluated meshes."""
    dg = bpy.context.evaluated_depsgraph_get()
    bm = bmesh.new()
    for o in objs:
        me = o.evaluated_get(dg).to_mesh()
        part = bmesh.new(); part.from_mesh(me); part.transform(o.matrix_world)
        offset = len(bm.verts)
        for v in part.verts: bm.verts.new(v.co)
        bm.verts.ensure_lookup_table()
        for f in part.faces:
            try: bm.faces.new([bm.verts[offset + v.index] for v in f.verts])
            except ValueError: pass
        part.free(); o.evaluated_get(dg).to_mesh_clear()
    bm.normal_update()
    tree = BVHTree.FromBMesh(bm)
    bm.free()
    return tree


class Surface:
    """The body's UV layout: a point by its UV on the base mesh (rest) and on this body."""
    def __init__(self, body):
        me = body.data
        me.calc_loop_triangles()
        M = body.matrix_world
        self.rest = [M @ Vector(a.vector) for a in me.attributes['rest_position'].data]
        self.cur = [M @ v.co for v in me.vertices]
        uv = me.uv_layers['UVMap'].data
        self.tris = list(me.loop_triangles)
        self.uvs = [Vector((*uv[l].uv, 0.)) for t in self.tris for l in t.loops]
        self.tree = BVHTree.FromPolygons(self.uvs, [(3 * i, 3 * i + 1, 3 * i + 2) for i in range(len(self.tris))])

    def locate(self, u, v):
        hit, _, i, _ = self.tree.find_nearest(Vector((u, v, 0.)))
        a, b, c = self.uvs[3 * i:3 * i + 3]
        return self.tris[i], barycentric_transform(hit, a, b, c, Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1)))

    def frame(self, co, tri, w):
        vs = [co[i] for i in tri.vertices]
        return vs[0] * w[0] + vs[1] * w[1] + vs[2] * w[2], (vs[1] - vs[0]).cross(vs[2] - vs[0]).normalized()


def load_style(name):
    if not PACK.exists():
        raise RuntimeError(f'Hair pack missing: {PACK}. Run Tools/Get-HairPack.ps1 (MakeHuman hair editor pack, CC0).')
    with bpy.data.libraries.load(str(PACK), link=False) as (src, dst):
        if name not in src.objects: raise ValueError(f'No hair style {name} in the pack')
        dst.objects = [name]
    obj = dst.objects[0]
    bpy.context.collection.objects.link(obj)
    return obj


def set_inputs(obj, overrides):
    """{"Set Hair Curve Profile/Length": 2.} -> that modifier's input (by modifier or node group name prefix, input name)."""
    for key, value in (overrides or {}).items():
        mod_name, socket = key.split('/')
        mods = [m for m in obj.modifiers if m.type == 'NODES' and m.node_group and
                (m.name.startswith(mod_name) or m.node_group.name.startswith(mod_name))]
        if not mods: raise ValueError(f'{obj.name}: no modifier {mod_name}')
        for m in mods:
            ident = next((it.identifier for it in m.node_group.interface.items_tree
                          if getattr(it, 'in_out', '') == 'INPUT' and it.name == socket), None)
            if ident is None: raise ValueError(f'{obj.name}/{m.name}: no input {socket}')
            m[ident] = value
            m.node_group.interface_update(bpy.context)


def transplant(obj, body, surf, head_scale):
    """Each guide from the base mesh to this body: its root to the same UV on this body, the
    rest of it turned with the surface there and scaled with the head."""
    cv = obj.data
    suv = cv.attributes['surface_uv_coordinate'].data
    pos = [p.position.copy() for p in cv.points]
    Mo = obj.matrix_world
    out = list(pos)
    for ci, c in enumerate(cv.curves):
        tri, w = surf.locate(*suv[ci].vector)
        pr, nr = surf.frame(surf.rest, tri, w)
        pd, nd = surf.frame(surf.cur, tri, w)
        R = nr.rotation_difference(nd).to_matrix()
        root = Mo @ pos[c.first_point_index]
        for k in range(c.points_length):
            j = c.first_point_index + k
            out[j] = pd + R @ ((Mo @ pos[j]) - root) * head_scale
    obj.matrix_world = Matrix.Identity(4)
    cv.points.foreach_set('position', [x for p in out for x in p])
    for m in list(obj.modifiers):
        if m.name == 'Surface Deform': obj.modifiers.remove(m)
    cv.surface = body
    cv.update_tag()


def evaluated_strands(obj):
    """[(points (k,3) array, radii (k,) array)] of the evaluated style."""
    ev = obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
    cv = ev.data
    n = len(cv.points)
    pos = np.zeros(n * 3, np.float32); cv.attributes['position'].data.foreach_get('vector', pos)
    pos = pos.reshape(-1, 3)
    rad = np.full(n, .0001, np.float32)
    if 'radius' in cv.attributes: cv.attributes['radius'].data.foreach_get('value', rad)
    off = np.zeros(len(cv.curves) + 1, np.int32); cv.curve_offset_data.foreach_get('value', off)
    M = np.array(ev.matrix_world, np.float32)
    pos = pos @ M[:3, :3].T + M[:3, 3]
    return [(pos[a:b].copy(), rad[a:b].copy()) for a, b in zip(off[:-1], off[1:]) if b - a >= 2]


def resample(pts, rad, n):
    seg = np.linalg.norm(np.diff(pts, axis=0), axis=1)
    s = np.concatenate([[0.], np.cumsum(seg)])
    if s[-1] < 1e-5: return None, None, 0.
    t = np.linspace(0., s[-1], n)
    return (np.stack([np.interp(t, s, pts[:, i]) for i in range(3)], 1), np.interp(t, s, rad), float(s[-1]))


class Groomer:
    def __init__(self, kit, info, meshes, beard=None):
        self.kit, self.info = kit, info
        body = info.obj
        self.surf = Surface(body)
        heads = [i for i in range(len(info.co)) if info.bone[i] == kit.Body.HEAD]
        span = lambda co: (max(co[i].z for i in heads) - min(co[i].z for i in heads))
        self.head_scale = span(self.surf.cur) / span(self.surf.rest)
        _, _, self.head_y = kit.face_marks(info)
        self.centre = Vector((0., self.head_y, info.eye_z + .01))
        names = lambda o: o.name.split('.')[0]
        hair_like = ('beard', 'beard_strands', 'hair', 'hair_strands', 'braid', 'fringe', 'scalp', 'beard_rings')
        mouth_like = {'Eyes', 'Teeth', 'Tongue', 'Eyebrows', 'Eyelashes'}
        above_neck = lambda o: min((o.matrix_world @ Vector(c)).z for c in o.bound_box) > info.neck.z
        self.covers = [o for o in meshes if any(k in names(o).lower() for k in ('cap', 'helmet', 'hat', 'kerchief', 'hood', 'nasal'))
                       or (names(o) == 'trim' and above_neck(o))]   # a cap's rolled cuff
        solid = [o for o in meshes if o.type == 'MESH' and names(o) not in hair_like and o not in self.covers
                 and not any(o.name.startswith(k) for k in mouth_like)]
        self.solid = merged_bvh(solid)
        self.skin = merged_bvh([body])
        self.cover = merged_bvh(self.covers) if self.covers else None
        self.beard = merged_bvh([beard]) if beard else None
        self.beard_obj = beard
        self.cloud = None
        self.mouth = kit.Mouth(info)
        # Weights of each body vertex for the bone split.
        self.kd = KDTree(len(info.co))
        for i, c in enumerate(info.co): self.kd.insert(c, i)
        self.kd.balance()
        jaw = body.vertex_groups.get('jaw')
        self.jaw = {}
        if jaw:
            for v in body.data.vertices:
                for e in v.groups:
                    if e.group == jaw.index: self.jaw[v.index] = e.weight
        g = body.vertex_groups.get('ears')
        ears = [info.co[v.index] for v in body.data.vertices if g and any(x.group == g.index and x.weight > .5 for x in v.groups)]
        self.ears = KDTree(max(1, len(ears)))
        for k, c in enumerate(ears): self.ears.insert(c, k)
        self.ears.balance()
        self.ear_front = min((c.y for c in ears), default=self.head_y)
        self.ear_back = max((c.y for c in ears), default=self.head_y)
        self.has_ears = bool(ears)
        print('CHUCK_GROOM_CONTEXT', f'head_scale={self.head_scale:.3f}', f'covers={[o.name for o in self.covers]}', f'solid={len(solid)}', f'beard={bool(beard)}')

    # Tests ---------------------------------------------------------------
    def inside_beard(self, p):
        hit, n, _, d = self.beard.find_nearest(p)
        return hit is not None and (p - hit).dot(n) < 0.

    def covered(self, p):
        if not self.cover: return False
        d = p - self.centre
        if d.length < 1e-4: return True
        return self.cover.ray_cast(p, d.normalized(), .12)[0] is not None

    def cover_gap(self, p):
        """The room between the skin and the headwear over p, measured out from the skin below it
        (None where nothing covers it). From the skin, not from p: a point already through the cap
        (strands grow longer than the room under it) is still under it."""
        if not self.cover: return None
        hit, n, _, _ = self.skin.find_nearest(p, .1)
        if hit is None: return None
        q = hit + n * .0005
        d = q - self.centre
        if d.length < 1e-4: return None
        loc, _, _, dist = self.cover.ray_cast(q, d.normalized(), .08)
        return dist if loc is not None else None

    def snug(self, strands, gap):
        """A ponytail's strands moved together so its tie sits `gap` off the skull: the pack's tails
        are tied out where a full head of hair would end, which leaves a hollow over a close comb."""
        if self.tie is None: return strands
        hit, n, _, _ = self.skin.find_nearest(self.tie, .2)
        if hit is None: return strands
        shift = (hit + n * gap) - self.tie
        self.tie = self.tie + shift
        print('CHUCK_GROOM_SNUG', f'moved_mm={shift.length * 1000:.1f}')
        return [([self.push_out(p + shift, .002) for p in P], rad) for P, rad in strands]

    def push_out(self, p, gap=.0015):
        for tree in (self.solid,):
            hit, n, _, d = tree.find_nearest(p, .05)
            if hit is not None and (p - hit).dot(n) < gap: p = hit + n * gap
        return p

    def keep_root(self, p, entry, rnd):
        info, kit = self.info, self.kit
        if self.has_ears and self.ears.find(p)[2] < entry.get('ear_clear', .008): return False
        band = entry.get('fringe')
        if band:   # a bald man's horseshoe of hair round the back and over the ears (make_fringe's band)
            if p.y < self.ear_front + .015 or not (info.eye_z - band['low'] < p.z < info.eye_z + band['high']): return False
            if p.z < info.eye_z - .015 and p.y < self.ear_front + .05: return False
            top = (p.z - (info.eye_z - band['low'])) / (band['low'] + band['high'])
            if rnd.random() > 1.15 - top: return False      # thinning toward its top edge: no hard line round the head
        if 'hairline' in entry:   # no hair in front of this hairline (the gnome's receding one)
            # Behind the ears the hair goes down to the nape (make_hair's cap line; 2026-10-29: the front
            # hairline alone, applied all round, left the back of the head bare below the eyes).
            line = kit.front_hairline(info, p, entry['hairline'], info.eye_z + .005)
            behind = smooth(self.ear_back + .002, self.ear_back + .03, p.y)
            if p.z < line * (1 - behind) + (info.neck.z + .035) * behind: return False
        if entry.get('below_z') is not None and p.z > info.eye_z + entry['below_z']: return False
        return True

    def bone_of(self, root):
        _, i, _ = self.kd.find(root)
        return 'jaw' if self.jaw.get(i, 0.) > .5 else 'head'

    # One style -----------------------------------------------------------
    def grow(self, entry, rnd):
        obj = load_style(entry['style'])
        transplant(obj, self.info.obj, self.surf, self.head_scale * entry.get('scale', 1.))
        set_inputs(obj, entry.get('set'))
        strands = evaluated_strands(obj)
        mats = list(obj.data.materials)
        bpy.data.objects.remove(obj)
        for m in mats:
            if m and m.users == 0: bpy.data.materials.remove(m)
        fill = entry.get('fill') == 'beard' and self.beard is not None
        n_pts = entry.get('points', 10)
        length = entry.get('length')        # beards: how far a strand may grow (it ends earlier where it leaves the beard)
        width = entry.get('width', 1.)
        out, stats = [], dict(kept=0, roots=0, mouth=0, covered=0, outside=0)
        def finish(pts, rad):
            """One grown strand: resampled, out of the skin and clothes, flat under headwear, clear of the mouth."""
            pts, rad, L = resample(pts, rad, n_pts)
            if pts is None: return
            P = [Vector(p) for p in pts]
            flat = entry.get('under', .004)
            all_covered = True
            for k in range(1, len(P)):
                room = self.cover_gap(P[k])
                if room is not None:   # under the headwear: no higher than half the room under it
                    hit, n, _, d = self.skin.find_nearest(P[k], .1)
                    if hit is not None: P[k] = hit + n * min(max(0., (P[k] - hit).dot(n)), flat, max(.0006, room * .45))
                else: all_covered = False
                P[k] = self.push_out(P[k], .0006 if room is not None else .0015)
            if all_covered and self.cover_gap(P[0]) is not None: stats['covered'] += 1; return
            # Never across the mouth: cut where a strand reaches the lips.
            if entry.get('mouth', fill):
                cut = next((k for k in range(1, len(P)) if self.mouth.in_front(P[k]) and self.mouth.gap(P[k], .001) < 1.), None)
                if cut is not None:
                    if cut < 3: stats['mouth'] += 1; return
                    P, rad = P[:cut], rad[:cut]
            out.append((P, rad * width))
            stats['kept'] += 1

        for pts, rad in strands:
            root = Vector(pts[0])
            if not self.keep_root(root, entry, rnd): stats['roots'] += 1; continue
            if fill and entry.get('mass'):   # a bushy beard: strands fanned out through the whole volume
                copies = entry.get('copies', 1)
                for _ in range(copies):
                    g_pts, g_rad = self.fill_mass(pts, rad, length, entry, rnd)
                    if g_pts is None: stats['outside'] += 1
                    else: finish(g_pts, g_rad)
                continue
            if fill:
                pts, rad = self.fill_beard(pts, rad, length, entry, rnd)
                if pts is None: stats['outside'] += 1; continue
            if entry.get('comb') and self.tie is not None:
                pts, rad = self.comb(root, float(rad[0]), entry['comb'], rnd)
            finish(pts, rad)
        print('CHUCK_GROOM_STYLE', entry['style'], entry['group'], f'evaluated={len(strands)}',
              ' '.join(f'{k}={v}' for k, v in stats.items()))
        return out

    tie = None   # where the last style's strands start (a ponytail's tie): `comb` draws hair back to it

    def comb(self, root, r0, spec, rnd):
        """Hair pulled back from its root over the scalp to the tie (the ranger's ponytail): lying on the
        skin, a little fuller over the crown, gathering at the tie."""
        step, lift = .004, spec.get('lift', .003)
        path, p = [root], root.copy()
        for k in range(120):
            to = self.tie - p
            if to.length < spec.get('gather', .012): break
            hit, n, _, _ = self.skin.find_nearest(p, .1)
            if hit is None: break
            d = to - n * to.dot(n)
            if d.length < step:   # under the tie (it stands off the scalp): up into it
                path.append(self.tie.copy()); break
            off = lift * (1 + .6 * math.sin(math.pi * min(1., k / 25))) * rnd.uniform(.7, 1.3)
            p = hit + d.normalized() * step + n * off
            q, n2, _, _ = self.skin.find_nearest(p, .1)
            if q is not None: p = q + n2 * off
            path.append(p)
        if len(path) < 3: return np.array([tuple(root), tuple(root + Vector((0, 0, .002)))], np.float32), np.array([r0, r0 * .5], np.float32)
        if spec.get('tail'):   # on through the tie and down as the ponytail: the tail is the same hair (2026-10-29)
            path += self.tail(spec['tail'], rnd)
        r = np.array([r0 * (1 - .4 * i / (len(path) - 1)) for i in range(len(path))], np.float32)
        return np.array([tuple(q) for q in path], np.float32), r

    def tail(self, spec, rnd):
        """A ponytail strand from the tie: out from the head a little, then down, in a bundle that fills
        out below the tie and thins to the tip; each strand its own length and place in the bundle."""
        hit, n, _, _ = self.skin.find_nearest(self.tie, .2)
        flat = Vector((n.x, n.y, 0.)) if hit is not None else Vector((0., 1., 0.))
        out = flat.normalized() if flat.length > .1 else Vector((0., 1., 0.))
        side = out.cross(DOWN).normalized()
        full = spec.get('length', .2)
        L = full * rnd.uniform(.7, 1.)
        th, rr = rnd.uniform(0, 2 * math.pi), math.sqrt(rnd.random())
        wob = rnd.uniform(0, 6.28)
        pts, step, k = [], .006, 1
        while k * step <= L:
            t = k * step
            f = t / full
            width = spec.get('tie_radius', .006) + spec.get('radius', .016) * math.sin(math.pi * min(1., f * 1.2) * .85)
            q = th + .25 * math.sin(f * 5 + wob)
            off = (out * math.cos(q) * .6 + side * math.sin(q)) * rr * width
            c = self.tie + out * spec.get('lift', .02) * math.sin(min(1., t / .07) * math.pi / 2) + DOWN * t
            pts.append(self.push_out(c + off, .004))
            k += 1
        return pts

    def beard_cloud(self, rnd):
        """Points filling the beard volume (`mass` strands end among them)."""
        if self.cloud is None:
            M = self.beard_obj.matrix_world
            box = [M @ Vector(c) for c in self.beard_obj.bound_box]
            lo = Vector([min(c[i] for c in box) for i in range(3)]); hi = Vector([max(c[i] for c in box) for i in range(3)])
            pts = []
            for _ in range(60000):
                p = Vector([rnd.uniform(lo[i], hi[i]) for i in range(3)])
                if self.inside_beard(p): pts.append(tuple(p))
                if len(pts) >= 8000: break
            self.cloud = np.array(pts, np.float32)
            print('CHUCK_GROOM_BEARD_CLOUD', len(pts), f'box_cm={[round((hi[i] - lo[i]) * 100, 1) for i in range(3)]}')
        return self.cloud

    def fill_mass(self, pts, rad, length, entry, rnd):
        """A bushy beard's strand (the dwarf, user 2026-10-29: "a big bushy beard like before"): from its
        root out and down to a point somewhere in the beard volume, bowed outward, so the strands fill the
        volume's depth rather than lying on the chest; a wave of its own, out of the body and clothes."""
        P0, P1 = Vector(pts[0]), Vector(pts[1])
        if not self.inside_beard(P0 + (P1 - P0).normalized() * .002): return None, None
        C = self.beard_cloud(rnd)
        drop = P0.z - C[:, 2]
        flat = np.hypot(C[:, 0] - P0.x, C[:, 1] - P0.y)
        ok = np.nonzero((drop > .012) & (flat < .025 + drop * entry.get('spread', .9)) &
                        (np.sqrt(drop ** 2 + flat ** 2) < (length or .2)))[0]
        if not len(ok): return None, None
        picks = [ok[rnd.randrange(len(ok))] for _ in range(2)]
        T = Vector(C[min(picks, key=lambda i: C[i, 2])] if rnd.random() < entry.get('low', .5) else C[picks[0]])
        hit, n, _, _ = self.skin.find_nearest(P0, .05)
        out = (n if hit is not None else (P1 - P0)).normalized()
        L = (T - P0).length
        c1 = P0 + (out * .7 + (P1 - P0).normalized() * .3).normalized() * L * entry.get('bow', .45)
        steps = max(4, int(L / .004))
        phase, freq, amp = rnd.uniform(0, 6.28), rnd.uniform(25., 60.), entry.get('wave', .35) * rnd.uniform(.4, 1.) * .006
        side = (T - P0).cross(out)
        side = side.normalized() if side.length > 1e-6 else Vector((1., 0., 0.))
        path = [P0]
        for k in range(1, steps + 1):
            t = k / steps
            b = P0 * (1 - t) ** 2 + c1 * 2 * t * (1 - t) + T * t * t
            b = b + side * amp * math.sin(phase + t * L * freq) * min(1., t * 4)
            p = self.push_out(b, .003)
            path.append(p)
        stray = rnd.uniform(0., entry.get('stray', .008))
        if stray > .002:   # a little past its end, on the way it was going
            d = (path[-1] - path[-2]).normalized()
            path.append(self.push_out(path[-1] + d * stray, .003))
        r0 = float(rad[0])
        r = np.array([r0 * (1 - .7 * (i / (len(path) - 1)) ** 2) for i in range(len(path))], np.float32)
        return np.array([tuple(p) for p in path], np.float32), r

    def fill_beard(self, pts, rad, length, entry, rnd):
        """A beard strand: the pack's start (how it grows from the skin), then on down under
        gravity, resting on the chest, to where it leaves the beard volume (with a little
        stray length past it), at most `length` long."""
        P = [Vector(p) for p in pts]
        if not self.inside_beard(P[0] + (P[1] - P[0]).normalized() * .002): return None, None
        step = .004
        start = P[1] - P[0]
        # The pack's first couple of centimetres set the direction it leaves the skin.
        path, s, k = [P[0]], 0., 1
        while k < len(P) and s < entry.get('lead', .015):
            s += (P[k] - path[-1]).length; path.append(P[k]); k += 1
        d = (path[-1] - path[-2]).normalized() if len(path) > 1 else start.normalized()
        bend = entry.get('bend', .03)
        limit = (length or .1) * rnd.uniform(.75, 1.)
        stray = rnd.uniform(0., entry.get('stray', .006))
        out_for = 0.
        # A slow wave of its own (beard hair is never straight): a little sideways and in and out.
        phase, freq, amp = rnd.uniform(0, 6.28), rnd.uniform(25., 60.), entry.get('wave', .35) * rnd.uniform(.4, 1.)
        side = DOWN.cross(Vector((0., -1., 0.)))
        while s < limit:
            w = smooth(0., bend, s)
            d = (d * (1 - .25 * w) + DOWN * .25 * w).normalized()
            wv = amp * w * .25
            d = (d + side * wv * math.sin(phase + s * freq) + Vector((0., wv * .6, 0.)) * math.cos(phase * 1.7 + s * freq * .8)).normalized()
            p = self.push_out(path[-1] + d * step, .003)
            d = (p - path[-1]).normalized()
            path.append(p); s += step
            if not self.inside_beard(p):
                out_for += step
                if out_for > stray: break
        if len(path) < 3: return None, None
        r0 = float(rad[0])
        r = np.array([r0 * (1 - .7 * (i / (len(path) - 1)) ** 2) for i in range(len(path))], np.float32)
        return np.array([tuple(p) for p in path], np.float32), r

    def braid(self, paths, entry, rnd):
        """A braid of strands: each of its three plaits (make_braid's paths: points and radii)
        filled with strands lying along it."""
        out = []
        per = entry.get('per_plait', 220)
        for pts, radii in paths:
            P = [Vector(p) for p in pts]
            frames = []
            for k in range(len(P)):
                t = (P[min(k + 1, len(P) - 1)] - P[max(k - 1, 0)]).normalized()
                a = t.cross(Vector((1, 0, 0)))
                if a.length < .1: a = t.cross(Vector((0, 1, 0)))
                a.normalize(); frames.append((a, t.cross(a).normalized()))
            for _ in range(per):
                rr, th = math.sqrt(rnd.random()) * .85, rnd.uniform(0, 2 * math.pi)
                wob = rnd.uniform(0, 6.28)
                strand = []
                for k, (c, r) in enumerate(zip(P, radii)):
                    a, b = frames[k]
                    q = th + .15 * math.sin(k * .3 + wob)
                    strand.append(c + (a * math.cos(q) + b * math.sin(q)) * rr * r)
                # Past the tie, a loose tuft: on down, spreading a little, each its own length.
                tuft = entry.get('tuft', .045) * rnd.uniform(.6, 1.)
                if tuft > 0:
                    a, b = frames[-1]
                    t = (P[-1] - P[-2]).normalized()
                    end, r = strand[-1], radii[-1]
                    for j in range(1, 6):
                        f = j / 5
                        strand.append(end + t * tuft * f + (a * math.cos(th) + b * math.sin(th)) * rr * r * 1.4 * f + DOWN * .004 * f * f)
                pts_a = np.array([tuple(p) for p in strand], np.float32)
                rad = np.full(len(strand), entry.get('radius', .00008), np.float32)
                p2, r2, _ = resample(pts_a, rad, entry.get('points', 24))
                out.append(([Vector(p) for p in p2], r2))
        return out


def make_grooms(kit, info, spec_groom, meshes, beard=None, braids=None):
    """Grooms from a spec's `groom` list: {group: {'bone', 'strands': [(points, radii)], 'colour', ...}}."""
    g = Groomer(kit, info, meshes, beard)
    rnd = random.Random(29)
    groups = {}
    for entry in spec_groom:
        if entry.get('comb', {}).get('to') == 'braid' and braids:   # hair combed back into the braid's top
            g.tie = sum((Vector(pts[0]) for pts, _ in braids), Vector()) / len(braids)
        if entry.get('tie_only'):   # a style that only places the tie (its strands are left out): a later comb gathers there
            got = g.grow(entry, rnd)
            if got:
                n = min(len(P) for P, _ in got)
                best = None
                for k in range(1, n):
                    c = sum((P[k] for P, _ in got), Vector()) / len(got)
                    spread = sum((P[k] - c).length for P, _ in got) / len(got)
                    if best is None or spread < best[0]: best = (spread, c)
                g.tie = best[1] if best else None
                if entry.get('snug'): g.snug(got, entry['snug'])
            continue
        if entry.get('style') == 'braid':
            # On the head (2026-10-29): on the spine it stayed put as the head turned, and came away from it.
            got = g.braid(braids or [], entry, rnd)
            bone_of = lambda root: entry.get('bone', 'head')
        else:
            got = g.grow(entry, rnd)
            bone_of = (lambda root: 'head') if entry.get('bone') == 'head' else g.bone_of
        if got and entry.get('style') != 'braid' and not entry.get('comb'):   # where its strands gather tightest (a ponytail's tie): a later `comb` entry draws hair to it
            n = min(len(P) for P, _ in got)
            best = None
            for k in range(1, n):
                c = sum((P[k] for P, _ in got), Vector()) / len(got)
                spread = sum((P[k] - c).length for P, _ in got) / len(got)
                if best is None or spread < best[0]: best = (spread, c)
            g.tie = best[1] if best else None
            if entry.get('snug'): got = g.snug(got, entry['snug'])
        for P, rad in got:
            bone = bone_of(P[0])
            name = entry['group'] + ('Jaw' if bone == 'jaw' else 'Back' if bone.startswith('spine') else '')
            grp = groups.setdefault(name, {'bone': bone, 'strands': [], 'material': entry.get('material', entry['group'])})
            grp['strands'].append((P, rad))
    for name, grp in groups.items():
        print('CHUCK_GROOM_GROUP', name, grp['bone'], f'strands={len(grp["strands"])}', f'points={sum(len(p) for p, _ in grp["strands"])}')
    return groups


def roots_tree(groups, names=None):
    """A KD tree of the roots of the named groups (the scalp's stubble fades out past them)."""
    roots = [P[0] for n, grp in groups.items() if names is None or n in names for P, _ in grp['strands']]
    kd = KDTree(max(1, len(roots)))
    for k, c in enumerate(roots): kd.insert(c, k)
    kd.balance()
    return kd, len(roots)


def export_groom(path, strands, M, scale):
    """Alembic for Unreal's groom importer (Tools/import_npc_grooms.py): in the FBX's space
    (M: the kit's final placement), centimetres (Chuck's grooms are authored in cm)."""
    counts = [len(P) for P, _ in strands]
    cv = bpy.data.hair_curves.new(path.stem)
    cv.add_curves(counts)
    cv.set_types(type='POLY')
    pos = [x for P, _ in strands for p in P for x in (M @ p)]
    cv.points.foreach_set('position', pos)
    cv.points.foreach_set('radius', [float(r) * scale * 100. for _, rad in strands for r in rad])   # cm: global_scale moves the points only
    obj = bpy.data.objects.new(path.stem, cv)
    bpy.context.collection.objects.link(obj)
    for o in bpy.context.scene.objects: o.select_set(False)
    obj.select_set(True); bpy.context.view_layer.objects.active = obj
    path.parent.mkdir(parents=True, exist_ok=True)
    # Written beside it and swapped in, retried: overwriting a large .abc in place sometimes failed
    # (another process briefly holding the old file), leaving the old one silently.
    import os, time
    tmp = path.with_name(path.stem + '.new.abc')
    for attempt in range(6):
        if tmp.exists(): tmp.unlink()
        bpy.ops.wm.alembic_export(filepath=str(tmp), selected=True, start=0, end=0, export_hair=True, export_particles=False,
                                  uvs=False, normals=False, face_sets=False, global_scale=100.)
        try:
            if tmp.exists() and tmp.stat().st_size > 0:
                os.replace(tmp, path); break
        except OSError as e:
            print('CHUCK_GROOM_EXPORT_RETRY', path.name, e)
        time.sleep(2.)
    else:
        raise RuntimeError(f'Could not write {path}')
    roots = [M @ P[0] for P, _ in strands]
    bpy.data.objects.remove(obj); bpy.data.hair_curves.remove(cv)
    # Unreal component space (cm; the FBX mirrors Y), for the importer's check: the roots' mean and box
    # (Unreal reorders the curves on import, so not strand by strand).
    ue = [(r.x * 100, -r.y * 100, r.z * 100) for r in roots]
    mean = [round(sum(v[i] for v in ue) / len(ue), 2) for i in range(3)]
    return {'mean': mean, 'min': [round(min(v[i] for v in ue), 2) for i in range(3)], 'max': [round(max(v[i] for v in ue), 2) for i in range(3)]}
