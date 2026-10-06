// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

// Typed lengths and angles (fude/zoom/units): every unit, fractions and mixed
// numbers, feet and inches, precedence, the kinds' rules and their errors with
// where they are, variables, commas, the Unicode operators and primes — and
// how values are said: decimals, fractions at each denominator, ≈ exactly when
// the value is rounded, and back again.
#include "zoom/units.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int fails = 0, checks = 0;
#define CHECK(c) do { checks++; if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while(0)

static u32 rng = 2026u;
static u32 rnd(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }
static f64 rndf(void) { return (f64)(rnd() & 0xFFFFFF) / (f64)0xFFFFFF; }

static fude_zoom_vars vars;

static b8 near(f64 a, f64 b) { return fabs(a - b) <= 1e-9 * fmax(1.0, fabs(b)); }
#define INCH(x) ((x) * 25.4)

// A length field's value, NAN when it fails.
static f64 len(const c8* s, FUDE_ZOOM_UNIT_ u) {
    f64 mm = NAN;
    fude_zoom_units_error e = { FUDE_ZOOM_UNITS_ERROR_COUNT, 999u };
    if(!fude_zoom_units_length(s, u, &vars, &mm, &e)) {
        printf("  \"%s\" failed: error %d at %u\n", s, (int)e.code, e.at);
        return NAN;
    }
    if(e.code != FUDE_ZOOM_UNITS_OK || e.at != 0u) return NAN;
    return mm;
}
static f64 ang(const c8* s) {
    f64 d = NAN;
    if(!fude_zoom_units_angle(s, &vars, &d, NULL)) printf("  angle \"%s\" failed\n", s);
    return d;
}
static f64 num(const c8* s) {
    f64 d = NAN;
    if(!fude_zoom_units_number(s, &vars, &d, NULL)) printf("  number \"%s\" failed\n", s);
    return d;
}
#define LEN(s, u, mm) CHECK(near(len(s, FUDE_ZOOM_UNIT_##u), (mm)))
#define ANG(s, d)     CHECK(near(ang(s), (d)))
#define NUM(s, d)     CHECK(near(num(s), (d)))

// field: 0 length (unit u), 1 angle, 2 number. It must fail with code at byte
// at, leaving the output alone.
static b8 fails_with(u8 field, const c8* s, FUDE_ZOOM_UNIT_ u, FUDE_ZOOM_UNITS_ERROR_ code, u32 at) {
    f64 out = 12345.0;
    fude_zoom_units_error e = { FUDE_ZOOM_UNITS_OK, 999u };
    const b8 ok = field == 0 ? fude_zoom_units_length(s, u, &vars, &out, &e)
                : field == 1 ? fude_zoom_units_angle(s, &vars, &out, &e)
                : fude_zoom_units_number(s, &vars, &out, &e);
    if(ok || e.code != code || e.at != at || out != 12345.0) {
        printf("  \"%s\": ok %d, error %d at %u (wanted %d at %u)\n", s ? s : "(null)", ok, (int)e.code, e.at, (int)code, at);
        return false;
    }
    return true;
}
#define LEN_ERR(s, u, code, at) CHECK(fails_with(0, s, FUDE_ZOOM_UNIT_##u, FUDE_ZOOM_UNITS_ERROR_##code, at))
#define ANG_ERR(s, code, at)    CHECK(fails_with(1, s, FUDE_ZOOM_UNIT_MM, FUDE_ZOOM_UNITS_ERROR_##code, at))
#define NUM_ERR(s, code, at)    CHECK(fails_with(2, s, FUDE_ZOOM_UNIT_MM, FUDE_ZOOM_UNITS_ERROR_##code, at))

static c8 shown[FUDE_ZOOM_UNITS_TEXT];
static b8 says(f64 mm, FUDE_ZOOM_UNIT_ u, u8 decimals, u8 den, b8 fi, const c8* want) {
    const fude_zoom_units_style st = { u, decimals, den, fi };
    const u32 n = fude_zoom_units_format(mm, &st, shown, sizeof shown);
    if(n != strlen(shown) || strcmp(shown, want) != 0) {
        printf("  %.17g mm: \"%s\" (%u), wanted \"%s\"\n", mm, shown, n, want);
        return false;
    }
    return true;
}
static b8 says_angle(f64 d, u8 decimals, const c8* want) {
    const u32 n = fude_zoom_units_format_angle(d, decimals, shown, sizeof shown);
    if(n != strlen(shown) || strcmp(shown, want) != 0) {
        printf("  %.17g°: \"%s\", wanted \"%s\"\n", d, shown, want);
        return false;
    }
    return true;
}
#define SAYS(mm, u, d, den, fi, want) CHECK(says(mm, FUDE_ZOOM_UNIT_##u, d, den, fi, want))
#define SAYS_ANGLE(deg, d, want)     CHECK(says_angle(deg, d, want))

// ----------------------------------------------------------------------------------------------

static void test_unit_table(void) {
    CHECK(fude_zoom_unit_mm(FUDE_ZOOM_UNIT_MM) == 1.0);
    CHECK(fude_zoom_unit_mm(FUDE_ZOOM_UNIT_CM) == 10.0);
    CHECK(fude_zoom_unit_mm(FUDE_ZOOM_UNIT_M) == 1000.0);
    CHECK(fude_zoom_unit_mm(FUDE_ZOOM_UNIT_IN) == 25.4);
    CHECK(fude_zoom_unit_mm(FUDE_ZOOM_UNIT_FT) == 304.8);
    CHECK(fude_zoom_unit_mm(FUDE_ZOOM_UNIT_COUNT) == 0.0);
    CHECK(strcmp(fude_zoom_unit_suffix(FUDE_ZOOM_UNIT_MM), "mm") == 0);
    CHECK(strcmp(fude_zoom_unit_suffix(FUDE_ZOOM_UNIT_CM), "cm") == 0);
    CHECK(strcmp(fude_zoom_unit_suffix(FUDE_ZOOM_UNIT_M), "m") == 0);
    CHECK(strcmp(fude_zoom_unit_suffix(FUDE_ZOOM_UNIT_IN), "\"") == 0);
    CHECK(strcmp(fude_zoom_unit_suffix(FUDE_ZOOM_UNIT_FT), "'") == 0);
    CHECK(strcmp(fude_zoom_unit_suffix(FUDE_ZOOM_UNIT_COUNT), "") == 0);
}

