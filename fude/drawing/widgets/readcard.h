// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_READCARD
#define FUDE_READCARD

#include "rde.h"
#include "drawing/widgets/scroll.h"

// ===========================================================================
// A card that has to be read: a title, its text (scrolled when it is long; \n,
// the two characters, breaks a line, as a strings file holds no newline) and
// Close, which counts down and only then closes. A tap outside it, Escape or
// Back do nothing — the one way out is Close, once its wait is over. Over
// everything, in the theme's colours, as the welcome.
//
// Once read it can stay read: the owner gives a bit of fude_app.cards_read
// (saved with the settings) and Close sets it, so the owner opens it only while
// the bit is clear.
//
// One with no wait (0 seconds) is a plain card: Back closes it too. It can have a
// second button, at Close's left, that does something (the voice card's Open
// settings) and closes it.
// ===========================================================================

RDE_STRUCT {
    b8            open;
    u32           title;       // FUDE_TEXT_ ids: they follow a change of language
    u32           body;
    f32           seconds;     // Close's wait
    u8*           read;        // where Close sets `bit` (fude_app.cards_read); NULL: nowhere
    u8            bit;
    f64           opened_at;
    fude_scroller scroller;    // the text's
    // As laid out last frame (screen space), for the pointer.
    rde_vec_2F    text_min, text_max;
    rde_vec_2F    close_min, close_max;
    f32           content_h;   // the text's whole height
    b8            pressing;    // on Close
    u32           action;      // the second button's words (FUDE_TEXT_ id; FUDE_TEXT_COUNT: none)
    void        (*on_action)(void);
    rde_vec_2F    action_min, action_max;
    b8            pressing_action;
} fude_readcard;

// Opened with its words (FUDE_TEXT_ ids), Close's wait, and the bit Close sets in
// *_read (NULL: none).
void fude_readcard_open(fude_readcard* _card, u32 _title, u32 _body, f32 _seconds, u8* _read, u8 _bit);
// After opening: the second button, _text (FUDE_TEXT_ id), _on its press (then it closes).
void fude_readcard_set_action(fude_readcard* _card, u32 _text, void (*_on)(void));
// Close can close it now (its wait is over).
b8   fude_readcard_ready(const fude_readcard* _card);
// Seconds still to wait, rounded up (0: ready).
u32  fude_readcard_wait(const fude_readcard* _card);

void fude_readcard_pointer_down(fude_readcard* _card, rde_vec_2F _screen, f64 _now);
void fude_readcard_pointer_moved(fude_readcard* _card, rde_vec_2F _screen, f64 _now);
void fude_readcard_pointer_up(fude_readcard* _card, rde_vec_2F _screen, f64 _now);
void fude_readcard_update(fude_readcard* _card, f32 _dt);
void fude_readcard_render(fude_readcard* _card, rde_window* _window, rde_font* _font, f32 _font_px);

// The screen (screen.h): over everything, no row of buttons of its own.
extern const struct fude_screen FUDE_READCARD_SCREEN;

#endif
