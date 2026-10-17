"""Draws Master Suite installer art: dark plate, gold/blue glow-ring knob."""
import math, sys
from PIL import Image, ImageDraw, ImageFont, ImageFilter
fontpath = sys.argv[1]
MINT, COPPER = (255, 181, 46), (126, 227, 107)

def plate(w, h):
    img = Image.new('RGBA', (w, h)); d = ImageDraw.Draw(img)
    for y in range(h):
        t = y / h
        d.line([(0, y), (w, y)], fill=(int(44 - 22*t), int(47 - 23*t), int(53 - 25*t), 255))
    weave = Image.new('RGBA', (w, h), (0, 0, 0, 0)); wd = ImageDraw.Draw(weave)
    for y in range(0, h, 6):
        for x in range(0, w, 6):
            if ((x // 6) + (y // 6)) % 2 == 0: wd.rectangle([x, y, x + 5, y + 2], fill=(255, 255, 255, 10))
            else: wd.rectangle([x, y + 3, x + 5, y + 5], fill=(0, 0, 0, 28))
    img.alpha_composite(weave)
    return img

def knob(img, cx, cy, r):
    glow = Image.new('RGBA', img.size, (0, 0, 0, 0)); gd = ImageDraw.Draw(glow)
    rr = r * 1.25
    gd.arc([cx-rr, cy-rr, cx+rr, cy+rr], 135, 135 + 270, fill=(30, 34, 40, 255), width=max(4, int(r * 0.12)))
    gd.arc([cx-rr, cy-rr, cx+rr, cy+rr], 135, 135 + 190, fill=MINT + (255,), width=max(4, int(r * 0.12)))
    img.alpha_composite(glow.filter(ImageFilter.GaussianBlur(r * 0.08)))
    img.alpha_composite(glow)
    sh = Image.new('RGBA', img.size, (0, 0, 0, 0)); ImageDraw.Draw(sh).ellipse([cx-r, cy-r+r*0.12, cx+r, cy+r+r*0.12], fill=(0, 0, 0, 190))
    img.alpha_composite(sh.filter(ImageFilter.GaussianBlur(r * 0.12)))
    d = ImageDraw.Draw(img)
    for i in range(int(r)):
        t = i / r
        c = int(110 - 85 * t)
        d.ellipse([cx - r + 0, cy - r + i * 0.0, cx + r, cy + r], outline=None)
    d.ellipse([cx-r, cy-r, cx+r, cy+r], fill=(80, 86, 96), outline=(10, 11, 13), width=2)
    br = r * 0.86
    d.ellipse([cx-br, cy-br, cx+br, cy+br], fill=(34, 37, 43))
    a = math.radians(135 + 190)
    p1 = (cx + math.cos(a) * br * 0.3, cy + math.sin(a) * br * 0.3); p2 = (cx + math.cos(a) * br * 0.9, cy + math.sin(a) * br * 0.9)
    d.line([p1, p2], fill=COPPER, width=max(3, int(r * 0.09)))

def text(d, xy, s, f, col):
    x, y = xy
    d.text((x, y + 2), s, font=f, fill=(0, 0, 0, 200))
    d.text((x, y), s, font=f, fill=col)

W, H = 328, 628
img = plate(W, H); knob(img, W // 2, 220, 95)
d = ImageDraw.Draw(img)
f1 = ImageFont.truetype(fontpath, 34); f2 = ImageFont.truetype(fontpath, 22); f3 = ImageFont.truetype(fontpath, 14)
def c(y, s, f, col): text(d, ((W - d.textlength(s, font=f)) / 2, y), s, f, col)
c(390, "MASTER", f1, (255, 244, 220)); c(432, "SUITE", f1, (255, 244, 220))
c(482, "EQ  DYNAMICS  LOUDNESS", f3, (200, 205, 212))
d.line([(60, 525), (W - 60, 525)], fill=MINT, width=2)
c(550, "ROHIT JI", f2, MINT)
img.convert('RGB').save('wizard-large.bmp'); img.convert('RGB').resize((164, 314), Image.LANCZOS).save('wizard-large-100.bmp')
s = plate(110, 110); knob(s, 55, 55, 32)
s.convert('RGB').save('wizard-small.bmp'); s.convert('RGB').resize((55, 55), Image.LANCZOS).save('wizard-small-100.bmp')
ic = Image.new('RGBA', (512, 512), (0, 0, 0, 0))
m = Image.new('L', (480, 480), 0); ImageDraw.Draw(m).rounded_rectangle([0, 0, 479, 479], 100, fill=255)
ic.paste(plate(480, 480), (16, 16), m); knob(ic, 256, 256, 140)
ic.save('icon.ico', sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
