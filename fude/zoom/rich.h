// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_RICH
#define FUDE_ZOOM_RICH

#include "rde.h"
#include "zoom/zoom.h"

// ===========================================================================
// A TEXT READ AS A NOTE (0.1.86): its marks shown, not written.
//
//   # A heading       ## a smaller one       ### smaller still
//   - a bullet (or * + •), two spaces in front a level deeper; 1. a numbered line (or 1) )
//   [ ] a task, [x] one done, after a bullet too ("- [ ] milk"): a tap ticks it (fude_zoom_rich_tick)
//   | a | b |  a table's row: such lines one after another a table; a row of only - : | and spaces under its
//            first makes that first its header (and its colons each column's side: :-- left, :-: middle, --: right)
//   ---        a rule across (three or more - * or _, and nothing else)
//   in a line: **bold**, *italic* or _italic_, ~~struck~~, `code`, [words](address), and addresses written out
//   (https://…, http://…, www.…) as links.
//
// Laid out for a width as RUNS of words of one style each, placed from the text's top left with Y DOWN, in the
// units its letters' size is given in; with MARKS drawn beside them (a task's box, a table's lines, a rule, a
// link's line, a code span's ground) and HITS, where a tap ticks a task or follows a link. Letters are measured by
// the caller (its fonts).
// ===========================================================================

typedef enum {
    FUDE_ZOOM_RICH_BOLD   = 1,
    FUDE_ZOOM_RICH_ITALIC = 2,
    FUDE_ZOOM_RICH_CODE   = 4,    // on a ground of its own
    FUDE_ZOOM_RICH_LINK   = 8,    // in the links' colour, a line under it
    FUDE_ZOOM_RICH_STRUCK = 16,   // a line through it
    FUDE_ZOOM_RICH_SOFT   = 32,   // softer: a task done
    FUDE_ZOOM_RICH_OWN    = 64,   // its characters FUDE_ZOOM_RICH_BULLET, not the text's: a bullet
    FUDE_ZOOM_RICH_MARK   = 128,  // a mark, kept as it is being written (fude_zoom_rich_lay_raw): faint
} FUDE_ZOOM_RICH_;

#define FUDE_ZOOM_RICH_BULLET "\xE2\x80\xA2"   // •
#define FUDE_ZOOM_RICH_NONE   0xFFFFFFFFu        // (none: scene.h' FUDE_ZOOM_NONE)

typedef enum {
    FUDE_ZOOM_RICH_MARK_LINE = 0,   // from (x0, y0) to (x1, y1): a rule, a table's line, a strike, a link's line
    FUDE_ZOOM_RICH_MARK_BOX,        // a task's box
    FUDE_ZOOM_RICH_MARK_TICKED,     // a task's box, ticked
    FUDE_ZOOM_RICH_MARK_GROUND,     // a box filled softly: a code span's, a table's header's
} FUDE_ZOOM_RICH_MARK_;

typedef struct {
    f32 x, y;        // its baseline's left end
    f32 w, px;       // its width; its letters' size
    u32 from, len;   // its characters in the text (FUDE_ZOOM_RICH_OWN: in FUDE_ZOOM_RICH_BULLET)
    u8  style;       // FUDE_ZOOM_RICH_
} fude_zoom_rich_run;

typedef struct {
    f32 x0, y0, x1, y1;
    u8  kind;        // FUDE_ZOOM_RICH_MARK_
    u8  soft;        // drawn fainter: a table's inner lines
} fude_zoom_rich_mark;

typedef enum {
    FUDE_ZOOM_RICH_HIT_TASK = 0,    // a task to do
    FUDE_ZOOM_RICH_HIT_DONE,        // a task done
    FUDE_ZOOM_RICH_HIT_LINK,
} FUDE_ZOOM_RICH_HIT_;

typedef struct {
    f32 x0, y0, x1, y1;   // where a tap takes it
    u32 at, len;          // a task: its "[" in the text (3 long); a link: its address in the text
    u8  kind;             // FUDE_ZOOM_RICH_HIT_
} fude_zoom_rich_hit;

// A line of the text (up to its '\n'), as laid out: where its first baseline is.
typedef struct {
    u32 from, to;    // its bytes (its '\n' not in it)
    f32 x, y;        // where its words start, its first baseline
    f32 px;          // its letters' size
} fude_zoom_rich_line;

typedef struct {
    rde_arr runs;    // fude_zoom_rich_run
    rde_arr marks;   // fude_zoom_rich_mark
    rde_arr hits;    // fude_zoom_rich_hit
    rde_arr lines;   // fude_zoom_rich_line: each line of the text's
    f32     w, h;    // its widest line's right end; its lines' height
    f32     line;    // its lines apart (letters)
} fude_zoom_rich;

