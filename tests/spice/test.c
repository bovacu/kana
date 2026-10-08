// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

// The circuit simulator (fude/zoom/circuit.c) against a reference: ngspice, the standard open simulator, given the same
// circuits with the same models (a diode's IS and N, a transistor's Ebers–Moll, a MOSFET's square law, an op-amp's
// curve) — what differs is ours: the solver, the integration, the steps. Each circuit drawn on a canvas as a person
// draws it (parts, wires), played, measured; the same as a netlist, run in ngspice, measured; the two compared. Where
// the answer is known on paper (a 555's period), against that too. No ngspice installed: said, and skipped.
#include "zoom/scene.h"
#include "zoom/shape.h"
#include "zoom/symbol.h"
#include "zoom/circuit.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while(0)

// --- drawing a circuit --------------------------------------------------------------------------------

static u32 part_put(fude_zoom_scene* s, const c8* id, f64 x, f64 y, f64 hw, f64 hh, const c8* text) {
    f64 n[FUDE_ZOOM_SHAPE_NUMBERS + 200];
    const u32 kind = fude_zoom_symbol_find(id);
    CHECK(kind != FUDE_ZOOM_NONE);
    const u32 k = fude_zoom_symbol_numbers(n, kind, hw, hh, 2.0, text);
    return fude_zoom_scene_add_shape(s, s->root, (fude_zoom_place){ { x, y }, 0.0, 1.0 }, FUDE_ZOOM_SHAPE_SYMBOL, n, k, (rde_color){ 1, 1, 1, 255 }, 0.2f, 0u, 0);
}

static void wire_put(fude_zoom_scene* s, u32 a, u32 pa, u32 b, u32 pb) {
    fude_zoom_v2 p0 = { 0.0, 0.0 }, p1 = { 0.0, 0.0 }, r[6];
    CHECK(fude_zoom_part_pin_at(s, a, pa, &p0) && fude_zoom_part_pin_at(s, b, pb, &p1));
    const u32 m = fude_zoom_wire_route(p0, fude_zoom_part_side_at(s, a, pa), p1, fude_zoom_part_side_at(s, b, pb), 10.0, r);
    fude_zoom_wire_add(s, s->root, r, m, a, (i32)pa, b, (i32)pb, (rde_color){ 1, 1, 1, 255 }, 0.1f);
}

static const fude_zoom_circuit_part* cpart(const fude_zoom_circuit* c, u32 object) {
    for(u32 i = 0; i < (u32)rde_arr_length(&c->parts); i++) {
        if(((const fude_zoom_circuit_part*)c->parts.memory)[i].object == object) return &((const fude_zoom_circuit_part*)c->parts.memory)[i];
    }
    return NULL;
}

// A part's pin's voltage, as last solved.
static f64 volts(const fude_zoom_circuit* c, u32 object, u32 pin) {
    const fude_zoom_circuit_part* p = cpart(c, object);
    return p != NULL ? fude_zoom_circuit_volts(c, p->node[pin]) : NAN;
}

// On to time _t.
static b8 until(fude_zoom_circuit* c, f64 t) {
    return t <= c->time || fude_zoom_circuit_advance(c, t - c->time, 1000000u, true);
}

// --- ngspice ------------------------------------------------------------------------------------------

static const c8* spice = NULL;   // its command (NULL: not installed)
static c8 build_dir[512];

// Netlist _net run in ngspice; each .meas or printed value "name = number" of its output into _names' places of _out.
static b8 spice_run(const c8* net, const c8* const* names, u32 n, f64* out) {
    c8 cir[600], log_path[600], cmd[1400];
    snprintf(cir, sizeof(cir), "%s/case.cir", build_dir);
    snprintf(log_path, sizeof(log_path), "%s/case.log", build_dir);
    FILE* f = fopen(cir, "w");
    if(f == NULL) return false;
    fputs(net, f);
    fclose(f);
    snprintf(cmd, sizeof(cmd), "%s -b '%s' > '%s' 2>&1", spice, cir, log_path);
    if(system(cmd) != 0) printf("  (ngspice exited unhappily: see %s)\n", log_path);
    f = fopen(log_path, "r");
    if(f == NULL) return false;
    for(u32 i = 0; i < n; i++) out[i] = NAN;
    c8 line[512];
    while(fgets(line, sizeof(line), f) != NULL) {
        c8 name[128];
        f64 v;
        if(sscanf(line, " %127s = %lf", name, &v) == 2) {
            for(u32 i = 0; i < n; i++) if(strcmp(name, names[i]) == 0) out[i] = v;
        }
    }
    fclose(f);
    b8 all = true;
    for(u32 i = 0; i < n; i++) all = all && !isnan(out[i]);
    if(!all) printf("  (ngspice: not every measure found: see %s)\n", log_path);
    return all;
}

