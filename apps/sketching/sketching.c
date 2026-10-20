// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

// ===========================================================================
// Sketching (a working name): the deep-zoom drawing app, where anyone can zoom in or out as far as they like, and
// draw at every depth (docs/infinite_canvas_design.md). It is the developer's build, with everything in it: every
// topic and tool of the canvas, the formulas too (docs/product_split.md). This file says what the app is: its name,
// fonts and licences, what it offers, and its home screen. The canvas apps' shell (zoom/shell.h) runs it, with all
// that every canvas app shares; a developer's launch flags are zoom/look.h's.
// ===========================================================================

#include "rde.h"
#include "zoom/shell.h"
#include "zoom/product.h"
#include "drawing/base/text.h"
#include "version.h"

// Settings › Licences' documents: the fonts, the maths font (formulas), the Hershey letters (handwriting written out),
// every library built into the app (tools/licences/build.py writes the file), and ML Kit's notices (its handwriting
// reader).
static const fude_app_licence SKETCHING_LICENCES[] = {
    { FUDE_TEXT_LICENCE_FONTS,       { "assets/fonts/LICENSE-Roboto.txt", "assets/fonts/LICENSE-NotoSansJP.txt", "assets/fonts/LICENSE-Phosphor.txt" } },
    { FUDE_TEXT_ZOOM_LICENCE_MATHS,  { "assets/fonts/LICENSE-NotoSansMath.txt", NULL, NULL } },
    { FUDE_TEXT_ZOOM_LICENCE_HAND,   { "assets/fonts/LICENSE-Hershey.txt", NULL, NULL } },
    { FUDE_TEXT_LICENCE_LIBRARIES,   { "assets/licenses/libraries.txt", NULL, NULL } },
    { FUDE_TEXT_LICENCE_MLKIT,       { "assets/licenses/ml-kit-notices.txt", NULL, NULL } },
};

// What Sketching is (info.h; its name and version come from version.h). Its script font holds only the language
// picker's names (Noto Sans JP, cut down).
static const fude_app_info SKETCHING_INFO = {
    .name = SKETCHING_NAME, .version = SKETCHING_VERSION, .store_id = SKETCHING_APP_STORE_ID,
    .script_font = "assets/fonts/NotoSansJP-Names.otf", .credits = FUDE_TEXT_COUNT,
    FUDE_APP_LICENCES_OF(SKETCHING_LICENCES),
    .turns = true,   // It turns to landscape too, for more room to draw (Borja asked, 2026-10-05).
};

// What the shell runs: every topic and tool (product.h's Sketching), a home screen of plain canvases with no welcome,
// and a launch on the canvas last open.
static const fude_zoom_shell_def SKETCHING_SHELL = {
    .info       = &SKETCHING_INFO,
    .product    = FUDE_ZOOM_PRODUCT_ALL,
    .home_title = FUDE_TEXT_ZOOM_HOME_CANVASES,
    .home_new   = FUDE_TEXT_ZOOM_HOME_NEW_CANVAS,
};

// Runs the app: the canvas apps' shell does everything, with what is defined above.
int main(i32 _argc, c8* _argv[]) {
    return fude_zoom_shell_run(&SKETCHING_SHELL, _argc, _argv);
}
