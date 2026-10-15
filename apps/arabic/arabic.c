// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

// ===========================================================================
// Arabic: learning to write Arabic by hand, in the Naskh hand. This file says what the app is: its name, fonts and
// licences, its lectures, and its side panel's Study entries. The study apps' shell (study/app/shell.h) runs it,
// with all that every study app shares.
// ===========================================================================

#include "rde.h"
#include "study/app/shell.h"
#include "study/app/study.h"
#include "drawing/base/text.h"
#include "drawing/base/icons.h"
#include "version.h"

// Settings › Licences' documents.
static const fude_app_licence ARABIC_LICENCES[] = {
    { FUDE_TEXT_LICENCE_DATA,      { "assets/data/LICENSE-data.txt", NULL, NULL } },
    { FUDE_TEXT_LICENCE_FONTS,     { "assets/fonts/LICENSE-Roboto.txt", "assets/fonts/LICENSE-Noto.txt", "assets/fonts/LICENSE-Phosphor.txt" } },
    { FUDE_TEXT_LICENCE_LIBRARIES, { "assets/licenses/libraries.txt", NULL, NULL } },
    { FUDE_TEXT_LICENCE_MLKIT,     { "assets/licenses/ml-kit-notices.txt", NULL, NULL } },
    { FUDE_TEXT_LICENCE_LECTURES,  { "assets/lectures/LICENSE-lectures.txt", NULL, NULL } },
};

// What Arabic is (info.h). Its credits are the short ones that the character data's licences require be shown to
// users; assets/data/LICENSE-data.txt has them in full. The full texts are in Settings › Licences (Data: each
// source's attribution; ML Kit: Google's terms and notices).
static const fude_app_info ARABIC_INFO = {
    .name = ARABIC_NAME, .id = ARABIC_ID, .version = ARABIC_VERSION, .store_id = ARABIC_APP_STORE_ID,
    .script_font = "assets/fonts/NotoNaskhArabic-Regular.ttf", .latin_font = "assets/fonts/NotoSansLatinExt-Regular.ttf",
    .credits = FUDE_TEXT_CREDITS,
    FUDE_APP_LICENCES_OF(ARABIC_LICENCES),
};

// --- the lectures (the Library's books: doc.h) ------------------------------------------------
// Each is free to share and change, also in an app that is sold. Each one's licence, and what Arabic Learn! changed,
// are in assets/lectures/LICENSE-lectures.txt (Settings › Licences › Lectures), and how each was made is in
// tools/lectures/. Ids are never reused, because a canvas keeps its book's id (fude_doc_book).
static const fude_doc_book ARABIC_LIBRARY[] = {
    // Wikivoyage's Arabic phrasebook, one for each language it is in; the Library shows the one in the app's
    // language, else the English one.
    { 2u, "assets/lectures/phrasebook_en.pdf", FUDE_TEXT_LECTURE_PHRASEBOOK, FUDE_TEXT_LECTURE_PHRASEBOOK_ABOUT,
      "Wikivoyage contributors \xC2\xB7 CC BY-SA 4.0 \xC2\xB7 Arabic Learn! edition: pictures and links left out", "assets/lectures/phrasebook_en.png", "en" },
    { 3u, "assets/lectures/phrasebook_es.pdf", FUDE_TEXT_LECTURE_PHRASEBOOK, FUDE_TEXT_LECTURE_PHRASEBOOK_ABOUT,
      "Colaboradores de Wikiviajes \xC2\xB7 CC BY-SA 4.0 \xC2\xB7 Edici\xC3\xB3n de Arabic Learn!: sin im\xC3\xA1genes ni enlaces", "assets/lectures/phrasebook_es.png", "es" },
    { 4u, "assets/lectures/phrasebook_fr.pdf", FUDE_TEXT_LECTURE_PHRASEBOOK, FUDE_TEXT_LECTURE_PHRASEBOOK_ABOUT,
      "Contributeurs de Wikivoyage \xC2\xB7 CC BY-SA 4.0 \xC2\xB7 \xC3\x89" "dition de Arabic Learn! : sans images ni liens", "assets/lectures/phrasebook_fr.png", "fr" },
    { 5u, "assets/lectures/phrasebook_pt.pdf", FUDE_TEXT_LECTURE_PHRASEBOOK, FUDE_TEXT_LECTURE_PHRASEBOOK_ABOUT,
      "Colaboradores do Wikivoyage \xC2\xB7 CC BY-SA 4.0 \xC2\xB7 Edi\xC3\xA7\xC3\xA3o do Arabic Learn!: sem imagens nem links", "assets/lectures/phrasebook_pt.png", "pt" },
};

