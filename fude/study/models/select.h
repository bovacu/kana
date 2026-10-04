// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_SELECT
#define FUDE_SELECT

#include "rde.h"
#include "study/models/marks.h"

// ===========================================================================
// A selection of characters, ticked by hand: Browse's and the kana chart's
// Select mode, where a tap ticks a character instead of opening it. One
// selection for both, so kanji from Browse and kana from the chart go together;
// kept in the order ticked, which is the order they are practised in.
//
// It lives while the app does (not saved): leaving Select mode keeps the ticks,
// None clears them.
// ===========================================================================

RDE_STRUCT {
    u8*               _marks;    // one per record: ticked
    u32               _size;     // records
    rde_arr TYPE(u32) order;     // the ticked records, in the order ticked
    b8                active;    // Select mode: a tap ticks
} fude_selection;

void fude_selection_init(fude_selection* _selection, u32 _records);
void fude_selection_destroy(fude_selection* _selection);

b8   fude_selection_has(const fude_selection* _selection, u32 _record);
void fude_selection_toggle(fude_selection* _selection, u32 _record);
// Ticks every one of _records not ticked yet (in their order, after the rest).
void fude_selection_add(fude_selection* _selection, const u32* _records, u32 _count);
void fude_selection_clear(fude_selection* _selection);

u32        fude_selection_count(const fude_selection* _selection);
const u32* fude_selection_records(const fude_selection* _selection);

// A cell in Select mode (inside a 2D block, screen space): its top-left at _tl,
// _size square. Behind what it shows, a ticked one is tinted; over it, a ticked
// one has a filled circle with a check in its corner, the rest an empty circle
// there, to show they can be ticked.
void fude_selection_draw_behind(b8 _ticked, rde_vec_2F _tl, f32 _size);
void fude_selection_draw_tick(b8 _ticked, rde_vec_2F _tl, f32 _size);

// A character's study mark (marks.h) in a corner of its cell or square — the top
// right, or the top left when a tick has that: Studying an amber star, Known a
// green seal (icons.h, on a disc of the colour). Nothing when not marked.
void fude_selection_draw_mark(FUDE_MARK_ _mark, rde_vec_2F _tl, f32 _size, b8 _left);

#endif
