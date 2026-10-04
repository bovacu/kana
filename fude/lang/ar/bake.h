// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_BAKE
#define FUDE_BAKE

#include "rde.h"

// ===========================================================================
// Arabic's data bake (study/chars/bake.h), a DESKTOP mode of the app itself:
//
//   <desktop build> --bake [--strokes=PATH] [--prepared=DIR] [--out=PATH]
//
// run from the project root after apps/arabic/tools/ (strokes/compose.py, then
// data/fetch.py and data/prepare.py). Defaults: data/raw/ar/arabicvg.xml (the
// letters' strokes, Arabic's own), data/raw/ar/prepared/, apps/arabic/assets/data/characters.kana.
// ===========================================================================

// Is --bake on the command line? Always false on mobile.
b8  fude_bake_requested(i32 _argc, c8** _argv);

// Runs the bake and reports on the log. 0 on success.
i32 fude_bake_run(i32 _argc, c8** _argv);

#endif
