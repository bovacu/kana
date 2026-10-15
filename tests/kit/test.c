// The form kit (drawing/widgets/kit.h: fude_kit_form_*), which the canvas's cards (zoom's *form.c) are built on now.
// Its keypad's typing (fude_kit_form_type) took the place of the value card's and the limits card's own: this suite
// keeps both as they were, word for word, and types long random runs of keys into each and into the kit, which must
// leave the same text after every key: a number's first key replacing it, back, the point once (after a 0 when it
// comes first, or after a lone minus on the value card), the minus turning the sign, and the most characters each
// card takes.
#include "drawing/widgets/kit.h"
#include <stdio.h>
#include <string.h>

static int fails = 0;
#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while(0)

static u64 rng = 0x2545F4914F6CDD1Dull;
static u64 next_u64(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return rng; }

// The value card's keys as they were (valueform.c: 10 the point, 11 the minus, 12 back; 12 characters at most).
static void old_value_key(c8* _typed, usize _size, b8* _fresh, u32 _index) {
    usize _n = strlen(_typed);
    if(*_fresh && _index != 12u) {
        _typed[0] = 0;
        _n = 0;
    }
    *_fresh = false;
    if(_index == 12u) {
        if(_n > 0u) {
            _typed[_n - 1u] = 0;
        }
    } else if(_index == 11u) {
        if(_typed[0] == '-') {
            memmove(_typed, _typed + 1, _n);
        } else if(_n + 1u < _size) {
            memmove(_typed + 1, _typed, _n + 1u);
            _typed[0] = '-';
        }
    } else if(_index == 10u) {
        if(strchr(_typed, '.') == NULL && _n + 2u < _size) {
            if(_n == 0u || (_n == 1u && _typed[0] == '-')) {
                _typed[_n++] = '0';
            }
            _typed[_n++] = '.';
            _typed[_n] = 0;
        }
    } else if(_n + 1u < _size && _n < 12u) {
        _typed[_n] = (c8)('0' + _index);
        _typed[_n + 1u] = 0;
    }
}

// The limits card's keys as they were (limitsform.c: 10 the point, 11 back; 9 characters at most; no minus).
static void old_limits_key(c8* _typed, usize _size, b8* _fresh, u32 _index) {
    usize _n = strlen(_typed);
    if(*_fresh && _index != 11u) {
        _typed[0] = 0;
        _n = 0;
    }
    *_fresh = false;
    if(_index == 11u) {
        if(_n > 0u) {
            _typed[_n - 1u] = 0;
        }
    } else if(_index == 10u) {
        if(strchr(_typed, '.') == NULL && _n + 2u < _size) {
            if(_n == 0u) {
                _typed[_n++] = '0';
            }
            _typed[_n++] = '.';
            _typed[_n] = 0;
        }
    } else if(_n + 1u < _size && _n < 9u) {
        _typed[_n] = (c8)('0' + _index);
        _typed[_n + 1u] = 0;
    }
}

int main(void) {
    static const c8 value_keys[13]  = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '.', '-', '\b' };
    static const c8 limits_keys[12] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '.', '\b' };
    static const c8* const opened[] = { "", "4.7", "-12", "0.5", "123456789012", "-", "100" };
    u32 differ = 0, keys = 0;
    for(u32 run = 0; run < 20000u; run++) {
        // The value card: typed[24], opened with a number (fresh), then 1 to 40 keys, back and the minus more often.
        c8 a[24], b[24];
        b8 fa = true, fb = true;
        snprintf(a, sizeof(a), "%s", opened[run % 7u]);
        snprintf(b, sizeof(b), "%s", opened[run % 7u]);
        const u32 n = 1u + (u32)(next_u64() % 40u);
        for(u32 k = 0; k < n; k++) {
            const u64 r = next_u64() % 20u;
            const u32 index = r < 13u ? (u32)r : (r < 17u ? 12u : (r < 19u ? 11u : 10u));
            old_value_key(a, sizeof(a), &fa, index);
            fude_kit_form_type(b, sizeof(b), 12u, true, &fb, value_keys[index]);
            differ += strcmp(a, b) != 0 || fa != fb ? 1u : 0u;
            keys++;
        }
        // The limits card: typed[16], opened with its limit written out (fresh), then its keys.
        c8 c[16], d[16];
        b8 fc = run % 3u != 0u, fd = fc;
        snprintf(c, sizeof(c), "%s", opened[(run + 3u) % 7u][0] == '-' ? "0.25" : opened[(run + 3u) % 7u]);
        snprintf(d, sizeof(d), "%s", c);
        for(u32 k = 0; k < n; k++) {
            const u64 r = next_u64() % 16u;
            const u32 index = r < 12u ? (u32)r : (r < 14u ? 11u : 10u);
            old_limits_key(c, sizeof(c), &fc, index);
            fude_kit_form_type(d, sizeof(d), 9u, false, &fd, limits_keys[index]);
            differ += strcmp(c, d) != 0 || fc != fd ? 1u : 0u;
            keys++;
        }
    }
    CHECK(differ == 0u && keys > 400000u);

    // A few by hand: the first key replaces the number opened with, back edits it; a point first is "0."; the minus
    // turns the sign and back again; the value card stops at 12 characters, the limits card at 9.
    c8 t[24] = "4.7";
    b8 fresh = true;
    fude_kit_form_type(t, sizeof(t), 12u, true, &fresh, '\b');
    CHECK(strcmp(t, "4.") == 0 && !fresh);
    snprintf(t, sizeof(t), "4.7");
    fresh = true;
    fude_kit_form_type(t, sizeof(t), 12u, true, &fresh, '.');
    CHECK(strcmp(t, "0.") == 0);
    fude_kit_form_type(t, sizeof(t), 12u, true, &fresh, '-');
    CHECK(strcmp(t, "-0.") == 0);
    fude_kit_form_type(t, sizeof(t), 12u, true, &fresh, '-');
    CHECK(strcmp(t, "0.") == 0);
    t[0] = 0;
    fude_kit_form_type(t, sizeof(t), 12u, true, &fresh, '-');
    fude_kit_form_type(t, sizeof(t), 12u, true, &fresh, '.');
    CHECK(strcmp(t, "-0.") == 0);
    t[0] = 0;
    for(u32 k = 0; k < 20u; k++) {
        fude_kit_form_type(t, sizeof(t), 9u, false, &fresh, '7');
    }
    CHECK(strcmp(t, "777777777") == 0);

    if(fails == 0) printf("ALL PASSED\n"); else printf("%d FAILED\n", fails);
    return fails == 0 ? 0 : 1;
}
