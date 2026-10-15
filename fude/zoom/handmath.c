// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/handmath.h"
#include "zoom/hand_maths.h"

// ===========================================================================
// See handmath.h.
//
// Internal names start fzhm_. Laid out as: the shapes (the Hershey table's lookup, the symbols drawn here as text
// specs, and fzhm_put, which writes either), the characters a font's glyphs stand for, and the writer. A shape is
// made in Hershey units and only fitted to the glyph's ink box as it is written, so the same shapes serve the reader
// (mathread.c) and the writer.
// ===========================================================================

#define FZHM_STEPS 4u   // points between two of a shape's, smoothed

// --- the shapes ------------------------------------------------------------------------------------

// Returns _cp's index in hand_maths.h's table (its code points are in ascending order, so a binary search), or -1.
RDE_INTERNAL i32 fzhm_find(u32 _cp) {
    u32 _lo = 0, _hi = FUDE_ZOOM_HAND_MATHS;
    while(_lo < _hi) {
        const u32 _mid = (_lo + _hi) / 2u;
        if(FUDE_ZOOM_HAND_MATHS_CP[_mid] == _cp) {
            return (i32)_mid;
        }
        if(FUDE_ZOOM_HAND_MATHS_CP[_mid] < _cp) {
            _lo = _mid + 1u;
        } else {
            _hi = _mid;
        }
    }
    return -1;
}

// See the header.
u32 fude_zoom_handmath_symbols(const u32** _cps) {
    *_cps = FUDE_ZOOM_HAND_MATHS_CP;
    return FUDE_ZOOM_HAND_MATHS;
}

// The symbols the Hershey sets have no shape for (or a poor one), drawn here: strokes in the same units (y up, the
// baseline at 0, a capital 21 tall), "|" between them; "x,y" points; "o:x,y,r" a ring; "@HEX" another symbol's shape,
// ":dx,dy,sx,sy" moving and scaling it (a minus scale: turned over). Looked for before the Hershey ones.
typedef struct {
    u32       cp;
    const c8* spec;
} fzhm_custom;

