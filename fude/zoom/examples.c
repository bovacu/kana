// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/examples.h"
#include "zoom/placer.h"
#include "zoom/circuit.h"
#include "zoom/shape.h"
#include "zoom/props.h"
#include <math.h>
#include <string.h>

#define FZX_DEG 0.017453292519943295

b8 fude_zoom_example_goes_wrong(u32 _example) {
    return _example == FUDE_ZOOM_EXAMPLE_LED_RESISTOR || _example == FUDE_ZOOM_EXAMPLE_RESISTOR_WATTS || _example == FUDE_ZOOM_EXAMPLE_SHORT_FUSE ||
           _example == FUDE_ZOOM_EXAMPLE_CAP_POLARITY || _example == FUDE_ZOOM_EXAMPLE_TRANSISTOR_SIZE || _example == FUDE_ZOOM_EXAMPLE_TOO_HEAVY ||
           _example == FUDE_ZOOM_EXAMPLE_MOTOR_TORQUE || _example == FUDE_ZOOM_EXAMPLE_SERVO_ANGLES;
}

u8 fude_zoom_example_group(u32 _example) {
    // (in the list's order: the electronics' up to the first mechanism's, the mechanisms' up to the first of both's)
    return _example < FUDE_ZOOM_EXAMPLE_CRANK ? FUDE_ZOOM_EXAMPLES_ELECTRONICS :
           (_example < FUDE_ZOOM_EXAMPLE_MOTOR_GEARS ? FUDE_ZOOM_EXAMPLES_MECHANISMS : FUDE_ZOOM_EXAMPLES_BOTH);
}

u8 fude_zoom_example_set(u32 _example) {
    switch(_example) {
    case FUDE_ZOOM_EXAMPLE_TORCH: case FUDE_ZOOM_EXAMPLE_FLASHER: case FUDE_ZOOM_EXAMPLE_CHASER: case FUDE_ZOOM_EXAMPLE_SPEAKER_TONE:
        return FUDE_ZOOM_EXAMPLE_SET_FIRST;
    case FUDE_ZOOM_EXAMPLE_GATES: case FUDE_ZOOM_EXAMPLE_FULL_ADDER: case FUDE_ZOOM_EXAMPLE_ADDER_4: case FUDE_ZOOM_EXAMPLE_ADDER_CHIP:
    case FUDE_ZOOM_EXAMPLE_COUNTER: case FUDE_ZOOM_EXAMPLE_FLIPFLOPS: case FUDE_ZOOM_EXAMPLE_DECADE:
        return FUDE_ZOOM_EXAMPLE_SET_LOGIC;
    case FUDE_ZOOM_EXAMPLE_DIGIT_COUNTER: case FUDE_ZOOM_EXAMPLE_MATRIX_SCAN: case FUDE_ZOOM_EXAMPLE_BAR_METER: case FUDE_ZOOM_EXAMPLE_PANEL_METERS:
    case FUDE_ZOOM_EXAMPLE_LCD_BY_HAND:
        return FUDE_ZOOM_EXAMPLE_SET_DISPLAYS;
    case FUDE_ZOOM_EXAMPLE_SCOPE_RC: case FUDE_ZOOM_EXAMPLE_LIGHT_TEMP: case FUDE_ZOOM_EXAMPLE_COMPARATOR: case FUDE_ZOOM_EXAMPLE_LIGHT_LINK:
        return FUDE_ZOOM_EXAMPLE_SET_SENSING;
    case FUDE_ZOOM_EXAMPLE_THYRISTORS: case FUDE_ZOOM_EXAMPLE_POWER_SUPPLY:
        return FUDE_ZOOM_EXAMPLE_SET_POWER;
    case FUDE_ZOOM_EXAMPLE_LED_RESISTOR: case FUDE_ZOOM_EXAMPLE_RESISTOR_WATTS: case FUDE_ZOOM_EXAMPLE_SHORT_FUSE: case FUDE_ZOOM_EXAMPLE_CAP_POLARITY:
    case FUDE_ZOOM_EXAMPLE_TRANSISTOR_SIZE: case FUDE_ZOOM_EXAMPLE_RINGING:
        return FUDE_ZOOM_EXAMPLE_SET_E_WRONG;
    case FUDE_ZOOM_EXAMPLE_CRANK: case FUDE_ZOOM_EXAMPLE_PISTON: case FUDE_ZOOM_EXAMPLE_COUPLER_CURVE: case FUDE_ZOOM_EXAMPLE_BY_HAND:
        return FUDE_ZOOM_EXAMPLE_SET_LINKAGES;
    case FUDE_ZOOM_EXAMPLE_GEAR_TRAIN: case FUDE_ZOOM_EXAMPLE_RACK: case FUDE_ZOOM_EXAMPLE_GEARS_RACK: case FUDE_ZOOM_EXAMPLE_GEARBOX:
    case FUDE_ZOOM_EXAMPLE_BELTS: case FUDE_ZOOM_EXAMPLE_WORM:
        return FUDE_ZOOM_EXAMPLE_SET_GEARS;
    case FUDE_ZOOM_EXAMPLE_PENDULUM: case FUDE_ZOOM_EXAMPLE_PULLEYS: case FUDE_ZOOM_EXAMPLE_BODIES: case FUDE_ZOOM_EXAMPLE_CAM:
    case FUDE_ZOOM_EXAMPLE_RATCHET: case FUDE_ZOOM_EXAMPLE_DAMPER:
        return FUDE_ZOOM_EXAMPLE_SET_MOTION;
    case FUDE_ZOOM_EXAMPLE_MATERIALS: case FUDE_ZOOM_EXAMPLE_TOO_HEAVY: case FUDE_ZOOM_EXAMPLE_MOTOR_TORQUE:
        return FUDE_ZOOM_EXAMPLE_SET_M_WRONG;
    case FUDE_ZOOM_EXAMPLE_MACHINE:
        return FUDE_ZOOM_EXAMPLE_SET_M_BIG;
    case FUDE_ZOOM_EXAMPLE_MOTOR_GEARS: case FUDE_ZOOM_EXAMPLE_DYNAMO: case FUDE_ZOOM_EXAMPLE_FORWARD_BACK: case FUDE_ZOOM_EXAMPLE_SHUTTLE:
        return FUDE_ZOOM_EXAMPLE_SET_MOTORS;
    case FUDE_ZOOM_EXAMPLE_SERVO_TESTER: case FUDE_ZOOM_EXAMPLE_SERVO_ANGLES: case FUDE_ZOOM_EXAMPLE_SOLENOID: case FUDE_ZOOM_EXAMPLE_STEPPER:
        return FUDE_ZOOM_EXAMPLE_SET_ACTUATORS;
    case FUDE_ZOOM_EXAMPLE_TURN_COUNTER: case FUDE_ZOOM_EXAMPLE_SHAFT_SENSORS:
        return FUDE_ZOOM_EXAMPLE_SET_MACHINE_SENSORS;
    default:
        // (any other: its group's big projects)
        return fude_zoom_example_group(_example) == FUDE_ZOOM_EXAMPLES_ELECTRONICS ? FUDE_ZOOM_EXAMPLE_SET_E_BIG :
               (fude_zoom_example_group(_example) == FUDE_ZOOM_EXAMPLES_MECHANISMS ? FUDE_ZOOM_EXAMPLE_SET_M_BIG : FUDE_ZOOM_EXAMPLE_SET_B_BIG);
    }
}

