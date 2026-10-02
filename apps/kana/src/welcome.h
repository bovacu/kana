#ifndef KANA_WELCOME
#define KANA_WELCOME

#include "rde.h"

// ===========================================================================
// The welcome: a few pages over everything the first time Kana opens (no
// settings saved yet) — what Kana is, the pen and the fingers, the long press
// and the lasso, where the rest is — because much of Kana is gestures nobody
// would guess. Skip, Next, and on the last page Start writing. Settings can show
// it again.
// ===========================================================================

#define KANA_WELCOME_PAGES 4u

RDE_STRUCT {
    b8         open;
    u32        page;
    // As laid out last frame (Kana's screen space), for the pointer.
    rde_vec_2F next_min, next_max;
    rde_vec_2F skip_min, skip_max;
    i32        pressed;   // 0 none, 1 Next, 2 Skip
} kana_welcome;

void kana_welcome_open(kana_welcome* _welcome);
// The pointer, in Kana's screen space: a press and its release on the same button.
void kana_welcome_pointer_down(kana_welcome* _welcome, rde_vec_2F _screen);
void kana_welcome_pointer_up(kana_welcome* _welcome, rde_vec_2F _screen);
// Over everything else (the page's own colour behind it).
void kana_welcome_render(kana_welcome* _welcome, rde_window* _window, rde_font* _font, f32 _font_px);

// The screen (screen.h): its rows of buttons, and what they do.
extern const struct fude_screen KANA_WELCOME_SCREEN;

#endif
