// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "lang/lang.h"

#if !defined(RDE_PLATFORM_MOBILE)

#include "study/chars/bake.h"

// ===========================================================================
// See lang.h (fude_bake_run). Hanzi's data bake (study/chars/bake.h), a DESKTOP mode of the app itself:
//
//   <desktop build> --bake [--strokes=PATH] [--prepared=DIR] [--out=PATH]
//
// It is run from the project root after apps/hanzi/tools/data/ (fetch.py, strokes.py, prepare.py). The defaults are
// data/raw/zh/hanzivg.xml, data/raw/zh/prepared/ and apps/hanzi/assets/data/characters.kana, with the strokes beside
// it in a file of their own (strokes.kana). The strokes are Make Me a Hanzi's, under the Arphic Public License, which
// asks for them to be kept apart like this and for a notice of how they were changed.
//
// Chinese's sources are read by its tools (apps/hanzi/tools/data/); this file only bakes what they prepared, through
// the study's bake (fude_bake_run_prepared). It is left out of phone and tablet builds.
// ===========================================================================

// The notice the Arphic Public License asks for when the strokes are changed (its §2a): where they come from, how
// they were changed and when, and where the changed data is published.
#define FUDE_ZH_NOTICE \
    "Hanzi's stroke data is Make Me a Hanzi's (github.com/skishore/makemeahanzi), via hanzi-writer-data 2.0.1, " \
    "derived from Arphic Technology's fonts and licensed under the Arphic Public License (ARPHICPL.TXT). " \
    "Changed by Hanzi's tools (apps/hanzi/tools/data/strokes.py), October 2026: each stroke's median turned " \
    "into cubic Bezier curves in a 109-unit box, then stored in this file's format (fixed point). " \
    "The changed data is published under the same licence: https://github.com/bovacu/hazi_custom_stroke_data"

// Look-alikes are found for the characters on HSK's lists or among the 3,000 commonest. Chinese has no script of
// letters to compare apart, so no character is one.
RDE_INTERNAL b8 fude_zh_script(u32 _cp) {
    RDE_UNUSED(_cp);
    return false;
}

// Returns whether a character is worth look-alikes: it has an HSK level, or it is among the 3,000 commonest.
RDE_INTERNAL b8 fude_zh_common(const fude_kanji_info* _info) {
    return _info->level != 0u || (_info->frequency >= 1u && _info->frequency <= 3000u);
}

// The look-alike rules: the matcher's highest cost that counts (in a 100-unit box), and at most 3 strokes apart.
static const fude_bake_look FUDE_ZH_LOOK = {
    .script      = fude_zh_script,
    .common      = fude_zh_common,
    .cost        = 11.0f,   // As for kanji, where 未 末 and 土 士 are look-alikes.
    .cost_script = 0.0f,
    .strokes     = 3u,
};

// The meanings' other languages, as the tools' prepare.py writes them.
static const c8* const FUDE_ZH_LANGS[] = { "es", "pt", "fr" };   // prepare.py's LANGS.

// The bake: its default sources and output, and the strokes' notice. Words and examples keep the study's default
// lengths.
static const fude_bake_prepared_def FUDE_ZH_BAKE = {
    .strokes        = "data/raw/zh/hanzivg.xml",
    .prepared       = "data/raw/zh/prepared",
    .out            = "apps/hanzi/assets/data/characters.kana",
    .tools          = "apps/hanzi/tools/data: fetch.py, strokes.py, prepare.py",
    .languages      = FUDE_ZH_LANGS,
    .language_count = (u32)(sizeof(FUDE_ZH_LANGS) / sizeof(FUDE_ZH_LANGS[0])),   // Counted, so a language added cannot be left out.
    .strokes_notice = FUDE_ZH_NOTICE,   // In a file of their own, because the licence asks for the notice with them.
    .look           = &FUDE_ZH_LOOK,
};

// Runs Hanzi's bake with the app's command line. Returns 0 on success.
i32 fude_bake_run(i32 _argc, c8** _argv) {
    return fude_bake_run_prepared(&FUDE_ZH_BAKE, _argc, _argv);
}

#endif
