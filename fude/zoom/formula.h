// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_FORMULA
#define FUDE_ZOOM_FORMULA

#include "rde.h"
#include "zoom/zoom.h"
#include "zoom/tex.h"

// ===========================================================================
// Formula: formulas on the canvas. A formula is a text object whose words are LaTeX (scene.h's
// FUDE_ZOOM_TEXT_FORMULA), set by tex.h with the maths font and drawn from its glyphs' outlines, so it is as sharp at
// any zoom as a stroke.
//
// Where it sits: zoom, the canvas apps' layer. The page (page.h) owns one fude_zoom_formulas; the renderer
// (render_text.c, render_symbol.c), the PDF canvas (pdfview.c) and the exports (export.c) draw formulas through it.
// Its main types and their lifecycle: fude_zoom_formulas is the caller's struct. fude_zoom_formulas_init sets it up,
// fude_zoom_formulas_load reads the font into it, and fude_zoom_formulas_free frees everything it holds. The boxes and
// triangles it hands out stay its own; see each function for how long they stay valid.
// Threads: main thread only. Setting a formula or cutting a glyph changes its caches.
// Files: reads the maths font file whole into memory (kfile.h's fude_file_read); the page loads
//   assets/fonts/NotoSansMath-Regular.ttf.
// Design:
//   It keeps, for every page that draws: the font (its file's bytes), each formula's setting by its words (set once,
//   drawn every frame; at most 512 are kept, then all are dropped and set again as they are drawn), and each glyph's
//   triangles at the detail it is seen at (at most 6000 are kept, then all are dropped).
//   A glyph's curves are cut finer the bigger it is on screen. There are 9 levels of detail: at the coarsest, a curve
//   is at most 64 font units from its straight pieces, and each finer level is four times as close. The coarsest level
//   within a third of a screen point is used. Each glyph is cut once at each level and kept.
//   A glyph's contours are joined into one polygon filled even-odd, so the joins cancel out and an o's hole stays a
//   hole, and that polygon is triangulated by fill.h.
//   See docs/infinite_canvas_design.md §63.
// ===========================================================================

#define FUDE_ZOOM_FORMULA_PAD 0.12   // the space round a formula's ink inside its text object's box, in ems

// A glyph cut at one level of detail: its triangles (font units, three points to a triangle) and its contours (all
// their points one after another, and where each contour ends).
typedef struct {
    rde_arr tris;   // fude_zoom_v2, three to a triangle
    rde_arr ring;   // fude_zoom_v2, every contour's points one after another
    rde_arr ends;   // u32: one past each contour's last point
} fude_zoom_formula_mesh;

// The font and the caches of set formulas and cut glyphs.
typedef struct fude_zoom_formulas {
    u8*             bytes;      // the font file, owned; NULL when none was read
    fude_zoom_mfont font;
    // True when the font was read and loaded; while false, every call returns NULL or 0.
    b8              ready;
    rde_hash_map    sets;       // 'D' or 'T' (its style) then a formula's words (owned strings) to its index in boxes
    rde_arr         boxes;      // fude_zoom_tex_box
    rde_hash_map    meshes;     // key: the glyph shifted left 8 bits, or'd with the level; value: index in mesh_list
    rde_arr         mesh_list;  // fude_zoom_formula_mesh
} fude_zoom_formulas;

// fude_zoom_formulas_init sets up empty caches with no font; fude_zoom_formulas_free frees the caches and the font.
void fude_zoom_formulas_init(fude_zoom_formulas* _f);
void fude_zoom_formulas_free(fude_zoom_formulas* _f);
// Reads the maths font from _path, replacing any font read before, and empties the caches. Returns false when the file
// cannot be read or is not a font mathfont.h can load; formulas are then not drawn.
b8   fude_zoom_formulas_load(fude_zoom_formulas* _f, const c8* _path);

// Returns _text (_len bytes of LaTeX) set in display style, as a formula of its own. It is set once and then kept by
// its words. A formula ending in "=" is set with its answer after it when it is a sum that has one (texcalc.h).
// Returns NULL when no font is loaded. The box stays valid only until another formula is set, because the kept boxes
// live in an array that grows.
const fude_zoom_tex_box* fude_zoom_formulas_set(fude_zoom_formulas* _f, const c8* _text, u32 _len);

// As fude_zoom_formulas_set, but _display false sets it in text style, as in a line of words (a fraction's parts
// smaller, limits beside the sign), which a graph's legend uses. The two styles are kept apart.
const fude_zoom_tex_box* fude_zoom_formulas_set_as(fude_zoom_formulas* _f, const c8* _text, u32 _len, b8 _display);

// Returns how many triangles glyph _g has when cut finely enough for its em to be _px screen points across, and points
// *_out at them (font units, three points to a triangle). The triangles stay valid until another glyph is cut (which
// may empty the cache) or the font is loaded again. Returns 0, with *_out NULL, when no font is loaded or _px is not
// above 0.
u32  fude_zoom_formulas_mesh(fude_zoom_formulas* _f, u32 _g, f64 _px, const fude_zoom_v2** _out);

// Appends glyph _g's outline to _out (fude_zoom_v2, font units, y up), its curves no further than _tol from their
// straight pieces, and returns how many points it added. Its contours are joined into one ring, filled even-odd as it
// is drawn, or with _apart each contour is a ring of its own (as a cutting machine wants its paths). Unless _rings is
// NULL, each point's ring number (u32) is appended to it. Returns 0 when no font is loaded.
u32  fude_zoom_formulas_outline(const fude_zoom_formulas* _f, u32 _g, f64 _tol, b8 _apart, rde_arr* _out, rde_arr* _rings);

// Returns how many contours glyph _g has at the detail fude_zoom_formulas_mesh would cut it at, pointing *_points at
// their points (font units) and *_ends at one past each contour's last point. Kept and valid as the triangles are.
u32  fude_zoom_formulas_rings(fude_zoom_formulas* _f, u32 _g, f64 _px, const fude_zoom_v2** _points, const u32** _ends);

// Writes the size of a formula's box (frame units) for an em of _size: its ink plus FUDE_ZOOM_FORMULA_PAD round it.
// This is its text object's width and height. A NULL _box counts as one em square.
void fude_zoom_formulas_size(const fude_zoom_tex_box* _box, f64 _size, f64* _w, f64* _h);

#endif
