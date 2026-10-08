"""Cut the HUD stamina emblem out of the user's icon (user 2026-10-08).

python Tools/build_stamina_emblem.py

Source: References/ArtDirection/Chuck-Stamina-Icon.png (1254 px, white paw
emblem inside a purple ring on black). The ring is drawn by the HUD itself so
it can show how full stamina is (DockGameMode.cpp, ADockHUD::DrawStamina);
only the emblem inside it becomes a texture: white, its alpha the source's
brightness, cropped to the ring's inner disc (centre 635,606, radius 320 of
the ring's outer 400) and saved at 256 px to SourceAssets/UI/T_StaminaEmblem.png.
"""
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'References/ArtDirection/Chuck-Stamina-Icon.png'
OUT = ROOT / 'SourceAssets/UI/T_StaminaEmblem.png'
CENTRE, INNER, SIZE = (635, 606), 320, 256

image = Image.open(SOURCE).convert('RGB')
crop = image.crop((CENTRE[0] - INNER, CENTRE[1] - INNER, CENTRE[0] + INNER, CENTRE[1] + INNER))
# Brightness of the near-white strokes only: the black ground goes clear, and
# any purple from the ring's inner edge is kept out by the disc mask.
alpha = crop.convert('L').point(lambda v: max(0, min(255, (v - 30) * 255 // 200)))
disc = Image.new('L', crop.size, 0)
ImageDraw.Draw(disc).ellipse((2, 2, crop.size[0] - 3, crop.size[1] - 3), fill=255)
alpha = Image.composite(alpha, Image.new('L', crop.size, 0), disc)
emblem = Image.new('RGBA', crop.size, (255, 255, 255, 0))
emblem.putalpha(alpha)
OUT.parent.mkdir(parents=True, exist_ok=True)
emblem.resize((SIZE, SIZE), Image.LANCZOS).save(OUT)
print('CHUCK_STAMINA_EMBLEM', OUT, SIZE)
