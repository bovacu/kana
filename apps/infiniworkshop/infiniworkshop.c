// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

// ===========================================================================
// InfiniWorkshop: the hobby-projects app made from the deep-zoom canvas (docs/product_split.md): circuits that run,
// mechanisms that move, plans at true size, wood and sewing. This file says what the app is: its name, fonts and
// licences, what it offers, its home screen of projects and its welcome. The canvas apps' shell (zoom/shell.h) runs
// it, with all that every canvas app shares.
// ===========================================================================

#include "rde.h"
#include "zoom/shell.h"
#include "zoom/product.h"
#include "drawing/base/text.h"
#include "version.h"

// Settings › Licences' documents: the fonts, the maths font (formulas), the Hershey letters (handwriting written out),
// every library built into the app (tools/licences/build.py writes the file), and ML Kit's notices (its handwriting
// reader).
static const fude_app_licence INFINIWORKSHOP_LICENCES[] = {
    { FUDE_TEXT_LICENCE_FONTS,       { "assets/fonts/LICENSE-Roboto.txt", "assets/fonts/LICENSE-NotoSansJP.txt", "assets/fonts/LICENSE-Phosphor.txt" } },
    { FUDE_TEXT_ZOOM_LICENCE_MATHS,  { "assets/fonts/LICENSE-NotoSansMath.txt", NULL, NULL } },
    { FUDE_TEXT_ZOOM_LICENCE_HAND,   { "assets/fonts/LICENSE-Hershey.txt", NULL, NULL } },
    { FUDE_TEXT_LICENCE_LIBRARIES,   { "assets/licenses/libraries.txt", NULL, NULL } },
    { FUDE_TEXT_LICENCE_MLKIT,       { "assets/licenses/ml-kit-notices.txt", NULL, NULL } },
};

// What InfiniWorkshop is (info.h; its name and version come from version.h). Its script font holds only the language
// picker's names (Noto Sans JP, cut down). It opens Sketching's Your data files too (Settings › Your data › Import),
// so that what was drawn there carries over (docs/product_split.md §9).
static const fude_app_info INFINIWORKSHOP_INFO = {
    .name = INFINIWORKSHOP_NAME, .imports = "sketching", .version = INFINIWORKSHOP_VERSION, .store_id = INFINIWORKSHOP_APP_STORE_ID,
    .script_font = "assets/fonts/NotoSansJP-Names.otf", .credits = FUDE_TEXT_COUNT,
    FUDE_APP_LICENCES_OF(INFINIWORKSHOP_LICENCES),
    .turns = true,   // It turns to landscape too, for more room to draw.
};

// The welcome, over the home screen the first time (and again from the side panel's Tutorial): the examples, building
// one's own, drawing; its first step puts the examples on a canvas.
static const fude_zoom_shell_welcome INFINIWORKSHOP_WELCOME = {
    .title = FUDE_TEXT_ZOOM_WELCOME_TITLE,
    .lines = { FUDE_TEXT_ZOOM_WELCOME_WORKSHOP_EXAMPLES, FUDE_TEXT_ZOOM_WELCOME_WORKSHOP_BUILD, FUDE_TEXT_ZOOM_WELCOME_WORKSHOP_DRAW },
    .first = FUDE_TEXT_ZOOM_WELCOME_EXAMPLES,
};

// What the shell runs: Workshop's topics and tools and its themes (product.h), a home screen of projects with its
// welcome, and a launch on that home screen.
static const fude_zoom_shell_def INFINIWORKSHOP_SHELL = {
    .info       = &INFINIWORKSHOP_INFO,
    .product    = FUDE_ZOOM_PRODUCT_WORKSHOP,
    .home_title = FUDE_TEXT_ZOOM_HOME_PROJECTS,
    .home_new   = FUDE_TEXT_ZOOM_HOME_NEW_PROJECT,
    .welcome    = &INFINIWORKSHOP_WELCOME,
    .opens_home = true,
};

// Runs the app: the canvas apps' shell does everything, with what is defined above.
int main(i32 _argc, c8* _argv[]) {
    return fude_zoom_shell_run(&INFINIWORKSHOP_SHELL, _argc, _argv);
}
