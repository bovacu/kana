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
#
# Every shot loses the status bar (the iPad's clock and battery). A caption's
# [[words]] are drawn in the marker's yellow. A shot in CALLOUTS also shows one
# part of it enlarged over it, so it reads even as the store's thumbnail.

import itertools, os, sys
from PIL import Image, ImageDraw, ImageFilter, ImageFont

FONT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "assets", "fonts", "NotoSansJP-Regular.otf")

# What each screenshot says, by its name. The first ten in the store's order;
# then the spares, for a shot put in place of one of them.
CAPTIONS = {
    "write-translate":
        {"en": "Write Japanese by hand. Kana reads it and [[translates it]].", "es": "Escribe japonés a mano. Kana lo lee y [[lo traduce]].",
         "pt": "Escreva japonês à mão. O Kana lê e [[traduz]].", "ja": "手で書いた日本語を、読み取って[[翻訳]]",
         "fr": "Écrivez le japonais à la main. Kana le lit et [[le traduit]]."},
    "translate":
        {"en": "Point the camera at Japanese. [[Read it in your language.]]", "es": "Apunta la cámara al japonés. [[Léelo en tu idioma.]]",
         "pt": "Aponte a câmera para o japonês. [[Leia no seu idioma.]]", "ja": "カメラを向けるだけで、[[日本語を翻訳]]",
         "fr": "Visez du japonais. [[Lisez-le dans votre langue.]]"},
    "kanji":
        {"en": "Every kanji writes itself, [[stroke by stroke]]", "es": "Cada kanji se escribe solo, [[trazo a trazo]]", "pt": "Cada kanji se escreve sozinho, [[traço a traço]]",
         "ja": "どの漢字も、[[書き順どおりに]]書いて見せる", "fr": "Chaque kanji s’écrit sous vos yeux, [[trait par trait]]"},
    "lectures":
        {"en": "[[Real Japanese lessons]] to read, highlight and write on", "es": "[[Lecciones de japonés]] para leer, subrayar y escribir encima",
         "pt": "[[Lições de japonês]] para ler, marcar e escrever por cima", "ja": "[[本物の教材]]を読んで、マーカーを引いて、書き込む",
         "fr": "[[De vraies leçons de japonais]] à lire, surligner et annoter"},
    "practice":
        {"en": "Practice with guidance, until you write it [[from memory]]", "es": "Practica con guía, hasta escribirlo [[de memoria]]",
         "pt": "Pratique com orientação, até escrever [[de memória]]", "ja": "なぞり書きから、[[何も見ずに書ける]]まで",
         "fr": "Entraînez-vous guidé, jusqu’à l’écrire [[de mémoire]]"},
    "draw-search":
        {"en": "Can’t type a kanji? [[Draw it]]", "es": "¿No sabes teclear un kanji? [[Dibújalo]]", "pt": "Não sabe digitar um kanji? [[Desenhe]]",
         "ja": "読めない漢字は、[[描いて探す]]", "fr": "Impossible de taper un kanji ? [[Dessinez-le]]"},
    "vocabulary":
        {"en": "Your words come back [[right before you forget them]]", "es": "Tus palabras vuelven [[justo antes de que las olvides]]",
         "pt": "Suas palavras voltam [[pouco antes de você esquecê-las]]", "ja": "[[忘れかけたころに]]、自分の単語が戻ってくる",
         "fr": "Vos mots reviennent [[juste avant que vous les oubliiez]]"},
    "kanji-list":
        {"en": "[[6,412 kanji]] and all the kana, by JLPT level", "es": "[[6.412 kanji]] y todos los kana, por nivel del JLPT", "pt": "[[6.412 kanji]] e todos os kana, por nível do JLPT",
         "ja": "[[漢字6,412字]]とすべてのかな、JLPTレベル別", "fr": "[[6 412 kanji]] et tous les kana, par niveau JLPT"},
    "kana":
        {"en": "Hiragana and katakana, [[from the very first stroke]]", "es": "Hiragana y katakana, [[desde el primer trazo]]", "pt": "Hiragana e katakana, [[desde o primeiro traço]]",
         "ja": "ひらがなとカタカナを、[[最初の一画から]]", "fr": "Hiragana et katakana, [[dès le premier trait]]"},
    "statistics":
        {"en": "Watch your Japanese [[grow]], day after day", "es": "Mira [[crecer]] tu japonés, día a día", "pt": "Veja seu japonês [[crescer]], dia após dia",
         "ja": "日本語の[[上達]]が、毎日見える", "fr": "Voyez votre japonais [[progresser]], jour après jour"},
    # Spares.
    "page":
        {"en": "Write Japanese by hand. [[Kana reads it.]]", "es": "Escribe japonés a mano. [[Kana lo lee.]]", "pt": "Escreva japonês à mão. [[O Kana lê.]]",
         "ja": "手で書いた日本語を、[[Kanaが読み取る]]", "fr": "Écrivez le japonais à la main. [[Kana le lit.]]"},
    "select":
        {"en": "[[Circle a word]]: check it, copy it as text, save it", "es": "[[Rodea una palabra]]: revísala, cópiala como texto, guárdala",
         "pt": "[[Circule uma palavra]]: verifique, copie como texto, salve", "ja": "[[言葉を囲んで]]、チェック、コピー、単語帳へ",
         "fr": "[[Entourez un mot]] : vérifiez-le, copiez-le en texte, gardez-le"},
    "into-japanese":
        {"en": "Say it in your language, [[write it in Japanese]]", "es": "Dilo en tu idioma, [[escríbelo en japonés]]",
         "pt": "Diga no seu idioma, [[escreva em japonês]]", "ja": "自分の言葉を、[[日本語にして書く]]",
         "fr": "Dites-le dans votre langue, [[écrivez-le en japonais]]"},
    "library":
        {"en": "[[Free lessons included]], and room for your own PDFs and scans", "es": "[[Lecciones gratis incluidas]], y sitio para tus PDF y escaneos",
         "pt": "[[Lições grátis incluídas]], e espaço para seus PDFs e digitalizações", "ja": "[[無料の教材入り]]。自分のPDFやスキャンも",
         "fr": "[[Des leçons gratuites incluses]], et vos propres PDF et scans"},
    "album":
        {"en": "Every character you have written, [[with its score]]", "es": "Cada carácter que has escrito, [[con su nota]]",
         "pt": "Cada caractere que você escreveu, [[com sua nota]]", "ja": "書いた字を、[[点数とともに]]アルバムに",
         "fr": "Chaque caractère écrit, [[avec sa note]]"},
}
LANGS = ["en", "es", "pt", "ja", "fr"]