static void test_numbers(void) {
    LEN("450", MM, 450.0);
    LEN("17.5", MM, 17.5);
    LEN("17,5", MM, 17.5);
    LEN(".5", MM, 0.5);
    LEN(",5", MM, 0.5);
    LEN("5.", MM, 5.0);
    LEN("0.005", MM, 0.005);
    LEN("007", MM, 7.0);
    LEN("1,200", MM, 1.2);   // no thousands separator: a comma is the point
    LEN("12345678901234567890123", MM, 1.2345678901234568e22);
    // Read exactly: the double nearest the decimal, as strtod gives it.
    CHECK(len("9.525", FUDE_ZOOM_UNIT_MM) == 9.525);
    CHECK(len("0.1", FUDE_ZOOM_UNIT_MM) == 0.1);
    CHECK(len("123456.789", FUDE_ZOOM_UNIT_MM) == 123456.789);
    CHECK(len("0,0001", FUDE_ZOOM_UNIT_MM) == 0.0001);
    CHECK(len("3/8\"", FUDE_ZOOM_UNIT_MM) == 3.0 / 8.0 * 25.4);
    LEN_ERR("1.2.3", MM, SYNTAX, 3);
    LEN_ERR("1,2,3", MM, SYNTAX, 3);
}

static void test_units(void) {
    LEN("2mm", MM, 2.0);
    LEN("2cm", MM, 20.0);
    LEN("2m", MM, 2000.0);
    LEN("2in", MM, 50.8);
    LEN("2\"", MM, 50.8);
    LEN("2\xE2\x80\xB3", MM, 50.8);   // ″
    LEN("2''", MM, 50.8);
    LEN("2ft", MM, 609.6);
    LEN("2'", MM, 609.6);
    LEN("2\xE2\x80\xB2", MM, 609.6);   // ′
    LEN("2MM", MM, 2.0);
    LEN("2Cm", MM, 20.0);
    LEN("2M", MM, 2000.0);
    LEN("2IN", MM, 50.8);
    LEN("2Ft", MM, 609.6);
    LEN("450 mm", MM, 450.0);
    LEN("2 m", MM, 2000.0);
    LEN("2\xC2\xA0mm", MM, 2.0);   // a no-break space
    LEN("2\"", CM, 50.8);
    // A bare number in the field's unit.
    LEN("2", MM, 2.0);
    LEN("2", CM, 20.0);
    LEN("2", M, 2000.0);
    LEN("2", IN, 50.8);
    LEN("2", FT, 609.6);
    LEN("1.5", M, 1500.0);
}

static void test_fractions(void) {
    LEN("3/8\"", MM, INCH(0.375));
    LEN("1-3/8\"", MM, INCH(1.375));
    LEN("1 3/8\"", MM, INCH(1.375));
    LEN("1   3/8\"", MM, INCH(1.375));
    LEN("1-3/8", IN, INCH(1.375));
    // A tight hyphen and a proper fraction: a mixed number. Spaced: a subtraction.
    LEN("10-1/2", IN, INCH(10.5));
    LEN("10 - 1/2", IN, INCH(9.5));
    LEN("10 -1/2", IN, INCH(9.5));
    LEN("10- 1/2", IN, INCH(9.5));
    LEN("450-2*18", MM, 414.0);   // 2*18 is no fraction
    // A product next: as written. The unit mark, or brackets, keep it mixed.
    LEN("1-3/8*2", IN, INCH(1.0 - 0.75));
    LEN("1-3/8/2", IN, INCH(1.0 - 0.1875));
    LEN("1-3/8 \xC3\x97 2", IN, INCH(1.0 - 0.75));
    LEN("(1-3/8)*2", IN, INCH(2.75));
    LEN("1-3/8\"*2", MM, INCH(2.75));
    LEN("1 3/8*2", IN, INCH(2.75));   // a space has no other reading
    // Not proper, not whole: no mixed number.
    LEN("1-9/8", IN, INCH(-0.125));
    LEN("1-8/8", IN, 0.0);
    LEN("1.5-1/2", IN, INCH(1.0));
    LEN("5+1-3/8", IN, INCH(6.375));
    LEN("2-1-3/8", IN, INCH(0.625));
    LEN("-1-3/8\"", MM, INCH(-1.375));
    LEN("1-3/8 + 1-3/8", IN, INCH(2.75));
    LEN("1 1/2-1/4", IN, INCH(1.25));
    // A quotient: tight, any decimals, one number before a unit.
    LEN("1200/3", MM, 400.0);
    LEN("1.5/2", MM, 0.75);
    LEN("1,5/2", MM, 0.75);
    LEN("10/2mm", MM, 5.0);
    LEN("1/8*360", MM, 45.0);
    LEN("12/2/3", MM, 2.0);
    LEN("2*3/8\"", MM, INCH(0.75));
    LEN_ERR("3 / 8\"", MM, DIMENSION, 2);   // spaced: 3 over 8 inches
    LEN_ERR("2 9/8", IN, SYNTAX, 2);
    LEN_ERR("3/0", MM, ZERO, 1);
    LEN_ERR("1-3/0", MM, ZERO, 3);
}

