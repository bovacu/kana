// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_UNITS
#define FUDE_ZOOM_UNITS

#include "rde.h"

// ===========================================================================
// Units: reads lengths, angles and plain numbers as they are typed into the numpad's fields, and writes a measurement
// as it is shown (docs/research/infinite_canvas_research.md §2.10, items 1 and 7).
//
// Where it sits: `fude/zoom`, the canvas layer. numpad.c and the forms that take sizes (sizeform, pointform,
// partsform, repeatform, sheetform), the instruments, the renderer and the exports use it.
// Its main types are plain values the caller keeps: a table of variables (fude_zoom_vars) and a document's precision
// (fude_zoom_units_style). Nothing is allocated.
// Threads: pure functions over the caller's data and constant tables, safe on any thread; they are called on the
// app's main thread.
//
//   TYPED    A field takes a number, a unit, a fraction, arithmetic and named variables: 1-3/8", 450-2*18, 2' 3-1/2",
//            (1200/3), stock-2*kerf. What is typed is kept exactly, as an f64 in millimetres: 3/8" is 9.525 mm.
//   SHOWN    A value is shown at the document's precision: metric to so many decimals; inches to so many decimals or
//            to a fraction (1/2 to 1/64); feet and inches if the document prefers. A value the precision rounds is
//            shown with ≈ first ("≈ 17.46 mm"); one shown exactly has none ("3/8\"").
//
// The grammar. Spaces may go between any two tokens (a no-break space too). A leading ≈, as fude_zoom_units_format
// writes it, is passed over.
//
//   NUMBERS   Decimals take '.' or ',' as the point (17.5, 17,5, .5). There is no thousands separator, so 1,200 is
//             one point two. Two numbers with '/' between them and no spaces are one number, a quotient: 3/8" is
//             three eighths of an inch, not 3 over 8".
//   MIXED     A whole number, then a hyphen (no spaces round it) or spaces, then a proper fraction (written tight, its
//             top below its bottom): 1-3/8, 1 3/8. The hyphen form counts only when no '*' '/' '×' '÷' comes next:
//             1-3/8*2 is 1 - 3/8*2, as written, while (1-3/8)*2 and 1-3/8"*2 are 1-3/8 doubled. So 10-1/2 is ten
//             and a half but 10 - 1/2 is nine and a half; 450-2*18 is a subtraction (2*18 is not a fraction), and so
//             is 1-9/8 (not a proper fraction). The spaced form cannot be read any other way, so 1 3/8*2 is 1-3/8
//             doubled.
//   UNITS     They go after a number or a bracketed group: mm cm m in ft (in any case), " ″ '' for inches, ' ′ for
//             feet, ° º for degrees. A number in feet followed straight away by a number is feet and inches: 1' 6",
//             1'6, 1' 6-3/8", 2ft 3in (the second number is in inches, and its unit may be left out). A tight hyphen
//             does the same, the drawing office's 1'-6", by the mixed number's rule: but not when a product follows
//             (2'-6*2) or the inches carry another unit (2'-6mm); those subtract. Variables take no unit, and unit
//             words cannot be variable names.
//   SUMS      + - − (U+2212), * × / ÷, brackets and unary minus, with the usual precedence, left to right.
//   KINDS     Each value is a measure (a length, or an angle in an angle field) or a plain number. measure ± measure,
//             measure × plain and measure ÷ plain are measures; measure ÷ measure is plain; in plain ± measure the
//             plain number is taken in the field's unit (450mm-18 in an inch field is 450 mm less 18"); measure ×
//             measure and plain ÷ measure are errors. A plain result is in the field's unit, so 450-2*18 in a mm
//             field is 414 mm, and (1200mm/3mm) is 400 mm.
//   FIELDS    A length field takes length units and length variables; an angle field takes degrees only (a length in
//             it is an error); a number field takes no units and no lengths at all.
//
// Variables: a small table of names ([A-Za-z_][A-Za-z0-9_]*, up to 23 bytes, matched in any case) to values, each a
// length in mm or a plain number (stock, kerf, bit, n).
// ===========================================================================

// The units of length a field or a document can be in.
typedef enum {
    FUDE_ZOOM_UNIT_MM = 0,
    FUDE_ZOOM_UNIT_CM,
    FUDE_ZOOM_UNIT_M,
    FUDE_ZOOM_UNIT_IN,
    FUDE_ZOOM_UNIT_FT,
    FUDE_ZOOM_UNIT_COUNT
} FUDE_ZOOM_UNIT_;

// Returns how many millimetres one _u is (exactly: 25.4 for an inch, 304.8 for a foot); 0 for no unit.
f64       fude_zoom_unit_mm(FUDE_ZOOM_UNIT_ _u);
// Returns what fude_zoom_units_format writes after a value in _u: "mm", "cm", "m", "\"" or "'" (the workshop's marks,
// not "in" and "ft"); "" for no unit. The strings are static.
const c8* fude_zoom_unit_suffix(FUDE_ZOOM_UNIT_ _u);

// The most variables a table holds, and the size of a variable's name.
#define FUDE_ZOOM_VARS_MOST 32u
#define FUDE_ZOOM_VAR_NAME  24u   // in bytes, its terminating 0 included

// A named variable: a length (`length` true, `value` in millimetres) or a plain number.
typedef struct {
    c8  name[FUDE_ZOOM_VAR_NAME];
    f64 value;    // in millimetres when it is a length
    b8  length;
} fude_zoom_var;

