"""Line up the ship track and its concertina version (Claude, 2026-11-02; docs/SHIP.md, "Below decks and above").

The user: when he comes up on deck the concertina version takes over "at the current part of the song as close as
possible". The two recordings are the same tune, but the concertina's is played about 5% slower, and it leaves out
about 18 s of the ship track's opening and a beat or two at some section joins. So a simple percentage or offset is
wrong by up to a second. This finds, for every moment of each track, the moment of the other that sounds most like it
(harmony over 4 s, then the beats lined up), and writes the two maps and each track's beat grid to
Unreal/Chuck3D/Source/Chuck3D/ShipMusicMap.inl, which ShipCrew.cpp includes.

Needs numpy: run with the Python that ships with Blender (CHUCK-Tools), isolated:
  "%LOCALAPPDATA%/Programs/CHUCK-Tools/blender-4.5.14-windows-x64/4.5/python/bin/python.exe" -I Tools/align_ship_music.py [--demo]
--demo also writes Local/ShipMusicSwitch/*.wav: the ship track crossing into the concertina at a few moments, to listen to.
"""
import sys, wave
from pathlib import Path
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
SHIP = ROOT / 'SourceAssets/Audio/Ship.wav'
CONCERTINA = ROOT / 'SourceAssets/Audio/ShipConcertina.wav'
OUT = ROOT / 'Unreal/Chuck3D/Source/Chuck3D/ShipMusicMap.inl'
FPS = 20                 # harmony frames per second
WIN = 2.0                # +- seconds of harmony compared at each moment
JUMP = 30.0              # what it costs the path to jump elsewhere in the other track (in matched frames' similarity):
                         # only a clearly better match held for many seconds (where the concertina leaves something out)
BEND = .02               # what it costs to hold or skip a frame (the tempos differ by about 5%)
ONSET_HOP = 256


def load(p):
    w = wave.open(str(p)); sr = w.getframerate()
    x = np.frombuffer(w.readframes(w.getnframes()), np.int16).reshape(-1, w.getnchannels()).astype(np.float32).mean(1) / 32768.
    return x, sr


def chroma(x, sr):
    hop = sr // FPS; N = 8192
    frames = 1 + (len(x) - N) // hop
    win = np.hanning(N).astype(np.float32)
    f = np.fft.rfftfreq(N, 1 / sr); ok = (f > 60) & (f < 2000)
    pc = (np.round(12 * np.log2(f[ok] / 440.)) % 12).astype(int)
    C = np.zeros((frames, 12), np.float32)
    for s in range(0, frames, 512):
        idx = np.arange(N)[None, :] + hop * np.arange(s, min(frames, s + 512))[:, None]
        S = np.abs(np.fft.rfft(x[idx] * win, axis=1))[:, ok] ** 2
        for k in range(12): C[s:s + len(idx), k] = S[:, pc == k].sum(1)
    C = np.sqrt(C)
    return C / (np.linalg.norm(C, axis=1, keepdims=True) + 1e-9)


def onset(x, sr):
    N = 1024; frames = 1 + (len(x) - N) // ONSET_HOP
    idx = np.arange(N)[None, :] + ONSET_HOP * np.arange(frames)[:, None]
    S = np.log1p(100 * np.abs(np.fft.rfft(x[idx] * np.hanning(N), axis=1)))
    o = np.concatenate([[0], np.maximum(0, np.diff(S, axis=0)).sum(1)])
    o = np.maximum(o - np.convolve(o, np.ones(64) / 64, 'same'), 0)
    return o / (o.std() + 1e-9)


def beat_grid(o, fps, lo, hi):
    t = np.arange(len(o)) / fps
    bpm = max(((abs((o * np.exp(-2j * np.pi * t * b / 60)).sum()), b) for b in np.arange(lo, hi, .005)))[1]
    z = (o * np.exp(-2j * np.pi * t * bpm / 60)).sum()
    return bpm, (-np.angle(z) / (2 * np.pi)) % 1 * 60 / bpm