static void test_feet_inches(void) {
    LEN("1' 6\"", MM, INCH(18.0));
    LEN("1'6", MM, INCH(18.0));
    LEN("1'6\"", MM, INCH(18.0));
    LEN("1' 6-3/8\"", MM, INCH(18.375));
    LEN("1' 6 3/8\"", MM, INCH(18.375));
    LEN("1' 1/4\"", MM, INCH(12.25));
    LEN("2ft 3in", MM, INCH(27.0));
    LEN("2FT3IN", MM, INCH(27.0));
    LEN("1ft 6", MM, INCH(18.0));
    LEN("1\xE2\x80\xB2" "6\xE2\x80\xB3", MM, INCH(18.0));   // 1′6″
    LEN("1'6''", MM, INCH(18.0));
    LEN("6''", MM, INCH(6.0));
    LEN("1' 6", IN, INCH(18.0));
    LEN("1' 6", FT, INCH(18.0));   // the second is inches whatever the field's unit
    LEN("-1' 6\"", MM, INCH(-18.0));
    LEN("1' 6\" + 1' 6\"", MM, INCH(36.0));
    LEN("2*1' 6\"", MM, INCH(36.0));
    // The drawing office's hyphen: tight joins, spaced subtracts.
    LEN("1'-6\"", MM, INCH(18.0));
    LEN("2'-3-1/2\"", MM, INCH(27.5));
    LEN("2ft-3in", MM, INCH(27.0));
    LEN("1' - 6\"", MM, INCH(6.0));
    LEN("1' -6\"", MM, INCH(6.0));
    LEN("2'-6*2", MM, 609.6 - 12.0);
    LEN("2'-6mm", MM, 609.6 - 6.0);
    LEN_ERR("1' 6mm", MM, UNIT, 4);
    LEN_ERR("1' 6'", MM, UNIT, 4);
    LEN_ERR("1' 6\" 3\"", MM, SYNTAX, 6);
    LEN_ERR("(1+1)' 6", MM, SYNTAX, 7);   // feet and inches are numbers, not sums
}

static void test_arithmetic(void) {
    LEN("2+3*4", MM, 14.0);
    LEN("(2+3)*4", MM, 20.0);
    LEN("2*(3+4)", MM, 14.0);
    LEN("2*3+4*5", MM, 26.0);
    LEN("-2*3", MM, -6.0);
    LEN("2*-3", MM, -6.0);
    LEN("--2", MM, 2.0);
    LEN("+5", MM, 5.0);
    LEN("2+-3", MM, -1.0);
    LEN("-(2+3)", MM, -5.0);
    LEN("10-2-3", MM, 5.0);
    LEN("1-2-3", MM, -4.0);
    LEN("((2))", MM, 2.0);
    LEN("2 * (3 - 1) / 4", MM, 1.0);
    LEN("2\xC3\x97" "3", MM, 6.0);   // ×
    LEN("6\xC3\xB7" "2", MM, 3.0);   // ÷
    LEN("5\xE2\x88\x92" "2", MM, 3.0);   // −
    LEN("\xE2\x88\x92" "5", MM, -5.0);
    LEN("  2 +\t3  ", MM, 5.0);
    LEN("\xE2\x89\x88 17.46 mm", MM, 17.46);   // as format writes it
}

static void test_kinds(void) {
    LEN("2*18mm", MM, 36.0);
    LEN("18mm*2", MM, 36.0);
    LEN("18mm/2", MM, 9.0);
    LEN("450mm-18", MM, 432.0);
    LEN("100mm+1\"", MM, 125.4);
    LEN("(1200mm/3)", MM, 400.0);
    LEN("1200mm/3mm", MM, 400.0);   // plain: in the field's unit
    LEN("1200mm/3mm", CM, 4000.0);
    LEN("(2+3)mm", MM, 5.0);
    LEN("(1+2)\"", MM, INCH(3.0));
    LEN("2\"*3", MM, INCH(6.0));
    LEN("1\"+1", IN, INCH(2.0));
    LEN("25.4mm+1", IN, INCH(2.0));
    LEN("450mm-18", IN, 450.0 - INCH(18.0));
    LEN("1m-1", CM, 990.0);
    LEN_ERR("10mm*2mm", MM, DIMENSION, 4);
    LEN_ERR("2 / 4mm", MM, DIMENSION, 2);
    LEN_ERR("1/(2mm)", MM, DIMENSION, 1);
    LEN_ERR("(2mm)cm", MM, UNIT, 5);
    LEN_ERR("2mm mm", MM, UNIT, 4);
    LEN_ERR("2mm\"", MM, UNIT, 3);
    LEN_ERR("2mm/0", MM, ZERO, 3);
    LEN_ERR("2/(1-1)", MM, ZERO, 1);
    LEN_ERR("2\xC3\xB7" "0", MM, ZERO, 1);
}

