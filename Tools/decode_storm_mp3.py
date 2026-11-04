"""Decode the storm's MP3-only sources to WAV with Blender's audaspace (Claude, 2026-11-03).

blender --background --factory-startup --python Tools/decode_storm_mp3.py -- <download folder>

BigSoundBank serves some recordings (the flags, a cliff's waves, a storm) only as MP3 to this machine (their BWF links
return 403). This writes <download folder>/dec/<NNNN>.wav (48 kHz, 16-bit, the file's own channels) from
<download folder>/mp3/<NNNN>.mp3 for Tools/make_storm_sounds.py. Decoding only; nothing else is changed.
"""
import sys
from pathlib import Path
import aud

src = Path(sys.argv[sys.argv.index('--') + 1])
(src / 'dec').mkdir(exist_ok=True)
for mp3 in sorted((src / 'mp3').glob('*.mp3')):
    sound = aud.Sound(str(mp3))
    rate, channels = sound.specs
    out = src / 'dec' / (mp3.stem + '.wav')
    sound.resample(48000, False).write(str(out), 48000, channels, aud.FORMAT_S16, aud.CONTAINER_WAV, aud.CODEC_PCM)
    print('CHUCK_DECODED', mp3.name, rate, channels, out)