// The symbols drawn here, searched in order. 0xE000 and 0xE001, in the private use area, are no characters: they are
// the small integral's and the root-with-its-bar's shapes, kept here beside the others.
RDE_INTERNAL const fzhm_custom FZHM_CUSTOM[] = {
    { 0x00AC, "-6,10 6,10 6,5" },                                        // ¬
    { 0x0300, "2,14 -2,19" }, { 0x0301, "-2,14 2,19" },                 // ` ´
    { 0x0302, "-5,14 0,19 5,14" }, { 0x030C, "-5,19 0,14 5,19" },       // ^ ˇ
    { 0x0303, "-7,15 -4,17 0,15 4,13 7,15" }, { 0x02DC, "-7,15 -4,17 0,15 4,13 7,15" },   // ~ (over)
    { 0x0304, "-6,17 6,17" }, { 0x00AF, "-6,17 6,17" }, { 0x0305, "-6,17 6,17" },          // ¯
    { 0x0306, "-5,19 -3,16 0,15 3,16 5,19" },                            // ˘
    { 0x0307, "o:0,17,0.8" }, { 0x0308, "o:-3,17,0.8|o:3,17,0.8" }, { 0x20DB, "o:-5,17,0.8|o:0,17,0.8|o:5,17,0.8" },
    { 0x030A, "o:0,17,2.5" },
    { 0x20D6, "6,17 -6,17|-3,19.5 -6,17 -3,14.5" }, { 0x20D7, "-6,17 6,17|3,19.5 6,17 3,14.5" },   // over-arrows
    { 0x2032, "1.5,21 -1,15" }, { 0x2033, "@2032:-2,0,1,1|@2032:2,0,1,1" }, { 0x2034, "@2032:-4,0,1,1|@2032:0,0,1,1|@2032:4,0,1,1" },
    { 0x0027, "1.5,21 -1,15" },                                          // ' (a prime)
    { 0x03D6, "@03C0" }, { 0x03F1, "@03C1" },                            // ϖ ϱ
    { 0x2020, "0,21 0,-5|-5,14 5,14" },                                  // †
    { 0x2026, "o:-8,0.8,0.8|o:0,0.8,0.8|o:8,0.8,0.8" },                  // …
    { 0x22EF, "o:-8,7,0.8|o:0,7,0.8|o:8,7,0.8" }, { 0x22EE, "o:0,2,0.8|o:0,9,0.8|o:0,16,0.8" }, { 0x22F1, "o:-8,16,0.8|o:0,9,0.8|o:8,2,0.8" },
    { 0x210F, "@0068|-6,15 1,18" },                                      // ℏ
    { 0x2111, "@0049" }, { 0x211C, "@0052" }, { 0x2118, "@0070" },       // ℑ ℜ ℘
    { 0x2194, "-9,7 9,7|-6,10 -9,7 -6,4|6,10 9,7 6,4" },                 // ↔
    { 0x2195, "0,-2 0,16|-3,13 0,16 3,13|-3,1 0,-2 3,1" },               // ↕
    { 0x2197, "-6,1 6,13|0,13 6,13 6,7" }, { 0x2198, "-6,13 6,1|0,1 6,1 6,7" },
    { 0x219D, "@2192" }, { 0x21AA, "@2192" },
    { 0x21A6, "-9,7 9,7|6,10 9,7 6,4|-9,10 -9,4" }, { 0x27FC, "@21A6" }, // ↦
    { 0x21CC, "-9,9 9,9 5,13|9,5 -9,5 -5,1" },                           // ⇌
    { 0x21D0, "9,10 -4,10|9,4 -4,4|-1,13 -8,7 -1,1" }, { 0x27F8, "@21D0" },
    { 0x21D2, "-9,10 4,10|-9,4 4,4|1,13 8,7 1,1" }, { 0x27F9, "@21D2" },
    { 0x21D4, "-5,10 5,10|-5,4 5,4|-2,13 -9,7 -2,1|2,13 9,7 2,1" }, { 0x27FA, "@21D4" },
    { 0x21D1, "-3,-2 -3,11|3,-2 3,11|-6,8 0,15 6,8" }, { 0x21D3, "-3,16 -3,3|3,16 3,3|-6,6 0,-1 6,6" },
    { 0x27F5, "@2190" }, { 0x27F6, "@2192" },
    { 0x2200, "-7,21 0,0 7,21|-4,10 4,10" },                             // ∀
    { 0x2204, "@2203|-5,-3 5,24" },                                      // ∄
    { 0x2205, "o:0,9,7.5|-7,-2 7,20" },                                  // ∅
    { 0x2209, "@2208|-4,-2 4,18" }, { 0x220B, "@2208:0,0,-1,1" },        // ∉ ∋
    { 0x2210, "@220F:0,0,1,-1" },                                        // ∐
    { 0x2216, "@005C" }, { 0x2218, "o:0,7,3" }, { 0x2219, "o:0,7,1.5" }, // ∖ ∘ ∙
    { 0x2224, "@007C|-4,4 4,12" },                                       // ∤
    { 0x2227, "-6,0 0,14 6,0" }, { 0x2228, "-6,14 0,0 6,14" },           // ∧ ∨
    { 0x22C0, "@2227" }, { 0x22C1, "@2228" }, { 0x22C2, "@2229" }, { 0x22C3, "@222A" },
    { 0x222C, "@222B:-4,0,1,1|@222B:4,0,1,1" }, { 0x222D, "@222B:-7,0,1,1|@222B:0,0,1,1|@222B:7,0,1,1" },
    { 0x222E, "@222B|o:0,9,4" }, { 0x222F, "@222C|o:0,9,6" },            // ∬ ∭ ∮ ∯
    { 0x2240, "0,14 -1.5,10 1.5,4 0,0" },                                // ≀
    { 0x2248, "-7,10 -4,12 0,10 4,8 7,10|-7,4 -4,6 0,4 4,2 7,4" }, { 0x224D, "@2248" },   // ≈ ≍
    { 0x2243, "-7,10 -4,12 0,10 4,8 7,10|-7,4 7,4" },                    // ≃
    { 0x2245, "-7,13 -4,15 0,13 4,11 7,13|-7,7 7,7|-7,2 7,2" },          // ≅
    { 0x2250, "-7,9 7,9|-7,4 7,4|o:0,14,0.8" },                          // ≐
    { 0x2254, "o:-8,10,0.8|o:-8,4,0.8|-5,10 7,10|-5,4 7,4" },            // ≔
    { 0x226A, "@003C:-3,0,1,1|@003C:3,0,1,1" }, { 0x226B, "@003E:-3,0,1,1|@003E:3,0,1,1" },   // ≪ ≫
    { 0x227A, "8,14 2,9 -6,7 2,5 8,0" }, { 0x227B, "-8,14 -2,9 6,7 -2,5 -8,0" },             // ≺ ≻
    { 0x2AAF, "@227A|-7,-4 8,-4" }, { 0x2AB0, "@227B|-8,-4 7,-4" },
    { 0x2286, "@2282|-6,-4 6,-4" }, { 0x2287, "@2283|-6,-4 6,-4" }, { 0x228A, "@2282|-6,-4 6,-4|-1,-6 1,-2" },
    { 0x2293, "-6,0 -6,14 6,14 6,0" }, { 0x2294, "-6,14 -6,0 6,0 6,14" }, { 0x2A06, "@2294" },
    { 0x2295, "o:0,7,7|-7,7 7,7|0,0 0,14" }, { 0x2296, "o:0,7,7|-7,7 7,7" }, { 0x2297, "o:0,7,7|-5,2 5,12|-5,12 5,2" },
    { 0x2298, "o:0,7,7|-5,2 5,12" }, { 0x2299, "o:0,7,7|o:0,7,1" },
    { 0x2A00, "@2299" }, { 0x2A01, "@2295" }, { 0x2A02, "@2297" }, { 0x2A04, "@222A" },
    { 0x22A2, "-6,14 -6,0|-6,7 6,7" }, { 0x22A3, "6,14 6,0|6,7 -6,7" }, { 0x22A4, "-7,14 7,14|0,14 0,0" },
    { 0x22A7, "-6,14 -6,0|-6,9 6,9|-6,5 6,5" }, { 0x27C2, "@22A5" },
    { 0x22C6, "@002A" },                                                  // ⋆
    { 0x2308, "-3,-4 -3,22 4,22" }, { 0x2309, "3,-4 3,22 -4,22" }, { 0x230A, "-3,22 -3,-4 4,-4" }, { 0x230B, "3,22 3,-4 -4,-4" },
    { 0x27E8, "4,22 -4,9 4,-4" }, { 0x27E9, "-4,22 4,9 -4,-4" },         // ⟨ ⟩
    { 0x23DE, "-12,0 -11,3 -1,3 0,6 1,3 11,3 12,0" }, { 0x23DF, "-12,6 -11,3 -1,3 0,0 1,3 11,3 12,6" },   // ⏞ ⏟
    { 0x25A1, "-6,0 -6,12 6,12 6,0 -6,0" }, { 0x25B3, "-7,0 0,13 7,0 -7,0" },   // □ △
    { 0x2661, "0,0 -7,8 -6,12 -3,13 0,10 3,13 6,12 7,8 0,0" },          // ♡
    { 0x2663, "o:0,11,3|o:-4,6,3|o:4,6,3|0,6 0,0" },                     // ♣
    { 0x2713, "-6,7 -2,2 7,14" },                                        // ✓
    // (the Hershey sets draw these thick, several strokes side by side: a hand's single ones)
    { 0x0045, "8,21 -6,21 -6,0 8,0|-6,11 5,11" }, { 0x0395, "@0045" },   // E Ε
    { 0x004D, "-8,0 -8,21 0,4 8,21 8,0" }, { 0x039C, "@004D" },         // M Μ
    { 0x0057, "-10,21 -5,0 0,15 5,0 10,21" }, { 0x0077, "-8,14 -4,0 0,10 4,0 8,14" },   // W w
    { 0x005B, "4,22 -2,22 -2,-4 4,-4" }, { 0x005D, "-4,22 2,22 2,-4 -4,-4" },           // [ ]
    { 0x2135, "-6,0 6,21|-6,21 -3,11|6,0 2,10" },                        // ℵ
    { 0x2203, "-7,21 7,21 7,0 -7,0|-5,11 7,11" },                        // ∃
    { 0x2207, "-8,21 8,21 0,0 -8,21" },                                  // ∇
    { 0x220F, "-9,21 9,21|-6,21 -6,-1|6,21 6,-1" },                      // ∏
    { 0x2211, "8,21 -7,21 2,10 -7,-1 8,-1" },                            // ∑
    { 0x222B, "6,22 4,23 2,21 0,10 -2,-1 -4,-2 -6,0" },                  // ∫
    { 0xE000, "4,20 2,21 1,19 0,10 -1,1 -2,-1 -4,0" },                   // (∫ written small, straighter)
    { 0x221A, "-8,8 -6,9 -3,0 2,22" },                                   // √ (its bar a rule of its own)
    { 0x2264, "7,20 -7,13 7,6|-7,1 7,1" }, { 0x2265, "-7,20 7,13 -7,6|-7,1 7,1" },   // ≤ ≥ (one line under, as written)
    { 0xE001, "-8,8 -6,9 -3,0 2,22 14,22" },                             // (√ written with its bar)
};