static void test_errors(void) {
    LEN_ERR("", MM, EMPTY, 0);
    LEN_ERR("   ", MM, EMPTY, 0);
    LEN_ERR(NULL, MM, EMPTY, 0);
    LEN_ERR("\xE2\x89\x88 ", MM, EMPTY, 0);
    LEN_ERR("2+", MM, SYNTAX, 2);
    LEN_ERR("2 3", MM, SYNTAX, 2);
    LEN_ERR("2)", MM, SYNTAX, 1);
    LEN_ERR("*2", MM, SYNTAX, 0);
    LEN_ERR("2*", MM, SYNTAX, 2);
    LEN_ERR("()", MM, SYNTAX, 1);
    LEN_ERR("2#", MM, SYNTAX, 1);
    LEN_ERR("(2 3)", MM, SYNTAX, 3);
    LEN_ERR("(2+3", MM, PAREN, 0);
    LEN_ERR("2*(3+(4)", MM, PAREN, 2);
    LEN_ERR("2\xC3\x97(3", MM, PAREN, 3);   // byte offsets: × is two bytes
    LEN_ERR("mm", MM, UNIT, 0);
    LEN_ERR("\"", MM, UNIT, 0);
    LEN_ERR("2 mn", MM, UNIT, 2);
    LEN_ERR("2mm3", MM, SYNTAX, 3);
    LEN_ERR("2mmx", MM, UNIT, 1);
    LEN_ERR("1e3", MM, UNIT, 1);
    LEN_ERR("2\xC2\xB0", MM, UNIT, 1);   // degrees in a length
    LEN_ERR("x", MM, NAME, 0);
    LEN_ERR("2*abc", MM, NAME, 2);
    LEN_ERR("\xE2\x88\x92" "5+x", MM, NAME, 5);
    // Too big, too deep.
    c8 big[512];
    memset(big, '0', sizeof big);
    big[0] = '1';
    big[400] = 0;
    LEN_ERR(big, MM, RANGE, 0);
    big[200] = '*';
    big[201] = '1';
    LEN_ERR(big, MM, RANGE, 200);
    c8 deep[128];
    memset(deep, '(', 70);
    strcpy(deep + 70, "1");
    LEN_ERR(deep, MM, RANGE, 64);
    memset(deep, '-', 70);
    LEN_ERR(deep, MM, RANGE, 64);
    memset(deep, '(', 60);
    strcpy(deep + 60, "1");
    memset(deep + 61, ')', 60);
    deep[121] = 0;
    LEN(deep, MM, 1.0);
    // No variables at all, no error wanted, and success says so.
    f64 mm = 0.0;
    fude_zoom_units_error e = { FUDE_ZOOM_UNITS_ERROR_SYNTAX, 7u };
    CHECK(!fude_zoom_units_length("stock", FUDE_ZOOM_UNIT_MM, NULL, &mm, &e) && e.code == FUDE_ZOOM_UNITS_ERROR_NAME && e.at == 0u);
    CHECK(fude_zoom_units_length("2+2", FUDE_ZOOM_UNIT_MM, NULL, &mm, NULL) && mm == 4.0);
    CHECK(!fude_zoom_units_length("2+", FUDE_ZOOM_UNIT_MM, NULL, &mm, NULL) && mm == 4.0);
    CHECK(fude_zoom_units_length("3", FUDE_ZOOM_UNIT_MM, NULL, &mm, &e) && e.code == FUDE_ZOOM_UNITS_OK && e.at == 0u);
}

static void test_variables(void) {
    fude_zoom_vars v;
    memset(&v, 0, sizeof v);
    CHECK(fude_zoom_vars_set(&v, "stock", 18.0, true));
    CHECK(fude_zoom_vars_set(&v, "kerf", 3.2, true));
    CHECK(fude_zoom_vars_set(&v, "n", 4.0, false));
    CHECK(v.count == 3u);
    const fude_zoom_var* s = fude_zoom_vars_find(&v, "STOCK");
    CHECK(s != NULL && s->value == 18.0 && s->length && strcmp(s->name, "stock") == 0);
    CHECK(fude_zoom_vars_find(&v, "Kerf") != NULL && !fude_zoom_vars_find(&v, "n")->length);
    CHECK(fude_zoom_vars_find(&v, "nope") == NULL);
    CHECK(fude_zoom_vars_find(&v, "stoc") == NULL);
    CHECK(fude_zoom_vars_find(&v, "stocks") == NULL);
    CHECK(fude_zoom_vars_find(&v, NULL) == NULL);
    CHECK(fude_zoom_vars_find(NULL, "stock") == NULL);
    // Names: [A-Za-z_][A-Za-z0-9_]*, up to 23 bytes, never a unit word.
    CHECK(!fude_zoom_vars_set(&v, "", 1.0, false));
    CHECK(!fude_zoom_vars_set(&v, NULL, 1.0, false));
    CHECK(!fude_zoom_vars_set(NULL, "a", 1.0, false));
    CHECK(!fude_zoom_vars_set(&v, "1abc", 1.0, false));
    CHECK(!fude_zoom_vars_set(&v, "a-b", 1.0, false));
    CHECK(!fude_zoom_vars_set(&v, "a b", 1.0, false));
    CHECK(!fude_zoom_vars_set(&v, "ca\xC3\xB1" "a", 1.0, false));
    CHECK(fude_zoom_vars_set(&v, "_bit_2", 6.35, true));
    CHECK(fude_zoom_vars_set(&v, "abcdefghijklmnopqrstuvw", 1.0, false));   // 23
    CHECK(!fude_zoom_vars_set(&v, "abcdefghijklmnopqrstuvwx", 1.0, false));   // 24
    CHECK(!fude_zoom_vars_set(&v, "mm", 1.0, false));
    CHECK(!fude_zoom_vars_set(&v, "MM", 1.0, false));
    CHECK(!fude_zoom_vars_set(&v, "In", 1.0, false));
    CHECK(!fude_zoom_vars_set(&v, "ft", 1.0, false));
    CHECK(!fude_zoom_vars_set(&v, "m", 1.0, false));
    CHECK(!fude_zoom_vars_set(&v, "cm", 1.0, false));
    CHECK(fude_zoom_vars_set(&v, "mmm", 1.0, false));
    CHECK(!fude_zoom_vars_set(&v, "x", NAN, false));
    CHECK(!fude_zoom_vars_set(&v, "x", INFINITY, true));
    CHECK(v.count == 6u);
    // The same name in another case replaces it, spelling and all.
    CHECK(fude_zoom_vars_set(&v, "Stock", 19.0, true));
    CHECK(v.count == 6u && fude_zoom_vars_find(&v, "stock")->value == 19.0 && strcmp(fude_zoom_vars_find(&v, "stock")->name, "Stock") == 0);
    CHECK(fude_zoom_vars_set(&v, "n", 2.0, true) && fude_zoom_vars_find(&v, "n")->length);
    // Full at 32; replacing still works.
    c8 name[8];
    for(u32 i = 0; v.count < FUDE_ZOOM_VARS_MOST; i++) {
        snprintf(name, sizeof name, "v%u", i);
        CHECK(fude_zoom_vars_set(&v, name, (f64)i, false));
    }
    CHECK(!fude_zoom_vars_set(&v, "extra", 1.0, false));
    CHECK(fude_zoom_vars_set(&v, "kerf", 3.0, true) && v.count == FUDE_ZOOM_VARS_MOST);
    // Taken out: the rest keep their order.
    CHECK(fude_zoom_vars_remove(&v, "KERF"));
    CHECK(v.count == FUDE_ZOOM_VARS_MOST - 1u && fude_zoom_vars_find(&v, "kerf") == NULL);
    CHECK(strcmp(v.vars[0].name, "Stock") == 0 && strcmp(v.vars[1].name, "n") == 0 && strcmp(v.vars[2].name, "_bit_2") == 0);
    CHECK(!fude_zoom_vars_remove(&v, "kerf"));
    CHECK(fude_zoom_vars_set(&v, "extra", 1.0, false));

    // In fields (the shared table: stock 18 mm, kerf 3.2 mm, bit 6.35 mm, n 4, half 0.5).
    LEN("stock", MM, 18.0);
    LEN("STOCK", MM, 18.0);
    LEN("(stock)", MM, 18.0);
    LEN("2*stock+kerf", MM, 39.2);
    LEN("n*stock", MM, 72.0);
    LEN("450-2*stock", MM, 414.0);
    LEN("stock-2*kerf", MM, 11.6);
    LEN("stock/2", MM, 9.0);
    LEN("stock/kerf", MM, 5.625);   // plain: in the field's unit
    LEN("n", IN, INCH(4.0));
    LEN("n+stock", IN, INCH(4.0) + 18.0);
    LEN("bit+1/4\"", MM, 12.7);
    LEN("(n)mm", MM, 4.0);
    LEN("half*1\"", MM, 12.7);
    LEN_ERR("stock*stock", MM, DIMENSION, 5);
    LEN_ERR("n/stock", MM, DIMENSION, 1);
    LEN_ERR("stock mm", MM, UNIT, 6);
    LEN_ERR("stock\"", MM, UNIT, 5);
    LEN_ERR("n mm", MM, UNIT, 2);
    LEN_ERR("stock2", MM, NAME, 0);
    LEN_ERR("stock kerf", MM, SYNTAX, 6);
    LEN_ERR("2stock", MM, UNIT, 1);
}

