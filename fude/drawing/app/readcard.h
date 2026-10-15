// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_READCARD
#define FUDE_READCARD

#include "rde.h"
#include "drawing/widgets/scroll.h"

// ===========================================================================
// Read card: a card that has to be read. It has a title, its text (scrolled when it is long; "\n", the two
// characters, breaks a line, since a strings file holds no newline) and Close, which counts down and only then closes.
// A tap outside it, Escape or Back do nothing: the one way out is Close, once its wait is over. It shows over
// everything, in the theme's colours, as the welcome does.
//
// Once read it can stay read: the owner gives it a bit of fude_app.cards_read (saved with the settings) and Close sets
// it, so the owner opens the card only while the bit is clear.
//
// A card with no wait (0 seconds) is a plain card, and Back closes it too. A card can have a second button, at
// Close's left, that does something (the voice card's Open settings) and closes it.
//
// Where it sits: drawing/app. It is a screen (screen.h: FUDE_READCARD_SCREEN); the study apps' shell and welcome
// (study/app) open it.
//
// Its main types and their lifecycle: the owner holds a fude_readcard (zeroed to start) in a screen slot for the
// app's life; there is nothing to free.
//
// Threads: main thread only.
// ===========================================================================

// A card's state. The owner reads `open`; the rest is readcard.c's.
RDE_STRUCT {
    b8            open;
    u32           title;       // FUDE_TEXT_ ids, so they follow a change of language.
    u32           body;
    f32           seconds;     // How long Close waits.
    u8*           read;        // Where Close sets `bit` (fude_app.cards_read); NULL for nowhere.
    u8            bit;
    f64           opened_at;
    fude_scroller scroller;    // The text's.
    // As laid out last frame (screen space), for the pointer.
    rde_vec_2F    text_min, text_max;
    rde_vec_2F    close_min, close_max;
    f32           content_h;   // The text's whole height.
    b8            pressing;    // The pointer is down on Close.
    u32           action;      // The second button's words (a FUDE_TEXT_ id; FUDE_TEXT_COUNT for none).
    void        (*on_action)(void);
    rde_vec_2F    action_min, action_max;
    b8            pressing_action;
} fude_readcard;

// Opens the card with its words (FUDE_TEXT_ ids), Close's wait in seconds, and the bit Close sets in *_read (_read
// NULL: none). It starts scrolled to the top, with no second button.
void fude_readcard_open(fude_readcard* _card, u32 _title, u32 _body, f32 _seconds, u8* _read, u8 _bit);
// After opening, adds the second button: _text (a FUDE_TEXT_ id), and _on, called on its press after the card closes
// (setting its bit, as Close does).
void fude_readcard_set_action(fude_readcard* _card, u32 _text, void (*_on)(void));
// Whether Close can close it now: its wait is over.
b8   fude_readcard_ready(const fude_readcard* _card);
// The seconds still to wait, rounded up (0 when ready).
u32  fude_readcard_wait(const fude_readcard* _card);

// The card's pointer (screen space) and frame, as the screen calls them: a press on Close or the second button counts
// only once the wait is over, the text scrolls, and anywhere else nothing happens. fude_readcard_render draws it.
void fude_readcard_pointer_down(fude_readcard* _card, rde_vec_2F _screen, f64 _now);
void fude_readcard_pointer_moved(fude_readcard* _card, rde_vec_2F _screen, f64 _now);
void fude_readcard_pointer_up(fude_readcard* _card, rde_vec_2F _screen, f64 _now);
void fude_readcard_update(fude_readcard* _card, f32 _dt);
void fude_readcard_render(fude_readcard* _card, rde_window* _window, rde_font* _font, f32 _font_px);

// The card as a screen (screen.h): an overlay over everything, with no row of buttons of its own. Its slot's self is
// the fude_readcard.
extern const struct fude_screen FUDE_READCARD_SCREEN;

#endif
