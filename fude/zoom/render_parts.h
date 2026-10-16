// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_RENDER_PARTS
#define FUDE_ZOOM_RENDER_PARTS

#include "zoom/render.h"
#include "zoom/formula.h"
#include "zoom/fill.h"
#include "zoom/shape.h"
#include "zoom/sheet.h"
#include "zoom/symbol.h"
#include "zoom/circuit.h"
#include "zoom/plot.h"
#include "zoom/rich.h"
#include "drawing/base/theme.h"
#include "drawing/base/draw.h"

// ===========================================================================
// Render parts: the renderer's private interface. The renderer (render.h) is written as render.c and the render_*.c
// modules; this is what they share with each other: the functions one module calls in another, and the constants and
// types more than one needs. Included by render.c and render_*.c only, never anywhere else: everything outside the
// renderer uses render.h.
//
// Where it sits: fude/zoom, under render.h (which it includes, with every header the renderer uses). The modules
// include this header and nothing else.
// Its main types and their lifecycle: the renderer (fude_zoom_renderer, render.h), made with fude_zoom_render_init and
// freed with fude_zoom_render_destroy (render.c), and its caches, kept in it and emptied by fude_zoom_render_trim.
// Threads: the main thread only, inside the app's 2D drawing block, as the renderer itself.
// Files: none.
// Design: render.c was one file of some 2,800 lines; it was split by what each part draws, its code moved unchanged,
// and then its two functions of over 150 lines were broken into steps by extraction alone (fude_zoom_render_symbol into
// fude_zoom_render_symbol_*, fude_zoom_render_shape into fude_zoom_render_shape_*). A function one module calls in
// another is declared here (not RDE_INTERNAL); what one module alone uses stays RDE_INTERNAL in it. Every internal name
// keeps render.c's prefixes: fude_zoom_render_, fude_zoom_draw_ and fude_zoom_clip_.
//
// Which module holds what:
//   render.c         the coordinator: the renderer and its caches (the kept geometry, decoded strokes), pictures,
//                    frames, the backdrop, the draw (fude_zoom_render), one object alone, the glow, the frames tools
//                    may edit
//   render_stroke.c  strokes and fills: polygons clipped to the screen, a stroke (its mesh kept), a fill and its soft
//                    edge
//   render_shape.c   shapes: measures, arrows, boards, sheets, guides, a shape's and a cut board's geometry kept, and a
//                    shape's outline drawn (fude_zoom_render_shape)
//   render_text.c    texts: formulas, notes (their layouts kept), a text, text in a box
//   render_symbol.c  symbols: their geometry kept, and a symbol drawn (fude_zoom_render_symbol)
// ===========================================================================

// --- the constants and types the modules share (as render.c had them) ------------------------------------------------

// How far off the screen the GPU is given nothing: a stroke, a shape's line or a cut board with a point (or a stroke
// its translation and size) or its half-width reaching it is cut to the screen on the CPU. A fill and a picture switch
// at a tenth of it, by their size on the screen: a fill that big has its rings cut to the screen, a picture half that
// big is drawn only in its part on the screen.
#define FUDE_ZOOM_RENDER_FAR     1.0e5    // screen units

// The bounds of the kept geometry (render.c) and of the texts' layouts kept (render_text.c). Past one, the one used
// longest ago makes room; what was used in the current draw is never let go (what does not fit is made each time).
#define FUDE_ZOOM_RENDER_GEOS       2048u        // geometry kept, at most
#define FUDE_ZOOM_RENDER_GEO_POINTS (1u << 21)   // points and triangle corners the kept geometry may hold, at most
#define FUDE_ZOOM_RENDER_RICH       256u         // texts' layouts kept, at most

// A text's layout kept (rich.h): laid out if it has marks. What it is the layout of is its slot's key in the renderer's
// rich_lru (render_text.c's fude_zoom_render_rich_aligned).
typedef struct {
    fude_zoom_rich rich;     // its layout (laid out only when marked; its arrays made the first time the slot is used)
    b8             marked;   // it has marks: drawn from the layout, else as plain lines
    u32            used;     // the draw it was last used in (never let go during that draw)
    // A plain one's lines as last drawn on the canvas (render_text.c's fude_zoom_render_text): draw.c's layout at its
    // letters' size and width on the screen (plain_px, plain_width; plain_px 0: none), each line's first byte and the
    // byte after it in plain_spans (all the firsts, then all the ends). Its words and font are the slot's key, so it is
    // good while the size and width on the screen are the same; a slot laid out into again forgets it.
    f32               plain_px, plain_width;
    u32               plain_lines;
    rde_arr TYPE(u32) plain_spans;   // made the first time the slot holds a plain one
} fude_zoom_render_rich_kept;