#define FZHM_DEPTH 3u   // a symbol's shape made of others', so deep at most

// A move and a scale put on a shape's points: each point (x, y) becomes (dx + x * sx, dy + y * sy).
typedef struct {
    f64 dx, dy, sx, sy;
} fzhm_move;

RDE_INTERNAL u32 fzhm_put(u32 _cp, fzhm_move _m, rde_arr* _points, rde_arr* _ends, u32 _depth);

// Ends a stroke: the number of points so far goes on _ends.
RDE_INTERNAL void fzhm_end(rde_arr* _points, rde_arr* _ends) {
    const u32 _end = (u32)rde_arr_length(_points);
    rde_arr_add(_ends, (any)&_end);
}

// Appends a custom symbol's strokes (its spec, FZHM_CUSTOM's format), moved by _m, and returns how many. A part longer
// than 95 bytes is cut; a stroke whose points cannot be read adds nothing. A ring is 6 points, or 13 from a radius of
// 1.5. Another symbol's shape is taken only while _depth is under FZHM_DEPTH.
RDE_INTERNAL u32 fzhm_spec(const c8* _spec, fzhm_move _m, rde_arr* _points, rde_arr* _ends, u32 _depth) {
    u32 _strokes = 0;
    for(const c8* _s = _spec; *_s != 0;) {
        const c8* _bar = rde_strchr(_s, '|');
        const usize _len = _bar != NULL ? (usize)(_bar - _s) : rde_strlen(_s);
        c8 _part[96];
        rde_snprintf(_part, sizeof(_part), "%.*s", (int)(_len < sizeof(_part) - 1u ? _len : sizeof(_part) - 1u), _s);
        _s = _bar != NULL ? _bar + 1 : _s + _len;
        if(_part[0] == '@') {
            // (another symbol's shape, moved and scaled: its own move put after this one's)
            c8* _after = NULL;
            const u32 _sub = (u32)rde_strtoul(_part + 1, &_after, 16);
            fzhm_move _n = { 0.0, 0.0, 1.0, 1.0 };
            if(_after != NULL && *_after == ':') {
                rde_sscanf(_after + 1, "%lf,%lf,%lf,%lf", &_n.dx, &_n.dy, &_n.sx, &_n.sy);
            }
            const fzhm_move _both = { _m.dx + _n.dx * _m.sx, _m.dy + _n.dy * _m.sy, _m.sx * _n.sx, _m.sy * _n.sy };
            _strokes += _depth < FZHM_DEPTH ? fzhm_put(_sub, _both, _points, _ends, _depth + 1u) : 0u;
            continue;
        }
        if(_part[0] == 'o' && _part[1] == ':') {
            f64 _cx = 0.0, _cy = 0.0, _r = 1.0;
            rde_sscanf(_part + 2, "%lf,%lf,%lf", &_cx, &_cy, &_r);
            const u32 _n = _r < 1.5 ? 6u : 13u;
            for(u32 _i = 0; _i < _n; _i++) {
                const f64 _a = 2.0 * RDE_PI * (f64)_i / (f64)(_n - 1u) + 1.2;
                const fude_zoom_v2 _p = { _m.dx + (_cx + rde_math_cos_f64(_a) * _r) * _m.sx, _m.dy + (_cy + rde_math_sin_f64(_a) * _r) * _m.sy };
                rde_arr_add(_points, (any)&_p);
            }
            fzhm_end(_points, _ends);
            _strokes++;
            continue;
        }
        u32 _n = 0;
        for(const c8* _q = _part; *_q != 0;) {
            f64 _x = 0.0, _y = 0.0;
            i32 _used = 0;
            if(rde_sscanf(_q, " %lf,%lf%n", &_x, &_y, &_used) != 2 || _used <= 0) {
                break;
            }
            _q += _used;
            const fude_zoom_v2 _p = { _m.dx + _x * _m.sx, _m.dy + _y * _m.sy };
            rde_arr_add(_points, (any)&_p);
            _n++;
        }
        if(_n > 0u) {
            fzhm_end(_points, _ends);
            _strokes++;
        }
    }
    return _strokes;
}

