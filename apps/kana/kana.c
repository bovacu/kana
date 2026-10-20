// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

// ===========================================================================
// Kana: learning to write Japanese by hand. This file says what the app is: its name, fonts and licences, its
// lectures, and its side panel's Study entries. The study apps' shell (study/app/shell.h) runs it, with all that
// every study app shares.
// ===========================================================================

#include "rde.h"
#include "study/app/shell.h"
#include "study/app/study.h"
#include "drawing/base/text.h"
#include "drawing/base/icons.h"
#include "version.h"

// Settings › Licences' documents (Data: the attribution KanjiVG and EDRDG require; ML Kit: Google's terms and
// notices).
static const fude_app_licence KANA_LICENCES[] = {
    { FUDE_TEXT_LICENCE_DATA,      { "assets/data/LICENSE-data.txt", NULL, NULL } },
    { FUDE_TEXT_LICENCE_FONTS,     { "assets/fonts/LICENSE-Roboto.txt", "assets/fonts/LICENSE-NotoSansJP.txt", "assets/fonts/LICENSE-Phosphor.txt" } },
    { FUDE_TEXT_LICENCE_LIBRARIES, { "assets/licenses/libraries.txt", NULL, NULL } },
    { FUDE_TEXT_LICENCE_MLKIT,     { "assets/licenses/ml-kit-notices.txt", NULL, NULL } },
    { FUDE_TEXT_LICENCE_LECTURES,  { "assets/lectures/LICENSE-lectures.txt", NULL, NULL } },
};

// What Kana is (info.h). Its credits are the short ones that the character data's licences require be shown to
// users; assets/data/LICENSE-data.txt has them in full, and the full texts are in Settings › Licences.
static const fude_app_info KANA_INFO = {
    .name = KANA_NAME, .id = KANA_ID, .version = KANA_VERSION, .store_id = KANA_APP_STORE_ID,
    .script_font = "assets/fonts/NotoSansJP-Regular.otf", .credits = FUDE_TEXT_CREDITS,
    FUDE_APP_LICENCES_OF(KANA_LICENCES),
};

// --- the lectures (the Library's books: doc.h) ------------------------------------------------
// Each is free to share and change, also in an app that is sold. Each one's licence, and what Kana changed, are in
// assets/lectures/LICENSE-lectures.txt (Settings › Licences › Lectures), and how each was made is in tools/lectures/
// (jpn101's is in Kana's own apps/kana/tools/lectures/). Ids are never reused, because a canvas keeps its book's id
// (fude_doc_book).
static const fude_doc_book KANA_LIBRARY[] = {
    { 2u, "assets/lectures/jpn101.pdf", FUDE_TEXT_LECTURE_JPN101, FUDE_TEXT_LECTURE_JPN101_ABOUT,
      "Yoko Sato, Mt Hood Community College \xC2\xB7 CC BY 4.0 \xC2\xB7 Kana edition: pages 1, 2 and 21 left out", "assets/lectures/jpn101.png", NULL },
    // Wikivoyage's Japanese phrasebook, one for each language; the Library shows the one in the app's
    // language, else the English one.
    { 3u, "assets/lectures/phrasebook_en.pdf", FUDE_TEXT_LECTURE_PHRASEBOOK, FUDE_TEXT_LECTURE_PHRASEBOOK_ABOUT,
      "Wikivoyage contributors \xC2\xB7 CC BY-SA 4.0 \xC2\xB7 Kana edition: pictures and links left out", "assets/lectures/phrasebook_en.png", "en" },
    { 4u, "assets/lectures/phrasebook_es.pdf", FUDE_TEXT_LECTURE_PHRASEBOOK, FUDE_TEXT_LECTURE_PHRASEBOOK_ABOUT,
      "Colaboradores de Wikiviajes \xC2\xB7 CC BY-SA 4.0 \xC2\xB7 Edici\xC3\xB3n de Kana: sin im\xC3\xA1genes ni enlaces", "assets/lectures/phrasebook_es.png", "es" },
    { 5u, "assets/lectures/phrasebook_fr.pdf", FUDE_TEXT_LECTURE_PHRASEBOOK, FUDE_TEXT_LECTURE_PHRASEBOOK_ABOUT,
      "Contributeurs de Wikivoyage \xC2\xB7 CC BY-SA 4.0 \xC2\xB7 \xC3\x89" "dition de Kana : sans images ni liens", "assets/lectures/phrasebook_fr.png", "fr" },
    { 6u, "assets/lectures/phrasebook_pt.pdf", FUDE_TEXT_LECTURE_PHRASEBOOK, FUDE_TEXT_LECTURE_PHRASEBOOK_ABOUT,
      "Colaboradores do Wikivoyage \xC2\xB7 CC BY-SA 4.0 \xC2\xB7 Edi\xC3\xA7\xC3\xA3o do Kana: sem imagens nem links", "assets/lectures/phrasebook_pt.png", "pt" },
};

