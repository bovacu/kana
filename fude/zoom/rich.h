// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_RICH
#define FUDE_ZOOM_RICH

#include "rde.h"
#include "zoom/zoom.h"

// ===========================================================================
// Rich: a text read as a note (0.1.86), its Markdown-like marks shown as what they mean rather than as written.
//
// Where it sits: zoom, the canvas apps' layer. The renderer (render_text.c) draws texts, sticky notes and shapes'
// labels with it, the exports (export.c) and the PDF canvas (pdfview.c) write them, and the page (page_text.c) uses its
// hits for taps, its caret functions while a text is being written, and fude_zoom_rich_format for the format bar's
// buttons.
// Its main types and their lifecycle: fude_zoom_rich is the caller's struct. fude_zoom_rich_init creates its arrays on
// the standard heap, each layout clears and refills them, and fude_zoom_rich_free frees them.
// Threads: called on the main thread. It keeps no global state; the measuring callback is called synchronously, on
//   the caller's thread, during a layout.
// Design:
//   The marks it reads, as people already write them:
//     # A heading       ## a smaller one       ### smaller still
//     - a bullet (or * + •); two spaces in front make it a level deeper. 1. a numbered line (or 1) ).
//     [ ] a task and [x] one done, also after a bullet ("- [ ] milk"); a tap ticks it (fude_zoom_rich_tick).
//     | a | b |  a table's row: such lines one after another make a table. A row of only - : | and spaces under the
//              first row makes that first row the header, and its colons set each column's side: :-- left, :-:
//              middle, --: right.
//     ---        a rule across (three or more of - * or _, and nothing else on the line).
//     Within a line: **bold**, *italic* or _italic_, ~~struck~~, `code`, [words](address), and addresses written out
//     (https://…, http://…, www.…) as links.
//   A text is laid out for a width as RUNS of words of one style each, placed from the text's top left with y DOWN,
//   in the units its letters' size is given in. MARKS are drawn beside them (a task's box, a table's lines, a rule, a
//   link's line, a code span's ground), and HITS say where a tap ticks a task or follows a link.
//   Letters are measured by the caller (fude_zoom_rich_measure_fn), so this module needs no fonts and the tests can
//   use a made-up one.
//   See docs/infinite_canvas_design.md §67 and §72.
// ===========================================================================

// A run's style, as bit flags that combine.
typedef enum {
    FUDE_ZOOM_RICH_BOLD   = 1,
    FUDE_ZOOM_RICH_ITALIC = 2,
    FUDE_ZOOM_RICH_CODE   = 4,    // drawn on a ground of its own
    FUDE_ZOOM_RICH_LINK   = 8,    // drawn in the links' colour, with a line under it
    FUDE_ZOOM_RICH_STRUCK = 16,   // drawn with a line through it
    FUDE_ZOOM_RICH_SOFT   = 32,   // drawn softer, as a task that is done
    FUDE_ZOOM_RICH_OWN    = 64,   // a bullet: its characters are FUDE_ZOOM_RICH_BULLET, not the text's
    FUDE_ZOOM_RICH_MARK   = 128,  // a mark kept while being written (fude_zoom_rich_lay_raw), drawn faint
} FUDE_ZOOM_RICH_;

#define FUDE_ZOOM_RICH_BULLET "\xE2\x80\xA2"   // • in UTF-8
#define FUDE_ZOOM_RICH_NONE   0xFFFFFFFFu        // no index; the same value as scene.h's FUDE_ZOOM_NONE

// What a mark draws.
typedef enum {
    FUDE_ZOOM_RICH_MARK_LINE = 0,   // a line from (x0, y0) to (x1, y1): a rule, a table's line, a strike, a link's line
    FUDE_ZOOM_RICH_MARK_BOX,        // a task's box
    FUDE_ZOOM_RICH_MARK_TICKED,     // a task's box, ticked
    FUDE_ZOOM_RICH_MARK_GROUND,     // a box filled softly, behind a code span or a table's header
} FUDE_ZOOM_RICH_MARK_;

