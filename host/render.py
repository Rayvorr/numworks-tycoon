"""Rejoue les dessins enregistrés par la fausse calculatrice et sort une image PNG par capture."""
import sys
from PIL import Image, ImageDraw, ImageFont

ops_path, out_dir = sys.argv[1], sys.argv[2]
FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"
fonts = {True: ImageFont.truetype(FONT, 15), False: ImageFont.truetype(FONT, 11)}
CELL = {True: (10, 18), False: (7, 14)}


def rgb(c):
    c = int(c)
    return ((c >> 11 & 31) * 255 // 31, (c >> 5 & 63) * 255 // 63, (c & 31) * 255 // 31)


img = Image.new("RGB", (320, 240), (0, 0, 0))
d = ImageDraw.Draw(img)
for line in open(ops_path, encoding="utf-8"):
    line = line.rstrip("\n")
    if line.startswith("F "):
        x, y, w, h, c = map(int, line.split()[1:6])
        if w and h:
            d.rectangle([x, y, x + w - 1, y + h - 1], fill=rgb(c))
    elif line.startswith("P "):
        v = line.split()
        x, y, w, h = map(int, v[1:5])
        px = v[5:]
        for i, c in enumerate(px):
            img.putpixel((x + i % w, y + i // w), rgb(c))
    elif line.startswith("S "):
        parts = line.split(" ", 6)
        x, y, big, fg, bg = int(parts[1]), int(parts[2]), parts[3] == "1", parts[4], parts[5]
        t = parts[6] if len(parts) > 6 else ""
        cw, ch = CELL[big]
        d.rectangle([x, y, x + cw * len(t) - 1, y + ch - 1], fill=rgb(bg))
        for i, chr_ in enumerate(t):
            if chr_ != " ":
                d.text((x + i * cw + cw / 2, y + ch / 2), chr_, font=fonts[big], fill=rgb(fg), anchor="mm")
    elif line.startswith("SNAP "):
        name = line.split()[1]
        img.resize((640, 480), Image.NEAREST).save(f"{out_dir}/{name}.png")
