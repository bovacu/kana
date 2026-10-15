// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/examples_parts.h"

// ===========================================================================
// See examples.h. The examples with both a circuit and a mechanism working together: a circuit's motor turns
// the gear (or the arm) pinned on its middle and is turned by it (a dynamo); a push button is pressed by a part of the
// mechanism over it (coupling.h). One function for each (fzx_ and its name), a few with helpers of their own just
// before them: first the solenoid engine, then the rest in the list's order, ending with two canvases of many examples
// (fzx_workbench, fzx_everything).
//
// Part of the examples: examples_parts.h holds what the modules share and says which module holds what. File-local
// names start with fzx_. Every position is in screen points at ×1 from the view's middle (Y up). Main thread only.
// ===========================================================================

// Declared here for the solenoid engine; defined with the both group's helpers below.
RDE_INTERNAL u32 fzx_motor(fzx* _x, f64 _px, f64 _py, f64 _turn, const c8* _text, const c8* _gear);

// A solenoid engine: a big solenoid (400 across: its plunger's stroke 60) pulling a rod (200) on a hand crank's pin (27
// round: its plunger out, all the way, with the pin furthest from it — where it is drawn; the crank's axle nearly free,
// 0.1 N·m, a flywheel's). A cam on the crank's shaft blocks a slotted sensor's beam, under the shaft, for the half turn
// the pin comes toward the solenoid going clockwise (the pin under the shaft, from 5° after it is furthest): the
// sensor's C, pulled up and smoothed (10 µF: what flickers as the cam's edge passes the beam left out), through a
// buffer, a switch (its ignition) and 1k into a TIP120 that switches the coil on 12 V, a diode across it. It starts by
// itself: its rod's weight lets its pin down into that half; the faster it goes, the later the smoothing lets its coil
// on and off (100 ms) — a speed it keeps. A panel meter in its supply; a dynamo on its crank's belt its tachometer (its
// volts on a panel meter); a 74HC161 counting its turns on four LEDs (8 4 2 1); an oscilloscope on the sensor.
void fzx_solenoid_engine(fzx* _x) {
    const f64 _r = 27.0, _rod = 200.0, _tip = 170.0, _cx = _tip + _rod - _r;
    const u32 _so = fzx_sized(_x, "solenoid", 0, 0, 0, 400, 120, "12V");
    fzx_sized(_x, "hand crank", _cx, 0, 0, 2.0 * _r / 0.7, 2.0 * _r / 0.7, "0.1");   // (its axle nearly free: 0.1 N·m)
    fzx_bar(_x, (fude_zoom_v2){ _tip, 0.0 }, (fude_zoom_v2){ _cx + _r, 0.0 }, 30.0);

    const f64 _lobe = 5.0 * FZX_DEG, _cam = 200.0, _e = 0.285 * 0.5 * _cam, _rim = 0.95 * 0.5 * _cam;
    fzx_sized(_x, "cam", _cx + _e * rde_math_cos_f64(_lobe), _e * rde_math_sin_f64(_lobe), _lobe / FZX_DEG, _cam, _cam, "");
    // (the sensor's beam straight under the shaft, where the cam's edge passes it a quarter turn either side of its
    // lobe)
    const f64 _beam = rde_math_sqrt_f64(_rim * _rim - _e * _e);
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
    // Its tachometer: a belt from its crank to a dynamo (a motor, turned: as many volts as it turns), a panel meter on
    // it.
    const u32 _gen = fzx_motor(_x, _cx + 230.0, 170.0, 0, "6V 300rpm", "sprocket");
    fzx_belt(_x, "belt", (fude_zoom_v2){ _cx, 0.0 }, (fude_zoom_v2){ _cx + 230.0, 170.0 }, "");
    const u32 _tm = fzx_part(_x, "panel meter", _cx + 400.0, 170.0, 0, "20V");
    fzx_route(_x, _gen, 1, _tm, 0, NULL, 0);
    fzx_route(_x, _tm, 1, _gen, 0, FZX_VIA({ _cx + 500.0, 170.0 }, { _cx + 500.0, 240.0 }, { _cx + 170.0, 240.0 }, { _cx + 170.0, 170.0 }));
    // The coil: 12 V through a panel meter into its diode's cathode and on, its low side down to the TIP120, the
    // diode's anode there too.
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
    // Its turns counted: a 74HC161 (enabled, loaded never, cleared never) clocked by the buffer; its Q's down into
    // LEDs.
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
void fzx_motor_gears(fzx* _x) {
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
void fzx_dynamo(fzx* _x) {
    fzx_part(_x, "drive motor", 0, 125, 0, "30 rpm");
    fzx_part(_x, "gear 40T", 0, 125, 0, "");
    // (turned the clock's way: its right pin its plus)
    const u32 _gen = fzx_motor(_x, 0, 0, 0, "6V 60rpm", "gear 10T");
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

// Places an L293D at _px, _py with its supply wired (its logic on a 5 V rail, both halves enabled; its motors' supply
// from a 9 V battery on its left) and returns where it is: the chip, its two sides' x and its middle's y.
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
void fzx_forward_back(fzx* _x) {
    const fzx_driver_at _d = fzx_driver(_x, 250, 40);
    // The motor over the rack, left of the chip (its 20-tooth pinion's pitch circle 50 round, the rack's pitch line 5
    // over its middle): 1Y to its first pin, 2Y to its second, round over the gear.
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
void fzx_turn_counter(fzx* _x) {
    const u32 _bat = fzx_part(_x, "battery", -650, 40, 0, "6V");
    const u32 _sw  = fzx_part(_x, "SPST switch", -580, 160, 0, "on");
    const u32 _mo  = fzx_motor(_x, -470, 40, 0, "6V 30rpm", NULL);
    // (its holes at its round ends, 126 apart: one on the shaft)
    fzx_sized(_x, "link", -470 + 63, 40, 0, 150, 24, "");
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
void fzx_shuttle(fzx* _x) {
    const fzx_driver_at _d = fzx_driver(_x, 420, -60);
    const u32 _mo = fzx_motor(_x, 0, 250, 0, "6V 10rpm", "gear 20T");
    fzx_sized(_x, "rack", -60, 250 - 55, 0, 300, 30, "");   // (from −210 to 90: over the left button)
    // 1Y to the motor's first pin, 2Y to its second, round over the gear.
    fzx_route(_x, _d.ic, 2u, _mo, 0, FZX_VIA({ _d.left - 50.0, _d.y + 30.0 }, { _d.left - 50.0, 370 }, { -80, 370 }, { -80, 250 }));
    fzx_route(_x, _d.ic, 5u, _mo, 1, FZX_VIA({ _d.left - 70.0, _d.y - 30.0 }, { _d.left - 70.0, 350 }, { 80, 350 }, { 80, 250 }));
    // The buttons on the rack's line (upright: a pin over it, a pin under it): 5 V over each, the latch's input under
    // it (pulled down through 10k).
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

// A servo tester: a 555 on four AA cells (6 V) whose pulses are short — it charges through R1 alone, past R2 by a
// diode, and discharges through R2 (270k: about 17 ms low) — R1 5.6k and a 20k pot (0.5 to 2.3 ms: the whole of a
// servo's travel, 1.4 ms with the pot halfway); its output the servo's signal, an arm on its shaft. A tap on the pot
// turns it.
void fzx_servo_tester(fzx* _x) {
    const u32 _bat = fzx_part(_x, "battery", -330, 0, 0, "6V");
    const u32 _t   = fzx_part(_x, "NE555", 0, 0, 0, "NE555");
    const u32 _pot = fzx_part(_x, "potentiometer", 220, 290, 0, "20k 50%");
    const u32 _r1  = fzx_part(_x, "resistor", 220, 200, 0, "5.6k");
    const u32 _r2  = fzx_part(_x, "resistor", 220, 120, 0, "270k");
    const u32 _d   = fzx_part(_x, "diode", 220, 50, 180, "1N4148");
    const u32 _cap = fzx_part(_x, "capacitor", -180, -160, -90, "100nF");
    // The servo below the battery, its arm up (the middle of its travel: clear of its pins' names, GND, VCC, SIG down
    // its left).
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
void fzx_servo_angles(fzx* _x) {
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
    // (its leads 50 left of its middle, 10 over and under; the button's into the upper from over it, the battery's
    // minus from under the lower)
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
    // (the plunger's end 42.5 along; the link's other hole 126 on, the slider's middle; a rail under it, 50 of travel
    // either way)
    fzx_sized(_x, "rail", _px + 188.5, _py, 0, 160, 16, "");
    fzx_sized(_x, "link", _px + 42.5 + 63.0, _py, 0, 150, 24, "");
    fzx_part(_x, "slider", _px + 168.5, _py, 0, "");
}

// A solenoid's latch with a diode across its coil, and one with none under it.
void fzx_solenoid(fzx* _x) {
    fzx_latch(_x, 0, 120, true);
    fzx_latch(_x, 0, -200, false);
    fzx_word(_x, -330, -340, 16.0, 380.0, FUDE_ZOOM_EXAMPLE_WORD_NO_DIODE);
    fzx_title(_x, -330, 300, FUDE_ZOOM_EXAMPLE_SOLENOID);
}

// A clock (200 Hz) into two D flip-flops clocked together, the first's D the second's /Q, the second's D the first's Q:
// a ring of four states (00, 10, 11, 01) — Q1, Q2, /Q1, /Q2 into a ULN2003's IN1–IN4, its outputs sinking a 28BYJ-48's
// coils A–D (COM on 5 V, the ULN2003's COM too: its clamp diodes): two coils at a time, a step on each tick — an arm on
// its shaft round in ten seconds.
void fzx_stepper(fzx* _x) {
    const u32 _f1  = fzx_part(_x, "D flip-flop", 0, 100, 0, "");
    const u32 _f2  = fzx_part(_x, "D flip-flop", 0, -60, 0, "");
    const u32 _clk = fzx_part(_x, "clock", -250, 0, 0, "200Hz");
    const u32 _uln = fzx_part(_x, "ULN2003", 350, 20, 0, "ULN2003");
    const u32 _st  = fzx_part(_x, "stepper motor", 650, 20, 0, "28BYJ-48");
    // (its holes 60 apart: one on the shaft, all of it inside the motor's face)
    fzx_sized(_x, "link", 650, 20 + 30, 90, 80, 20, "");
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
// A and B through 470 Ω into an LED — lit while its contact is closed, the two in turn as it turns. A hand crank on a
// pot across 5 V, a panel meter on its wiper: as far round, as many volts. A drive motor's arm through a slotted
// sensor's slot each turn: its transistor let go — C high —, a T flip-flop toggled, its probe a turn on, a turn off.
void fzx_shaft_sensors(fzx* _x) {
    fzx_part(_x, "hand crank", 0, 0, 0, "");
    // (A, C, B up its right: 40 along, 20 under to 20 over)
    const u32 _en = fzx_part(_x, "rotary encoder", 0, 0, 180, "20");
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
    // The slotted sensor over the motor's shaft, its beam where the arm's far end goes by (120 up): its LED on 5 V
    // through 330 Ω, its C pulled up through 10k; C the T flip-flop's clock, its T held high by a logic input.
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

// Four examples side by side: the counter and the gates beside the gears and rack and the pulleys.
void fzx_workbench(fzx* _x) {
    void (*const _each[4])(fzx*) = { fzx_counter, fzx_gates, fzx_gears_rack, fzx_pulleys };
    const f64 _at[4][2] = { { -550, 350 }, { -550, -500 }, { 500, 350 }, { 500, -450 } };
    for(u32 _i = 0; _i < 4u; _i++) {
        const fude_zoom_v2 _was = fzx_move(_x, _at[_i][0], _at[_i][1]);
        _each[_i](_x);
        _x->p.o = _was;
    }
    fzx_title(_x, -1000, 900, FUDE_ZOOM_EXAMPLE_WORKBENCH);
}

// Seven examples on one canvas: the chaser, the 4-bit adder, the flasher, the gearbox, the machine, the shuttle and the
// turn counter.
void fzx_everything(fzx* _x) {
    void (*const _each[7])(fzx*) = { fzx_chaser, fzx_adder_4, fzx_flasher, fzx_gearbox, fzx_machine, fzx_shuttle, fzx_turn_counter };
    const f64 _at[7][2] = { { -1500, 1300 }, { -1500, -400 }, { 300, 1350 }, { 1400, 1300 }, { 1000, -800 }, { -1300, -2300 }, { 700, -2200 } };
    for(u32 _i = 0; _i < 7u; _i++) {
        const fude_zoom_v2 _was = fzx_move(_x, _at[_i][0], _at[_i][1]);
        _each[_i](_x);
        _x->p.o = _was;
    }
    fzx_title(_x, -2100, 2000, FUDE_ZOOM_EXAMPLE_EVERYTHING);
}
