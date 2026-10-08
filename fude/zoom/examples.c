// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/examples.h"
#include "zoom/placer.h"
#include "zoom/circuit.h"
#include "zoom/shape.h"
#include "zoom/props.h"
#include <math.h>
#include <string.h>

#define FZX_DEG 0.017453292519943295


RDE_INTERNAL const u8 FZX_GROUP[FUDE_ZOOM_EXAMPLE_COUNT] = {
    // (the electronics', 19; the mechanisms', 13; both, 9)
    FUDE_ZOOM_EXAMPLES_ELECTRONICS, FUDE_ZOOM_EXAMPLES_ELECTRONICS, FUDE_ZOOM_EXAMPLES_ELECTRONICS, FUDE_ZOOM_EXAMPLES_ELECTRONICS,
    FUDE_ZOOM_EXAMPLES_ELECTRONICS, FUDE_ZOOM_EXAMPLES_ELECTRONICS, FUDE_ZOOM_EXAMPLES_ELECTRONICS, FUDE_ZOOM_EXAMPLES_ELECTRONICS,
    FUDE_ZOOM_EXAMPLES_ELECTRONICS, FUDE_ZOOM_EXAMPLES_ELECTRONICS, FUDE_ZOOM_EXAMPLES_ELECTRONICS, FUDE_ZOOM_EXAMPLES_ELECTRONICS,
    FUDE_ZOOM_EXAMPLES_ELECTRONICS, FUDE_ZOOM_EXAMPLES_ELECTRONICS, FUDE_ZOOM_EXAMPLES_ELECTRONICS, FUDE_ZOOM_EXAMPLES_ELECTRONICS,
    FUDE_ZOOM_EXAMPLES_ELECTRONICS, FUDE_ZOOM_EXAMPLES_ELECTRONICS, FUDE_ZOOM_EXAMPLES_ELECTRONICS,
    FUDE_ZOOM_EXAMPLES_MECHANISMS, FUDE_ZOOM_EXAMPLES_MECHANISMS, FUDE_ZOOM_EXAMPLES_MECHANISMS, FUDE_ZOOM_EXAMPLES_MECHANISMS,
    FUDE_ZOOM_EXAMPLES_MECHANISMS, FUDE_ZOOM_EXAMPLES_MECHANISMS, FUDE_ZOOM_EXAMPLES_MECHANISMS, FUDE_ZOOM_EXAMPLES_MECHANISMS,
    FUDE_ZOOM_EXAMPLES_MECHANISMS, FUDE_ZOOM_EXAMPLES_MECHANISMS, FUDE_ZOOM_EXAMPLES_MECHANISMS, FUDE_ZOOM_EXAMPLES_MECHANISMS,
    FUDE_ZOOM_EXAMPLES_MECHANISMS,
    FUDE_ZOOM_EXAMPLES_BOTH, FUDE_ZOOM_EXAMPLES_BOTH, FUDE_ZOOM_EXAMPLES_BOTH, FUDE_ZOOM_EXAMPLES_BOTH, FUDE_ZOOM_EXAMPLES_BOTH,
    FUDE_ZOOM_EXAMPLES_BOTH, FUDE_ZOOM_EXAMPLES_BOTH, FUDE_ZOOM_EXAMPLES_BOTH, FUDE_ZOOM_EXAMPLES_BOTH,
};

b8 fude_zoom_example_goes_wrong(u32 _example) {
    return _example == FUDE_ZOOM_EXAMPLE_LED_RESISTOR || _example == FUDE_ZOOM_EXAMPLE_RESISTOR_WATTS || _example == FUDE_ZOOM_EXAMPLE_SHORT_FUSE ||
           _example == FUDE_ZOOM_EXAMPLE_CAP_POLARITY || _example == FUDE_ZOOM_EXAMPLE_TRANSISTOR_SIZE || _example == FUDE_ZOOM_EXAMPLE_TOO_HEAVY ||
           _example == FUDE_ZOOM_EXAMPLE_MOTOR_TORQUE || _example == FUDE_ZOOM_EXAMPLE_SERVO_ANGLES;
}

u8 fude_zoom_example_group(u32 _example) {
    return _example < FUDE_ZOOM_EXAMPLE_COUNT ? FZX_GROUP[_example] : FUDE_ZOOM_EXAMPLES_BOTH;
}

// --- drawing one: the library's parts placed, wired, written by ----------------------------------------------------

typedef struct {
    fude_zoom_placer        p;
    fude_zoom_example_title title;
} fzx;

RDE_INTERNAL fude_zoom_v2 fzx_at(const fzx* _x, f64 _px, f64 _py) { return fude_zoom_placer_at(&_x->p, _px, _py); }
RDE_INTERNAL u32 fzx_sized(fzx* _x, const c8* _id, f64 _px, f64 _py, f64 _turn, f64 _w, f64 _h, const c8* _text) {
    return fude_zoom_placer_part(&_x->p, _id, _px, _py, _turn, _w, _h, _text);
}
RDE_INTERNAL u32 fzx_part(fzx* _x, const c8* _id, f64 _px, f64 _py, f64 _turn, const c8* _text) { return fzx_sized(_x, _id, _px, _py, _turn, 0.0, 0.0, _text); }
RDE_INTERNAL fude_zoom_v2 fzx_pin(const fzx* _x, u32 _o, u32 _pin) { return fude_zoom_placer_pin(&_x->p, _o, _pin); }
RDE_INTERNAL void fzx_wire(fzx* _x, u32 _a, u32 _pa, u32 _b, u32 _pb) { fude_zoom_placer_wire(&_x->p, _a, _pa, _b, _pb, false); }
RDE_INTERNAL void fzx_line(fzx* _x, u32 _a, u32 _pa, u32 _b, u32 _pb) { fude_zoom_placer_wire(&_x->p, _a, _pa, _b, _pb, true); }
RDE_INTERNAL void fzx_text(fzx* _x, f64 _px, f64 _py, f64 _size, f64 _w, const c8* _text) { fude_zoom_placer_text(&_x->p, _px, _py, _size, _w, _text); }
RDE_INTERNAL u32 fzx_path(fzx* _x, const fude_zoom_v2* _pts, u32 _n, b8 _closed, f64 _width) { return fude_zoom_placer_path(&_x->p, _pts, _n, _closed, _width); }

// The one being drawn moved to _px, _py (points from where it was); the middle it had back.
RDE_INTERNAL fude_zoom_v2 fzx_move(fzx* _x, f64 _px, f64 _py) {
    const fude_zoom_v2 _was = _x->p.o;
    _x->p.o = (fude_zoom_v2){ _was.x + _px, _was.y + _py };
    return _was;
}

// A wire from part _a's pin _pa to _b's _pb through the corners listed after (points from the middle): laid out by hand,
// so no wire passes over another's pin (it would join it) or along another's way.
#define FZX_VIA(...) (const fude_zoom_v2[]){ __VA_ARGS__ }, (u32)(sizeof((const fude_zoom_v2[]){ __VA_ARGS__ }) / sizeof(fude_zoom_v2))
RDE_INTERNAL void fzx_route(fzx* _x, u32 _a, u32 _pa, u32 _b, u32 _pb, const fude_zoom_v2* _via, u32 _n) {
    fude_zoom_placer_wire_via(&_x->p, _a, _pa, _b, _pb, _via, _n);
}

// A word by what is drawn (FUDE_ZOOM_EXAMPLE_WORD_'s, a material's: examples.h), _size points high from _x, _y.
RDE_INTERNAL void fzx_word(fzx* _x, f64 _px, f64 _py, f64 _size, f64 _w, u32 _word) {
    const c8* _t = _x->title != NULL ? _x->title(_word) : NULL;
    if(_t != NULL && _t[0] != 0) {
        fzx_text(_x, _px, _py, _size, _w, _t);
    }
}

// Limits of its own on a part (its card's: props.h): _w, _a, _v, _r (0: none), the real part they are.
RDE_INTERNAL void fzx_limits(fzx* _x, u32 _part, f64 _w, f64 _a, f64 _v, f64 _r, i32 _preset) {
    const f64 _most[4] = { _w, _a, _v, _r };
    const u32 _made = fude_zoom_props_add_limits(_x->p.s, _part, _most, _preset);
    rde_arr_add(_x->p.born, (any)&_made);
}

// A drawing made a body of material _material (sim/body.h's; fixed: the ground): a pen line through _pts.
RDE_INTERNAL u32 fzx_drawn(fzx* _x, const fude_zoom_v2* _pts, u32 _n, b8 _closed, u32 _material, b8 _fixed) {
    const u32 _o = fzx_path(_x, _pts, _n, _closed, 4.0);
    if(_o != FUDE_ZOOM_NONE) {
        const fude_zoom_body_props _bp = { _material, _fixed, 0.0, -1.0, -1.0 };
        const u32 _made = fude_zoom_props_add_body(_x->p.s, _o, &_bp);
        rde_arr_add(_x->p.born, (any)&_made);
    }
    return _o;
}

// A drawn ball of _r points round at _px, _py, of material _material.
RDE_INTERNAL void fzx_ball(fzx* _x, f64 _px, f64 _py, f64 _r, u32 _material) {
    const f64 _n[2] = { _r * _x->p.u, _r * _x->p.u };
    const u32 _o = fude_zoom_scene_add_shape(_x->p.s, _x->p.frame, (fude_zoom_place){ fzx_at(_x, _px, _py), 0.0, 1.0 }, FUDE_ZOOM_SHAPE_ELLIPSE, _n, 2u, _x->p.ink,
                                             (f32)(1.5 * _x->p.u), 0, 0);
    if(_o != FUDE_ZOOM_NONE) {
        rde_arr_add(_x->p.born, (any)&_o);
        const fude_zoom_body_props _bp = { _material, false, 0.0, -1.0, -1.0 };
        const u32 _made = fude_zoom_props_add_body(_x->p.s, _o, &_bp);
        rde_arr_add(_x->p.born, (any)&_made);
    }
}

RDE_INTERNAL void fzx_title(fzx* _x, f64 _px, f64 _py, u32 _example) {
    const c8* _t = _x->title != NULL ? _x->title(_example) : NULL;
    if(_t != NULL && _t[0] != 0) {
        fzx_text(_x, _px, _py, 26.0, 520.0, _t);
    }
}

// --- electronics ---------------------------------------------------------------------------------------------------

RDE_INTERNAL void fzx_torch(fzx* _x) {
    const u32 _bat = fzx_part(_x, "battery", -200, 0, 0, "9V");
    const u32 _sw  = fzx_part(_x, "SPST switch", -60, 120, 0, "on");
    const u32 _r   = fzx_part(_x, "resistor", 100, 120, 0, "330");
    const u32 _led = fzx_part(_x, "LED", 230, 40, -90, "red");
    fzx_wire(_x, _bat, 0, _sw, 0);
    fzx_wire(_x, _sw, 1, _r, 0);
    fzx_wire(_x, _r, 1, _led, 0);
    fzx_wire(_x, _led, 1, _bat, 1);
    fzx_title(_x, -240, 230, FUDE_ZOOM_EXAMPLE_TORCH);
}

// Every gate on A and B (A 1, B 0), a probe on each in a colour of its own, its name by it.
RDE_INTERNAL void fzx_gates(fzx* _x) {
    static const c8* const _gates[7] = { "AND gate", "OR gate", "XOR gate", "NAND gate", "NOR gate", "XNOR gate", "NOT gate" };
    static const c8* const _names[7] = { "A AND B", "A OR B", "A XOR B", "A NAND B", "A NOR B", "A XNOR B", "NOT A" };
    static const rde_color _lights[7] = { { 255, 40, 40, 255 }, { 40, 230, 70, 255 }, { 60, 130, 255, 255 }, { 255, 220, 30, 255 },
                                         { 255, 140, 20, 255 }, { 255, 110, 200, 255 }, { 255, 255, 250, 255 } };
    const u32 _a = fzx_part(_x, "logic input", -330, 90, 0, "1");
    const u32 _b = fzx_part(_x, "logic input", -330, -90, 0, "0");
    fzx_text(_x, -400, 130, 18.0, 40.0, "A");
    fzx_text(_x, -400, -50, 18.0, 40.0, "B");
    for(u32 _i = 0; _i < 7u; _i++) {
        const f64 _y = 270.0 - (f64)_i * 90.0;
        const u32 _g = fzx_part(_x, _gates[_i], 0, _y, 0, "");
        const u32 _p = fzx_part(_x, "logic probe", 220, _y, 0, _names[_i]);
        fzx_wire(_x, _a, 0, _g, 0);
        if(_i < 6u) {
            fzx_wire(_x, _b, 0, _g, 1);
        }
        fzx_wire(_x, _g, _i < 6u ? 2u : 1u, _p, 0);
        const u32 _l = fude_zoom_props_add_light(_x->p.s, _p, _lights[_i]);
        rde_arr_add(_x->p.born, (any)&_l);
    }
    fzx_title(_x, -330, 380, FUDE_ZOOM_EXAMPLE_GATES);
}

