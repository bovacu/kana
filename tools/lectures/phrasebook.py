# The study apps' lectures from Wikivoyage's phrasebooks (CC BY-SA 4.0), one per
# language the app speaks that has one: Kana's Japanese ones (English, Spanish,
# French, Portuguese), Hanzi's Chinese and Hangul's Korean ones (those and
# Japanese):
#   python3 tools/lectures/phrasebook.py OUT_DIR [--book=ja|zh|ko] [LANG...]
# writes OUT_DIR/phrasebook_<lang>.pdf and .png (its cover, for the Library).
# The book is Japanese (Kana's) unless --book says.
# Needs a Mac (html2pdf.swift lays the pages out with AppKit; sips draws the
# cover) and pypdf (pip install pypdf), and the network.
#
# What changes, and why: each page as the wiki has it now (its revision is on
# the title page), its text only — the pictures (each under a licence of its
# own), the links, the boxes for editors and the section of further links left
# out — laid out as an A4 PDF. A title page says where it is from, who made it,
# its licence and these changes, as CC BY-SA 4.0 asks; the PDF is under CC BY-SA
# 4.0 too.
import html, io, json, os, re, subprocess, sys, urllib.parse, urllib.request
from html.parser import HTMLParser

HERE = os.path.dirname(os.path.abspath(__file__))
AGENT = 'RDELectures/1.0 (rde.apps.support@gmail.com)'   # Wikimedia asks scripts to say who they are

# Each book: the app it is for, the word its title page shows big, the font for
# its script, and its page in each language (with the sections of further links
# left out).
BOOKS = {
    'ja': dict(app='Kana', big='日本語', font='Hiragino Sans', pages={
        'en': ('Japanese phrasebook', ['Learning more']),
        'es': ('Guía de japonés', []),
        'fr': ('Guide linguistique japonais', ['Approfondir']),
        'pt': ('Guia de conversação japonês', ['Aprenda mais']),
    }),
    'zh': dict(app='Hanzi', big='中文', font='PingFang SC', pages={
        'en': ('Chinese phrasebook', ['Learning more']),
        'es': ('Guía de chino', []),
        'fr': ('Guide linguistique mandarin', ['Approfondir']),
        'pt': ('Guia de conversação mandarim padrão', ['Aprendendo mais']),
        'ja': ('中国語会話集', []),
    }),
    'ko': dict(app='Hangul', big='한국어', font='Apple SD Gothic Neo', pages={
        'en': ('Korean phrasebook', ['Learning more', 'External links']),
        'es': ('Guía de coreano', []),
        'fr': ('Guide linguistique coréen', ['Approfondir']),
        'pt': ('Guia de conversação coreano', ['Aprendendo mais', 'Ligações externas']),
        'ja': ('朝鮮語会話集', ['もっとよく知る']),
    }),
}
BOOK  = BOOKS['ja']
PAGES = BOOK['pages']

# The title page, in the book's own language.
WORDS = {
    'en': dict(site='Wikivoyage', tagline='From Wikivoyage, the free travel guide',
               credit='“{title}” from Wikivoyage ({url}), by its contributors ({history}), revision {rev} of {date}, is licensed under CC BY-SA 4.0: https://creativecommons.org/licenses/by-sa/4.0/',
               changes='Changes made for {app}: {left} left out; laid out as a PDF. This PDF is under CC BY-SA 4.0 too: you may share and adapt it, crediting Wikivoyage, under the same licence.',
               left='the pictures, the links and the section “{sections}”', left_none='the pictures and the links'),
    'es': dict(site='Wikiviajes', tagline='De Wikiviajes, la guía de viajes libre',
               credit='«{title}», de Wikiviajes ({url}), por sus colaboradores ({history}), revisión {rev} del {date}, tiene licencia CC BY-SA 4.0: https://creativecommons.org/licenses/by-sa/4.0/deed.es',
               changes='Cambios para {app}: se han quitado {left}; maquetado como PDF. Este PDF también tiene licencia CC BY-SA 4.0: puedes compartirlo y adaptarlo citando a Wikiviajes, con la misma licencia.',
               left='las imágenes, los enlaces y la sección «{sections}»', left_none='las imágenes y los enlaces'),
    'fr': dict(site='Wikivoyage', tagline='De Wikivoyage, le guide de voyage libre',
               credit='« {title} », de Wikivoyage ({url}), par ses contributeurs ({history}), révision {rev} du {date}, est sous licence CC BY-SA 4.0 : https://creativecommons.org/licenses/by-sa/4.0/deed.fr',
               changes='Modifications pour {app} : {left} ont été retirés ; mis en page en PDF. Ce PDF est aussi sous licence CC BY-SA 4.0 : vous pouvez le partager et l’adapter en citant Wikivoyage, sous la même licence.',
               left='les images, les liens et la section « {sections} »', left_none='les images et les liens'),
    'pt': dict(site='Wikivoyage', tagline='Do Wikivoyage, o guia de viagem livre',
               credit='“{title}”, do Wikivoyage ({url}), por seus colaboradores ({history}), revisão {rev} de {date}, está licenciado sob CC BY-SA 4.0: https://creativecommons.org/licenses/by-sa/4.0/deed.pt',
               changes='Alterações para o {app}: {left} foram retirados; diagramado como PDF. Este PDF também está sob CC BY-SA 4.0: você pode compartilhá-lo e adaptá-lo, dando crédito ao Wikivoyage, sob a mesma licença.',
               left='as imagens, os links e a seção “{sections}”', left_none='as imagens e os links'),
    'ja': dict(site='ウィキボヤージュ', tagline='自由な旅行ガイド、ウィキボヤージュより',
               credit='ウィキボヤージュの「{title}」（{url}）は、執筆者たち（{history}）による{date}の版{rev}で、CC BY-SA 4.0 のもとで利用できます：https://creativecommons.org/licenses/by-sa/4.0/deed.ja',
               changes='{app}のための変更：{left}を除き、PDFとしてレイアウトしました。このPDFも CC BY-SA 4.0 です。ウィキボヤージュをクレジットすれば、同じライセンスのもとで共有・改変できます。',
               left='画像、リンク、「{sections}」の節', left_none='画像とリンク'),
}

