// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_CALC_H
#define FUDE_ZOOM_CALC_H

#include "rde.h"

// ===========================================================================
// Calc: reads a function of x as it is written ("y = sin(x)", "x^2/4 - 1", "2x + 3", "3(x+1)(x-2)", "sqrt(abs(x))",
// "e^-x", "|x|"), parses it once into steps, and works it out quickly for any x.
//
// Where it sits: `fude/zoom`, the canvas layer. plot.c draws the graphs with it (symbol.h's Maths family), and
// texcalc.c turns LaTeX into it and back.
// Its main type, fude_zoom_calc, is a plain value the caller keeps; there is nothing to free.
// Threads: pure functions over the caller's data, so they are safe on any thread; they are called on the app's main
// thread.
//
// What it reads: + − × ÷ (also * and /), ^ (also **; it groups right to left, so 2^3^2 is 2^9), % (the remainder),
// brackets, and |…| for a size. A number put before x, a letter, a function or a bracket multiplies it (2x, 3(x+1),
// 2sin(x)). pi (π) and e are constants. The functions are sin cos tan asin acos atan sinh cosh tanh sqrt (√) cbrt
// abs exp ln log (base 10) log2 floor ceil round sign, and sen and tg also work. x is the variable, and t and θ are
// read as x; any other letter (a, b, k…) is a variable whose value is given when the function is worked out. Capital
// letters are read as small ones, and "y =" or "f(x) =" before it is left out.
//
// Design: it is parsed into a list of steps run on a stack, so that working it out for each of the many x a graph
// needs costs no parsing.
// ===========================================================================

// The most steps a function may take; a longer one does not parse.
#define FUDE_ZOOM_CALC_STEPS 128u

// A function parsed into `count` steps: each step's operation (calc.c's own codes) and its number (a constant's value,
// or a variable's letter, 0 for a).
typedef struct {
    u8  op[FUDE_ZOOM_CALC_STEPS];
    f64 value[FUDE_ZOOM_CALC_STEPS];
    u32 count;
    u32 vars;   // the variables it uses, a bit for each letter (a is 1, b is 2…)
    b8  uses_x; // whether it uses x (or t, or θ)
} fude_zoom_calc;

// Parses the first line of _text; only what comes after its first "=" is read, so "y = …" and "f(x) = …" work.
// Returns false when it cannot be parsed, and then _c is left empty.
b8  fude_zoom_calc_parse(fude_zoom_calc* _c, const c8* _text);
// Parses the first line of _text the same way, but without leaving out what comes before an "=": for one side of an
// equation that the caller has split already (plot.h's).
b8  fude_zoom_calc_parse_expr(fude_zoom_calc* _c, const c8* _text);
// Reads a number as it is written ("2", "-2pi", "pi/2", "1e3") into _value. Returns false when it is not one: it uses
// x or a variable, it is not maths, or it is not finite.
b8  fude_zoom_calc_number(const c8* _text, f64* _value);
// Reads a plain decimal number at the very start of _s, as people write one: digits, a point and more digits, then an
// exponent when digits follow its e or E ("12", ".5", "1e3", "2.5E-4"; in "2e" only the 2). With _signed, a + or -
// before it is read too. Nothing else is: no hex ("0x2" is the 0, the x left to read), no "inf" or "nan", all of which
// rde_strtod would take. The value goes into *_v, however long the number is. Returns how many bytes it read: 0 when
// no number starts there. The one reader of typed numbers for calc, plots and charts. Any thread.
usize fude_zoom_calc_decimal(const c8* _s, b8 _signed, f64* _v);
// Writes _c back in calc's own words, with as few brackets as the order of operations needs ("x^2/4 - 1",
// "2sin(3x)", "-(x + 1)^2", "pi/2"); x is written for t and θ. Returns false when it does not fit in _out (_size
// bytes).
b8  fude_zoom_calc_write(const fude_zoom_calc* _c, c8* _out, usize _size);
// Writes _c as LaTeX, as tex.h reads it (\frac, ^{…}, \sqrt, \sin x, \left|…\right|, \pi), with x written as _x
// ("x", "t", "\\theta"). Returns false when it does not fit in _out (_size bytes).
b8  fude_zoom_calc_latex(const fude_zoom_calc* _c, const c8* _x, c8* _out, usize _size);
// Returns f(_x), or NaN where it has no value: the square root of a negative number, a division by zero, the logarithm
// of zero or less, or any function that uses a variable.
f64 fude_zoom_calc_at(const fude_zoom_calc* _c, f64 _x);
// Returns f(_x) with the variables' values _vars: 26 numbers, a to z, where NaN means not given (and gives NaN).
f64 fude_zoom_calc_at_with(const fude_zoom_calc* _c, f64 _x, const f64* _vars);

#endif
