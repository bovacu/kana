// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_NOTICE
#define FUDE_NOTICE

#include "rde.h"

// ===========================================================================
// Notice: one line in a pill near the bottom of the screen that says what an action did (Copy as text: what it
// copied; a sheet: where it went; the hand: who writes now). It stays a moment, then fades. There is one at a time:
// a new one replaces the one showing. It is drawn over whatever is on screen, over the row of buttons if one is there.
//
// Where it sits: drawing/widgets. Anything may show a notice; the app's frame draws it (study/app/shell.c and the
// canvas apps).
//
// Its state: a single notice, kept by notice.c: its text, copied whole (on the std allocator, made by the first notice
// and kept for the app's life); there is nothing to create or free.
//
// Threads: main thread only.
// ===========================================================================

// Shows _text as the notice, replacing any notice showing now. The text is copied whole: a notice is never cut, as it
// may hold a name or a path of any length.
void fude_notice_show(const c8* _text);
// Draws the notice's pill, if one is showing; call it once a frame, last. _above is how far above the bottom of the
// safe area it sits, so it stays clear of a screen's row.
void fude_notice_render(rde_window* _window, rde_font* _font, f32 _font_px, f32 _above);

#endif
