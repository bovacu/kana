// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/examples_parts.h"

// ===========================================================================
// See examples.h.
//
// In order: the list's groups and folders; the drawing helpers (fzx_, over placer.h); and the table that builds an
// example by its number (fude_zoom_example_build). The examples themselves are in examples_electronics.c,
// examples_mechanisms.c and examples_both.c, one function each. Every position is in screen points at ×1 from the
// view's middle (Y up), as placer.h takes them.
//
// Part of the examples: examples_parts.h holds what the modules share and says which module holds what. File-local
// names start with fzx_. Main thread only.
// ===========================================================================

// See the header: the examples made to break something (a part burnt out, a short, a pin sheared, a motor stalled).
// Ringing and Materials sit in the "What goes wrong" folders (fude_zoom_example_set) but are left out on purpose: they
// show a surprise, not a breakage. Ringing's capacitor swings to twice the supply, within every part's limits, and
// Materials' ice slides away, so neither has a part's event to show, and the tests hold both to running with none.
b8 fude_zoom_example_goes_wrong(u32 _example) {
    return _example == FUDE_ZOOM_EXAMPLE_LED_RESISTOR || _example == FUDE_ZOOM_EXAMPLE_RESISTOR_WATTS || _example == FUDE_ZOOM_EXAMPLE_SHORT_FUSE ||
           _example == FUDE_ZOOM_EXAMPLE_CAP_POLARITY || _example == FUDE_ZOOM_EXAMPLE_TRANSISTOR_SIZE || _example == FUDE_ZOOM_EXAMPLE_TOO_HEAVY ||
           _example == FUDE_ZOOM_EXAMPLE_MOTOR_TORQUE || _example == FUDE_ZOOM_EXAMPLE_SERVO_ANGLES;
}

// See the header.
u8 fude_zoom_example_group(u32 _example) {
    // In the list's order: the electronics' up to the first mechanism's, the mechanisms' up to the first of both's.
    return _example < FUDE_ZOOM_EXAMPLE_CRANK ? FUDE_ZOOM_EXAMPLES_ELECTRONICS :
           (_example < FUDE_ZOOM_EXAMPLE_MOTOR_GEARS ? FUDE_ZOOM_EXAMPLES_MECHANISMS : FUDE_ZOOM_EXAMPLES_BOTH);
}

// See the header. Any example not named here goes to its group's big projects.
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
        // Any other: its group's big projects.
        return fude_zoom_example_group(_example) == FUDE_ZOOM_EXAMPLES_ELECTRONICS ? FUDE_ZOOM_EXAMPLE_SET_E_BIG :
               (fude_zoom_example_group(_example) == FUDE_ZOOM_EXAMPLES_MECHANISMS ? FUDE_ZOOM_EXAMPLE_SET_M_BIG : FUDE_ZOOM_EXAMPLE_SET_B_BIG);
    }
}

// See the header: the folders are in their groups' order, so each group's are those up to its big projects.
u8 fude_zoom_example_set_group(u8 _set) {
    return _set <= FUDE_ZOOM_EXAMPLE_SET_E_BIG ? FUDE_ZOOM_EXAMPLES_ELECTRONICS :
           (_set <= FUDE_ZOOM_EXAMPLE_SET_M_BIG ? FUDE_ZOOM_EXAMPLES_MECHANISMS : FUDE_ZOOM_EXAMPLES_BOTH);
}

// --- drawing one: the library's parts placed, wired, written by ----------------------------------------------------

// Small helpers over placer.h, in points from the middle: a point in the frame's units; a part of the library at a
// size (0: its own) or at its own; a part's pin; a wire routed as the Wire tool does, or a straight one (fzx_line); a
// plain text; a pen line.
fude_zoom_v2 fzx_at(const fzx* _x, f64 _px, f64 _py) { return fude_zoom_placer_at(&_x->p, _px, _py); }
u32 fzx_sized(fzx* _x, const c8* _id, f64 _px, f64 _py, f64 _turn, f64 _w, f64 _h, const c8* _text) {
    return fude_zoom_placer_part(&_x->p, _id, _px, _py, _turn, _w, _h, _text);
}
u32 fzx_part(fzx* _x, const c8* _id, f64 _px, f64 _py, f64 _turn, const c8* _text) { return fzx_sized(_x, _id, _px, _py, _turn, 0.0, 0.0, _text); }
fude_zoom_v2 fzx_pin(const fzx* _x, u32 _o, u32 _pin) { return fude_zoom_placer_pin(&_x->p, _o, _pin); }
void fzx_wire(fzx* _x, u32 _a, u32 _pa, u32 _b, u32 _pb) { fude_zoom_placer_wire(&_x->p, _a, _pa, _b, _pb, false); }
void fzx_line(fzx* _x, u32 _a, u32 _pa, u32 _b, u32 _pb) { fude_zoom_placer_wire(&_x->p, _a, _pa, _b, _pb, true); }
void fzx_text(fzx* _x, f64 _px, f64 _py, f64 _size, f64 _w, const c8* _text) { fude_zoom_placer_text(&_x->p, _px, _py, _size, _w, _text); }
u32 fzx_path(fzx* _x, const fude_zoom_v2* _pts, u32 _n, b8 _closed, f64 _width) { return fude_zoom_placer_path(&_x->p, _pts, _n, _closed, _width); }