// How wide _len bytes of _text are in the style _style (FUDE_ZOOM_RICH_), letters _px.
typedef f32 (*fude_zoom_rich_measure_fn)(void* _self, const c8* _text, u32 _len, f32 _px, u8 _style);

void fude_zoom_rich_init(fude_zoom_rich* _r);
void fude_zoom_rich_free(fude_zoom_rich* _r);

// _text (_len bytes) laid out _width wide (0: its lines as long as they are, never broken), its letters _px, its
// lines _line × their letters apart, measured by _measure (handed _self). Runs, marks and hits cleared first.
void fude_zoom_rich_lay(fude_zoom_rich* _r, const c8* _text, u32 _len, f32 _width, f32 _px, f32 _line, fude_zoom_rich_measure_fn _measure, void* _self);
// ...its plain lines and headings each to a side of _width (0 left, 1 middle, 2 right: a diagram shape's label); its
// lists, tasks and tables from the left still.
void fude_zoom_rich_lay_aligned(fude_zoom_rich* _r, const c8* _text, u32 _len, f32 _width, f32 _px, f32 _line, u8 _align, fude_zoom_rich_measure_fn _measure, void* _self);

// ...as it is being written on the page (0.1.90): every byte in its runs, in order — its marks too, faint
// (FUDE_ZOOM_RICH_MARK), a line's own (# - 1. [ ] |) as written, no box, bullet or table drawn — its words styled as
// they will be; so a caret has a place at every byte (fude_zoom_rich_caret, fude_zoom_rich_byte_at).
void fude_zoom_rich_lay_raw(fude_zoom_rich* _r, const c8* _text, u32 _len, f32 _width, f32 _px, f32 _line, fude_zoom_rich_measure_fn _measure, void* _self);

// Where a caret before byte _at goes, as laid out (by lay_raw): its baseline's (x, y), its letters' size.
void fude_zoom_rich_caret(const fude_zoom_rich* _r, const c8* _text, u32 _len, u32 _at, fude_zoom_rich_measure_fn _measure, void* _self,
                          f32* _x, f32* _y, f32* _px);
// The byte a caret put at (x, y) goes before (the nearest place between two characters: a line's end, past it).
u32  fude_zoom_rich_byte_at(const fude_zoom_rich* _r, const c8* _text, u32 _len, f32 _x, f32 _y, fude_zoom_rich_measure_fn _measure, void* _self);

// Has it any marks at all? (None: its lines are plain, and drawn as they always were.)
b8   fude_zoom_rich_marked(const c8* _text, u32 _len);

// The hit at (x, y) (as it is laid out), _reach more each way: its index; FUDE_ZOOM_RICH_NONE: none.
u32  fude_zoom_rich_hit_at(const fude_zoom_rich* _r, f32 _x, f32 _y, f32 _reach);

// The task whose "[" is at _at ticked, or unticked if it was (its length the same). False: no task there.
b8   fude_zoom_rich_tick(c8* _text, u32 _len, u32 _at);

// A link's address, to be opened: as written, with https:// in front when it names no way (www.…, example.com).
void fude_zoom_rich_address(const c8* _text, u32 _at, u32 _len, c8* _out, usize _size);

// --- a note's marks put in by a tap (the text card's buttons) ---------------------------------------------

typedef enum {
    FUDE_ZOOM_RICH_DO_BOLD = 0,   // its selection between ** (out again if it was); nothing chosen: **|** to type in
    FUDE_ZOOM_RICH_DO_ITALIC,     // ...between *
    FUDE_ZOOM_RICH_DO_HEADING,    // the lines it touches: # then ## then ### then plain again
    FUDE_ZOOM_RICH_DO_BULLETS,    // ...bullets (plain again if they all were)
    FUDE_ZOOM_RICH_DO_NUMBERS,    // ...1. 2. 3. (plain again if they all were numbered)
    FUDE_ZOOM_RICH_DO_TASKS,      // ...tasks to do (plain again if they all were tasks)
    FUDE_ZOOM_RICH_DO_TABLE,      // a table of two columns and two rows after it, its first cell to type in
    FUDE_ZOOM_RICH_DO_LINK,       // its selection a link's words, its address to type; nothing chosen: https:// to go on
    FUDE_ZOOM_RICH_DOS
} FUDE_ZOOM_RICH_DO_;

// _text (_len bytes) with its selection _from.._to (equal: a caret) and FUDE_ZOOM_RICH_DO_ _do done: into _out (NUL-
// ended; never _text itself), the selection after into *_sel_from, *_sel_to. False: it did not fit in _out (_size
// bytes; _len × 7 + 256 always does).
b8 fude_zoom_rich_format(const c8* _text, u32 _len, u32 _from, u32 _to, u8 _do, c8* _out, u32 _size, u32* _sel_from, u32* _sel_to);

#endif
