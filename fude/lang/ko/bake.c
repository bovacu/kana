#include "lang/ko/bake.h"

#include <string.h>

#if !defined(RDE_PLATFORM_MOBILE)

#include <stdio.h>

#include "study/chars/bake.h"

// ===========================================================================
// See bake.h: Korean's readers are its tools' (apps/hangul/tools/data/); this
// reads what they prepared, through the study's bake.
// ===========================================================================

#define FUDE_KO_OUT "apps/hangul/assets/data/characters.kana"

// Look-alikes: Hangul's letters among themselves (갈 걀, ㅔ ㅐ), the hanja
// among theirs; the ones of a level, or among the 3,000 commonest.
RDE_INTERNAL b8 fude_ko_script(u32 _cp) {
    return (_cp >= 0x3131u && _cp <= 0x3163u) || (_cp >= 0xAC00u && _cp <= 0xD7A3u);
}

RDE_INTERNAL b8 fude_ko_common(const fude_kanji_info* _info) {
    return _info->level != 0u || (_info->frequency >= 1u && _info->frequency <= 3000u);
}

static const fude_bake_look FUDE_KO_LOOK = {
    .script      = fude_ko_script,
    .common      = fude_ko_common,
    .cost        = 11.0f,   // as kanji's (未 末, 土 士)
    .cost_script = 21.0f,   // as kana's (わ れ 8, シ ン 18)
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
    const c8* _strokes  = fude_bake_arg(_argc, _argv, "--strokes=",  "data/raw/ko/koreanvg.xml");
    const c8* _prepared = fude_bake_arg(_argc, _argv, "--prepared=", "data/raw/ko/prepared");
    const c8* _out      = fude_bake_arg(_argc, _argv, "--out=",      FUDE_KO_OUT);
    if(!rde_file_exists(_strokes)) {
        rde_log_level(RDE_LOG_LEVEL_ERROR, "bake: %s not found (apps/hangul/tools: strokes/compose.py, data/fetch.py, data/prepare.py; run from the project root)", _strokes);
        return 1;
    }

    static const c8* const _langs[] = { "es", "pt", "fr", "ja" };   // prepare.py's LANGS
    fude_bake _bake;
    fude_bake_init(&_bake, _langs, 4u);
    const f64 _t0 = rde_engine_get_time_now();
    i32       _rc = 0;
    if(!fude_bake_kanjivg(&_bake, _strokes) || !fude_bake_prepared(&_bake, _prepared)) {
        _rc = 1;
    }
    if(_rc == 0 && !fude_bake_write(&_bake, _out, NULL, NULL, _t0)) {   // the strokes in it: Hangul's own and KanjiVG's
        _rc = 1;
    }
    if(_rc == 0 && !fude_bake_lookalikes(_out, &FUDE_KO_LOOK)) {
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
