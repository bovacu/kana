// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "arabic_app.h"
#include "version.h"
#include "drawing/base/text.h"
#include "drawing/widgets/icons.h"
#include "drawing/app/look.h"

#include <stdlib.h>

// ===========================================================================
// See arabic_app.h.
// ===========================================================================

// What Arabic is (info.h). Its credits are the short ones the character data's
// licences require be shown to users (assets/data/LICENSE-data.txt has them in
// full); in full they are Settings › Licences (Data: each source's attribution;
// ML Kit: Google's terms and notices).
const fude_app_info ARABIC_INFO = {
    .name = ARABIC_NAME, .id = ARABIC_ID, .version = ARABIC_VERSION, .store_id = ARABIC_APP_STORE_ID,
    .script_font = "assets/fonts/NotoNaskhArabic-Regular.ttf", .latin_font = "assets/fonts/NotoSansLatinExt-Regular.ttf",
    .credits = FUDE_TEXT_CREDITS,
    .licences = {
        { FUDE_TEXT_LICENCE_DATA,      { "assets/data/LICENSE-data.txt", NULL, NULL } },
        { FUDE_TEXT_LICENCE_FONTS,     { "assets/fonts/LICENSE-Roboto.txt", "assets/fonts/LICENSE-Noto.txt", "assets/fonts/LICENSE-Phosphor.txt" } },
        { FUDE_TEXT_LICENCE_LIBRARIES, { "assets/licenses/libraries.txt", NULL, NULL } },
        { FUDE_TEXT_LICENCE_MLKIT,     { "assets/licenses/ml-kit-notices.txt", NULL, NULL } },
        { FUDE_TEXT_LICENCE_LECTURES,  { "assets/lectures/LICENSE-lectures.txt", NULL, NULL } },
    },
    .licence_count = 5u,
};

// --- the lectures (the Library's books: doc.h) ------------------------------------------------
// Free to share and change, also in an app that is sold: each one's licence and
// what Arabic Learn! changed are in assets/lectures/LICENSE-lectures.txt (Settings ›
// Licences › Lectures), and how it was made in tools/lectures/. Ids are never
// reused: a canvas keeps its book's.
static const fude_doc_book ARABIC_LIBRARY[] = {
    // Wikivoyage's Arabic phrasebook, one per language it has (the app's, else English).
    { 2u, "assets/lectures/phrasebook_en.pdf", FUDE_TEXT_LECTURE_PHRASEBOOK, FUDE_TEXT_LECTURE_PHRASEBOOK_ABOUT,
      "Wikivoyage contributors \xC2\xB7 CC BY-SA 4.0 \xC2\xB7 Arabic Learn! edition: pictures and links left out", "assets/lectures/phrasebook_en.png", "en" },
    { 3u, "assets/lectures/phrasebook_es.pdf", FUDE_TEXT_LECTURE_PHRASEBOOK, FUDE_TEXT_LECTURE_PHRASEBOOK_ABOUT,
      "Colaboradores de Wikiviajes \xC2\xB7 CC BY-SA 4.0 \xC2\xB7 Edici\xC3\xB3n de Arabic Learn!: sin im\xC3\xA1genes ni enlaces", "assets/lectures/phrasebook_es.png", "es" },
    { 4u, "assets/lectures/phrasebook_fr.pdf", FUDE_TEXT_LECTURE_PHRASEBOOK, FUDE_TEXT_LECTURE_PHRASEBOOK_ABOUT,
      "Contributeurs de Wikivoyage \xC2\xB7 CC BY-SA 4.0 \xC2\xB7 \xC3\x89" "dition de Arabic Learn! : sans images ni liens", "assets/lectures/phrasebook_fr.png", "fr" },
    { 5u, "assets/lectures/phrasebook_pt.pdf", FUDE_TEXT_LECTURE_PHRASEBOOK, FUDE_TEXT_LECTURE_PHRASEBOOK_ABOUT,
      "Colaboradores do Wikivoyage \xC2\xB7 CC BY-SA 4.0 \xC2\xB7 Edi\xC3\xA7\xC3\xA3o do Arabic Learn!: sem imagens nem links", "assets/lectures/phrasebook_pt.png", "pt" },
};