RDE_INTERNAL void fzx_flasher(fzx* _x) {
    const u32 _bat = fzx_part(_x, "battery", -330, 0, 0, "9V");
    const u32 _t   = fzx_part(_x, "NE555", 0, 0, 0, "NE555");
    const u32 _r1  = fzx_part(_x, "resistor", 220, 200, 0, "1k");
    const u32 _r2  = fzx_part(_x, "resistor", 220, 120, 0, "47k");
    const u32 _cap = fzx_part(_x, "electrolytic", -180, -160, -90, "10uF");
    const u32 _rl  = fzx_part(_x, "resistor", -170, 120, 0, "330");
    const u32 _led = fzx_part(_x, "LED", -250, 200, 90, "yellow");
    fzx_wire(_x, _bat, 0, _t, 7);
    fzx_wire(_x, _t, 7, _t, 3);
    fzx_wire(_x, _t, 0, _bat, 1);
    fzx_wire(_x, _t, 7, _r1, 1);
    fzx_wire(_x, _r1, 0, _t, 6);
    fzx_wire(_x, _t, 6, _r2, 1);
    fzx_wire(_x, _r2, 0, _t, 5);
    fzx_wire(_x, _t, 5, _t, 1);
    fzx_wire(_x, _t, 1, _cap, 0);
    fzx_wire(_x, _cap, 1, _bat, 1);
    fzx_wire(_x, _t, 2, _rl, 1);
    fzx_wire(_x, _rl, 0, _led, 0);
    fzx_wire(_x, _led, 1, _bat, 1);
    fzx_title(_x, -330, 300, FUDE_ZOOM_EXAMPLE_FLASHER);
}

// --- what goes wrong (limits.h): parts past what they take -------------------------------------------------------------

// An LED behind 470 Ω on 9 V (19 mA: lit, well within its 30 mA), and one straight on it (its battery's own 0.5 Ω all
// that holds it back: burnt at once — Restart puts in new parts).
RDE_INTERNAL void fzx_led_resistor(fzx* _x) {
    for(u32 _k = 0; _k < 2u; _k++) {
        const f64 _y = _k == 0u ? 140.0 : -160.0;
        const u32 _bat = fzx_part(_x, "battery", -200, _y, 0, "9V");
        const u32 _sw  = fzx_part(_x, "SPST switch", -60, _y + 120, 0, "on");
        const u32 _led = fzx_part(_x, "LED", 230, _y + 40, -90, _k == 0u ? "red" : "green");
        fzx_wire(_x, _bat, 0, _sw, 0);
        if(_k == 0u) {
            const u32 _r = fzx_part(_x, "resistor", 100, _y + 120, 0, "470");
            fzx_wire(_x, _sw, 1, _r, 0);
            fzx_wire(_x, _r, 1, _led, 0);
        } else {
            fzx_wire(_x, _sw, 1, _led, 0);
        }
        fzx_wire(_x, _led, 1, _bat, 1);
    }
    fzx_word(_x, 280, -80, 16.0, 220.0, FUDE_ZOOM_EXAMPLE_WORD_RESTART);
    fzx_title(_x, -240, 350, FUDE_ZOOM_EXAMPLE_LED_RESISTOR);
}

// 47 Ω on 5 V (0.53 W in it): a resistor of a quarter of a watt (its text naming no other: a typical one's) heating past
// its watts — burnt in about half a second —; one of 1 W (its card's limits) warm, well within them.
RDE_INTERNAL void fzx_resistor_watts(fzx* _x) {
    for(u32 _k = 0; _k < 2u; _k++) {
        const f64 _y = _k == 0u ? 140.0 : -160.0;
        const u32 _bat = fzx_part(_x, "battery", -200, _y, 0, "5V");
        const u32 _sw  = fzx_part(_x, "SPST switch", -60, _y + 120, 0, "on");
        const u32 _r   = fzx_part(_x, "resistor", 100, _y + 120, 0, "47");
        fzx_wire(_x, _bat, 0, _sw, 0);
        fzx_wire(_x, _sw, 1, _r, 0);
        fzx_route(_x, _r, 1, _bat, 1, FZX_VIA({ 230, _y + 120 }, { 230, _y - 60 }, { -200, _y - 60 }));
        if(_k == 1u) {
            fzx_limits(_x, _r, 1.0, 0.0, 0.0, 0.0, 3);   // (1 W: the resistors' fourth real part)
        }
        fzx_text(_x, 70, _y + 175, 18.0, 90.0, _k == 0u ? "0.25 W" : "1 W");
    }
    fzx_title(_x, -240, 350, FUDE_ZOOM_EXAMPLE_RESISTOR_WATTS);
}

// A 9 V lamp behind a 1 A fuse, a switch across the lamp (off: tap it on — a short); the fuse blows, the lamp goes out,
// nothing else hurt. Below, the same with no fuse: the battery says it is shorted, and the switch, past its 3 A, burns.
RDE_INTERNAL void fzx_short_fuse(fzx* _x) {
    for(u32 _k = 0; _k < 2u; _k++) {
        const f64 _y = _k == 0u ? 140.0 : -200.0;
        const u32 _bat  = fzx_part(_x, "battery", -200, _y, 0, "9V");
        const u32 _lamp = fzx_part(_x, "lamp", 110, _y + 120, 0, "9V 3W");
        const u32 _sw   = fzx_part(_x, "SPST switch", 110, _y + 50, 0, "off");
        if(_k == 0u) {
            const u32 _fuse = fzx_part(_x, "fuse", -60, _y + 120, 0, "1A");
            fzx_wire(_x, _bat, 0, _fuse, 0);
            fzx_wire(_x, _fuse, 1, _lamp, 0);
        } else {
            fzx_route(_x, _bat, 0, _lamp, 0, FZX_VIA({ -200, _y + 120 }));
            fzx_word(_x, -170, _y + 170, 16.0, 160.0, FUDE_ZOOM_EXAMPLE_WORD_NO_FUSE);
        }
        fzx_route(_x, _lamp, 0, _sw, 0, FZX_VIA({ 70, _y + 120 }, { 70, _y + 50 }));
        fzx_route(_x, _lamp, 1, _sw, 1, FZX_VIA({ 150, _y + 120 }, { 150, _y + 50 }));
        fzx_route(_x, _lamp, 1, _bat, 1, FZX_VIA({ 240, _y + 120 }, { 240, _y - 60 }, { -200, _y - 60 }));
        fzx_word(_x, 265, _y + 60, 16.0, 260.0, FUDE_ZOOM_EXAMPLE_WORD_TAP_SHORT);   // (right of it, level with the switch)
    }
    fzx_title(_x, -240, 350, FUDE_ZOOM_EXAMPLE_SHORT_FUSE);
}

// Electrolytics charged from 12 V: a 16 V one, well; one backwards (its + toward ground: past its 1 V the wrong way round,
// popped); a 6.3 V one (its card's limits) past its volts, popped.
RDE_INTERNAL void fzx_cap_polarity(fzx* _x) {
    for(u32 _k = 0; _k < 3u; _k++) {
        const f64 _y = 200.0 - 160.0 * (f64)_k;
        const u32 _rail = fzx_part(_x, "supply rail", -200, _y + 40, 0, "12V");
        const u32 _cap  = fzx_part(_x, "electrolytic", 0, _y, _k == 1u ? 180 : 0, "100uF");
        const u32 _gnd  = fzx_part(_x, "ground", 200, _y - 40, 0, "");
        fzx_route(_x, _rail, 0, _cap, _k == 1u ? 1u : 0u, FZX_VIA({ -200, _y }));
        fzx_route(_x, _cap, _k == 1u ? 0u : 1u, _gnd, 0, FZX_VIA({ 200, _y }));
        if(_k == 2u) {
            fzx_limits(_x, _cap, 0.0, 0.0, 6.3, 1.0, 1);   // (6.3 V: the electrolytics' second real part)
        }
        if(_k == 1u) {
            fzx_word(_x, -40, _y + 50, 16.0, 160.0, FUDE_ZOOM_EXAMPLE_WORD_BACKWARDS);
        } else {
            fzx_text(_x, -30, _y + 50, 18.0, 80.0, _k == 0u ? "16V" : "6.3V");
        }
    }
    fzx_word(_x, -220, -200, 16.0, 260.0, FUDE_ZOOM_EXAMPLE_WORD_RESTART);
    fzx_title(_x, -240, 340, FUDE_ZOOM_EXAMPLE_CAP_POLARITY);
}

// A 12 V 5 W lamp (0.42 A) switched by a transistor, its base from 5 V through 470 Ω: a BC547 (100 mA at most: its text
// names it) burnt at once; a TIP120 (5 A) lighting it.
RDE_INTERNAL void fzx_transistor_size(fzx* _x) {
    for(u32 _k = 0; _k < 2u; _k++) {
        const f64 _y = _k == 0u ? 150.0 : -230.0;
        const u32 _r12  = fzx_part(_x, "supply rail", 100, _y + 170, 0, "12V");
        const u32 _lamp = fzx_part(_x, "lamp", 100, _y + 110, -90, "12V 5W");
        const u32 _q    = fzx_part(_x, "NPN", 90, _y, 0, _k == 0u ? "BC547" : "TIP120");
        const u32 _gnd  = fzx_part(_x, "ground", 100, _y - 80, 0, "");
        const u32 _r5   = fzx_part(_x, "supply rail", -150, _y + 40, 0, "5V");
        const u32 _rb   = fzx_part(_x, "resistor", -20, _y, 0, "470");
        fzx_wire(_x, _r12, 0, _lamp, 0);
        fzx_wire(_x, _lamp, 1, _q, 1);
        fzx_wire(_x, _q, 2, _gnd, 0);
        fzx_route(_x, _r5, 0, _rb, 0, FZX_VIA({ -150, _y }));
        fzx_wire(_x, _rb, 1, _q, 0);
    }
    fzx_word(_x, 160, 150, 16.0, 220.0, FUDE_ZOOM_EXAMPLE_WORD_RESTART);
    fzx_title(_x, -240, 380, FUDE_ZOOM_EXAMPLE_TRANSISTOR_SIZE);
}

// 5 V switched onto 1 Ω, 10 H and 1000 µF in a row: the capacitor rings up to twice the supply and back, 1.6 times a
// second, dying away over half a minute — its voltmeter swinging (the steps keep a swing as it is).
RDE_INTERNAL void fzx_ringing(fzx* _x) {
    const u32 _bat = fzx_part(_x, "battery", -250, 0, 0, "5V");
    const u32 _sw  = fzx_part(_x, "SPST switch", -110, 120, 0, "on");
    const u32 _r   = fzx_part(_x, "resistor", 30, 120, 0, "1");
    const u32 _l   = fzx_part(_x, "inductor", 170, 120, 0, "10H");
    const u32 _cap = fzx_part(_x, "electrolytic", 300, 40, -90, "1000uF");
    const u32 _vm  = fzx_part(_x, "voltmeter", 410, 40, -90, "");
    fzx_wire(_x, _bat, 0, _sw, 0);
    fzx_wire(_x, _sw, 1, _r, 0);
    fzx_wire(_x, _r, 1, _l, 0);
    fzx_route(_x, _l, 1, _cap, 0, FZX_VIA({ 300, 120 }));
    fzx_route(_x, _cap, 0, _vm, 0, FZX_VIA({ 300, 90 }, { 410, 90 }));
    fzx_route(_x, _cap, 1, _vm, 1, FZX_VIA({ 300, -10 }, { 410, -10 }));
    fzx_route(_x, _cap, 1, _bat, 1, FZX_VIA({ 300, -60 }, { -250, -60 }));
    fzx_title(_x, -280, 230, FUDE_ZOOM_EXAMPLE_RINGING);
}

// --- displays (display.h) ---