static void test_angles(void) {
    ANG("45", 45.0);
    ANG("45\xC2\xB0", 45.0);   // °
    ANG("45\xC2\xBA", 45.0);   // º, a Spanish keyboard's
    ANG("90-30", 60.0);
    ANG("22.5", 22.5);
    ANG("22,5", 22.5);
    ANG("1/8*360", 45.0);
    ANG("(90-30)\xC2\xB0", 60.0);
    ANG("45\xC2\xB0*2", 90.0);
    ANG("90\xC2\xB0/45\xC2\xB0", 2.0);
    ANG("90\xC2\xB0 - 30", 60.0);
    ANG("half*90", 45.0);
    ANG("n*22.5", 90.0);
    ANG("\xE2\x88\x92" "45\xC2\xB0", -45.0);
    ANG("\xE2\x89\x88 33.7\xC2\xB0", 33.7);
    ANG("10-1/2", 10.5);
    ANG_ERR("45\xC2\xB0*2\xC2\xB0", DIMENSION, 4);
    ANG_ERR("2 / 45\xC2\xB0", DIMENSION, 2);
    ANG("2/45\xC2\xB0", 2.0 / 45.0);   // tight: one number, then the unit
    ANG_ERR("45mm", UNIT, 2);
    ANG_ERR("45\"", UNIT, 2);
    ANG_ERR("45'", UNIT, 2);
    ANG_ERR("45\xC2\xB0\xC2\xB0", UNIT, 4);
    ANG_ERR("stock", DIMENSION, 0);
    ANG_ERR("90-stock", DIMENSION, 3);
    ANG_ERR("", EMPTY, 0);
    ANG_ERR("45/0", ZERO, 2);
}

static void test_plain_numbers(void) {
    NUM("4", 4.0);
    NUM("2*3+1", 7.0);
    NUM("n+1", 5.0);
    NUM("1-1/2", 1.5);
    NUM("1/3", 1.0 / 3.0);
    NUM("12,5", 12.5);
    NUM_ERR("4mm", UNIT, 1);
    NUM_ERR("4\"", UNIT, 1);
    NUM_ERR("4\xC2\xB0", UNIT, 1);
    NUM_ERR("stock", DIMENSION, 0);
    NUM_ERR("2*kerf", DIMENSION, 2);
    NUM_ERR("x+1", NAME, 0);
}