def best_path(cs, ct):
    """For each frame of the source, the frame of the target that sounds most like it, keeping to a continuous run
    where it can and jumping only when another place matches clearly better for a while."""
    sim = cs @ ct.T
    W = int(WIN * FPS); n, m = sim.shape
    pad = np.pad(sim, W, constant_values=np.nan)
    E = np.zeros_like(sim); cnt = np.zeros_like(sim)
    for k in range(-W, W + 1):
        s = pad[W + k:W + k + n, W + k:W + k + m]
        ok = ~np.isnan(s); E += np.where(ok, s, 0); cnt += ok
    E /= np.maximum(cnt, 1)
    V = E[0] - JUMP * (np.arange(m) > FPS); back = np.zeros((n, m), np.int32)   # both begin at the beginning
    ar = np.arange(m)
    for t in range(1, n):
        diag = np.concatenate([[-1e9], V[:-1]])
        hold = V - BEND
        skip = np.concatenate([[-1e9, -1e9], V[:-2]]) - BEND
        j = int(np.argmax(V)); jump = V[j] - JUMP
        cand = np.stack([diag, hold, skip, np.full(m, jump)])
        k = np.argmax(cand, 0)
        prev = np.choose(k, [ar - 1, ar, ar - 2, np.full(m, j)])
        back[t] = prev; V = cand[k, ar] + E[t]
    path = np.zeros(n, np.int32); path[-1] = int(np.argmax(V))
    for t in range(n - 1, 0, -1): path[t - 1] = back[t, path[t]]
    return path, E[np.arange(n), path]


def refine(path, os_, ot, step=.25):
    """Line the beats up: around each source moment, the target offset (within a frame either way) where the two tracks'
    note onsets agree best."""
    ofps = 48000 / ONSET_HOP; half = int(1.5 * ofps); reach = int(.08 * ofps)
    src = np.arange(0, len(path) / FPS, step); dst = []
    for s in src:
        c = path[min(int(s * FPS), len(path) - 1)] / FPS
        a = int(s * ofps); b = int(c * ofps)
        if a - half < 0 or a + half > len(os_) or b - half - reach < 0 or b + half + reach > len(ot): dst.append(c); continue
        seg = os_[a - half:a + half]
        r = [np.dot(seg, ot[b + d - half:b + d + half]) for d in range(-reach, reach + 1)]
        dst.append((b + int(np.argmax(r)) - reach) / ofps)
    dst = np.array(dst)
    # smooth within runs (a jump is where the offset changes by more than half a second)
    off = dst - src; out = off.copy(); run0 = 0
    for i in range(1, len(off) + 1):
        if i == len(off) or abs(off[i] - off[i - 1]) > .5:
            seg = off[run0:i]
            for k in range(len(seg)):
                out[run0 + k] = np.median(seg[max(0, k - 4):k + 5])
            run0 = i
    return src, src + out


def knots(src, dst, tol=.02):
    """Fewest points that keep the map within tol seconds (jumps kept as a pair of points)."""
    keep = {0, len(src) - 1}
    for i in range(1, len(src)):
        if abs((dst[i] - src[i]) - (dst[i - 1] - src[i - 1])) > .5: keep |= {i - 1, i}
    stack = []
    ks = sorted(keep)
    for a, b in zip(ks[:-1], ks[1:]): stack.append((a, b))
    while stack:
        a, b = stack.pop()
        if b - a < 2: continue
        t = (src[a + 1:b] - src[a]) / (src[b] - src[a])
        err = abs(dst[a + 1:b] - (dst[a] + t * (dst[b] - dst[a])))
        if err.max() > tol:
            k = a + 1 + int(np.argmax(err)); keep.add(k); stack += [(a, k), (k, b)]
    ks = sorted(keep)
    return src[ks], dst[ks]


