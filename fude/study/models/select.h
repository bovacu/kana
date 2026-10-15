// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_SELECT
#define FUDE_SELECT

#include "rde.h"
#include "study/models/marks.h"

// ===========================================================================
// Selection: a set of characters ticked by hand, in Select mode on Browse and on the chart, where a tap ticks a
// character instead of opening it. Both screens share one selection, so kanji from Browse and kana from the chart go
// together. It keeps the characters in the order they were ticked, which is the order they are practised in.
//
// Where it sits: a study model (study/models). The study shell owns the one selection (study/app/shell.c) and hands
// it to Browse and the chart; Select's row of buttons (study/app/verbs.c) acts on it. The drawing helpers below are
// also used by the exams and the viewer to draw a cell's tick or study mark.
// Its main types and their lifecycle: fude_selection, made by its owner with fude_selection_init for a number of
// records (the character data's count) and freed with fude_selection_destroy.
// Threads: main thread only; the drawing functions must be called while a 2D drawing block is open.
// Files: none. The selection lives while the app does and is never saved: leaving Select mode keeps the ticks, and
// None clears them.
// ===========================================================================

RDE_STRUCT {
    u8*               _marks;    // One byte per record: 1 when it is ticked.
    u32               _size;     // How many records _marks covers.
    rde_arr TYPE(u32) order;     // The ticked records, in the order they were ticked.
    b8                active;    // Select mode is on: a tap ticks instead of opening.
} fude_selection;

// Makes an empty selection for _records records (0 makes one that never ticks anything); destroy frees it, and leaves
// it zeroed.
void fude_selection_init(fude_selection* _selection, u32 _records);
void fude_selection_destroy(fude_selection* _selection);

// Whether record _record is ticked (false for a record past the end), and ticking or unticking it. Unticking takes it
// out of the order, the others keeping theirs.
b8   fude_selection_has(const fude_selection* _selection, u32 _record);
void fude_selection_toggle(fude_selection* _selection, u32 _record);
// Ticks every one of _records not ticked yet, in their order, after the ones already ticked.
void fude_selection_add(fude_selection* _selection, const u32* _records, u32 _count);
// Unticks everything.
void fude_selection_clear(fude_selection* _selection);

// How many records are ticked, and the records in the order ticked. The pointer belongs to the selection and stays
// valid until it next changes.
u32        fude_selection_count(const fude_selection* _selection);
const u32* fude_selection_records(const fude_selection* _selection);

// A cell in Select mode, in screen space: its top-left corner at _tl and _size wide and tall. Call them inside a 2D
// drawing block. fude_selection_draw_behind goes before the cell's contents and tints a ticked cell;
// fude_selection_draw_tick goes after them and puts a filled circle with a check in the corner of a ticked cell, or an
// empty circle on the others, to show that they can be ticked.
void fude_selection_draw_behind(b8 _ticked, rde_vec_2F _tl, f32 _size);
void fude_selection_draw_tick(b8 _ticked, rde_vec_2F _tl, f32 _size);

// A character's study mark (marks.h) in a corner of its cell or square: the top right, or the top left (_left) when a
// tick is in the top right. Studying is an amber star and Known a green seal, each a white icon (icons.h) on a disc of
// that colour so it reads on any cell. Nothing is drawn when the character is not marked.
void fude_selection_draw_mark(FUDE_MARK_ _mark, rde_vec_2F _tl, f32 _size, b8 _left);

#endif
