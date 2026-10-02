#!/usr/bin/env python3
# The App Store's iPad screenshots, from screenshots taken on the iPad: each
# framed on Kana's blue with a caption on top, at the 13-inch size App Store
# Connect asks for (2048 x 2732 portrait, 2732 x 2048 landscape — the store
# shows them, scaled, on every iPad), once per language.
#
#   python3 -m pip install pillow          # once
#   python3 tools/store/screenshots.py IN_DIR [OUT_DIR]
#
# IN_DIR: the screenshots (PNG or JPEG; any iPad, either orientation: a smaller
# iPad's are scaled into the frame), each named NN-what.png: NN its place in the
# store's order (1 to 10), what the caption below it gets (01-page.png: the
# first, captioned "page"). A name with only a number (4.png) gets the caption
# of that place in CAPTIONS. store/README.md lists what each shows.
# Shots of the app in each language (Settings › Language) can go in IN_DIR/en,
# IN_DIR/es, IN_DIR/pt, IN_DIR/ja, IN_DIR/fr: each language then uses its own;
# otherwise every language uses IN_DIR's.
# OUT_DIR (default: IN_DIR/store): one folder per language, en es pt ja fr,
# ready to drag into App Store Connect.

import itertools, os, sys
from PIL import Image, ImageDraw, ImageFilter, ImageFont

FONT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "assets", "fonts", "NotoSansJP-Regular.otf")

# What each screenshot says, by its name. The first ten in the store's order;
# then the spares, for a shot put in place of one of them.
CAPTIONS = {
    "page":
        {"en": "Write Japanese by hand. Kana reads it.", "es": "Escribe japonés a mano. Kana lo lee.", "pt": "Escreva japonês à mão. O Kana lê.",
         "ja": "手で書いた日本語を、Kanaが読み取る", "fr": "Écrivez le japonais à la main. Kana le lit."},
    "kanji":
        {"en": "Every kanji writes itself, stroke by stroke", "es": "Cada kanji se escribe solo, trazo a trazo", "pt": "Cada kanji se escreve sozinho, traço a traço",
         "ja": "どの漢字も、書き順どおりに書いて見せる", "fr": "Chaque kanji s’écrit sous vos yeux, trait par trait"},
    "lectures":
        {"en": "Real Japanese lessons to read, highlight and write on", "es": "Lecciones de japonés para leer, subrayar y escribir encima",
         "pt": "Lições de japonês para ler, marcar e escrever por cima", "ja": "本物の教材を読んで、マーカーを引いて、書き込む",
         "fr": "De vraies leçons de japonais à lire, surligner et annoter"},
    "select":
        {"en": "Circle a word: check it, copy it as text, save it", "es": "Rodea una palabra: revísala, cópiala como texto, guárdala",
         "pt": "Circule uma palavra: verifique, copie como texto, salve", "ja": "言葉を囲んで、チェック、コピー、単語帳へ",
         "fr": "Entourez un mot\u00a0: vérifiez-le, copiez-le en texte, gardez-le"},
    "practice":
        {"en": "Practice with guidance, until you write it from memory", "es": "Practica con guía, hasta escribirlo de memoria",
         "pt": "Pratique com orientação, até escrever de memória", "ja": "なぞり書きから、何も見ずに書けるまで",
         "fr": "Entraînez-vous guidé, jusqu’à l’écrire de mémoire"},
    "draw-search":
        {"en": "Can’t type a kanji? Draw it", "es": "¿No sabes teclear un kanji? Dibújalo", "pt": "Não sabe digitar um kanji? Desenhe",
         "ja": "読めない漢字は、描いて探す", "fr": "Impossible de taper un kanji\u00a0? Dessinez-le"},
    "vocabulary":
        {"en": "Your words come back right before you forget them", "es": "Tus palabras vuelven justo antes de que las olvides",
         "pt": "Suas palavras voltam pouco antes de você esquecê-las", "ja": "忘れかけたころに、自分の単語が戻ってくる",
         "fr": "Vos mots reviennent juste avant que vous les oubliiez"},
    "kanji-list":
        {"en": "6,412 kanji and all the kana, by JLPT level", "es": "6.412 kanji y todos los kana, por nivel del JLPT", "pt": "6.412 kanji e todos os kana, por nível do JLPT",
         "ja": "漢字6,412字とすべてのかな、JLPTレベル別", "fr": "6\u00a0412 kanji et tous les kana, par niveau JLPT"},
    "kana":
        {"en": "Hiragana and katakana, from the very first stroke", "es": "Hiragana y katakana, desde el primer trazo", "pt": "Hiragana e katakana, desde o primeiro traço",
         "ja": "ひらがなとカタカナを、最初の一画から", "fr": "Hiragana et katakana, dès le premier trait"},
    "statistics":
        {"en": "Watch your Japanese grow, day after day", "es": "Mira crecer tu japonés, día a día", "pt": "Veja seu japonês crescer, dia após dia",
         "ja": "日本語の上達が、毎日見える", "fr": "Voyez votre japonais progresser, jour après jour"},
    # Spares. Text from a photo with its translation and Into Japanese are the
    # iPad's own (the Simulator has no ML Kit).
    "translate":
        {"en": "Point the camera at Japanese. Read it in your language.", "es": "Apunta la cámara al japonés. Léelo en tu idioma.",
         "pt": "Aponte a câmera para o japonês. Leia no seu idioma.", "ja": "カメラを向けるだけで、日本語を翻訳",
         "fr": "Visez du japonais. Lisez-le dans votre langue."},
    "into-japanese":
        {"en": "Say it in your language, write it in Japanese", "es": "Dilo en tu idioma, escríbelo en japonés",
         "pt": "Diga no seu idioma, escreva em japonês", "ja": "自分の言葉を、日本語にして書く",
         "fr": "Dites-le dans votre langue, écrivez-le en japonais"},
    "library":
        {"en": "Free lessons included, and room for your own PDFs and scans", "es": "Lecciones gratis incluidas, y sitio para tus PDF y escaneos",
         "pt": "Lições grátis incluídas, e espaço para seus PDFs e digitalizações", "ja": "無料の教材入り。自分のPDFやスキャンも",
         "fr": "Des leçons gratuites incluses, et vos propres PDF et scans"},
    "album":
        {"en": "Every character you have written, with its score", "es": "Cada carácter que has escrito, con su nota",
         "pt": "Cada caractere que você escreveu, com sua nota", "ja": "書いた字を、点数とともにアルバムに",
         "fr": "Chaque caractère écrit, avec sa note"},
}
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

