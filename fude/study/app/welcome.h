// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_STUDY_WELCOME_H
#define FUDE_STUDY_WELCOME_H

#include "rde.h"
#include "drawing/app/readcard.h"

// ===========================================================================
// Welcome: a few pages shown over everything the first time a study app opens (when no settings are saved yet),
// because much of the app is gestures and features nobody would guess are there. The pages say what the app is,
// how the pen and the fingers work, how to read text with the camera, the lasso and pasting text as handwriting,
// translating, what to study, and the progress the app keeps. They have Skip and Next, and Start writing on the last
// page. The side panel's Tutorial shows them again.
//
// Where it sits: the study layer (study/app). The study shell (shell.h) owns the one fude_welcome as a static and
// stacks its screen over every other but the must-read card; the study (study.h) opens it on the first launch and
// from the Tutorial button, and updates it each frame.
// Its main types and their lifecycle: fude_welcome is plain data with nothing to free. The shell sets card and
// cards_read before anything else uses it.
// Threads: everything runs on the app's main thread.
// Files: none of its own. The cards read are bits of fude_app.cards_read, which the session saves with the settings.
// Its words are the app's own strings (WELCOME_1_TITLE, WELCOME_1 and so on, in its strings tool).
// Design: once closed, skipped or finished, the welcome opens a must-read card (readcard.h) on why the app works
// offline, the first time only: it shows until its Close is pressed, which sets the FUDE_WELCOME_CARD_OFFLINE bit of
// fude_app.cards_read. After that, once, when the device has no voice for the language (speech.h: MISSING), it opens
// the voice card, which says how to add one (on Android, with an Open settings button). The side panel's Set up the
// voice button opens the voice card again.
// ===========================================================================

#define FUDE_WELCOME_PAGES 7u
#define FUDE_WELCOME_CARD_OFFLINE 0x01u   // fude_app.cards_read: why the app works offline, read
#define FUDE_WELCOME_CARD_VOICE   0x02u   // and the voice card, shown once
#define FUDE_WELCOME_CARD_SECONDS 5.0f    // how long the offline card's Close waits before it can be pressed

RDE_STRUCT {
    b8         open;
    u32        page;
    // Where the buttons were laid out last frame (screen space), for the pointer.
    rde_vec_2F next_min, next_max;
    rde_vec_2F skip_min, skip_max;
    i32        pressed;   // 0 none, 1 Next, 2 Skip
    // Set by the shell: the card it opens once closed, and the app's cards read (fude_app.cards_read). NULL when
    // there is none.
    fude_readcard* card;
    u8*            cards_read;
} fude_welcome;

// Opens the welcome at its first page.
void fude_welcome_open(fude_welcome* _welcome);
// Opens the card on why the app works offline now, whether it was read before or not (for a developer's look flag).
void fude_welcome_offline(fude_welcome* _welcome);
// Opens the voice card now, which says how to add a voice for the language (the side panel's button, or a look
// flag), with the steps for the platform it runs on.
void fude_welcome_voice(fude_welcome* _welcome);
// The same, with a given platform's steps: _android gives Android's steps and Open settings, and a developer's look
// can ask for them on any platform.
void fude_welcome_voice_as(fude_welcome* _welcome, b8 _android);
// Once a frame (from fude_study_update): opens the voice card, the first time only, after the offline card has been
// read, when the device has no voice for the language. It asks for the voice's state every frame, so that Android's
// engine starts, and asks again when the app comes back.
void fude_welcome_update(fude_welcome* _welcome);
// The pointer, in screen space: a press and its release on the same button act (Next, Start writing or Skip).
void fude_welcome_pointer_down(fude_welcome* _welcome, rde_vec_2F _screen);
void fude_welcome_pointer_up(fude_welcome* _welcome, rde_vec_2F _screen);
// Draws the welcome over everything else, with the page's own colour behind it. Nothing is drawn while it is closed.
void fude_welcome_render(fude_welcome* _welcome, rde_window* _window, rde_font* _font, f32 _font_px);

// The screen (screen.h): it lies over everything, the page and the screens drawn under it, and takes the pointer for
// its own buttons; it has no row of its own.
extern const struct fude_screen FUDE_WELCOME_SCREEN;

#endif
