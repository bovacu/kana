// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

// ===========================================================================
// Hangul: learning to write Korean by hand. This file says what the app is: its name, fonts and licences, its
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
static const fude_app_licence HANGUL_LICENCES[] = {
    { FUDE_TEXT_LICENCE_DATA,      { "assets/data/LICENSE-data.txt", NULL, NULL } },
    { FUDE_TEXT_LICENCE_FONTS,     { "assets/fonts/LICENSE-Roboto.txt", "assets/fonts/LICENSE-NotoSansCJK.txt", "assets/fonts/LICENSE-Phosphor.txt" } },
    { FUDE_TEXT_LICENCE_LIBRARIES, { "assets/licenses/libraries.txt", NULL, NULL } },
    { FUDE_TEXT_LICENCE_MLKIT,     { "assets/licenses/ml-kit-notices.txt", NULL, NULL } },
    { FUDE_TEXT_LICENCE_LECTURES,  { "assets/lectures/LICENSE-lectures.txt", NULL, NULL } },
};

// What Hangul is (info.h). Its credits are the short ones that the character data's licences require be shown to
// users; assets/data/LICENSE-data.txt has them in full. The full texts are in Settings › Licences (Data: the
// attribution NIKL, KanjiVG and EDRDG require; ML Kit: Google's terms and notices).
static const fude_app_info HANGUL_INFO = {
    .name = HANGUL_NAME, .id = HANGUL_ID, .version = HANGUL_VERSION, .store_id = HANGUL_APP_STORE_ID,
    .script_font = "assets/fonts/NotoSansKR-Regular.otf", .credits = FUDE_TEXT_CREDITS,
    FUDE_APP_LICENCES_OF(HANGUL_LICENCES),
};

// --- the lectures (the Library's books: doc.h) ------------------------------------------------
// Each is free to share and change, also in an app that is sold. Each one's licence, and what Hangul changed, are in
// assets/lectures/LICENSE-lectures.txt (Settings › Licences › Lectures), and how each was made is in
// tools/lectures/. Ids are never reused, because a canvas keeps its book's id (fude_doc_book).
static const fude_doc_book HANGUL_LIBRARY[] = {
    // Wikivoyage's Korean phrasebook, one for each language; the Library shows the one in the app's
    // language, else the English one.
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

// --- the side panel's Study section -----------------------------------------------------------

// The icons: where Kana has a character, Hangul has that character as Korean reads it, written in Hangul
// (復 복, 語 어, 試 시).
static const fude_extension_nav_entry HANGUL_NAV_ENTRIES[] = {
    { FUDE_TEXT_HANGUL_CHARACTERS, "\xEA\xB8\x80",       15.0f, fude_study_nav_open,   FUDE_STUDY_SCREEN_BROWSE,  fude_study_nav_has_data,  NULL,                     0 },   // 글
    { FUDE_TEXT_HANGUL,            "\xEA\xB0\x80",       15.0f, fude_study_nav_open,   FUDE_STUDY_SCREEN_CHART,   fude_study_nav_has_chart, NULL,                     0 },   // 가
    { FUDE_TEXT_ALBUM,             FUDE_ICON_BOOKS,      16.0f, fude_study_nav_open,   FUDE_STUDY_SCREEN_ALBUM,   fude_study_nav_has_album, NULL,                     0 },
    { FUDE_TEXT_REVIEWS,           "\xEB\xB3\xB5",       15.0f, fude_study_nav_review, 0,                         fude_study_nav_has_data,  fude_study_reviews_count, FUDE_TEXT_REVIEWS_N },   // 복
    { FUDE_TEXT_VOCAB,             "\xEC\x96\xB4",       15.0f, fude_study_nav_open,   FUDE_STUDY_SCREEN_VOCAB,   NULL,                     NULL,                     0 },   // 어
    { FUDE_TEXT_LIBRARY_TITLE,     FUDE_ICON_BOOK,       16.0f, fude_study_nav_open,   FUDE_STUDY_SCREEN_LIBRARY, NULL,                     NULL,                     0 },   // The lectures.
    { FUDE_TEXT_EXAMS,             "\xEC\x8B\x9C",       15.0f, fude_study_nav_open,   FUDE_STUDY_SCREEN_EXAM,    fude_study_nav_has_data,  NULL,                     0 },   // 시
    { FUDE_TEXT_STATISTICS,        FUDE_ICON_CHART_LINE, 16.0f, fude_study_nav_open,   FUDE_STUDY_SCREEN_STATS,   NULL,                     NULL,                     0 },
};
static const fude_extension_nav HANGUL_NAV = { FUDE_TEXT_SIDE_STUDY, HANGUL_NAV_ENTRIES, (u32)(sizeof(HANGUL_NAV_ENTRIES) / sizeof(HANGUL_NAV_ENTRIES[0])) };

// The study's extension (FUDE_STUDY_EXTENSION), with Hangul's own side panel and lectures added.
static const fude_extension HANGUL_EXTENSION = {
    FUDE_STUDY_EXTENSION,
    .nav = &HANGUL_NAV, .library = HANGUL_LIBRARY, .library_count = (u32)(sizeof(HANGUL_LIBRARY) / sizeof(HANGUL_LIBRARY[0])),
};

// What the shell runs: what the app is, what it adds, and what its data bake needs (paths from the project root).
static const fude_study_shell_def HANGUL_SHELL = {
    .info = &HANGUL_INFO, .ext = &HANGUL_EXTENSION,
    .bake = {
        .config = "apps/hangul/assets/config.rdef",
        .out    = "apps/hangul/assets/data/characters.kana",
        .tools  = "apps/hangul/tools: strokes/compose.py, data/fetch.py, data/prepare.py",
    },
};

// Runs the app: the study apps' shell does everything, with what is defined above.
int main(i32 _argc, c8* _argv[]) {
    return fude_study_shell_run(&HANGUL_SHELL, _argc, _argv);
}
