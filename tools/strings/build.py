# The UI strings' generator, for every app: each layer's strings (fude/drawing/
# strings.py, the core's; fude/study/strings.py, a study app's) and the app's
# own, in that order, become the app's
#   src/text_ids.h            the ids, in order (FUDE_TEXT_ID(X) lines)
#   assets/text/strings.rdel  RDE's localization file, one block per language
# once every language is checked to have every id with the same placeholders.
#
# An app's tools/strings.py (Kana's: apps/kana/tools/strings.py) reads the layers
# it is built from (layer), adds its own (t), may give a lower layer's string
# other words (o; a layer may too: the study's Your data says what a study app
# keeps), then writes.
#
# {APP} in a text is the app's name, filled in as it is written: the one in the
# app's src/version.h (#define KANA_NAME "Kana"), the name its C code shows too.
#
# A row: t('ID', English, Spanish, Portuguese (Brazil), Japanese, French).
# Messages use RDE's placeholders: {0} plain, {0,plural, one{...} other{...}}
# (# is the count). An apostrophe quotes the next character in RDE, so the
# texts use the typographic one (’) instead.
import os, re, sys

L = ['EN-US', 'ES-ES', 'PT-BR', 'JA-JP', 'FR-FR']
LOCALES = {
    'EN-US': ['@plural = n != 1'],
    'ES-ES': ['@plural = n != 1', '@decimal = ,', '@group = .'],
    'PT-BR': ['@plural = n > 1', '@decimal = ,', '@group = .'],
    'JA-JP': ['@plural = 1'],
    'FR-FR': ['@plural = n > 1', '@decimal = ,', '@group =  '],
}
NAMES = {'EN-US': 'English', 'ES-ES': 'Spanish', 'PT-BR': 'Portuguese (Brazil)', 'JA-JP': 'Japanese', 'FR-FR': 'French'}

T = []          # [id, en, es, pt, ja, fr], in order
OVERRIDDEN = set()

P = lambda one, other: '{0,plural, one{%s} other{%s}}' % (one, other)

def t(i, en, es, pt, ja, fr):
    T.append([i, en, es, pt, ja, fr])

# A string of a layer's, in the app's own words (its place in the order kept).
def o(i, en, es, pt, ja, fr):
    for row in T:
        if row[0] == i:
            row[1:] = [en, es, pt, ja, fr]
            OVERRIDDEN.add(i)
            return
    print('o(): no such id', i); sys.exit(1)

# A layer's strings (a file of t() rows, and o() for a lower layer's), read in.
def layer(path):
    exec(open(path, encoding='utf-8').read(), {'t': t, 'o': o, 'P': P})

# The app's name, from its src/version.h (#define <APP>_NAME "...").
def app_name(_app_dir):
    m = re.search(r'#define\s+\w+_NAME\s+"([^"]+)"', open(os.path.join(_app_dir, 'src', 'version.h'), encoding='utf-8').read())
    if m is None:
        print('no #define <APP>_NAME "..." in', os.path.join(_app_dir, 'src', 'version.h')); sys.exit(1)
    return m.group(1)

def placeholders(s):
    return sorted(set(re.findall(r'\{(\d)', s)))

def check():
    ok = True
    ids = set()
    for row in T:
        i = row[0]
        if i in ids:
            print('duplicate id', i); ok = False
        ids.add(i)
        base = placeholders(row[1])
        for k, s in enumerate(row[1:]):
            if "'" in s:
                print('ASCII apostrophe in', i, L[k]); ok = False
            if s.count('{') != s.count('}'):
                print('braces', i, L[k]); ok = False
            if placeholders(s) != base:
                print('placeholders differ', i, L[k], placeholders(s), base); ok = False
            if not s.strip():
                print('empty', i, L[k]); ok = False
    return ok

# The app's name in, checked, then the app's two files written: _app_dir its
# folder (apps/<app>), _tool its tool's path (the files' notes).
def write(_app_dir, _tool):
    _name = app_name(_app_dir)
    for row in T:
        row[1:] = [s.replace('{APP}', _name) for s in row[1:]]
    if not check():
        sys.exit(1)
    with open(os.path.join(_app_dir, 'src', 'text_ids.h'), 'w') as f:
        f.write('// The UI strings\' ids, in order: one FUDE_TEXT_ID(x) per string. Generated\n')
        f.write('// by %s with assets/text/strings.rdel (the texts, in\n' % _tool)
        f.write('// every language); see fude/drawing/base/text.h.\n')
        for row in T:
            f.write('FUDE_TEXT_ID(%s)\n' % row[0])
    with open(os.path.join(_app_dir, 'assets', 'text', 'strings.rdel'), 'w') as f:
        f.write('// %s\'s words, one block per language (RDE\'s localization format: a\n' % _name)
        f.write('// LANGUAGE: line, then ID=text lines). Every block has every id of\n')
        f.write('// src/text_ids.h; both are made by %s: edit it, not\n' % _tool)
        f.write('// them. {0}, {1}... are placeholders, {0,plural, one{...} other{...}} counts\n')
        f.write('// (# the number); an ASCII apostrophe quotes the next character, so write the\n')
        f.write('// typographic one (U+2019).\n')
        for k, lang in enumerate(L):
            f.write('\n// %s\n%s:\n' % (NAMES[lang], lang))
            for loc in LOCALES[lang]:
                f.write(loc + '\n')
            for row in T:
                f.write('%s=%s\n' % (row[0], row[1 + k]))
    print(len(T), 'strings,', len(L), 'languages')