STYLE = '''<style>
body { font-family: "Helvetica Neue", "{font}"; font-size: 11pt; color: #111; }
h1 { font-size: 26pt; margin: 0 0 6pt 0; }
h2 { font-size: 17pt; margin: 18pt 0 6pt 0; color: #1d3b6b; }
h3 { font-size: 13.5pt; margin: 14pt 0 4pt 0; color: #1d3b6b; }
h4, h5 { font-size: 11.5pt; margin: 10pt 0 3pt 0; }
p { margin: 0 0 6pt 0; line-height: 1.3; }
dl { margin: 0 0 4pt 0; }
dt { font-weight: bold; margin-top: 5pt; }
dd { margin: 1pt 0 0 18pt; }
table { border-collapse: collapse; margin: 4pt 0 8pt 0; }
td, th { border: 0.5pt solid #9aa3b0; padding: 3pt 5pt; vertical-align: top; }
.big { font-family: "{font}"; font-size: 64pt; color: #1d3b6b; margin: 120pt 0 12pt 0; }
.tag { font-size: 14pt; color: #555; margin-bottom: 140pt; }
.credit { font-size: 9.5pt; color: #333; line-height: 1.35; }
</style>'''


def style():
    return STYLE.replace('{font}', BOOK['font'])


def get(lang, params):
    url = 'https://%s.wikivoyage.org/w/api.php?%s' % (lang, urllib.parse.urlencode(dict(params, format='json')))
    with urllib.request.urlopen(urllib.request.Request(url, headers={'User-Agent': AGENT}), timeout=60) as r:
        return json.load(r)


# --- the page's text, without what is not its own -----------------------------------------------

DROP = {'figure', 'img', 'style', 'script', 'link', 'meta', 'sup', 'noscript', 'audio', 'video'}
KEEP = {'h2', 'h3', 'h4', 'h5', 'p', 'dl', 'dt', 'dd', 'ul', 'ol', 'li', 'table', 'tbody', 'thead', 'tr', 'td', 'th', 'b', 'strong', 'i', 'em', 'code', 'br', 'small', 'caption'}
VOID = {'br', 'img', 'link', 'meta', 'hr', 'input', 'wbr', 'source'}
NOT_TEXT = ('noprint', 'mw-editsection', 'article-status', 'error-deadlink', 'mw-empty-elt', 'navbox', 'metadata', 'reference', 'mw-references', 'thumb')


class Cleaner(HTMLParser):
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.out, self.stack, self.skip = [], [], 0

    def handle_starttag(self, tag, attrs):
        a = dict(attrs)
        classes = a.get('class', '') or ''
        void = tag in VOID
        if self.skip or tag in DROP or any(c in classes for c in NOT_TEXT):
            if not void:
                self.stack.append((tag, True))
                self.skip += 1
            return
        if not void:
            self.stack.append((tag, False))
        if tag in KEEP:
            span = ''.join(' %s="%s"' % (k, html.escape(a[k])) for k in ('colspan', 'rowspan') if k in a)
            self.out.append((tag, '<%s%s>' % (tag, span)))

    def handle_endtag(self, tag):
        # A void element has no end (an <img/> sends one): nothing to close. An
        # end with no start open: stray, ignored.
        if tag in VOID or all(t != tag for t, _ in self.stack):
            return
        while self.stack:
            t, skipping = self.stack.pop()
            if skipping:
                self.skip -= 1
            elif t in KEEP:
                self.out.append((t, '</%s>' % t))
            if t == tag:
                break

    def handle_data(self, data):
        if self.skip:
            return
        self.out.append(('#', html.escape(data)))

    def html(self):
        return ''.join(s for _, s in self.out)