# The part of a shot shown enlarged over it ("box": left, top, right, bottom as
# shares of the shot as taken, status bar included), its top where the box's is,
# its left edge at "left" (a share of the shot's width: far enough left to hide
# whatever it half covers; omitted, centred on the box).
CALLOUTS = {
    "write-translate": {"box": (366 / 1488, 732 / 2266, 1246 / 1488, 1014 / 2266), "left": 90 / 1488},   # the translation under 日本語, its edges
}
CALLOUT_ZOOM = 1.5     # how much bigger than the shot around it
STATUS_BAR   = 24      # points (every iPad draws 2 pixels to the point)

TOP    = (45, 79, 168)    # the background's gradient, top to bottom: Kana's blue
BOTTOM = (22, 36, 84)
WHITE  = (255, 255, 255)
MARKER = (255, 214, 92)   # a caption's [[words]]: the marker's yellow
SHADOW = (8, 12, 30)

def marked(caption):
    # "a [[b]] c" → "a b c", and the places of the characters in [[ ]].
    plain, lit, on, i = "", set(), False, 0
    while i < len(caption):
        if caption.startswith("[[", i) or caption.startswith("]]", i):
            on = caption[i] == "["
            i += 2
            continue
        if on:
            lit.add(len(plain))
        plain += caption[i]
        i += 1
    return plain, lit

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
    # Each line with where it starts in the text.
    out, at = [], 0
    for l in lines:
        at = text.index(l, at)
        out.append((at, l))
        at += len(l)
    return out

PHRASE_END = ",.:;?!、。"

def wrap_two(draw, text, font, width):
    # One line if it fits; else the two that read best: as even as they can be,
    # and broken after a comma or a stop when one is near the middle (Latin text
    # at a space; Japanese after 、, or between any two characters when that
    # will not do). Each line with where it starts in the text; None: no two
    # lines fit.
    if draw.textlength(text, font=font) <= width:
        return [(0, text)]
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
                best, best_score = [(0, a), (start, b)], score
        if best is not None:
            return best
    return None

def draw_line(draw, x, y, line, start, lit, font):
    # The line in runs: white, and the [[ ]] words in yellow. A thin outline of
    # the same colour makes the regular weight read a little bolder.
    run, run_lit = "", None
    for i, c in enumerate(line + "\0"):
        now = (start + i) in lit
        if c == "\0" or (run and now != run_lit):
            fill = MARKER if run_lit else WHITE
            draw.text((x, y), run, font=font, fill=fill, stroke_width=2, stroke_fill=fill)
            x += draw.textlength(run, font=font)
            run = ""
        if c != "\0":
            run, run_lit = run + c, now

