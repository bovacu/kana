#include "kana_app.h"
#include "version.h"
#include "drawing/base/text.h"
#include "drawing/widgets/icons.h"
#include "drawing/app/look.h"

#include <stdlib.h>

// ===========================================================================
// See kana_app.h.
// ===========================================================================

// What Kana is (info.h). Its credits are the short ones the character data's
// licences require be shown to users (assets/data/LICENSE-data.txt has them in
// full); in full they are Settings › Licences (Data: the attribution KanjiVG and
// EDRDG require; ML Kit: Google's terms and notices).
const fude_app_info KANA_INFO = {
    .name = KANA_NAME, .version = KANA_VERSION, .store_id = KANA_APP_STORE_ID,
    .script_font = "assets/fonts/NotoSansJP-Regular.otf", .credits = FUDE_TEXT_CREDITS,
    .licences = {
        { FUDE_TEXT_LICENCE_DATA,      { "assets/data/LICENSE-data.txt", NULL, NULL } },
        { FUDE_TEXT_LICENCE_FONTS,     { "assets/fonts/LICENSE-Roboto.txt", "assets/fonts/LICENSE-NotoSansJP.txt", "assets/fonts/LICENSE-Phosphor.txt" } },
        { FUDE_TEXT_LICENCE_LIBRARIES, { "assets/licenses/libraries.txt", NULL, NULL } },
        { FUDE_TEXT_LICENCE_MLKIT,     { "assets/licenses/ml-kit-notices.txt", NULL, NULL } },
    },
    .licence_count = 4u,
};

void kana_app_open(fude_app* _app, KANA_SCREEN_ _screen) {
    kana_app* _kana = KANA_APP(_app);
    fude_lasso_clear(_app->lasso, _app->ink);
    switch(_screen) {
        case KANA_SCREEN_BROWSE: fude_chart_close(_kana->chart);          fude_browse_open(_kana->study.browse); break;
        case KANA_SCREEN_CHART:  fude_browse_close(_kana->study.browse); fude_chart_open(_kana->chart);          break;
        case KANA_SCREEN_EXAM:   fude_exam_open(_kana->study.exam);       break;
        case KANA_SCREEN_VOCAB:  fude_vocabview_open(_kana->study.vocab); break;
        case KANA_SCREEN_STATS:  fude_stats_open(_kana->study.stats);     break;
        case KANA_SCREEN_ALBUM:  fude_album_open(_kana->study.album);     break;
        default: break;
    }
}

// --- the side panel's Study section --------------------------------------------------------

RDE_INTERNAL b8 kana_nav_open(fude_app* _app, u32 _screen) {
    kana_app_open(_app, (KANA_SCREEN_)_screen);
    return true;
}

// Reviews: with none due, a notice instead, and the panel stays.
RDE_INTERNAL b8 kana_nav_review(fude_app* _app, u32 _arg) {
    RDE_UNUSED(_arg);
    return fude_study_review(_app);
}

// Without the character data, what needs it is greyed out.
RDE_INTERNAL b8 kana_nav_has_data(const fude_app* _app) { return fude_browse_available(FUDE_STUDY(_app)->browse); }
RDE_INTERNAL b8 kana_nav_has_kana(const fude_app* _app) { return fude_chart_count(KANA_APP(_app)->chart) > 0u; }
RDE_INTERNAL b8 kana_nav_has_album(const fude_app* _app) { return FUDE_STUDY(_app)->album->db != NULL; }

