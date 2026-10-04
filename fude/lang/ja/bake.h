// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_BAKE
#define FUDE_BAKE

#include "rde.h"

// ===========================================================================
// The offline data bake: KanjiVG + KANJIDIC2 (+ JMdict) → the character data
// file the app ships (format in kanji.h). A DESKTOP mode of Kana itself, not a
// separate tool:
//
//   <desktop build> --bake [--kanjivg=PATH] [--kanjidic=PATH] [--jlpt=DIR] [--jmdict=PATH] [--out=PATH]
//
// run from the project root. Defaults: data/raw/kanjivg.xml,
// data/raw/kanjidic2.xml, data/raw/jlpt/ (Waller's n1..n5-kanji-char-eng.mem),
// data/raw/JMdict_e.xml, assets/data/characters.kana. The sources are NOT in the
// repository (COMMANDS.txt has where to get them); the baked file is an asset.
// The JLPT lists are optional: without them every character reads "not listed".
// So is JMdict: without it there are no example words.
//
// Every character KanjiVG has strokes for is kept (kana and kanji, plus some
// punctuation), with KANJIDIC2's readings and meanings where it has an entry.
// A KanjiVG path it cannot parse drops that character and is reported: the file
// never holds half a character.
// ===========================================================================

// Is --bake on the command line? Always false on mobile.
b8  fude_bake_requested(i32 _argc, c8** _argv);

// Runs the bake and reports on the log. 0 on success.
i32 fude_bake_run(i32 _argc, c8** _argv);

#endif
