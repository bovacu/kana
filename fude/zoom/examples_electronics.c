// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/examples_parts.h"

// ===========================================================================
// See examples.h. The examples, the electronics': one function for each (fzx_ and its name), in the list's order
// (examples.h's FUDE_ZOOM_EXAMPLE_, as examples.c's table has them): the first circuits and the logic, what goes wrong
// (limits.h), the displays (display.h), then measuring, sensing, power, sound and the big projects. A helper of their
// own sits just before the first example that uses it.
//
// Part of the examples: examples_parts.h holds what the modules share and says which module holds what. File-local
// names start with fzx_. Every position is in screen points at ×1 from the view's middle (Y up). Main thread only.
// ===========================================================================

// --- the first circuits and the logic --------------------------------------------------------------------------------

// A battery, a switch, a resistor and an LED: a red LED lit through 330 Ω from 9 V.
void fzx_torch(fzx* _x) {
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
void fzx_gates(fzx* _x) {
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

// A 555 astable on 9 V (R1 1k, R2 47k, 10 µF) flashing a yellow LED through 330 Ω.
void fzx_flasher(fzx* _x) {
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

// One full adder of gates (1 + 1 + 0), its carry out on a probe of its own.
void fzx_full_adder(fzx* _x) {
    u32 _co;
    fzx_full_adder_at(_x, 0.0, "1", "1", FUDE_ZOOM_NONE, 0u, "0", "S", &_co);
    const u32 _cp = fzx_part(_x, "logic probe", 300, -80.0, 0, "CO");
    fzx_wire(_x, _co, 2, _cp, 0);
    fzx_title(_x, -430, 230, FUDE_ZOOM_EXAMPLE_FULL_ADDER);
}

// Four of them, the carry along (A 0101 = 5, B 0011 = 3: S 1000 = 8, no carry out).
void fzx_adder_4(fzx* _x) {
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
void fzx_counter(fzx* _x) {
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
void fzx_adder_chip(fzx* _x) {
    const u32 _ic = fzx_part(_x, "74HC283", 0, 0, 0, "74HC283");
    // (its pins: 0 S2, 1 B2, 2 A2, 3 S1, 4 A1, 5 B1, 6 C0, 7 GND, 8 C4, 9 S4, 10 B4, 11 A4, 12 S3, 13 A3, 14 B3,
    // 15 VCC)
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
void fzx_chaser(fzx* _x) {
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

// --- what goes wrong (limits.h): parts past what they take -----------------------------------------------------------

// An LED behind 470 Ω on 9 V (19 mA: lit, well within its 30 mA), and one straight on it (its battery's own 0.5 Ω all
// that holds it back: burnt at once — Restart puts in new parts).
void fzx_led_resistor(fzx* _x) {
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

// 47 Ω on 5 V (0.53 W in it): a resistor of a quarter of a watt (its text naming no other: a typical one's) heating
// past its watts — burnt in about half a second —; one of 1 W (its card's limits) warm, well within them.
void fzx_resistor_watts(fzx* _x) {
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
void fzx_short_fuse(fzx* _x) {
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
        // (right of it, level with the switch)
        fzx_word(_x, 265, _y + 60, 16.0, 260.0, FUDE_ZOOM_EXAMPLE_WORD_TAP_SHORT);
    }
    fzx_title(_x, -240, 350, FUDE_ZOOM_EXAMPLE_SHORT_FUSE);
}

// Electrolytics charged from 12 V: a 16 V one, well; one backwards (its + toward ground: past its 1 V the wrong way
// round, popped); a 6.3 V one (its card's limits) past its volts, popped.
void fzx_cap_polarity(fzx* _x) {
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
void fzx_transistor_size(fzx* _x) {
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
void fzx_ringing(fzx* _x) {
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

// --- displays (display.h) --------------------------------------------------------------------------------------------

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
void fzx_digit_counter(fzx* _x) {
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
void fzx_matrix_scan(fzx* _x) {
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

// A small ground or supply rail (_volts NULL: a ground) turned to face a pin on its part's left, _gap from it: a wire
// straight between them.
RDE_INTERNAL void fzx_tie_left(fzx* _x, u32 _o, u32 _pin, f64 _gap, const c8* _volts) {
    const fude_zoom_v2 _p = fzx_pin(_x, _o, _pin);
    const u32 _t = _volts == NULL ? fzx_sized(_x, "ground", _p.x - _gap - 10.0, _p.y, -90.0, 20, 20, "")
                                  : fzx_sized(_x, "supply rail", _p.x - _gap - 6.0, _p.y, 90.0, 20, 12, _volts);
    fzx_line(_x, _o, _pin, _t, 0);
}

// An LM3914 lighting a bar graph (turned: its first LED at the bottom) as far up as SIG is of its reference (1.25 V on
// RHI, RLO grounded: steps of 125 mV) — SIG a pot's wiper across that reference (tap it: 65 %, six lit); a bar (MODE on
// V+); its LEDs' current ten times what REF OUT gives (R1 1.2 kΩ and the pot: about 13 mA).
void fzx_bar_meter(fzx* _x) {
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
void fzx_panel_meters(fzx* _x) {
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
void fzx_lcd_by_hand(fzx* _x) {
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

// --- measuring, sensing, power, sound and the big projects -----------------------------------------------------------

// A 100 Hz square wave into 10 kΩ and 100 nF (a millisecond its time constant): on an oscilloscope, the square on CH1
// (yellow), the capacitor charging and discharging toward it on CH2 (cyan), 2 ms a division.
void fzx_scope_rc(fzx* _x) {
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

// A night light: an LDR under 47 kΩ from 5 V, its middle through 4.7 kΩ into a transistor's base — in the dark the
// LDR's resistance high, the base high, the LED on (a room's light: off) —; and a thermometer: a thermistor under
// 10 kΩ, a panel meter on it (2.50 V at 25 °C, less as it warms). Play's sliders, or a tap on them, change the light
// and the heat.
void fzx_light_temp(fzx* _x) {
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

// A 4 Hz clock into a CD4017 (its reset and its clock inhibit held low, on 5 V): each of its ten outputs through 470 Ω
// into an LED, in a row under it, numbered — each output's wire out to its own lane and down, none crossing another
// (the row in its package's order: 5 1 0 2 6 7 3 on its left, 8 4 9 on its right). One lit at a time, 0 to 9 and round.
void fzx_decade(fzx* _x) {
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
void fzx_flipflops(fzx* _x) {
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

// A night light: an LM393 (on 5 V) comparing an LDR's divider (10k from 5 V over it, the LDR to ground: higher as it
// gets darker) with a pot's wiper; dark enough, its open collector sinks an LED's current from 5 V through 220 Ω.
void fzx_comparator(fzx* _x) {
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

// An SCR on 12 V in a row with a switch and a lamp: a logic input through 1k into its gate fires it, and it stays on
// when the gate lets go — the switch opened, it stops (closed again, it stays off). A TRIAC on 12 V AC with a lamp: a
// switch and 1k from its MT2 side into its gate fire it each half of each cycle.
void fzx_thyristors(fzx* _x) {
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
void fzx_power_supply(fzx* _x) {
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
// phototransistor sinking a green LED's current from a 9 V battery of its own. A photodiode backwards on 5 V through
// 100k, a phototransistor on 5 V through 1k, a voltmeter on each: as much light, as much less.
void fzx_light_link(fzx* _x) {
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
// round its left and under it (none along its pins' ends). (Its pins: GND 30, TRIG 10, OUT −10, RESET −30 down its
// left, x −100; VCC 30, DISCH 10, THRES −10, CTRL −30 down its right, x 100.)
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
void fzx_speaker_tone(fzx* _x) {
    const u32 _t  = fzx_astable(_x, 0, 0, "9V", "1k", "15k", "100nF", false, true);
    const u32 _rl = fzx_part(_x, "resistor", -230, -10, 0, "470");
    const u32 _sp = fzx_part(_x, "speaker", -310, -60, -90, "8 ohm");
    const u32 _gs = fzx_part(_x, "ground", -370, -40, 0, "");
    fzx_route(_x, _t, 2, _rl, 1, NULL, 0);
    fzx_route(_x, _rl, 0, _sp, 0, FZX_VIA({ -300, -10 }));
    fzx_route(_x, _sp, 1, _gs, 0, FZX_VIA({ -320, 0 }, { -370, 0 }));
    fzx_title(_x, -370, 170, FUDE_ZOOM_EXAMPLE_SPEAKER_TONE);
}

// The traffic lights' CD4017 with its supply, INH and RST (logic inputs, low), and its clock (a 555 at about 1.4 Hz)
// on its CLK. Returns the CD4017.
RDE_INTERNAL u32 fzx_traffic_counter(fzx* _x) {
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
    return _ch;
}

// The traffic lights' housings: a box round each three lights and its name (N–S, E–W) by its top.
RDE_INTERNAL void fzx_traffic_housings(fzx* _x) {
    // (their housings, a box round each three)
    for(u32 _h = 0; _h < 2u; _h++) {
        // (round its three lights and the names under them, apart from the other's; its own name by its
        // top, right of the grounds — between the two there is no room for it)
        const f64 _top = -260.0 - 240.0 * (f64)_h + 24.0, _bottom = _top - 232.0;
        const fude_zoom_v2 _box[4] = { fzx_at(_x, 470, _top), fzx_at(_x, 550, _top), fzx_at(_x, 550, _bottom), fzx_at(_x, 470, _bottom) };
        fzx_path(_x, _box, 4u, true, 2.0);
        fzx_text(_x, 594, _top, 15.0, 60.0, _h == 0u ? "N\xE2\x80\x93S" : "E\xE2\x80\x93W");
    }
}

// The traffic lights' CD4511 (_dec, its middle _dy high) set to show (~LT, ~BI high, LE low), its ground and its
// supply, and the digit it lights, a resistor in each segment's line.
RDE_INTERNAL void fzx_traffic_digit(fzx* _x, u32 _dec, f64 _dy) {
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
}

// The traffic lights' beeper: a 555 (1k, 6.8k, 100 nF: about 1 kHz) right of WALK's resistor _wr (its line _wy high),
// its RESET on WALK, its OUT through 220 Ω into a speaker.
RDE_INTERNAL void fzx_traffic_beeper(fzx* _x, u32 _wr, f64 _wy) {
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
}

// A crossroads' traffic lights, as they were made before microcontrollers: a 555 (about 1.4 Hz) steps a CD4017 through
// its ten outputs, and a DIODE MATRIX (a ROM of 39 diodes) makes of them every light — each output's lane down through
// a diode into each line it lights (N–S red 5–9, yellow 4, green 0–3; E–W red 0–4, yellow 9, green 5–8; the
// pedestrians' WALK 5–8), and into the lines of its number in binary (1, 2, 4, 8) for a CD4511 and a digit: the step
// shown. While they may walk, a second 555 (RESET on WALK, pulled down) beeps on a speaker. INH and RST (logic inputs)
// hold it or start it again. Each line pulled down where nothing lights it (10k), each diode's anode on its lane, its
// cathode on its line.
void fzx_traffic(fzx* _x) {
    const u32 _ch = fzx_traffic_counter(_x);
    // The matrix: each output's lane (its count, its pin, its x — the higher its pin, the farther out, none crossing)
    // and the lines it is in.
    static const u32 _pin[10]  = { 2, 1, 3, 6, 9, 0, 4, 5, 8, 10 };   // (count 0 to 9)
    static const f64 _lane[10] = { -440, -510, -370, -160, 230, -580, -300, -230, 160, 300 };
    enum { NS_R, NS_Y, NS_G, EW_R, EW_Y, EW_G, BIT_2, BIT_4, BIT_8, BIT_1, WALK, LINES };
    // (bit n: count n)
    static const u16 _of[LINES] = { 0x3E0, 0x010, 0x00F, 0x01F, 0x200, 0x1E0, 0x0CC, 0x0F0, 0x300, 0x2AA, 0x1E0 };
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
    // The lights: each line through 1k into its LED (a CD4017's output gives 10 mA at most, and one lights two or
    // three) — N–S's red, yellow, green over each other, E–W's under them.
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
    fzx_traffic_housings(_x);
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
    fzx_traffic_digit(_x, _dec, _dy);
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
    fzx_traffic_beeper(_x, _wr, _wy);
    fzx_word(_x, -620, 300, 16.0, 640.0, FUDE_ZOOM_EXAMPLE_WORD_TRAFFIC);
    fzx_title(_x, -620, 400, FUDE_ZOOM_EXAMPLE_TRAFFIC);
}

// A 555 piano: R1 1k from 9 V to DISCH, and from DISCH thirteen keys — each a resistor and a push button — to THRES and
// TRIG's capacitor (100 nF): held, a key is R2, its note about 1.44 / ((R1 + 2 R2) C) — C4 to C5, a semitone a key, its
// frequency under it. Its OUT through 100 Ω and a volume pot (a rheostat: tapped, a quarter round) into 10 µF and a
// speaker — silent while no key is held (OUT high, nothing through the capacitor). An oscilloscope on OUT.
void fzx_piano(fzx* _x) {
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
