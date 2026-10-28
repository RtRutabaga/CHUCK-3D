"""Contact sheet of NPC face shots (-ChuckNPCCapture's Head / RatEye shots), for reviews.
python Tools/face_sheet.py <dir> <shot> <out.png> [cols]"""
import sys
from pathlib import Path
from PIL import Image, ImageDraw
src, shot, out = Path(sys.argv[1]), sys.argv[2], sys.argv[3]
cols = int(sys.argv[4]) if len(sys.argv) > 4 else 4
files = sorted(src.glob(f'NPC_*_{shot}.png'))
w, h = 480, 270
sheet = Image.new('RGB', (cols * w, ((len(files) + cols - 1) // cols) * h), (30, 30, 30))
for k, f in enumerate(files):
    im = Image.open(f).convert('RGB')
    # The middle of the frame: the face.
    W, H = im.size
    im = im.crop((W * .2, H * .1, W * .8, H * .9)).resize((w, h))
    ImageDraw.Draw(im).text((6, 4), f.stem.split('_')[1], fill=(255, 255, 0))
    sheet.paste(im, ((k % cols) * w, (k // cols) * h))
sheet.save(out)
print(out, len(files))