// A small ground or supply rail (_volts NULL: a ground) turned to face a pin on its part's left, _gap from it: a wire
// straight between them.
RDE_INTERNAL void fzx_tie_left(fzx* _x, u32 _o, u32 _pin, f64 _gap, const c8* _volts) {
    const fude_zoom_v2 _p = fzx_pin(_x, _o, _pin);
    const u32 _t = _volts == NULL ? fzx_sized(_x, "ground", _p.x - _gap - 10.0, _p.y, -90.0, 20, 20, "")
                                  : fzx_sized(_x, "supply rail", _p.x - _gap - 6.0, _p.y, 90.0, 20, 12, _volts);
    fzx_line(_x, _o, _pin, _t, 0);
}

// A 74HC161 counting a clock of _hz, its middle at _cx, _cy: its supply; ~CLR and ENP high on its left, ENT and ~LOAD
// round its right to VCC (its A–D left open: it never loads). Its outputs (QA–QD: pins 13 down to 10) its caller's.
RDE_INTERNAL u32 fzx_counter_161(fzx* _x, f64 _cx, f64 _cy, const c8* _hz) {
    const u32 _cnt = fzx_part(_x, "74HC161", _cx, _cy, 0, "74HC161");
    const f64 _l = _cx - 100.0, _r = _cx + 100.0;
    const u32 _vr = fzx_part(_x, "supply rail", _r, _cy + 120.0, 0, "5V");
    fzx_line(_x, _cnt, 15u, _vr, 0);
    fzx_route(_x, _cnt, 9u, _cnt, 15u, FZX_VIA({ _r + 10.0, _cy - 50.0 }, { _r + 10.0, _cy + 70.0 }));
    fzx_route(_x, _cnt, 8u, _cnt, 15u, FZX_VIA({ _r + 20.0, _cy - 70.0 }, { _r + 20.0, _cy + 70.0 }));
    const u32 _cr = fzx_part(_x, "supply rail", _l - 40.0, _cy + 110.0, 0, "5V");
    fzx_route(_x, _cnt, 0u, _cr, 0, FZX_VIA({ _l - 40.0, _cy + 70.0 }));
    const u32 _er = fzx_part(_x, "supply rail", _l - 60.0, _cy - 30.0, 0, "5V");
    fzx_route(_x, _cnt, 6u, _er, 0, FZX_VIA({ _l - 60.0, _cy - 50.0 }));
    const u32 _g = fzx_part(_x, "ground", _l, _cy - 130.0, 0, "");
    fzx_line(_x, _cnt, 7u, _g, 0);
    const u32 _clk = fzx_part(_x, "clock", _l - 130.0, _cy, 0, _hz);
    const u32 _cg  = fzx_part(_x, "ground", _l - 130.0, _cy - 80.0, 0, "");
    fzx_route(_x, _clk, 0, _cnt, 1u, FZX_VIA({ _l - 80.0, _cy + 30.0 }, { _l - 80.0, _cy + 50.0 }));
    fzx_line(_x, _clk, 1, _cg, 0);
    return _cnt;
}

// A 2 Hz clock counted by a 74HC161, its four bits into a CD4511 (lamp test and blanking off on logic inputs: tap them;
// its latch open), its segments through 330 Ω into a 7-segment digit, its common grounded: 0 to 9, then six blank (the
// CD4511 shows nothing past 9), over again.
RDE_INTERNAL void fzx_digit_counter(fzx* _x) {
    const u32 _cnt = fzx_counter_161(_x, -420, 0, "2Hz");
    const u32 _dec = fzx_part(_x, "CD4511", 0, 100, 0, "CD4511");
    const u32 _dig = fzx_part(_x, "7-segment panel", 520, 110, 0, "1 digit red");
    // QA, QB, QC, QD (pins 13, 12, 11, 10) up into A, B, C, D (6, 0, 1, 5): each its own lane.
    fzx_route(_x, _cnt, 13u, _dec, 6u, FZX_VIA({ -270, 30 }, { -270, 50 }));
    fzx_route(_x, _cnt, 12u, _dec, 0u, FZX_VIA({ -250, 10 }, { -250, 170 }));
    fzx_route(_x, _cnt, 11u, _dec, 1u, FZX_VIA({ -230, -10 }, { -230, 150 }));
    fzx_route(_x, _cnt, 10u, _dec, 5u, FZX_VIA({ -290, -30 }, { -290, 70 }));
    // ~LT, ~BI high, LE low; its supply.
    static const c8* const _sets[3] = { "1", "1", "0" };
    for(u32 _k = 0; _k < 3u; _k++) {
        const fude_zoom_v2 _p = fzx_pin(_x, _dec, 2u + _k);
        const u32 _in = fzx_sized(_x, "logic input", _p.x - 40.0, _p.y, 0, 60, 16, _sets[_k]);
        fzx_line(_x, _in, 0, _dec, 2u + _k);
    }
    const u32 _dg = fzx_part(_x, "ground", -120, -10, 0, "");
    fzx_route(_x, _dec, 7u, _dg, 0, FZX_VIA({ -120, 30 }));
    const u32 _dv = fzx_part(_x, "supply rail", 100, 220, 0, "5V");
    fzx_line(_x, _dec, 15u, _dv, 0);
    // Its segments (a b c d e f g: pins 12, 11, 10, 9, 8, 14, 13) through a resistor each into the digit's (0–6).
    static const u32 _out[7] = { 12u, 11u, 10u, 9u, 8u, 14u, 13u };
    static const f64 _lane[7] = { 340, 360, 380, 400, 420, 320, 300 };
    for(u32 _g = 0; _g < 7u; _g++) {
        const fude_zoom_v2 _o = fzx_pin(_x, _dec, _out[_g]), _in = fzx_pin(_x, _dig, _g);
        const u32 _r = fzx_part(_x, "resistor", 250, _o.y, 0, "330");
        fzx_line(_x, _dec, _out[_g], _r, 0);
        fzx_route(_x, _r, 1, _dig, _g, FZX_VIA({ _lane[_g], _o.y }, { _lane[_g], _in.y }));
    }
    const fude_zoom_v2 _d1 = fzx_pin(_x, _dig, 8u);
    const u32 _gd = fzx_part(_x, "ground", _d1.x, _d1.y - 50.0, 0, "");
    fzx_line(_x, _dig, 8u, _gd, 0);
    fzx_title(_x, -700, 300, FUDE_ZOOM_EXAMPLE_DIGIT_COUNTER);
}

// A 500 Hz clock counted by a 74HC161, its three low bits into a 74HC138 whose outputs, low one at a time, are the rows
// of a 3 × 8 LED matrix (its rows cathodes); the same three bits through 470 Ω are its columns — row n lit as n is in
// binary, each an eighth of the time, all seen at once (each row its number: 0 at the top, 7 at the bottom).
RDE_INTERNAL void fzx_matrix_scan(fzx* _x) {
    const u32 _cnt = fzx_counter_161(_x, -420, 0, "500Hz");
    const u32 _dec = fzx_part(_x, "74HC138", 0, 0, 0, "74HC138");
    const u32 _mx  = fzx_part(_x, "LED matrix", 240, -30, 0, "3x8 CC red");
    // QA, QB, QC into A, B, C.
    fzx_route(_x, _cnt, 13u, _dec, 0u, FZX_VIA({ -240, 30 }, { -240, 70 }));
    fzx_route(_x, _cnt, 12u, _dec, 1u, FZX_VIA({ -220, 10 }, { -220, 50 }));
    fzx_route(_x, _cnt, 11u, _dec, 2u, FZX_VIA({ -200, -10 }, { -200, 30 }));
    // Enabled (~G2A, ~G2B low, G1 high: logic inputs to tap), its supply.
    static const c8* const _sets[3] = { "0", "0", "1" };
    for(u32 _k = 0; _k < 3u; _k++) {
        const fude_zoom_v2 _p = fzx_pin(_x, _dec, 3u + _k);
        const u32 _in = fzx_sized(_x, "logic input", _p.x - 40.0, _p.y, 0, 60, 16, _sets[_k]);
        fzx_line(_x, _in, 0, _dec, 3u + _k);
    }
    const u32 _dv = fzx_part(_x, "supply rail", 100, 120, 0, "5V");
    fzx_line(_x, _dec, 15u, _dv, 0);
    const u32 _dg = fzx_part(_x, "ground", -150, -110, 0, "");
    fzx_route(_x, _dec, 7u, _dg, 0, FZX_VIA({ -150, -70 }));
    // ~Y0–~Y6 (pins 14 down to 8) straight across to rows 1–7; ~Y7 (pin 6, its left) round under it to row 8.
    for(u32 _r = 0; _r < 7u; _r++) {
        fzx_line(_x, _dec, 14u - _r, _mx, _r);
    }
    fzx_route(_x, _dec, 6u, _mx, 7u, FZX_VIA({ -125, -50 }, { -125, -115 }, { 160, -115 }, { 160, -90 }));
    // Its columns (pins 8, 9, 10: anodes) down through 470 Ω each, back under it all to QA, QB, QC.
    for(u32 _c = 0; _c < 3u; _c++) {
        const fude_zoom_v2 _p = fzx_pin(_x, _mx, 8u + _c);
        const u32 _rr = fzx_part(_x, "resistor", _p.x, -190.0 - 60.0 * (f64)_c, 90.0, "470");
        fzx_line(_x, _mx, 8u + _c, _rr, 1);
        const fude_zoom_v2 _b = fzx_pin(_x, _rr, 0), _q = fzx_pin(_x, _cnt, 13u - _c);
        const f64 _low = -400.0 - 20.0 * (f64)_c, _lane = -290.0 + 10.0 * (f64)_c;
        fzx_route(_x, _rr, 0, _cnt, 13u - _c, FZX_VIA({ _b.x, _low }, { _lane, _low }, { _lane, _q.y }));
    }
    fzx_title(_x, -700, 260, FUDE_ZOOM_EXAMPLE_MATRIX_SCAN);
}

// An LM3914 lighting a bar graph (turned: its first LED at the bottom) as far up as SIG is of its reference (1.25 V on
// RHI, RLO grounded: steps of 125 mV) — SIG a pot's wiper across that reference (tap it: 65 %, six lit); a bar (MODE on
// V+); its LEDs' current ten times what REF OUT gives (R1 1.2 kΩ and the pot: about 13 mA).
RDE_INTERNAL void fzx_bar_meter(fzx* _x) {
    const u32 _lm  = fzx_part(_x, "LM3914", 0, 0, 0, "LM3914");
    const u32 _bar = fzx_part(_x, "LED bar graph", 320, 0, 180.0, "red");
    fzx_tie_left(_x, _lm, 1u, 20.0, NULL);    // V−
    fzx_tie_left(_x, _lm, 2u, 20.0, "5V");    // V+
    fzx_tie_left(_x, _lm, 3u, 20.0, NULL);    // RLO
    fzx_tie_left(_x, _lm, 7u, 20.0, NULL);    // REF ADJ
    fzx_tie_left(_x, _lm, 8u, 20.0, "5V");    // MODE: a bar
    fzx_route(_x, _lm, 5u, _lm, 6u, FZX_VIA({ -112, -20 }, { -112, -40 }));   // (RHI on REF OUT)
    const u32 _r1 = fzx_sized(_x, "resistor", -160, -40, 0, 40, 12, "1.2k");
    fzx_line(_x, _lm, 6u, _r1, 1);
    fzx_tie_left(_x, _r1, 0, 10.0, NULL);
    const u32 _pot = fzx_part(_x, "potentiometer", -260, -20, 0, "10k 65%");
    fzx_line(_x, _pot, 2, _lm, 4u);                                   // (its wiper on SIG)
    fzx_route(_x, _pot, 1, _lm, 5u, FZX_VIA({ -230, -20 }));           // (across the reference)
    fzx_tie_left(_x, _pot, 0, 10.0, NULL);
    // LED1 (its left) over the top to the bar's first cathode; LED2–LED10 (its right, top down) across to theirs (the
    // bar's bottom up), each its own lane.
    fzx_route(_x, _lm, 0u, _bar, 10u, FZX_VIA({ -120, 80 }, { -120, 130 }, { 235, 130 }, { 235, -90 }));
    for(u32 _n = 2u; _n <= 10u; _n++) {
        const f64 _lane = 120.0 + 12.0 * (f64)(_n - 2u);
        fzx_route(_x, _lm, 19u - _n, _bar, 9u + _n, FZX_VIA({ _lane, 120.0 - 20.0 * (f64)_n }, { _lane, 20.0 * (f64)_n - 110.0 }));
    }
    // Its anodes one to the next, up to 5 V.
    for(u32 _k = 0; _k + 1u < 10u; _k++) {
        fzx_line(_x, _bar, _k, _bar, _k + 1u);
    }
    const u32 _ar = fzx_part(_x, "supply rail", 390, 140, 0, "5V");
    fzx_line(_x, _bar, 9u, _ar, 0);
    fzx_title(_x, -320, 240, FUDE_ZOOM_EXAMPLE_BAR_METER);
}

