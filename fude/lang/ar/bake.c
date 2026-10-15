// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "lang/lang.h"

#if !defined(RDE_PLATFORM_MOBILE)

#include "study/chars/bake.h"

// ===========================================================================
// See lang.h (fude_bake_run). Arabic's data bake (study/chars/bake.h), a DESKTOP mode of the app itself:
//
//   <desktop build> --bake [--strokes=PATH] [--prepared=DIR] [--out=PATH]
//
// It is run from the project root after apps/arabic/tools/ (strokes/compose.py, then data/fetch.py and
// data/prepare.py). The defaults are data/raw/ar/arabicvg.xml (the letters' strokes, Arabic's own),
// data/raw/ar/prepared/ and apps/arabic/assets/data/characters.kana.
//
// Arabic's sources are read by its tools (apps/arabic/tools/data/); this file only bakes what they prepared,
// through the study's bake (fude_bake_run_prepared). It is left out of phone and tablet builds.
// ===========================================================================

// Look-alikes are found among Arabic's letters and their forms (ب ت ث, ج ح خ, د ر), for every one of them, since
// there are few. This says which characters are the script's letters: the Arabic block and the presentation forms.
RDE_INTERNAL b8 fude_ar_script(u32 _cp) {
    return (_cp >= 0x0600u && _cp <= 0x06FFu) || (_cp >= 0xFE70u && _cp <= 0xFEFFu);
}

// Returns whether a character is worth look-alikes: every letter is.
RDE_INTERNAL b8 fude_ar_common(const fude_kanji_info* _info) {
    RDE_UNUSED(_info);
    return true;
}

// The look-alike rules: the matcher's highest cost that counts (in a 100-unit box), and at most 2 strokes apart.
static const fude_bake_look FUDE_AR_LOOK = {
    .script      = fude_ar_script,
    .common      = fude_ar_common,
    .cost        = 21.0f,   // As for kana, since these are letters too (わ れ cost 8, シ ン 18).
    .cost_script = 21.0f,
    .strokes     = 2u,
};

// The meanings' other languages, as the tools' prepare.py writes them.
static const c8* const FUDE_AR_LANGS[] = { "es", "pt", "fr", "ja" };   // prepare.py's LANGS.

// The bake: its default sources and output, and its limits on words and examples.
static const fude_bake_prepared_def FUDE_AR_BAKE = {
    .strokes        = "data/raw/ar/arabicvg.xml",
    .prepared       = "data/raw/ar/prepared",
    .out            = "apps/arabic/assets/data/characters.kana",
    .tools          = "apps/arabic/tools: strokes/compose.py, data/fetch.py, data/prepare.py",
    .languages      = FUDE_AR_LANGS,
    .language_count = (u32)(sizeof(FUDE_AR_LANGS) / sizeof(FUDE_AR_LANGS[0])),   // Counted, so a language added cannot be left out.
    .word_chars     = 20u,   // In code points, harakat included, since words keep their vowels (كِتَاب is 6).
    .example_chars  = 12u,
    .look           = &FUDE_AR_LOOK,
};

// Runs Arabic's bake with the app's command line. Returns 0 on success.
i32 fude_bake_run(i32 _argc, c8** _argv) {
    return fude_bake_run_prepared(&FUDE_AR_BAKE, _argc, _argv);
}

#endif
