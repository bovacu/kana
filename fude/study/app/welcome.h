#ifndef FUDE_STUDY_WELCOME_H
#define FUDE_STUDY_WELCOME_H

#include "rde.h"

// ===========================================================================
// The welcome: a few pages over everything the first time a study app opens (no
// settings saved yet) — what the app is, the pen and the fingers, reading text
// with the camera, the lasso and pasting text as handwriting, translating, what to
// study, and the progress kept — because much of it is gestures and features
// nobody would guess are there. Skip, Next, and on the last page Start writing. Settings can show it
// again. Its words are the app's own (WELCOME_1_TITLE, WELCOME_1... in its
// strings tool).
// ===========================================================================

#define FUDE_WELCOME_PAGES 7u

RDE_STRUCT {
    b8         open;
    u32        page;
    // As laid out last frame (screen space), for the pointer.
    rde_vec_2F next_min, next_max;
    rde_vec_2F skip_min, skip_max;
    i32        pressed;   // 0 none, 1 Next, 2 Skip
} fude_welcome;

void fude_welcome_open(fude_welcome* _welcome);
// The pointer, in screen space: a press and its release on the same button.
void fude_welcome_pointer_down(fude_welcome* _welcome, rde_vec_2F _screen);
void fude_welcome_pointer_up(fude_welcome* _welcome, rde_vec_2F _screen);
// Over everything else (the page's own colour behind it).
void fude_welcome_render(fude_welcome* _welcome, rde_window* _window, rde_font* _font, f32 _font_px);

// The screen (screen.h): its rows of buttons, and what they do.
extern const struct fude_screen FUDE_WELCOME_SCREEN;

#endif
