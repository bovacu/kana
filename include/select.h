#ifndef KANA_SELECT
#define KANA_SELECT

#include "rde.h"
#include "marks.h"

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
} kana_selection;

void kana_selection_init(kana_selection* _selection, u32 _records);
void kana_selection_destroy(kana_selection* _selection);

b8   kana_selection_has(const kana_selection* _selection, u32 _record);
void kana_selection_toggle(kana_selection* _selection, u32 _record);
// Ticks every one of _records not ticked yet (in their order, after the rest).
void kana_selection_add(kana_selection* _selection, const u32* _records, u32 _count);
void kana_selection_clear(kana_selection* _selection);

u32        kana_selection_count(const kana_selection* _selection);
const u32* kana_selection_records(const kana_selection* _selection);

// A cell in Select mode (inside a 2D block, screen space): its top-left at _tl,
// _size square. Behind what it shows, a ticked one is tinted; over it, a ticked
// one has a filled circle with a check in its corner, the rest an empty circle
// there, to show they can be ticked.
void kana_selection_draw_behind(b8 _ticked, rde_vec_2F _tl, f32 _size);
void kana_selection_draw_tick(b8 _ticked, rde_vec_2F _tl, f32 _size);

// A character's study mark (marks.h) in a corner of its cell or square — the top
// right, or the top left when a tick has that: Studying an amber star, Known a
// green seal (icons.h, on a disc of the colour). Nothing when not marked.
void kana_selection_draw_mark(KANA_MARK_ _mark, rde_vec_2F _tl, f32 _size, b8 _left);

#endif
