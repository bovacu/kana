#!/usr/bin/env python3
# The App Store's iPad screenshots, from screenshots taken on the iPad: each
# framed on Kana's blue with a caption on top, at the 13-inch size App Store
# Connect asks for (2048 x 2732 portrait, 2732 x 2048 landscape — the store
# shows them, scaled, on every iPad), once per language.
#
#   python3 -m pip install pillow          # once
#   python3 tools/store/screenshots.py IN_DIR [OUT_DIR]
#
# IN_DIR: the iPad's screenshots (top button + volume up), named so they sort in
# the order wanted — 1.png, 2.png... (PNG or JPEG; any iPad, either orientation).
# N.png gets caption N below (a name without a number: its place in the
# order); docs/store/listing.md lists what each should show.
# Shots of the app in each language (Settings › Language) can go in IN_DIR/en,
# IN_DIR/es, IN_DIR/pt, IN_DIR/ja, IN_DIR/fr: each language then uses its own;
# otherwise every language uses IN_DIR's.
# OUT_DIR (default: IN_DIR/store): one folder per language, en es pt ja fr,
# ready to drag into App Store Connect.

import itertools, os, sys
from PIL import Image, ImageDraw, ImageFilter, ImageFont

FONT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "assets", "fonts", "NotoSansJP-Regular.otf")

CAPTIONS = [
    {"en": "Every kanji writes itself, stroke by stroke", "es": "Cada kanji se escribe solo, trazo a trazo", "pt": "Cada kanji se escreve sozinho, traço a traço",
     "ja": "どの漢字も、書き順どおりに書いて見せる", "fr": "Chaque kanji s’écrit sous vos yeux, trait par trait"},
    {"en": "Write it with the Apple Pencil, and get points for shape and order", "es": "Escríbelo con el Apple Pencil y gana puntos por la forma y el orden",
     "pt": "Escreva com o Apple Pencil e ganhe pontos pela forma e pela ordem", "ja": "Apple Pencilで書いて、形と書き順に点数",
     "fr": "Écrivez-le à l’Apple Pencil, et gagnez des points pour la forme et l’ordre"},
    {"en": "Check anything you write, from a character to a sentence", "es": "Revisa todo lo que escribas, de un carácter a una frase",
     "pt": "Verifique tudo o que você escreve, de um caractere a uma frase", "ja": "1文字から文まで、書いたものをチェック",
     "fr": "Vérifiez tout ce que vous écrivez, du caractère à la phrase"},
    {"en": "Your vocabulary, in your own lists", "es": "Tu vocabulario, en tus propias listas", "pt": "Seu vocabulário, nas suas próprias listas",
     "ja": "自分の単語帳を、自分のリストで", "fr": "Votre vocabulaire, dans vos propres listes"},
    {"en": "Write whole words from their meaning — or by ear", "es": "Escribe palabras enteras a partir de su significado, o de oído",
     "pt": "Escreva palavras inteiras pelo significado, ou de ouvido", "ja": "意味から、または聞いて、言葉をまるごと書く",
     "fr": "Écrivez des mots entiers à partir du sens, ou à l’oreille"},
    {"en": "Point the camera at Japanese, and write it", "es": "Apunta la cámara al japonés y escríbelo", "pt": "Aponte a câmera para o japonês e escreva",
     "ja": "カメラを向けた日本語を、自分の手で", "fr": "Visez du japonais avec l’appareil photo, et écrivez-le"},
    {"en": "See what gets mixed up, and keep your own notes", "es": "Mira qué se confunde y guarda tus notas", "pt": "Veja o que se confunde e guarde suas notas",
     "ja": "まぎらわしい字と、自分のメモ", "fr": "Voyez ce qui se confond, et gardez vos notes"},
    {"en": "Reviews bring back what you are about to forget", "es": "Los repasos te traen lo que estás a punto de olvidar",
     "pt": "As revisões trazem o que você está para esquecer", "ja": "忘れかけたころに、復習", "fr": "Les révisions ramènent ce que vous alliez oublier"},
]
LANGS = ["en", "es", "pt", "ja", "fr"]

