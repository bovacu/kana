#include "lang/hi/bake.h"

#include <string.h>

#if !defined(RDE_PLATFORM_MOBILE)

#include <stdio.h>

#include "study/chars/bake.h"

// ===========================================================================
// See bake.h: Hindi's readers are its tools' (apps/hindi/tools/data/); this
// reads what they prepared, through the study's bake.
// ===========================================================================

#define FUDE_HI_OUT "apps/hindi/assets/data/characters.kana"

// Look-alikes: Devanagari's letters among themselves (घ ध, भ म, ब व), every one
// of them (there are few).
RDE_INTERNAL b8 fude_hi_script(u32 _cp) {
    return _cp >= 0x0900u && _cp <= 0x097Fu;
}

RDE_INTERNAL b8 fude_hi_common(const fude_kanji_info* _info) {
    RDE_UNUSED(_info);
    return true;
}

static const fude_bake_look FUDE_HI_LOOK = {
    .script      = fude_hi_script,
    .common      = fude_hi_common,
    .cost        = 21.0f,   // as kana's: letters (わ れ 8, シ ン 18)
    .cost_script = 21.0f,
    .strokes     = 2u,
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
    const c8* _strokes  = fude_bake_arg(_argc, _argv, "--strokes=",  "data/raw/hi/devanagarivg.xml");
    const c8* _prepared = fude_bake_arg(_argc, _argv, "--prepared=", "data/raw/hi/prepared");
    const c8* _out      = fude_bake_arg(_argc, _argv, "--out=",      FUDE_HI_OUT);
    if(!rde_file_exists(_strokes)) {
        rde_log_level(RDE_LOG_LEVEL_ERROR, "bake: %s not found (apps/hindi/tools: strokes/compose.py, data/fetch.py, data/prepare.py; run from the project root)", _strokes);
        return 1;
    }

    static const c8* const _langs[] = { "es", "pt", "fr", "ja" };   // prepare.py's LANGS
    fude_bake _bake;
    fude_bake_init(&_bake, _langs, 4u);
    _bake.word_chars    = 16u;   // a word's code points: its vowel signs and marks count too
    _bake.example_chars = 10u;
    const f64 _t0 = rde_engine_get_time_now();
    i32       _rc = 0;
    if(!fude_bake_kanjivg(&_bake, _strokes) || !fude_bake_prepared(&_bake, _prepared)) {
        _rc = 1;
    }
    if(_rc == 0 && !fude_bake_write(&_bake, _out, NULL, NULL, _t0)) {   // the strokes in it: Hindi's own
        _rc = 1;
    }
    if(_rc == 0 && !fude_bake_lookalikes(_out, &FUDE_HI_LOOK)) {
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
