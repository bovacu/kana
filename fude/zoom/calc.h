// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_CALC_H
#define FUDE_ZOOM_CALC_H

#include "rde.h"

// ===========================================================================
// A function of x as it is written ("y = sin(x)", "x^2/4 - 1", "2x + 3",
// "3(x+1)(x-2)", "sqrt(abs(x))", "e^-x", "|x|"): parsed once into steps, worked
// out for any x quickly (Sketching's graphs: symbol.h's Maths family).
//
// + − × ÷ (also * /), ^ (right to left), % (remainder), brackets, |…| (its
// size), a number put before x or a bracket multiplies it; pi (π), e; sin cos
// tan asin acos atan sinh cosh tanh sqrt (√) cbrt abs exp ln log (base 10) log2
// floor ceil round sign; x (and t, θ as x); any other letter a variable (a, b,
// k…: its value given as it is worked out). "y =" or "f(x) =" before it is left out.
// ===========================================================================

#define FUDE_ZOOM_CALC_STEPS 128u

typedef struct {
    u8  op[FUDE_ZOOM_CALC_STEPS];
    f64 value[FUDE_ZOOM_CALC_STEPS];
    u32 count;
    u32 vars;   // the variables it uses (a bit a letter: a 1, b 2…)
    b8  uses_x; // ...and x (t, θ)
} fude_zoom_calc;

// _text parsed (its first line). False: it cannot be (nothing of it kept).
b8  fude_zoom_calc_parse(fude_zoom_calc* _c, const c8* _text);
// ...all of it, an "=" in it not left out (a side of an equation: plot.h's).
b8  fude_zoom_calc_parse_expr(fude_zoom_calc* _c, const c8* _text);
// A number as it is written ("2", "-2pi", "pi/2", "1e3"): its value. False: not one (x or a variable in it, or not maths).
b8  fude_zoom_calc_number(const c8* _text, f64* _value);
// _c written back as calc's words, as few brackets as its order needs ("x^2/4 - 1", "2sin(3x)", "-(x + 1)^2", "pi/2"; x for
// t and θ). False: no room in _out (_size bytes).
b8  fude_zoom_calc_write(const fude_zoom_calc* _c, c8* _out, usize _size);
// ...as LaTeX (tex.h's): \frac, ^{…}, \sqrt, \sin x, \left|…\right|, \pi, its variable written _x ("x", "t", "\\theta").
b8  fude_zoom_calc_latex(const fude_zoom_calc* _c, const c8* _x, c8* _out, usize _size);
// f(_x) (NaN: not there — a square root of less than nothing, a division by nothing; or a variable it uses).
f64 fude_zoom_calc_at(const fude_zoom_calc* _c, f64 _x);
// ...its variables' values _vars (26: a to z; NaN: not given).
f64 fude_zoom_calc_at_with(const fude_zoom_calc* _c, f64 _x, const f64* _vars);

#endif