// _ours against _ref, within _rel of it (or _abs).
static b8 near(const c8* what, f64 ours, f64 ref, f64 rel, f64 abs_) {
    const b8 ok = fabs(ours - ref) <= fmax(rel * fabs(ref), abs_);
    printf("  %-34s ours %12.6g  ngspice %12.6g  %s\n", what, ours, ref, ok ? "" : "  <-- off");
    return ok;
}

// --- the circuits ------------------------------------------------------------------------------------

// An RC charging from 5 V through 1 kΩ into 1 µF (τ 1 ms): its voltage at ½, 1, 2, 3 τ — and as e says.
static void case_rc(void) {
    printf("RC charging\n");
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    const u32 rail = part_put(&s, "supply rail", 0, 100, 30, 10, "5V");
    const u32 r = part_put(&s, "resistor", 100, 0, 30, 10, "1k");
    const u32 cap = part_put(&s, "capacitor", 220, 0, 30, 10, "1uF");
    const u32 g = part_put(&s, "ground", 320, -100, 20, 20, "");
    wire_put(&s, rail, 0, r, 0); wire_put(&s, r, 1, cap, 0); wire_put(&s, cap, 1, g, 0);
    fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
    const f64 at[4] = { 0.5e-3, 1e-3, 2e-3, 3e-3 };
    f64 ours[4];
    for(u32 i = 0; i < 4u; i++) {
        CHECK(until(&c, at[i]));
        ours[i] = volts(&c, cap, 0) - volts(&c, cap, 1);
    }
    const c8* const names[4] = { "a", "b", "c", "d" };
    f64 ref[4];
    if(spice != NULL && spice_run("* rc\nV1 1 0 DC 5\nR1 1 2 1k\nC1 2 0 1u IC=0\n.tran 1u 3m UIC\n"
                                  ".meas tran a FIND v(2) AT=0.5m\n.meas tran b FIND v(2) AT=1m\n.meas tran c FIND v(2) AT=2m\n.meas tran d FIND v(2) AT=3m\n.end\n",
                                  names, 4u, ref)) {
        for(u32 i = 0; i < 4u; i++) { c8 w[48]; snprintf(w, sizeof(w), "v(C) at %.1f ms", at[i] * 1e3); CHECK(near(w, ours[i], ref[i], 0.01, 1e-3)); }
    }
    for(u32 i = 0; i < 4u; i++) CHECK(fabs(ours[i] - 5.0 * (1.0 - exp(-at[i] / 1e-3))) < 0.01 * 5.0);
    fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
}

// An RL: 5 V through 100 Ω into 10 mH (τ 0.1 ms): its current at ½, 1, 2 τ.
static void case_rl(void) {
    printf("RL\n");
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    const u32 rail = part_put(&s, "supply rail", 0, 100, 30, 10, "5V");
    const u32 r = part_put(&s, "resistor", 100, 0, 30, 10, "100");
    const u32 l = part_put(&s, "inductor", 220, 0, 30, 10, "10mH");
    const u32 g = part_put(&s, "ground", 320, -100, 20, 20, "");
    wire_put(&s, rail, 0, r, 0); wire_put(&s, r, 1, l, 0); wire_put(&s, l, 1, g, 0);
    fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
    const f64 at[3] = { 0.05e-3, 0.1e-3, 0.2e-3 };
    f64 ours[3];
    for(u32 i = 0; i < 3u; i++) {
        CHECK(until(&c, at[i]));
        ours[i] = fabs(cpart(&c, l)->pin_i[0]);
    }
    const c8* const names[3] = { "a", "b", "c" };
    f64 ref[3];
    if(spice != NULL && spice_run("* rl\nV1 1 0 DC 5\nR1 1 2 100\nL1 2 0 10m IC=0\n.tran 0.1u 0.3m UIC\n"
                                  ".meas tran a FIND i(L1) AT=0.05m\n.meas tran b FIND i(L1) AT=0.1m\n.meas tran c FIND i(L1) AT=0.2m\n.end\n",
                                  names, 3u, ref)) {
        for(u32 i = 0; i < 3u; i++) { c8 w[48]; snprintf(w, sizeof(w), "i(L) at %.2f ms", at[i] * 1e3); CHECK(near(w, ours[i], fabs(ref[i]), 0.01, 1e-5)); }
    }
    fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
}