// Appends character _cp's shape, moved by _m, and returns how many strokes; 0 when it has none. _cp is mapped through
// fude_zoom_handmath_base first, and a symbol drawn here is used before the Hershey one. _depth: how many shapes deep
// this one is inside others (fzhm_spec).
RDE_INTERNAL u32 fzhm_put(u32 _cp, fzhm_move _m, rde_arr* _points, rde_arr* _ends, u32 _depth) {
    _cp = fude_zoom_handmath_base(_cp);
    for(u32 _i = 0; _i < sizeof(FZHM_CUSTOM) / sizeof(FZHM_CUSTOM[0]); _i++) {
        if(FZHM_CUSTOM[_i].cp == _cp) {
            return fzhm_spec(FZHM_CUSTOM[_i].spec, _m, _points, _ends, _depth);
        }
    }
    const i32 _k = fzhm_find(_cp);
    if(_k < 0) {
        return 0u;
    }
    u32 _strokes = 0;
    b8 _open = false;
    for(u32 _i = FUDE_ZOOM_HAND_MATHS_STARTS[_k]; _i + 1u < FUDE_ZOOM_HAND_MATHS_STARTS[_k + 1]; _i += 2u) {
        const i8 _x = FUDE_ZOOM_HAND_MATHS_POINTS[_i], _y = FUDE_ZOOM_HAND_MATHS_POINTS[_i + 1u];
        if(_x == -128 && _y == -128) {
            if(_open) {
                fzhm_end(_points, _ends);
                _strokes++;
            }
            _open = false;
            continue;
        }
        const fude_zoom_v2 _p = { _m.dx + (f64)_x * _m.sx, _m.dy + (9.0 - (f64)_y) * _m.sy };   // (y up, the baseline at 0)
        rde_arr_add(_points, (any)&_p);
        _open = true;
    }
    if(_open) {
        fzhm_end(_points, _ends);
        _strokes++;
    }
    return _strokes;
}

// See the header.
u32 fude_zoom_handmath_shape(u32 _cp, rde_arr* _points, rde_arr* _ends) {
    return fzhm_put(_cp, (fzhm_move){ 0.0, 0.0, 1.0, 1.0 }, _points, _ends, 0u);
}

