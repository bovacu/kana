// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

// ===========================================================================
// InfiNote: the note-taking app made from the deep-zoom canvas (docs/product_split.md): writing and drawing on pages
// that zoom without end, PDFs, maths, diagrams and recordings. This file says what the app is: its name, fonts and
// licences, what it offers, and its home screen of notebooks. The canvas apps' shell (zoom/shell.h) runs it, with all
// that every canvas app shares.
// ===========================================================================

#include "rde.h"
#include "zoom/shell.h"
#include "zoom/product.h"
#include "zoom/pages.h"
#include "drawing/base/text.h"
#include "version.h"

// Settings › Licences' documents: the fonts, the maths font (formulas), the Hershey letters (handwriting written out),
// every library built into the app (tools/licences/build.py writes the file), and ML Kit's notices (its handwriting
// reader).
static const fude_app_licence INFINOTE_LICENCES[] = {
    { FUDE_TEXT_LICENCE_FONTS,       { "assets/fonts/LICENSE-Roboto.txt", "assets/fonts/LICENSE-NotoSansJP.txt", "assets/fonts/LICENSE-Phosphor.txt" } },
    { FUDE_TEXT_ZOOM_LICENCE_MATHS,  { "assets/fonts/LICENSE-NotoSansMath.txt", NULL, NULL } },
    { FUDE_TEXT_ZOOM_LICENCE_HAND,   { "assets/fonts/LICENSE-Hershey.txt", NULL, NULL } },
    { FUDE_TEXT_LICENCE_LIBRARIES,   { "assets/licenses/libraries.txt", NULL, NULL } },
    { FUDE_TEXT_LICENCE_MLKIT,       { "assets/licenses/ml-kit-notices.txt", NULL, NULL } },
};

// What InfiNote is (info.h; its name and version come from version.h). Its script font holds only the language
// picker's names (Noto Sans JP, cut down). It opens Sketching's Your data files too (Settings › Your data › Import),
// so that what was drawn there carries over (docs/product_split.md §9).
static const fude_app_info INFINOTE_INFO = {
    .name = INFINOTE_NAME, .imports = "sketching", .version = INFINOTE_VERSION, .store_id = INFINOTE_APP_STORE_ID,
    .script_font = "assets/fonts/NotoSansJP-Names.otf", .credits = FUDE_TEXT_COUNT,
    FUDE_APP_LICENCES_OF(INFINOTE_LICENCES),
    .turns = true,   // It turns to landscape too, for more room to draw.
};

// What the shell runs: Notes' topics and tools (product.h), a home screen of notebooks whose New makes a notebook of
// A4 pages, and a launch on that home screen.
static const fude_zoom_shell_def INFINOTE_SHELL = {
    .info         = &INFINOTE_INFO,
    .product      = FUDE_ZOOM_PRODUCT_NOTES,
    .home_title   = FUDE_TEXT_ZOOM_HOME_NOTEBOOKS,
    .home_new     = FUDE_TEXT_ZOOM_HOME_NEW_NOTEBOOK,
    .new_notebook = true,
    .new_pages    = FUDE_ZOOM_PAGES_A4,
    .opens_home   = true,
};

// Runs the app: the canvas apps' shell does everything, with what is defined above.
int main(i32 _argc, c8* _argv[]) {
    return fude_zoom_shell_run(&INFINOTE_SHELL, _argc, _argv);
}