// What kept geometry (render.h's fude_zoom_geo) is of: its key in the renderer's geo_lru, compared whole. A symbol's:
// its kind, detail and half sizes, and its custom part (the made definition's address) or 0. A shape's: its type, its
// payload (blob and serial: what its numbers are), its detail and how many of its numbers at most it is made from. A
// cut board's: its type and payload. Set to 0 before it is filled in, so the fields a kind leaves out are always alike.
typedef struct {
    u32 what;            // FUDE_ZOOM_RENDER_GEO_
    u32 kind;            // a symbol's kind; a shape's or a board's type (shape.h's FUDE_ZOOM_SHAPE_)
    u32 blob, serial;    // a shape's or a board's payload (scene.h's fude_zoom_blob)
    u32 segments;        // the detail its curves are made at
    u32 numbers;         // a shape's: how many of its numbers at most it is made from
    f64 hw, hh;          // a symbol's half sizes
    u64 custom;          // a symbol's custom part, or 0
} fude_zoom_render_geo_key;

// What kept geometry is of (fude_zoom_render_geo_key's what).
typedef enum { FUDE_ZOOM_RENDER_GEO_SYMBOL = 1, FUDE_ZOOM_RENDER_GEO_SHAPE, FUDE_ZOOM_RENDER_GEO_BOARD } FUDE_ZOOM_RENDER_GEO_;

// --- render.c: the caches, decoded strokes ---------------------------------------------------------------------------

// The texts' layouts let go.
void fude_zoom_render_rich_forget(fude_zoom_renderer* _r);
// A graph's lines for its kind _id, text _text and half sizes: as kept, or made now (kept, unless every slot was drawn
// in this draw: then into the spare), good until the next is made.
const fude_zoom_plot_made* fude_zoom_render_plot_made(fude_zoom_renderer* _r, const c8* _id, const c8* _text, f64 _hw, f64 _hh);
// Makes _arr an empty array of _size-byte elements on the std allocator: cleared when it is made already, else made.
void fude_zoom_render_fresh(rde_arr* _arr, usize _size);
// The geometry kept let go, the spare's too (made again as it is drawn).
void fude_zoom_render_geo_forget(fude_zoom_renderer* _r);
// Returns the geometry kept for _key, marked used in this draw; NULL when none is.
fude_zoom_geo* fude_zoom_render_geo_find(fude_zoom_renderer* _r, const fude_zoom_render_geo_key* _key);
// Returns the spare emptied, to make new geometry into (each maker makes the arrays it fills: fude_zoom_render_fresh).
fude_zoom_geo* fude_zoom_render_geo_begin(fude_zoom_renderer* _r);
// Keeps what was just made into the spare as _key's, and returns where it is now: in a free slot, or in the slot of
// the one used longest ago (let go). When that one was used in this draw too (every slot is), or the budget cannot be
// kept, it is not kept: it stays in the spare, good until the next geometry is made, and is made again next time.
fude_zoom_geo* fude_zoom_render_geo_keep(fude_zoom_renderer* _r, const fude_zoom_render_geo_key* _key);
// Returns how many triangles _g's inside is, its points as one polygon in rings (a board's; a shape's are all one
// ring), made into its tris the first time it is asked for and counted against the budget then.
u32 fude_zoom_render_geo_tris(fude_zoom_renderer* _r, fude_zoom_geo* _g);
// Returns object _object's points decoded for drawing (a fill's rings too), from the cache or decoded now and kept;
// NULL when its bytes cannot be read or it has no points (nothing to draw, and no room of 0 to ask rde_arr_add_n
// for). The pointer is good until the next call that decodes, which may move the cache.
const fude_zoom_decoded* fude_zoom_render_decoded(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _object);

// --- render_stroke.c: strokes and fills ------------------------------------------------------------------------------