// A series RLC rung by a 5 V step (10 Ω, 1 mH, 1 µF: about 5 kHz, its swing dying as e^-5000t): the capacitor's voltage
// through its first two swings — how well the steps keep an oscillation (they damp it if too coarse).
static void case_rlc(void) {
    printf("RLC ringing\n");
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    const u32 rail = part_put(&s, "supply rail", 0, 100, 30, 10, "5V");
    const u32 r = part_put(&s, "resistor", 100, 0, 30, 10, "10");
    const u32 l = part_put(&s, "inductor", 220, 0, 30, 10, "1mH");
    const u32 cap = part_put(&s, "capacitor", 340, 0, 30, 10, "1uF");
    const u32 g = part_put(&s, "ground", 440, -100, 20, 20, "");
    wire_put(&s, rail, 0, r, 0); wire_put(&s, r, 1, l, 0); wire_put(&s, l, 1, cap, 0); wire_put(&s, cap, 1, g, 0);
    fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
    const f64 at[5] = { 0.1e-3, 0.2e-3, 0.3e-3, 0.4e-3, 0.6e-3 };
    f64 ours[5];
    for(u32 i = 0; i < 5u; i++) {
        CHECK(until(&c, at[i]));
        ours[i] = volts(&c, cap, 0) - volts(&c, cap, 1);
    }
    const c8* const names[5] = { "a", "b", "c", "d", "e" };
    f64 ref[5];
    if(spice != NULL && spice_run("* rlc\nV1 1 0 DC 5\nR1 1 2 10\nL1 2 3 1m IC=0\nC1 3 0 1u IC=0\n.tran 0.05u 0.7m UIC\n"
                                  ".meas tran a FIND v(3) AT=0.1m\n.meas tran b FIND v(3) AT=0.2m\n.meas tran c FIND v(3) AT=0.3m\n"
                                  ".meas tran d FIND v(3) AT=0.4m\n.meas tran e FIND v(3) AT=0.6m\n.end\n", names, 5u, ref)) {
        for(u32 i = 0; i < 5u; i++) { c8 w[48]; snprintf(w, sizeof(w), "v(C) at %.1f ms", at[i] * 1e3); CHECK(near(w, ours[i], ref[i], 0.03, 0.1)); }
    }
    fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
}

// A diode (1N4148's IS 1e-14, N 1) behind 1 kΩ on 5 V, and an LED (red: its own curve — a diode of IS 7.6e-18, N 2)
// behind 330 Ω: their voltages and currents.
static void case_diodes(void) {
    printf("diode, LED\n");
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    const u32 rail = part_put(&s, "supply rail", 0, 100, 30, 10, "5V");
    const u32 r = part_put(&s, "resistor", 100, 0, 30, 10, "1k");
    const u32 d = part_put(&s, "diode", 220, 0, 30, 10, "1N4148");
    const u32 g = part_put(&s, "ground", 320, -100, 20, 20, "");
    wire_put(&s, rail, 0, r, 0); wire_put(&s, r, 1, d, 0); wire_put(&s, d, 1, g, 0);
    const u32 rail2 = part_put(&s, "supply rail", 0, 600, 30, 10, "5V");
    const u32 r2 = part_put(&s, "resistor", 100, 500, 30, 10, "330");
    const u32 led = part_put(&s, "LED", 220, 500, 30, 20, "red");
    const u32 g2 = part_put(&s, "ground", 320, 400, 20, 20, "");
    wire_put(&s, rail2, 0, r2, 0); wire_put(&s, r2, 1, led, 0); wire_put(&s, led, 1, g2, 0);
    fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
    CHECK(fude_zoom_circuit_dc(&c));
    const f64 ours[4] = { volts(&c, d, 0), cpart(&c, d)->pin_i[0], volts(&c, led, 0), cpart(&c, led)->pin_i[0] };
    const c8* const names[4] = { "vd", "id", "vl", "il" };
    f64 ref[4];
    if(spice != NULL && spice_run("* diodes\n.model D1 D(IS=1e-14 N=1)\n.model DL D(IS=7.6017e-18 N=2)\nV1 1 0 DC 5\nR1 1 2 1k\nD1 2 0 D1\n"
                                  "R2 1 3 330\nD2 3 0 DL\n.control\nop\nlet vd = v(2)\nlet id = (v(1)-v(2))/1k\nlet vl = v(3)\nlet il = (v(1)-v(3))/330\n"
                                  "print vd id vl il\n.endc\n.end\n", names, 4u, ref)) {
        CHECK(near("diode volts", ours[0], ref[0], 0.002, 1e-3));
        CHECK(near("diode current", ours[1], ref[1], 0.002, 1e-6));
        CHECK(near("LED volts", ours[2], ref[2], 0.002, 1e-3));
        CHECK(near("LED current", ours[3], ref[3], 0.002, 1e-6));
    }
    fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
}