// A 9 V battery: a 10 kΩ pot across it, a 20 V panel meter on its wiper (4.50 V: tap the pot); a 20 mA one in a row
// with 1 kΩ (8.91 mA: its shunt 10 Ω, the battery's own 0.5 Ω).
RDE_INTERNAL void fzx_panel_meters(fzx* _x) {
    const u32 _bat = fzx_part(_x, "battery", -300, 0, 0, "9V");
    const u32 _pot = fzx_part(_x, "potentiometer", -120, 100, 0, "10k 50%");
    const u32 _vm  = fzx_part(_x, "panel meter", 150, 160, 0, "20V");
    const u32 _am  = fzx_part(_x, "panel meter", 0, -120, 0, "20mA");
    const u32 _r   = fzx_part(_x, "resistor", 200, -120, 0, "1k");
    fzx_route(_x, _bat, 0, _pot, 0, FZX_VIA({ -300, 90 }));
    fzx_route(_x, _pot, 1, _bat, 1, FZX_VIA({ -60, 90 }, { -60, -60 }, { -300, -60 }));
    fzx_route(_x, _pot, 2, _vm, 0, FZX_VIA({ -120, 160 }));
    fzx_route(_x, _vm, 1, _bat, 1, FZX_VIA({ 260, 160 }, { 260, -180 }, { -300, -180 }));
    fzx_route(_x, _bat, 0, _am, 0, FZX_VIA({ -340, 30 }, { -340, -120 }));
    fzx_line(_x, _am, 1, _r, 0);
    fzx_route(_x, _r, 1, _bat, 1, FZX_VIA({ 230, -160 }, { -300, -160 }));
    fzx_title(_x, -340, 260, FUDE_ZOOM_EXAMPLE_PANEL_METERS);
}

// A 16 × 2 LCD written by hand: RS, RW and D0–D7 on logic inputs over its pins (tap to set them), E on a push button
// (pulled down: pressed and let go, what is set is sent — D0–D3 on first, the display on), V0 grounded (full contrast),
// its backlight on 5 V.
RDE_INTERNAL void fzx_lcd_by_hand(fzx* _x) {
    const u32 _lcd = fzx_part(_x, "character LCD", 0, 0, 0, "16x2");
    const u32 _gs = fzx_part(_x, "ground", -200, 40, 0, "");
    fzx_route(_x, _lcd, 0, _gs, 0, FZX_VIA({ -150, 85 }, { -200, 85 }));            // VSS
    fzx_route(_x, _lcd, 2, _lcd, 0, FZX_VIA({ -110, 75 }, { -150, 75 }));          // V0 on VSS
    const u32 _vd = fzx_part(_x, "supply rail", -130, 122, 0, "5V");
    fzx_line(_x, _lcd, 1, _vd, 0);                                                 // VDD
    static const u32 _set[10] = { 3, 4, 6, 7, 8, 9, 10, 11, 12, 13 };               // (RS RW D0 … D7)
    static const c8* const _val[10] = { "0", "0", "1", "1", "1", "1", "0", "0", "0", "0" };
    for(u32 _k = 0; _k < 10u; _k++) {
        const fude_zoom_v2 _p = fzx_pin(_x, _lcd, _set[_k]);
        const u32 _in = fzx_sized(_x, "logic input", _p.x, _p.y + 73.0, -90.0, 36, 16, _val[_k]);
        fzx_line(_x, _lcd, _set[_k], _in, 0);
    }
    const u32 _bt = fzx_part(_x, "push button", -290, 100, 0, "E");
    fzx_route(_x, _lcd, 5, _bt, 1, FZX_VIA({ -50, 100 }));
    const u32 _br = fzx_part(_x, "supply rail", -340, 120, 0, "5V");
    fzx_route(_x, _bt, 0, _br, 0, FZX_VIA({ -340, 100 }));
    const u32 _pd = fzx_part(_x, "resistor", -260, 60, 90.0, "10k");
    fzx_line(_x, _bt, 1, _pd, 1);
    const u32 _pg = fzx_part(_x, "ground", -260, -10, 0, "");
    fzx_line(_x, _pd, 0, _pg, 0);
    const u32 _ar = fzx_sized(_x, "supply rail", 130, 170, 0, 30, 14, "5V");
    fzx_line(_x, _lcd, 14, _ar, 0);                                                // A
    const u32 _gk = fzx_part(_x, "ground", 200, 40, 0, "");
    fzx_route(_x, _lcd, 15, _gk, 0, FZX_VIA({ 150, 85 }, { 200, 85 }));            // K
    fzx_word(_x, -340, -90, 16.0, 560.0, FUDE_ZOOM_EXAMPLE_WORD_LCD_STEPS);
    fzx_title(_x, -340, 240, FUDE_ZOOM_EXAMPLE_LCD_BY_HAND);
}

// One full adder of gates with its inputs left of it at _y: A, B (their texts), its carry in from _cin's pin _cin_pin
// (FUDE_ZOOM_NONE: an input of its own, its text _ci); its sum into a probe. Its carry out: *_cout's pin 2 (its OR).
RDE_INTERNAL void fzx_full_adder_at(fzx* _x, f64 _y, const c8* _a_text, const c8* _b_text, u32 _cin, u32 _cin_pin, const c8* _ci, const c8* _sum, u32* _cout) {
    const u32 _a  = fzx_part(_x, "logic input", -430, _y + 40.0, 0, _a_text);
    const u32 _b  = fzx_part(_x, "logic input", -430, _y - 40.0, 0, _b_text);
    if(_cin == FUDE_ZOOM_NONE) {
        _cin     = fzx_part(_x, "logic input", -430, _y + 120.0, 0, _ci);
        _cin_pin = 0u;
    }
    const u32 _x1 = fzx_part(_x, "XOR gate", -250, _y + 30.0, 0, "");
    const u32 _a1 = fzx_part(_x, "AND gate", -250, _y - 70.0, 0, "");
    const u32 _x2 = fzx_part(_x, "XOR gate", -50, _y + 10.0, 0, "");
    const u32 _a2 = fzx_part(_x, "AND gate", -50, _y - 90.0, 0, "");
    const u32 _o1 = fzx_part(_x, "OR gate", 130, _y - 80.0, 0, "");
    const u32 _s  = fzx_part(_x, "logic probe", 300, _y + 10.0, 0, _sum);
    fzx_wire(_x, _a, 0, _x1, 0);
    fzx_wire(_x, _b, 0, _x1, 1);
    fzx_wire(_x, _a, 0, _a1, 0);
    fzx_wire(_x, _b, 0, _a1, 1);
    fzx_wire(_x, _x1, 2, _x2, 0);
    fzx_wire(_x, _cin, _cin_pin, _x2, 1);
    fzx_wire(_x, _x1, 2, _a2, 0);
    fzx_wire(_x, _cin, _cin_pin, _a2, 1);
    fzx_wire(_x, _a1, 2, _o1, 0);
    fzx_wire(_x, _a2, 2, _o1, 1);
    fzx_wire(_x, _x2, 2, _s, 0);
    *_cout = _o1;
}

RDE_INTERNAL void fzx_full_adder(fzx* _x) {
    u32 _co;
    fzx_full_adder_at(_x, 0.0, "1", "1", FUDE_ZOOM_NONE, 0u, "0", "S", &_co);
    const u32 _cp = fzx_part(_x, "logic probe", 300, -80.0, 0, "CO");
    fzx_wire(_x, _co, 2, _cp, 0);
    fzx_title(_x, -430, 230, FUDE_ZOOM_EXAMPLE_FULL_ADDER);
}

// Four of them, the carry along (A 0101 = 5, B 0011 = 3: S 1000 = 8, no carry out).
RDE_INTERNAL void fzx_adder_4(fzx* _x) {
    static const c8* const _a[4] = { "1", "0", "1", "0" }, *const _b[4] = { "1", "1", "0", "0" }, *const _s[4] = { "S0", "S1", "S2", "S3" };
    u32 _carry = FUDE_ZOOM_NONE;
    for(u32 _i = 0; _i < 4u; _i++) {
        u32 _co;
        fzx_full_adder_at(_x, 330.0 - (f64)_i * 260.0, _a[_i], _b[_i], _carry, 2u, "0", _s[_i], &_co);
        _carry = _co;
    }
    const u32 _cp = fzx_part(_x, "logic probe", 300, 330.0 - 3.0 * 260.0 - 160.0, 0, "C4");
    fzx_wire(_x, _carry, 2, _cp, 0);
    fzx_title(_x, -430, 500, FUDE_ZOOM_EXAMPLE_ADDER_4);
    fzx_text(_x, -430, 455, 18.0, 520.0, "A = 0101 (5), B = 0011 (3)");
}

// A 74HC161 seated across a breadboard's gap, its supply on the rails, a 2 Hz clock into it, four LEDs on its outputs.
RDE_INTERNAL void fzx_counter(fzx* _x) {
    const u32 _bb = fzx_part(_x, "breadboard", 0, 0, 0, "");
    const u32 _cols = FUDE_ZOOM_BREADBOARD_COLS, _col = 6u;
    const fude_zoom_v2 _e0 = fzx_pin(_x, _bb, 6u * _cols + _col), _f0 = fzx_pin(_x, _bb, 7u * _cols + _col);
    const fude_zoom_v2 _f1 = fzx_pin(_x, _bb, 7u * _cols + _col + 1u);
    const f64 _pitch = _f1.x - _f0.x;
    // (its sides' pins across the gap: its width the rows' gap; its length nine holes, its pins a hole apart)
    fzx_sized(_x, "74HC161", _f0.x + _pitch * 3.5, (_e0.y + _f0.y) * 0.5, 90.0, _e0.y - _f0.y, _pitch * 9.0, "74HC161");
    const fude_zoom_v2 _top = fzx_pin(_x, _bb, 2u);
    const u32 _bat = fzx_part(_x, "battery", _top.x - 120.0, _top.y + 110.0, 0, "5V");
    fzx_line(_x, _bat, 0, _bb, 0u * _cols + 2u);
    fzx_line(_x, _bat, 1, _bb, 1u * _cols + 2u);
    fzx_line(_x, _bb, 0u * _cols + 1u, _bb, 12u * _cols + 1u);
    fzx_line(_x, _bb, 1u * _cols + 28u, _bb, 13u * _cols + 28u);
    // (~CLR 1, ENP 7, ~LOAD 9, ENT 10, VCC 16 to plus; A B C D 3–6 and GND 8 to minus — each along its column)
    const u32 _hi[5] = { 0u, 6u, 8u, 9u, 15u }, _lo[5] = { 2u, 3u, 4u, 5u, 7u };
    for(u32 _k = 0; _k < 5u; _k++) {
        const u32 _ch = _hi[_k] < 8u ? _col + _hi[_k] : _col + 15u - _hi[_k];
        const u32 _sh = _hi[_k] < 8u ? 10u * _cols + _ch : 3u * _cols + _ch;
        fzx_line(_x, _bb, _sh, _bb, (_hi[_k] < 8u ? 12u : 0u) * _cols + _ch);
        fzx_line(_x, _bb, 10u * _cols + _col + _lo[_k], _bb, 13u * _cols + _col + _lo[_k]);
    }
    const fude_zoom_v2 _ck = fzx_pin(_x, _bb, 10u * _cols + _col + 1u);
    const u32 _clk = fzx_part(_x, "clock", _ck.x, _ck.y - 150.0, 0, "2Hz");
    fzx_line(_x, _clk, 0, _bb, 10u * _cols + _col + 1u);
    fzx_line(_x, _clk, 1, _bb, 13u * _cols + 22u);
    for(u32 _i = 0; _i < 4u; _i++) {
        const u32 _k = 13u - _i;   // (QA, QB, QC, QD)
        const fude_zoom_v2 _h = fzx_pin(_x, _bb, 3u * _cols + _col + 15u - _k);
        const u32 _r = fzx_part(_x, "resistor", _h.x + 40.0, _top.y + 70.0 + (f64)_i * 42.0, 0, "330");
        const u32 _led = fzx_part(_x, "LED", _h.x + 140.0, _top.y + 70.0 + (f64)_i * 42.0, 0, "red");
        fzx_line(_x, _bb, 3u * _cols + _col + 15u - _k, _r, 0);
        fzx_line(_x, _r, 1, _led, 0);
        fzx_line(_x, _led, 1, _bb, 1u * _cols + 20u + _i);
    }
    fzx_title(_x, -170, _top.y + 280.0, FUDE_ZOOM_EXAMPLE_COUNTER);
}

