// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "lang/lang.h"

#if !defined(RDE_PLATFORM_MOBILE)

#include "study/chars/bake.h"

// ===========================================================================
// See lang.h (fude_bake_run). Chinese's data bake (study/chars/bake.h), a DESKTOP mode of the app itself:
//
//   <desktop build> --bake [--strokes=PATH] [--prepared=DIR] [--out=PATH]
//
// It is run from the project root after the app's data tools (fetch.py, strokes.py, prepare.py). The defaults are
// data/raw/zh/hanzivg.xml, data/raw/zh/prepared/ and the app's out (fude_bake_app), with the strokes beside it in a
// file of their own (strokes.kana). The strokes are Make Me a Hanzi's, under the Arphic Public License, which asks for
// them to be kept apart like this and for a notice of how they were changed: the app's (fude_bake_app's
// strokes_notice), since its tools changed them. A bake without one fails.
//
// Chinese's sources are read by its tools (the app's); this file only bakes what they prepared, through
// the study's bake (fude_bake_run_prepared). It is left out of phone and tablet builds.
// ===========================================================================

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

// The bake: its default sources. Words and examples keep the study's default lengths.
static const fude_bake_prepared_def FUDE_ZH_BAKE = {
    .strokes        = "data/raw/zh/hanzivg.xml",
    .prepared       = "data/raw/zh/prepared",
    .languages      = FUDE_ZH_LANGS,
    .language_count = (u32)(sizeof(FUDE_ZH_LANGS) / sizeof(FUDE_ZH_LANGS[0])),   // Counted, so a language added cannot be left out.
    .look           = &FUDE_ZH_LOOK,
};

// Runs Chinese's bake with the app's command line. Returns 0 on success.
i32 fude_bake_run(const fude_bake_app* _app, i32 _argc, c8** _argv) {
    // The Arphic Public License asks for the changed strokes to be kept apart, with a notice of how they were changed
    // (its §2a): the app's, since its tools changed them.
    if(_app->strokes_notice == NULL) {
        rde_log_level(RDE_LOG_LEVEL_ERROR, "bake: Chinese's strokes need the app's notice (fude_bake_app: strokes_notice)");
        return 1;
    }
    return fude_bake_run_prepared(&FUDE_ZH_BAKE, _app, _argc, _argv);
}

#endif
