// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_UNITS
#define FUDE_ZOOM_UNITS

#include "rde.h"

// ===========================================================================
// Lengths and angles as they are typed and read (research §2.10, items 1 and
// 7): what our numpad's fields take, and how a measurement is said.
//
//   TYPED    a field takes a number, a unit, a fraction, arithmetic and named
//            variables: 1-3/8", 450-2*18, 2' 3-1/2", (1200/3), stock-2*kerf.
//            What is typed is kept exactly (f64, millimetres): 3/8" is 9.525 mm.
//   SHOWN    at the document's precision — metric to so many decimals, inches
//            to so many decimals or to a fraction (1/2 to 1/64), feet and
//            inches if it likes. A value the precision rounds shows ≈ first:
//            "≈ 17.46 mm". One it shows exactly does not ("3/8\"").
//
// The grammar. Spaces may go between any two tokens (NBSP too); a leading ≈,
// as format writes it, is passed over.
//
//   NUMBERS   decimals with '.' or ',' as the point (17.5, 17,5, .5); there is
//             no thousands separator, so 1,200 is one point two. Two numbers
//             with '/' between them and no spaces are one number, a quotient:
//             3/8" is three eighths of an inch, not 3 over 8".
//   MIXED     a whole number, then a hyphen (no spaces round it) or spaces,
//             then a proper fraction (written tight, top below bottom): 1-3/8,
//             1 3/8. The hyphen form only when no '*' '/' '×' '÷' comes next:
//             1-3/8*2 is 1 - 3/8*2, as written, while (1-3/8)*2 and 1-3/8"*2
//             are 1-3/8 doubled. So 10-1/2 is ten and a half but 10 - 1/2 is
//             nine and a half; 450-2*18 is a subtraction (2*18 is no fraction),
//             and so is 1-9/8 (not proper). The spaced form has no other
//             reading, so 1 3/8*2 is 1-3/8 doubled.
//   UNITS     after a number or a bracketed group: mm cm m in ft (any case),
//             " ″ '' for inches, ' ′ for feet, ° º for degrees. A number in
//             feet followed straight away by a number is feet and inches: 1' 6",
//             1'6, 1' 6-3/8", 2ft 3in (the second in inches, its unit optional).
//             So is a tight hyphen, the drawing office's 1'-6", by the mixed
//             number's rule: not with a product next (2'-6*2) or another unit
//             on the inches (2'-6mm); those subtract. Variables take no unit;
//             unit words cannot be variable names.
//   SUMS      + - − (U+2212), * × / ÷, brackets and unary minus, the usual
//             precedence, left to right.
//   KINDS     each value is a measure (a length, or in an angle field an angle)
//             or a plain number. measure ± measure, measure × plain and
//             measure ÷ plain are measures; measure ÷ measure is plain; plain ±
//             measure takes the plain one in the field's unit (450mm-18 in an
//             inch field is 450 mm less 18"); measure × measure and plain ÷
//             measure are errors. A plain result is in the field's unit, so
//             450-2*18 in a mm field is 414 mm, and (1200mm/3mm) is 400 of it.
//   FIELDS    a length field takes length units and length variables; an angle
//             field degrees only (a length in it is an error); a number field
//             no units and no lengths at all.
//
// Variables: a small table of names ([A-Za-z_][A-Za-z0-9_]*, up to 23 bytes,
// any case matching) to values — lengths in mm, or plain numbers (stock, kerf,
// bit, n).
// ===========================================================================

typedef enum {
    FUDE_ZOOM_UNIT_MM = 0,
    FUDE_ZOOM_UNIT_CM,
    FUDE_ZOOM_UNIT_M,
    FUDE_ZOOM_UNIT_IN,
    FUDE_ZOOM_UNIT_FT,
    FUDE_ZOOM_UNIT_COUNT
} FUDE_ZOOM_UNIT_;

// Millimetres in one of _u (exact: 25.4 an inch, 304.8 a foot); 0 for no unit.
f64       fude_zoom_unit_mm(FUDE_ZOOM_UNIT_ _u);
// What format writes after a value in _u: "mm", "cm", "m", "\"" and "'" (the
// workshop's marks, not "in" and "ft"); "" for no unit.
const c8* fude_zoom_unit_suffix(FUDE_ZOOM_UNIT_ _u);

#define FUDE_ZOOM_VARS_MOST 32u
#define FUDE_ZOOM_VAR_NAME  24u   // bytes, its NUL included

typedef struct {
    c8  name[FUDE_ZOOM_VAR_NAME];
    f64 value;    // mm when a length
    b8  length;
} fude_zoom_var;