// A common-emitter stage: 9 V, 1 kΩ to the collector, 470 kΩ from the supply to the base (Ebers–Moll: IS 1e-14, β 100):
// its base and collector voltages.
static void case_bjt(void) {
    printf("transistor stage\n");
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    const u32 rail = part_put(&s, "supply rail", 0, 300, 30, 10, "9V");
    const u32 rc = part_put(&s, "resistor", 300, 200, 30, 10, "1k");
    const u32 rb = part_put(&s, "resistor", 100, 0, 30, 10, "470k");
    const u32 q = part_put(&s, "NPN", 400, 0, 20, 30, "");
    const u32 g = part_put(&s, "ground", 420, -150, 20, 20, "");
    wire_put(&s, rail, 0, rc, 0); wire_put(&s, rc, 1, q, 1);
    wire_put(&s, rail, 0, rb, 0); wire_put(&s, rb, 1, q, 0);
    wire_put(&s, q, 2, g, 0);
    fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
    CHECK(fude_zoom_circuit_dc(&c));
    const f64 ours[2] = { volts(&c, q, 0), volts(&c, q, 1) };
    const c8* const names[2] = { "vb", "vc" };
    f64 ref[2];
    if(spice != NULL && spice_run("* ce\n.model QN NPN(IS=1e-14 BF=100 BR=1)\nV1 1 0 DC 9\nRC 1 2 1k\nRB 1 3 470k\nQ1 2 3 0 QN\n"
                                  ".control\nop\nlet vb = v(3)\nlet vc = v(2)\nprint vb vc\n.endc\n.end\n", names, 2u, ref)) {
        CHECK(near("base volts", ours[0], ref[0], 0.002, 1e-3));
        CHECK(near("collector volts", ours[1], ref[1], 0.005, 5e-3));
    }
    fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
}

// A MOSFET (square law: VTO 2 V, KP 1, λ 0.01) as a switch: 5 V on its gate, 100 Ω from 5 V to its drain — and half
// on: 2.5 V on its gate, 1 kΩ.
static void case_mosfet(void) {
    printf("MOSFET\n");
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    const u32 rail = part_put(&s, "supply rail", 0, 300, 30, 10, "5V");
    const u32 rd = part_put(&s, "resistor", 300, 200, 30, 10, "100");
    const u32 gate = part_put(&s, "supply rail", 200, 100, 30, 10, "5V");
    const u32 m = part_put(&s, "N-MOSFET", 400, 0, 20, 30, "");
    const u32 g = part_put(&s, "ground", 420, -150, 20, 20, "");
    wire_put(&s, rail, 0, rd, 0); wire_put(&s, rd, 1, m, 1); wire_put(&s, gate, 0, m, 0); wire_put(&s, m, 2, g, 0);
    const u32 rail2 = part_put(&s, "supply rail", 0, 1300, 30, 10, "5V");
    const u32 rd2 = part_put(&s, "resistor", 300, 1200, 30, 10, "1k");
    const u32 gate2 = part_put(&s, "supply rail", 200, 1100, 30, 10, "2.5V");
    const u32 m2 = part_put(&s, "N-MOSFET", 400, 1000, 20, 30, "");
    const u32 g2 = part_put(&s, "ground", 420, 850, 20, 20, "");
    wire_put(&s, rail2, 0, rd2, 0); wire_put(&s, rd2, 1, m2, 1); wire_put(&s, gate2, 0, m2, 0); wire_put(&s, m2, 2, g2, 0);
    fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
    CHECK(fude_zoom_circuit_dc(&c));
    const f64 ours[2] = { volts(&c, m, 1), volts(&c, m2, 1) };
    const c8* const names[2] = { "vd", "vd2" };
    f64 ref[2];
    if(spice != NULL && spice_run("* mos\n.model MN NMOS(LEVEL=1 VTO=2 KP=1 LAMBDA=0.01)\nV1 1 0 DC 5\nRD 1 2 100\nVG 3 0 DC 5\nM1 2 3 0 0 MN W=1 L=1\n"
                                  "RD2 1 4 1k\nVG2 5 0 DC 2.5\nM2 4 5 0 0 MN W=1 L=1\n.control\nop\nlet vd = v(2)\nlet vd2 = v(4)\nprint vd vd2\n.endc\n.end\n",
                                  names, 2u, ref)) {
        CHECK(near("drain volts, on", ours[0], ref[0], 0.01, 2e-3));
        CHECK(near("drain volts, half on", ours[1], ref[1], 0.01, 2e-3));
    }
    fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
}

