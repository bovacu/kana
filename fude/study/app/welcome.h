// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_STUDY_WELCOME_H
#define FUDE_STUDY_WELCOME_H

#include "rde.h"
#include "drawing/widgets/readcard.h"

// ===========================================================================
// The welcome: a few pages over everything the first time a study app opens (no
// settings saved yet) — what the app is, the pen and the fingers, reading text
// with the camera, the lasso and pasting text as handwriting, translating, what to
// study, and the progress kept — because much of it is gestures and features
// nobody would guess are there. Skip, Next, and on the last page Start writing. Settings can show it
// again. Its words are the app's own (WELCOME_1_TITLE, WELCOME_1... in its
// strings tool).
//
// Closed — skipped or finished — it opens a must-read card once (readcard.h):
// why the app works offline, until its Close is pressed (fude_app.cards_read's
// FUDE_WELCOME_CARD_OFFLINE bit, saved). Then, once, when the device has no voice
// for the language (speech.h: MISSING), the voice card: how to add one (Android:
// with Open settings). The side panel's Set up the voice opens it again.
// ===========================================================================

#define FUDE_WELCOME_PAGES 7u
#define FUDE_WELCOME_CARD_OFFLINE 0x01u   // fude_app.cards_read: why the app works offline, read
#define FUDE_WELCOME_CARD_VOICE   0x02u   // and the voice card, shown once
#define FUDE_WELCOME_CARD_SECONDS 5.0f    // its Close's wait

RDE_STRUCT {
    b8         open;
    u32        page;
    // As laid out last frame (screen space), for the pointer.
    rde_vec_2F next_min, next_max;
    rde_vec_2F skip_min, skip_max;
    i32        pressed;   // 0 none, 1 Next, 2 Skip
    // Set by the shell: the card it opens once closed, and the app's cards read
    // (fude_app.cards_read). NULL: none.
    fude_readcard* card;
    u8*            cards_read;
} fude_welcome;

void fude_welcome_open(fude_welcome* _welcome);
// Why the app works offline (the card), now — whether read or not (a look flag's).
void fude_welcome_offline(fude_welcome* _welcome);
// The voice card, now: how to add a voice for the language (the side panel's
// button, a look flag's).
void fude_welcome_voice(fude_welcome* _welcome);
// ...as a given platform's (_android: its steps and Open settings; a look's, anywhere).
void fude_welcome_voice_as(fude_welcome* _welcome, b8 _android);
// Once a frame (fude_study_update): the voice card after the offline one, the first time.
void fude_welcome_update(fude_welcome* _welcome);
// The pointer, in screen space: a press and its release on the same button.
void fude_welcome_pointer_down(fude_welcome* _welcome, rde_vec_2F _screen);
void fude_welcome_pointer_up(fude_welcome* _welcome, rde_vec_2F _screen);
// Over everything else (the page's own colour behind it).
void fude_welcome_render(fude_welcome* _welcome, rde_window* _window, rde_font* _font, f32 _font_px);

// The screen (screen.h): its rows of buttons, and what they do.
extern const struct fude_screen FUDE_WELCOME_SCREEN;

#endif