// See the header. The maths alphabets are worked out from their blocks' layout in Unicode; the letterlike symbols that
// fill their holes are listed one by one.
u32 fude_zoom_handmath_base(u32 _cp) {
    // Mathematical Alphanumeric Symbols: 13 alphabets of A–Z a–z, 5 of Greek, 5 of digits.
    if(_cp >= 0x1D400u && _cp < 0x1D6A4u) {
        const u32 _i = (_cp - 0x1D400u) % 52u;
        return _i < 26u ? 'A' + _i : 'a' + (_i - 26u);
    }
    if(_cp == 0x1D6A4u) {
        return 'i';   // (dotless ı)
    }
    if(_cp == 0x1D6A5u) {
        return 'j';
    }
    if(_cp >= 0x1D6A8u && _cp < 0x1D7CAu) {
        // (each: Α–Ω with ϴ, ∇, α–ω, ∂, ϵ ϑ ϰ ϕ ϱ ϖ)
        const u32 _i = (_cp - 0x1D6A8u) % 58u;
        static const u32 _tail[6] = { 0x03B5u, 0x03B8u, 0x03BAu, 0x03C6u, 0x03C1u, 0x03C0u };
        return _i < 25u ? (_i == 17u ? 0x0398u : 0x0391u + _i) : _i == 25u ? 0x2207u : _i < 51u ? 0x03B1u + (_i - 26u) : _i == 51u ? 0x2202u : _tail[_i - 52u];
    }
    if(_cp >= 0x1D7CEu && _cp <= 0x1D7FFu) {
        return '0' + (_cp - 0x1D7CEu) % 10u;
    }
    switch(_cp) {   // (the letterlike symbols the alphabets' holes are)
    case 0x210Eu: return 'h';
    case 0x212Fu: case 0x2130u: return _cp == 0x212Fu ? 'e' : 'E';
    case 0x210Au: return 'g';
    case 0x2134u: return 'o';
    case 0x212Cu: return 'B';
    case 0x2131u: return 'F';
    case 0x210Bu: case 0x210Cu: case 0x210Du: return 'H';
    case 0x2110u: case 0x2111u: return 'I';
    case 0x2112u: return 'L';
    case 0x2133u: return 'M';
    case 0x211Bu: case 0x211Cu: case 0x211Du: return 'R';
    case 0x2102u: case 0x212Du: return 'C';
    case 0x2115u: return 'N';
    case 0x2119u: return 'P';
    case 0x211Au: return 'Q';
    case 0x2124u: case 0x2128u: return 'Z';
    case 0x2113u: return 'l';   // ℓ
    case 0x0131u: return 'i';
    case 0x2061u: return 0u;    // (function application: nothing)
    default: return _cp;
    }
}

// --- a font's glyphs' characters ----------------------------------------------------------------------

// See the header.
void fude_zoom_handmath_chars_init(fude_zoom_handmath_chars* _c) {
    rde_memset(_c, 0, sizeof(*_c));
    _c->chars = rde_hash_map_new(sizeof(u64), sizeof(u32), rde_hash_map_fn_u64_hash, rde_hash_map_fn_u64_cmp, NULL, rde_memory_allocator_get_default_std());
}

// See the header.
void fude_zoom_handmath_chars_free(fude_zoom_handmath_chars* _c) {
    rde_hash_map_free(&_c->chars);
    rde_memset(_c, 0, sizeof(*_c));
}

// Notes that glyph _g stands for _cp, unless either is 0.
RDE_INTERNAL void fzhm_note(fude_zoom_handmath_chars* _c, u32 _g, u32 _cp) {
    if(_g != 0u && _cp != 0u) {
        const u64 _key = _g;
        rde_hash_map_add(&_c->chars, &_key, &_cp);   // (the first found kept: a glyph's own character before a piece's)
    }
}

// See the header. A font's glyph map goes from characters to glyphs, so each character a formula may use is looked up
// in turn: first every one's own glyph, then the bigger forms and pieces of each, so a glyph that is both keeps its
// own character.
void fude_zoom_handmath_chars_read(fude_zoom_handmath_chars* _c, const fude_zoom_mfont* _font) {
    if(_font == NULL || _c->font == (const any)_font) {
        return;
    }
    rde_hash_map_clear(&_c->chars);
    _c->font = _font;
    // (each character a formula is set with that it may come to: those it has shapes for, the maths alphabets, and
    // then each one's bigger forms and pieces)
    rde_arr _cps = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    for(u32 _i = 0; _i < FUDE_ZOOM_HAND_MATHS; _i++) {
        rde_arr_add(&_cps, (any)&FUDE_ZOOM_HAND_MATHS_CP[_i]);
    }
    for(u32 _cp = 0x1D400u; _cp <= 0x1D7FFu; _cp++) {
        rde_arr_add(&_cps, (any)&_cp);
    }
    static const u32 _ranges[][2] = { { 0x0300u, 0x036Fu }, { 0x02C6u, 0x02DDu }, { 0x20D0u, 0x20F0u }, { 0x2100u, 0x214Fu }, { 0x2190u, 0x21FFu },
                                      { 0x2200u, 0x22FFu }, { 0x2300u, 0x23FFu }, { 0x25A0u, 0x25FFu }, { 0x2660u, 0x2667u }, { 0x2713u, 0x2713u },
                                      { 0x27C0u, 0x27FFu }, { 0x2A00u, 0x2AFFu }, { 0x0391u, 0x03F5u }, { 0x00A0u, 0x00FFu } };
    for(u32 _r = 0; _r < sizeof(_ranges) / sizeof(_ranges[0]); _r++) {
        for(u32 _cp = _ranges[_r][0]; _cp <= _ranges[_r][1]; _cp++) {
            rde_arr_add(&_cps, (any)&_cp);
        }
    }
    static const u32 _more[] = { 0x210Eu, 0x2113u, 0x0131u, 0x2212u, 0x22C5u, 0x2217u, 0x2032u, 0x2033u, 0x2034u, 0x230Au, 0x230Bu, 0x2308u, 0x2309u,
                                 0x27E8u, 0x27E9u, 0x222Cu, 0x222Du, 0x222Eu, 0x22C3u, 0x22C2u, 0x2210u };
    for(u32 _i = 0; _i < sizeof(_more) / sizeof(_more[0]); _i++) {
        rde_arr_add(&_cps, (any)&_more[_i]);
    }
    const u32* _list = (const u32*)_cps.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_cps); _i++) {
        fzhm_note(_c, fude_zoom_mfont_glyph(_font, _list[_i]), fude_zoom_handmath_base(_list[_i]));
    }
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_cps); _i++) {
        const u32 _g = fude_zoom_mfont_glyph(_font, _list[_i]);
        const u32 _base = fude_zoom_handmath_base(_list[_i]);
        if(_g == 0u || _base == 0u) {
            continue;
        }
        for(u32 _way = 0; _way < 2u; _way++) {
            u32 _v[32];
            i32 _sizes[32];
            const u32 _nv = fude_zoom_mfont_variants(_font, _g, _way == 0u, _v, _sizes, 32u);
            for(u32 _k = 0; _k < _nv; _k++) {
                fzhm_note(_c, _v[_k], _base);
            }
            fude_zoom_mfont_part _parts[16];
            const u32 _np = fude_zoom_mfont_assembly(_font, _g, _way == 0u, _parts, 16u);
            for(u32 _k = 0; _k < _np; _k++) {
                fzhm_note(_c, _parts[_k].glyph, _base);
            }
        }
    }
    rde_arr_free(&_cps);
}