// --- the side panel's Study section -----------------------------------------------------------

// The icons: where an entry has a Japanese character for an icon, it is drawn as text (字, あ, 復, 語, 試).
static const fude_extension_nav_entry KANA_NAV_ENTRIES[] = {
    { FUDE_TEXT_KANJI,         "\xE5\xAD\x97",       15.0f, fude_study_nav_open,   FUDE_STUDY_SCREEN_BROWSE,  fude_study_nav_has_data,  NULL,                     0 },   // 字
    { FUDE_TEXT_KANA,          "\xE3\x81\x82",       15.0f, fude_study_nav_open,   FUDE_STUDY_SCREEN_CHART,   fude_study_nav_has_chart, NULL,                     0 },   // あ
    { FUDE_TEXT_ALBUM,         FUDE_ICON_BOOKS,      16.0f, fude_study_nav_open,   FUDE_STUDY_SCREEN_ALBUM,   fude_study_nav_has_album, NULL,                     0 },
    { FUDE_TEXT_REVIEWS,       "\xE5\xBE\xA9",       15.0f, fude_study_nav_review, 0,                         fude_study_nav_has_data,  fude_study_reviews_count, FUDE_TEXT_REVIEWS_N },   // 復
    { FUDE_TEXT_VOCAB,         "\xE8\xAA\x9E",       15.0f, fude_study_nav_open,   FUDE_STUDY_SCREEN_VOCAB,   NULL,                     NULL,                     0 },   // 語
    { FUDE_TEXT_LIBRARY_TITLE, FUDE_ICON_BOOK,       16.0f, fude_study_nav_open,   FUDE_STUDY_SCREEN_LIBRARY, NULL,                     NULL,                     0 },   // The lectures.
    { FUDE_TEXT_EXAMS,         "\xE8\xA9\xA6",       15.0f, fude_study_nav_open,   FUDE_STUDY_SCREEN_EXAM,    fude_study_nav_has_data,  NULL,                     0 },   // 試
    { FUDE_TEXT_STATISTICS,    FUDE_ICON_CHART_LINE, 16.0f, fude_study_nav_open,   FUDE_STUDY_SCREEN_STATS,   NULL,                     NULL,                     0 },
};
static const fude_extension_nav KANA_NAV = { FUDE_TEXT_SIDE_STUDY, KANA_NAV_ENTRIES, (u32)(sizeof(KANA_NAV_ENTRIES) / sizeof(KANA_NAV_ENTRIES[0])) };

// The study's extension (FUDE_STUDY_EXTENSION), with Kana's own side panel and lectures added.
static const fude_extension KANA_EXTENSION = {
    FUDE_STUDY_EXTENSION,
    .nav = &KANA_NAV, .library = KANA_LIBRARY, .library_count = (u32)(sizeof(KANA_LIBRARY) / sizeof(KANA_LIBRARY[0])),
};

// What the shell runs: what the app is, what it adds, and what its data bake needs (paths from the project root).
static const fude_study_shell_def KANA_SHELL = {
    .info = &KANA_INFO, .ext = &KANA_EXTENSION,
    .bake = { .config = "apps/kana/assets/config.rdef" },
};

// Runs the app: the study apps' shell does everything, with what is defined above.
int main(i32 _argc, c8* _argv[]) {
    return fude_study_shell_run(&KANA_SHELL, _argc, _argv);
}