// An inverting amplifier: 0.5 V in through 1 kΩ, 10 kΩ back (gain −10), the op-amp's own ±12 V rails: −5 V out.
static void case_opamp(void) {
    printf("op-amp\n");
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    const u32 in = part_put(&s, "supply rail", 0, 100, 30, 10, "0.5V");
    const u32 rin = part_put(&s, "resistor", 140, 20, 30, 10, "1k");
    const u32 op = part_put(&s, "op-amp", 320, 0, 40, 40, "");
    const u32 rf = part_put(&s, "resistor", 320, 120, 30, 10, "10k");
    const u32 g = part_put(&s, "ground", 200, -120, 20, 20, "");
    wire_put(&s, in, 0, rin, 0); wire_put(&s, rin, 1, op, 0);
    wire_put(&s, op, 0, rf, 0); wire_put(&s, rf, 1, op, 2);
    wire_put(&s, op, 1, g, 0);
    fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
    CHECK(fude_zoom_circuit_dc(&c));
    const f64 ours = volts(&c, op, 2);
    const c8* const names[1] = { "vo" };
    f64 ref[1];
    if(spice != NULL && spice_run("* opamp\nV1 1 0 DC 0.5\nRIN 1 2 1k\nRF 2 4 10k\nBX 3 0 V = 11.95*tanh(1e5*(0-v(2))/11.95)\nRO 3 4 1\n"
                                  ".control\nop\nlet vo = v(4)\nprint vo\n.endc\n.end\n", names, 1u, ref)) {
        CHECK(near("output volts", ours, ref[0], 0.002, 2e-3));
    }
    CHECK(fabs(ours + 5.0) < 0.01);
    fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
}

