#include "hanzi_app.h"
#include "version.h"
#include "drawing/base/text.h"
#include "drawing/widgets/icons.h"
#include "drawing/app/look.h"

#include <stdlib.h>

// ===========================================================================
// See hanzi_app.h.
// ===========================================================================

// What Hanzi is (info.h). Its credits are the short ones the character data's
// licences require be shown to users (assets/data/LICENSE-data.txt has them in
// full, with the stroke data's Arphic Public License); in full they are Settings
// › Licences (Data; ML Kit: Google's terms and notices).
const fude_app_info HANZI_INFO = {
    .name = HANZI_NAME, .id = HANZI_ID, .version = HANZI_VERSION, .store_id = HANZI_APP_STORE_ID,
    .script_font = "assets/fonts/NotoSansSC-Regular.otf", .credits = FUDE_TEXT_CREDITS,
    .licences = {
        { FUDE_TEXT_LICENCE_DATA,      { "assets/data/LICENSE-data.txt", "assets/data/ARPHICPL.TXT", NULL } },
        { FUDE_TEXT_LICENCE_FONTS,     { "assets/fonts/LICENSE-Roboto.txt", "assets/fonts/LICENSE-NotoSansCJK.txt", "assets/fonts/LICENSE-Phosphor.txt" } },
        { FUDE_TEXT_LICENCE_LIBRARIES, { "assets/licenses/libraries.txt", NULL, NULL } },
        { FUDE_TEXT_LICENCE_MLKIT,     { "assets/licenses/ml-kit-notices.txt", NULL, NULL } },
        { FUDE_TEXT_LICENCE_LECTURES,  { "assets/lectures/LICENSE-lectures.txt", NULL, NULL } },
    },
    .licence_count = 5u,
};

// --- the lectures (the Library's books: doc.h) ------------------------------------------------
// Free to share and change, also in an app that is sold: each one's licence and
// what Hanzi changed are in assets/lectures/LICENSE-lectures.txt (Settings ›
// Licences › Lectures), and how it was made in tools/lectures/. Ids
// are never reused: a canvas keeps its book's.
static const fude_doc_book HANZI_LIBRARY[] = {
    // Wikivoyage's Chinese phrasebook, one per language (the app's, else English).
    { 2u, "assets/lectures/phrasebook_en.pdf", FUDE_TEXT_LECTURE_PHRASEBOOK, FUDE_TEXT_LECTURE_PHRASEBOOK_ABOUT,
      "Wikivoyage contributors \xC2\xB7 CC BY-SA 4.0 \xC2\xB7 Hanzi edition: pictures and links left out", "assets/lectures/phrasebook_en.png", "en" },
    { 3u, "assets/lectures/phrasebook_es.pdf", FUDE_TEXT_LECTURE_PHRASEBOOK, FUDE_TEXT_LECTURE_PHRASEBOOK_ABOUT,
      "Colaboradores de Wikiviajes \xC2\xB7 CC BY-SA 4.0 \xC2\xB7 Edici\xC3\xB3n de Hanzi: sin im\xC3\xA1genes ni enlaces", "assets/lectures/phrasebook_es.png", "es" },
    { 4u, "assets/lectures/phrasebook_fr.pdf", FUDE_TEXT_LECTURE_PHRASEBOOK, FUDE_TEXT_LECTURE_PHRASEBOOK_ABOUT,
      "Contributeurs de Wikivoyage \xC2\xB7 CC BY-SA 4.0 \xC2\xB7 \xC3\x89" "dition de Hanzi : sans images ni liens", "assets/lectures/phrasebook_fr.png", "fr" },
    { 5u, "assets/lectures/phrasebook_pt.pdf", FUDE_TEXT_LECTURE_PHRASEBOOK, FUDE_TEXT_LECTURE_PHRASEBOOK_ABOUT,
      "Colaboradores do Wikivoyage \xC2\xB7 CC BY-SA 4.0 \xC2\xB7 Edi\xC3\xA7\xC3\xA3o do Hanzi: sem imagens nem links", "assets/lectures/phrasebook_pt.png", "pt" },
    { 6u, "assets/lectures/phrasebook_ja.pdf", FUDE_TEXT_LECTURE_PHRASEBOOK, FUDE_TEXT_LECTURE_PHRASEBOOK_ABOUT,
      "Wikivoyage\xE3\x81\xAE\xE5\x9F\xB7\xE7\xAD\x86\xE8\x80\x85 \xC2\xB7 CC BY-SA 4.0 \xC2\xB7 Hanzi\xE7\x89\x88", "assets/lectures/phrasebook_ja.png", "ja" },
};

void hanzi_app_open(fude_app* _app, HANZI_SCREEN_ _screen) {
    hanzi_app* _hanzi = HANZI_APP(_app);
    fude_lasso_clear(_app->lasso, _app->ink);
    switch(_screen) {
        case HANZI_SCREEN_BROWSE: fude_browse_open(_hanzi->study.browse); break;
        case HANZI_SCREEN_EXAM:   fude_exam_open(_hanzi->study.exam);       break;
        case HANZI_SCREEN_VOCAB:  fude_vocabview_open(_hanzi->study.vocab); break;
        case HANZI_SCREEN_STATS:  fude_stats_open(_hanzi->study.stats);     break;
        case HANZI_SCREEN_ALBUM:  fude_album_open(_hanzi->study.album);     break;
        case HANZI_SCREEN_LIBRARY: fude_library_open(_hanzi->library);      break;
        default: break;
    }
}

// --- the side panel's Study section --------------------------------------------------------

RDE_INTERNAL b8 hanzi_nav_open(fude_app* _app, u32 _screen) {
    hanzi_app_open(_app, (HANZI_SCREEN_)_screen);
    return true;
}