typedef struct {
    fude_zoom_var vars[FUDE_ZOOM_VARS_MOST];
    u32           count;
} fude_zoom_vars;

// _name set to _value (a length in mm, or a plain number), replacing one of
// the same name in any case. False: a bad name (or a unit word), a value that
// is not finite, or the table full.
b8                   fude_zoom_vars_set(fude_zoom_vars* _v, const c8* _name, f64 _value, b8 _length);
// The variable called _name, in any case; NULL if there is none.
const fude_zoom_var* fude_zoom_vars_find(const fude_zoom_vars* _v, const c8* _name);
// _name taken out of the table (the rest keep their order). False: not there.
b8                   fude_zoom_vars_remove(fude_zoom_vars* _v, const c8* _name);

typedef enum {
    FUDE_ZOOM_UNITS_OK = 0,
    FUDE_ZOOM_UNITS_ERROR_EMPTY,       // nothing typed
    FUDE_ZOOM_UNITS_ERROR_SYNTAX,      // something where it cannot go, or the text ran out
    FUDE_ZOOM_UNITS_ERROR_PAREN,       // a bracket never closed (at: the bracket)
    FUDE_ZOOM_UNITS_ERROR_NAME,        // no variable of that name
    FUDE_ZOOM_UNITS_ERROR_UNIT,        // a unit the field does not take, on a value that has one, with no number, or a word that is no unit
    FUDE_ZOOM_UNITS_ERROR_DIMENSION,   // measure × measure, plain ÷ measure, a length where none goes
    FUDE_ZOOM_UNITS_ERROR_ZERO,        // division by zero
    FUDE_ZOOM_UNITS_ERROR_RANGE,       // a result too big to be finite, or brackets nested past 64
    FUDE_ZOOM_UNITS_ERROR_COUNT
} FUDE_ZOOM_UNITS_ERROR_;

// What went wrong, and the byte of the text where it did.
typedef struct {
    FUDE_ZOOM_UNITS_ERROR_ code;
    u32                    at;
} fude_zoom_units_error;

// A typed length into *_mm, in millimetres; a plain number is in _unit. _vars
// and _error may be NULL. False on anything malformed: *_mm is left alone and
// *_error says what and where (on success, OK at 0).
b8  fude_zoom_units_length(const c8* _text, FUDE_ZOOM_UNIT_ _unit, const fude_zoom_vars* _vars, f64* _mm, fude_zoom_units_error* _error);
// A typed angle into *_degrees: "45", "45°", "90-30", "22,5", "1/8*360".
b8  fude_zoom_units_angle(const c8* _text, const fude_zoom_vars* _vars, f64* _degrees, fude_zoom_units_error* _error);
// A typed plain number (a variable's value, a count) into *_out: no units.
b8  fude_zoom_units_number(const c8* _text, const fude_zoom_vars* _vars, f64* _out, fude_zoom_units_error* _error);

// A document's precision. unit is also what a bare number typed means.
//   decimals     digits after the point, at most FUDE_ZOOM_UNITS_DECIMALS:
//                metric, decimal inches, decimal feet, angles. Trailing zeros
//                are dropped ("17.5 mm", not "17.50 mm"): the ≈ says whether
//                what is shown is all there is.
//   denominator  inches: 0 decimal; else fractions of 1/denominator, reduced
//                (1, 2, 4 … 64; any other number is taken as the power of two
//                below it, 64 at most).
//   feet_inches  IN or FT shown as feet and inches, 1' 6-3/8" (just the inches
//                under a foot, just the feet when they are whole). Without it,
//                IN is all inches (18-3/8") and FT decimal feet (1.53').
typedef struct {
    FUDE_ZOOM_UNIT_ unit;
    u8              decimals;
    u8              denominator;
    b8              feet_inches;
} fude_zoom_units_style;

#define FUDE_ZOOM_UNITS_DECIMALS 6u
#define FUDE_ZOOM_UNITS_TEXT     48u   // a buffer this big holds anything format writes

// _mm said in _style into _out (UTF-8, NUL-terminated): "450 mm", "≈ 17.46 mm",
// "1-3/8\"", "≈ 1-23/64\"", "2' 3-1/2\"", "-12 mm". ≈ comes first when the
// value shown is not the value (1e-9 of it, or of a millimetre if smaller).
// What does not fit is left off a token at a time. The bytes written (0, and
// nothing, for a value that is not finite).
u32 fude_zoom_units_format(f64 _mm, const fude_zoom_units_style* _style, c8* _out, usize _size);
// _degrees the same way, to _decimals: "45°", "≈ 33.7°".
u32 fude_zoom_units_format_angle(f64 _degrees, u8 _decimals, c8* _out, usize _size);

#endif