static void test_format_metric(void) {
    SAYS(450.0, MM, 2, 0, false, "450 mm");
    SAYS(17.5, MM, 2, 0, false, "17.5 mm");   // trailing zeros dropped
    SAYS(17.456, MM, 2, 0, false, "\xE2\x89\x88 17.46 mm");
    SAYS(17.454, MM, 2, 0, false, "\xE2\x89\x88 17.45 mm");
    SAYS(17.5, MM, 0, 0, false, "\xE2\x89\x88 18 mm");
    SAYS(17.0, MM, 0, 0, false, "17 mm");
    SAYS(-12.0, MM, 2, 0, false, "-12 mm");
    SAYS(-12.25, MM, 1, 0, false, "\xE2\x89\x88 -12.3 mm");
    SAYS(-0.001, MM, 2, 0, false, "\xE2\x89\x88 0 mm");   // no "-0"
    SAYS(0.0, MM, 2, 0, false, "0 mm");
    SAYS(-0.0, MM, 2, 0, false, "0 mm");
    SAYS(0.05, MM, 2, 0, false, "0.05 mm");
    SAYS(9.525, MM, 3, 0, false, "9.525 mm");
    SAYS(9.525, MM, 1, 0, false, "\xE2\x89\x88 9.5 mm");
    SAYS(0.1 + 0.2, MM, 4, 0, false, "0.3 mm");   // arithmetic's last bits are no rounding
    SAYS(125.0, CM, 1, 0, false, "12.5 cm");
    SAYS(5.0, CM, 0, 0, false, "\xE2\x89\x88 1 cm");
    SAYS(1234.5, M, 4, 0, false, "1.2345 m");
    SAYS(1234.56, M, 3, 0, false, "\xE2\x89\x88 1.235 m");
    SAYS(1000.0, M, 2, 0, false, "1 m");
    SAYS(1.23456789, MM, 9, 0, false, "\xE2\x89\x88 1.234568 mm");   // at most 6 decimals
    SAYS(450.0, MM, 2, 16, true, "450 mm");   // fractions and feet are for inches only
    CHECK(says(450.0, (FUDE_ZOOM_UNIT_)77, 2, 0, false, "450 mm"));   // no such unit: millimetres
}

static void test_format_inches(void) {
    SAYS(INCH(1.375), IN, 0, 8, false, "1-3/8\"");
    SAYS(9.525, IN, 0, 8, false, "3/8\"");   // typed exactly: no ≈
    SAYS(9.525, IN, 0, 64, false, "3/8\"");   // 24/64 reduced
    SAYS(INCH(1.375), IN, 0, 16, false, "1-3/8\"");
    SAYS(INCH(1.375), IN, 0, 32, false, "1-3/8\"");
    SAYS(INCH(1.375), IN, 0, 64, false, "1-3/8\"");
    SAYS(INCH(1.36), IN, 0, 64, false, "\xE2\x89\x88 1-23/64\"");
    SAYS(INCH(1.36), IN, 0, 32, false, "\xE2\x89\x88 1-3/8\"");   // 44/32
    SAYS(INCH(1.34), IN, 0, 32, false, "\xE2\x89\x88 1-11/32\"");
    SAYS(INCH(1.36), IN, 0, 16, false, "\xE2\x89\x88 1-3/8\"");
    SAYS(INCH(1.36), IN, 0, 8, false, "\xE2\x89\x88 1-3/8\"");
    SAYS(INCH(1.36), IN, 0, 4, false, "\xE2\x89\x88 1-1/4\"");
    SAYS(INCH(1.375), IN, 0, 2, false, "\xE2\x89\x88 1-1/2\"");
    SAYS(INCH(1.375), IN, 0, 1, false, "\xE2\x89\x88 1\"");
    SAYS(INCH(0.5), IN, 0, 2, false, "1/2\"");
    SAYS(INCH(0.999), IN, 0, 16, false, "\xE2\x89\x88 1\"");   // up to the next whole
    SAYS(INCH(2.999), IN, 0, 16, false, "\xE2\x89\x88 3\"");
    SAYS(INCH(-1.375), IN, 0, 8, false, "-1-3/8\"");
    SAYS(INCH(-0.375), IN, 0, 8, false, "-3/8\"");
    SAYS(INCH(-1.36), IN, 0, 64, false, "\xE2\x89\x88 -1-23/64\"");
    SAYS(INCH(-0.01), IN, 0, 16, false, "\xE2\x89\x88 0\"");
    SAYS(INCH(2.0), IN, 0, 16, false, "2\"");
    SAYS(0.0, IN, 0, 16, false, "0\"");
    SAYS(INCH(18.375), IN, 0, 8, false, "18-3/8\"");
    // Decimal inches.
    SAYS(INCH(1.375), IN, 3, 0, false, "1.375\"");
    SAYS(INCH(1.376), IN, 2, 0, false, "\xE2\x89\x88 1.38\"");
    SAYS(INCH(1.5), IN, 2, 0, false, "1.5\"");
    // A denominator that is no power of two: the one below it, 64 at most.
    SAYS(INCH(1.375), IN, 0, 10, false, "1-3/8\"");
    SAYS(INCH(1.36), IN, 0, 100, false, "\xE2\x89\x88 1-23/64\"");
    SAYS(INCH(1.375), IN, 0, 3, false, "\xE2\x89\x88 1-1/2\"");
}

static void test_format_feet(void) {
    SAYS(INCH(27.5), IN, 0, 2, true, "2' 3-1/2\"");
    SAYS(INCH(18.375), IN, 0, 8, true, "1' 6-3/8\"");
    SAYS(INCH(18.375), FT, 0, 8, true, "1' 6-3/8\"");
    SAYS(INCH(6.375), IN, 0, 8, true, "6-3/8\"");   // under a foot: just inches
    SAYS(INCH(24.0), IN, 0, 8, true, "2'");   // whole feet: just feet
    SAYS(INCH(12.25), IN, 0, 4, true, "1' 1/4\"");
    SAYS(INCH(11.999), IN, 0, 16, true, "\xE2\x89\x88 1'");   // never 0' 12"
    SAYS(INCH(23.99), IN, 0, 16, true, "\xE2\x89\x88 2'");
    SAYS(INCH(-27.5), IN, 0, 2, true, "-2' 3-1/2\"");
    SAYS(INCH(-6.0), IN, 0, 2, true, "-6\"");
    SAYS(INCH(18.5), IN, 1, 0, true, "1' 6.5\"");
    SAYS(INCH(18.55), IN, 1, 0, true, "\xE2\x89\x88 1' 6.6\"");
    SAYS(INCH(12.0 * 1234 + 5.0), IN, 0, 16, true, "1234' 5\"");
    // FT without feet and inches: decimal feet, no fractions.
    SAYS(457.2, FT, 2, 0, false, "1.5'");
    SAYS(457.2, FT, 2, 16, false, "1.5'");
    SAYS(INCH(18.375), FT, 2, 0, false, "\xE2\x89\x88 1.53'");
    SAYS(-304.8, FT, 2, 0, false, "-1'");
}