void arabic_app_open(fude_app* _app, ARABIC_SCREEN_ _screen) {
    arabic_app* _arabic = ARABIC_APP(_app);
    fude_lasso_clear(_app->lasso, _app->ink);
    switch(_screen) {
        case ARABIC_SCREEN_BROWSE: fude_chart_close(_arabic->chart);          fude_browse_open(_arabic->study.browse); break;
        case ARABIC_SCREEN_CHART:  fude_browse_close(_arabic->study.browse); fude_chart_open(_arabic->chart);          break;
        case ARABIC_SCREEN_EXAM:   fude_exam_open(_arabic->study.exam);       break;
        case ARABIC_SCREEN_VOCAB:  fude_vocabview_open(_arabic->study.vocab); break;
        case ARABIC_SCREEN_STATS:  fude_stats_open(_arabic->study.stats);     break;
        case ARABIC_SCREEN_ALBUM:  fude_album_open(_arabic->study.album);     break;
        case ARABIC_SCREEN_LIBRARY: fude_library_open(_arabic->library);      break;
        default: break;
    }
}

// --- the side panel's Study section --------------------------------------------------------

RDE_INTERNAL b8 arabic_nav_open(fude_app* _app, u32 _screen) {
    arabic_app_open(_app, (ARABIC_SCREEN_)_screen);
    return true;
}

// Reviews: with none due, a notice instead, and the panel stays.
RDE_INTERNAL b8 arabic_nav_review(fude_app* _app, u32 _arg) {
    RDE_UNUSED(_arg);
    return fude_study_review(_app);
}

// Without the character data, what needs it is greyed out.
RDE_INTERNAL b8 arabic_nav_has_data(const fude_app* _app) { return fude_browse_available(FUDE_STUDY(_app)->browse); }
RDE_INTERNAL b8 arabic_nav_has_chart(const fude_app* _app) { return fude_chart_count(ARABIC_APP(_app)->chart) > 0u; }
RDE_INTERNAL b8 arabic_nav_has_album(const fude_app* _app) { return FUDE_STUDY(_app)->album->db != NULL; }

// The icons: letters of the script, each the start of the word it stands for.
static const fude_extension_nav_entry ARABIC_NAV_ENTRIES[] = {
    { FUDE_TEXT_ARABIC,     "\xD8\xA3",               15.0f, arabic_nav_open,   ARABIC_SCREEN_CHART,  arabic_nav_has_chart, NULL, 0 },                        // أ (أبجدية: alphabet)
    { FUDE_TEXT_ALBUM,      FUDE_ICON_BOOKS,      16.0f, arabic_nav_open,   ARABIC_SCREEN_ALBUM,  arabic_nav_has_album, NULL, 0 },
    { FUDE_TEXT_REVIEWS,    "\xD9\x85",               15.0f, arabic_nav_review, 0,                    arabic_nav_has_data,  fude_study_reviews_count, FUDE_TEXT_REVIEWS_N },   // م (مراجعة: review)
    { FUDE_TEXT_VOCAB,      "\xD9\x83",               15.0f, arabic_nav_open,   ARABIC_SCREEN_VOCAB,  NULL,                 NULL, 0 },                        // ك (كلمة: word)
    { FUDE_TEXT_LIBRARY_TITLE, FUDE_ICON_BOOK,    16.0f, arabic_nav_open,   ARABIC_SCREEN_LIBRARY, NULL,                NULL, 0 },                        // the lectures
    { FUDE_TEXT_EXAMS,      "\xD9\x81",               15.0f, arabic_nav_open,   ARABIC_SCREEN_EXAM,   arabic_nav_has_data,  NULL, 0 },                        // ف (فحص: test)
    { FUDE_TEXT_STATISTICS, FUDE_ICON_CHART_LINE, 16.0f, arabic_nav_open,   ARABIC_SCREEN_STATS,  NULL,                 NULL, 0 },
};
static const fude_extension_nav ARABIC_NAV = { FUDE_TEXT_SIDE_STUDY, ARABIC_NAV_ENTRIES, (u32)(sizeof(ARABIC_NAV_ENTRIES) / sizeof(ARABIC_NAV_ENTRIES[0])) };

