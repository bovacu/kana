// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_CRASHCARD
#define FUDE_CRASHCARD

#include "rde.h"
#include "drawing/widgets/kit.h"

// ===========================================================================
// The crash card (0.1.93, crash.h), every app's: at a launch after the app ended
// badly, over everything, "{APP} closed unexpectedly", what a report holds (never
// what was written or drawn), and Send report (an email to write to, the report
// in it, read before it goes) or Not now (forgotten). Once: either way the report
// is gone after.
// ===========================================================================

struct fude_ui;

typedef struct {
    fude_kit_modal modal;
    rde_ui_label*  title;
    rde_ui_label*  text;
    rde_ui_button* send;
    rde_ui_button* later;
    b8             asked;      // decided this launch whether to show it
    b8             open;
    rde_vec_2F     _laid_out;  // (the screen it was placed for)
} fude_crashcard;

// Built under _root (hidden); let go of with the canvas.
void fude_crashcard_create(struct fude_ui* _ui, rde_ui_node* _root);
void fude_crashcard_forget(struct fude_ui* _ui);
// Once a frame: up when the last session left a report (and no look is running), placed.
void fude_crashcard_update(struct fude_ui* _ui);
void fude_crashcard_restyle(struct fude_ui* _ui);
// Is it up (the whole screen its)?
b8   fude_crashcard_shown(const struct fude_ui* _ui);

#endif
