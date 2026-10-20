// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

// ===========================================================================
// Hanzi: learning to write Chinese by hand. This file says what the app is: its name, fonts and licences, its
// lectures, and its side panel's Study entries. The study apps' shell (study/app/shell.h) runs it, with all that
// every study app shares.
// ===========================================================================

#include "rde.h"
#include "study/app/shell.h"
#include "study/app/study.h"
#include "drawing/base/text.h"
#include "drawing/base/icons.h"
#include "version.h"

// Settings › Licences' documents.
static const fude_app_licence HANZI_LICENCES[] = {
    { FUDE_TEXT_LICENCE_DATA,      { "assets/data/LICENSE-data.txt", "assets/data/ARPHICPL.TXT", NULL } },
    { FUDE_TEXT_LICENCE_FONTS,     { "assets/fonts/LICENSE-Roboto.txt", "assets/fonts/LICENSE-NotoSansCJK.txt", "assets/fonts/LICENSE-Phosphor.txt" } },
    { FUDE_TEXT_LICENCE_LIBRARIES, { "assets/licenses/libraries.txt", NULL, NULL } },
    { FUDE_TEXT_LICENCE_MLKIT,     { "assets/licenses/ml-kit-notices.txt", NULL, NULL } },
    { FUDE_TEXT_LICENCE_LECTURES,  { "assets/lectures/LICENSE-lectures.txt", NULL, NULL } },
};

// What Hanzi is (info.h). Its credits are the short ones that the character data's licences require be shown to
// users; assets/data/LICENSE-data.txt has them in full, with the stroke data's Arphic Public License. The full texts
// are in Settings › Licences (Data; ML Kit: Google's terms and notices).
static const fude_app_info HANZI_INFO = {
    .name = HANZI_NAME, .id = HANZI_ID, .version = HANZI_VERSION, .store_id = HANZI_APP_STORE_ID,
    .script_font = "assets/fonts/NotoSansSC-Regular.otf", .credits = FUDE_TEXT_CREDITS,
    FUDE_APP_LICENCES_OF(HANZI_LICENCES),
};

// --- the lectures (the Library's books: doc.h) ------------------------------------------------
// Each is free to share and change, also in an app that is sold. Each one's licence, and what Hanzi changed, are in
// assets/lectures/LICENSE-lectures.txt (Settings › Licences › Lectures), and how each was made is in
// tools/lectures/. Ids are never reused, because a canvas keeps its book's id (fude_doc_book).
static const fude_doc_book HANZI_LIBRARY[] = {
    // Wikivoyage's Chinese phrasebook, one for each language; the Library shows the one in the app's
    // language, else the English one.
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

// --- the side panel's Study section -----------------------------------------------------------

// The icons: where an entry has a Chinese character for an icon, it is drawn as text (字, 复, 词, 试).
static const fude_extension_nav_entry HANZI_NAV_ENTRIES[] = {
    { FUDE_TEXT_HANZI,         "\xE5\xAD\x97",       15.0f, fude_study_nav_open,   FUDE_STUDY_SCREEN_BROWSE,  fude_study_nav_has_data,  NULL,                     0 },   // 字
    { FUDE_TEXT_ALBUM,         FUDE_ICON_BOOKS,      16.0f, fude_study_nav_open,   FUDE_STUDY_SCREEN_ALBUM,   fude_study_nav_has_album, NULL,                     0 },
    { FUDE_TEXT_REVIEWS,       "\xE5\xA4\x8D",       15.0f, fude_study_nav_review, 0,                         fude_study_nav_has_data,  fude_study_reviews_count, FUDE_TEXT_REVIEWS_N },   // 复
    { FUDE_TEXT_VOCAB,         "\xE8\xAF\x8D",       15.0f, fude_study_nav_open,   FUDE_STUDY_SCREEN_VOCAB,   NULL,                     NULL,                     0 },   // 词
    { FUDE_TEXT_LIBRARY_TITLE, FUDE_ICON_BOOK,       16.0f, fude_study_nav_open,   FUDE_STUDY_SCREEN_LIBRARY, NULL,                     NULL,                     0 },   // The lectures.
    { FUDE_TEXT_EXAMS,         "\xE8\xAF\x95",       15.0f, fude_study_nav_open,   FUDE_STUDY_SCREEN_EXAM,    fude_study_nav_has_data,  NULL,                     0 },   // 试
    { FUDE_TEXT_STATISTICS,    FUDE_ICON_CHART_LINE, 16.0f, fude_study_nav_open,   FUDE_STUDY_SCREEN_STATS,   NULL,                     NULL,                     0 },
};
static const fude_extension_nav HANZI_NAV = { FUDE_TEXT_SIDE_STUDY, HANZI_NAV_ENTRIES, (u32)(sizeof(HANZI_NAV_ENTRIES) / sizeof(HANZI_NAV_ENTRIES[0])) };

// The study's extension (FUDE_STUDY_EXTENSION), with Hanzi's own side panel and lectures added.
static const fude_extension HANZI_EXTENSION = {
    FUDE_STUDY_EXTENSION,
    .nav = &HANZI_NAV, .library = HANZI_LIBRARY, .library_count = (u32)(sizeof(HANZI_LIBRARY) / sizeof(HANZI_LIBRARY[0])),
};

// The notice the Arphic Public License asks for when the strokes are changed (its §2a): where they come from, how
// they were changed and when, and where the changed data is published. The bake writes it at the head of the strokes'
// own file (fude/lang/zh/bake.c).
#define HANZI_STROKES_NOTICE \
    "Hanzi's stroke data is Make Me a Hanzi's (github.com/skishore/makemeahanzi), via hanzi-writer-data 2.0.1, " \
    "derived from Arphic Technology's fonts and licensed under the Arphic Public License (ARPHICPL.TXT). " \
    "Changed by Hanzi's tools (apps/hanzi/tools/data/strokes.py), October 2026: each stroke's median turned " \
    "into cubic Bezier curves in a 109-unit box, then stored in this file's format (fixed point). " \
    "The changed data is published under the same licence: https://github.com/bovacu/hazi_custom_stroke_data"

// What the shell runs: what the app is, what it adds, and what its data bake needs (paths from the project root).
static const fude_study_shell_def HANZI_SHELL = {
    .info = &HANZI_INFO, .ext = &HANZI_EXTENSION,
    .bake = {
        .config = "apps/hanzi/assets/config.rdef",
        .out    = "apps/hanzi/assets/data/characters.kana",
        .tools  = "apps/hanzi/tools/data: fetch.py, strokes.py, prepare.py",
        .strokes_notice = HANZI_STROKES_NOTICE,   // The strokes in a file of their own, with it.
    },
};

// Runs the app: the study apps' shell does everything, with what is defined above.
int main(i32 _argc, c8* _argv[]) {
    return fude_study_shell_run(&HANZI_SHELL, _argc, _argv);
}
