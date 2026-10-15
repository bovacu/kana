// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_HANDWRITING
#define FUDE_HANDWRITING

#include "rde.h"

// ===========================================================================
// Handwriting: Settings' section for Google ML Kit's handwriting reader (drawing/ink/mlkit.h). It has a header, the
// switch (ML Kit on or off), and the model's state, with a Download or Retry button beside it while there is no model.
// Every app that reads handwriting with ML Kit lists it: the study apps (Check, Copy as text, Save word, and their
// text and translation services, which obey the same switch) and the canvas apps (the lasso's To text, Find's
// handwriting). Each states what being off means in its own words: the core's strings say handwriting is not read as
// text (the canvas apps), the study's say its own reader takes over, and a study app's language names its model (o()
// in their strings.py).
//
// Where it sits: drawing/app, beside Settings (side.h), since ML Kit is the drawing core's (drawing/ink). An app
// puts FUDE_HANDWRITING_SECTION in its extension's sections (extension.h); the switch is saved with the core's
// settings (settings.h: mlkit; session.c gathers and applies it), so a section listed or not, it is kept.
//
// Its main types and their lifecycle: the section's widgets are a fude_handwriting in the UI (fude_ui.handwriting),
// built with Settings' card and built again with it when the language changes.
//
// Threads: main thread only, like all of RDE's UI.
//
// Files: none; the switch goes in the settings file.
// ===========================================================================

struct fude_ui;

// The section's widgets. The fields are handwriting.c's.
typedef struct {
    rde_ui_label*  header;
    rde_ui_label*  label;      // "Read with Google ML Kit", beside the switch
    rde_ui_button* toggle;
    rde_ui_label*  status;     // the model's state
    rde_ui_button* download;   // Download, or Retry, while there is no model
    i32            _shown;     // what the status shows: the state, plus 100 when off; -1 before it shows any
} fude_handwriting;

// The section's hooks, for fude_extension_section (extension.h): build, layout, refresh and restyle.
void fude_handwriting_build(struct fude_ui* _ui, rde_ui_node* _card);
f32  fude_handwriting_layout(struct fude_ui* _ui, f32 _y, f32 _margin, f32 _card_w);
void fude_handwriting_refresh(struct fude_ui* _ui, b8 _force);
void fude_handwriting_restyle(struct fude_ui* _ui);

// The section, as an element of an app's sections (an initializer: C's constant initializers cannot copy a const
// object, and an app's sections are a const table).
#define FUDE_HANDWRITING_SECTION { fude_handwriting_build, fude_handwriting_layout, fude_handwriting_refresh, fude_handwriting_restyle }

#endif