// --- the welcome ------------------------------------------------------------------------------

// The very first launch: what Arabic is, and its gestures.
RDE_INTERNAL void arabic_first_launch(fude_app* _app) {
    fude_welcome_open(ARABIC_APP(_app)->welcome);
}

// The side panel's Tutorial: the welcome again.
RDE_INTERNAL void arabic_tutorial(fude_app* _app) {
    fude_welcome_open(ARABIC_APP(_app)->welcome);
}

// --- a developer's launch flags ---------------------------------------------------------------

RDE_INTERNAL struct {
    i32  argc;
    c8** argv;
    i32  welcome;   // --welcome[=PAGE]: the welcome at frame 10, on that page (-1: no)
    b8   demo;      // --demo: the welcome closed (the demo's saves are a learner's, not a first launch)
    b8   offline;   // --offline-note: why the app works offline (the must-read card) at frame 10
} arabic_look = { 0, NULL, -1, false, false };

void arabic_look_args(i32 _argc, c8** _argv) {
    fude_study_look_args(_argc, _argv);
    arabic_look.argc = _argc;
    arabic_look.argv = _argv;
    for(i32 _i = 1; _i < _argc; _i++) {
        const c8* _v;
        if(fude_look_is(_argv[_i], "--welcome"))                    { arabic_look.welcome = 0; }
        if(fude_look_is(_argv[_i], "--offline-note"))               { arabic_look.offline = true; }
        if(fude_look_is(_argv[_i], "--demo") || fude_look_value(_argv[_i], "--demo") != NULL) { arabic_look.demo = true; }
        if((_v = fude_look_value(_argv[_i], "--welcome")) != NULL) { arabic_look.welcome = (i32)strtol(_v, NULL, 10) - 1; }
    }
}

void arabic_look_start(fude_app* _app) {
    fude_study_look_start(_app);
    for(i32 _i = 1; _i < arabic_look.argc; _i++) {
        if(fude_look_is(arabic_look.argv[_i], "--chart")) { fude_chart_open(ARABIC_APP(_app)->chart); }
        if(fude_look_is(arabic_look.argv[_i], "--library")) { fude_library_open(ARABIC_APP(_app)->library); }
    }
}

void arabic_look_loaded(fude_app* _app) {
    fude_study_look_loaded(_app);
    if(arabic_look.demo) {
        ARABIC_APP(_app)->welcome->open = false;
    }
}

void arabic_look_frame(fude_app* _app) {
    fude_study_look_frame(_app);
    if(fude_look_shot_frame() == 10u && arabic_look.welcome >= 0) {
        fude_welcome_open(ARABIC_APP(_app)->welcome);
        ARABIC_APP(_app)->welcome->page = (u32)arabic_look.welcome < FUDE_WELCOME_PAGES ? (u32)arabic_look.welcome : 0u;
    }
    if(fude_look_shot_frame() == 10u && arabic_look.offline) {
        fude_welcome_offline(ARABIC_APP(_app)->welcome);
    }
}

// --- the extension ---------------------------------------------------------------------------

// The study's, and Arabic's own: its side panel, the welcome.
const fude_extension ARABIC_EXTENSION = {
    FUDE_STUDY_EXTENSION,
    .nav = &ARABIC_NAV, .tutorial = arabic_tutorial, .first_launch = arabic_first_launch,
    .library = ARABIC_LIBRARY, .library_count = (u32)(sizeof(ARABIC_LIBRARY) / sizeof(ARABIC_LIBRARY[0])),
};