// A chip's supply: VCC (pin _vcc) to a 5 V rail above it, GND (_gnd) to a ground below.
RDE_INTERNAL void fzx_supply(fzx* _x, u32 _chip, u32 _vcc, u32 _gnd) {
    const fude_zoom_v2 _v = fzx_pin(_x, _chip, _vcc), _g = fzx_pin(_x, _chip, _gnd);
    const u32 _rail = fzx_part(_x, "supply rail", _v.x + (_v.x > 0.0 ? 60.0 : -60.0), _v.y + 70.0, 0, "5V");
    const u32 _gr   = fzx_part(_x, "ground", _g.x + (_g.x > 0.0 ? 60.0 : -60.0), _g.y - 70.0, 0, "");
    fzx_wire(_x, _chip, _vcc, _rail, 0);
    fzx_wire(_x, _chip, _gnd, _gr, 0);
}

// A 74HC283's two nibbles from switches (A 0110 = 6, B 0111 = 7, no carry in): its sum on probes (1101 = 13).
RDE_INTERNAL void fzx_adder_chip(fzx* _x) {
    const u32 _ic = fzx_part(_x, "74HC283", 0, 0, 0, "74HC283");
    // (its pins: 0 S2, 1 B2, 2 A2, 3 S1, 4 A1, 5 B1, 6 C0, 7 GND, 8 C4, 9 S4, 10 B4, 11 A4, 12 S3, 13 A3, 14 B3, 15 VCC)
    static const c8* const _names[16] = { "S2", "B2", "A2", "S1", "A1", "B1", "C0", "", "C4", "S4", "B4", "A4", "S3", "A3", "B3", "" };
    static const c8* const _set[16]   = { "", "1", "1", "", "0", "1", "0", "", "", "", "0", "0", "", "1", "1", "" };
    for(u32 _k = 0; _k < 16u; _k++) {
        if(_names[_k][0] == 0) {
            continue;
        }
        const fude_zoom_v2 _p = fzx_pin(_x, _ic, _k);
        const b8 _left = _p.x < 0.0, _out = _names[_k][0] == 'S' || _k == 8u;
        const f64 _dx = _left ? -150.0 : 150.0;
        // (an input's pin on its right, a probe's on its left: turned over where they face the other way)
        const u32 _o = _out ? fzx_part(_x, "logic probe", _p.x + _dx, _p.y, _left ? 180.0 : 0.0, _names[_k])
                            : fzx_part(_x, "logic input", _p.x + _dx, _p.y, _left ? 0.0 : 180.0, _set[_k]);
        fzx_text(_x, _p.x + _dx * 1.55 - 18.0, _p.y + 11.0, 15.0, 40.0, _names[_k]);
        fzx_wire(_x, _ic, _k, _o, 0);
    }
    fzx_supply(_x, _ic, 15u, 7u);
    fzx_title(_x, -330, 290, FUDE_ZOOM_EXAMPLE_ADDER_CHIP);
    fzx_text(_x, -330, 250, 18.0, 520.0, "A = 0110 (6), B = 0111 (7)");
}

// A 4 Hz clock counted by a 74HC161, its three low bits decoded by a 74HC138: each of its outputs, low in turn, lights
// its LED (from a 5 V rail through a resistor into the output).
RDE_INTERNAL void fzx_chaser(fzx* _x) {
    static const c8* const _colours[8] = { "red", "orange", "yellow", "green", "blue", "purple", "pink", "white" };
    const u32 _cnt = fzx_part(_x, "74HC161", -300, 0, 0, "74HC161");
    const u32 _dec = fzx_part(_x, "74HC138", 120, 0, 0, "74HC138");
    fzx_supply(_x, _cnt, 15u, 7u);
    fzx_supply(_x, _dec, 15u, 7u);
    // The counter: cleared never, loaded never, counting always (~CLR, ENP, ~LOAD, ENT high); its A–D low.
    const u32 _hi[4] = { 0u, 6u, 8u, 9u };
    for(u32 _k = 0; _k < 4u; _k++) {
        const fude_zoom_v2 _p = fzx_pin(_x, _cnt, _hi[_k]);
        const u32 _r = fzx_part(_x, "supply rail", _p.x + (_p.x < -300.0 ? -70.0 : 70.0), _p.y + 30.0, 0, "5V");
        fzx_wire(_x, _cnt, _hi[_k], _r, 0);
    }
    for(u32 _k = 2u; _k <= 5u; _k++) {
        const fude_zoom_v2 _p = fzx_pin(_x, _cnt, _k);
        const u32 _g = fzx_part(_x, "ground", _p.x - 150.0, _p.y - 15.0, -90.0, "");
        fzx_wire(_x, _cnt, _k, _g, 0);
    }
    const fude_zoom_v2 _ck = fzx_pin(_x, _cnt, 1u);
    const u32 _clk = fzx_part(_x, "clock", _ck.x - 230.0, _ck.y + 80.0, 0, "4Hz");
    const u32 _cg  = fzx_part(_x, "ground", _ck.x - 230.0, _ck.y - 20.0, 0, "");
    fzx_wire(_x, _clk, 0, _cnt, 1u);
    fzx_wire(_x, _clk, 1, _cg, 0);
    // QA, QB, QC into A, B, C; enabled (G1 high, ~G2A and ~G2B low).
    fzx_wire(_x, _cnt, 13u, _dec, 0u);
    fzx_wire(_x, _cnt, 12u, _dec, 1u);
    fzx_wire(_x, _cnt, 11u, _dec, 2u);
    for(u32 _k = 3u; _k <= 4u; _k++) {
        const fude_zoom_v2 _p = fzx_pin(_x, _dec, _k);
        const u32 _g = fzx_part(_x, "ground", _p.x - 60.0, _p.y - 40.0, 0, "");
        fzx_wire(_x, _dec, _k, _g, 0);
    }
    {
        const fude_zoom_v2 _p = fzx_pin(_x, _dec, 5u);
        const u32 _r = fzx_part(_x, "supply rail", _p.x - 60.0, _p.y - 50.0, 0, "5V");
        fzx_wire(_x, _dec, 5u, _r, 0);
    }
    // ~Y0 … ~Y7: pins 14 down to 8, then 6.
    const u32 _y[8] = { 14u, 13u, 12u, 11u, 10u, 9u, 8u, 6u };
    for(u32 _k = 0; _k < 8u; _k++) {
        const f64 _ly = 330.0 - (f64)_k * 90.0;
        const u32 _led = fzx_part(_x, "LED", 450, _ly, 180.0, _colours[_k]);   // (its cathode toward the chip)
        const u32 _r   = fzx_part(_x, "resistor", 600, _ly, 0, "330");
        const u32 _rl  = fzx_part(_x, "supply rail", 720, _ly + 40.0, 0, "5V");
        fzx_wire(_x, _dec, _y[_k], _led, 1);
        fzx_wire(_x, _led, 0, _r, 0);
        fzx_wire(_x, _r, 1, _rl, 0);
    }
    fzx_title(_x, -560, 470, FUDE_ZOOM_EXAMPLE_CHASER);
}

// --- mechanisms ----------------------------------------------------------------------------------------------------

RDE_INTERNAL void fzx_crank(fzx* _x) {
    // Ground 110 (motor at -55, pivot at 55), crank 40, coupler 120, rocker 100 (Grashof: the crank turns round).
    fzx_part(_x, "drive motor", -55, 0, 0, "30 rpm");
    fzx_part(_x, "fixed pivot", 55, -10, 0, "");
    fzx_sized(_x, "link", -35, 0, 0, 40 + 24, 24, "");
    fzx_sized(_x, "link", (-15 + 51.43) * 0.5, 99.9 * 0.5, atan2(99.9, 66.43) / FZX_DEG, 120 + 24, 24, "");
    fzx_sized(_x, "link", (55 + 51.43) * 0.5, 99.9 * 0.5, atan2(99.9, -3.57) / FZX_DEG, 100 + 24, 24, "");
    fzx_title(_x, -200, 230, FUDE_ZOOM_EXAMPLE_CRANK);
}

RDE_INTERNAL void fzx_gear_train(fzx* _x) {
    fzx_part(_x, "drive motor", -100, 0, 0, "20 rpm");
    fzx_part(_x, "gear 20T", -100, 0, 0, "");
    fzx_part(_x, "gear 40T", 50, 0, 0, "");
    fzx_part(_x, "gear 10T", 50, 125, 0, "");
    fzx_title(_x, -200, 260, FUDE_ZOOM_EXAMPLE_GEAR_TRAIN);
}

RDE_INTERNAL void fzx_pendulum(fzx* _x) {
    fzx_part(_x, "fixed pivot", -100, 140, 0, "");
    const f64 _a = -45.0 * FZX_DEG;   // (let go from 45°)
    fzx_sized(_x, "link", -100 + sin(-_a) * 48, 150 - cos(_a) * 48, -45, 120, 24, "");
    fzx_part(_x, "weight", -100 + sin(-_a) * 96, 150 - cos(_a) * 96, 0, "1 kg");
    fzx_part(_x, "fixed pivot", 150, 140, 0, "");
    fzx_part(_x, "spring", 150, 150 - 45, -90, "");
    fzx_part(_x, "weight", 150, 150 - 90, 0, "2 kg");
    fzx_sized(_x, "wall", 0, -120, 0, 600, 20, "");
    fzx_title(_x, -280, 260, FUDE_ZOOM_EXAMPLE_PENDULUM);
}

RDE_INTERNAL void fzx_pulleys(fzx* _x) {
    // A pulley (its groove 31 round), a rope down each side: a 1 kg weight on one, a 2 kg crate on the other; and a
    // rope pendulum from a pivot.
    fzx_sized(_x, "pulley", 0, 100, 0, 80, 80, "");
    fzx_sized(_x, "rope", -31.2, 25, 90, 158, 12, "");
    fzx_sized(_x, "rope", 31.2, 25, 90, 158, 12, "");
    fzx_part(_x, "weight", -31.2, -62, 0, "1 kg");
    fzx_part(_x, "crate", 31.2, -68, 0, "2 kg");
    fzx_sized(_x, "wall", 0, -220, 0, 400, 20, "");
    fzx_part(_x, "fixed pivot", 150, 90, 0, "");
    fzx_sized(_x, "rope", 185, 60, atan2(-80.0, 70.0) / FZX_DEG, 106.3 / 0.95, 12, "");
    fzx_part(_x, "weight", 220, 20, 0, "1 kg");
    fzx_title(_x, -200, 230, FUDE_ZOOM_EXAMPLE_PULLEYS);
}

RDE_INTERNAL void fzx_piston(fzx* _x) {
    // A motor turning a crank 40 long, a rod 140 to a slider on a rail.
    fzx_part(_x, "drive motor", -120, 0, 0, "60 rpm");
    fzx_sized(_x, "link", -120 + 20, 0, 0, 40 + 24, 24, "");
    fzx_sized(_x, "link", -80 + 70, 0, 0, 140 + 24, 24, "");
    fzx_sized(_x, "rail", 100, 0, 0, 300, 16, "");
    fzx_part(_x, "slider", 60, 0, 0, "");
    fzx_title(_x, -200, 160, FUDE_ZOOM_EXAMPLE_PISTON);
}

RDE_INTERNAL void fzx_rack(fzx* _x) {
    // A 20-tooth gear (its pitch circle 50 round) on a motor, a rack under it.
    fzx_part(_x, "drive motor", 0, 60, 0, "10 rpm");
    fzx_part(_x, "gear 20T", 0, 60, 0, "");
    fzx_sized(_x, "rack", 0, 60 - 50 - 5, 0, 300, 30, "");
    fzx_title(_x, -160, 220, FUDE_ZOOM_EXAMPLE_RACK);
}

