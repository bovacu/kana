#include "hangul_app.h"
#include "version.h"
#include "drawing/base/text.h"
#include "drawing/widgets/icons.h"
#include "drawing/app/look.h"

#include <stdlib.h>

// ===========================================================================
// See hangul_app.h.
// ===========================================================================

// What Hangul is (info.h). Its credits are the short ones the character data's
// licences require be shown to users (assets/data/LICENSE-data.txt has them in
// full); in full they are Settings › Licences (Data: the attribution NIKL,
// KanjiVG and EDRDG require; ML Kit: Google's terms and notices).
const fude_app_info HANGUL_INFO = {
    .name = HANGUL_NAME, .id = HANGUL_ID, .version = HANGUL_VERSION, .store_id = HANGUL_APP_STORE_ID,
    .script_font = "assets/fonts/NotoSansKR-Regular.otf", .credits = FUDE_TEXT_CREDITS,
    .licences = {
        { FUDE_TEXT_LICENCE_DATA,      { "assets/data/LICENSE-data.txt", NULL, NULL } },
        { FUDE_TEXT_LICENCE_FONTS,     { "assets/fonts/LICENSE-Roboto.txt", "assets/fonts/LICENSE-NotoSansCJK.txt", "assets/fonts/LICENSE-Phosphor.txt" } },
        { FUDE_TEXT_LICENCE_LIBRARIES, { "assets/licenses/libraries.txt", NULL, NULL } },
        { FUDE_TEXT_LICENCE_MLKIT,     { "assets/licenses/ml-kit-notices.txt", NULL, NULL } },
        { FUDE_TEXT_LICENCE_LECTURES,  { "assets/lectures/LICENSE-lectures.txt", NULL, NULL } },
    },
    .licence_count = 5u,
};

// --- the lectures (the Library's books: doc.h) ------------------------------------------------
// Free to share and change, also in an app that is sold: each one's licence and
// what Hangul changed are in assets/lectures/LICENSE-lectures.txt (Settings ›
// Licences › Lectures), and how it was made in tools/lectures/. Ids are never
// reused: a canvas keeps its book's.
static const fude_doc_book HANGUL_LIBRARY[] = {
    // Wikivoyage's Korean phrasebook, one per language (the app's, else English).
    { 2u, "assets/lectures/phrasebook_en.pdf", FUDE_TEXT_LECTURE_PHRASEBOOK, FUDE_TEXT_LECTURE_PHRASEBOOK_ABOUT,
      "Wikivoyage contributors \xC2\xB7 CC BY-SA 4.0 \xC2\xB7 Hangul edition: pictures and links left out", "assets/lectures/phrasebook_en.png", "en" },
    { 3u, "assets/lectures/phrasebook_es.pdf", FUDE_TEXT_LECTURE_PHRASEBOOK, FUDE_TEXT_LECTURE_PHRASEBOOK_ABOUT,
      "Colaboradores de Wikiviajes \xC2\xB7 CC BY-SA 4.0 \xC2\xB7 Edici\xC3\xB3n de Hangul: sin im\xC3\xA1genes ni enlaces", "assets/lectures/phrasebook_es.png", "es" },
    { 4u, "assets/lectures/phrasebook_fr.pdf", FUDE_TEXT_LECTURE_PHRASEBOOK, FUDE_TEXT_LECTURE_PHRASEBOOK_ABOUT,
      "Contributeurs de Wikivoyage \xC2\xB7 CC BY-SA 4.0 \xC2\xB7 \xC3\x89" "dition de Hangul : sans images ni liens", "assets/lectures/phrasebook_fr.png", "fr" },
    { 5u, "assets/lectures/phrasebook_pt.pdf", FUDE_TEXT_LECTURE_PHRASEBOOK, FUDE_TEXT_LECTURE_PHRASEBOOK_ABOUT,
      "Colaboradores do Wikivoyage \xC2\xB7 CC BY-SA 4.0 \xC2\xB7 Edi\xC3\xA7\xC3\xA3o do Hangul: sem imagens nem links", "assets/lectures/phrasebook_pt.png", "pt" },
    { 6u, "assets/lectures/phrasebook_ja.pdf", FUDE_TEXT_LECTURE_PHRASEBOOK, FUDE_TEXT_LECTURE_PHRASEBOOK_ABOUT,
      "Wikivoyage\xE3\x81\xAE\xE5\x9F\xB7\xE7\xAD\x86\xE8\x80\x85 \xC2\xB7 CC BY-SA 4.0 \xC2\xB7 Hangul\xE7\x89\x88", "assets/lectures/phrasebook_ja.png", "ja" },
};