TOP    = (45, 79, 168)    # the background's gradient, top to bottom: Kana's blue
BOTTOM = (22, 36, 84)
WHITE  = (255, 255, 255)

def wrap(draw, text, font, width):
    # Words for Latin text; characters for Japanese (no spaces to break at).
    units = text.split(" ") if " " in text else list(text)
    joiner = " " if " " in text else ""
    lines, line = [], ""
    for u in units:
        trial = (line + joiner + u) if line else u
        if draw.textlength(trial, font=font) <= width or not line:
            line = trial
        else:
            lines.append(line)
            line = u
    if line:
        lines.append(line)
    return lines

def frame(shot, caption):
    portrait = shot.height >= shot.width
    W, H     = (2048, 2732) if portrait else (2732, 2048)
    canvas   = Image.new("RGB", (W, H))
    grad     = ImageDraw.Draw(canvas)
    for y in range(H):
        t = y / (H - 1)
        grad.line([(0, y), (W, y)], fill=tuple(int(TOP[i] + (BOTTOM[i] - TOP[i]) * t) for i in range(3)))

    # The caption: as big as fits in two lines.
    draw  = ImageDraw.Draw(canvas)
    room  = W - 2 * 140
    size  = 104 if portrait else 96
    while True:
        font  = ImageFont.truetype(FONT, size)
        lines = wrap(draw, caption, font, room)
        if len(lines) <= 2 or size <= 60:
            break
        size -= 4
    line_h = int(size * 1.3)
    top    = 150 if portrait else 110
    for i, l in enumerate(lines):
        w = draw.textlength(l, font=font)
        draw.text(((W - w) / 2, top + i * line_h), l, font=font, fill=WHITE)
    caption_bottom = top + len(lines) * line_h

    # The screenshot under it, as big as fits, rounded, on a soft shadow.
    margin  = 120
    box_w   = W - 2 * margin
    box_h   = H - caption_bottom - 90 - margin
    scale   = min(box_w / shot.width, box_h / shot.height)
    sw, sh  = int(shot.width * scale), int(shot.height * scale)
    shot    = shot.convert("RGB").resize((sw, sh), Image.LANCZOS)
    x, y    = (W - sw) // 2, caption_bottom + 90 + (box_h - sh) // 2
    radius  = int(min(sw, sh) * 0.035)
    mask    = Image.new("L", (sw, sh), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, sw - 1, sh - 1], radius=radius, fill=255)
    shadow  = Image.new("L", (W, H), 0)
    ImageDraw.Draw(shadow).rounded_rectangle([x, y + 24, x + sw, y + sh + 24], radius=radius, fill=150)
    shadow  = shadow.filter(ImageFilter.GaussianBlur(40))
    canvas  = Image.composite(Image.new("RGB", (W, H), (8, 12, 30)), canvas, shadow)
    canvas.paste(shot, (x, y), mask)
    return canvas

def main():
    if len(sys.argv) < 2:
        print(__doc__ or "usage: screenshots.py IN_DIR [OUT_DIR]")
        sys.exit(1)
    src = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(src, "store")
    def shots_in(folder):
        return sorted(f for f in os.listdir(folder) if f.lower().endswith((".png", ".jpg", ".jpeg")))
    for lang in LANGS:
        # IN_DIR/<lang>/ when there is one (the app shown in that language), else IN_DIR's.
        folder = os.path.join(src, lang) if os.path.isdir(os.path.join(src, lang)) else src
        shots  = shots_in(folder)
        os.makedirs(os.path.join(out, lang), exist_ok=True)
        done = 0
        for p, name in enumerate(shots):
            # The caption: the number the name starts with (4.png: the 4th), else its place.
            digits = "".join(itertools.takewhile(str.isdigit, name))
            n = int(digits) - 1 if digits else p
            if n < 0 or n >= len(CAPTIONS):
                print("no caption for", name, "(1 to %d)" % len(CAPTIONS))
                continue
            img = frame(Image.open(os.path.join(folder, name)), CAPTIONS[n][lang])
            img.save(os.path.join(out, lang, "%d.png" % (n + 1)))
            done += 1
        print(lang, done, "screenshots ->", os.path.join(out, lang))

if __name__ == "__main__":
    main()
