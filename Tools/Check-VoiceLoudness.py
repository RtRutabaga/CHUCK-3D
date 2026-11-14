"""Voice loudness check (Claude, 2026-11-13; docs/AUTOMATION-DEFECT-CATALOG.md, category 6).

Reads every voice line under SourceAssets/NPCs/Voice/<Speaker>/*.wav (not the source/ takes) and measures its speech
level: the power of 400 ms blocks (75 % overlap) above an absolute gate of -70 dBFS and then a relative gate 10 dB under
their mean, as in ITU-R BS.1770 but without its K-weighting (so "dBFS", close to LUFS for speech). Flags:

  - a line more than 4 dB off its speaker's median (one line much quieter or louder than the rest of that voice);
  - a speaker whose median is more than 4 dB off the median of all speakers (the smith too quiet);
  - a line peaking at or above -0.3 dBFS (clipped).

This is the files only: the game's own volume for each speaker and the distance it is heard at are on top of this and
are checked in the running game by -ChuckSoundAudit only for leaks. Standard library only (no numpy).

  python Tools/Check-VoiceLoudness.py [--csv Local/voice-loudness.csv]

Prints one line per finding and CHUCK_VOICE_LOUDNESS lines=.. speakers=.. line_outliers=.. speakers_off=.. clipped=..
"""
import array
import argparse
import glob
import math
import os
import statistics
import sys
import wave

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VOICE = os.path.join(ROOT, 'SourceAssets', 'NPCs', 'Voice')
LINE_OFF, SPEAKER_OFF, CLIP = 4.0, 4.0, -0.3


def level(path):
    with wave.open(path) as w:
        ch, width, rate, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
        raw = w.readframes(n)
    if width == 2:
        a = array.array('h'); a.frombytes(raw); scale = 32768.0
    elif width == 4:
        a = array.array('i'); a.frombytes(raw); scale = 2147483648.0
    elif width == 3:
        a = array.array('i', (int.from_bytes(raw[i:i + 3], 'little', signed=True) for i in range(0, len(raw), 3))); scale = 8388608.0
    else:
        raise ValueError(f'{width * 8}-bit samples')
    if sys.byteorder == 'big' and width != 3:
        a.byteswap()
    mono = [a[i] / scale for i in range(0, len(a), ch)]   # first channel
    peak = max((abs(x) for x in mono), default=0.0)
    block, hop = int(rate * .4), int(rate * .1)
    powers = []
    for s in range(0, max(1, len(mono) - block + 1), hop):
        seg = mono[s:s + block]
        if seg:
            powers.append(sum(x * x for x in seg) / len(seg))
    gated = [p for p in powers if p > 1e-7]                # -70 dBFS
    if not gated:
        return -99.0, -99.0
    mean = sum(gated) / len(gated)
    gated = [p for p in gated if p > mean * .1] or gated   # -10 dB relative
    loud = 10 * math.log10(sum(gated) / len(gated))
    return loud, 20 * math.log10(peak) if peak > 0 else -99.0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--csv', default=os.path.join(ROOT, 'Local', 'voice-loudness.csv'))
    args = ap.parse_args()
    lines = {}
    for path in sorted(glob.glob(os.path.join(VOICE, '*', '*.wav'))):
        speaker = os.path.basename(os.path.dirname(path))
        try:
            lines.setdefault(speaker, []).append((os.path.basename(path),) + level(path))
        except Exception as e:  # an unreadable file is a finding too
            print(f'CHUCK_VOICE_UNREADABLE {speaker}/{os.path.basename(path)} {e}')
    medians = {s: statistics.median(l[1] for l in v) for s, v in lines.items()}
    overall = statistics.median(medians.values()) if medians else 0
    outliers = speakers_off = clipped = 0
    rows = ['speaker,line,level_dbfs,peak_dbfs,off_speaker_db,speaker_off_all_db']
    for s, v in sorted(lines.items()):
        off_all = medians[s] - overall
        if abs(off_all) > SPEAKER_OFF:
            speakers_off += 1
            print(f'CHUCK_VOICE_SPEAKER_OFF {s} median={medians[s]:.1f} all={overall:.1f} off={off_all:+.1f}dB lines={len(v)}')
        for name, loud, peak in v:
            off = loud - medians[s]
            rows.append(f'{s},{name},{loud:.1f},{peak:.1f},{off:+.1f},{off_all:+.1f}')
            if abs(off) > LINE_OFF:
                outliers += 1
                print(f'CHUCK_VOICE_LINE_OFF {s}/{name} level={loud:.1f} speaker={medians[s]:.1f} off={off:+.1f}dB')
            if peak >= CLIP:
                clipped += 1
                print(f'CHUCK_VOICE_CLIPPED {s}/{name} peak={peak:.2f}dBFS')
    os.makedirs(os.path.dirname(args.csv), exist_ok=True)
    with open(args.csv, 'w', encoding='utf-8') as f:
        f.write('\n'.join(rows) + '\n')
    total = sum(len(v) for v in lines.values())
    print(f'CHUCK_VOICE_LOUDNESS lines={total} speakers={len(lines)} all_median={overall:.1f} line_outliers={outliers} '
          f'speakers_off={speakers_off} clipped={clipped} csv={args.csv}')


if __name__ == '__main__':
    main()