RDE_INTERNAL void fzx_gears_rack(fzx* _x) {
    // A motor turning a gear, three more on in an L (each drawn turned anyhow), a rack turned over on the last.
    fzx_part(_x, "drive motor", -150, -50, 0, "30 rpm");
    fzx_part(_x, "gear 20T", -150, -50, 17, "");
    fzx_part(_x, "gear 20T", -50, -50, 63, "");
    fzx_part(_x, "gear 20T", 50, -50, -23, "");
    fzx_part(_x, "gear 20T", 50, 50, 115, "");
    fzx_sized(_x, "rack", 110, 105, 180, 400, 30, "");
    fzx_title(_x, -230, 220, FUDE_ZOOM_EXAMPLE_GEARS_RACK);
}

RDE_INTERNAL void fzx_bodies(fzx* _x) {
    // A ramp drawn as a line (the ground), a pen-drawn triangle of wood, a steel ball, a rubber L, and a bar pinned at
    // one end (a pendulum).
    const fude_zoom_v2 _ramp[4] = { { -330, 130 }, { -40, -60 }, { 330, -60 }, { 330, 10 } };
    const fude_zoom_v2 _tri[3]  = { { -205, 175 }, { -150, 175 }, { -178, 225 } };
    const fude_zoom_v2 _ell[6]  = { { 150, 110 }, { 230, 110 }, { 230, 130 }, { 175, 130 }, { 175, 170 }, { 150, 170 } };
    u32 _bodies[5];
    _bodies[0] = fzx_path(_x, _ramp, 4u, false, 4.0);
    _bodies[1] = fzx_path(_x, _tri, 3u, true, 4.0);
    _bodies[2] = fzx_path(_x, _ell, 6u, true, 4.0);
    const f64 _ball[2] = { 26.0 * _x->p.u, 26.0 * _x->p.u };
    _bodies[3] = fude_zoom_scene_add_shape(_x->p.s, _x->p.frame, (fude_zoom_place){ fzx_at(_x, -265, 200), 0.0, 1.0 }, FUDE_ZOOM_SHAPE_ELLIPSE, _ball, 2u, _x->p.ink,
                                           (f32)(1.5 * _x->p.u), 0, 0);
    const f64 _bar[3] = { 70.0 * _x->p.u, 8.0 * _x->p.u, 0.0 };
    _bodies[4] = fude_zoom_scene_add_shape(_x->p.s, _x->p.frame, (fude_zoom_place){ fzx_at(_x, 40, 240), 0.0, 1.0 }, FUDE_ZOOM_SHAPE_RECT, _bar, 3u, _x->p.ink,
                                           (f32)(1.5 * _x->p.u), 0, 0);
    rde_arr_add(_x->p.born, (any)&_bodies[3]);
    rde_arr_add(_x->p.born, (any)&_bodies[4]);
    const u32 _material[5] = { 0u, 0u, 3u, 1u, 2u };   // (the ramp's: it is fixed whatever; wood, rubber, steel, aluminium)
    for(u32 _i = 0; _i < 5u; _i++) {
        if(_bodies[_i] == FUDE_ZOOM_NONE) {
            continue;
        }
        const fude_zoom_body_props _bp = { _material[_i], _i == 0u, 0.0, -1.0, -1.0 };
        const u32 _made = fude_zoom_props_add_body(_x->p.s, _bodies[_i], &_bp);
        rde_arr_add(_x->p.born, (any)&_made);
    }
    fzx_part(_x, "pin", -26, 240, 0, "");
    fzx_title(_x, -330, 320, FUDE_ZOOM_EXAMPLE_BODIES);
}

// Four blocks alike — ice, wood, rubber, steel — let go on four wooden ramps alike (31°: ice slides away, steel and wood
// after it more slowly, rubber holds where it is); three balls of rubber, glass, foam dropped on an ice floor (rubber
// bounces back up, glass and foam hardly). Each its material's name by it.
RDE_INTERNAL void fzx_materials(fzx* _x) {
    static const u32 _blocks[4] = { 6u, 0u, 3u, 1u };   // (sim/body.h's: ice, wood, rubber, steel)
    const f64 _dx = 250.0 / hypot(250.0, 150.0), _dy = -150.0 / hypot(250.0, 150.0);   // (down the slope)
    for(u32 _k = 0; _k < 4u; _k++) {
        const f64 _o = 330.0 - 160.0 * (f64)_k;
        const fude_zoom_v2 _ramp[4] = { { -420, _o + 150 }, { -170, _o }, { 330, _o }, { 330, _o + 40 } };
        fzx_drawn(_x, _ramp, 4u, false, 0u, true);
        // (a block 36 a side on the slope, 40 down from its top, a point off it)
        const fude_zoom_v2 _p0 = { -420.0 + _dx * 40.0 - _dy * 1.0, _o + 150.0 + _dy * 40.0 + _dx * 1.0 };
        const fude_zoom_v2 _sq[4] = { _p0, { _p0.x + _dx * 36.0, _p0.y + _dy * 36.0 }, { _p0.x + _dx * 36.0 - _dy * 36.0, _p0.y + _dy * 36.0 + _dx * 36.0 },
                                     { _p0.x - _dy * 36.0, _p0.y + _dx * 36.0 } };
        fzx_drawn(_x, _sq, 4u, true, _blocks[_k], false);
        fzx_word(_x, -570, _o + 190, 20.0, 150.0, FUDE_ZOOM_EXAMPLE_WORD_MATERIAL + _blocks[_k]);
    }
    static const u32 _balls[3] = { 3u, 5u, 7u };   // (rubber, glass, foam)
    const fude_zoom_v2 _floor[4] = { { 450, -250 }, { 450, -330 }, { 900, -330 }, { 900, -250 } };   // (a tray: a ball that rolls stays on it)
    fzx_drawn(_x, _floor, 4u, false, 6u, true);
    for(u32 _k = 0; _k < 3u; _k++) {
        const f64 _bx = 520.0 + 160.0 * (f64)_k;
        fzx_ball(_x, _bx, -100, 22, _balls[_k]);
        fzx_word(_x, _bx - 45, -35, 20.0, 150.0, FUDE_ZOOM_EXAMPLE_WORD_MATERIAL + _balls[_k]);
    }
    fzx_title(_x, -560, 600, FUDE_ZOOM_EXAMPLE_MATERIALS);
}

// Weights on links hung from pivots — 1 kg, 50 kg (490 N: its pin takes 980), 500 kg (4.9 kN: its pin shears, the link
// and the weight fall) — and on ropes — 100 kg (a 2 kN rope holds 980 N), 300 kg (2.9 kN: it snaps). A floor below.
RDE_INTERNAL void fzx_too_heavy(fzx* _x) {
    static const c8* const _links[3] = { "1 kg", "50 kg", "500 kg" };
    for(u32 _k = 0; _k < 3u; _k++) {
        const f64 _px = -420.0 + 160.0 * (f64)_k;
        fzx_part(_x, "fixed pivot", _px, 140, 0, "");   // (its hole 10 over its middle)
        fzx_sized(_x, "link", _px, 150 - 48, -90, 120, 24, "");
        fzx_part(_x, "weight", _px, 150 - 96, 0, _links[_k]);
    }
    static const c8* const _ropes[2] = { "100 kg", "300 kg" };
    for(u32 _k = 0; _k < 2u; _k++) {
        const f64 _px = 80.0 + 180.0 * (f64)_k;
        fzx_part(_x, "fixed pivot", _px, 140, 0, "");
        fzx_sized(_x, "rope", _px, 95, -90, 110.0 / 0.95, 12, "");
        fzx_part(_x, "weight", _px, 40, 0, _ropes[_k]);
    }
    fzx_sized(_x, "wall", -80, -200, 0, 760, 20, "");
    fzx_title(_x, -460, 300, FUDE_ZOOM_EXAMPLE_TOO_HEAVY);
}

// 10 kg at the end of an arm (126 mm) on a drive motor: one of 2 N·m (12.4 needed to lift it out level: it hangs, the
// motor held still — jammed, said); one of 30 N·m swinging it round and round.
RDE_INTERNAL void fzx_motor_torque(fzx* _x) {
    for(u32 _k = 0; _k < 2u; _k++) {
        const f64 _px = _k == 0u ? -250.0 : 200.0;
        fzx_part(_x, "drive motor", _px, 0, 0, _k == 0u ? "30 rpm 2Nm" : "30 rpm 30Nm");
        fzx_sized(_x, "link", _px + 63, 0, 0, 150, 24, "");
        fzx_part(_x, "weight", _px + 126, 0, 0, "10 kg");
    }
    fzx_title(_x, -300, 220, FUDE_ZOOM_EXAMPLE_MOTOR_TORQUE);
}

// Nine gears of four sizes (a 5-point module: pitch radii 25, 50, 75, 100) on a motor along a line, one up and one down
// off it, each turning a rack.
RDE_INTERNAL void fzx_gearbox(fzx* _x) {
    fzx_part(_x, "drive motor", -450, 0, 0, "30 rpm");
    static const struct { const c8* id; f64 x, y, turn; } _g[9] = {
        { "gear 10T", -450, 0, 0 }, { "gear 30T", -350, 0, 10 }, { "gear 20T", -225, 0, 33 }, { "gear 40T", -75, 0, 5 }, { "gear 10T", 50, 0, 20 },
        { "gear 30T", 150, 0, 3 }, { "gear 20T", 275, 0, 71 }, { "gear 20T", -75, 150, 44 }, { "gear 10T", 150, -100, 12 },
    };
    for(u32 _i = 0; _i < 9u; _i++) {
        fzx_part(_x, _g[_i].id, _g[_i].x, _g[_i].y, _g[_i].turn, "");
    }
    fzx_sized(_x, "rack", -15, 150 + 50 + 5, 180, 400, 30, "");   // (turned over on the top one: its pitch line at its rim)
    fzx_sized(_x, "rack", 150, -100 - 25 - 5, 0, 300, 30, "");    // (under the bottom one)
    fzx_title(_x, -500, 330, FUDE_ZOOM_EXAMPLE_GEARBOX);
}

// Six of them on one canvas, every one running at once.
RDE_INTERNAL void fzx_machine(fzx* _x) {
    void (*const _each[6])(fzx*) = { fzx_crank, fzx_pulleys, fzx_pendulum, fzx_piston, fzx_gears_rack, fzx_bodies };
    const f64 _at[6][2] = { { -850, 450 }, { 0, 450 }, { 850, 450 }, { -850, -450 }, { 0, -450 }, { 850, -450 } };
    for(u32 _i = 0; _i < 6u; _i++) {
        const fude_zoom_v2 _was = fzx_move(_x, _at[_i][0], _at[_i][1]);
        _each[_i](_x);
        _x->p.o = _was;
    }
    fzx_title(_x, -1150, 850, FUDE_ZOOM_EXAMPLE_MACHINE);
}

// --- both ----------------------------------------------------------------------------------------------------------
//
// A circuit's motor turns the gear (or the arm) pinned on its middle, and is turned by it (a dynamo); a push button is
// pressed by a part of the mechanism over it (coupling.h).

// A motor (80 points across: its ring round the gear on it) at _px, _py, its text _text; the gear _gear on its shaft.
RDE_INTERNAL u32 fzx_motor(fzx* _x, f64 _px, f64 _py, f64 _turn, const c8* _text, const c8* _gear) {
    if(_gear != NULL) {
        fzx_part(_x, _gear, _px, _py, 0, "");
    }
    return fzx_sized(_x, "motor", _px, _py, _turn, 80, 80, _text);
}

