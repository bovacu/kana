// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_TEXCALC
#define FUDE_ZOOM_TEXCALC

#include "rde.h"

// ===========================================================================
// Texcalc: a formula's LaTeX (tex.h) rewritten in calc.h's words, so what a formula says can be worked out:
// "\frac{3}{4} + 2^{3} =" is answered, and "y = \sqrt{x}" is graphed.
//
// Where it sits: zoom, the canvas apps' layer, over calc.h. formula.c uses it to set a sum ending in "=" with its
// answer, and the page (page_text.c) uses it for the selection's Graph, which draws the selected formulas on a graph.
// Its main types and their lifecycle: none; the caller owns the output buffers.
// Threads: called on the main thread. It keeps no state between calls.
// Design: only what calc.h can work out is rewritten; anything else makes the call return false rather than guess.
//   \frac \dfrac \tfrac \cfrac become (a)/(b); \sqrt{a} becomes sqrt(a) and \sqrt[n]{a} becomes (a)^(1/(n));
//   ^{a} becomes ^(a); { } [ ] become brackets, and \left, \right and the sizes \big \Big \bigg \Bigg (and their l r m
//   forms, as tex.h's fude_zoom_tex_sizing reads them) are dropped; \cdot \times \ast become *, and
//   \div becomes /; \pi and \theta become pi and θ; \le \ge \lt \gt become <= >= < >; \sin \cos \tan \arcsin \arccos
//   \arctan \sinh \cosh \tanh \ln \log \exp become calc's functions; \, \; \: \! \quad \qquad and ~ become a space;
//   | stays as it is; \operatorname \mathrm \text \mathit \mathbf keep only their argument.
//   Any other command (\infty, \sum, \alpha, \middle, \bigcup) and subscripts, factorials, primes, & columns and \\
//   line breaks are not calc's, so the call returns false. So is LaTeX nested more than about 64 deep.
// ===========================================================================

// Rewrites _latex (_len bytes) in calc.h's words into _out (_size bytes). Returns false when it says something calc
// cannot work out, or when the result does not fit in _out.
b8 fude_zoom_texcalc(const c8* _latex, u32 _len, c8* _out, usize _size);

// As fude_zoom_texcalc, but tidied for a graph's line (plot.h): each side of its relation (= < > <= >=) is written
// with only the brackets it needs, so "y = \frac{x^{2}}{4} - 1" becomes "y = x^2/4 - 1". Returns false when it is not
// calc's, or does not fit in _out.
b8 fude_zoom_texcalc_tidy(const c8* _latex, u32 _len, c8* _out, usize _size);

// Writes the answer of a formula ending in "=" (ignoring spaces after it), such as "\frac{3}{4} + 2^3 =", into _out,
// written short with up to 10 figures: "8.75", "0.3333333333", "1.414213562", "-2", "1e+20". Returns false when it is
// not such a sum: it does not end in "=", it already has a relation before the "=", or calc cannot work it out to a
// number (an unknown command or a variable). An answer within 1e-13 of 0 is written as 0.
b8 fude_zoom_texcalc_answer(const c8* _latex, u32 _len, c8* _out, usize _size);

#endif