// A stroke many screens across: each segment's band and each join, cut to the box.
void fude_zoom_draw_huge_stroke(const fude_zoom_v2* _p, const f64* _rad, u32 _n, fude_zoom_v2 _guard, rde_color _color);
// Draws a stroke in its own colour.
void fude_zoom_render_stroke(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _object, fude_zoom_sim _to_screen, fude_zoom_v2 _half);
// Draws stroke _object through _to_screen; _as: drawn all in that colour (a glow), _extra pt wider each side. Under a
// third of a pixel it is not drawn. One whose points or width reach FUDE_ZOOM_RENDER_FAR is cut to the screen on the
// CPU (fude_zoom_draw_huge_stroke); one far bigger than the screen is drawn only in the runs that come near it; any
// other is drawn whole, from its mesh (kept with its decoded points while the view pans).
void fude_zoom_render_stroke_as(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _object, fude_zoom_sim _to_screen, fude_zoom_v2 _half, const rde_color* _as, f32 _extra);
// Triangles (three points each, screen units) drawn in _color.
void fude_zoom_render_triangles(const fude_zoom_v2* _t, u32 _count, rde_color _color);
// Triangles (three points each, in their own units) drawn through _to (to the screen) in _color.
void fude_zoom_render_triangles_through(const fude_zoom_v2* _t, u32 _count, fude_zoom_sim _to, rde_color _color);
// A polygon's inside (screen units, any shape): what of it is on the guard box,
// as triangles. For one far bigger than the screen, or seen once.
void fude_zoom_render_polygon_fill(fude_zoom_renderer* _r, const fude_zoom_v2* _p, u32 _n, fude_zoom_v2 _guard, rde_color _color);
// A FILL: its triangles (made once, kept with its points) through its place; a
// fill far bigger than the screen cut to it first. _as: a glow — its outline.
void fude_zoom_render_fill(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _object, fude_zoom_sim _to_screen, fude_zoom_v2 _half, const rde_color* _as, f32 _extra);

// --- render_shape.c: shapes ------------------------------------------------------------------------------------------

// Returns shape _object's outline in its own units at _segments (its curves), from at most _numbers of its numbers, and
// whether it closes (fude_zoom_geo's points and closed): as kept, or made now (fude_zoom_render_geo_keep, above). Its
// triangles are made when a fill first asks (fude_zoom_render_geo_tris).
fude_zoom_geo* fude_zoom_render_shape_geo(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _object, u32 _segments, u32 _numbers);

// A shape (shape.h): its outline from its numbers, cut as finely as its size on
// screen asks; its line, then filled when it says so. _as and _extra as for
// strokes (a glow: the line only). Dimensions, arrows, sizes, angles, guides, cut
// boards and symbols are drawn by their own functions; attributes not at all.
void fude_zoom_render_shape(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _object, fude_zoom_sim _to_screen, fude_zoom_v2 _half, const rde_color* _as, f32 _extra);

// --- render_text.c: texts --------------------------------------------------------------------------------------------

// A note's layout drawn: its top left at (_ox, _oy) on the screen, a unit of it _k points (its Y down, the screen's
// up), its letters _size; its marks under its words.
void fude_zoom_render_rich_draw(const fude_zoom_renderer* _r, const fude_zoom_rich* _rich, const c8* _text, f64 _ox, f64 _oy, f64 _k, f64 _size, rde_color _ink);
// A text: its words in the app's font at its size on the screen, upright (the
// font is drawn from curves, which do not turn); a note on its own paper. Too
// small to read, a note is still its paper and a text a faint bar a line. A
// formula is set and drawn as one (formula.h); a text with marks as a note.
void fude_zoom_render_text(fude_zoom_renderer* _r, const fude_zoom_scene* _s, u32 _object, fude_zoom_sim _to_screen, const rde_color* _as);

// --- render_symbol.c: symbols ----------------------------------------------------------------------------------------

// A symbol (symbol.h): its parts — filled as the shape is, or solid in its line's colour — and their lines
// (dashed where they are), its compartments' lines, and its text in its boxes. A graph's lines come from plot.h (kept,
// or made each frame while it plays), a part's pins' names and its face's words are written by them, and what moves in
// it is drawn as far along as render.h's moved says.
void fude_zoom_render_symbol(fude_zoom_renderer* _r, const fude_zoom_object* _o, const f64* _n, u32 _count, fude_zoom_sim _to_screen, fude_zoom_v2 _half, const rde_color* _as, f32 _extra);

#endif