static void test_format_angle(void) {
    SAYS_ANGLE(45.0, 1, "45\xC2\xB0");
    SAYS_ANGLE(33.69, 1, "\xE2\x89\x88 33.7\xC2\xB0");
    SAYS_ANGLE(-12.5, 1, "-12.5\xC2\xB0");
    SAYS_ANGLE(90.0000000001, 2, "90\xC2\xB0");
    SAYS_ANGLE(22.5, 0, "\xE2\x89\x88 23\xC2\xB0");
    SAYS_ANGLE(0.0, 2, "0\xC2\xB0");
    SAYS_ANGLE(-0.001, 1, "\xE2\x89\x88 0\xC2\xB0");
    SAYS_ANGLE(1.0 / 3.0, 9, "\xE2\x89\x88 0.333333\xC2\xB0");
    CHECK(fude_zoom_units_format_angle(NAN, 1, shown, sizeof shown) == 0u && shown[0] == 0);
}

static void test_format_buffer(void) {
    const fude_zoom_units_style st = { FUDE_ZOOM_UNIT_MM, 2, 0, false };
    c8 small[16];
    // Nothing for nothing.
    CHECK(fude_zoom_units_format(17.456, &st, NULL, 0) == 0u);
    CHECK(fude_zoom_units_format(NAN, &st, small, sizeof small) == 0u && small[0] == 0);
    CHECK(fude_zoom_units_format(INFINITY, &st, small, sizeof small) == 0u && small[0] == 0);
    CHECK(fude_zoom_units_format(1.0, NULL, small, sizeof small) == 0u && small[0] == 0);
    // Short: a token at a time, never half a character, always NUL-terminated.
    memset(small, 'x', sizeof small);
    CHECK(fude_zoom_units_format(17.456, &st, small, 1) == 0u && small[0] == 0);
    CHECK(fude_zoom_units_format(17.456, &st, small, 4) == 0u && small[0] == 0);
    CHECK(fude_zoom_units_format(17.456, &st, small, 5) == 4u && strcmp(small, "\xE2\x89\x88 ") == 0);
    CHECK(fude_zoom_units_format(17.456, &st, small, 7) == 6u && strcmp(small, "\xE2\x89\x88 17") == 0);
    CHECK(fude_zoom_units_format(17.456, &st, small, 13) == 12u && strcmp(small, "\xE2\x89\x88 17.46 mm") == 0);
    CHECK(fude_zoom_units_format(17.456, &st, small, 12) == 10u && strcmp(small, "\xE2\x89\x88 17.46 ") == 0);
    // FUDE_ZOOM_UNITS_TEXT holds anything, however big or deep.
    c8 out[FUDE_ZOOM_UNITS_TEXT];
    u32 longest = 0;
    for(u32 u = 0; u < FUDE_ZOOM_UNIT_COUNT; u++) {
        for(i32 e = -12; e <= 307; e += 1) {
            for(u32 k = 0; k < 4; k++) {
                const fude_zoom_units_style s = { (FUDE_ZOOM_UNIT_)u, (u8)(k * 2), (u8)(k == 3 ? 0 : 64 >> k), (b8)(k & 1) };
                const f64 mm = -pow(10.0, (f64)e) * (1.0 + rndf() * 8.0) * (e == 307 ? 0.1 : 1.0);
                const u32 n = fude_zoom_units_format(mm, &s, out, sizeof out);
                CHECK(n == strlen(out) && n > 0u && n < FUDE_ZOOM_UNITS_TEXT);
                longest = n > longest ? n : longest;
            }
        }
    }
    SAYS(1e300, MM, 2, 0, false, "\xE2\x89\x88 1.000e+300 mm");
    SAYS(-1e300, IN, 0, 16, true, "\xE2\x89\x88 -3.937e+298\"");
    SAYS(1e15, MM, 6, 0, false, "1000000000000000 mm");   // fewer decimals, never a wrong figure
    SAYS_ANGLE(1e200, 2, "\xE2\x89\x88 1.000e+200\xC2\xB0");
    printf("  format: longest text %u bytes of %u\n", longest, FUDE_ZOOM_UNITS_TEXT);
}