// --- the side panel's Study section -----------------------------------------------------------

// The icons: letters of the script, each the first letter of the word it stands for.
static const fude_extension_nav_entry ARABIC_NAV_ENTRIES[] = {
    { FUDE_TEXT_ARABIC,        "\xD8\xA3",           15.0f, fude_study_nav_open,   FUDE_STUDY_SCREEN_CHART,   fude_study_nav_has_chart, NULL,                     0 },   // أ (أبجدية: alphabet)
    { FUDE_TEXT_ALBUM,         FUDE_ICON_BOOKS,      16.0f, fude_study_nav_open,   FUDE_STUDY_SCREEN_ALBUM,   fude_study_nav_has_album, NULL,                     0 },
    { FUDE_TEXT_REVIEWS,       "\xD9\x85",           15.0f, fude_study_nav_review, 0,                         fude_study_nav_has_data,  fude_study_reviews_count, FUDE_TEXT_REVIEWS_N },   // م (مراجعة: review)
    { FUDE_TEXT_VOCAB,         "\xD9\x83",           15.0f, fude_study_nav_open,   FUDE_STUDY_SCREEN_VOCAB,   NULL,                     NULL,                     0 },   // ك (كلمة: word)
    { FUDE_TEXT_LIBRARY_TITLE, FUDE_ICON_BOOK,       16.0f, fude_study_nav_open,   FUDE_STUDY_SCREEN_LIBRARY, NULL,                     NULL,                     0 },   // The lectures.
    { FUDE_TEXT_EXAMS,         "\xD9\x81",           15.0f, fude_study_nav_open,   FUDE_STUDY_SCREEN_EXAM,    fude_study_nav_has_data,  NULL,                     0 },   // ف (فحص: test)
    { FUDE_TEXT_STATISTICS,    FUDE_ICON_CHART_LINE, 16.0f, fude_study_nav_open,   FUDE_STUDY_SCREEN_STATS,   NULL,                     NULL,                     0 },
};
static const fude_extension_nav ARABIC_NAV = { FUDE_TEXT_SIDE_STUDY, ARABIC_NAV_ENTRIES, (u32)(sizeof(ARABIC_NAV_ENTRIES) / sizeof(ARABIC_NAV_ENTRIES[0])) };

// The study's extension (FUDE_STUDY_EXTENSION), with Arabic's own side panel and lectures added.
static const fude_extension ARABIC_EXTENSION = {
    FUDE_STUDY_EXTENSION,
    .nav = &ARABIC_NAV, .library = ARABIC_LIBRARY, .library_count = (u32)(sizeof(ARABIC_LIBRARY) / sizeof(ARABIC_LIBRARY[0])),
};

// What the shell runs: what the app is, what it adds, and its config as the data bake sees it.
static const fude_study_shell_def ARABIC_SHELL = {
    .info = &ARABIC_INFO, .ext = &ARABIC_EXTENSION,
    .bake_config = "apps/arabic/assets/config.rdef",
};

// Runs the app: the study apps' shell does everything, with what is defined above.
int main(i32 _argc, c8* _argv[]) {
    return fude_study_shell_run(&ARABIC_SHELL, _argc, _argv);
}