// The one being drawn moved to _px, _py (points from where it was); the middle it had back.
fude_zoom_v2 fzx_move(fzx* _x, f64 _px, f64 _py) {
    const fude_zoom_v2 _was = _x->p.o;
    _x->p.o = (fude_zoom_v2){ _was.x + _px, _was.y + _py };
    return _was;
}

// A wire from part _a's pin _pa to _b's _pb through the corners _via (_n of them; placer.h draws nothing when they do
// not fit in a wire).
void fzx_route(fzx* _x, u32 _a, u32 _pa, u32 _b, u32 _pb, const fude_zoom_v2* _via, u32 _n) {
    fude_zoom_placer_wire_via(&_x->p, _a, _pa, _b, _pb, _via, _n);
}

// A word by what is drawn (FUDE_ZOOM_EXAMPLE_WORD_'s, a material's: examples.h), _size points high, its top-left at
// _px, _py, wrapped at _w; nothing when the caller gives none.
void fzx_word(fzx* _x, f64 _px, f64 _py, f64 _size, f64 _w, u32 _word) {
    const c8* _t = _x->title != NULL ? _x->title(_word) : NULL;
    if(_t != NULL && _t[0] != 0) {
        fzx_text(_x, _px, _py, _size, _w, _t);
    }
}

// Limits of its own on a part (its card's: props.h): _w, _a, _v, _r (0: none), the real part they are.
void fzx_limits(fzx* _x, u32 _part, f64 _w, f64 _a, f64 _v, f64 _r, i32 _preset) {
    const f64 _most[4] = { _w, _a, _v, _r };
    const u32 _made = fude_zoom_props_add_limits(_x->p.s, _part, _most, _preset);
    rde_arr_add(_x->p.born, (any)&_made);
}

// A drawing made a body of material _material (sim/body.h's; fixed: the ground): a pen line through _pts.
u32 fzx_drawn(fzx* _x, const fude_zoom_v2* _pts, u32 _n, b8 _closed, u32 _material, b8 _fixed) {
    const u32 _o = fzx_path(_x, _pts, _n, _closed, 4.0);
    if(_o != FUDE_ZOOM_NONE) {
        const fude_zoom_body_props _bp = { _material, _fixed, 0.0, -1.0, -1.0 };
        const u32 _made = fude_zoom_props_add_body(_x->p.s, _o, &_bp);
        rde_arr_add(_x->p.born, (any)&_made);
    }
    return _o;
}

// A drawn ball of _r points round at _px, _py, of material _material.
void fzx_ball(fzx* _x, f64 _px, f64 _py, f64 _r, u32 _material) {
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

// Writes example _example's title (26 points high, wrapped at 520) with its top-left at _px, _py; nothing when the
// caller gives none.
void fzx_title(fzx* _x, f64 _px, f64 _py, u32 _example) {
    const c8* _t = _x->title != NULL ? _x->title(_example) : NULL;
    if(_t != NULL && _t[0] != 0) {
        fzx_text(_x, _px, _py, 26.0, 520.0, _t);
    }
}

// Each example's builder, in FUDE_ZOOM_EXAMPLE_ order: the one fude_zoom_example_build calls for it.
RDE_INTERNAL void (*const FZX_BUILDERS[])(fzx*) = {
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
// A compile-time check that there is a builder for every example (the size is -1, which does not compile, when there
// is not): a missing one would be a NULL called.
typedef char fzx_builders_as_examples_h[(sizeof(FZX_BUILDERS) / sizeof(FZX_BUILDERS[0]) == FUDE_ZOOM_EXAMPLE_COUNT) ? 1 : -1];

// See the header.
u32 fude_zoom_example_build(fude_zoom_scene* _s, u32 _frame, fude_zoom_sim _to_frame, rde_color _ink, u32 _example, fude_zoom_example_title _title,
                            rde_arr* _born) {
    if(_example >= FUDE_ZOOM_EXAMPLE_COUNT || _s == NULL || _born == NULL) {
        return 0u;
    }
    fzx _x = { fude_zoom_placer_make(_s, _frame, _to_frame, _ink, _born), _title };
    const u32 _was = (u32)rde_arr_length(_born);
    FZX_BUILDERS[_example](&_x);
    return (u32)rde_arr_length(_born) - _was;
}