class SectionCutter:
    # Sections left out: from an <h2> named so to the next <h2>.
    @staticmethod
    def cut(text, names):
        if not names:
            return text
        pieces = re.split(r'(?=<h2>)', text)
        kept = [p for p in pieces if not any(re.match(r'<h2>\s*%s\s*</h2>' % re.escape(html.escape(n)), p) for n in names)]
        return ''.join(kept)


def body_of(lang):
    title, drop = PAGES[lang]
    parsed = get(lang, {'action': 'parse', 'page': title, 'prop': 'text|revid', 'disableeditsection': '1'})['parse']
    cleaner = Cleaner()
    cleaner.feed(parsed['text']['*'])
    text = SectionCutter.cut(cleaner.html(), drop)
    text = re.sub(r'<(p|dd|dt|li|td|b|i)>\s*</\1>', '', text)   # left empty by what went
    text = re.sub(r'\n{2,}', '\n', text)
    # Headings with nothing under them (the wiki's sections still to be written).
    while True:
        cut = re.sub(r'<h([2-5])>[^<]*</h\1>\s*(?=<h([2-5])>)', lambda m: '' if int(m.group(2)) <= int(m.group(1)) else m.group(0), text)
        cut = re.sub(r'<h([2-5])>[^<]*</h\1>\s*$', '', cut)
        if cut == text:
            break
        text = cut
    return title, parsed['revid'], text


def revision_date(lang, revid):
    r = get(lang, {'action': 'query', 'revids': revid, 'prop': 'revisions', 'rvprop': 'timestamp'})
    page = list(r['query']['pages'].values())[0]
    return page['revisions'][0]['timestamp'][:10]


def title_page(lang, title, revid, date):
    w      = WORDS[lang]
    _, drop = PAGES[lang]
    page   = 'https://%s.wikivoyage.org/wiki/%s' % (lang, urllib.parse.quote(title.replace(' ', '_')))
    hist   = 'https://%s.wikivoyage.org/w/index.php?title=%s&action=history' % (lang, urllib.parse.quote(title.replace(' ', '_')))
    left   = w['left'].format(sections=('」「' if lang == 'ja' else '”, “').join(drop)) if drop else w['left_none']
    credit = w['credit'].format(title=title, url=page, history=hist, rev=revid, date=date)
    return ('<html><head><meta charset="utf-8">%s</head><body>'
            '<p class="big">%s</p><h1>%s</h1><p class="tag">%s</p>'
            '<p class="credit">%s</p><p class="credit">%s</p></body></html>') % (
        style(), BOOK['big'], html.escape(title), html.escape(w['tagline']), html.escape(credit), html.escape(w['changes'].format(app=BOOK['app'], left=left)))


def make(lang, out_dir):
    from pypdf import PdfReader, PdfWriter
    title, revid, body = body_of(lang)
    date = revision_date(lang, revid)
    work = os.path.join(out_dir, '.phrasebook_' + lang)
    os.makedirs(work, exist_ok=True)
    with open(os.path.join(work, 'title.html'), 'w') as f:
        f.write(title_page(lang, title, revid, date))
    with open(os.path.join(work, 'body.html'), 'w') as f:
        f.write('<html><head><meta charset="utf-8">%s</head><body><h1>%s</h1>%s</body></html>' % (style(), html.escape(title), body))
    swift = os.path.join(HERE, 'html2pdf.swift')
    subprocess.run(['swift', swift, os.path.join(work, 'title.html'), os.path.join(work, 'title.pdf'), '--no-numbers'], check=True)
    subprocess.run(['swift', swift, os.path.join(work, 'body.html'), os.path.join(work, 'body.pdf')], check=True)
    writer = PdfWriter()
    writer.add_page(PdfReader(os.path.join(work, 'title.pdf')).pages[0])
    for page in PdfReader(os.path.join(work, 'body.pdf')).pages:
        writer.add_page(page)
    writer.add_metadata({'/Title': '%s (Wikivoyage, %s edition)' % (title, BOOK['app']), '/Author': 'Wikivoyage contributors',
                         '/Subject': 'CC BY-SA 4.0. From revision %s of %s: pictures and links left out, laid out as a PDF.' % (revid, date)})
    out = os.path.join(out_dir, 'phrasebook_%s.pdf' % lang)
    with open(out, 'wb') as f:
        writer.write(f)
    # Its cover for the Library: the title page.
    subprocess.run(['sips', '-s', 'format', 'png', '-Z', '360', out, '--out', os.path.join(out_dir, 'phrasebook_%s.png' % lang)], check=True, stdout=subprocess.DEVNULL)
    print('%s: %d pages, revision %s of %s' % (out, len(writer.pages), revid, date))


if __name__ == '__main__':
    out_dir = os.path.abspath(sys.argv[1])
    langs   = []
    for a in sys.argv[2:]:
        if a.startswith('--book='):
            BOOK  = BOOKS[a[len('--book='):]]
            PAGES = BOOK['pages']
        else:
            langs.append(a)
    for lang in (langs or list(PAGES)):
        make(lang, out_dir)
