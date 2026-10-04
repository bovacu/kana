// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "lang/zh/bake.h"

#include <string.h>

#if !defined(RDE_PLATFORM_MOBILE)

#include <stdio.h>

#include "study/chars/bake.h"

// ===========================================================================
// See bake.h: Chinese's readers are its tools' (apps/hanzi/tools/data/); this
// reads what they prepared, through the study's bake.
// ===========================================================================

#define FUDE_ZH_OUT "apps/hanzi/assets/data/characters.kana"

// The Arphic Public License's notice for the strokes changed (its §2a).
#define FUDE_ZH_NOTICE \
    "Hanzi's stroke data is Make Me a Hanzi's (github.com/skishore/makemeahanzi), via hanzi-writer-data 2.0.1, " \
    "derived from Arphic Technology's fonts and licensed under the Arphic Public License (ARPHICPL.TXT). " \
    "Changed by Hanzi's tools (apps/hanzi/tools/data/strokes.py), October 2026: each stroke's median turned " \
    "into cubic Bezier curves in a 109-unit box, then stored in this file's format (fixed point). " \
    "The changed data is published under the same licence: https://github.com/bovacu/hazi_custom_stroke_data"

// Look-alikes: the characters on HSK's lists, or among the 3,000 commonest.
RDE_INTERNAL b8 fude_zh_script(u32 _cp) {
    RDE_UNUSED(_cp);
    return false;
}

RDE_INTERNAL b8 fude_zh_common(const fude_kanji_info* _info) {
    return _info->level != 0u || (_info->frequency >= 1u && _info->frequency <= 3000u);
}

static const fude_bake_look FUDE_ZH_LOOK = {
    .script      = fude_zh_script,
    .common      = fude_zh_common,
    .cost        = 11.0f,   // as kanji's (未 末, 土 士)
    .cost_script = 0.0f,
    .strokes     = 3u,
};

b8 fude_bake_requested(i32 _argc, c8** _argv) {
    for(i32 _i = 1; _i < _argc; _i++) {
        if(_argv[_i] != NULL && strcmp(_argv[_i], "--bake") == 0) {
            return true;
        }
    }
    return false;
}

i32 fude_bake_run(i32 _argc, c8** _argv) {
    const c8* _strokes  = fude_bake_arg(_argc, _argv, "--strokes=",  "data/raw/zh/hanzivg.xml");
    const c8* _prepared = fude_bake_arg(_argc, _argv, "--prepared=", "data/raw/zh/prepared");
    const c8* _out      = fude_bake_arg(_argc, _argv, "--out=",      FUDE_ZH_OUT);
    if(!rde_file_exists(_strokes)) {
        rde_log_level(RDE_LOG_LEVEL_ERROR, "bake: %s not found (apps/hanzi/tools/data: fetch.py, strokes.py, prepare.py; run from the project root)", _strokes);
        return 1;
    }

    // The strokes' own file, beside the character data (kanji.h).
    c8 _strokes_out[RDE_MAX_PATH];
    const c8* _slash = strrchr(_out, '/');
    snprintf(_strokes_out, sizeof(_strokes_out), "%.*s%s", _slash != NULL ? (int)(_slash - _out + 1) : 0, _out, FUDE_KANJI_STROKES_FILE);

    static const c8* const _langs[] = { "es", "pt", "fr" };   // prepare.py's LANGS
    fude_bake _bake;
    fude_bake_init(&_bake, _langs, 3u);
    const f64 _t0 = rde_engine_get_time_now();
    i32       _rc = 0;
    if(!fude_bake_kanjivg(&_bake, _strokes) || !fude_bake_prepared(&_bake, _prepared)) {
        _rc = 1;
    }
    if(_rc == 0 && !fude_bake_write(&_bake, _out, _strokes_out, FUDE_ZH_NOTICE, _t0)) {
        _rc = 1;
    }
    if(_rc == 0 && !fude_bake_lookalikes(_out, &FUDE_ZH_LOOK)) {
        rde_log_level(RDE_LOG_LEVEL_ERROR, "bake: the look-alikes could not be added to %s", _out);
        _rc = 1;
    }
    fude_bake_free(&_bake);
    return _rc;
}

#else

b8 fude_bake_requested(i32 _argc, c8** _argv) {
    RDE_UNUSED(_argc);
    RDE_UNUSED(_argv);
    return false;
}

i32 fude_bake_run(i32 _argc, c8** _argv) {
    RDE_UNUSED(_argc);
    RDE_UNUSED(_argv);
    return 1;
}

#endif