// Words of one style, placed on one line.
typedef struct {
    f32 x, y;        // its baseline's left end
    f32 w, px;       // its width, and its letters' size
    u32 from, len;   // its bytes in the text (with FUDE_ZOOM_RICH_OWN, in FUDE_ZOOM_RICH_BULLET instead)
    u8  style;       // FUDE_ZOOM_RICH_ flags
} fude_zoom_rich_run;

// Something drawn beside the words: a line or a box, from (x0, y0) to (x1, y1).
typedef struct {
    f32 x0, y0, x1, y1;
    u8  kind;        // FUDE_ZOOM_RICH_MARK_
    u8  soft;        // drawn fainter, as a table's inner lines are
} fude_zoom_rich_mark;

// What a tap on a hit does.
typedef enum {
    FUDE_ZOOM_RICH_HIT_TASK = 0,    // ticks a task still to do
    FUDE_ZOOM_RICH_HIT_DONE,        // unticks a task that is done
    // Follows a link.
    FUDE_ZOOM_RICH_HIT_LINK,
} FUDE_ZOOM_RICH_HIT_;

// A place a tap acts on.
typedef struct {
    f32 x0, y0, x1, y1;   // the box a tap must land in
    u32 at, len;          // for a task, its "[" in the text (3 bytes long); for a link, its address in the text
    u8  kind;             // FUDE_ZOOM_RICH_HIT_
} fude_zoom_rich_hit;

// One line of the text (up to its '\n') as laid out, kept so a caret can be placed in it.
typedef struct {
    u32 from, to;    // its bytes, not counting its '\n'
    f32 x, y;        // where its words start, and its first baseline
    f32 px;          // its letters' size
} fude_zoom_rich_line;

// A text laid out. Its struct is named, so render.h can return one without including this header.
typedef struct fude_zoom_rich {
    rde_arr runs;    // fude_zoom_rich_run
    rde_arr marks;   // fude_zoom_rich_mark
    rde_arr hits;    // fude_zoom_rich_hit
    rde_arr lines;   // fude_zoom_rich_line, one for each line of the text
    f32     w, h;    // the right end of its widest line, and the height of all its lines
    f32     line;    // the distance between lines, in letter sizes
} fude_zoom_rich;

// The caller's measure: returns how wide _len bytes of _text are in the style _style (FUDE_ZOOM_RICH_ flags) with
// letters _px in size. _self is what the caller handed the layout.
typedef f32 (*fude_zoom_rich_measure_fn)(any _self, const c8* _text, u32 _len, f32 _px, u8 _style);

// fude_zoom_rich_init sets up an empty layout (its arrays on the standard heap, lines 1.3 letters apart);
// fude_zoom_rich_free frees its arrays.
void fude_zoom_rich_init(fude_zoom_rich* _r);
void fude_zoom_rich_free(fude_zoom_rich* _r);

// Lays _text (_len bytes) out as a note, _width wide (0: each line as long as it is, never broken), its letters _px
// in size and its lines _line times their letters' size apart, measured by _measure (which is handed _self). The
// runs, marks, hits and lines are cleared first, and stay empty when _text is NULL or empty, _measure is NULL or _px
// is not above 0. A word wider than a line is cut between its characters, never left out.
void fude_zoom_rich_lay(fude_zoom_rich* _r, const c8* _text, u32 _len, f32 _width, f32 _px, f32 _line, fude_zoom_rich_measure_fn _measure, any _self);
// As fude_zoom_rich_lay, but its plain lines and headings are each put to a side of _width (_align 0 left, 1 middle,
// 2 right), as a diagram shape's label is. Lists, tasks and tables still start from the left.
void fude_zoom_rich_lay_aligned(fude_zoom_rich* _r, const c8* _text, u32 _len, f32 _width, f32 _px, f32 _line, u8 _align, fude_zoom_rich_measure_fn _measure, any _self);