// A 555 astable (R1 1 kΩ, R2 10 kΩ, C 1 µF, 9 V) against its datasheet: high for 0.693 (R1 + R2) C, low for 0.693 R2 C.
// And at a servo's 50 Hz with a 1.5 ms pulse? (Not a 555's: its high is always the longer.) Its edges measured where the
// output crosses half the supply, over several periods.
static void case_555(void) {
    printf("555 astable\n");
    fude_zoom_scene s; fude_zoom_scene_init(&s, 7);
    // DIP: GND 1, TRIG 2, OUT 3, RESET 4, CTRL 5, THRES 6, DISCH 7, VCC 8 (pins 0..7).
    const u32 ic = part_put(&s, "NE555", 400, 0, 40, 50, "NE555");
    const u32 rail = part_put(&s, "supply rail", 200, 300, 30, 10, "9V");
    const u32 r1 = part_put(&s, "resistor", 650, 250, 30, 10, "1k");
    const u32 r2 = part_put(&s, "resistor", 650, 120, 30, 10, "10k");
    const u32 cap = part_put(&s, "capacitor", 650, -150, 30, 10, "1uF");
    const u32 g = part_put(&s, "ground", 200, -300, 20, 20, "");
    const u32 load = part_put(&s, "resistor", 150, 0, 30, 10, "10k");
    wire_put(&s, rail, 0, ic, 7); wire_put(&s, rail, 0, ic, 3);   // (VCC, RESET)
    wire_put(&s, rail, 0, r1, 0); wire_put(&s, r1, 1, ic, 6);     // (R1 to DISCH)
    wire_put(&s, ic, 6, r2, 0); wire_put(&s, r2, 1, ic, 5);       // (R2 from DISCH to THRES)
    wire_put(&s, ic, 5, ic, 1);                                   // (THRES to TRIG)
    wire_put(&s, ic, 1, cap, 0); wire_put(&s, cap, 1, g, 0);
    wire_put(&s, ic, 0, g, 0);
    wire_put(&s, ic, 2, load, 1); wire_put(&s, load, 0, g, 0);   // (OUT through 10 kΩ to ground)
    fude_zoom_circuit c; fude_zoom_circuit_init(&c); fude_zoom_circuit_build(&c, &s);
    // Settled into its swing, then its output's edges: each crossing of half the supply, between steps interpolated.
    CHECK(until(&c, 0.05));
    f64 was = volts(&c, ic, 2), t_was = c.time, rises[16], falls[16];
    u32 nr = 0, nf = 0;
    while(c.time < 0.2 && nr < 16u && nf < 16u) {
        CHECK(fude_zoom_circuit_advance(&c, 1e-5, 1000u, false));
        const f64 v = volts(&c, ic, 2);
        if((was < 4.5) != (v < 4.5)) {
            const f64 at = t_was + (4.5 - was) / (v - was) * (c.time - t_was);
            if(v >= 4.5) rises[nr++] = at; else falls[nf++] = at;
        }
        was = v;
        t_was = c.time;
    }
    CHECK(nr >= 4u && nf >= 4u);
    f64 high = 0.0, low = 0.0;
    u32 nh = 0, nl = 0;
    for(u32 i = 0; i < nr; i++) {
        for(u32 j = 0; j < nf; j++) if(falls[j] > rises[i]) { high += falls[j] - rises[i]; nh++; break; }
    }
    for(u32 j = 0; j < nf; j++) {
        for(u32 i = 0; i < nr; i++) if(rises[i] > falls[j]) { low += rises[i] - falls[j]; nl++; break; }
    }
    high /= (f64)(nh > 0u ? nh : 1u);
    low  /= (f64)(nl > 0u ? nl : 1u);
    // (its discharge transistor's 10 Ω on, as a real one's saturates: R1 holds DISCH 89 mV up, the low that much longer than
    // the datasheet's ideal 0.693 R2 C)
    const f64 vsat = 9.0 * 10.0 / (1e3 + 10.0);
    const f64 want_high = 0.693 * 11e3 * 1e-6, want_low = log((6.0 - vsat) / (3.0 - vsat)) * 10e3 * 1e-6;
    printf("  %-34s ours %12.6g  paper    %12.6g\n", "high (ms)", high * 1e3, want_high * 1e3);
    printf("  %-34s ours %12.6g  paper    %12.6g\n", "low (ms)", low * 1e3, want_low * 1e3);
    CHECK(fabs(high - want_high) < 0.02 * want_high);
    CHECK(fabs(low - want_low) < 0.02 * want_low);
    fude_zoom_circuit_destroy(&c); fude_zoom_scene_destroy(&s);
}

int main(void) {
    snprintf(build_dir, sizeof(build_dir), ".");   // (where it runs: its own folder of tests/build)
    // ngspice: where Homebrew or a system puts it.
    static const c8* const where[] = { "/opt/homebrew/bin/ngspice", "/usr/local/bin/ngspice", "/usr/bin/ngspice" };
    for(u32 i = 0; i < sizeof(where) / sizeof(where[0]) && spice == NULL; i++) {
        FILE* f = fopen(where[i], "r");
        if(f != NULL) { fclose(f); spice = where[i]; }
    }
    if(spice == NULL) printf("ngspice not installed: only the answers on paper checked\n");
    case_rc();
    case_rl();
    case_rlc();
    case_diodes();
    case_bjt();
    case_mosfet();
    case_opamp();
    case_555();
    if(fails == 0) printf("ALL PASSED\n"); else printf("%d FAILED\n", fails);
    return fails == 0 ? 0 : 1;
}
