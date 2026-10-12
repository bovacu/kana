// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_FORMULA
#define FUDE_ZOOM_FORMULA

#include "rde.h"
#include "zoom/zoom.h"
#include "zoom/tex.h"

// ===========================================================================
// FORMULAS ON THE CANVAS: a text object whose words are LaTeX (scene.h's
// FUDE_ZOOM_TEXT_FORMULA), set by tex.h with the maths font and drawn from its
// glyphs' outlines — as sharp at any zoom as a stroke.
//
// Kept here, for every page that draws: the font (its file's bytes), each
// formula's setting by its words (set once, drawn every frame), and each
// glyph's triangles at the detail it is seen at (its curves cut finer the
// bigger it is on screen: a level each four times — cut once, kept).
// ===========================================================================

#define FUDE_ZOOM_FORMULA_PAD 0.12   // round a formula's ink in its box, ems

// A glyph at a detail: its triangles (font units, three a triangle) and its contours (their points in turn, each's end).
typedef struct {
    rde_arr tris;   // fude_zoom_v2
    rde_arr ring;   // fude_zoom_v2
    rde_arr ends;   // u32: past each contour's last point
} fude_zoom_formula_mesh;

typedef struct fude_zoom_formulas {
    u8*             bytes;      // the font file (owned; NULL: none read)
    fude_zoom_mfont font;
    b8              ready;
    rde_hash_map    sets;       // a formula's words (hashed) → its index in boxes
    rde_arr         boxes;      // fude_zoom_tex_box
    rde_hash_map    meshes;     // a glyph and a detail → its index in mesh_list
    rde_arr         mesh_list;  // fude_zoom_formula_mesh
} fude_zoom_formulas;

void fude_zoom_formulas_init(fude_zoom_formulas* _f);
void fude_zoom_formulas_free(fude_zoom_formulas* _f);
// The maths font from _path (false: none — formulas then not drawn).
b8   fude_zoom_formulas_load(fude_zoom_formulas* _f, const c8* _path);

// _text (_len bytes) set (in display style: a formula of its own), kept by its words — ending in "=", with its answer
// after it when it is a sum that has one (texcalc.h). NULL: no font. (Good until another
// formula is set: the kept ones' array grows.)
const fude_zoom_tex_box* fude_zoom_formulas_set(fude_zoom_formulas* _f, const c8* _text, u32 _len);

// ...in text style (_display false: a line of words' — a fraction's parts smaller, limits beside: a graph's legend), or
// display style; kept apart.
const fude_zoom_tex_box* fude_zoom_formulas_set_as(fude_zoom_formulas* _f, const c8* _text, u32 _len, b8 _display);

// Glyph _g's triangles cut finely enough for its em _px screen points across: how many; *_out at them (font units,
// three a triangle — kept until another is cut).
u32  fude_zoom_formulas_mesh(fude_zoom_formulas* _f, u32 _g, f64 _px, const fude_zoom_v2** _out);

// Glyph _g's outline (font units, Y up), its curves no further than _tol out, added to _out (fude_zoom_v2) — its
// contours joined into one ring (filled even-odd: as drawn), or each a ring of its own (_apart: a cutter's paths) —
// with each point's ring added to _rings (u32; NULL: not wanted). How many points.
u32  fude_zoom_formulas_outline(const fude_zoom_formulas* _f, u32 _g, f64 _tol, b8 _apart, rde_arr* _out, rde_arr* _rings);

// ...its contours at that detail: how many; *_points at their points (font units), *_ends at past each's last.
u32  fude_zoom_formulas_rings(fude_zoom_formulas* _f, u32 _g, f64 _px, const fude_zoom_v2** _points, const u32** _ends);

// A formula's box (frame units) for its em _size: its ink and the pad round it — what its text object's width and
// height are.
void fude_zoom_formulas_size(const fude_zoom_tex_box* _box, f64 _size, f64* _w, f64* _h);

#endif
