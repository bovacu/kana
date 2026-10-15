// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_THEMES_H
#define FUDE_ZOOM_THEMES_H

#include "rde.h"

// ===========================================================================
// Themes: the workshop's own set of colour themes, put in the places of the core's themes (drawing/base/theme.h). Paper
// and Night stay as the core has them; in place of the language apps' Washi, Matcha and Sakura come three of a
// workshop's:
//
//   KRAFT        brown wrapping paper, dark ink, a burnt orange accent
//   BLUEPRINT    a cyanotype's deep blue, white lines, a yellow accent
//   CUTTING MAT  a self-healing mat's green, white lines, an amber accent
//
// Where it sits: fude/zoom. The canvas apps' shell (zoom/shell.c) calls it at start-up when the product
// is a workshop one.
// Its main types and their lifecycle: the three themes are constant tables in themes.c; the set handed to the core
// lives in themes.c for the whole run.
// Threads: main thread only.
// Design: an export still prints on white with the Paper theme's ink, whichever theme is on the screen, which is why
// Paper keeps its place.
// ===========================================================================

// Puts the workshop's themes in place of the core's (through fude_theme_use). Call it before the UI is built.
void fude_zoom_themes_use(void);

#endif