// See the header.
u32 fude_zoom_handmath_char(const fude_zoom_handmath_chars* _c, u32 _g) {
    const u64  _key = _g;
    const u32* _cp = (const u32*)rde_hash_map_find(&_c->chars, &_key);
    return _cp != NULL ? *_cp : 0u;
}

// --- written ----------------------------------------------------------------------------------------

// One thing to write: a glyph's character, or several pieces of a tall one joined.
typedef struct {
    u32 cp;
    f64 x0, y0, x1, y1;   // its ink box (the points' units, y up)
    f64 em;               // its own em (a script's smaller)
} fzhm_item;

// A linear congruential generator's state (the same as hand.c's): the unevenness, seeded.
typedef struct {
    u32 state;
} fzhm_rng;

// Steps the generator and returns a number between -_by and _by.
RDE_INTERNAL f64 fzhm_spread(fzhm_rng* _r, f64 _by) {
    _r->state = _r->state * 1664525u + 1013904223u;
    return ((f64)(_r->state >> 8) / 16777216.0 * 2.0 - 1.0) * _by;
}

// Returns whether _cp is one the font grows by stacking pieces (a tall bracket, a root, an integral, a bar).
RDE_INTERNAL b8 fzhm_stretches(u32 _cp) {
    return _cp == '(' || _cp == ')' || _cp == '[' || _cp == ']' || _cp == '{' || _cp == '}' || _cp == '|' || _cp == 0x2016u || _cp == 0x2225u ||
           _cp == 0x221Au || _cp == 0x222Bu || _cp == 0x230Au || _cp == 0x230Bu || _cp == 0x2308u || _cp == 0x2309u || _cp == 0x27E8u || _cp == 0x27E9u ||
           _cp == '/' || _cp == '\\';
}

// Returns whether the line turns sharply at _b, from _a on to _c: by more than 60°.
RDE_INTERNAL b8 fzhm_sharp(fude_zoom_v2 _a, fude_zoom_v2 _b, fude_zoom_v2 _c) {
    const f64 _ux = _b.x - _a.x, _uy = _b.y - _a.y, _vx = _c.x - _b.x, _vy = _c.y - _b.y;
    const f64 _lu = rde_math_hypot_f64(_ux, _uy), _lv = rde_math_hypot_f64(_vx, _vy);
    return _lu > 1e-12 && _lv > 1e-12 && (_ux * _vx + _uy * _vy) / (_lu * _lv) < 0.5;
}

// Appends a stroke of _n points (the caller's units, y up) smoothed through them (Catmull-Rom), wavering by _waver,
// and its end. A one-point stroke becomes a tiny line, a dot; an empty one adds nothing.
RDE_INTERNAL void fzhm_stroke(const fude_zoom_v2* _p, u32 _n, f64 _waver, f64 _phase, rde_arr* _points, rde_arr* _ends) {
    if(_n == 0u) {
        return;
    }
    if(_n == 1u) {
        const fude_zoom_v2 _b = { _p[0].x + _waver, _p[0].y + _waver * 0.5 };
        rde_arr_add(_points, (any)&_p[0]);
        rde_arr_add(_points, (any)&_b);
    } else {
        for(u32 _i = 0; _i + 1u < _n; _i++) {
            fude_zoom_v2 _p0 = _p[_i > 0 ? _i - 1u : 0u], _p1 = _p[_i], _p2 = _p[_i + 1u], _p3 = _p[_i + 2u < _n ? _i + 2u : _n - 1u];
            // (a corner, turning more than 60°, kept a corner: no curve round it bowing the strokes out, an E's spine
            // off its bar)
            if(fzhm_sharp(_p0, _p1, _p2)) {
                _p0 = _p1;
            }
            if(fzhm_sharp(_p1, _p2, _p3)) {
                _p3 = _p2;
            }
            for(u32 _s = (_i == 0u ? 0u : 1u); _s <= FZHM_STEPS; _s++) {
                const f64 _t = (f64)_s / (f64)FZHM_STEPS, _t2 = _t * _t, _t3 = _t2 * _t;
                fude_zoom_v2 _u = {
                    0.5 * ((2.0 * _p1.x) + (-_p0.x + _p2.x) * _t + (2.0 * _p0.x - 5.0 * _p1.x + 4.0 * _p2.x - _p3.x) * _t2 + (-_p0.x + 3.0 * _p1.x - 3.0 * _p2.x + _p3.x) * _t3),
                    0.5 * ((2.0 * _p1.y) + (-_p0.y + _p2.y) * _t + (2.0 * _p0.y - 5.0 * _p1.y + 4.0 * _p2.y - _p3.y) * _t2 + (-_p0.y + 3.0 * _p1.y - 3.0 * _p2.y + _p3.y) * _t3),
                };
                const f64 _w = _phase + ((f64)_i + _t) * 0.9;
                _u.x += rde_math_sin_f64(_w) * _waver;
                _u.y += rde_math_cos_f64(_w * 1.3) * _waver * 0.8;
                rde_arr_add(_points, (any)&_u);
            }
        }
    }
    const u32 _end = (u32)rde_arr_length(_points);
    rde_arr_add(_ends, (any)&_end);
}

