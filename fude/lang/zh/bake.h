// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_BAKE
#define FUDE_BAKE

#include "rde.h"

// ===========================================================================
// Hanzi's data bake (study/chars/bake.h), a DESKTOP mode of the app itself:
//
//   <desktop build> --bake [--strokes=PATH] [--prepared=DIR] [--out=PATH]
//
// run from the project root after apps/hanzi/tools/data/ (fetch.py, strokes.py,
// prepare.py). Defaults: data/raw/zh/hanzivg.xml, data/raw/zh/prepared/,
// apps/hanzi/assets/data/characters.kana — and the strokes beside it in a file
// of their own (strokes.kana): they are Make Me a Hanzi's, under the Arphic
// Public License, which asks for that and for a notice of how they changed.
// ===========================================================================

// Is --bake on the command line? Always false on mobile.
b8  fude_bake_requested(i32 _argc, c8** _argv);

// Runs the bake and reports on the log. 0 on success.
i32 fude_bake_run(i32 _argc, c8** _argv);

#endif