def rounded(img, radius):
    mask = Image.new("L", img.size, 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, img.width - 1, img.height - 1], radius=radius, fill=255)
    return mask

def lift(canvas, box, radius, blur, strength, drop):
    # A soft shadow under the box (left, top, right, bottom) on the canvas.
    W, H   = canvas.size
    shadow = Image.new("L", (W, H), 0)
    ImageDraw.Draw(shadow).rounded_rectangle([box[0], box[1] + drop, box[2], box[3] + drop], radius=radius, fill=strength)
    shadow = shadow.filter(ImageFilter.GaussianBlur(blur))
    return Image.composite(Image.new("RGB", (W, H), SHADOW), canvas, shadow)

def frame(shot, caption, callout=None):
    shot     = shot.convert("RGB")
    bar      = 2 * STATUS_BAR
    taken    = shot.size
    shot     = shot.crop((0, bar, shot.width, shot.height))   # no clock, no battery
    portrait = shot.height >= shot.width
    W, H     = (2048, 2732) if portrait else (2732, 2048)
    canvas   = Image.new("RGB", (W, H))
    grad     = ImageDraw.Draw(canvas)
    for y in range(H):
        t = y / (H - 1)
        grad.line([(0, y), (W, y)], fill=tuple(int(TOP[i] + (BOTTOM[i] - TOP[i]) * t) for i in range(3)))

    # The caption: as big as fits in two lines.
    text, lit = marked(caption)
    draw  = ImageDraw.Draw(canvas)
    room  = W - 2 * 140
    size  = 104 if portrait else 96
    while True:
        font  = ImageFont.truetype(FONT, size)
        lines = wrap_two(draw, text, font, room)
        if lines is not None:
            break
        if size <= 60:
            lines = wrap(draw, text, font, room)   # longer than two lines can hold
            break
        size -= 4
    # Two lines' room even for one, centred in it: every screenshot the same size.
    line_h = int(size * 1.3)
    top    = 150 if portrait else 110
    rows   = max(2, len(lines))
    first  = top + (rows - len(lines)) * line_h / 2
    for i, (start, l) in enumerate(lines):
        w = draw.textlength(l, font=font)
        draw_line(draw, (W - w) / 2, first + i * line_h, l, start, lit, font)
    caption_bottom = top + rows * line_h

    # The screenshot under it, as big as fits, rounded, on a soft shadow.
    margin  = 120
    box_w   = W - 2 * margin
    box_h   = H - caption_bottom - 90 - margin
    scale   = min(box_w / shot.width, box_h / shot.height)
    sw, sh  = int(shot.width * scale), int(shot.height * scale)
    shown   = shot.resize((sw, sh), Image.LANCZOS)
    x, y    = (W - sw) // 2, caption_bottom + 90 + (box_h - sh) // 2
    radius  = int(min(sw, sh) * 0.035)
    canvas  = lift(canvas, (x, y, x + sw, y + sh), radius, 40, 150, 24)
    canvas.paste(shown, (x, y), rounded(shown, radius))

    # The callout: that part of the shot, enlarged, lifted over it — growing
    # downwards only, so what is above it (what it points at) stays in view.
    if callout is not None:
        box = callout["box"]
        l, t, r, b = (int(box[0] * taken[0]), int(box[1] * taken[1]) - bar,
                      int(box[2] * taken[0]), int(box[3] * taken[1]) - bar)
        part   = shot.crop((l, t, r, b))
        cw, ch = int(part.width * scale * CALLOUT_ZOOM), int(part.height * scale * CALLOUT_ZOOM)
        part   = part.resize((cw, ch), Image.LANCZOS)
        want_x = x + callout["left"] * taken[0] * scale if "left" in callout else x + (l + r) / 2 * scale - cw / 2
        cx     = int(min(max(want_x, 70), W - 70 - cw))
        cy     = int(min(y + t * scale, H - 70 - ch))
        border = 10
        canvas = lift(canvas, (cx - border, cy - border, cx + cw + border, cy + ch + border), 36, 36, 190, 28)
        ring   = Image.new("RGB", (cw + 2 * border, ch + 2 * border), WHITE)
        canvas.paste(ring, (cx - border, cy - border), rounded(ring, 36))
        canvas.paste(part, (cx, cy), rounded(part, 28))
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
        done   = 0
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
            img = frame(Image.open(os.path.join(folder, name)), CAPTIONS[what][lang], CALLOUTS.get(what))
            img.save(os.path.join(out, lang, "%d.png" % (n + 1)))
            done += 1
        print(lang, done, "screenshots ->", os.path.join(out, lang))

if __name__ == "__main__":
    main()