// Reviews: with none due, a notice instead, and the panel stays.
RDE_INTERNAL b8 hanzi_nav_review(fude_app* _app, u32 _arg) {
    RDE_UNUSED(_arg);
    return fude_study_review(_app);
}

// Without the character data, what needs it is greyed out.
RDE_INTERNAL b8 hanzi_nav_has_data(const fude_app* _app) { return fude_browse_available(FUDE_STUDY(_app)->browse); }
RDE_INTERNAL b8 hanzi_nav_has_album(const fude_app* _app) { return FUDE_STUDY(_app)->album->db != NULL; }

// The icons: the Chinese ones as characters.
static const fude_extension_nav_entry HANZI_NAV_ENTRIES[] = {
    { FUDE_TEXT_HANZI,      "\xE5\xAD\x97",       15.0f, hanzi_nav_open,   HANZI_SCREEN_BROWSE, hanzi_nav_has_data,  NULL, 0 },                        // 字
    { FUDE_TEXT_ALBUM,      FUDE_ICON_BOOKS,      16.0f, hanzi_nav_open,   HANZI_SCREEN_ALBUM,  hanzi_nav_has_album, NULL, 0 },
    { FUDE_TEXT_REVIEWS,    "\xE5\xA4\x8D",       15.0f, hanzi_nav_review, 0,                  hanzi_nav_has_data,  fude_study_reviews_count, FUDE_TEXT_REVIEWS_N },   // 复
    { FUDE_TEXT_VOCAB,      "\xE8\xAF\x8D",       15.0f, hanzi_nav_open,   HANZI_SCREEN_VOCAB,  NULL,               NULL, 0 },                        // 词
    { FUDE_TEXT_LIBRARY_TITLE, FUDE_ICON_BOOK,    16.0f, hanzi_nav_open,   HANZI_SCREEN_LIBRARY, NULL,              NULL, 0 },                        // the lectures
    { FUDE_TEXT_EXAMS,      "\xE8\xAF\x95",       15.0f, hanzi_nav_open,   HANZI_SCREEN_EXAM,   hanzi_nav_has_data,  NULL, 0 },                        // 试
    { FUDE_TEXT_STATISTICS, FUDE_ICON_CHART_LINE, 16.0f, hanzi_nav_open,   HANZI_SCREEN_STATS,  NULL,               NULL, 0 },
};
static const fude_extension_nav HANZI_NAV = { FUDE_TEXT_SIDE_STUDY, HANZI_NAV_ENTRIES, (u32)(sizeof(HANZI_NAV_ENTRIES) / sizeof(HANZI_NAV_ENTRIES[0])) };

// --- the welcome ------------------------------------------------------------------------------

// The very first launch: what Hanzi is, and its gestures.
RDE_INTERNAL void hanzi_first_launch(fude_app* _app) {
    fude_welcome_open(HANZI_APP(_app)->welcome);
}

// The side panel's Tutorial: the welcome again.
RDE_INTERNAL void hanzi_tutorial(fude_app* _app) {
    fude_welcome_open(HANZI_APP(_app)->welcome);
}

// --- a developer's launch flags ---------------------------------------------------------------

RDE_INTERNAL struct {
    i32  argc;
    c8** argv;
    i32  welcome;   // --welcome[=PAGE]: the welcome at frame 10, on that page (-1: no)
    b8   demo;      // --demo: the welcome closed (the demo's saves are a learner's, not a first launch)
} hanzi_look = { 0, NULL, -1, false };

void hanzi_look_args(i32 _argc, c8** _argv) {
    fude_study_look_args(_argc, _argv);
    hanzi_look.argc = _argc;
    hanzi_look.argv = _argv;
    for(i32 _i = 1; _i < _argc; _i++) {
        const c8* _v;
        if(fude_look_is(_argv[_i], "--welcome"))                    { hanzi_look.welcome = 0; }
        if(fude_look_is(_argv[_i], "--demo") || fude_look_value(_argv[_i], "--demo") != NULL) { hanzi_look.demo = true; }
        if((_v = fude_look_value(_argv[_i], "--welcome")) != NULL) { hanzi_look.welcome = (i32)strtol(_v, NULL, 10) - 1; }
    }
}

void hanzi_look_start(fude_app* _app) {
    fude_study_look_start(_app);
    for(i32 _i = 1; _i < hanzi_look.argc; _i++) {
        if(fude_look_is(hanzi_look.argv[_i], "--library")) { fude_library_open(HANZI_APP(_app)->library); }
    }
}

void hanzi_look_loaded(fude_app* _app) {
    fude_study_look_loaded(_app);
    if(hanzi_look.demo) {
        HANZI_APP(_app)->welcome->open = false;
    }
}

void hanzi_look_frame(fude_app* _app) {
    fude_study_look_frame(_app);
    if(fude_look_shot_frame() == 10u && hanzi_look.welcome >= 0) {
        fude_welcome_open(HANZI_APP(_app)->welcome);
        HANZI_APP(_app)->welcome->page = (u32)hanzi_look.welcome < FUDE_WELCOME_PAGES ? (u32)hanzi_look.welcome : 0u;
    }
}

// --- the extension ---------------------------------------------------------------------------

// The study's, and Hanzi's own: its side panel, the welcome.
const fude_extension HANZI_EXTENSION = {
    FUDE_STUDY_EXTENSION,
    .nav = &HANZI_NAV, .tutorial = hanzi_tutorial, .first_launch = hanzi_first_launch,
    .library = HANZI_LIBRARY, .library_count = (u32)(sizeof(HANZI_LIBRARY) / sizeof(HANZI_LIBRARY[0])),
};
