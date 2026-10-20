// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "lang/lang.h"

#if !defined(RDE_PLATFORM_MOBILE)

#include "study/chars/bake.h"

// ===========================================================================
// See lang.h (fude_bake_run). Hindi's data bake (study/chars/bake.h), a DESKTOP mode of the app itself:
//
//   <desktop build> --bake [--strokes=PATH] [--prepared=DIR] [--out=PATH]
//
// It is run from the project root after the app's tools (strokes/compose.py, then data/fetch.py and data/prepare.py).
// The defaults are data/raw/hi/devanagarivg.xml (the letters' strokes, Hindi's own), data/raw/hi/prepared/ and
// the app's out (fude_bake_app).
//
// Hindi's sources are read by its tools (the app's); this file only bakes what they prepared,
// through the study's bake (fude_bake_run_prepared). It is left out of phone and tablet builds.
// ===========================================================================

// Look-alikes are found among Devanagari's letters (घ ध, भ म, ब व), for every one of them, since there are few. This
// says which characters are the script's letters: the Devanagari block.
RDE_INTERNAL b8 fude_hi_script(u32 _cp) {
    return _cp >= 0x0900u && _cp <= 0x097Fu;
}

// Returns whether a character is worth look-alikes: every letter is.
RDE_INTERNAL b8 fude_hi_common(const fude_kanji_info* _info) {
    RDE_UNUSED(_info);
    return true;
}

// The look-alike rules: the matcher's highest cost that counts (in a 100-unit box), and at most 2 strokes apart.
static const fude_bake_look FUDE_HI_LOOK = {
    .script      = fude_hi_script,
    .common      = fude_hi_common,
    .cost        = 21.0f,   // As for kana, since these are letters too (わ れ cost 8, シ ン 18).
    .cost_script = 21.0f,
    .strokes     = 2u,
};

// The meanings' other languages, as the tools' prepare.py writes them.
static const c8* const FUDE_HI_LANGS[] = { "es", "pt", "fr", "ja" };   // prepare.py's LANGS.

// The bake: its default sources and output, and its limits on words and examples.
static const fude_bake_prepared_def FUDE_HI_BAKE = {
    .strokes        = "data/raw/hi/devanagarivg.xml",
    .prepared       = "data/raw/hi/prepared",
    .languages      = FUDE_HI_LANGS,
    .language_count = (u32)(sizeof(FUDE_HI_LANGS) / sizeof(FUDE_HI_LANGS[0])),   // Counted, so a language added cannot be left out.
    .word_chars     = 16u,   // In code points, so a word's vowel signs and marks count too.
    .example_chars  = 10u,
    .look           = &FUDE_HI_LOOK,
};

// Runs Hindi's bake with the app's command line. Returns 0 on success.
i32 fude_bake_run(const fude_bake_app* _app, i32 _argc, c8** _argv) {
    return fude_bake_run_prepared(&FUDE_HI_BAKE, _app, _argc, _argv);
}

#endif