// A battery, a switch, an ammeter and a motor (6 V, 60 rpm at them) turning a gear train; a green LED says it is on.
RDE_INTERNAL void fzx_motor_gears(fzx* _x) {
    const u32 _bat = fzx_part(_x, "battery", -300, 0, 0, "6V");
    const u32 _sw  = fzx_part(_x, "SPST switch", -200, 120, 0, "on");
    const u32 _am  = fzx_part(_x, "ammeter", -60, 120, 0, "");
    const u32 _mo  = fzx_motor(_x, 100, 0, 0, "6V 60rpm", "gear 10T");
    fzx_part(_x, "gear 30T", 100, 100, 0, "");    // (25 + 75 over it)
    fzx_part(_x, "gear 20T", 225, 100, 0, "");    // (75 + 50 on)
    const u32 _g1  = fzx_part(_x, "ground", 180, -40, 0, "");
    const u32 _g2  = fzx_part(_x, "ground", -300, -80, 0, "");
    const u32 _r   = fzx_part(_x, "resistor", -130, 40, 90, "330");
    const u32 _led = fzx_part(_x, "LED", -130, -50, -90, "green");
    const u32 _g3  = fzx_part(_x, "ground", -130, -130, 0, "");
    fzx_route(_x, _bat, 0, _sw, 0, FZX_VIA({ -300, 120 }));
    fzx_route(_x, _sw, 1, _am, 0, NULL, 0);
    fzx_route(_x, _am, 1, _mo, 0, FZX_VIA({ 10, 120 }, { 10, 0 }));
    fzx_route(_x, _mo, 1, _g1, 0, FZX_VIA({ 180, 0 }));
    fzx_route(_x, _bat, 1, _g2, 0, NULL, 0);
    fzx_route(_x, _sw, 1, _r, 1, FZX_VIA({ -130, 120 }));
    fzx_route(_x, _r, 0, _led, 0, NULL, 0);
    fzx_route(_x, _led, 1, _g3, 0, NULL, 0);
    fzx_title(_x, -330, 260, FUDE_ZOOM_EXAMPLE_MOTOR_GEARS);
}

// A drive motor at 30 rpm, a 40-tooth gear on it turning a 10-tooth one four times as fast under it on a motor — a
// dynamo: its volts as fast as it turns (about 12 V), through a resistor into a red LED; a voltmeter across it.
RDE_INTERNAL void fzx_dynamo(fzx* _x) {
    fzx_part(_x, "drive motor", 0, 125, 0, "30 rpm");
    fzx_part(_x, "gear 40T", 0, 125, 0, "");
    const u32 _gen = fzx_motor(_x, 0, 0, 0, "6V 60rpm", "gear 10T");   // (turned the clock's way: its right pin its plus)
    const u32 _r   = fzx_part(_x, "resistor", 130, 0, 0, "470");
    const u32 _led = fzx_part(_x, "LED", 250, 0, 0, "red");
    const u32 _vm  = fzx_part(_x, "voltmeter", 130, -60, 0, "");
    fzx_route(_x, _gen, 1, _r, 0, NULL, 0);
    fzx_route(_x, _r, 1, _led, 0, NULL, 0);
    fzx_route(_x, _led, 1, _gen, 0, FZX_VIA({ 310, 0 }, { 310, -100 }, { -80, -100 }, { -80, 0 }));
    fzx_route(_x, _vm, 0, _gen, 1, FZX_VIA({ 70, -60 }, { 70, 0 }));
    fzx_route(_x, _vm, 1, _gen, 0, FZX_VIA({ 190, -60 }, { 190, -100 }, { -80, -100 }, { -80, 0 }));
    const u32 _gnd = fzx_part(_x, "ground", -80, -150, 0, "");   // (its minus the circuit's 0 V)
    fzx_route(_x, _gnd, 0, _gen, 0, FZX_VIA({ -80, 0 }));
    fzx_title(_x, -120, 300, FUDE_ZOOM_EXAMPLE_DYNAMO);
}

// An L293D (its first two half bridges, enabled, on the side toward the motor) driving a motor both ways from a 9 V
// battery; its logic on 5 V. Its left side, top to bottom: EN1,2, 1A, 1Y, GND, GND, 2Y, 2A, VCC2 (20 apart from 70
// over its middle); its right side: VCC1 at the top, GND 10 over and under its middle (its GNDs are one inside).
typedef struct { u32 ic; f64 left, right, y; } fzx_driver_at;

RDE_INTERNAL fzx_driver_at fzx_driver(fzx* _x, f64 _px, f64 _py) {
    fzx_driver_at _d = { fzx_part(_x, "L293D", _px, _py, 0, "L293D"), _px - 100.0, _px + 100.0, _py };
    // (EN1,2 over its top to VCC1, VCC1 to a 5 V rail: its logic, both halves enabled)
    const u32 _rail = fzx_part(_x, "supply rail", _d.right + 20.0, _py + 120.0, 0, "5V");
    fzx_route(_x, _d.ic, 15u, _rail, 0, FZX_VIA({ _d.right + 20.0, _py + 70.0 }));
    fzx_route(_x, _d.ic, 0u, _d.ic, 15u, FZX_VIA({ _d.left - 10.0, _py + 70.0 }, { _d.left - 10.0, _py + 100.0 }, { _d.right + 20.0, _py + 100.0 },
                                                   { _d.right + 20.0, _py + 70.0 }));
    // (a GND of its right side to ground; VCC2 from the battery's plus below it on the left, its minus to ground)
    const u32 _gnd = fzx_part(_x, "ground", _d.right + 30.0, _py - 60.0, 0, "");
    fzx_route(_x, _d.ic, 11u, _gnd, 0, FZX_VIA({ _d.right + 30.0, _py - 10.0 }));
    const u32 _bat = fzx_part(_x, "battery", _d.left - 120.0, _py - 140.0, 0, "9V");
    const u32 _gb  = fzx_part(_x, "ground", _d.left - 120.0, _py - 220.0, 0, "");
    fzx_route(_x, _bat, 0, _d.ic, 7u, FZX_VIA({ _d.left - 120.0, _py - 70.0 }));
    fzx_route(_x, _bat, 1, _gb, 0, NULL, 0);
    return _d;
}

// Two logic inputs into an L293D's 1A and 2A (1, 0: one way; 0, 1: back; the same: it stops); its motor's pinion (6 V,
// 10 rpm) moves a rack.
RDE_INTERNAL void fzx_forward_back(fzx* _x) {
    const fzx_driver_at _d = fzx_driver(_x, 250, 40);
    // The motor over the rack, left of the chip (its 20-tooth pinion's pitch circle 50 round, the rack's pitch line 5 over
    // its middle): 1Y to its first pin, 2Y to its second, round over the gear.
    const u32 _mo = fzx_motor(_x, -160, 240, 0, "6V 10rpm", "gear 20T");
    fzx_sized(_x, "rack", -160, 240 - 55, 0, 360, 30, "");
    fzx_route(_x, _d.ic, 2u, _mo, 0, FZX_VIA({ _d.left - 60.0, _d.y + 30.0 }, { _d.left - 60.0, 360 }, { -240, 360 }, { -240, 240 }));
    fzx_route(_x, _d.ic, 5u, _mo, 1, FZX_VIA({ _d.left - 80.0, _d.y - 30.0 }, { _d.left - 80.0, 340 }, { -80, 340 }, { -80, 240 }));
    // The inputs, left of the chip below the rack.
    const u32 _a1 = fzx_part(_x, "logic input", _d.left - 140.0, _d.y + 50.0, 0, "1");
    const u32 _a2 = fzx_part(_x, "logic input", _d.left - 140.0, _d.y - 50.0, 0, "0");
    fzx_route(_x, _a1, 0, _d.ic, 1u, NULL, 0);
    fzx_route(_x, _a2, 0, _d.ic, 6u, NULL, 0);
    fzx_text(_x, _d.left - 230.0, _d.y + 62.0, 16.0, 40.0, "1A");
    fzx_text(_x, _d.left - 230.0, _d.y - 38.0, 16.0, 40.0, "2A");
    fzx_title(_x, -360, 450, FUDE_ZOOM_EXAMPLE_FORWARD_BACK);
}

// A motor (6 V, 30 rpm) turning an arm on its shaft: each turn the arm goes over a push button, the clock of a counter
// of four D flip-flops (each its /Q into its D: it toggles; each the next one's clock) — the turns, in binary, on four
// LEDs. The button pulled down through 10k.
RDE_INTERNAL void fzx_turn_counter(fzx* _x) {
    const u32 _bat = fzx_part(_x, "battery", -650, 40, 0, "6V");
    const u32 _sw  = fzx_part(_x, "SPST switch", -580, 160, 0, "on");
    const u32 _mo  = fzx_motor(_x, -470, 40, 0, "6V 30rpm", NULL);
    fzx_sized(_x, "link", -470 + 63, 40, 0, 150, 24, "");   // (its holes at its round ends, 126 apart: one on the shaft)
    const u32 _gm  = fzx_part(_x, "ground", -390, 0, 0, "");
    const u32 _gb  = fzx_part(_x, "ground", -650, -40, 0, "");
    fzx_route(_x, _bat, 0, _sw, 0, FZX_VIA({ -650, 160 }));
    fzx_route(_x, _sw, 1, _mo, 0, FZX_VIA({ -530, 160 }, { -530, 40 }));
    fzx_route(_x, _mo, 1, _gm, 0, FZX_VIA({ -390, 40 }));
    fzx_route(_x, _bat, 1, _gb, 0, NULL, 0);
    // (the button where the arm's middle goes by, below the shaft; 5 V on its left, the counter's clock on its right)
    const u32 _btn  = fzx_part(_x, "push button", -470, -55, 0, "");
    const u32 _top  = fzx_part(_x, "supply rail", -560, -30, 0, "5V");
    const u32 _pull = fzx_part(_x, "resistor", -300, -110, 90, "10k");
    const u32 _pg   = fzx_part(_x, "ground", -300, -190, 0, "");
    fzx_route(_x, _top, 0, _btn, 0, FZX_VIA({ -560, -55 }));
    fzx_route(_x, _pull, 1, _btn, 1, FZX_VIA({ -300, -55 }));
    fzx_route(_x, _pull, 0, _pg, 0, NULL, 0);
    // The flip-flops, 220 apart: D (−40, 20), CLK (−40, −20), Q (40, 20), /Q (40, −20) about each.
    static const c8* const _colours[4] = { "red", "yellow", "green", "blue" };
    u32 _ff[4];
    for(u32 _k = 0; _k < 4u; _k++) {
        const f64 _fx = -160.0 + 220.0 * (f64)_k, _fy = -35.0;
        _ff[_k] = fzx_part(_x, "D flip-flop", _fx, _fy, 0, "");
        // (/Q round under it into D)
        fzx_route(_x, _ff[_k], 3u, _ff[_k], 0u, FZX_VIA({ _fx + 60.0, _fy - 20.0 }, { _fx + 60.0, _fy - 60.0 }, { _fx - 60.0, _fy - 60.0 },
                                                         { _fx - 60.0, _fy + 20.0 }));
        // (its clock: the button's, or the one before's /Q)
        if(_k == 0u) {
            fzx_route(_x, _btn, 1, _ff[0], 1u, NULL, 0);
        } else {
            fzx_route(_x, _ff[_k - 1u], 3u, _ff[_k], 1u, NULL, 0);
        }
        // (Q up through 330 into its LED, to ground)
        const u32 _r   = fzx_part(_x, "resistor", _fx + 100.0, 100, 0, "330");
        const u32 _led = fzx_part(_x, "LED", _fx + 190.0, 100, 0, _colours[_k]);
        const u32 _g   = fzx_part(_x, "ground", _fx + 250.0, 60, 0, "");
        fzx_route(_x, _ff[_k], 2u, _r, 0, FZX_VIA({ _fx + 55.0, _fy + 20.0 }, { _fx + 55.0, 100 }));
        fzx_route(_x, _r, 1, _led, 0, NULL, 0);
        fzx_route(_x, _led, 1, _g, 0, FZX_VIA({ _fx + 250.0, 100 }));
    }
    fzx_title(_x, -650, 300, FUDE_ZOOM_EXAMPLE_TURN_COUNTER);
}