u8 fude_zoom_example_set_group(u8 _set) {
    return _set <= FUDE_ZOOM_EXAMPLE_SET_E_BIG ? FUDE_ZOOM_EXAMPLES_ELECTRONICS :
           (_set <= FUDE_ZOOM_EXAMPLE_SET_M_BIG ? FUDE_ZOOM_EXAMPLES_MECHANISMS : FUDE_ZOOM_EXAMPLES_BOTH);
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

// A 100 Hz square wave into 10 kΩ and 100 nF (a millisecond its time constant): on an oscilloscope, the square on CH1
// (yellow), the capacitor charging and discharging toward it on CH2 (cyan), 2 ms a division.
RDE_INTERNAL void fzx_scope_rc(fzx* _x) {
    const u32 _clk = fzx_part(_x, "clock", -250, 0, 0, "100Hz 5V");
    const u32 _r   = fzx_part(_x, "resistor", -100, 80, 0, "10k");
    const u32 _cap = fzx_part(_x, "capacitor", 0, 0, -90, "100nF");
    const u32 _g   = fzx_part(_x, "ground", 0, -100, 0, "");
    const u32 _sp  = fzx_part(_x, "oscilloscope", 340, 20, 0, "2ms 2V");
    fzx_route(_x, _clk, 0, _r, 0, FZX_VIA({ -250, 80 }));
    fzx_route(_x, _r, 1, _cap, 0, FZX_VIA({ 0, 80 }));
    fzx_line(_x, _cap, 1, _g, 0);
    fzx_route(_x, _clk, 1, _g, 0, FZX_VIA({ -250, -80 }));
    fzx_line(_x, _cap, 0, _sp, 1);
    fzx_route(_x, _sp, 0, _clk, 0, FZX_VIA({ 150, 60 }, { 150, 140 }, { -250, 140 }));
    fzx_route(_x, _sp, 2, _g, 0, FZX_VIA({ 150, -20 }, { 150, -80 }));
    fzx_word(_x, -300, -160, 16.0, 560.0, FUDE_ZOOM_EXAMPLE_WORD_SCOPE);
    fzx_title(_x, -300, 230, FUDE_ZOOM_EXAMPLE_SCOPE_RC);
}

// A night light: an LDR under 47 kΩ from 5 V, its middle through 4.7 kΩ into a transistor's base — in the dark the LDR's
// resistance high, the base high, the LED on (a room's light: off) —; and a thermometer: a thermistor under 10 kΩ, a panel
// meter on it (2.50 V at 25 °C, less as it warms). Play's sliders, or a tap on them, change the light and the heat.
RDE_INTERNAL void fzx_light_temp(fzx* _x) {
    const u32 _rail = fzx_part(_x, "supply rail", -300, 210, 0, "5V");
    const u32 _r1   = fzx_part(_x, "resistor", -300, 130, 90, "47k");
    const u32 _ldr  = fzx_part(_x, "LDR", -300, 10, 90, "GL5528");
    const u32 _g1   = fzx_part(_x, "ground", -300, -80, 0, "");
    const u32 _rb   = fzx_part(_x, "resistor", -200, 70, 0, "4.7k");
    const u32 _q    = fzx_part(_x, "NPN", -100, 70, 0, "2N2222");
    const u32 _led  = fzx_part(_x, "LED", -90, 170, -90, "yellow");
    const u32 _rl   = fzx_part(_x, "resistor", -90, 250, 90, "330");
    const u32 _rail2 = fzx_part(_x, "supply rail", -90, 320, 0, "5V");
    const u32 _g2   = fzx_part(_x, "ground", -90, -20, 0, "");
    fzx_line(_x, _rail, 0, _r1, 1);
    fzx_line(_x, _r1, 0, _ldr, 1);
    fzx_line(_x, _ldr, 0, _g1, 0);
    fzx_route(_x, _r1, 0, _rb, 0, FZX_VIA({ -300, 70 }));
    fzx_line(_x, _rb, 1, _q, 0);
    fzx_line(_x, _q, 1, _led, 1);
    fzx_line(_x, _led, 0, _rl, 0);
    fzx_line(_x, _rl, 1, _rail2, 0);
    fzx_line(_x, _q, 2, _g2, 0);
    // (the thermometer)
    const u32 _rail3 = fzx_part(_x, "supply rail", 150, 210, 0, "5V");
    const u32 _rt    = fzx_part(_x, "resistor", 150, 130, 90, "10k");
    const u32 _ntc   = fzx_part(_x, "thermistor", 150, 10, 90, "10k NTC");
    const u32 _g3    = fzx_part(_x, "ground", 150, -80, 0, "");
    const u32 _pm    = fzx_part(_x, "panel meter", 330, 70, 0, "20V");
    fzx_line(_x, _rail3, 0, _rt, 1);
    fzx_line(_x, _rt, 0, _ntc, 1);
    fzx_line(_x, _ntc, 0, _g3, 0);
    fzx_route(_x, _rt, 0, _pm, 0, FZX_VIA({ 150, 70 }));
    fzx_route(_x, _pm, 1, _g3, 0, FZX_VIA({ 440, 70 }, { 440, -60 }, { 150, -60 }));
    fzx_word(_x, -320, -140, 16.0, 640.0, FUDE_ZOOM_EXAMPLE_WORD_SLIDERS);
    fzx_title(_x, -320, 380, FUDE_ZOOM_EXAMPLE_LIGHT_TEMP);
}

// --- displays (display.h) ---

// A small ground or supply rail (_volts NULL: a ground) turned to face a pin on its part's left, _gap from it: a wire
// straight between them.
// A 4 Hz clock into a CD4017 (its reset and its clock inhibit held low, on 5 V): each of its ten outputs through 470 Ω
// into an LED, in a row under it, numbered — each output's wire out to its own lane and down, none crossing another (the
// row in its package's order: 5 1 0 2 6 7 3 on its left, 8 4 9 on its right). One lit at a time, 0 to 9 and round.
RDE_INTERNAL void fzx_decade(fzx* _x) {
    const u32 _ch = fzx_part(_x, "CD4017", 0, 0, 0, "CD4017");
    // (left, x −100: Q5 70, Q1 50, Q0 30, Q2 10, Q6 −10, Q7 −30, Q3 −50, VSS −70; right, x 100: Q8 −70, Q4 −50, Q9 −30,
    // CO −10, INH 10, CLK 30, RST 50, VDD 70)
    const u32 _vr = fzx_part(_x, "supply rail", 140, 90, 0, "5V");
    fzx_route(_x, _ch, 15u, _vr, 0, FZX_VIA({ 140, 70 }));
    const u32 _gi = fzx_part(_x, "ground", 120, -120, 0, "");
    fzx_route(_x, _ch, 14u, _gi, 0, FZX_VIA({ 120, 50 }));
    fzx_route(_x, _ch, 12u, _gi, 0, FZX_VIA({ 110, 10 }, { 110, -100 }));
    const u32 _clk = fzx_part(_x, "clock", 400, 0, 0, "4Hz");
    const u32 _gc  = fzx_part(_x, "ground", 400, -80, 0, "");
    fzx_route(_x, _ch, 13u, _clk, 0, NULL, 0);
    fzx_route(_x, _clk, 1, _gc, 0, NULL, 0);
    const u32 _gv = fzx_part(_x, "ground", -130, -120, 0, "");
    fzx_route(_x, _ch, 7u, _gv, 0, FZX_VIA({ -130, -70 }));
    // (each output: its pin, its count, its lane — the higher its pin, the farther out)
    static const u32 _pin[10]  = { 0, 1, 2, 3, 4, 5, 6, 10, 9, 8 };
    static const u32 _num[10]  = { 5, 1, 0, 2, 6, 7, 3, 9, 4, 8 };
    static const f64 _lane[10] = { -580, -510, -440, -370, -300, -230, -160, 300, 230, 160 };
    static const c8* const _digit[10] = { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9" };
    for(u32 _k = 0; _k < 10u; _k++) {
        const f64 _lx = _lane[_k];
        const u32 _r = fzx_part(_x, "resistor", _lx, -200, 90, "470");
        const u32 _l = fzx_sized(_x, "LED", _lx, -300, -90, 60, 20, _num[_k] == 0u ? "red" : "yellow");
        const u32 _g = fzx_part(_x, "ground", _lx, -380, 0, "");
        const fude_zoom_v2 _at = fzx_pin(_x, _ch, _pin[_k]);
        fzx_route(_x, _ch, _pin[_k], _r, 1, FZX_VIA({ _lx, _at.y }));
        fzx_route(_x, _r, 0, _l, 0, NULL, 0);
        fzx_route(_x, _l, 1, _g, 0, NULL, 0);
        fzx_text(_x, _lx + 16.0, -150.0, 18.0, 20.0, _digit[_num[_k]]);
    }
    fzx_title(_x, -580, 170, FUDE_ZOOM_EXAMPLE_DECADE);
}

// An SR latch, S and R on logic inputs, its Q and /Q on probes; a JK flip-flop, J and K on logic inputs (both 1: it
// changes on each tick), its clock 1 Hz.
RDE_INTERNAL void fzx_flipflops(fzx* _x) {
    const u32 _sr = fzx_part(_x, "SR latch", 0, 150, 0, "");
    const u32 _s  = fzx_sized(_x, "logic input", -150, 170, 0, 60, 20, "0");
    const u32 _rr = fzx_sized(_x, "logic input", -150, 130, 0, 60, 20, "0");
    const u32 _q1 = fzx_part(_x, "logic probe", 100, 170, 0, "");
    const u32 _n1 = fzx_part(_x, "logic probe", 100, 130, 0, "");
    fzx_route(_x, _s, 0, _sr, 0, NULL, 0);
    fzx_route(_x, _rr, 0, _sr, 1, NULL, 0);
    fzx_route(_x, _sr, 2, _q1, 0, NULL, 0);
    fzx_route(_x, _sr, 3, _n1, 0, NULL, 0);
    fzx_text(_x, -220, 180, 14.0, 30.0, "S");
    fzx_text(_x, -220, 140, 14.0, 30.0, "R");
    const u32 _jk = fzx_part(_x, "JK flip-flop", 0, -50, 0, "");
    const u32 _j  = fzx_sized(_x, "logic input", -150, -30, 0, 60, 20, "1");
    const u32 _k  = fzx_sized(_x, "logic input", -150, -70, 0, 60, 20, "1");
    const u32 _ck = fzx_part(_x, "clock", -230, -130, 0, "1Hz");
    const u32 _gc = fzx_part(_x, "ground", -230, -210, 0, "");
    const u32 _q2 = fzx_part(_x, "logic probe", 100, -30, 0, "");
    const u32 _n2 = fzx_part(_x, "logic probe", 100, -70, 0, "");
    fzx_route(_x, _j, 0, _jk, 0, NULL, 0);
    fzx_route(_x, _k, 0, _jk, 2, NULL, 0);
    fzx_route(_x, _ck, 0, _jk, 1, FZX_VIA({ -230, -50 }));
    fzx_route(_x, _ck, 1, _gc, 0, NULL, 0);
    fzx_route(_x, _jk, 3, _q2, 0, NULL, 0);
    fzx_route(_x, _jk, 4, _n2, 0, NULL, 0);
    fzx_text(_x, -220, -20, 14.0, 30.0, "J");
    fzx_text(_x, -220, -60, 14.0, 30.0, "K");
    fzx_word(_x, -230, -250, 16.0, 460.0, FUDE_ZOOM_EXAMPLE_WORD_FLIPFLOPS);
    fzx_title(_x, -230, 260, FUDE_ZOOM_EXAMPLE_FLIPFLOPS);
}

// A night light: an LM393 (on 5 V) comparing an LDR's divider (10k from 5 V over it, the LDR to ground: higher as it gets
// darker) with a pot's wiper; dark enough, its open collector sinks an LED's current from 5 V through 220 Ω.
RDE_INTERNAL void fzx_comparator(fzx* _x) {
    const u32 _ic = fzx_part(_x, "LM393", 0, 0, 0, "LM393");
    const u32 _vr = fzx_part(_x, "supply rail", 140, 60, 0, "5V");
    fzx_route(_x, _ic, 7u, _vr, 0, FZX_VIA({ 140, 30 }));
    const u32 _gi = fzx_part(_x, "ground", -110, -65, 0, "");
    fzx_route(_x, _ic, 3u, _gi, 0, FZX_VIA({ -110, -30 }));
    const u32 _rt = fzx_part(_x, "resistor", -300, 10, 0, "10k");
    const u32 _rr = fzx_part(_x, "supply rail", -360, 40, 0, "5V");
    const u32 _ldr = fzx_part(_x, "LDR", -270, -60, 90, "GL5528");
    const u32 _gl = fzx_part(_x, "ground", -270, -130, 0, "");
    fzx_route(_x, _rt, 1, _ic, 1u, NULL, 0);
    fzx_route(_x, _rt, 0, _rr, 0, FZX_VIA({ -360, 10 }));
    fzx_route(_x, _rt, 1, _ldr, 1, NULL, 0);
    fzx_route(_x, _ldr, 0, _gl, 0, NULL, 0);
    const u32 _pot = fzx_part(_x, "potentiometer", -150, -110, 0, "10k 50%");
    const u32 _pr  = fzx_part(_x, "supply rail", -210, -70, 0, "5V");
    const u32 _pg  = fzx_part(_x, "ground", -120, -160, 0, "");
    fzx_route(_x, _ic, 2u, _pot, 2, FZX_VIA({ -150, -10 }));
    fzx_route(_x, _pot, 0, _pr, 0, FZX_VIA({ -210, -120 }));
    fzx_route(_x, _pot, 1, _pg, 0, NULL, 0);
    const u32 _rl  = fzx_part(_x, "resistor", -170, 70, 0, "220");
    const u32 _led = fzx_sized(_x, "LED", -260, 70, 0, 60, 20, "white");
    const u32 _lr  = fzx_part(_x, "supply rail", -320, 100, 0, "5V");
    fzx_route(_x, _ic, 0u, _rl, 1, FZX_VIA({ -120, 30 }, { -120, 70 }));
    fzx_route(_x, _rl, 0, _led, 1, NULL, 0);
    fzx_route(_x, _led, 0, _lr, 0, FZX_VIA({ -320, 70 }));
    fzx_word(_x, -390, -190, 16.0, 520.0, FUDE_ZOOM_EXAMPLE_WORD_COMPARATOR);
    fzx_title(_x, -390, 170, FUDE_ZOOM_EXAMPLE_COMPARATOR);
}

// An SCR on 12 V in a row with a switch and a lamp: a logic input through 1k into its gate fires it, and it stays on when
// the gate lets go — the switch opened, it stops (closed again, it stays off). A TRIAC on 12 V AC with a lamp: a switch
// and 1k from its MT2 side into its gate fire it each half of each cycle.
RDE_INTERNAL void fzx_thyristors(fzx* _x) {
    const u32 _scr = fzx_part(_x, "SCR", 0, 100, 0, "C106");
    const u32 _bat = fzx_part(_x, "battery", 150, 100, 0, "12V");
    const u32 _sw  = fzx_part(_x, "SPST switch", 90, 160, 0, "on");
    const u32 _lp  = fzx_part(_x, "lamp", -20, 160, 0, "12V 5W");
    const u32 _gb  = fzx_part(_x, "ground", 190, 40, 0, "");
    fzx_route(_x, _bat, 0, _sw, 1, FZX_VIA({ 150, 160 }));
    fzx_route(_x, _sw, 0, _lp, 1, NULL, 0);
    fzx_route(_x, _lp, 0, _scr, 0, FZX_VIA({ -80, 160 }, { -80, 100 }));
    fzx_route(_x, _scr, 1, _bat, 1, FZX_VIA({ 90, 100 }, { 90, 70 }));
    fzx_route(_x, _bat, 1, _gb, 0, FZX_VIA({ 190, 70 }));
    const u32 _rg = fzx_part(_x, "resistor", 10, 45, 90, "1k");
    const u32 _gi = fzx_sized(_x, "logic input", -110, 15, 0, 60, 20, "0");
    fzx_route(_x, _rg, 1, _scr, 2, NULL, 0);
    fzx_route(_x, _gi, 0, _rg, 0, NULL, 0);
    fzx_text(_x, -180, 30, 14.0, 30.0, "G");
    const u32 _tr = fzx_part(_x, "TRIAC", 0, -150, 0, "BT136");
    const u32 _ac = fzx_part(_x, "AC source", 150, -150, 0, "12V 50Hz");
    const u32 _l2 = fzx_part(_x, "lamp", -20, -90, 0, "12V 5W");
    const u32 _ga = fzx_part(_x, "ground", 190, -210, 0, "");
    fzx_route(_x, _ac, 0, _l2, 1, FZX_VIA({ 150, -90 }));
    fzx_route(_x, _l2, 0, _tr, 0, FZX_VIA({ -80, -90 }, { -80, -150 }));
    fzx_route(_x, _tr, 1, _ac, 1, FZX_VIA({ 90, -150 }, { 90, -180 }));
    fzx_route(_x, _ac, 1, _ga, 0, FZX_VIA({ 190, -180 }));
    const u32 _r2 = fzx_part(_x, "resistor", -40, -200, 0, "1k");
    const u32 _s2 = fzx_part(_x, "SPST switch", -120, -200, 0, "off");
    fzx_route(_x, _tr, 2, _r2, 1, FZX_VIA({ 10, -200 }));
    fzx_route(_x, _r2, 0, _s2, 1, NULL, 0);
    fzx_route(_x, _s2, 0, _l2, 0, FZX_VIA({ -170, -200 }, { -170, -60 }, { -40, -60 }));
    fzx_word(_x, -200, -260, 16.0, 460.0, FUDE_ZOOM_EXAMPLE_WORD_THYRISTORS);
    fzx_title(_x, -200, 240, FUDE_ZOOM_EXAMPLE_THYRISTORS);
}

// A power supply: 24 V of AC into a 2:1 transformer, its secondary into a bridge rectifier (its − the ground), 1000 µF
// across it (about 10.5 V), a 7805 to 5 V, an LED on it through 330 Ω.
RDE_INTERNAL void fzx_power_supply(fzx* _x) {
    const u32 _ac = fzx_part(_x, "AC source", -300, 0, 0, "24V 50Hz");
    const u32 _tx = fzx_part(_x, "transformer", -150, 0, 0, "2:1");
    const u32 _g0 = fzx_part(_x, "ground", -340, -70, 0, "");
    fzx_route(_x, _ac, 0, _tx, 0, FZX_VIA({ -300, 50 }, { -210, 50 }, { -210, 20 }));
    fzx_route(_x, _ac, 1, _tx, 1, FZX_VIA({ -300, -50 }, { -210, -50 }, { -210, -20 }));
    fzx_route(_x, _ac, 1, _g0, 0, FZX_VIA({ -340, -30 }));
    const u32 _br = fzx_part(_x, "bridge rectifier", 0, 0, 0, "DB107");
    fzx_route(_x, _tx, 2, _br, 0, FZX_VIA({ -80, 20 }, { -80, 70 }, { 0, 70 }));
    fzx_route(_x, _tx, 3, _br, 2, FZX_VIA({ -70, -20 }, { -70, -70 }, { 0, -70 }));
    const u32 _g1 = fzx_part(_x, "ground", -50, -130, 0, "");
    fzx_route(_x, _br, 3, _g1, 0, FZX_VIA({ -50, 0 }));
    const u32 _cp = fzx_part(_x, "electrolytic", 100, -40, -90, "1000uF");
    const u32 _g2 = fzx_part(_x, "ground", 100, -100, 0, "");
    fzx_route(_x, _br, 1, _cp, 0, FZX_VIA({ 100, 0 }));
    fzx_route(_x, _cp, 1, _g2, 0, NULL, 0);
    const u32 _rg = fzx_part(_x, "7805", 220, 0, 0, "7805");
    const u32 _g3 = fzx_part(_x, "ground", 220, -60, 0, "");
    fzx_route(_x, _cp, 0, _rg, 0, FZX_VIA({ 160, -20 }, { 160, 10 }));
    fzx_route(_x, _rg, 1, _g3, 0, NULL, 0);
    const u32 _rl  = fzx_part(_x, "resistor", 310, 10, 0, "330");
    const u32 _led = fzx_sized(_x, "LED", 400, 10, 0, 60, 20, "green");
    const u32 _g4  = fzx_part(_x, "ground", 460, -20, 0, "");
    fzx_route(_x, _rg, 2, _rl, 0, NULL, 0);
    fzx_route(_x, _rl, 1, _led, 0, NULL, 0);
    fzx_route(_x, _led, 1, _g4, 0, FZX_VIA({ 460, 10 }));
    fzx_title(_x, -340, 150, FUDE_ZOOM_EXAMPLE_POWER_SUPPLY);
}

// Light. An optocoupler between two circuits that share nothing: a switch and 470 Ω from 5 V into its LED; its
// phototransistor sinking a green LED's current from a 9 V battery of its own. A photodiode backwards on 5 V through 100k,
// a phototransistor on 5 V through 1k, a voltmeter on each: as much light, as much less.
RDE_INTERNAL void fzx_light_link(fzx* _x) {
    const u32 _op = fzx_part(_x, "optocoupler", 0, 120, 0, "PC817");
    const u32 _vr = fzx_part(_x, "supply rail", -250, 170, 0, "5V");
    const u32 _sw = fzx_part(_x, "SPST switch", -200, 130, 0, "off");
    const u32 _ri = fzx_part(_x, "resistor", -110, 130, 0, "470");
    const u32 _gk = fzx_part(_x, "ground", -70, 70, 0, "");
    fzx_route(_x, _vr, 0, _sw, 0, FZX_VIA({ -250, 130 }));
    fzx_route(_x, _sw, 1, _ri, 0, NULL, 0);
    fzx_route(_x, _ri, 1, _op, 0, NULL, 0);
    fzx_route(_x, _op, 1, _gk, 0, FZX_VIA({ -70, 110 }));
    const u32 _bat = fzx_part(_x, "battery", 250, 120, 0, "9V");
    const u32 _ro  = fzx_part(_x, "resistor", 200, 170, 0, "1k");
    const u32 _led = fzx_sized(_x, "LED", 110, 170, 180, 60, 20, "green");
    fzx_route(_x, _bat, 0, _ro, 1, FZX_VIA({ 250, 170 }));
    fzx_route(_x, _ro, 0, _led, 0, NULL, 0);
    fzx_route(_x, _led, 1, _op, 2, FZX_VIA({ 70, 170 }, { 70, 130 }));
    fzx_route(_x, _op, 3, _bat, 1, FZX_VIA({ 80, 110 }, { 80, 80 }, { 250, 80 }));
    const u32 _pd = fzx_part(_x, "photodiode", -150, -100, 0, "BPW34");
    const u32 _r1 = fzx_part(_x, "resistor", -120, -30, 90, "100k");
    const u32 _v1 = fzx_part(_x, "supply rail", -120, 20, 0, "5V");
    const u32 _g1 = fzx_part(_x, "ground", -200, -140, 0, "");
    const u32 _m1 = fzx_part(_x, "voltmeter", -40, -100, 0, "");
    const u32 _g2 = fzx_part(_x, "ground", -20, -140, 0, "");
    fzx_route(_x, _pd, 1, _r1, 0, NULL, 0);
    fzx_route(_x, _r1, 1, _v1, 0, NULL, 0);
    fzx_route(_x, _pd, 0, _g1, 0, FZX_VIA({ -200, -100 }));
    fzx_route(_x, _pd, 1, _m1, 0, NULL, 0);
    fzx_route(_x, _m1, 1, _g2, 0, NULL, 0);
    const u32 _pt = fzx_part(_x, "phototransistor", 150, -110, 0, "TEPT5600");
    const u32 _r2 = fzx_part(_x, "resistor", 160, -20, 90, "1k");
    const u32 _v2 = fzx_part(_x, "supply rail", 160, 30, 0, "5V");
    const u32 _g3 = fzx_part(_x, "ground", 160, -180, 0, "");
    const u32 _m2 = fzx_part(_x, "voltmeter", 260, -110, 0, "");
    const u32 _g4 = fzx_part(_x, "ground", 280, -150, 0, "");
    fzx_route(_x, _pt, 0, _r2, 0, NULL, 0);
    fzx_route(_x, _r2, 1, _v2, 0, NULL, 0);
    fzx_route(_x, _pt, 1, _g3, 0, NULL, 0);
    fzx_route(_x, _pt, 0, _m2, 0, FZX_VIA({ 200, -80 }, { 200, -110 }));
    fzx_route(_x, _m2, 1, _g4, 0, NULL, 0);
    fzx_word(_x, -260, -200, 16.0, 560.0, FUDE_ZOOM_EXAMPLE_WORD_LIGHT);
    fzx_title(_x, -260, 240, FUDE_ZOOM_EXAMPLE_LIGHT_LINK);
}

// A 555 astable round (_cx, _cy) on 5 V (_volts): R1 _r1 and R2 _r2 (DISCH between them) and its capacitor _cap
// (_polar: an electrolytic) from THRES and TRIG to ground, about 1.44 / ((R1 + 2 R2) C). Its RESET to its supply when
// _reset_high; else left for what drives it. Its OUT (left, 10 under its middle) left for what it drives — its wires
// round its left and under it (none along its pins' ends). (Its pins: GND 30, TRIG 10, OUT −10, RESET −30 down its left,
// x −100; VCC 30, DISCH 10, THRES −10, CTRL −30 down its right, x 100.)
RDE_INTERNAL u32 fzx_astable(fzx* _x, f64 _cx, f64 _cy, const c8* _volts, const c8* _r1, const c8* _r2, const c8* _cap, b8 _polar, b8 _reset_high) {
    const u32 _t  = fzx_part(_x, "NE555", _cx, _cy, 0, "NE555");
    const u32 _va = fzx_part(_x, "supply rail", _cx + 140, _cy + 70, 0, _volts);
    fzx_route(_x, _t, 7, _va, 0, FZX_VIA({ _cx + 140, _cy + 30 }));
    const u32 _a1 = fzx_part(_x, "resistor", _cx + 220, _cy + 40, 90, _r1);
    const u32 _vb = fzx_part(_x, "supply rail", _cx + 220, _cy + 110, 0, _volts);
    fzx_route(_x, _a1, 1, _vb, 0, NULL, 0);
    fzx_route(_x, _t, 6, _a1, 0, NULL, 0);
    const u32 _a2 = fzx_part(_x, "resistor", _cx + 220, _cy - 40, 90, _r2);
    fzx_route(_x, _a1, 0, _a2, 1, NULL, 0);
    fzx_route(_x, _t, 5, _a2, 0, FZX_VIA({ _cx + 160, _cy - 10 }, { _cx + 160, _cy - 70 }));
    const u32 _c  = fzx_part(_x, _polar ? "electrolytic" : "capacitor", _cx + 220, _cy - 130, -90, _cap);
    const u32 _gc = fzx_part(_x, "ground", _cx + 220, _cy - 190, 0, "");
    fzx_route(_x, _a2, 0, _c, 0, NULL, 0);
    fzx_route(_x, _c, 1, _gc, 0, NULL, 0);
    fzx_route(_x, _t, 1, _c, 0, FZX_VIA({ _cx - 140, _cy + 10 }, { _cx - 140, _cy - 110 }));
    if(_reset_high) {
        const u32 _vc = fzx_part(_x, "supply rail", _cx - 40, _cy - 90, 0, _volts);
        fzx_route(_x, _t, 3, _vc, 0, FZX_VIA({ _cx - 120, _cy - 30 }, { _cx - 120, _cy - 100 }));
    }
    const u32 _gg = fzx_part(_x, "ground", _cx - 180, _cy + 50, 0, "");
    fzx_route(_x, _t, 0, _gg, 0, FZX_VIA({ _cx - 130, _cy + 30 }, { _cx - 130, _cy + 90 }, { _cx - 180, _cy + 90 }));
    return _t;
}

// A 555 astable (as the flasher's, 1k and 15k, 100 nF: about 460 Hz) on 9 V into a speaker through 470 Ω: heard as it
// plays.
RDE_INTERNAL void fzx_speaker_tone(fzx* _x) {
    const u32 _t  = fzx_astable(_x, 0, 0, "9V", "1k", "15k", "100nF", false, true);
    const u32 _rl = fzx_part(_x, "resistor", -230, -10, 0, "470");
    const u32 _sp = fzx_part(_x, "speaker", -310, -60, -90, "8 ohm");
    const u32 _gs = fzx_part(_x, "ground", -370, -40, 0, "");
    fzx_route(_x, _t, 2, _rl, 1, NULL, 0);
    fzx_route(_x, _rl, 0, _sp, 0, FZX_VIA({ -300, -10 }));
    fzx_route(_x, _sp, 1, _gs, 0, FZX_VIA({ -320, 0 }, { -370, 0 }));
    fzx_title(_x, -370, 170, FUDE_ZOOM_EXAMPLE_SPEAKER_TONE);
}

// A crossroads' traffic lights, as they were made before microcontrollers: a 555 (about 1.4 Hz) steps a CD4017 through
// its ten outputs, and a DIODE MATRIX (a ROM of 39 diodes) makes of them every light — each output's lane down through a
// diode into each line it lights (N–S red 5–9, yellow 4, green 0–3; E–W red 0–4, yellow 9, green 5–8; the pedestrians'
// WALK 5–8), and into the lines of its number in binary (1, 2, 4, 8) for a CD4511 and a digit: the step shown. While they
// may walk, a second 555 (RESET on WALK, pulled down) beeps on a speaker. INH and RST (logic inputs) hold it or start it
// again. Each line pulled down where nothing lights it (10k), each diode's anode on its lane, its cathode on its line.
RDE_INTERNAL void fzx_traffic(fzx* _x) {
    const u32 _ch = fzx_part(_x, "CD4017", 0, 0, 0, "CD4017");
    const u32 _vr = fzx_part(_x, "supply rail", 140, 110, 0, "5V");
    fzx_route(_x, _ch, 15u, _vr, 0, FZX_VIA({ 140, 70 }));
    const u32 _gv = fzx_part(_x, "ground", -130, -120, 0, "");
    fzx_route(_x, _ch, 7u, _gv, 0, FZX_VIA({ -130, -70 }));
    // (INH and RST: low, tapped to hold it or to start it again)
    const u32 _inh = fzx_sized(_x, "logic input", 170, 10, 180, 60, 16, "0");
    const u32 _rst = fzx_sized(_x, "logic input", 170, 50, 180, 60, 16, "0");
    fzx_route(_x, _inh, 0, _ch, 12u, NULL, 0);
    fzx_route(_x, _rst, 0, _ch, 14u, NULL, 0);
    fzx_text(_x, 210, 20, 13.0, 40.0, "INH");
    fzx_text(_x, 210, 60, 13.0, 40.0, "RST");
    // Its clock: a 555, 10k and 47k, 10 µF.
    const u32 _clk = fzx_astable(_x, 620, 280, "5V", "10k", "47k", "10uF", true, true);
    fzx_route(_x, _clk, 2, _ch, 13u, FZX_VIA({ 360, 270 }, { 360, 30 }));
    // The matrix: each output's lane (its count, its pin, its x — the higher its pin, the farther out, none crossing) and
    // the lines it is in.
    static const u32 _pin[10]  = { 2, 1, 3, 6, 9, 0, 4, 5, 8, 10 };   // (count 0 to 9)
    static const f64 _lane[10] = { -440, -510, -370, -160, 230, -580, -300, -230, 160, 300 };
    enum { NS_R, NS_Y, NS_G, EW_R, EW_Y, EW_G, BIT_2, BIT_4, BIT_8, BIT_1, WALK, LINES };
    static const u16 _of[LINES] = { 0x3E0, 0x010, 0x00F, 0x01F, 0x200, 0x1E0, 0x0CC, 0x0F0, 0x300, 0x2AA, 0x1E0 };   // (bit n: count n)
    u32 _last[LINES];   // (each line's rightmost diode)
    u32 _diode[10][LINES];
    for(u32 _n = 0; _n < 10u; _n++) {
        const f64 _lx = _lane[_n];
        u32 _above = FUDE_ZOOM_NONE;
        for(u32 _l = 0; _l < LINES; _l++) {
            _diode[_n][_l] = FUDE_ZOOM_NONE;
            if(!(_of[_l] >> _n & 1u)) {
                continue;
            }
            // (upright: its anode on the lane at the line's row, its cathode 60 under it, on the line)
            const f64 _ya = -200.0 - 80.0 * (f64)_l;
            const u32 _d = fzx_part(_x, "diode", _lx, _ya - 30.0, -90, "1N4148");
            _diode[_n][_l] = _d;
            if(_above == FUDE_ZOOM_NONE) {
                const fude_zoom_v2 _at = fzx_pin(_x, _ch, _pin[_n]);
                fzx_route(_x, _ch, _pin[_n], _d, 0, FZX_VIA({ _lx, _at.y }));
            } else {
                const f64 _yb = -200.0 - 80.0 * (f64)_above;
                fzx_route(_x, _diode[_n][_above], 0, _d, 0, FZX_VIA({ _lx - 30.0, _yb }, { _lx - 30.0, _ya }));
            }
            _above = _l;
        }
    }
    // (each line: its cathodes joined left to right)
    for(u32 _l = 0; _l < LINES; _l++) {
        u32 _prev = FUDE_ZOOM_NONE;
        f64 _prev_x = -1e9;
        for(u32 _k = 0; _k < 10u; _k++) {
            // (lanes from the left)
            u32 _best = FUDE_ZOOM_NONE;
            f64 _bx = 1e9;
            for(u32 _n = 0; _n < 10u; _n++) {
                if(_diode[_n][_l] != FUDE_ZOOM_NONE && _lane[_n] > _prev_x && _lane[_n] < _bx) {
                    _best = _n;
                    _bx = _lane[_n];
                }
            }
            if(_best == FUDE_ZOOM_NONE) {
                break;
            }
            if(_prev != FUDE_ZOOM_NONE) {
                fzx_route(_x, _diode[_prev][_l], 1, _diode[_best][_l], 1, NULL, 0);
            }
            _prev = _best;
            _prev_x = _bx;
        }
        _last[_l] = _prev != FUDE_ZOOM_NONE ? _diode[_prev][_l] : FUDE_ZOOM_NONE;
    }
    // The lights: each line through 1k into its LED (a CD4017's output gives 10 mA at most, and one lights two or three)
    // — N–S's red, yellow, green over each other, E–W's under them.
    static const c8* const _colour[6] = { "red", "yellow", "green", "red", "yellow", "green" };
    for(u32 _l = NS_R; _l <= EW_G; _l++) {
        const f64 _y = -260.0 - 80.0 * (f64)_l;
        const u32 _r = fzx_part(_x, "resistor", 420, _y, 0, "1k");
        const u32 _led = fzx_sized(_x, "LED", 510, _y, 0, 60, 30, _colour[_l]);
        const u32 _g = fzx_sized(_x, "ground", 575, _y - 32.0, 0, 24, 24, "");
        fzx_route(_x, _last[_l], 1, _r, 0, NULL, 0);
        fzx_route(_x, _r, 1, _led, 0, NULL, 0);
        fzx_route(_x, _led, 1, _g, 0, FZX_VIA({ 575, _y }));
    }
    // (their housings, a box round each three)
    for(u32 _h = 0; _h < 2u; _h++) {
        // (round its three lights and the names under them, apart from the other's; its own name by its
        // top, right of the grounds — between the two there is no room for it)
        const f64 _top = -260.0 - 240.0 * (f64)_h + 24.0, _bottom = _top - 232.0;
        const fude_zoom_v2 _box[4] = { fzx_at(_x, 470, _top), fzx_at(_x, 550, _top), fzx_at(_x, 550, _bottom), fzx_at(_x, 470, _bottom) };
        fzx_path(_x, _box, 4u, true, 2.0);
        fzx_text(_x, 594, _top, 15.0, 60.0, _h == 0u ? "N\xE2\x80\x93S" : "E\xE2\x80\x93W");
    }
    // The step in binary into a CD4511 (B, C, D, A: its pins top down), each line pulled down by 10k; its digit.
    const f64 _dy = -840.0;
    const u32 _dec = fzx_part(_x, "CD4511", 720, _dy, 0, "CD4511");
    static const u32 _bits[4] = { BIT_2, BIT_4, BIT_8, BIT_1 };
    static const u32 _bit_pin[4] = { 0u, 1u, 5u, 6u };   // (B, C, D, A)
    static const f64 _bit_x[4] = { 520, 520, 520, 540 };
    for(u32 _b = 0; _b < 4u; _b++) {
        const f64 _y = -260.0 - 80.0 * (f64)_bits[_b];
        const u32 _pd = fzx_sized(_x, "resistor", 400, _y - 20.0, 90, 40, 14, "10k");
        const u32 _g  = fzx_sized(_x, "ground", 400, _y - 64.0, 0, 24, 24, "");
        fzx_route(_x, _last[_bits[_b]], 1, _pd, 1, NULL, 0);
        fzx_route(_x, _pd, 0, _g, 0, NULL, 0);
        const fude_zoom_v2 _in = fzx_pin(_x, _dec, _bit_pin[_b]);
        fzx_route(_x, _pd, 1, _dec, _bit_pin[_b], FZX_VIA({ _bit_x[_b], _y }, { _bit_x[_b], _in.y }));
    }
    static const c8* const _sets[3] = { "1", "1", "0" };   // (~LT, ~BI high, LE low)
    for(u32 _k = 0; _k < 3u; _k++) {
        const fude_zoom_v2 _p = fzx_pin(_x, _dec, 2u + _k);
        const u32 _in = fzx_sized(_x, "logic input", _p.x - 40.0, _p.y, 0, 60, 16, _sets[_k]);
        fzx_line(_x, _in, 0, _dec, 2u + _k);
    }
    const u32 _dg = fzx_part(_x, "ground", 600, _dy - 110.0, 0, "");
    fzx_route(_x, _dec, 7u, _dg, 0, FZX_VIA({ 600, _dy - 70.0 }));
    const u32 _dv = fzx_part(_x, "supply rail", 840, _dy + 110.0, 0, "5V");
    fzx_route(_x, _dec, 15u, _dv, 0, FZX_VIA({ 840, _dy + 70.0 }));
    const u32 _dig = fzx_part(_x, "7-segment panel", 1120, _dy, 0, "1 digit red");
    static const u32 _out[7] = { 12u, 11u, 10u, 9u, 8u, 14u, 13u };   // (a b c d e f g)
    static const f64 _seg_lane[7] = { 990, 1000, 1010, 1020, 1030, 980, 970 };
    for(u32 _g = 0; _g < 7u; _g++) {
        const fude_zoom_v2 _o = fzx_pin(_x, _dec, _out[_g]), _in = fzx_pin(_x, _dig, _g);
        const u32 _r = fzx_sized(_x, "resistor", 900, _o.y, 0, 50, 14, "330");
        fzx_line(_x, _dec, _out[_g], _r, 0);
        fzx_route(_x, _r, 1, _dig, _g, FZX_VIA({ _seg_lane[_g], _o.y }, { _seg_lane[_g], _in.y }));
    }
    const fude_zoom_v2 _d1 = fzx_pin(_x, _dig, 8u);
    const u32 _gd = fzx_part(_x, "ground", _d1.x, _d1.y - 50.0, 0, "");
    fzx_line(_x, _dig, 8u, _gd, 0);
    // WALK: pulled down, a white LED through 1k, and the beeper's RESET — a 555 (1k, 6.8k, 100 nF: about 1 kHz) into a
    // speaker through 220 Ω.
    const f64 _wy = -260.0 - 80.0 * (f64)WALK;
    const u32 _wpd = fzx_sized(_x, "resistor", 400, _wy - 20.0, 90, 40, 14, "10k");
    const u32 _wg  = fzx_sized(_x, "ground", 400, _wy - 64.0, 0, 24, 24, "");
    fzx_route(_x, _last[WALK], 1, _wpd, 1, NULL, 0);
    fzx_route(_x, _wpd, 0, _wg, 0, NULL, 0);
    const u32 _wr  = fzx_part(_x, "resistor", 480, _wy, 0, "1k");
    const u32 _wl  = fzx_sized(_x, "LED", 570, _wy, 0, 60, 30, "white");
    const u32 _wlg = fzx_sized(_x, "ground", 635, _wy - 32.0, 0, 24, 24, "");
    fzx_route(_x, _wpd, 1, _wr, 0, NULL, 0);
    fzx_route(_x, _wr, 1, _wl, 0, NULL, 0);
    fzx_route(_x, _wl, 1, _wlg, 0, FZX_VIA({ 635, _wy }));
    fzx_text(_x, 545, _wy + 34.0, 14.0, 60.0, "WALK");
    // (the beeper right of WALK's resistor, its RESET from it; its OUT left across to the speaker)
    const f64 _by = _wy - 300.0;
    const u32 _bp = fzx_astable(_x, 800, _by, "5V", "1k", "6.8k", "100nF", false, false);
    fzx_route(_x, _wr, 0, _bp, 3, FZX_VIA({ 450, _by - 30.0 }));
    const u32 _rs = fzx_part(_x, "resistor", 330, _by - 10.0, 0, "220");
    const u32 _sp = fzx_part(_x, "speaker", 220, _by - 60.0, -90, "8 ohm");
    const u32 _gs = fzx_part(_x, "ground", 160, _by - 30.0, 0, "");
    fzx_route(_x, _bp, 2, _rs, 1, NULL, 0);
    fzx_route(_x, _rs, 0, _sp, 0, FZX_VIA({ 230, _by - 10.0 }));
    fzx_route(_x, _sp, 1, _gs, 0, FZX_VIA({ 210, _by + 10.0 }, { 160, _by + 10.0 }));
    fzx_word(_x, -620, 300, 16.0, 640.0, FUDE_ZOOM_EXAMPLE_WORD_TRAFFIC);
    fzx_title(_x, -620, 400, FUDE_ZOOM_EXAMPLE_TRAFFIC);
}

// A 555 piano: R1 1k from 9 V to DISCH, and from DISCH thirteen keys — each a resistor and a push button — to THRES and
// TRIG's capacitor (100 nF): held, a key is R2, its note about 1.44 / ((R1 + 2 R2) C) — C4 to C5, a semitone a key, its
// frequency under it. Its OUT through 100 Ω and a volume pot (a rheostat: tapped, a quarter round) into 10 µF and a
// speaker — silent while no key is held (OUT high, nothing through the capacitor). An oscilloscope on OUT.
RDE_INTERNAL void fzx_piano(fzx* _x) {
    const u32 _t  = fzx_part(_x, "NE555", 0, 0, 0, "NE555");
    const u32 _va = fzx_part(_x, "supply rail", 140, 70, 0, "9V");
    fzx_route(_x, _t, 7, _va, 0, FZX_VIA({ 140, 30 }));
    const u32 _r1 = fzx_part(_x, "resistor", 220, 40, 90, "1k");
    const u32 _vb = fzx_part(_x, "supply rail", 220, 110, 0, "9V");
    fzx_route(_x, _r1, 1, _vb, 0, NULL, 0);
    fzx_route(_x, _t, 6, _r1, 0, NULL, 0);
    const u32 _vc = fzx_part(_x, "supply rail", -40, -90, 0, "9V");
    fzx_route(_x, _t, 3, _vc, 0, FZX_VIA({ -120, -30 }, { -120, -100 }));
    const u32 _gg = fzx_part(_x, "ground", -180, 50, 0, "");
    fzx_route(_x, _t, 0, _gg, 0, FZX_VIA({ -130, 30 }, { -130, 90 }, { -180, 90 }));
    // (THRES down to the timing capacitor, TRIG round under the chip to it, the keys' lower line from it)
    const u32 _c  = fzx_part(_x, "capacitor", 160, -130, -90, "100nF");
    const u32 _gc = fzx_part(_x, "ground", 160, -190, 0, "");
    fzx_route(_x, _t, 5, _c, 0, FZX_VIA({ 160, -10 }));
    fzx_route(_x, _c, 1, _gc, 0, NULL, 0);
    fzx_route(_x, _t, 1, _c, 0, FZX_VIA({ -140, 10 }, { -140, -110 }));
    // The keys: R2 from DISCH's line (y 10) down to a button, the button down to THRES's line (y −140).
    static const c8* const _r2[13]  = { "27k", "25.5k", "24k", "22.6k", "21.3k", "20.1k", "19k", "17.9k", "16.8k", "15.9k", "14.9k", "14.1k", "13.3k" };
    static const c8* const _hz[13]  = { "262", "277", "294", "311", "330", "349", "370", "392", "415", "440", "466", "494", "523" };
    static const b8 _black[13] = { false, true, false, true, false, false, true, false, true, false, true, false, false };
    u32 _prev_r = _r1, _prev_b = _c;
    for(u32 _k = 0; _k < 13u; _k++) {
        const f64 _kx = 300.0 + 70.0 * (f64)_k;
        const u32 _r = fzx_part(_x, "resistor", _kx, -20, 90, _r2[_k]);
        const u32 _b = fzx_part(_x, "push button", _kx, -110, 90, "");
        fzx_route(_x, _prev_r, _k == 0u ? 0u : 1u, _r, 1, NULL, 0);
        fzx_route(_x, _r, 0, _b, 1, NULL, 0);
        if(_k == 0u) {
            fzx_route(_x, _prev_b, 0, _b, 0, FZX_VIA({ 240, -110 }, { 240, -140 }));
        } else {
            fzx_route(_x, _prev_b, 0, _b, 0, NULL, 0);
        }
        _prev_r = _r;
        _prev_b = _b;
        // (its key round it: a white one tall, a black one short; its note's frequency under it)
        const f64 _hw = _black[_k] ? 24.0 : 32.0, _low = _black[_k] ? -160.0 : -215.0;
        const fude_zoom_v2 _key[4] = { fzx_at(_x, _kx - _hw, -76), fzx_at(_x, _kx + _hw, -76), fzx_at(_x, _kx + _hw, _low), fzx_at(_x, _kx - _hw, _low) };
        fzx_path(_x, _key, 4u, true, _black[_k] ? 3.0 : 1.5);
        fzx_text(_x, _kx - 14.0, _black[_k] ? -172.0 : -228.0, 13.0, 40.0, _hz[_k]);
    }
    fzx_text(_x, 1180, -228, 13.0, 40.0, "Hz");
    // OUT: 100 Ω, the volume pot (from its right end to its wiper), 10 µF, the speaker.
    const u32 _rs = fzx_part(_x, "resistor", -200, -10, 0, "100");
    const u32 _pot = fzx_part(_x, "potentiometer", -300, -20, 0, "1k 75%");
    const u32 _cc = fzx_part(_x, "electrolytic", -360, 40, 180, "10uF");
    const u32 _sp = fzx_part(_x, "speaker", -420, -20, -90, "8 ohm");
    const u32 _gs = fzx_part(_x, "ground", -470, -10, 0, "");
    fzx_route(_x, _t, 2, _rs, 1, NULL, 0);
    fzx_route(_x, _rs, 0, _pot, 1, FZX_VIA({ -250, -10 }, { -250, -30 }));
    fzx_route(_x, _pot, 2, _cc, 0, FZX_VIA({ -300, 40 }));
    fzx_route(_x, _cc, 1, _sp, 0, FZX_VIA({ -410, 40 }));
    fzx_route(_x, _sp, 1, _gs, 0, FZX_VIA({ -430, 30 }, { -470, 30 }));
    fzx_text(_x, -290, 30, 13.0, 40.0, "VOL");
    // (the oscilloscope on OUT, from under its resistor)
    const u32 _sc = fzx_part(_x, "oscilloscope", 150, -320, 0, "1ms 5V");
    const u32 _gsc = fzx_part(_x, "ground", -40, -400, 0, "");
    fzx_route(_x, _rs, 1, _sc, 0, FZX_VIA({ -170, -280 }));
    fzx_route(_x, _sc, 2, _gsc, 0, FZX_VIA({ -40, -360 }));
    fzx_word(_x, -480, -470, 16.0, 760.0, FUDE_ZOOM_EXAMPLE_WORD_PIANO);
    fzx_title(_x, -480, 180, FUDE_ZOOM_EXAMPLE_PIANO);
}

// Two circles' crossing nearest _near (_a's, _ra round; _b's, _rb round); _near itself where they do not cross.
RDE_INTERNAL fude_zoom_v2 fzx_crossing(fude_zoom_v2 _a, f64 _ra, fude_zoom_v2 _b, f64 _rb, fude_zoom_v2 _near) {
    const f64 _dx = _b.x - _a.x, _dy = _b.y - _a.y, _d = hypot(_dx, _dy);
    if(_d <= 0.0 || _d > _ra + _rb || _d < fabs(_ra - _rb)) {
        return _near;
    }
    const f64 _along = (_ra * _ra - _rb * _rb + _d * _d) / (2.0 * _d), _h = sqrt(fmax(_ra * _ra - _along * _along, 0.0));
    const f64 _mx = _a.x + _along * _dx / _d, _my = _a.y + _along * _dy / _d;
    const fude_zoom_v2 _p = { _mx + _h * _dy / _d, _my - _h * _dx / _d }, _q = { _mx - _h * _dy / _d, _my + _h * _dx / _d };
    return hypot(_p.x - _near.x, _p.y - _near.y) <= hypot(_q.x - _near.x, _q.y - _near.y) ? _p : _q;
}

// Theo Jansen's leg (his "holy numbers": the bars' lengths), its crank 15 round (0, 0), its fixed pivot at (−38, −7.8):
// its joints with its crank turned _turn (radians, counter-clockwise from the right) — followed there from where they
// are with it at 0, a degree at a time, each the crossing nearest where it was (a linkage does not jump to its other
// way of closing). a: the crank's pin; b over and c under the pivot (on the crank by j and k); d the upper triangle's
// outer corner (the pivot's, b's); f the lower triangle's, from d; g its foot.
typedef struct {
    fude_zoom_v2 a, b, c, d, f, g;
} fzx_jansen_leg;

RDE_INTERNAL fzx_jansen_leg fzx_jansen(f64 _turn) {
    const f64 _lb = 41.5, _lc = 39.3, _ld = 40.1, _le = 55.8, _lf = 39.4, _lg = 36.7, _lh = 65.7, _li = 49.0, _lj = 50.0, _lk = 61.9, _lm = 15.0;
    const fude_zoom_v2 _p = { -38.0, -7.8 };
    fzx_jansen_leg _l = { { 15.0, 0.0 }, { -24.01, 31.27 }, { -26.95, -45.52 }, { -74.79, 8.14 }, { -59.23, -28.05 }, { -43.16, -91.76 } };
    const u32 _steps = (u32)fmax(ceil(fabs(_turn) / FZX_DEG), 1.0);
    for(u32 _s = 1; _s <= _steps; _s++) {
        const f64 _t = _turn * (f64)_s / (f64)_steps;
        _l.a = (fude_zoom_v2){ _lm * cos(_t), _lm * sin(_t) };
        _l.b = fzx_crossing(_l.a, _lj, _p, _lb, _l.b);
        _l.c = fzx_crossing(_l.a, _lk, _p, _lc, _l.c);
        _l.d = fzx_crossing(_l.b, _le, _p, _ld, _l.d);
        _l.f = fzx_crossing(_l.d, _lf, _l.c, _lg, _l.f);
        _l.g = fzx_crossing(_l.f, _lh, _l.c, _li, _l.g);
    }
    return _l;
}

// A link (a bar _thick thick) with its holes on _a and _b.
RDE_INTERNAL u32 fzx_bar(fzx* _x, fude_zoom_v2 _a, fude_zoom_v2 _b, f64 _thick) {
    const f64 _len = hypot(_b.x - _a.x, _b.y - _a.y);
    return fzx_sized(_x, "link", 0.5 * (_a.x + _b.x), 0.5 * (_a.y + _b.y), atan2(_b.y - _a.y, _b.x - _a.x) / FZX_DEG, _len + _thick, _thick, "");
}

// The Strandbeest's legs: two of Jansen's legs, four times his size, on one motor's two cranks half a turn apart — the
// left one's at 90°, the right one the left one mirrored (its crank at 270°: the mirror of a leg at −90°) —, each on
// its own fixed pivot; a tracer on each foot.
RDE_INTERNAL void fzx_strandbeest(fzx* _x) {
    const f64 _k = 4.0, _thick = 20.0;
    fzx_part(_x, "drive motor", 0, 0, 0, "15 rpm 100");
    for(u32 _side = 0; _side < 2u; _side++) {
        const f64 _m = _side == 0u ? 1.0 : -1.0;
        const fzx_jansen_leg _l = fzx_jansen(_side == 0u ? 90.0 * FZX_DEG : -90.0 * FZX_DEG);
        #define FZX_J(_v) ((fude_zoom_v2){ _m * _k * (_v).x, _k * (_v).y })
        const fude_zoom_v2 _o = { 0.0, 0.0 }, _p = FZX_J(((fude_zoom_v2){ -38.0, -7.8 }));
        const fude_zoom_v2 _a = FZX_J(_l.a), _b = FZX_J(_l.b), _c = FZX_J(_l.c), _d = FZX_J(_l.d), _f = FZX_J(_l.f), _g = FZX_J(_l.g);
        #undef FZX_J
        fzx_part(_x, "fixed pivot", _p.x, _p.y - 10.0, 0, "");   // (its hole 10 over its middle)
        fzx_bar(_x, _o, _a, _thick);   // (its crank)
        fzx_bar(_x, _a, _b, _thick);   // j
        fzx_bar(_x, _a, _c, _thick);   // k
        fzx_bar(_x, _p, _b, _thick);   // b
        fzx_bar(_x, _p, _c, _thick);   // c
        fzx_bar(_x, _b, _d, _thick);   // e
        fzx_bar(_x, _p, _d, _thick);   // d
        fzx_bar(_x, _d, _f, _thick);   // f
        fzx_bar(_x, _c, _f, _thick);   // g
        fzx_bar(_x, _f, _g, _thick);   // h
        fzx_bar(_x, _c, _g, _thick);   // i
        fzx_part(_x, "tracer", _g.x, _g.y, 0, "");
    }
    fzx_word(_x, -360, -440, 16.0, 720.0, FUDE_ZOOM_EXAMPLE_WORD_STRANDBEEST);
    fzx_title(_x, -360, 260, FUDE_ZOOM_EXAMPLE_STRANDBEEST);
}

// A belt (or a chain) whose ends' middles are on _a and _b (40 across).
RDE_INTERNAL u32 fzx_belt(fzx* _x, const c8* _id, fude_zoom_v2 _a, fude_zoom_v2 _b, const c8* _text) {
    const f64 _len = hypot(_b.x - _a.x, _b.y - _a.y);
    return fzx_sized(_x, _id, 0.5 * (_a.x + _b.x), 0.5 * (_a.y + _b.y), atan2(_b.y - _a.y, _b.x - _a.x) / FZX_DEG, _len + 40.0, 40.0, _text);
}

// Which way a drive motor turns what is on it (+1: counter-clockwise), and so the crank and the cams below.
#define FZX_ENGINE_TURN 1.0

// A four-stroke engine, its cylinder upright: a starter motor turning a crank (50) and a 30-tooth gear; a rod (170) up to
// the piston, a slider on a rail between the cylinder's walls (its top at the start: the intake stroke's beginning). The
// gear turns a 30-tooth gear either side of it (out of the cylinder's way), each with a small sprocket on it and a belt
// up to a camshaft's sprocket twice its size: the camshafts at half the crank's speed, as a four-stroke's are, each with
// a cam on it opening a valve (a follower toward the cylinder's head) — the left one (intake) as the piston first goes
// down, the right one (exhaust) as it last comes up: its lobe pointing at its valve a quarter turn of the crank after
// the start, and three and a half turns. A tracer on the rod's middle.
RDE_INTERNAL void fzx_engine(fzx* _x) {
    const f64 _r = 50.0, _rod = 170.0;
    fzx_part(_x, "drive motor", 0, 0, 0, "30 rpm 200");
    fzx_part(_x, "gear 30T", 0, 0, 0, "");
    const fude_zoom_v2 _o = { 0.0, 0.0 }, _a = { 0.0, _r }, _w = { 0.0, _r + _rod };
    fzx_bar(_x, _o, _a, 18.0);
    fzx_bar(_x, _a, _w, 16.0);
    fzx_part(_x, "tracer", 0.0, _r + 0.5 * _rod, 0, "");
    fzx_sized(_x, "rail", 0, 170, 90, 220, 16, "");   // (as long as the piston goes, and its own width: what it slides between)
    fzx_sized(_x, "slider", _w.x, _w.y, 0, 100, 60, "");
    for(u32 _side = 0; _side < 2u; _side++) {
        const f64 _m = _side == 0u ? -1.0 : 1.0;
        fzx_sized(_x, "wall", _m * 62.0, 220, 90, 220, 20, "");   // (the cylinder's walls)
        // (the side gear, its sprocket, the belt up to the camshaft's)
        const fude_zoom_v2 _g = { _m * 150.0, 0.0 }, _s = { _m * 150.0, 520.0 };
        fzx_part(_x, "gear 30T", _g.x, _g.y, 0, "");
        fzx_sized(_x, "sprocket", _g.x, _g.y, 0, 60, 60, "");
        fzx_sized(_x, "sprocket", _s.x, _s.y, 0, 120, 120, "");
        fzx_belt(_x, "belt", _g, _s, "");
        // (its valve: a follower from the camshaft toward the head, its roller on the cam)
        const fude_zoom_v2 _seat = { _m * 25.0, 310.0 };
        const f64 _ul = hypot(_seat.x - _s.x, _seat.y - _s.y);
        const fude_zoom_v2 _u = { (_seat.x - _s.x) / _ul, (_seat.y - _s.y) / _ul };
        fzx_sized(_x, "follower", _s.x + 107.0 * _u.x, _s.y + 107.0 * _u.y, atan2(-_u.y, -_u.x) / FZX_DEG, 160, 30, "");
        // (its cam: the camshaft turns -1/2 as the crank — the side gear turns it the other way —; its lobe on its valve
        // after the crank's quarter turn (intake), or its three and a half (exhaust))
        const f64 _lobe = atan2(_u.y, _u.x) / FZX_DEG + FZX_ENGINE_TURN * 0.5 * (_side == 0u ? 90.0 : 630.0);
        fzx_sized(_x, "cam", _s.x + 0.285 * 40.0 * cos(_lobe * FZX_DEG), _s.y + 0.285 * 40.0 * sin(_lobe * FZX_DEG), _lobe, 80, 80, "");
    }
    fzx_word(_x, -360, -180, 16.0, 720.0, FUDE_ZOOM_EXAMPLE_WORD_ENGINE);
    fzx_title(_x, -360, 680, FUDE_ZOOM_EXAMPLE_ENGINE);
}

RDE_INTERNAL u32 fzx_motor(fzx* _x, f64 _px, f64 _py, f64 _turn, const c8* _text, const c8* _gear);

// A solenoid engine: a big solenoid (400 across: its plunger's stroke 60) pulling a rod (200) on a hand crank's pin (27
// round: its plunger out, all the way, with the pin furthest from it — where it is drawn; the crank's axle nearly free,
// 0.1 N·m, a flywheel's). A cam on the crank's shaft blocks a slotted sensor's beam, under the shaft, for the half turn the
// pin comes toward the solenoid going clockwise (the pin under the shaft, from 5° after it is furthest): the sensor's C,
// pulled up and smoothed (10 µF: what flickers as the cam's edge passes the beam left out), through a buffer, a switch
// (its ignition) and 1k into a TIP120 that switches the coil on 12 V, a diode across it. It starts by itself: its rod's
// weight lets its pin down into that half; the faster it goes, the later the smoothing lets its coil on and off (100 ms)
// — a speed it keeps. A panel meter in its supply; a dynamo on its crank's belt its tachometer (its volts on a panel
// meter); a 74HC161 counting its turns on four LEDs (8 4 2 1); an oscilloscope on the sensor.
RDE_INTERNAL void fzx_solenoid_engine(fzx* _x) {
    const f64 _r = 27.0, _rod = 200.0, _tip = 170.0, _cx = _tip + _rod - _r;
    const u32 _so = fzx_sized(_x, "solenoid", 0, 0, 0, 400, 120, "12V");
    fzx_sized(_x, "hand crank", _cx, 0, 0, 2.0 * _r / 0.7, 2.0 * _r / 0.7, "0.1");   // (its axle nearly free: 0.1 N·m)
    fzx_bar(_x, (fude_zoom_v2){ _tip, 0.0 }, (fude_zoom_v2){ _cx + _r, 0.0 }, 30.0);

    const f64 _lobe = 5.0 * FZX_DEG, _cam = 200.0, _e = 0.285 * 0.5 * _cam, _rim = 0.95 * 0.5 * _cam;
    fzx_sized(_x, "cam", _cx + _e * cos(_lobe), _e * sin(_lobe), _lobe / FZX_DEG, _cam, _cam, "");
    // (the sensor's beam straight under the shaft, where the cam's edge passes it a quarter turn either side of its lobe)
    const f64 _beam = sqrt(_rim * _rim - _e * _e);
    const u32 _sl = fzx_part(_x, "slotted sensor", _cx, -_beam - 9.0, 0, "ITR9608");
    const fude_zoom_v2 _sa = fzx_pin(_x, _sl, 0), _sk = fzx_pin(_x, _sl, 1), _sc = fzx_pin(_x, _sl, 2), _se = fzx_pin(_x, _sl, 3);
    const u32 _rl  = fzx_part(_x, "resistor", _sa.x - 40.0, _sa.y, 0, "330");
    const u32 _vl  = fzx_part(_x, "supply rail", _sa.x - 70.0, _sa.y + 40.0, 0, "5V");
    fzx_route(_x, _rl, 1, _sl, 0, NULL, 0);
    fzx_route(_x, _rl, 0, _vl, 0, NULL, 0);
    const u32 _gk = fzx_part(_x, "ground", _sk.x - 28.0, _sk.y - 35.0, 0, "");
    fzx_route(_x, _sl, 1, _gk, 0, FZX_VIA({ _sk.x - 28.0, _sk.y }));
    const u32 _pu  = fzx_part(_x, "resistor", _sc.x + 90.0, _sc.y, 0, "10k");
    const u32 _vu  = fzx_part(_x, "supply rail", _sc.x + 147.0, _sc.y + 40.0, 0, "5V");
    fzx_route(_x, _sl, 2, _pu, 0, NULL, 0);
    fzx_route(_x, _pu, 1, _vu, 0, FZX_VIA({ _sc.x + 147.0, _sc.y }));
    const u32 _ge = fzx_part(_x, "ground", _se.x + 17.0, _se.y - 35.0, 0, "");
    fzx_route(_x, _sl, 3, _ge, 0, FZX_VIA({ _se.x + 17.0, _se.y }));
    // (C smoothed: down to 10 µF)
    const u32 _db = fzx_part(_x, "electrolytic", _sc.x + 60.0, -335, -90, "10uF");
    const u32 _gd = fzx_part(_x, "ground", _sc.x + 60.0, -395, 0, "");
    fzx_route(_x, _pu, 0, _db, 0, NULL, 0);
    fzx_route(_x, _db, 1, _gd, 0, NULL, 0);
    // Its tachometer: a belt from its crank to a dynamo (a motor, turned: as many volts as it turns), a panel meter on it.
    const u32 _gen = fzx_motor(_x, _cx + 230.0, 170.0, 0, "6V 300rpm", "sprocket");
    fzx_belt(_x, "belt", (fude_zoom_v2){ _cx, 0.0 }, (fude_zoom_v2){ _cx + 230.0, 170.0 }, "");
    const u32 _tm = fzx_part(_x, "panel meter", _cx + 400.0, 170.0, 0, "20V");
    fzx_route(_x, _gen, 1, _tm, 0, NULL, 0);
    fzx_route(_x, _tm, 1, _gen, 0, FZX_VIA({ _cx + 500.0, 170.0 }, { _cx + 500.0, 240.0 }, { _cx + 170.0, 240.0 }, { _cx + 170.0, 170.0 }));
    // The coil: 12 V through a panel meter into its diode's cathode and on, its low side down to the TIP120, the diode's
    // anode there too.
    const u32 _bat = fzx_part(_x, "battery", -700, 0, 0, "12V");
    const u32 _gb  = fzx_part(_x, "ground", -700, -90, 0, "");
    fzx_route(_x, _bat, 1, _gb, 0, NULL, 0);
    const u32 _pm  = fzx_part(_x, "panel meter", -420, 80, 0, "2A");
    const u32 _d   = fzx_part(_x, "diode", -260, 0, 90, "1N4007");
    fzx_route(_x, _bat, 0, _pm, 0, FZX_VIA({ -700, 80 }));
    fzx_route(_x, _pm, 1, _d, 1, FZX_VIA({ -260, 80 }));
    fzx_route(_x, _d, 1, _so, 0, NULL, 0);
    const u32 _q  = fzx_part(_x, "NPN", -210, -170, 0, "TIP120");
    const u32 _gq = fzx_part(_x, "ground", -200, -250, 0, "");
    fzx_route(_x, _so, 1, _q, 1, NULL, 0);
    fzx_route(_x, _d, 0, _q, 1, FZX_VIA({ -260, -140 }));
    fzx_route(_x, _q, 2, _gq, 0, NULL, 0);
    // (its drive through a switch — its ignition: opened, the coil let go)
    const u32 _rb = fzx_part(_x, "resistor", -300, -170, 0, "1k");
    const u32 _sw = fzx_part(_x, "SPST switch", -440, -170, 0, "on");
    const u32 _bf = fzx_part(_x, "buffer", -560, -170, 0, "");
    fzx_route(_x, _rb, 1, _q, 0, NULL, 0);
    fzx_route(_x, _sw, 1, _rb, 0, NULL, 0);
    fzx_route(_x, _bf, 1, _sw, 0, NULL, 0);
    // (the sensor's C, from its capacitor, round under everything into the buffer)
    fzx_route(_x, _db, 0, _bf, 0, FZX_VIA({ -640, -315 }, { -640, -170 }));
    // Its turns counted: a 74HC161 (enabled, loaded never, cleared never) clocked by the buffer; its Q's down into LEDs.
    const f64 _qx = -420.0, _qy = -520.0;
    const u32 _cnt = fzx_part(_x, "74HC161", _qx, _qy, 0, "74HC161");
    fzx_route(_x, _bf, 1, _cnt, 1u, FZX_VIA({ -520, _qy + 150.0 }, { _qx - 180.0, _qy + 150.0 }, { _qx - 180.0, _qy + 50.0 }));
    const u32 _cv = fzx_part(_x, "supply rail", _qx + 100.0, _qy + 120.0, 0, "5V");
    fzx_line(_x, _cnt, 15u, _cv, 0);
    fzx_route(_x, _cnt, 9u, _cnt, 15u, FZX_VIA({ _qx + 110.0, _qy - 50.0 }, { _qx + 110.0, _qy + 70.0 }));
    fzx_route(_x, _cnt, 8u, _cnt, 15u, FZX_VIA({ _qx + 120.0, _qy - 70.0 }, { _qx + 120.0, _qy + 70.0 }));
    const u32 _cr = fzx_part(_x, "supply rail", _qx - 140.0, _qy + 110.0, 0, "5V");
    fzx_route(_x, _cnt, 0u, _cr, 0, FZX_VIA({ _qx - 140.0, _qy + 70.0 }));
    const u32 _er = fzx_part(_x, "supply rail", _qx - 160.0, _qy - 30.0, 0, "5V");
    fzx_route(_x, _cnt, 6u, _er, 0, FZX_VIA({ _qx - 160.0, _qy - 50.0 }));
    const u32 _cg = fzx_part(_x, "ground", _qx - 100.0, _qy - 130.0, 0, "");
    fzx_line(_x, _cnt, 7u, _cg, 0);
    static const u32 _qpin[4] = { 10u, 11u, 12u, 13u };   // (QD, QC, QB, QA: the farthest right the highest)
    static const c8* const _weight[4] = { "8", "4", "2", "1" };
    for(u32 _k = 0; _k < 4u; _k++) {
        const f64 _lx = _qx + 170.0 + 55.0 * (f64)(3u - _k);
        const fude_zoom_v2 _at = fzx_pin(_x, _cnt, _qpin[_k]);
        const u32 _r2 = fzx_sized(_x, "resistor", _lx, _qy - 110.0, 90, 50, 14, "330");
        const u32 _l2 = fzx_sized(_x, "LED", _lx, _qy - 180.0, -90, 50, 22, "green");
        const u32 _g2 = fzx_sized(_x, "ground", _lx, _qy - 240.0, 0, 24, 24, "");
        fzx_route(_x, _cnt, _qpin[_k], _r2, 1, FZX_VIA({ _lx, _at.y }));
        fzx_route(_x, _r2, 0, _l2, 0, NULL, 0);
        fzx_route(_x, _l2, 1, _g2, 0, NULL, 0);
        fzx_text(_x, _lx - 4.0, _qy - 268.0, 13.0, 20.0, _weight[_k]);
    }
    // The oscilloscope on the sensor (from its capacitor, right).
    const u32 _scp = fzx_part(_x, "oscilloscope", _sc.x + 330.0, -420, 0, "100ms 2V");
    const u32 _gsc = fzx_part(_x, "ground", _sc.x + 150.0, -510, 0, "");
    fzx_route(_x, _db, 0, _scp, 0, FZX_VIA({ _sc.x + 130.0, -315 }, { _sc.x + 130.0, -380 }));
    fzx_route(_x, _scp, 2, _gsc, 0, FZX_VIA({ _sc.x + 150.0, -460 }));
    fzx_word(_x, -760, -820, 16.0, 760.0, FUDE_ZOOM_EXAMPLE_WORD_SOLENOID_ENGINE);
    fzx_title(_x, -760, 360, FUDE_ZOOM_EXAMPLE_SOLENOID_ENGINE);
}

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

// A crank and rocker on a motor turning at _rpm: ground 110 (motor at -55, pivot at 55), crank 40, coupler 120, rocker
// 100 (Grashof: the crank turns round).
RDE_INTERNAL void fzx_crank_rocker(fzx* _x, const c8* _rpm) {
    fzx_part(_x, "drive motor", -55, 0, 0, _rpm);
    fzx_part(_x, "fixed pivot", 55, -10, 0, "");
    fzx_sized(_x, "link", -35, 0, 0, 40 + 24, 24, "");
    fzx_sized(_x, "link", (-15 + 51.43) * 0.5, 99.9 * 0.5, atan2(99.9, 66.43) / FZX_DEG, 120 + 24, 24, "");
    fzx_sized(_x, "link", (55 + 51.43) * 0.5, 99.9 * 0.5, atan2(99.9, -3.57) / FZX_DEG, 100 + 24, 24, "");
}

RDE_INTERNAL void fzx_crank(fzx* _x) {
    fzx_crank_rocker(_x, "30 rpm");
    fzx_title(_x, -200, 230, FUDE_ZOOM_EXAMPLE_CRANK);
}

// The crank and rocker with a tracer on its crank's end (a circle) and one on its coupler's middle (its coupler curve: the
// shape a linkage is chosen for).
// By hand: a crank and rocker on a hand crank (its handle 21 out: the crank; coupler 120, rocker 100, ground 89 — it turns
// round), a tracer on its coupler; beside it a pendulum, its weight to take hold of and drag.
RDE_INTERNAL void fzx_by_hand(fzx* _x) {
    fzx_part(_x, "hand crank", -55, 0, 0, "");
    fzx_part(_x, "fixed pivot", 55, -10, 0, "");
    const f64 _cx = 35.22, _cy = 98.02;   // (where the coupler meets the rocker)
    fzx_sized(_x, "link", (-34.0 + _cx) * 0.5, _cy * 0.5, atan2(_cy, _cx + 34.0) / FZX_DEG, 120 + 24, 24, "");
    fzx_sized(_x, "link", (55.0 + _cx) * 0.5, _cy * 0.5, atan2(_cy, _cx - 55.0) / FZX_DEG, 100 + 24, 24, "");
    fzx_part(_x, "tracer", (-34.0 + _cx) * 0.5, _cy * 0.5, 0, "");
    fzx_word(_x, -140, -70, 16.0, 220.0, FUDE_ZOOM_EXAMPLE_WORD_TURN_CRANK);
    // (the pendulum)
    fzx_part(_x, "fixed pivot", 250, 90, 0, "");
    fzx_sized(_x, "link", 250, 100 - 60, -90, 144, 24, "");
    fzx_part(_x, "weight", 250, 100 - 120, 0, "1 kg");
    fzx_word(_x, 200, -70, 16.0, 180.0, FUDE_ZOOM_EXAMPLE_WORD_DRAG_WEIGHT);
    fzx_title(_x, -200, 230, FUDE_ZOOM_EXAMPLE_BY_HAND);
}

RDE_INTERNAL void fzx_coupler_curve(fzx* _x) {
    fzx_crank_rocker(_x, "20 rpm");
    fzx_part(_x, "tracer", -15, 0, 0, "");
    fzx_part(_x, "tracer", (-15 + 51.43) * 0.5, 99.9 * 0.5, 0, "");
    fzx_title(_x, -200, 230, FUDE_ZOOM_EXAMPLE_COUPLER_CURVE);
}

// A drive motor (30 rpm) with a small sprocket on it, a chain round it and a sprocket twice as big — half as fast, the same
// way; under it, the same motor with a crossed belt to a sprocket as big — as fast, the other way.
RDE_INTERNAL void fzx_belts(fzx* _x) {
    for(u32 _k = 0; _k < 2u; _k++) {
        const f64 _y = _k == 0u ? 0.0 : -220.0;
        fzx_part(_x, "drive motor", -300, _y, 0, "30 rpm");
        fzx_sized(_x, "sprocket", -300, _y, 0, 60, 60, "");
        fzx_sized(_x, "sprocket", -50, _y, 0, _k == 0u ? 120 : 60, _k == 0u ? 120 : 60, "");
        fzx_sized(_x, _k == 0u ? "chain" : "belt", -175, _y, 0, 290, 40, _k == 0u ? "" : "crossed");   // (its ends' middles 250 apart: the two axles)
    }
    fzx_title(_x, -360, 160, FUDE_ZOOM_EXAMPLE_BELTS);
}

// An eccentric cam on a drive motor (15 rpm: its hole, off its middle, on the shaft), a follower over it pointing down —
// its roller pressed onto the cam, rising and falling its eccentricity either way.
RDE_INTERNAL void fzx_cam(fzx* _x) {
    fzx_part(_x, "drive motor", 0, 0, 0, "15 rpm");
    fzx_sized(_x, "cam", 0.285 * 40.0, 0, 0, 80, 80, "");
    fzx_sized(_x, "follower", 0, 92, -90, 120, 30, "");
    fzx_title(_x, -150, 230, FUDE_ZOOM_EXAMPLE_CAM);
}

// Two ratchet wheels on their axles, a weight on a rope tied to each one's right (pulling it clockwise): the left one's pawl
// (along its top, its point on its teeth) holds it; the right one, with none, runs back. Dragged round counter-clockwise,
// the left one clicks past its pawl and stays.
RDE_INTERNAL void fzx_ratchet(fzx* _x) {
    for(u32 _k = 0; _k < 2u; _k++) {
        const f64 _cx = _k == 0u ? -150.0 : 150.0;
        fzx_sized(_x, "ratchet", _cx, 0, 0, 80, 80, "");
        if(_k == 0u) {
            fzx_sized(_x, "pawl", _cx + 34.0, 33, 180, 70, 16, "");   // (its point 33 over the wheel's middle: between its roots and tips)
        }
        fzx_sized(_x, "rope", _cx + 36.0, -59, 90, 120, 12, "");
        fzx_part(_x, "weight", _cx + 36.0, -130, 0, "1 kg");
    }
    fzx_word(_x, -260, -190, 16.0, 520.0, FUDE_ZOOM_EXAMPLE_WORD_RATCHET_DRAG);
    fzx_title(_x, -260, 120, FUDE_ZOOM_EXAMPLE_RATCHET);
}

// Two weights (1 kg) on springs (stiffness 4: 4 Hz) from fixed pivots: the left one bounces on and on; the right one has a
// damper (3) under it, to a pivot on the ground (in line with its spring: a shock absorber) — it settles in a bounce.
RDE_INTERNAL void fzx_damper(fzx* _x) {
    for(u32 _k = 0; _k < 2u; _k++) {
        const f64 _cx = _k == 0u ? -150.0 : 150.0;
        fzx_part(_x, "fixed pivot", _cx, 200, 0, "");
        fzx_sized(_x, "spring", _cx, 156, 90, 120, 24, "4");   // (its ends 108 apart: the pivot's hole, the weight's middle)
        fzx_part(_x, "weight", _cx, 102, 0, "1 kg");
    }
    // (from the weight's middle down to a pivot's hole under it: 108 long)
    fzx_sized(_x, "damper", 150, 48, 90, 120, 24, "3");
    fzx_part(_x, "fixed pivot", 150, -16, 0, "");
    fzx_title(_x, -230, 300, FUDE_ZOOM_EXAMPLE_DAMPER);
}

// A worm (60 rpm) under a 30-tooth gear on its own axle (its pitch circle on the worm's): the gear a tooth a turn of it —
// 2 rpm —, a 10-tooth gear beside it three times as fast; a weight on a rope from the big gear's left, lowered as it turns,
// never let fall: a gear cannot turn a worm.
RDE_INTERNAL void fzx_worm(fzx* _x) {
    fzx_sized(_x, "worm", 0, 0, 0, 120, 40, "60 rpm");
    fzx_part(_x, "gear 30T", 0, 84, 0, "");
    fzx_part(_x, "gear 10T", 100, 84, 0, "");
    fzx_sized(_x, "rope", -72, 15, 90, 140, 12, "");
    fzx_part(_x, "weight", -72, -65, 0, "1 kg");
    fzx_title(_x, -150, 230, FUDE_ZOOM_EXAMPLE_WORM);
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

// A latch: a battery (12 V), a push button, a 12 V solenoid; its plunger's end pinned to a link whose other end is a
// slider's on a rail along it — the bolt, drawn back as the button is held. A diode across its coil (its cathode on its
// plus: the coil's current carried round when the button lets go), or none: the button sparks.
RDE_INTERNAL void fzx_latch(fzx* _x, f64 _px, f64 _py, b8 _diode) {
    // (its leads 50 left of its middle, 10 over and under; the button's into the upper from over it, the battery's minus
    // from under the lower)
    const u32 _so  = fzx_part(_x, "solenoid", _px, _py, 0, "12V");
    const u32 _bat = fzx_part(_x, "battery", _px - 300.0, _py, 0, "12V");
    const u32 _btn = fzx_part(_x, "push button", _px - 200.0, _py + 80.0, 0, "");
    fzx_limits(_x, _btn, 0, 3, 0, 0, 2);   // (a 3 A button: a 6 mm one's 50 mA would burn on half an ampere)
    const u32 _gnd = fzx_part(_x, "ground", _px - 340.0, _py - 90.0, 0, "");
    fzx_route(_x, _bat, 0, _btn, 0, FZX_VIA({ _px - 300.0, _py + 80.0 }));
    fzx_route(_x, _btn, 1, _so, 0, FZX_VIA({ _px - 50.0, _py + 80.0 }));
    fzx_route(_x, _so, 1, _bat, 1, FZX_VIA({ _px - 50.0, _py - 70.0 }, { _px - 300.0, _py - 70.0 }));
    fzx_route(_x, _gnd, 0, _bat, 1, FZX_VIA({ _px - 340.0, _py - 30.0 }));
    if(_diode) {
        // (upright left of its leads: its cathode on the plus, its anode on the minus)
        const u32 _d = fzx_part(_x, "diode", _px - 130.0, _py, 90, "1N4007");
        fzx_route(_x, _d, 1, _so, 0, FZX_VIA({ _px - 130.0, _py + 45.0 }, { _px - 65.0, _py + 45.0 }, { _px - 65.0, _py + 10.0 }));
        fzx_route(_x, _d, 0, _so, 1, FZX_VIA({ _px - 130.0, _py - 45.0 }, { _px - 65.0, _py - 45.0 }, { _px - 65.0, _py - 10.0 }));
    }
    // (the plunger's end 42.5 along; the link's other hole 126 on, the slider's middle; a rail under it, 50 of travel either
    // way)
    fzx_sized(_x, "rail", _px + 188.5, _py, 0, 160, 16, "");
    fzx_sized(_x, "link", _px + 42.5 + 63.0, _py, 0, 150, 24, "");
    fzx_part(_x, "slider", _px + 168.5, _py, 0, "");
}

RDE_INTERNAL void fzx_solenoid(fzx* _x) {
    fzx_latch(_x, 0, 120, true);
    fzx_latch(_x, 0, -200, false);
    fzx_word(_x, -330, -340, 16.0, 380.0, FUDE_ZOOM_EXAMPLE_WORD_NO_DIODE);
    fzx_title(_x, -330, 300, FUDE_ZOOM_EXAMPLE_SOLENOID);
}

// A clock (200 Hz) into two D flip-flops clocked together, the first's D the second's /Q, the second's D the first's Q: a
// ring of four states (00, 10, 11, 01) — Q1, Q2, /Q1, /Q2 into a ULN2003's IN1–IN4, its outputs sinking a 28BYJ-48's
// coils A–D (COM on 5 V, the ULN2003's COM too: its clamp diodes): two coils at a time, a step on each tick — an arm on
// its shaft round in ten seconds.
RDE_INTERNAL void fzx_stepper(fzx* _x) {
    const u32 _f1  = fzx_part(_x, "D flip-flop", 0, 100, 0, "");
    const u32 _f2  = fzx_part(_x, "D flip-flop", 0, -60, 0, "");
    const u32 _clk = fzx_part(_x, "clock", -250, 0, 0, "200Hz");
    const u32 _uln = fzx_part(_x, "ULN2003", 350, 20, 0, "ULN2003");
    const u32 _st  = fzx_part(_x, "stepper motor", 650, 20, 0, "28BYJ-48");
    fzx_sized(_x, "link", 650, 20 + 30, 90, 80, 20, "");   // (its holes 60 apart: one on the shaft, all of it inside the motor's face)
    const u32 _g1 = fzx_part(_x, "ground", -250, -80, 0, "");
    const u32 _g2 = fzx_part(_x, "ground", 235, -120, 0, "");
    const u32 _r1 = fzx_part(_x, "supply rail", 600, -80, 0, "5V");
    const u32 _r2 = fzx_part(_x, "supply rail", 520, -40, 0, "5V");
    // D flip-flops: D (−40, 20), CLK (−40, −20), Q (40, 20), /Q (40, −20) about each.
    fzx_route(_x, _f1, 2, _uln, 0, FZX_VIA({ 200, 120 }, { 200, 90 }));
    fzx_route(_x, _f2, 2, _uln, 1, FZX_VIA({ 170, -40 }, { 170, 70 }));
    fzx_route(_x, _f1, 3, _uln, 2, FZX_VIA({ 140, 80 }, { 140, 50 }));
    fzx_route(_x, _f2, 3, _uln, 3, FZX_VIA({ 220, -80 }, { 220, 30 }));
    fzx_route(_x, _f1, 0, _f2, 3, FZX_VIA({ -80, 120 }, { -80, -110 }, { 40, -110 }));
    fzx_route(_x, _f2, 0, _f1, 2, FZX_VIA({ -60, -40 }, { -60, 150 }, { 40, 150 }));
    fzx_route(_x, _clk, 0, _f1, 1, FZX_VIA({ -250, 80 }));
    fzx_route(_x, _clk, 0, _f2, 1, FZX_VIA({ -200, 30 }, { -200, -80 }));
    fzx_route(_x, _clk, 1, _g1, 0, NULL, 0);
    fzx_route(_x, _uln, 7, _g2, 0, FZX_VIA({ 235, -50 }));
    // Its outputs (OUT1–OUT4) to the coils A–D; its COM and the motor's on 5 V.
    fzx_route(_x, _uln, 15, _st, 0, FZX_VIA({ 520, 90 }, { 520, 60 }));
    fzx_route(_x, _uln, 14, _st, 1, FZX_VIA({ 530, 70 }, { 530, 40 }));
    fzx_route(_x, _uln, 13, _st, 2, FZX_VIA({ 540, 50 }, { 540, 20 }));
    fzx_route(_x, _uln, 12, _st, 3, FZX_VIA({ 550, 30 }, { 550, 0 }));
    fzx_route(_x, _uln, 8, _r1, 0, FZX_VIA({ 470, -50 }, { 470, -110 }, { 600, -110 }));
    fzx_route(_x, _st, 4, _r2, 0, FZX_VIA({ 560, -20 }, { 560, -60 }, { 520, -60 }));
    fzx_title(_x, -260, 260, FUDE_ZOOM_EXAMPLE_STEPPER);
}

// Sensors on shafts. A hand crank on a rotary encoder (turned round: its pins on its right), its common on 5 V: each of
// A and B through 470 Ω into an LED — lit while its contact is closed, the two in turn as it turns. A hand crank on a pot
// across 5 V, a panel meter on its wiper: as far round, as many volts. A drive motor's arm through a slotted sensor's
// slot each turn: its transistor let go — C high —, a T flip-flop toggled, its probe a turn on, a turn off.
RDE_INTERNAL void fzx_shaft_sensors(fzx* _x) {
    fzx_part(_x, "hand crank", 0, 0, 0, "");
    const u32 _en = fzx_part(_x, "rotary encoder", 0, 0, 180, "20");   // (A, C, B up its right: 40 along, 20 under to 20 over)
    const u32 _rc = fzx_part(_x, "supply rail", 60, 120, 0, "5V");
    fzx_route(_x, _en, 1, _rc, 0, FZX_VIA({ 60, 0 }));
    for(u32 _k = 0; _k < 2u; _k++) {
        const f64 _y = _k == 0u ? -20.0 : 20.0;
        const u32 _r   = fzx_part(_x, "resistor", 150, _y, 0, "470");
        const u32 _led = fzx_part(_x, "LED", 250, _y, 0, _k == 0u ? "yellow" : "green");
        const f64 _gx  = 320.0 + 40.0 * (f64)_k;
        const u32 _g   = fzx_part(_x, "ground", _gx, _y - 40.0, 0, "");
        fzx_route(_x, _en, _k == 0u ? 0u : 2u, _r, 0, NULL, 0);
        fzx_route(_x, _r, 1, _led, 0, NULL, 0);
        fzx_route(_x, _led, 1, _g, 0, FZX_VIA({ _gx, _y }));
    }
    fzx_text(_x, 280, 50, 14.0, 40.0, "B");
    fzx_text(_x, 280, -70, 14.0, 40.0, "A");
    // The pot: its ends 30 either side of its middle and 10 under, its wiper 20 over.
    fzx_part(_x, "hand crank", 450, 0, 0, "");
    const u32 _pot = fzx_part(_x, "potentiometer", 450, 0, 0, "10k 50%");
    const u32 _rp  = fzx_part(_x, "supply rail", 340, 20, 0, "5V");
    const u32 _gp  = fzx_part(_x, "ground", 500, -60, 0, "");
    const u32 _mt  = fzx_part(_x, "panel meter", 450, 140, 0, "20V");
    const u32 _gm  = fzx_part(_x, "ground", 560, -60, 0, "");
    fzx_route(_x, _pot, 0, _rp, 0, FZX_VIA({ 340, -10 }));
    fzx_route(_x, _pot, 1, _gp, 0, FZX_VIA({ 500, -10 }));
    fzx_route(_x, _pot, 2, _mt, 0, FZX_VIA({ 450, 60 }, { 350, 60 }, { 350, 140 }));
    fzx_route(_x, _mt, 1, _gm, 0, FZX_VIA({ 560, 140 }));
    // The slotted sensor over the motor's shaft, its beam where the arm's far end goes by (120 up): its LED on 5 V through
    // 330 Ω, its C pulled up through 10k; C the T flip-flop's clock, its T held high by a logic input.
    fzx_part(_x, "drive motor", 850, 0, 0, "30 rpm");
    fzx_sized(_x, "link", 850 + 63, 0, 0, 150, 24, "");
    const u32 _sl = fzx_part(_x, "slotted sensor", 850, 111, 0, "ITR9608");
    const u32 _rl = fzx_part(_x, "resistor", 760, 121, 0, "330");
    const u32 _rr = fzx_part(_x, "supply rail", 710, 180, 0, "5V");
    const u32 _ru = fzx_part(_x, "resistor", 940, 121, 0, "10k");
    const u32 _rs = fzx_part(_x, "supply rail", 990, 180, 0, "5V");
    const u32 _gk = fzx_part(_x, "ground", 790, 40, 0, "");
    const u32 _ge = fzx_part(_x, "ground", 910, 40, 0, "");
    fzx_route(_x, _sl, 0, _rl, 1, NULL, 0);
    fzx_route(_x, _rl, 0, _rr, 0, FZX_VIA({ 710, 121 }));
    fzx_route(_x, _sl, 1, _gk, 0, FZX_VIA({ 790, 101 }));
    fzx_route(_x, _sl, 3, _ge, 0, FZX_VIA({ 910, 101 }));
    fzx_route(_x, _sl, 2, _ru, 0, NULL, 0);
    fzx_route(_x, _ru, 1, _rs, 0, FZX_VIA({ 990, 121 }));
    const u32 _tf = fzx_part(_x, "T flip-flop", 1100, -60, 0, "");
    const u32 _hi = fzx_part(_x, "logic input", 1000, -40, 0, "1");
    const u32 _pr = fzx_part(_x, "logic probe", 1200, -40, 0, "");
    fzx_route(_x, _sl, 2, _tf, 1, FZX_VIA({ 890, 150 }, { 1045, 150 }, { 1045, -80 }));
    fzx_route(_x, _hi, 0, _tf, 0, NULL, 0);
    fzx_route(_x, _tf, 2, _pr, 0, NULL, 0);
    fzx_word(_x, -60, -110, 16.0, 420.0, FUDE_ZOOM_EXAMPLE_WORD_TURN_CRANK);
    fzx_title(_x, -60, 280, FUDE_ZOOM_EXAMPLE_SHAFT_SENSORS);
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
        fzx_digit_counter, fzx_matrix_scan, fzx_bar_meter, fzx_panel_meters, fzx_lcd_by_hand, fzx_scope_rc, fzx_light_temp,
        fzx_decade, fzx_flipflops, fzx_comparator, fzx_thyristors, fzx_power_supply, fzx_light_link, fzx_speaker_tone, fzx_traffic,
        fzx_piano,
        fzx_crank, fzx_gear_train, fzx_pendulum, fzx_pulleys, fzx_piston, fzx_rack, fzx_gears_rack, fzx_bodies, fzx_gearbox, fzx_machine,
        fzx_materials, fzx_too_heavy, fzx_motor_torque, fzx_coupler_curve, fzx_by_hand, fzx_belts, fzx_cam, fzx_ratchet, fzx_damper, fzx_worm,
        fzx_strandbeest, fzx_engine,
        fzx_motor_gears, fzx_dynamo, fzx_forward_back, fzx_turn_counter, fzx_shuttle, fzx_servo_tester, fzx_servo_angles,
        fzx_solenoid, fzx_stepper, fzx_shaft_sensors, fzx_workbench, fzx_everything, fzx_solenoid_engine,
    };
    if(_example >= FUDE_ZOOM_EXAMPLE_COUNT || _s == NULL || _born == NULL) {
        return 0u;
    }
    fzx _x = { fude_zoom_placer_make(_s, _frame, _to_frame, _ink, _born), _title };
    const u32 _was = (u32)rde_arr_length(_born);
    _build[_example](&_x);
    return (u32)rde_arr_length(_born) - _was;
}
