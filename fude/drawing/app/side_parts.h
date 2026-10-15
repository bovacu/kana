// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_SIDE_PARTS
#define FUDE_SIDE_PARTS

#include "drawing/app/side.h"

// ===========================================================================
// Side panel's parts: what the side panel's two files share. side.c is the panel (its navigation, Settings, the cards,
// Your data, Licences, FAQ & Contact) and side_list.c its list of canvases and folders (the rows, built from the notes'
// tree, and a row dragged to another place).
//
// Where it sits: drawing/app, private to the side panel: only side.c and side_list.c include it. The rest of the app
// sees side.h.
// Its main types and their lifecycle: none of its own; the panel's state is side.h's fude_side.
// Threads: main thread only, like all of RDE's UI.
// Files: none.
// Design: the list is a file of its own because the panel grew past a file's size (docs/coding_rules.md §6); the line
// between them is the list's rows, which side.c builds again when the notes change and lays out with the panel.
// ===========================================================================

// The layout's sizes both files use, in UI units.
#define FUDE_SIDE_GAP       6.0f      // Between buttons, and between the list's rows.
#define FUDE_SIDE_NOTE_H    40.0f     // A row of the notes list.

// side.c: opens the note card in _mode for _note (a row's "…" opens it with the note's actions).
void fude_side_card(struct fude_ui* _ui, FUDE_SIDE_CARD_ _mode, u32 _note);

// side_list.c: builds the notes list again from the notes (each row with its drag handle, its name and "…"), when
// the notes changed or another canvas opened.
void fude_side_build_notes(struct fude_ui* _ui);
// side_list.c: hides the line and the box that show where a dragged row would land.
void fude_side_hide_drop(fude_side* _side);
// side_list.c: works out where the dragged row would land with the pointer at the panel's drag_at, and shows it.
void fude_side_drop_target(struct fude_ui* _ui);

#endif