def main():
    xs, sr = load(SHIP); xc, _ = load(CONCERTINA)
    cs, cc = chroma(xs, sr), chroma(xc, sr)
    os_, oc = onset(xs, sr), onset(xc, sr)
    ofps = sr / ONSET_HOP
    grids = {'Ship': beat_grid(os_, ofps, 134, 138), 'Concertina': beat_grid(oc, ofps, 127, 131)}
    for k, (b, f) in grids.items(): print('CHUCK_MUSIC_GRID %s bpm=%.3f first_beat=%.3f' % (k, b, f))
    maps = {}
    for name, (a, b, oa, ob) in {'ShipToConcertina': (cs, cc, os_, oc), 'ConcertinaToShip': (cc, cs, oc, os_)}.items():
        path, score = best_path(a, b)
        src, dst = refine(path, oa, ob)
        dst = np.maximum(dst, 0.)
        ks, kd = knots(src, dst)
        # Do the beats agree where the map lands? (each track's own grid; 0 = on the same part of the beat)
        gs, gd = (grids['Ship'], grids['Concertina']) if name == 'ShipToConcertina' else (grids['Concertina'], grids['Ship'])
        ph = ((dst - gd[1]) * gd[0] / 60 - (src - gs[1]) * gs[0] / 60 + .5) % 1 - .5
        print('CHUCK_MUSIC_BEATS %s beat_phase_gap median=%.3f within_0.15=%.0f%%' % (name, np.median(abs(ph)), 100 * np.mean(abs(ph) < .15)))
        jumps = int(np.sum(np.abs(np.diff(dst - src)) > .5))
        print('CHUCK_MUSIC_MAP %s knots=%d jumps=%d harmony_match mean=%.3f worst_5pct=%.3f' % (name, len(ks), jumps, score.mean(), np.percentile(score, 5)))
        for i in range(1, len(src)):
            if abs((dst[i] - src[i]) - (dst[i - 1] - src[i - 1])) > .5: print('   jump at %6.2f: %6.2f -> %6.2f' % (src[i], dst[i - 1], dst[i]))
        for s in range(0, int(src[-1]) + 1, 10):
            i = int(np.argmin(abs(src - s))); print('   %6.1f -> %6.2f' % (src[i], dst[i]))
        maps[name] = (ks, kd, src, dst)
    durs = {'Ship': len(xs) / sr, 'Concertina': len(xc) / sr}
    L = ['// Generated by Tools/align_ship_music.py from SourceAssets/Audio/Ship.wav and ShipConcertina.wav. Do not edit.',
         '// For each moment of one track, the moment of the other that sounds most like it (seconds into each), and each',
         '// track\'s beat grid. Included by ShipCrew.cpp.', '#pragma once', '', 'namespace ShipMusicMap', '{']
    for k in ('Ship', 'Concertina'):
        L.append('    constexpr float %sBPM = %.3ff, %sFirstBeat = %.3ff, %sDuration = %.3ff;' % (k, grids[k][0], k, grids[k][1], k, durs[k]))
    for name, (ks, kd, _, _) in maps.items():
        L.append('    // %d points' % len(ks))
        L.append('    constexpr float %s[][2] = {' % name)
        row = []
        for s, d in zip(ks, kd):
            row.append('{%.2ff, %.3ff}' % (s, d))
            if len(row) == 6: L.append('        ' + ', '.join(row) + ','); row = []
        if row: L.append('        ' + ', '.join(row) + ',')
        L.append('    };')
    L += ['}', '']
    OUT.write_text('\n'.join(L), encoding='utf-8')
    print('CHUCK_MUSIC_MAP_WRITTEN', OUT)
    if '--demo' in sys.argv:
        d = ROOT / 'Local/ShipMusicSwitch'; d.mkdir(parents=True, exist_ok=True)
        ws = wave.open(str(SHIP)); wc = wave.open(str(CONCERTINA))
        S = np.frombuffer(ws.readframes(ws.getnframes()), np.int16).reshape(-1, 2).astype(np.float32)
        C = np.frombuffer(wc.readframes(wc.getnframes()), np.int16).reshape(-1, 2).astype(np.float32)
        ks, kd, _, _ = maps['ShipToConcertina']
        for t in (12, 30, 60, 104.5, 150, 190):
            to = float(np.interp(t, ks, kd)); fade = int(1.5 * sr)
            a = S[int((t - 6) * sr):int(t * sr) + fade].copy()
            b = C[int(to * sr):int(to * sr) + int(8 * sr)].copy()
            ramp = np.linspace(1, 0, fade)[:, None]
            mix = np.concatenate([a[:-fade], a[-fade:] * ramp + b[:fade] * (1 - ramp), b[fade:]])
            out = wave.open(str(d / ('ship_%05.1f_to_concertina_%05.1f.wav' % (t, to))), 'wb')
            out.setnchannels(2); out.setsampwidth(2); out.setframerate(sr)
            out.writeframes(np.clip(mix, -32768, 32767).astype(np.int16).tobytes()); out.close()
        print('CHUCK_MUSIC_DEMO', d)


if __name__ == '__main__':
    main()
