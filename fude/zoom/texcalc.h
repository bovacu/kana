// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_TEXCALC
#define FUDE_ZOOM_TEXCALC

#include "rde.h"

// ===========================================================================
// LaTeX's maths as calc.h writes it: what a formula (tex.h) says, worked out —
// "\frac{3}{4} + 2^{3} =" answered, "y = \sqrt{x}" graphed.
//
// \frac \dfrac \tfrac → (a)/(b); \sqrt{a} → sqrt(a), \sqrt[n]{a} → (a)^(1/(n));
// ^{a} → ^(a); { } \left( \right) [ ] → brackets; \cdot \times → *; \div → /;
// \pi → pi; \sin \cos \tan \arcsin \arccos \arctan \sinh \cosh \tanh \ln \log
// \exp → calc's; \, \; \: \! \quad \qquad → a space; | as it is. Anything else
// a command (\infty, \sum, x_1, n!): not calc's — false.
// ===========================================================================

// _latex (_len bytes) as calc.h's words into _out (_size bytes). False: it says what calc does not.
b8 fude_zoom_texcalc(const c8* _latex, u32 _len, c8* _out, usize _size);

// ...tidied for a graph's line (plot.h): each side of its relation (= < > <= >=) with as few brackets as it needs —
// "y = \frac{x^{2}}{4} - 1" as "y = x^2/4 - 1". False: not calc's.
b8 fude_zoom_texcalc_tidy(const c8* _latex, u32 _len, c8* _out, usize _size);

// A formula ending in "=" (spaces after it aside) with nothing unknown before it ("\frac{3}{4} + 2^3 ="): its answer,
// written short (up to 10 figures: "8.75", "0.3333333333", "1.414213562", "-2", "1e+20"), into _out. False: not one.
b8 fude_zoom_texcalc_answer(const c8* _latex, u32 _len, c8* _out, usize _size);

#endif