// See the header. Three steps: the glyphs are gathered with their ink boxes (a tall bracket's pieces joined into one),
// put in writing order with the rules, and each written, a glyph as its shape fitted into its box, a rule as a line
// along its middle.
u32 fude_zoom_handmath_write(const fude_zoom_handmath_chars* _c, const fude_zoom_mfont* _font, const fude_zoom_tex_box* _box, f64 _em, u32 _seed,
                             rde_arr* _points, rde_arr* _ends, u32* _missing) {
    if(_missing != NULL) {
        *_missing = 0u;
    }
    if(_c == NULL || _font == NULL || _box == NULL || !(_em > 0.0)) {
        return 0u;
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _items = rde_arr_new(sizeof(fzhm_item), _heap);
    const f64 _upem = (f64)_font->units_per_em;
    // Each glyph: its character and its ink box; a tall bracket's pieces, one above another, one box.
    const fude_zoom_tex_glyph* _g = (const fude_zoom_tex_glyph*)_box->glyphs.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_box->glyphs); _i++) {
        i32 _ink[4];
        if(!fude_zoom_mfont_box(_font, _g[_i].glyph, _ink)) {
            continue;   // (a space)
        }
        const u32 _cp = fude_zoom_handmath_char(_c, _g[_i].glyph);
        const f64 _k = _g[_i].size * _em / _upem;
        fzhm_item _it = { _cp, _g[_i].x * _em + _ink[0] * _k, _g[_i].y * _em + _ink[1] * _k, _g[_i].x * _em + _ink[2] * _k, _g[_i].y * _em + _ink[3] * _k,
                          _g[_i].size * _em };
        fzhm_item* _last = rde_arr_length(&_items) > 0u ? &((fzhm_item*)_items.memory)[rde_arr_length(&_items) - 1u] : NULL;
        if(_last != NULL && _cp != 0u && _last->cp == _cp && fzhm_stretches(_cp)) {
            const f64 _over = rde_math_min_f64(_last->x1, _it.x1) - rde_math_max_f64(_last->x0, _it.x0);
            const f64 _gap  = rde_math_max_f64(_last->y0, _it.y0) - rde_math_min_f64(_last->y1, _it.y1);   // (below nothing: they overlap)
            if(_over >= 0.5 * rde_math_min_f64(_last->x1 - _last->x0, _it.x1 - _it.x0) && _gap <= 0.2 * _it.em) {
                _last->x0 = rde_math_min_f64(_last->x0, _it.x0); _last->y0 = rde_math_min_f64(_last->y0, _it.y0);
                _last->x1 = rde_math_max_f64(_last->x1, _it.x1); _last->y1 = rde_math_max_f64(_last->y1, _it.y1);
                continue;
            }
        }
        rde_arr_add(&_items, (any)&_it);
    }
    // In the order a hand writes them (left to right, a fraction's bar before what is over it), each its shape, fitted
    // into its box, a little uneven; a rule a line.
    fzhm_rng _rng = { _seed * 2654435761u + 12345u };
    rde_arr _shape = rde_arr_new(sizeof(fude_zoom_v2), _heap), _shape_ends = rde_arr_new(sizeof(u32), _heap), _one = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    u32 _strokes = 0;
    const fzhm_item* _it = (const fzhm_item*)_items.memory;
    const fude_zoom_tex_rule* _r = (const fude_zoom_tex_rule*)_box->rules.memory;
    const u32 _ni = (u32)rde_arr_length(&_items), _nr = (u32)rde_arr_length(&_box->rules);
    rde_arr _order_arr = rde_arr_new(sizeof(u32), _heap);
    for(u32 _i = 0; _i < _ni + _nr; _i++) {
        // (insertion by left edge, kept in turn where they tie)
        const f64 _x = _i < _ni ? _it[_i].x0 : _r[_i - _ni].x * _em;
        u32 _at = (u32)rde_arr_length(&_order_arr);
        const u32* _o = (const u32*)_order_arr.memory;
        while(_at > 0u) {
            const u32 _p = _o[_at - 1u];
            const f64 _px = _p < _ni ? _it[_p].x0 : _r[_p - _ni].x * _em;
            if(_px <= _x) {
                break;
            }
            _at--;
        }
        rde_arr_add(&_order_arr, (any)&_i);
        u32* _ow = (u32*)_order_arr.memory;
        rde_memmove(_ow + _at + 1u, _ow + _at, sizeof(u32) * ((u32)rde_arr_length(&_order_arr) - 1u - _at));
        _ow[_at] = _i;
    }
    const u32* _order = (const u32*)_order_arr.memory;
    for(u32 _oi = 0; _oi < _ni + _nr; _oi++) {
        const u32 _i = _order[_oi];
        if(_i >= _ni) {
            const u32 _k = _i - _ni;
            const f64 _x0 = _r[_k].x * _em, _y0 = _r[_k].y * _em, _w = _r[_k].w * _em, _h = _r[_k].h * _em;
            const b8 _across = _w >= _h;
            const fude_zoom_v2 _line[3] = { _across ? (fude_zoom_v2){ _x0, _y0 + 0.5 * _h + fzhm_spread(&_rng, 0.01 * _em) } : (fude_zoom_v2){ _x0 + 0.5 * _w, _y0 },
                                            _across ? (fude_zoom_v2){ _x0 + 0.5 * _w, _y0 + 0.5 * _h + fzhm_spread(&_rng, 0.015 * _em) } :
                                                      (fude_zoom_v2){ _x0 + 0.5 * _w + fzhm_spread(&_rng, 0.015 * _em), _y0 + 0.5 * _h },
                                            _across ? (fude_zoom_v2){ _x0 + _w, _y0 + 0.5 * _h + fzhm_spread(&_rng, 0.01 * _em) } : (fude_zoom_v2){ _x0 + 0.5 * _w, _y0 + _h } };
            fzhm_stroke(_line, 3u, 0.006 * _em, fzhm_spread(&_rng, 3.0), _points, _ends);
            _strokes++;
            continue;
        }
        rde_arr_clear(&_shape);
        rde_arr_clear(&_shape_ends);
        const u32 _n = _it[_i].cp != 0u ? fude_zoom_handmath_shape(_it[_i].cp, &_shape, &_shape_ends) : 0u;
        if(_n == 0u) {
            if(_missing != NULL) {
                (*_missing)++;
            }
            continue;
        }
        const fude_zoom_v2* _s = (const fude_zoom_v2*)_shape.memory;
        f64 _sx0 = 1e300, _sy0 = 1e300, _sx1 = -1e300, _sy1 = -1e300;
        for(u32 _k = 0; _k < (u32)rde_arr_length(&_shape); _k++) {
            _sx0 = rde_math_min_f64(_sx0, _s[_k].x); _sx1 = rde_math_max_f64(_sx1, _s[_k].x);
            _sy0 = rde_math_min_f64(_sy0, _s[_k].y); _sy1 = rde_math_max_f64(_sy1, _s[_k].y);
        }
        // (the hand: a slant, a size and a place each a touch off, less for what stretches over others)
        const b8  _tall  = fzhm_stretches(_it[_i].cp);
        const f64 _e     = _it[_i].em;
        const f64 _slant = fzhm_spread(&_rng, _tall ? 0.02 : 0.07);
        const f64 _grow  = 1.0 + fzhm_spread(&_rng, 0.04);
        const f64 _dx = fzhm_spread(&_rng, 0.025 * _e), _dy = fzhm_spread(&_rng, 0.025 * _e);
        const f64 _bw = _it[_i].x1 - _it[_i].x0, _bh = _it[_i].y1 - _it[_i].y0;
        const f64 _cx = 0.5 * (_it[_i].x0 + _it[_i].x1), _cy = 0.5 * (_it[_i].y0 + _it[_i].y1);
        const f64 _phase = fzhm_spread(&_rng, RDE_PI);
        const u32* _se = (const u32*)_shape_ends.memory;
        u32 _from = 0;
        for(u32 _k = 0; _k < _n; _k++) {
            rde_arr_clear(&_one);
            for(u32 _q = _from; _q < _se[_k]; _q++) {
                // (a shape with no width, a bar's or a 1's, kept upright through the box's middle; one with no
                // height, across it)
                const f64 _u = _sx1 > _sx0 ? (_s[_q].x - _sx0) / (_sx1 - _sx0) - 0.5 : 0.0;
                const f64 _v = _sy1 > _sy0 ? (_s[_q].y - _sy0) / (_sy1 - _sy0) - 0.5 : 0.0;
                const f64 _y = _cy + _v * _bh * _grow + _dy;
                const fude_zoom_v2 _p = { _cx + _u * _bw * _grow + (_y - _cy) * _slant + _dx, _y };
                rde_arr_add(&_one, (any)&_p);
            }
            fzhm_stroke((const fude_zoom_v2*)_one.memory, (u32)rde_arr_length(&_one), 0.012 * _e, _phase + (f64)_k, _points, _ends);
            _strokes++;
            _from = _se[_k];
        }
    }
    rde_arr_free(&_order_arr);
    rde_arr_free(&_items);
    rde_arr_free(&_shape);
    rde_arr_free(&_shape_ends);
    rde_arr_free(&_one);
    return _strokes;
}
