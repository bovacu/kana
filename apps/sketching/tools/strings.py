# Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

# Sketching's UI strings: the drawing core's (fude/drawing/strings.py) and the
# deep-zoom canvas's (fude/zoom/strings.py). Run after changing them:
#
#   python3 apps/sketching/tools/strings.py
#
# It checks every language has every id with the same placeholders, then writes
#   apps/sketching/src/text_ids.h             the ids, in order (FUDE_TEXT_ID(X) lines)
#   apps/sketching/assets/text/strings.rdel   RDE's localization file, one block per language
# (both generated). tools/strings/build.py has how a row is written.
import os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..', '..'))
sys.dont_write_bytecode = True   # no __pycache__ left in tools/strings/
sys.path.insert(0, os.path.join(ROOT, 'tools', 'strings'))
from build import t, o, P, layer, write, no_fourth

layer(os.path.join(ROOT, 'fude', 'drawing', 'strings.py'))
layer(os.path.join(ROOT, 'fude', 'zoom', 'strings.py'))
no_fourth()   # Sketching teaches no language: English, Spanish, Portuguese and French

if __name__ == '__main__':
    write(sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, '..'), 'apps/sketching/tools/strings.py')
