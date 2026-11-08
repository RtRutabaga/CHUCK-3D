"""Blender 4.5.14 (its aud module and numpy): the ship storm's voice takes cut into one WAV per line, for listening
(Claude, 2026-11-08; docs/regions/SHIP-STORM-ENDING.md).

blender --background --factory-startup --python Tools/split_ship_storm_voice.py
  reads  SourceAssets/NPCs/Voice/ShipStorm/lines.json   per line: id, npc, text, source, start, end (seconds)
  writes Local/ShipStormVoice/<npc>/<id>.wav             the source's own rate, mono 16-bit, 10 ms fades, no levelling

The user's ElevenLabs takes came one file per script section, every line in script order. The cut points in lines.json
were found once from the takes themselves (silences, then Windows' offline speech recognizer given only the script's
sentences, aligned in script order) and checked; this tool only applies them. The game's own copies are made later by
Tools/build_npc_voice.py, which levels each line and takes the same source/start/end once the lines are added to
SourceAssets/NPCs/dialogue.json.
"""
from pathlib import Path
import json
import wave
import aud
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
VOICE = ROOT / 'SourceAssets/NPCs'
OUT = ROOT / 'Local/ShipStormVoice'
spec = json.loads((VOICE / 'Voice/ShipStorm/lines.json').read_text(encoding='utf-8'))
cache = {}
for line in spec['lines']:
    src = VOICE / line['source']
    if src not in cache:
        s = aud.Sound(str(src))
        a = s.data()
        cache[src] = ((a.mean(axis=1) if a.ndim > 1 else a).astype(np.float64), int(s.specs[0]))
    x, rate = cache[src]
    y = x[int(line['start'] * rate):int(line['end'] * rate)].copy()
    fade = int(rate * .01)
    y[:fade] *= np.linspace(0, 1, fade); y[-fade:] *= np.linspace(1, 0, fade)
    dst = OUT / line['npc'] / f"{line['id']}.wav"
    dst.parent.mkdir(parents=True, exist_ok=True)
    with wave.open(str(dst), 'wb') as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(rate)
        w.writeframes((np.clip(y, -1, 1) * 32767).astype('<i2').tobytes())
    print('CHUCK_STORM_VOICE', line['npc'], line['id'], f"{len(y) / rate:.2f}s")
print('CHUCK_STORM_VOICE_DONE', len(spec['lines']), OUT)
