"""Blender 4.5.14 (its aud module and numpy): NPC voice lines from SourceAssets/NPCs/dialogue.json
(docs/NPC-VOICE-PLAN.md; user 2026-10-05: the dwarf's ElevenLabs line).

blender --background --factory-startup --python Tools/build_npc_voice.py
  -> SourceAssets/NPCs/Voice/<NPC>/<id>.wav            44.1 kHz mono 16-bit, trimmed to the line, faded in/out 10 ms
     SourceAssets/NPCs/Voice/manifest.json               per line: wav, seconds, peaks
     Unreal/Chuck3D/Source/Chuck3D/NPCVoiceData.h        per line: text, sound asset path, loudness envelope

Every line is levelled to the same speech loudness, SPEECH_DB (RMS over its
voiced 50 ms windows), as far as its peaks allow (PEAK_DB): the ElevenLabs
takes came in 4 dB apart, and the quietest sat barely over the music (user
2026-10-06: "make all npc dialogue louder in relation to the music"; the game
also ducks the music while anyone speaks, ADockGameMode).
The envelope is the line's own loudness at 60 frames a second (RMS over 30 ms,
divided by its 95th percentile, clipped to 1, as 0..255): ADockNPC opens the
jaw, lifts the brows and nods with it while the line plays. (Rhubarb mouth
shapes, phase 2 of the plan, can replace it for NPCs whose mouths show; the
dwarf's beard hides his lips.)
Lines are an NPC's 'talk' lines, then its 'ambient' ones (started by the game,
not by talking: the tavern at night). A line without a 'source' (its audio not
supplied yet) is written text-only: no wav, no asset, an empty envelope and a
length of TEXT_SECONDS_PER_WORD a word, so the game can run it on its subtitles.
"""
from pathlib import Path
import json
import wave
import aud
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
NPCS = ROOT / 'SourceAssets/NPCs'
OUT_H = ROOT / 'Unreal/Chuck3D/Source/Chuck3D/NPCVoiceData.h'
FPS = 60
SPEECH_DB, PEAK_DB = -18., -1.   # the music is about -19 dB RMS at 0.45 (-26 in the mix), ducked further under speech
TEXT_SECONDS_PER_WORD = .38
SPEC = json.loads((NPCS / 'dialogue.json').read_text(encoding='utf-8'))['npcs']


def decode(path):
    s = aud.Sound(str(path))
    rate = int(s.specs[0])
    a = s.data()
    return (a.mean(axis=1) if a.ndim > 1 else a).astype(np.float64), rate


def level(x, rate):
    """Gain (dB) bringing the voiced windows' RMS to SPEECH_DB, held so the peak stays at or under PEAK_DB."""
    win = int(rate * .05)
    rms = np.sqrt((x[:len(x) // win * win].reshape(-1, win) ** 2).mean(axis=1))
    voiced = rms[rms > .01]
    speech = 20 * np.log10(np.sqrt((voiced ** 2).mean())) if len(voiced) else SPEECH_DB
    peak = 20 * np.log10(max(np.abs(x).max(), 1e-9))
    return min(SPEECH_DB - speech, PEAK_DB - peak)


def envelope(x, rate):
    hop, win = rate // FPS, int(rate * .03)
    rms = np.sqrt(np.convolve(x * x, np.ones(win) / win, 'same'))[::hop]
    ref = np.percentile(rms[rms > rms.max() * .05], 95) if (rms > rms.max() * .05).any() else rms.max()
    return np.clip(rms / max(ref, 1e-6), 0, 1)


manifest, entries = {'generator': 'Tools/build_npc_voice.py', 'lines': {}}, []
for npc, spec in SPEC.items():
    for line in spec.get('talk', []) + spec.get('ambient', []):
        if not line.get('source'):
            secs = len(line['text'].split()) * TEXT_SECONDS_PER_WORD
            entries.append((npc, line, '', np.zeros(0, dtype=int), secs))
            print('CHUCK_VOICE_LINE', npc, line['id'], f'seconds={secs:.2f}', 'text_only=1')
            continue
        x, rate = decode(NPCS / line['source'])
        x = x[int(line.get('start', 0) * rate):int(line.get('end', len(x) / rate) * rate)]
        fade = int(rate * .01)
        x[:fade] *= np.linspace(0, 1, fade); x[-fade:] *= np.linspace(1, 0, fade)
        gain_db = level(x, rate)
        x = x * 10 ** (gain_db / 20)
        dst = NPCS / 'Voice' / npc / f"{line['id']}.wav"
        dst.parent.mkdir(parents=True, exist_ok=True)
        with wave.open(str(dst), 'wb') as w:
            w.setnchannels(1); w.setsampwidth(2); w.setframerate(rate)
            w.writeframes((np.clip(x, -1, 1) * 32767).astype('<i2').tobytes())
        env = envelope(x, rate)
        q = np.round(env * 255).astype(int)
        peaks = int(((env[1:-1] > env[:-2]) & (env[1:-1] >= env[2:]) & (env[1:-1] > .6)).sum())
        asset = f"/Game/Art/Audio/Voice/{npc}/VO_{npc}_{line['id']}"
        manifest['lines'][f"{npc}/{line['id']}"] = {'wav': dst.relative_to(NPCS).as_posix(), 'asset': asset,
                                                     'seconds': round(len(x) / rate, 3), 'rate': rate, 'peaks': peaks,
                                                     'gain_db': round(gain_db, 2)}
        entries.append((npc, line, asset, q, len(x) / rate))
        print('CHUCK_VOICE_LINE', npc, line['id'], f'seconds={len(x) / rate:.2f}', f'frames={len(q)}', f'peaks={peaks}', f'gain_db={gain_db:+.2f}')
(NPCS / 'Voice/manifest.json').write_text(json.dumps(manifest, indent=1) + '\n', encoding='utf-8')
BS = chr(92)   # backslash
cpp = lambda t: ''.join(BS + BS if ch == BS else BS + '"' if ch == '"' else ch if ord(ch) < 128 else f'{BS}u{ord(ch):04x}' for ch in t)   # ASCII-only C++ literal
lines = ['// Generated by Tools/build_npc_voice.py from SourceAssets/NPCs/dialogue.json. Do not edit by hand.', '#pragma once', '',
         'namespace NPCVoiceData', '{', f'    constexpr float EnvelopeRate = {FPS}.f;   // envelope frames per second',
         '    struct FLine { const TCHAR* Npc; const TCHAR* Id; const TCHAR* Text; const TCHAR* Sound; float Seconds; const uint8* Envelope; int32 Frames; };']
for k, (npc, line, asset, q, secs) in enumerate(entries):
    lines.append(f'    constexpr uint8 Envelope{k}[] = {{{", ".join(str(v) for v in q) if len(q) else "0"}}};')
lines.append('    const FLine Lines[] = {')
for k, (npc, line, asset, q, secs) in enumerate(entries):
    sound = f"{asset}.VO_{npc}_{line['id']}" if asset else ''
    lines.append(f'        {{ TEXT("{npc}"), TEXT("{line["id"]}"), TEXT("{cpp(line["text"])}"), TEXT("{sound}"), {secs:.3f}f, Envelope{k}, {len(q)} }},')
lines += ['    };', '    constexpr int32 LineCount = UE_ARRAY_COUNT(Lines);', '}', '']
OUT_H.write_text('\n'.join(lines), encoding='utf-8')
print('CHUCK_VOICE_READY', len(entries))
