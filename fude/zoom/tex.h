// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_TEX
#define FUDE_ZOOM_TEX

#include "rde.h"
#include "zoom/mathfont.h"

// ===========================================================================
// MATHS SET AS TEX SETS IT (docs/product_split.md §4.3): LaTeX's maths read
// and laid out by TeX's own rules (The TeXbook, Appendix G) with an OpenType
// maths font's tables (mathfont.h) — glyphs placed and rules drawn, ready to
// be drawn at any size.
//
// What it reads (LaTeX's maths, as people write it):
//   letters in italic, digits upright; + − × · ± ∓ ÷ ∗ as operations,
//   = < > ≤ ≥ ≠ ≈ ≡ ∼ ∝ ∈ ∉ ⊂ ⊃ ⊆ ⊇ → ← ⇒ ⇔ ↦ … as relations (commands or
//   the characters themselves); Greek (\alpha … \Omega) and the usual symbols
//   (\infty \partial \nabla \hbar \ell \forall \exists \emptyset \angle …);
//   x^2  x_i  x_i^2  x'  {…} groups; \frac \dfrac \tfrac \binom; \sqrt[n]{…};
//   \sum \prod \int \iint \iiint \oint \bigcup \bigcap … with limits (above
//   and below in display, beside it in text; \limits \nolimits); \lim \max
//   \min \sup \inf \sin \cos \tan \log \ln \exp \det … and \operatorname{…};
//   \left( … \right) any of ( ) [ ] \{ \} | \| \langle \rangle \lfloor
//   \rfloor \lceil \rceil and . as none, grown to what is between them;
//   matrix pmatrix bmatrix Bmatrix vmatrix Vmatrix cases aligned
//   environments (& between cells, \\ between rows); \hat \bar \vec \dot
//   \ddot \tilde \check \breve \acute \grave \widehat \widetilde; \overline
//   \underline; \text{…} \mathrm \mathbf \mathit \mathbb \mathcal \mathfrak
//   \mathsf \mathtt; \, \: \; \! \quad \qquad ~; \displaystyle \textstyle
//   \scriptstyle \scriptscriptstyle.
// What it does not know is set as it was written, upright (and counted).
//
// Its units are ems — the formula's size 1 —, its origin the left end of its
// baseline, Y up.
// ===========================================================================

typedef struct {
    u32 glyph;
    f64 x, y;      // its origin (the left end of its baseline)
    f64 size;      // its em, ems (1: the formula's; a script's smaller)
} fude_zoom_tex_glyph;

typedef struct {
    f64 x, y, w, h;   // its bottom left, its width and its height
} fude_zoom_tex_rule;

typedef struct {
    rde_arr TYPE(fude_zoom_tex_glyph) glyphs;
    rde_arr TYPE(fude_zoom_tex_rule)  rules;
    f64 width, height, depth;   // right of the origin; above and below the baseline
    u32 unknown;                // what it did not know (set as written): commands, unmatched braces
} fude_zoom_tex_box;

void fude_zoom_tex_box_init(fude_zoom_tex_box* _b);
void fude_zoom_tex_box_free(fude_zoom_tex_box* _b);

// _latex (_len bytes, UTF-8) set in display style (an equation of its own: big operators' limits above and below, a
// fraction's parts full size) or in text style (in a line of words), with _font, into _out (emptied first).
void fude_zoom_tex_set(const fude_zoom_mfont* _font, const c8* _latex, u32 _len, b8 _display, fude_zoom_tex_box* _out);

#endif