// Said, then typed back as said: the value shown, and said the same again
// without the ≈ — every style, every size, both signs.
static void test_round_trips(void) {
    const fude_zoom_units_style styles[] = {
        { FUDE_ZOOM_UNIT_MM, 0, 0, false }, { FUDE_ZOOM_UNIT_MM, 2, 0, false }, { FUDE_ZOOM_UNIT_MM, 4, 0, false },
        { FUDE_ZOOM_UNIT_CM, 1, 0, false }, { FUDE_ZOOM_UNIT_CM, 3, 0, false }, { FUDE_ZOOM_UNIT_M, 4, 0, false },
        { FUDE_ZOOM_UNIT_M, 0, 0, false },  { FUDE_ZOOM_UNIT_IN, 0, 1, false }, { FUDE_ZOOM_UNIT_IN, 0, 2, false },
        { FUDE_ZOOM_UNIT_IN, 0, 8, false }, { FUDE_ZOOM_UNIT_IN, 0, 16, false }, { FUDE_ZOOM_UNIT_IN, 0, 64, false },
        { FUDE_ZOOM_UNIT_IN, 3, 0, false }, { FUDE_ZOOM_UNIT_IN, 0, 16, true }, { FUDE_ZOOM_UNIT_IN, 0, 64, true },
        { FUDE_ZOOM_UNIT_IN, 2, 0, true },  { FUDE_ZOOM_UNIT_FT, 0, 32, true }, { FUDE_ZOOM_UNIT_FT, 2, 0, false },
        { FUDE_ZOOM_UNIT_FT, 0, 4, true },
    };
    const u32 nstyles = (u32)(sizeof styles / sizeof styles[0]);
    u32 tried = 0, rounded = 0, bad_parse = 0, bad_again = 0, bad_exact = 0, bad_step = 0;
    for(u32 i = 0; i < 4000; i++) {
        const fude_zoom_units_style* st = &styles[i % nstyles];
        // Log-uniform from a hundredth of a mm to 100 m, a third of them on a grid of the style's.
        f64 mm = pow(10.0, -2.0 + rndf() * 7.0) * (rnd() & 1u ? -1.0 : 1.0);
        const b8 imperial = st->unit == FUDE_ZOOM_UNIT_IN || st->unit == FUDE_ZOOM_UNIT_FT;
        const b8 feet     = st->unit == FUDE_ZOOM_UNIT_FT && !st->feet_inches;
        const f64 per     = feet ? 304.8 : imperial ? 25.4 : fude_zoom_unit_mm(st->unit);
        const f64 q       = imperial && !feet && st->denominator > 0 ? (f64)st->denominator : pow(10.0, st->decimals);
        if(i % 3u == 0u) {
            mm = round(mm / per * q) / q * per;
        }
        c8 a[FUDE_ZOOM_UNITS_TEXT], b[FUDE_ZOOM_UNITS_TEXT];
        fude_zoom_units_format(mm, st, a, sizeof a);
        const b8 approx = strncmp(a, "\xE2\x89\x88 ", 4) == 0;
        rounded += approx;
        tried++;
        f64 back = NAN;
        fude_zoom_units_error e;
        if(!fude_zoom_units_length(a, st->unit, NULL, &back, &e)) {
            if(bad_parse++ < 5) printf("  \"%s\" did not read back: error %d at %u\n", a, (int)e.code, e.at);
            continue;
        }
        fude_zoom_units_format(back, st, b, sizeof b);
        if(strcmp(b, approx ? a + 4 : a) != 0) {
            if(bad_again++ < 5) printf("  \"%s\" read back said \"%s\"\n", a, b);
        }
        if(!approx && !near(back, mm)) {
            if(bad_exact++ < 5) printf("  \"%s\" (no ≈) read back %.17g, was %.17g\n", a, back, mm);
        }
        if(fabs(back - mm) > 0.5 * per / q * (1.0 + 1e-9) + 1e-9 * fabs(mm)) {
            if(bad_step++ < 5) printf("  \"%s\" read back %.17g, more than half a step from %.17g\n", a, back, mm);
        }
    }
    CHECK(bad_parse == 0u);
    CHECK(bad_again == 0u);
    CHECK(bad_exact == 0u);
    CHECK(bad_step == 0u);
    CHECK(rounded > tried / 4u && rounded < tried);   // both kinds were tried
    // Angles too.
    for(u32 i = 0; i < 500; i++) {
        const f64 d = (rndf() - 0.5) * 720.0;
        const u8  k = (u8)(i % 4u);
        c8 a[FUDE_ZOOM_UNITS_TEXT], b[FUDE_ZOOM_UNITS_TEXT];
        fude_zoom_units_format_angle(d, k, a, sizeof a);
        f64 back = NAN;
        CHECK(fude_zoom_units_angle(a, NULL, &back, NULL));
        fude_zoom_units_format_angle(back, k, b, sizeof b);
        CHECK(strcmp(b, strncmp(a, "\xE2\x89\x88 ", 4) == 0 ? a + 4 : a) == 0);
        CHECK(fabs(back - d) <= 0.5 * pow(10.0, -k) + 1e-9);
    }
    // Typed exactly, said exactly: what is typed is stored, not rounded.
    const c8* typed[] = { "3/8\"", "1-3/8\"", "2' 3-1/2\"", "17.5", "9,525", "450-2*18", "1' 6-3/8\"" };
    const fude_zoom_units_style fine[] = { { FUDE_ZOOM_UNIT_IN, 0, 64, true }, { FUDE_ZOOM_UNIT_MM, 3, 0, false } };
    for(u32 i = 0; i < sizeof typed / sizeof typed[0]; i++) {
        f64 mm = NAN;
        CHECK(fude_zoom_units_length(typed[i], FUDE_ZOOM_UNIT_MM, NULL, &mm, NULL));
        c8 a[FUDE_ZOOM_UNITS_TEXT];
        fude_zoom_units_format(mm, &fine[i < 3 || i == 6 ? 0 : 1], a, sizeof a);
        CHECK(strncmp(a, "\xE2\x89\x88", 3) != 0);
    }
    printf("  round trips: %u lengths (%u rounded), 500 angles\n", tried, rounded);
}

int main(void) {
    memset(&vars, 0, sizeof vars);
    fude_zoom_vars_set(&vars, "stock", 18.0, true);
    fude_zoom_vars_set(&vars, "kerf", 3.2, true);
    fude_zoom_vars_set(&vars, "bit", 6.35, true);
    fude_zoom_vars_set(&vars, "n", 4.0, false);
    fude_zoom_vars_set(&vars, "half", 0.5, false);
    test_unit_table();
    test_numbers();
    test_units();
    test_fractions();
    test_feet_inches();
    test_arithmetic();
    test_kinds();
    test_errors();
    test_variables();
    test_angles();
    test_plain_numbers();
    test_format_metric();
    test_format_inches();
    test_format_feet();
    test_format_angle();
    test_format_buffer();
    test_round_trips();
    printf("  units: %d checks\n", checks);
    printf(fails ? "%d FAILURES\n" : "ALL PASSED\n", fails);
    return fails != 0;
}
