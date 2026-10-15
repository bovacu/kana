// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_TEX
#define FUDE_ZOOM_TEX

#include "rde.h"
#include "zoom/mathfont.h"

// ===========================================================================
// Tex: LaTeX maths read and laid out by TeX's own rules (The TeXbook, Appendix G) with an OpenType maths font's tables
// (mathfont.h), giving glyphs placed and rules to draw, ready to be drawn at any size (docs/product_split.md §4.3).
//
// Where it sits: zoom, the canvas apps' layer. formula.c sets the canvas's formulas with it (and caches the result),
// handmath.c writes a set formula out by hand, and the renderer and the exports draw what it sets.
// Its main types and their lifecycle: fude_zoom_tex_box is the caller's struct. fude_zoom_tex_box_init creates its two
// arrays on the standard heap, fude_zoom_tex_set refills them, and fude_zoom_tex_box_free frees them.
// Threads: called on the main thread. It keeps no global state; each call touches only the font (read only) and the
//   caller's box.
// Design:
//   It works in two passes, as TeX does: the LaTeX is read into a tree of atoms (ordinary symbols, operators, binary
//   operations, relations, openings, closings, punctuation, inner formulas, whose classes set the space between them),
//   and the tree is laid out into boxes in a style (display, text, script or scriptscript, cramped or not).
//   What it reads is LaTeX's maths as people write it:
//     letters in italic and digits upright; + − × · ± ∓ ÷ ∗ as operations;
//     = < > ≤ ≥ ≠ ≈ ≡ ∼ ∝ ∈ ∉ ⊂ ⊃ ⊆ ⊇ → ← ⇒ ⇔ ↦ … as relations, written as commands or as the characters themselves;
//     Greek (\alpha … \Omega) and the usual symbols (\infty \partial \nabla \hbar \ell \forall \exists \emptyset
//     \angle …);
//     x^2  x_i  x_i^2  x'  and {…} groups; \frac \dfrac \tfrac \binom; \sqrt[n]{…};
//     \sum \prod \int \iint \iiint \oint \bigcup \bigcap … with limits (above and below in display style, beside
//     the sign in text style; \limits and \nolimits choose);
//     \lim \max \min \sup \inf \sin \cos \tan \log \ln \exp \det … and \operatorname{…};
//     \left( … \right) with any of ( ) [ ] \{ \} | \| \langle \rangle \lfloor \rfloor \lceil \rceil, or . for
//     none, grown to fit what is between them, and \middle between them grown with them (a \middle anywhere else is
//     counted as not known, and set at its own size); \big and its larger sizes;
//     the environments matrix, pmatrix, bmatrix, Bmatrix, vmatrix, Vmatrix, smallmatrix, array, cases, aligned and
//     align (& between cells, \\ between rows);
//     \hat \bar \vec \dot \ddot \tilde \check \breve \acute \grave \widehat \widetilde \overbrace \underbrace;
//     \overline \underline;
//     \text{…} \mathrm \mathbf \mathit \mathbb \mathcal \mathfrak \mathsf \mathtt;
//     \, \: \; \! \quad \qquad ~; \displaystyle \textstyle \scriptstyle \scriptscriptstyle.
//   What it does not know is set as it was written, upright, and counted in fude_zoom_tex_box.unknown, so a caller
//   can tell the person that something was not understood instead of failing.
//   Its units are ems (the formula's size is 1), its origin the left end of the baseline, y up.
//   See docs/infinite_canvas_design.md §63.
// ===========================================================================

// One glyph placed in a set formula; glyph is the font's glyph number.
typedef struct {
    u32 glyph;
    f64 x, y;      // its origin (the left end of its baseline), in ems
    f64 size;      // its em, in ems: 1 at the formula's own size, smaller in a script
} fude_zoom_tex_glyph;

// A filled rectangle in a set formula: a fraction's bar, a root's bar, an overline. Ems.
typedef struct {
    f64 x, y, w, h;   // its bottom left, its width and its height
} fude_zoom_tex_rule;

// A set formula.
typedef struct {
    rde_arr TYPE(fude_zoom_tex_glyph) glyphs;
    rde_arr TYPE(fude_zoom_tex_rule)  rules;
    f64 width, height, depth;   // its extent right of the origin, above the baseline and below it (ems)
    u32 unknown;                // how many things it did not know, such as commands and unmatched braces
} fude_zoom_tex_box;

// fude_zoom_tex_box_init sets up an empty box, its arrays on the standard heap; fude_zoom_tex_box_free frees them.
void fude_zoom_tex_box_init(fude_zoom_tex_box* _b);
void fude_zoom_tex_box_free(fude_zoom_tex_box* _b);

// Sets _latex (_len bytes of UTF-8, which need not end in a 0) with _font into _out, which is emptied first.
// _display chooses display style (an equation of its own: big operators' limits above and below, a fraction's parts
// full size) or text style (maths in a line of words). _out stays empty when _font is NULL or not loaded, or _latex is
// NULL.
void fude_zoom_tex_set(const fude_zoom_mfont* _font, const c8* _latex, u32 _len, b8 _display, fude_zoom_tex_box* _out);

// Reads _name (a command's name, without its backslash) as one of LaTeX's sizes for a delimiter: \big \Big \bigg
// \Bigg, each also with l, r or m after it (\bigl, \Biggr, \bigm: an opening, a closing, a relation). Returns false for
// any other name, \bigcup and \bigstar among them. *_ems (NULL: not wanted) gets how tall it makes its delimiter, in
// ems (1.2, 1.8, 2.4 or 3.0), and *_side (NULL: not wanted) 0, 'l', 'r' or 'm'. One list of these names for the two
// readers of LaTeX: tex.c sets them, and texcalc.c drops them (calc has no sizes). Any thread.
b8 fude_zoom_tex_sizing(const c8* _name, f64* _ems, c8* _side);

#endif