void hangul_app_open(fude_app* _app, HANGUL_SCREEN_ _screen) {
    hangul_app* _hangul = HANGUL_APP(_app);
    fude_lasso_clear(_app->lasso, _app->ink);
    switch(_screen) {
        case HANGUL_SCREEN_BROWSE: fude_chart_close(_hangul->chart);          fude_browse_open(_hangul->study.browse); break;
        case HANGUL_SCREEN_CHART:  fude_browse_close(_hangul->study.browse); fude_chart_open(_hangul->chart);          break;
        case HANGUL_SCREEN_EXAM:   fude_exam_open(_hangul->study.exam);       break;
        case HANGUL_SCREEN_VOCAB:  fude_vocabview_open(_hangul->study.vocab); break;
        case HANGUL_SCREEN_STATS:  fude_stats_open(_hangul->study.stats);     break;
        case HANGUL_SCREEN_ALBUM:  fude_album_open(_hangul->study.album);     break;
        case HANGUL_SCREEN_LIBRARY: fude_library_open(_hangul->library);      break;
        default: break;
    }
}

// --- the side panel's Study section --------------------------------------------------------

RDE_INTERNAL b8 hangul_nav_open(fude_app* _app, u32 _screen) {
    hangul_app_open(_app, (HANGUL_SCREEN_)_screen);
    return true;
}

// Reviews: with none due, a notice instead, and the panel stays.
RDE_INTERNAL b8 hangul_nav_review(fude_app* _app, u32 _arg) {
    RDE_UNUSED(_arg);
    return fude_study_review(_app);
}

// Without the character data, what needs it is greyed out.
RDE_INTERNAL b8 hangul_nav_has_data(const fude_app* _app) { return fude_browse_available(FUDE_STUDY(_app)->browse); }
RDE_INTERNAL b8 hangul_nav_has_chart(const fude_app* _app) { return fude_chart_count(HANGUL_APP(_app)->chart) > 0u; }
RDE_INTERNAL b8 hangul_nav_has_album(const fude_app* _app) { return FUDE_STUDY(_app)->album->db != NULL; }

// The icons: the Korean ones as Hangul — Kana's hanja as Korean reads them (復 복,
// 語 어, 試 시).
static const fude_extension_nav_entry HANGUL_NAV_ENTRIES[] = {
    { FUDE_TEXT_HANGUL_CHARACTERS, "\xEA\xB8\x80", 15.0f, hangul_nav_open,   HANGUL_SCREEN_BROWSE, hangul_nav_has_data,  NULL, 0 },                        // 글
    { FUDE_TEXT_HANGUL,     "\xEA\xB0\x80",       15.0f, hangul_nav_open,   HANGUL_SCREEN_CHART,  hangul_nav_has_chart, NULL, 0 },                        // 가
    { FUDE_TEXT_ALBUM,      FUDE_ICON_BOOKS,      16.0f, hangul_nav_open,   HANGUL_SCREEN_ALBUM,  hangul_nav_has_album, NULL, 0 },
    { FUDE_TEXT_REVIEWS,    "\xEB\xB3\xB5",       15.0f, hangul_nav_review, 0,                    hangul_nav_has_data,  fude_study_reviews_count, FUDE_TEXT_REVIEWS_N },   // 복
    { FUDE_TEXT_VOCAB,      "\xEC\x96\xB4",       15.0f, hangul_nav_open,   HANGUL_SCREEN_VOCAB,  NULL,                 NULL, 0 },                        // 어
    { FUDE_TEXT_LIBRARY_TITLE, FUDE_ICON_BOOK,    16.0f, hangul_nav_open,   HANGUL_SCREEN_LIBRARY, NULL,                NULL, 0 },                        // the lectures
    { FUDE_TEXT_EXAMS,      "\xEC\x8B\x9C",       15.0f, hangul_nav_open,   HANGUL_SCREEN_EXAM,   hangul_nav_has_data,  NULL, 0 },                        // 시
    { FUDE_TEXT_STATISTICS, FUDE_ICON_CHART_LINE, 16.0f, hangul_nav_open,   HANGUL_SCREEN_STATS,  NULL,                 NULL, 0 },
};
static const fude_extension_nav HANGUL_NAV = { FUDE_TEXT_SIDE_STUDY, HANGUL_NAV_ENTRIES, (u32)(sizeof(HANGUL_NAV_ENTRIES) / sizeof(HANGUL_NAV_ENTRIES[0])) };