// The icons: the Japanese ones as characters.
static const fude_extension_nav_entry KANA_NAV_ENTRIES[] = {
    { FUDE_TEXT_KANJI,      "\xE5\xAD\x97",       15.0f, kana_nav_open,   KANA_SCREEN_BROWSE, kana_nav_has_data,  NULL, 0 },                        // 字
    { FUDE_TEXT_KANA,       "\xE3\x81\x82",       15.0f, kana_nav_open,   KANA_SCREEN_CHART,  kana_nav_has_kana,  NULL, 0 },                        // あ
    { FUDE_TEXT_ALBUM,      FUDE_ICON_BOOKS,      16.0f, kana_nav_open,   KANA_SCREEN_ALBUM,  kana_nav_has_album, NULL, 0 },
    { FUDE_TEXT_REVIEWS,    "\xE5\xBE\xA9",       15.0f, kana_nav_review, 0,                  kana_nav_has_data,  fude_study_reviews_count, FUDE_TEXT_REVIEWS_N },   // 復
    { FUDE_TEXT_VOCAB,      "\xE8\xAA\x9E",       15.0f, kana_nav_open,   KANA_SCREEN_VOCAB,  NULL,               NULL, 0 },                        // 語
    { FUDE_TEXT_EXAMS,      "\xE8\xA9\xA6",       15.0f, kana_nav_open,   KANA_SCREEN_EXAM,   kana_nav_has_data,  NULL, 0 },                        // 試
    { FUDE_TEXT_STATISTICS, FUDE_ICON_CHART_LINE, 16.0f, kana_nav_open,   KANA_SCREEN_STATS,  NULL,               NULL, 0 },
};
static const fude_extension_nav KANA_NAV = { FUDE_TEXT_SIDE_STUDY, KANA_NAV_ENTRIES, (u32)(sizeof(KANA_NAV_ENTRIES) / sizeof(KANA_NAV_ENTRIES[0])) };

// --- the welcome ------------------------------------------------------------------------------

// The very first launch: what Kana is, and its gestures.
RDE_INTERNAL void kana_first_launch(fude_app* _app) {
    kana_welcome_open(KANA_APP(_app)->welcome);
}

// The side panel's Tutorial: the welcome again.
RDE_INTERNAL void kana_tutorial(fude_app* _app) {
    kana_welcome_open(KANA_APP(_app)->welcome);
}

// --- a developer's launch flags ---------------------------------------------------------------

RDE_INTERNAL struct {
    i32  argc;
    c8** argv;
    i32  welcome;   // --welcome[=PAGE]: the welcome at frame 10, on that page (-1: no)
} kana_look = { 0, NULL, -1 };

void kana_look_args(i32 _argc, c8** _argv) {
    fude_study_look_args(_argc, _argv);
    kana_look.argc = _argc;
    kana_look.argv = _argv;
    for(i32 _i = 1; _i < _argc; _i++) {
        const c8* _v;
        if(fude_look_is(_argv[_i], "--welcome"))                    { kana_look.welcome = 0; }
        if((_v = fude_look_value(_argv[_i], "--welcome")) != NULL) { kana_look.welcome = (i32)strtol(_v, NULL, 10) - 1; }
    }
}

void kana_look_start(fude_app* _app) {
    fude_study_look_start(_app);
    for(i32 _i = 1; _i < kana_look.argc; _i++) {
        if(fude_look_is(kana_look.argv[_i], "--kana")) { fude_chart_open(KANA_APP(_app)->chart); }
    }
}

void kana_look_loaded(fude_app* _app) {
    fude_study_look_loaded(_app);
}

void kana_look_frame(fude_app* _app) {
    fude_study_look_frame(_app);
    if(fude_look_shot_frame() == 10u && kana_look.welcome >= 0) {
        kana_welcome_open(KANA_APP(_app)->welcome);
        KANA_APP(_app)->welcome->page = (u32)kana_look.welcome < KANA_WELCOME_PAGES ? (u32)kana_look.welcome : 0u;
    }
}

// --- the extension ---------------------------------------------------------------------------

// The study's, and Kana's own: its side panel, the welcome.
const fude_extension KANA_EXTENSION = {
    FUDE_STUDY_EXTENSION,
    .nav = &KANA_NAV, .tutorial = kana_tutorial, .first_launch = kana_first_launch,
};
