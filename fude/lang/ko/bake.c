// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "lang/lang.h"

#if !defined(RDE_PLATFORM_MOBILE)

#include "lang/ko/hangul.h"
#include "study/chars/bake.h"

// ===========================================================================
// See lang.h (fude_bake_run). Korean's data bake (study/chars/bake.h), a DESKTOP mode of the app itself:
//
//   <desktop build> --bake [--strokes=PATH] [--prepared=DIR] [--out=PATH]
//
// It is run from the project root after the app's tools (strokes/compose.py, then data/fetch.py and
// data/prepare.py). The defaults are data/raw/ko/koreanvg.xml (the jamo's and the syllables' strokes, which are
// Hangul's own, and the hanja's, which are KanjiVG's), data/raw/ko/prepared/ and the app's out (fude_bake_app).
//
// Korean's sources are read by its tools (the app's); this file only bakes what they prepared, through
// the study's bake (fude_bake_run_prepared). It is left out of phone and tablet builds.
// ===========================================================================

// Look-alikes are found among Hangul's letters and syllables (갈 걀, ㅔ ㅐ), and among the hanja, each kind on its own,
// for the characters of a level or among the 3,000 commonest. This says which characters are the script's: the
// compatibility jamo and the syllables.
RDE_INTERNAL b8 fude_ko_script(u32 _cp) {
    return (_cp >= FUDE_KO_JAMO_FIRST && _cp <= FUDE_KO_JAMO_LAST) || fude_ko_is_syllable(_cp);
}

// Returns whether a character is worth look-alikes: it has a level, or it is among the 3,000 commonest.
RDE_INTERNAL b8 fude_ko_common(const fude_kanji_info* _info) {
    return _info->level != 0u || (_info->frequency >= 1u && _info->frequency <= 3000u);
}

// The look-alike rules: the matcher's highest cost that counts (in a 100-unit box), for the hanja and for Hangul,
// and at most 2 strokes apart.
static const fude_bake_look FUDE_KO_LOOK = {
    .script      = fude_ko_script,
    .common      = fude_ko_common,
    .cost        = 11.0f,   // As for kanji, where 未 末 and 土 士 are look-alikes.
    .cost_script = 21.0f,   // As for kana (わ れ cost 8, シ ン 18).
    .strokes     = 2u,
};

// The meanings' other languages, as the tools' prepare.py writes them.
static const c8* const FUDE_KO_LANGS[] = { "es", "pt", "fr", "ja" };   // prepare.py's LANGS.

// The bake: its default sources and output. Words and examples keep the study's default lengths.
static const fude_bake_prepared_def FUDE_KO_BAKE = {
    .strokes        = "data/raw/ko/koreanvg.xml",
    .prepared       = "data/raw/ko/prepared",
    .languages      = FUDE_KO_LANGS,
    .language_count = (u32)(sizeof(FUDE_KO_LANGS) / sizeof(FUDE_KO_LANGS[0])),   // Counted, so a language added cannot be left out.
    .look           = &FUDE_KO_LOOK,
};

// Runs Korean's bake with the app's command line. Returns 0 on success.
i32 fude_bake_run(const fude_bake_app* _app, i32 _argc, c8** _argv) {
    return fude_bake_run_prepared(&FUDE_KO_BAKE, _app, _argc, _argv);
}

#endif