// Lays the text out as it is being written on the page (0.1.90): every byte is in a run, in order, its marks too.
// A line's own marks (# - 1. [ ] |) are kept as written, with no box, bullet or table drawn; a span's marks are kept
// faint (FUDE_ZOOM_RICH_MARK); the words are styled as they will be. That way a caret has a place at every byte
// (fude_zoom_rich_caret, fude_zoom_rich_byte_at). A NULL _text counts as empty.
void fude_zoom_rich_lay_raw(fude_zoom_rich* _r, const c8* _text, u32 _len, f32 _width, f32 _px, f32 _line, fude_zoom_rich_measure_fn _measure, any _self);

// Writes where a caret before byte _at goes in a layout made by fude_zoom_rich_lay_raw: its baseline's point (*_x,
// *_y) and its letters' size (*_px). All three are 0 when the layout has no lines.
void fude_zoom_rich_caret(const fude_zoom_rich* _r, const c8* _text, u32 _len, u32 _at, fude_zoom_rich_measure_fn _measure, any _self,
                          f32* _x, f32* _y, f32* _px);
// Returns the byte a caret put at (_x, _y) goes before: the nearest place between two characters, or the line's end
// when the point is past it. Returns 0 when the layout has no lines. For a layout made by fude_zoom_rich_lay_raw.
u32  fude_zoom_rich_byte_at(const fude_zoom_rich* _r, const c8* _text, u32 _len, f32 _x, f32 _y, fude_zoom_rich_measure_fn _measure, any _self);

// Returns whether _text has any marks at all. A text with none is plain, and is drawn as texts always were.
b8   fude_zoom_rich_marked(const c8* _text, u32 _len);

// Returns the index of the first hit whose box, grown by _reach each way, holds (_x, _y), or FUDE_ZOOM_RICH_NONE.
u32  fude_zoom_rich_hit_at(const fude_zoom_rich* _r, f32 _x, f32 _y, f32 _reach);

// Ticks the task whose "[" is at byte _at, or unticks it if it was ticked, in place: the text keeps its length.
// Returns false when there is no task there.
b8   fude_zoom_rich_tick(c8* _text, u32 _len, u32 _at);

// Writes the link address at _text[_at] (_len bytes) into _out (_size bytes, cut short if needed), ready to open: as
// written, with https:// in front when it names no scheme (www.…, example.com).
void fude_zoom_rich_address(const c8* _text, u32 _at, u32 _len, c8* _out, usize _size);

// --- a note's marks put in by a tap (the text card's buttons) ---------------------------------------------

// What a format button does.
typedef enum {
    FUDE_ZOOM_RICH_DO_BOLD = 0,   // puts the selection between ** (or takes them off); with none, **|** to type in
    FUDE_ZOOM_RICH_DO_ITALIC,     // the same with *
    FUDE_ZOOM_RICH_DO_HEADING,    // turns the lines it touches through # then ## then ### then plain again
    FUDE_ZOOM_RICH_DO_BULLETS,    // makes those lines bullets (plain again if they all were)
    FUDE_ZOOM_RICH_DO_NUMBERS,    // numbers them 1. 2. 3. (plain again if they all were numbered)
    FUDE_ZOOM_RICH_DO_TASKS,      // makes them tasks to do (plain again if they all were tasks)
    FUDE_ZOOM_RICH_DO_TABLE,      // adds a table of two columns and two rows after the line, its first cell to type in
    FUDE_ZOOM_RICH_DO_LINK,       // makes the selection a link's words with its address to type; with none, https://
    // How many there are.
    FUDE_ZOOM_RICH_DOS
} FUDE_ZOOM_RICH_DO_;

// Does the format button _do (FUDE_ZOOM_RICH_DO_) to _text (_len bytes) with the selection _from to _to (equal for a
// caret), writing the new text, ended by a 0, into _out (_size bytes; it must not be _text itself). The selection
// afterwards goes to *_sel_from and *_sel_to, unless they are NULL. Returns false, with _out empty, when the result
// did not fit; _len × 7 + 256 bytes always fits.
b8 fude_zoom_rich_format(const c8* _text, u32 _len, u32 _from, u32 _to, u8 _do, c8* _out, u32 _size, u32* _sel_from, u32* _sel_to);

#endif