// A table of variables, the first `count` in use.
typedef struct {
    fude_zoom_var vars[FUDE_ZOOM_VARS_MOST];
    u32           count;
} fude_zoom_vars;

// Sets variable _name to _value (a length in mm when _length, else a plain number), replacing one of the same name in
// any case. Returns false for a bad name (or a unit word, so that 2 m always stays two metres), a value that is not
// finite, or a full table.
b8                   fude_zoom_vars_set(fude_zoom_vars* _v, const c8* _name, f64 _value, b8 _length);
// Returns the variable called _name, matched in any case, or NULL when there is none. The pointer is into the table,
// and stays valid until the table changes.
const fude_zoom_var* fude_zoom_vars_find(const fude_zoom_vars* _v, const c8* _name);
// Takes variable _name out of the table; the rest keep their order. Returns false when it is not there.
b8                   fude_zoom_vars_remove(fude_zoom_vars* _v, const c8* _name);

// What went wrong reading a typed value.
typedef enum {
    FUDE_ZOOM_UNITS_OK = 0,
    FUDE_ZOOM_UNITS_ERROR_EMPTY,       // nothing typed
    FUDE_ZOOM_UNITS_ERROR_SYNTAX,      // something where it cannot go, or the text ran out
    FUDE_ZOOM_UNITS_ERROR_PAREN,       // a bracket never closed (`at` is the bracket)
    FUDE_ZOOM_UNITS_ERROR_NAME,        // no variable of that name
    // A unit the field does not take, a unit on a value that already has one, a unit with no number, or a word that
    // is not a unit.
    FUDE_ZOOM_UNITS_ERROR_UNIT,        // a unit where it cannot go, or a word that is not one
    FUDE_ZOOM_UNITS_ERROR_DIMENSION,   // measure × measure, plain ÷ measure, or a length where none can go
    FUDE_ZOOM_UNITS_ERROR_ZERO,        // division by zero
    FUDE_ZOOM_UNITS_ERROR_RANGE,       // a result too big to be finite, or brackets nested past 64
    FUDE_ZOOM_UNITS_ERROR_COUNT
} FUDE_ZOOM_UNITS_ERROR_;

// What went wrong, and the byte of the text where it did. On success the code is FUDE_ZOOM_UNITS_OK and `at` is 0.
typedef struct {
    FUDE_ZOOM_UNITS_ERROR_ code;
    u32                    at;
} fude_zoom_units_error;

// Reads a typed length into *_mm, in millimetres; a plain number is taken in _unit. _vars and _error may be NULL.
// Returns false for anything malformed: *_mm is then left alone, and *_error says what went wrong and where.
b8  fude_zoom_units_length(const c8* _text, FUDE_ZOOM_UNIT_ _unit, const fude_zoom_vars* _vars, f64* _mm, fude_zoom_units_error* _error);
// Reads a typed angle into *_degrees: "45", "45°", "90-30", "22,5", "1/8*360". Returns false as the length does.
b8  fude_zoom_units_angle(const c8* _text, const fude_zoom_vars* _vars, f64* _degrees, fude_zoom_units_error* _error);
// Reads a typed plain number (a variable's value, a count) into *_out; it takes no units. Returns false as the length
// does.
b8  fude_zoom_units_number(const c8* _text, const fude_zoom_vars* _vars, f64* _out, fude_zoom_units_error* _error);

// A document's precision. `unit` is what values are shown in, and also what a bare number typed means.
//   decimals     The digits after the point, at most FUDE_ZOOM_UNITS_DECIMALS, for metric, decimal inches, decimal
//                feet and angles. Trailing zeros are dropped ("17.5 mm", not "17.50 mm"), because the ≈ already says
//                whether what is shown is all there is.
//   denominator  For inches: 0 shows decimals; otherwise fractions of 1/denominator, reduced (1, 2, 4 … 64; any other
//                number is taken as the power of two below it, 64 at most).
//   feet_inches  Shows IN or FT as feet and inches, 1' 6-3/8" (only the inches under a foot, only the feet when the
//                inches are none). Without it, IN is all inches (18-3/8") and FT is decimal feet (1.53').
typedef struct {
    FUDE_ZOOM_UNIT_ unit;
    u8              decimals;
    u8              denominator;
    b8              feet_inches;
} fude_zoom_units_style;

// The most decimals shown, and a buffer size that holds anything fude_zoom_units_format writes.
#define FUDE_ZOOM_UNITS_DECIMALS 6u
#define FUDE_ZOOM_UNITS_TEXT     48u   // a buffer this big holds anything fude_zoom_units_format writes

// Writes _mm as _style shows it into _out (UTF-8, ending in a 0): "450 mm", "≈ 17.46 mm", "1-3/8\"", "≈ 1-23/64\"",
// "2' 3-1/2\"", "-12 mm". ≈ comes first when the value shown is not the value (to within 1e-9 of it, or 1e-9 mm for a
// value under a millimetre). What does not fit is left off a whole token at a time, so the text never ends inside
// a character. Returns the bytes written; 0, writing nothing, for a value that is not finite.
u32 fude_zoom_units_format(f64 _mm, const fude_zoom_units_style* _style, c8* _out, usize _size);
// Writes _degrees the same way, to _decimals decimals: "45°", "≈ 33.7°".
u32 fude_zoom_units_format_angle(f64 _degrees, u8 _decimals, c8* _out, usize _size);

#endif
