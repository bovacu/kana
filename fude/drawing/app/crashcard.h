// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_CRASHCARD
#define FUDE_CRASHCARD

#include "rde.h"
#include "drawing/widgets/kit.h"

// ===========================================================================
// Crash card: every app's card (since 0.1.93) that asks about a crash report (crash.h). At a launch after the app
// ended badly, it shows over everything: "{APP} closed unexpectedly", what a report holds (never what was written or
// drawn), and two buttons. Send report opens an email to write, with the report in it to be read before it goes; Not
// now (or a tap outside the card) forgets the report. It is asked once: either way the report is gone afterwards.
//
// Where it sits: drawing/app. The UI (ui.h) holds the fude_crashcard (fude_ui.crash), builds it over everything and
// drives it.
//
// Its main types and their lifecycle: fude_crashcard_create builds its widgets under the UI's root, hidden; they go
// with the UI canvas, and fude_crashcard_forget drops the pointers to them first. Whether it was asked and is open
// survive a rebuild of the UI.
//
// Threads: main thread only, like all of RDE's UI.
//
// Files: reads the last session's crash report, and deletes it once answered (crash.h).
// ===========================================================================

struct fude_ui;

// The crash card. The fields are crashcard.c's.
typedef struct {
    fude_kit_modal modal;
    rde_ui_label*  title;
    rde_ui_label*  text;
    rde_ui_button* send;
    rde_ui_button* later;
    b8             asked;      // Whether to show it has been decided this launch.
    b8             open;
    rde_vec_2F     _laid_out;  // The screen size it was placed for.
} fude_crashcard;

// fude_crashcard_create builds the card under _root, hidden (shown again if it was open before a rebuild).
// fude_crashcard_forget drops the pointers to its widgets before the UI canvas goes, keeping whether it was asked and
// is open.
void fude_crashcard_create(struct fude_ui* _ui, rde_ui_node* _root);
void fude_crashcard_forget(struct fude_ui* _ui);
// Called once a frame. The first time each launch, it shows the card when the last session left a report and no look
// is running (unless the look asks for the card: --crash-card); while open, it places the card for the screen.
void fude_crashcard_update(struct fude_ui* _ui);
// Restyles the card from the theme.
void fude_crashcard_restyle(struct fude_ui* _ui);
// Whether the card is up; while it is, the whole screen is its.
b8   fude_crashcard_shown(const struct fude_ui* _ui);

#endif