// A rack under a motor's pinion (an L293D's, 9 V), a push button at each end of its travel: the left one (pressed as it
// starts, the rack over it) sets a latch of two NOR gates — the motor turns it right —, the right one resets it — it
// turns back. On and on, a green LED while it goes right, a red one while it goes left.
RDE_INTERNAL void fzx_shuttle(fzx* _x) {
    const fzx_driver_at _d = fzx_driver(_x, 420, -60);
    const u32 _mo = fzx_motor(_x, 0, 250, 0, "6V 10rpm", "gear 20T");
    fzx_sized(_x, "rack", -60, 250 - 55, 0, 300, 30, "");   // (from −210 to 90: over the left button)
    // 1Y to the motor's first pin, 2Y to its second, round over the gear.
    fzx_route(_x, _d.ic, 2u, _mo, 0, FZX_VIA({ _d.left - 50.0, _d.y + 30.0 }, { _d.left - 50.0, 370 }, { -80, 370 }, { -80, 250 }));
    fzx_route(_x, _d.ic, 5u, _mo, 1, FZX_VIA({ _d.left - 70.0, _d.y - 30.0 }, { _d.left - 70.0, 350 }, { 80, 350 }, { 80, 250 }));
    // The buttons on the rack's line (upright: a pin over it, a pin under it): 5 V over each, the latch's input under it
    // (pulled down through 10k).
    const u32 _lb = fzx_part(_x, "push button", -190, 195, 90, "");
    const u32 _rb = fzx_part(_x, "push button", 190, 195, 90, "");
    const u32 _rl = fzx_part(_x, "supply rail", -190, 270, 0, "5V");
    const u32 _rr = fzx_part(_x, "supply rail", 190, 270, 0, "5V");
    fzx_route(_x, _lb, 1, _rl, 0, NULL, 0);
    fzx_route(_x, _rb, 1, _rr, 0, NULL, 0);
    // The latch under the rack: the left button sets it (its NOR's output /Q), the right one resets it (Q).
    const u32 _set = fzx_part(_x, "NOR gate", -100, 40, 0, "");
    const u32 _res = fzx_part(_x, "NOR gate", -100, -80, 0, "");
    const u32 _pl  = fzx_part(_x, "resistor", -250, 60, 90, "10k");
    const u32 _gl  = fzx_part(_x, "ground", -250, -10, 0, "");
    const u32 _pr  = fzx_part(_x, "resistor", 120, 60, 90, "10k");
    const u32 _gr  = fzx_part(_x, "ground", 120, -10, 0, "");
    fzx_route(_x, _lb, 0, _set, 0u, FZX_VIA({ -190, 120 }, { -170, 120 }, { -170, 60 }));
    fzx_route(_x, _pl, 1, _lb, 0, FZX_VIA({ -250, 120 }, { -190, 120 }));
    fzx_route(_x, _pl, 0, _gl, 0, NULL, 0);
    fzx_route(_x, _rb, 0, _res, 1u, FZX_VIA({ 190, 130 }, { 160, 130 }, { 160, -140 }, { -180, -140 }, { -180, -100 }));
    fzx_route(_x, _pr, 1, _rb, 0, FZX_VIA({ 120, 130 }, { 190, 130 }));
    fzx_route(_x, _pr, 0, _gr, 0, NULL, 0);
    // (each output into the other's free input)
    fzx_route(_x, _set, 2u, _res, 0u, FZX_VIA({ -30, 40 }, { -30, 0 }, { -160, 0 }, { -160, -60 }));
    fzx_route(_x, _res, 2u, _set, 1u, FZX_VIA({ -10, -80 }, { -10, -20 }, { -200, -20 }, { -200, 20 }));
    // Q into 1A, /Q into 2A: Q high, the motor turns the rack right.
    fzx_route(_x, _res, 2u, _d.ic, 1u, FZX_VIA({ 30, -80 }, { 30, _d.y + 50.0 }));
    fzx_route(_x, _set, 2u, _d.ic, 6u, FZX_VIA({ 50, 40 }, { 50, _d.y - 50.0 }));
    // Its way on two LEDs: Q's green, /Q's red, under the latch.
    const u32 _rg = fzx_part(_x, "resistor", -100, -200, 0, "330");
    const u32 _lg = fzx_part(_x, "LED", 10, -200, 0, "green");
    const u32 _rd = fzx_part(_x, "resistor", -100, -280, 0, "330");
    const u32 _ld = fzx_part(_x, "LED", 10, -280, 0, "red");
    const u32 _g  = fzx_part(_x, "ground", 100, -320, 0, "");
    fzx_route(_x, _res, 2u, _rg, 0, FZX_VIA({ -10, -80 }, { -10, -160 }, { -150, -160 }, { -150, -200 }));
    fzx_route(_x, _set, 2u, _rd, 0, FZX_VIA({ -30, 40 }, { -30, 0 }, { -220, 0 }, { -220, -280 }));
    fzx_route(_x, _rg, 1, _lg, 0, NULL, 0);
    fzx_route(_x, _rd, 1, _ld, 0, NULL, 0);
    fzx_route(_x, _lg, 1, _g, 0, FZX_VIA({ 100, -200 }));
    fzx_route(_x, _ld, 1, _g, 0, FZX_VIA({ 100, -280 }));
    fzx_title(_x, -330, 470, FUDE_ZOOM_EXAMPLE_SHUTTLE);
}

// A servo tester: a 555 on four AA cells (6 V) whose pulses are short — it charges through R1 alone, past R2 by a diode,
// and discharges through R2 (270k: about 17 ms low) — R1 5.6k and a 20k pot (0.5 to 2.3 ms: the whole of a servo's
// travel, 1.4 ms with the pot halfway); its output the servo's signal, an arm on its shaft. A tap on the pot turns it.
RDE_INTERNAL void fzx_servo_tester(fzx* _x) {
    const u32 _bat = fzx_part(_x, "battery", -330, 0, 0, "6V");
    const u32 _t   = fzx_part(_x, "NE555", 0, 0, 0, "NE555");
    const u32 _pot = fzx_part(_x, "potentiometer", 220, 290, 0, "20k 50%");
    const u32 _r1  = fzx_part(_x, "resistor", 220, 200, 0, "5.6k");
    const u32 _r2  = fzx_part(_x, "resistor", 220, 120, 0, "270k");
    const u32 _d   = fzx_part(_x, "diode", 220, 50, 180, "1N4148");
    const u32 _cap = fzx_part(_x, "capacitor", -180, -160, -90, "100nF");
    // The servo below the battery, its arm up (the middle of its travel: clear of its pins' names, GND, VCC, SIG down its
    // left).
    const u32 _sv  = fzx_part(_x, "servo", -280, -300, 0, "SG90 servo");
    fzx_sized(_x, "link", -280, -300 + 63, 90, 150, 24, "");
    fzx_wire(_x, _bat, 0, _t, 7);
    fzx_wire(_x, _t, 7, _t, 3);
    fzx_wire(_x, _t, 0, _bat, 1);
    fzx_wire(_x, _t, 7, _pot, 0);   // (the pot as a resistor: its end and its wiper)
    fzx_wire(_x, _pot, 2, _r1, 1);
    fzx_wire(_x, _r1, 0, _t, 6);
    fzx_wire(_x, _t, 6, _r2, 1);
    fzx_wire(_x, _r2, 0, _t, 5);
    fzx_wire(_x, _t, 6, _d, 0);     // (its anode on DISCH, its cathode on THRES: charging past R2)
    fzx_wire(_x, _d, 1, _t, 5);
    fzx_wire(_x, _t, 5, _t, 1);
    fzx_wire(_x, _t, 1, _cap, 0);
    fzx_wire(_x, _cap, 1, _bat, 1);
    fzx_route(_x, _t, 2, _sv, 2, FZX_VIA({ -130, -10 }, { -130, -380 }, { -390, -380 }, { -390, -320 }));
    fzx_route(_x, _sv, 1, _bat, 0, FZX_VIA({ -420, -300 }, { -420, 80 }, { -330, 80 }));
    fzx_route(_x, _sv, 0, _bat, 1, FZX_VIA({ -370, -280 }, { -370, -70 }, { -330, -70 }));
    fzx_title(_x, -330, 420, FUDE_ZOOM_EXAMPLE_SERVO_TESTER);
}

// Three servos, each its own pulse source at 50 Hz — 1 ms, 1.5 ms, 2 ms — and its own four AA cells: their arms at 45°,
// 90°, 135° (each drawn up, the middle of its travel: turned 45° clockwise, left, 45° counter-clockwise); a fourth on
// 12 V, past its 7 V: burnt.
RDE_INTERNAL void fzx_servo_angles(fzx* _x) {
    static const c8* const _pulse[4] = { "50Hz 5V 1ms", "50Hz 5V 1.5ms", "50Hz 5V 2ms", "50Hz 5V 1.5ms" };
    for(u32 _k = 0; _k < 4u; _k++) {
        const f64 _y = 330.0 - 200.0 * (f64)_k;
        const u32 _bat = fzx_part(_x, "battery", -420, _y, 0, _k < 3u ? "6V" : "12V");
        const u32 _clk = fzx_part(_x, "clock", -250, _y - 60, 0, _pulse[_k]);
        const u32 _sv  = fzx_part(_x, "servo", 0, _y, 0, "SG90 servo");
        fzx_sized(_x, "link", 0, _y + 63, 90, 150, 24, "");
        fzx_route(_x, _bat, 0, _sv, 1, FZX_VIA({ -420, _y + 50 }, { -100, _y + 50 }, { -100, _y }));
        fzx_route(_x, _sv, 0, _bat, 1, FZX_VIA({ -360, _y + 20 }, { -360, _y - 50 }, { -420, _y - 50 }));
        fzx_route(_x, _sv, 2, _clk, 0, FZX_VIA({ -250, _y - 20 }));
        fzx_route(_x, _clk, 1, _bat, 1, FZX_VIA({ -250, _y - 110 }, { -440, _y - 110 }, { -440, _y - 50 }, { -420, _y - 50 }));
    }
    fzx_word(_x, 120, -320, 16.0, 220.0, FUDE_ZOOM_EXAMPLE_WORD_RESTART);
    fzx_title(_x, -460, 470, FUDE_ZOOM_EXAMPLE_SERVO_ANGLES);
}

RDE_INTERNAL void fzx_workbench(fzx* _x) {
    void (*const _each[4])(fzx*) = { fzx_counter, fzx_gates, fzx_gears_rack, fzx_pulleys };
    const f64 _at[4][2] = { { -550, 350 }, { -550, -500 }, { 500, 350 }, { 500, -450 } };
    for(u32 _i = 0; _i < 4u; _i++) {
        const fude_zoom_v2 _was = fzx_move(_x, _at[_i][0], _at[_i][1]);
        _each[_i](_x);
        _x->p.o = _was;
    }
    fzx_title(_x, -1000, 900, FUDE_ZOOM_EXAMPLE_WORKBENCH);
}

RDE_INTERNAL void fzx_everything(fzx* _x) {
    void (*const _each[7])(fzx*) = { fzx_chaser, fzx_adder_4, fzx_flasher, fzx_gearbox, fzx_machine, fzx_shuttle, fzx_turn_counter };
    const f64 _at[7][2] = { { -1500, 1300 }, { -1500, -400 }, { 300, 1350 }, { 1400, 1300 }, { 1000, -800 }, { -1300, -2300 }, { 700, -2200 } };
    for(u32 _i = 0; _i < 7u; _i++) {
        const fude_zoom_v2 _was = fzx_move(_x, _at[_i][0], _at[_i][1]);
        _each[_i](_x);
        _x->p.o = _was;
    }
    fzx_title(_x, -2100, 2000, FUDE_ZOOM_EXAMPLE_EVERYTHING);
}

u32 fude_zoom_example_build(fude_zoom_scene* _s, u32 _frame, fude_zoom_sim _to_frame, rde_color _ink, u32 _example, fude_zoom_example_title _title,
                            rde_arr* _born) {
    void (*const _build[FUDE_ZOOM_EXAMPLE_COUNT])(fzx*) = {
        fzx_torch, fzx_gates, fzx_flasher, fzx_full_adder, fzx_adder_4, fzx_counter, fzx_adder_chip, fzx_chaser,
        fzx_led_resistor, fzx_resistor_watts, fzx_short_fuse, fzx_cap_polarity, fzx_transistor_size, fzx_ringing,
        fzx_digit_counter, fzx_matrix_scan, fzx_bar_meter, fzx_panel_meters, fzx_lcd_by_hand,
        fzx_crank, fzx_gear_train, fzx_pendulum, fzx_pulleys, fzx_piston, fzx_rack, fzx_gears_rack, fzx_bodies, fzx_gearbox, fzx_machine,
        fzx_materials, fzx_too_heavy, fzx_motor_torque,
        fzx_motor_gears, fzx_dynamo, fzx_forward_back, fzx_turn_counter, fzx_shuttle, fzx_servo_tester, fzx_servo_angles, fzx_workbench,
        fzx_everything,
    };
    if(_example >= FUDE_ZOOM_EXAMPLE_COUNT || _s == NULL || _born == NULL) {
        return 0u;
    }
    fzx _x = { fude_zoom_placer_make(_s, _frame, _to_frame, _ink, _born), _title };
    const u32 _was = (u32)rde_arr_length(_born);
    _build[_example](&_x);
    return (u32)rde_arr_length(_born) - _was;
}