// --- the welcome ------------------------------------------------------------------------------

// The very first launch: what Hangul is, and its gestures.
RDE_INTERNAL void hangul_first_launch(fude_app* _app) {
    fude_welcome_open(HANGUL_APP(_app)->welcome);
}

// The side panel's Tutorial: the welcome again.
RDE_INTERNAL void hangul_tutorial(fude_app* _app) {
    fude_welcome_open(HANGUL_APP(_app)->welcome);
}

// --- a developer's launch flags ---------------------------------------------------------------

RDE_INTERNAL struct {
    i32  argc;
    c8** argv;
    i32  welcome;   // --welcome[=PAGE]: the welcome at frame 10, on that page (-1: no)
    b8   demo;      // --demo: the welcome closed (the demo's saves are a learner's, not a first launch)
    b8   offline;   // --offline-note: why the app works offline (the must-read card) at frame 10
} hangul_look = { 0, NULL, -1, false, false };

void hangul_look_args(i32 _argc, c8** _argv) {
    fude_study_look_args(_argc, _argv);
    hangul_look.argc = _argc;
    hangul_look.argv = _argv;
    for(i32 _i = 1; _i < _argc; _i++) {
        const c8* _v;
        if(fude_look_is(_argv[_i], "--welcome"))                    { hangul_look.welcome = 0; }
        if(fude_look_is(_argv[_i], "--offline-note"))               { hangul_look.offline = true; }
        if(fude_look_is(_argv[_i], "--demo") || fude_look_value(_argv[_i], "--demo") != NULL) { hangul_look.demo = true; }
        if((_v = fude_look_value(_argv[_i], "--welcome")) != NULL) { hangul_look.welcome = (i32)strtol(_v, NULL, 10) - 1; }
    }
}

void hangul_look_start(fude_app* _app) {
    fude_study_look_start(_app);
    for(i32 _i = 1; _i < hangul_look.argc; _i++) {
        if(fude_look_is(hangul_look.argv[_i], "--chart")) { fude_chart_open(HANGUL_APP(_app)->chart); }
        if(fude_look_is(hangul_look.argv[_i], "--library")) { fude_library_open(HANGUL_APP(_app)->library); }
    }
}

void hangul_look_loaded(fude_app* _app) {
    fude_study_look_loaded(_app);
    if(hangul_look.demo) {
        HANGUL_APP(_app)->welcome->open = false;
    }
}

void hangul_look_frame(fude_app* _app) {
    fude_study_look_frame(_app);
    if(fude_look_shot_frame() == 10u && hangul_look.welcome >= 0) {
        fude_welcome_open(HANGUL_APP(_app)->welcome);
        HANGUL_APP(_app)->welcome->page = (u32)hangul_look.welcome < FUDE_WELCOME_PAGES ? (u32)hangul_look.welcome : 0u;
    }
    if(fude_look_shot_frame() == 10u && hangul_look.offline) {
        fude_welcome_offline(HANGUL_APP(_app)->welcome);
    }
}

// --- the extension ---------------------------------------------------------------------------

// The study's, and Hangul's own: its side panel, the welcome.
const fude_extension HANGUL_EXTENSION = {
    FUDE_STUDY_EXTENSION,
    .nav = &HANGUL_NAV, .tutorial = hangul_tutorial, .first_launch = hangul_first_launch,
    .library = HANGUL_LIBRARY, .library_count = (u32)(sizeof(HANGUL_LIBRARY) / sizeof(HANGUL_LIBRARY[0])),
};