PHRASE_END = ",.:;?!、。"

def wrap_two(draw, text, font, width):
    # One line if it fits; else the two that read best: as even as they can be,
    # and broken after a comma or a stop when one is near the middle (Latin text
    # at a space; Japanese after 、, or between any two characters when that
    # will not do). None: no two lines fit.
    if draw.textlength(text, font=font) <= width:
        return [text]
    if " " in text:
        cut_sets = [[(i, i + 1) for i, c in enumerate(text) if c == " "]]
    else:
        cut_sets = [[(i + 1, i + 1) for i, c in enumerate(text[:-1]) if c in "、。"],
                    [(i, i) for i in range(1, len(text)) if not (text[i - 1].isascii() and text[i].isascii())]]   # not inside "JLPT" or "6,412"
    for cuts in cut_sets:
        best, best_score = None, None
        for end, start in cuts:
            a, b   = text[:end], text[start:]
            longer = max(draw.textlength(a, font=font), draw.textlength(b, font=font))
            if longer > width:
                continue
            score = longer * (0.6 if a[-1] in ".?!。" else 0.7 if a[-1] in PHRASE_END else 1.0)   # a stop, a comma, neither
            if best_score is None or score < best_score:
                best, best_score = [a, b], score
        if best is not None:
            return best
    return None

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
        lines = wrap_two(draw, caption, font, room)
        if lines is not None:
            break
        if size <= 60:
            lines = wrap(draw, caption, font, room)   # longer than two lines can hold
            break
        size -= 4
    # Two lines' room even for one, centred in it: every screenshot the same size.
    line_h = int(size * 1.3)
    top    = 150 if portrait else 110
    rows   = max(2, len(lines))
    first  = top + (rows - len(lines)) * line_h / 2
    for i, l in enumerate(lines):
        w = draw.textlength(l, font=font)
        draw.text(((W - w) / 2, first + i * line_h), l, font=font, fill=WHITE)
    caption_bottom = top + rows * line_h

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
        order  = list(CAPTIONS)
        for p, name in enumerate(shots):
            # Its place: the number the name starts with (else its place in the
            # folder); its caption: the word after it (else that place's).
            stem   = os.path.splitext(name)[0]
            digits = "".join(itertools.takewhile(str.isdigit, stem))
            n      = int(digits) - 1 if digits else p
            what   = stem[len(digits):].lstrip("-_ ") or (order[n] if 0 <= n < len(order) else "")
            if n < 0 or what not in CAPTIONS:
                print("no caption for", name, "(captions:", ", ".join(order) + ")")
                continue
            img = frame(Image.open(os.path.join(folder, name)), CAPTIONS[what][lang])
            img.save(os.path.join(out, lang, "%d.png" % (n + 1)))
            done += 1
        print(lang, done, "screenshots ->", os.path.join(out, lang))

if __name__ == "__main__":
    main()
