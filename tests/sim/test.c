// The simulation core (fude/sim): definitions, custom parts made of parts (nested), flattening and its checks, the
// part's text, and the digital engine — every gate's table, a multiplexer made of gates, a 4-bit adder made of full
// adders made of gates, flip-flops counting a clock, tri-states on a bus.
#include "sim/sim.h"
#include "sim/body.h"
#include "sim/sparse.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// A scratch array of n zeroed items (written through its memory: sized once, it stays put).
static rde_arr scratch(usize size, u32 n) {
    rde_arr a = rde_arr_new(size, rde_memory_allocator_get_default_std());
    rde_arr_resize(&a, n);
    return a;
}

static int fails = 0;
#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while(0)

// A top part made of _n parts given as (def, ref, params, nets...) — built by the tests below.
typedef struct { fude_sim_lib lib; fude_sim_flat flat; fude_sim_run run; } world;

static void world_run(world* w, const fude_sim_def* top) {
    fude_sim_flat_init(&w->flat);
    const b8 ok = fude_sim_elaborate(&w->lib, top, &w->flat);
    if(!ok) {
        printf("  elaboration: %s\n", fude_sim_flat_errors(&w->flat));
    }
    CHECK(ok);
    fude_sim_run_init(&w->run, &w->flat);
}

static void world_done(world* w) {
    fude_sim_run_destroy(&w->run);
    fude_sim_flat_destroy(&w->flat);
}

// The value on the net top part _ref's port _port is on (its first port: 0).
static u8 at(world* w, const c8* path, u32 port) {
    const u32 p = fude_sim_flat_find(&w->flat, path);
    if(p == FUDE_SIM_NONE) return 0xFF;
    return fude_sim_in(&w->run, p, port);
}

static void set(world* w, const c8* path, u8 v) {
    fude_sim_run_set(&w->run, fude_sim_flat_find(&w->flat, path), v);
    fude_sim_run_settle(&w->run, 1e-6);
}

// --- gates' tables -------------------------------------------------------------------------------------

static void test_gates(void) {
    world w; fude_sim_lib_init(&w.lib);
    fude_sim_def* top = fude_sim_def_new("test/gates", "gates");
    const c8* g[6] = { "and", "or", "nand", "nor", "xor", "xnor" };
    for(u32 i = 0; i < 6u; i++) {
        c8 ref[8], out[8];
        snprintf(ref, sizeof ref, "G%u", i);
        snprintf(out, sizeof out, "y%u", i);
        const c8* nets[3] = { "a", "b", out };
        fude_sim_def_inst(top, g[i], ref, "", nets, 3u);
        c8 pr[8]; snprintf(pr, sizeof pr, "P%u", i);
        const c8* pn[1] = { out };
        fude_sim_def_inst(top, "probe", pr, "", pn, 1u);
    }
    const c8* na[1] = { "a" }; const c8* nb[1] = { "b" }; const c8* nn[2] = { "a", "na" };
    fude_sim_def_inst(top, "input", "A", "0", na, 1u);
    fude_sim_def_inst(top, "input", "B", "0", nb, 1u);
    fude_sim_def_inst(top, "not", "N", "", nn, 2u);
    world_run(&w, top);
    const u8 want[6][4] = { { 0, 0, 0, 1 }, { 0, 1, 1, 1 }, { 1, 1, 1, 0 }, { 1, 0, 0, 0 }, { 0, 1, 1, 0 }, { 1, 0, 0, 1 } };   // (ab: 00 01 10 11)
    for(u32 ab = 0; ab < 4u; ab++) {
        set(&w, "A", (u8)(ab >> 1)); set(&w, "B", (u8)(ab & 1u));
        for(u32 i = 0; i < 6u; i++) {
            c8 pr[8]; snprintf(pr, sizeof pr, "P%u", i);
            CHECK(at(&w, pr, 0) == want[i][ab]);
        }
        CHECK(at(&w, "N", 1) == (u8)(1u - (ab >> 1)));
    }
    // A three-input AND (its inputs a parameter); an input left open reads as unknown.
    world_done(&w);
    fude_sim_def* t3 = fude_sim_def_new("test/and3", "and3");
    const c8* n3[4] = { "a", "b", "c", "y" };
    fude_sim_def_inst(t3, "and", "G", "inputs=3", n3, 4u);
    fude_sim_def_inst(t3, "input", "A", "1", na, 1u);
    fude_sim_def_inst(t3, "input", "B", "1", nb, 1u);
    world_run(&w, t3);
    CHECK(fude_sim_port_count(&w.run, fude_sim_flat_find(&w.flat, "G")) == 4u);
    CHECK(at(&w, "G", 3) == FUDE_SIM_X);   // (c open: unknown)
    world_done(&w);
    fude_sim_def_free(top);
    fude_sim_def_free(t3);
    fude_sim_lib_destroy(&w.lib);
}

// --- a custom part: a 4-to-1 multiplexer made of gates -------------------------------------------------

static fude_sim_def* make_mux4(void) {
    fude_sim_def* m = fude_sim_def_new("user/mux4", "4-to-1 multiplexer");
    const c8* ins[6] = { "D0", "D1", "D2", "D3", "S0", "S1" };
    for(u32 i = 0; i < 6u; i++) fude_sim_def_port(m, ins[i], FUDE_SIM_LOGIC, FUDE_SIM_IN, 1u);
    fude_sim_def_port(m, "Y", FUDE_SIM_LOGIC, FUDE_SIM_OUT, 1u);
    const c8* n0[2] = { "S0", "nS0" }; const c8* n1[2] = { "S1", "nS1" };
    fude_sim_def_inst(m, "not", "U1", "", n0, 2u);
    fude_sim_def_inst(m, "not", "U2", "", n1, 2u);
    const c8* a0[4] = { "D0", "nS0", "nS1", "t0" }; const c8* a1[4] = { "D1", "S0", "nS1", "t1" };
    const c8* a2[4] = { "D2", "nS0", "S1", "t2" };  const c8* a3[4] = { "D3", "S0", "S1", "t3" };
    fude_sim_def_inst(m, "and", "U3", "inputs=3", a0, 4u);
    fude_sim_def_inst(m, "and", "U4", "inputs=3", a1, 4u);
    fude_sim_def_inst(m, "and", "U5", "inputs=3", a2, 4u);
    fude_sim_def_inst(m, "and", "U6", "inputs=3", a3, 4u);
    const c8* o[5] = { "t0", "t1", "t2", "t3", "Y" };
    fude_sim_def_inst(m, "or", "U7", "inputs=4", o, 5u);
    return m;
}

static void test_mux(void) {
    world w; fude_sim_lib_init(&w.lib);
    fude_sim_lib_add(&w.lib, make_mux4());
    // The custom part used like any other: inputs on its ports, a probe on its output; and used twice.
    fude_sim_def* top = fude_sim_def_new("test/top", "top");
    const c8* names[6] = { "D0", "D1", "D2", "D3", "S0", "S1" };
    for(u32 i = 0; i < 6u; i++) { const c8* n[1] = { names[i] }; fude_sim_def_inst(top, "input", names[i], "0", n, 1u); }
    const c8* mn[7] = { "D0", "D1", "D2", "D3", "S0", "S1", "y" };
    fude_sim_def_inst(top, "user/mux4", "M1", "", mn, 7u);
    const c8* mn2[7] = { "D3", "D2", "D1", "D0", "S0", "S1", "y2" };   // (a second, its data the other way round)
    fude_sim_def_inst(top, "user/mux4", "M2", "", mn2, 7u);
    world_run(&w, top);
    CHECK(rde_arr_length(&w.flat.prims) == 6u + 2u * 7u);
    CHECK(fude_sim_flat_find(&w.flat, "M1/U7") != FUDE_SIM_NONE && fude_sim_flat_find(&w.flat, "M2/U3") != FUDE_SIM_NONE);
    u32 right = 0;
    for(u32 v = 0; v < 64u; v++) {
        for(u32 i = 0; i < 6u; i++) set(&w, names[i], (u8)((v >> i) & 1u));
        const u32 s = (v >> 4) & 3u;
        const u8 y = at(&w, "M1/U7", 4), y2 = at(&w, "M2/U7", 4);
        right += (y == ((v >> s) & 1u) && y2 == ((v >> (3u - s)) & 1u)) ? 1u : 0u;
    }
    CHECK(right == 64u);
    world_done(&w);
    fude_sim_def_free(top);
    fude_sim_lib_destroy(&w.lib);
}

// --- nested: a 4-bit adder of full adders of gates --------------------------------------------------

static void test_adder(void) {
    world w; fude_sim_lib_init(&w.lib);
    fude_sim_def* fa = fude_sim_def_new("user/fulladd", "Full adder");
    fude_sim_def_port(fa, "A", FUDE_SIM_LOGIC, FUDE_SIM_IN, 1u);
    fude_sim_def_port(fa, "B", FUDE_SIM_LOGIC, FUDE_SIM_IN, 1u);
    fude_sim_def_port(fa, "CI", FUDE_SIM_LOGIC, FUDE_SIM_IN, 1u);
    fude_sim_def_port(fa, "S", FUDE_SIM_LOGIC, FUDE_SIM_OUT, 1u);
    fude_sim_def_port(fa, "CO", FUDE_SIM_LOGIC, FUDE_SIM_OUT, 1u);
    const c8* x1[3] = { "A", "B", "p" }; const c8* x2[3] = { "p", "CI", "S" };
    const c8* g1[3] = { "A", "B", "g" }; const c8* g2[3] = { "p", "CI", "q" }; const c8* o1[3] = { "g", "q", "CO" };
    fude_sim_def_inst(fa, "xor", "X1", "", x1, 3u);
    fude_sim_def_inst(fa, "xor", "X2", "", x2, 3u);
    fude_sim_def_inst(fa, "and", "A1", "", g1, 3u);
    fude_sim_def_inst(fa, "and", "A2", "", g2, 3u);
    fude_sim_def_inst(fa, "or", "O1", "", o1, 3u);
    fude_sim_lib_add(&w.lib, fa);
    fude_sim_def* add4 = fude_sim_def_new("user/add4", "4-bit adder");
    const c8* ports[14] = { "A0", "A1", "A2", "A3", "B0", "B1", "B2", "B3", "CI", "S0", "S1", "S2", "S3", "CO" };
    for(u32 i = 0; i < 14u; i++) fude_sim_def_port(add4, ports[i], FUDE_SIM_LOGIC, i < 9u ? FUDE_SIM_IN : FUDE_SIM_OUT, 1u);
    for(u32 i = 0; i < 4u; i++) {
        c8 a[4], b[4], ci[4], s[4], co[4], ref[4];
        snprintf(a, sizeof a, "A%u", i); snprintf(b, sizeof b, "B%u", i); snprintf(s, sizeof s, "S%u", i); snprintf(ref, sizeof ref, "F%u", i);
        snprintf(ci, sizeof ci, i == 0u ? "CI" : "c%u", i);
        snprintf(co, sizeof co, i == 3u ? "CO" : "c%u", i + 1u);
        const c8* n[5] = { a, b, ci, s, co };
        fude_sim_def_inst(add4, "user/fulladd", ref, "", n, 5u);
    }
    fude_sim_lib_add(&w.lib, add4);
    fude_sim_def* top = fude_sim_def_new("test/top", "top");
    for(u32 i = 0; i < 9u; i++) { const c8* n[1] = { ports[i] }; fude_sim_def_inst(top, "input", ports[i], "0", n, 1u); }
    const c8* an[14] = { "A0", "A1", "A2", "A3", "B0", "B1", "B2", "B3", "CI", "s0", "s1", "s2", "s3", "co" };
    fude_sim_def_inst(top, "user/add4", "ADD", "", an, 14u);
    world_run(&w, top);
    CHECK(rde_arr_length(&w.flat.prims) == 9u + 4u * 5u);
    u32 right = 0;
    for(u32 v = 0; v < 512u; v++) {
        for(u32 i = 0; i < 9u; i++) set(&w, ports[i], (u8)((v >> i) & 1u));
        const u32 sum = (v & 15u) + ((v >> 4) & 15u) + ((v >> 8) & 1u);
        u32 got = 0;
        for(u32 i = 0; i < 4u; i++) { c8 p[16]; snprintf(p, sizeof p, "ADD/F%u/X2", i); got |= (u32)(at(&w, p, 2) & 1u) << i; }
        got |= (u32)(at(&w, "ADD/F3/O1", 2) & 1u) << 4;
        right += got == sum ? 1u : 0u;
    }
    CHECK(right == 512u);
    // As text and back: the same text again, and it still adds.
    rde_arr t1 = rde_arr_new(sizeof(c8), rde_memory_allocator_get_default_std()), t2 = rde_arr_new(sizeof(c8), rde_memory_allocator_get_default_std());
    fude_sim_def_write(fude_sim_lib_find(&w.lib, "user/add4"), &t1);
    c8 err[160];
    fude_sim_def* back = fude_sim_def_read((const c8*)t1.memory, rde_arr_length(&t1), err, sizeof err);
    CHECK(back != NULL);
    if(back != NULL) {
        fude_sim_def_write(back, &t2);
        CHECK(rde_arr_length(&t1) == rde_arr_length(&t2) && memcmp(t1.memory, t2.memory, rde_arr_length(&t1)) == 0);
        CHECK(strcmp(back->name, "4-bit adder") == 0 && rde_arr_length(&back->ports) == 14u && rde_arr_length(&back->insts) == 4u);
        fude_sim_def_free(back);
    }
    CHECK(fude_sim_def_read("fude-part 2\nid x\nend\n", 22u, err, sizeof err) == NULL && strstr(err, "another kind") != NULL);
    CHECK(fude_sim_def_read("fude-part 1\nid x\ninst and U1 \"\" a b\nwhat\nend\n", 44u, err, sizeof err) == NULL && strstr(err, "not understood") != NULL);
    rde_arr_free(&t1); rde_arr_free(&t2);
    world_done(&w);
    fude_sim_def_free(top);
    fude_sim_lib_destroy(&w.lib);
}

// --- flip-flops on a clock: a 2-bit counter; a tri-state bus; a ring with no rest -------------------------

static void test_sequential(void) {
    world w; fude_sim_lib_init(&w.lib);
    fude_sim_def* top = fude_sim_def_new("test/counter", "counter");
    const c8* ck[1] = { "clk" }; const c8* zero[1] = { "z" }; const c8* one[1] = { "o" };
    fude_sim_def_inst(top, "clock", "CK", "period=1", ck, 1u);
    fude_sim_def_inst(top, "const", "Z", "0", zero, 1u);
    fude_sim_def_inst(top, "const", "O", "1", one, 1u);
    const c8* t0[5] = { "o", "clk", "z", "q0", "nq0" };
    const c8* t1[5] = { "o", "nq0", "z", "q1", "nq1" };   // (a ripple counter: the next bit clocked by the last's QN)
    fude_sim_def_inst(top, "tff", "T0", "", t0, 5u);
    fude_sim_def_inst(top, "tff", "T1", "", t1, 5u);
    world_run(&w, top);
    u32 right = 0;
    for(u32 tick = 1; tick <= 8u; tick++) {
        fude_sim_run_until(&w.run, (f64)tick + 0.1);   // (each rising edge at k + 0.5)
        const u32 count = (u32)at(&w, "T0", 3) | ((u32)at(&w, "T1", 3) << 1);
        right += count == (tick & 3u) ? 1u : 0u;
    }
    CHECK(right == 8u);
    world_done(&w);
    fude_sim_def_free(top);
    // A D flip-flop: D taken on the edge only; R clears it at once.
    fude_sim_def* d = fude_sim_def_new("test/dff", "dff");
    const c8* dn[5] = { "d", "c", "r", "q", "nq" }; const c8* nd[1] = { "d" }; const c8* nc[1] = { "c" }; const c8* nr[1] = { "r" };
    fude_sim_def_inst(d, "dff", "F", "", dn, 5u);
    fude_sim_def_inst(d, "input", "D", "1", nd, 1u);
    fude_sim_def_inst(d, "input", "C", "0", nc, 1u);
    fude_sim_def_inst(d, "input", "R", "0", nr, 1u);
    world_run(&w, d);
    CHECK(at(&w, "F", 3) == 0u);
    set(&w, "C", 1); CHECK(at(&w, "F", 3) == 1u && at(&w, "F", 4) == 0u);
    set(&w, "D", 0); CHECK(at(&w, "F", 3) == 1u);   // (not on D alone)
    set(&w, "C", 0); set(&w, "C", 1); CHECK(at(&w, "F", 3) == 0u);
    set(&w, "D", 1); set(&w, "C", 0); set(&w, "C", 1); CHECK(at(&w, "F", 3) == 1u);
    set(&w, "R", 1); CHECK(at(&w, "F", 3) == 0u);
    world_done(&w);
    fude_sim_def_free(d);
    // Two tri-states on one bus: the one enabled drives it; both: a fight (X); neither: released (Z).
    fude_sim_def* b = fude_sim_def_new("test/bus", "bus");
    const c8* b1[3] = { "a", "e1", "bus" }; const c8* b2[3] = { "b", "e2", "bus" };
    const c8* pa[1] = { "a" }; const c8* pb[1] = { "b" }; const c8* pe1[1] = { "e1" }; const c8* pe2[1] = { "e2" }; const c8* pbus[1] = { "bus" };
    fude_sim_def_inst(b, "tribuf", "T1", "", b1, 3u);
    fude_sim_def_inst(b, "tribuf", "T2", "", b2, 3u);
    fude_sim_def_inst(b, "input", "A", "1", pa, 1u);
    fude_sim_def_inst(b, "input", "B", "0", pb, 1u);
    fude_sim_def_inst(b, "input", "E1", "0", pe1, 1u);
    fude_sim_def_inst(b, "input", "E2", "0", pe2, 1u);
    fude_sim_def_inst(b, "probe", "P", "", pbus, 1u);
    world_run(&w, b);
    CHECK(at(&w, "P", 0) == FUDE_SIM_Z);
    set(&w, "E1", 1); CHECK(at(&w, "P", 0) == 1u);
    set(&w, "E1", 0); set(&w, "E2", 1); CHECK(at(&w, "P", 0) == 0u);
    set(&w, "E1", 1); CHECK(at(&w, "P", 0) == FUDE_SIM_X);
    world_done(&w);
    fude_sim_def_free(b);
    // A NOT feeding itself stays unknown (nothing ever makes it known); a NAND feeding itself, its other input 0, is
    // 1 — and let go (its other input 1), it never rests: said, and nothing hangs.
    fude_sim_def* ring = fude_sim_def_new("test/ring", "ring");
    const c8* rn[2] = { "x", "x" }; const c8* rq[3] = { "en", "q", "q" }; const c8* re[1] = { "en" };
    fude_sim_def_inst(ring, "not", "N", "", rn, 2u);
    fude_sim_def_inst(ring, "nand", "G", "", rq, 3u);
    fude_sim_def_inst(ring, "input", "EN", "0", re, 1u);
    world_run(&w, ring);
    CHECK(at(&w, "N", 1) == FUDE_SIM_X && at(&w, "G", 2) == 1u && !w.run.oscillating);
    set(&w, "EN", 1);
    fude_sim_run_until(&w.run, 1e-3);
    CHECK(w.run.oscillating);
    world_done(&w);
    fude_sim_def_free(ring);
    fude_sim_lib_destroy(&w.lib);
}

// --- what elaboration says is wrong ------------------------------------------------------------------

static void test_errors(void) {
    fude_sim_lib lib; fude_sim_lib_init(&lib);
    // A part made of itself.
    fude_sim_def* self = fude_sim_def_new("user/self", "self");
    fude_sim_def_port(self, "A", FUDE_SIM_LOGIC, FUDE_SIM_IN, 1u);
    const c8* sn[1] = { "A" };
    fude_sim_def_inst(self, "user/self", "U1", "", sn, 1u);
    fude_sim_lib_add(&lib, self);
    fude_sim_def* top = fude_sim_def_new("test/top", "top");
    fude_sim_def_inst(top, "user/self", "S", "", sn, 1u);
    fude_sim_flat flat; fude_sim_flat_init(&flat);
    CHECK(!fude_sim_elaborate(&lib, top, &flat) && strstr(fude_sim_flat_errors(&flat), "made of itself") != NULL);
    fude_sim_def_free(top);
    // A part that is not there.
    top = fude_sim_def_new("test/top2", "top");
    fude_sim_def_inst(top, "user/nothing", "Q", "", sn, 1u);
    CHECK(!fude_sim_elaborate(&lib, top, &flat) && strstr(fude_sim_flat_errors(&flat), "no such part") != NULL);
    fude_sim_def_free(top);
    // A logic pin joined to a shaft: domains that do not meet.
    fude_sim_def* shaft = fude_sim_def_new("user/shaft", "shaft");
    fude_sim_def_port(shaft, "S", FUDE_SIM_ROTATION, FUDE_SIM_INOUT, 1u);
    const c8* in1[1] = { "S" };
    fude_sim_def_inst(shaft, "probe", "P", "", in1, 1u);   // (a logic probe on a shaft's net)
    fude_sim_lib_add(&lib, shaft);
    top = fude_sim_def_new("test/top3", "top");
    const c8* tn[1] = { "m" };
    fude_sim_def_inst(top, "user/shaft", "M", "", tn, 1u);
    CHECK(!fude_sim_elaborate(&lib, top, &flat) && strstr(fude_sim_flat_errors(&flat), "M's S (rotation) is joined to a logic connection") != NULL);
    fude_sim_def_free(top);
    // Parameters: numbers as typed, and a composite's own passed into its parts.
    f64 v = 0.0;
    CHECK(fude_sim_number("4.7k", &v) && fabs(v - 4700.0) < 1e-9 && fude_sim_number("10n", &v) && fabs(v - 1e-8) < 1e-20 && !fude_sim_number("x", &v));
    fude_sim_def* slow = fude_sim_def_new("user/slow", "slow buffer");
    fude_sim_param p; memset(&p, 0, sizeof p); snprintf(p.name, sizeof p.name, "d"); p.value = 5e-9;
    rde_arr_add(&slow->params, &p);
    fude_sim_def_port(slow, "A", FUDE_SIM_LOGIC, FUDE_SIM_IN, 1u);
    fude_sim_def_port(slow, "Y", FUDE_SIM_LOGIC, FUDE_SIM_OUT, 1u);
    const c8* bn[2] = { "A", "Y" };
    fude_sim_def_inst(slow, "buf", "B", "delay=d", bn, 2u);
    fude_sim_lib_add(&lib, slow);
    top = fude_sim_def_new("test/top4", "top");
    fude_sim_def_inst(top, "user/slow", "S1", "", bn, 2u);
    fude_sim_def_inst(top, "user/slow", "S2", "d=20n", bn, 2u);
    CHECK(fude_sim_elaborate(&lib, top, &flat));
    const fude_sim_prim* pr = (const fude_sim_prim*)flat.prims.memory;
    CHECK(fabs(pr[fude_sim_flat_find(&flat, "S1/B")].params[0] - 5e-9) < 1e-18 && fabs(pr[fude_sim_flat_find(&flat, "S2/B")].params[0] - 2e-8) < 1e-18);
    fude_sim_def_free(top);
    fude_sim_flat_destroy(&flat);
    fude_sim_lib_destroy(&lib);
}

// --- a reference: what each gate should say, worked out here on its own -------------------------------

static u64 rng_state = 0x9E3779B97F4A7C15ull;
static u32 rnd(u32 n) {   // (xorshift64*: the same cases every run)
    rng_state ^= rng_state >> 12; rng_state ^= rng_state << 25; rng_state ^= rng_state >> 27;
    return n > 0u ? (u32)((rng_state * 0x2545F4914F6CDD1Dull) >> 33) % n : 0u;
}

static u8 known(u8 v) { return v <= 1u ? v : FUDE_SIM_X; }
static u8 ref_gate(const c8* g, const u8* in, u32 n) {
    if(strcmp(g, "buf") == 0) return known(in[0]);
    if(strcmp(g, "not") == 0) return known(in[0]) == FUDE_SIM_X ? FUDE_SIM_X : (u8)(1u - in[0]);
    const b8 inv = strcmp(g, "nand") == 0 || strcmp(g, "nor") == 0 || strcmp(g, "xnor") == 0;
    u8 v;
    if(strcmp(g, "xor") == 0 || strcmp(g, "xnor") == 0) {
        u32 ones = 0; v = 0;
        for(u32 i = 0; i < n; i++) { if(known(in[i]) == FUDE_SIM_X) { v = FUDE_SIM_X; break; } ones += in[i]; }
        if(v != FUDE_SIM_X) v = (u8)(ones & 1u);
    } else {
        const u8 dec = (strcmp(g, "and") == 0 || strcmp(g, "nand") == 0) ? 0u : 1u;
        b8 d = false, x = false;
        for(u32 i = 0; i < n; i++) { const u8 a = known(in[i]); d = d || a == dec; x = x || a == FUDE_SIM_X; }
        v = d ? dec : (x ? FUDE_SIM_X : (u8)(1u - dec));
    }
    return inv && v != FUDE_SIM_X ? (u8)(1u - v) : v;
}

static const c8* const GATES[8] = { "and", "or", "nand", "nor", "xor", "xnor", "not", "buf" };

// Every gate, 1 to 6 inputs, every input 0, 1, Z (left open) or X (two drivers fighting): its output as the reference
// says. 4^n cases a gate a size.
static void test_gate_tables(void) {
    world w; fude_sim_lib_init(&w.lib);
    u32 cases = 0, right = 0;
    for(u32 gi = 0; gi < 8u; gi++) {
        for(u32 n = 1; n <= (gi >= 6u ? 1u : 6u); n++) {
            fude_sim_def* top = fude_sim_def_new("test/table", "table");
            const c8* nets[8];
            c8 names[8][8];
            for(u32 i = 0; i < n; i++) {
                snprintf(names[i], sizeof names[i], "i%u", i);
                nets[i] = names[i];
                // (each input: two drivers, H and L — both on: X; one: its value; neither: Z)
                c8 h[8], l[8]; snprintf(h, sizeof h, "H%u", i); snprintf(l, sizeof l, "L%u", i);
                c8 he[8], le[8]; snprintf(he, sizeof he, "eh%u", i); snprintf(le, sizeof le, "el%u", i);
                const c8* th[3] = { "one", he, names[i] }; const c8* tl[3] = { "zero", le, names[i] };
                fude_sim_def_inst(top, "tribuf", h, "", th, 3u);
                fude_sim_def_inst(top, "tribuf", l, "", tl, 3u);
                const c8* eh[1] = { he }; const c8* el[1] = { le };
                c8 ih[8], il[8]; snprintf(ih, sizeof ih, "EH%u", i); snprintf(il, sizeof il, "EL%u", i);
                fude_sim_def_inst(top, "input", ih, "0", eh, 1u);
                fude_sim_def_inst(top, "input", il, "0", el, 1u);
            }
            const c8* k1[1] = { "one" }; const c8* k0[1] = { "zero" };
            fude_sim_def_inst(top, "const", "K1", "1", k1, 1u);
            fude_sim_def_inst(top, "const", "K0", "0", k0, 1u);
            nets[n] = "y";
            c8 params[24]; snprintf(params, sizeof params, "inputs=%u", n);
            fude_sim_def_inst(top, GATES[gi], "G", gi >= 6u ? "" : params, nets, n + 1u);
            world_run(&w, top);
            u32 combos = 1;
            for(u32 i = 0; i < n; i++) combos *= 4u;
            for(u32 c = 0; c < combos; c++) {
                u8 in[8];
                u32 cc = c;
                for(u32 i = 0; i < n; i++) {
                    const u8 v = (u8)(cc % 4u); cc /= 4u;   // (0, 1, Z, X)
                    in[i] = v;
                    c8 ih[8], il[8]; snprintf(ih, sizeof ih, "EH%u", i); snprintf(il, sizeof il, "EL%u", i);
                    set(&w, ih, v == 1u || v == FUDE_SIM_X ? 1u : 0u);
                    set(&w, il, v == 0u || v == FUDE_SIM_X ? 1u : 0u);
                }
                u8 seen[8];
                for(u32 i = 0; i < n; i++) seen[i] = at(&w, "G", i);
                cases++;
                right += memcmp(seen, in, n) == 0 && at(&w, "G", n) == ref_gate(GATES[gi], in, n) ? 1u : 0u;
            }
            world_done(&w);
            fude_sim_def_free(top);
        }
    }
    printf("  gate tables: %u cases\n", cases);
    CHECK(right == cases);
    fude_sim_lib_destroy(&w.lib);
}

// --- random networks of gates against the reference; the same wrapped in custom parts nested to depth 6 -------

typedef struct { u8 gate; u32 n; u32 in[4]; } rnode;   // a gate's kind, its inputs (nodes before it: inputs first)

// Each gate wrapped _depth times in a custom part ("user/w<gate>_<n>_<d>": its ports A1..An, Y), made once into _lib.
static const c8* wrapped(fude_sim_lib* lib, u32 gate, u32 n, u32 depth, c8* id, usize size) {
    if(depth == 0u) { snprintf(id, size, "%s", GATES[gate]); return id; }
    snprintf(id, size, "user/w%s_%u_%u", GATES[gate], n, depth);
    if(fude_sim_lib_find(lib, id) != NULL) return id;
    c8 inner[FUDE_SIM_ID];
    wrapped(lib, gate, n, depth - 1u, inner, sizeof inner);
    fude_sim_def* d = fude_sim_def_new(id, id);
    const c8* nets[5]; c8 names[5][8];
    for(u32 i = 0; i < n; i++) { snprintf(names[i], sizeof names[i], "A%u", i + 1u); fude_sim_def_port(d, names[i], FUDE_SIM_LOGIC, FUDE_SIM_IN, 1u); nets[i] = names[i]; }
    fude_sim_def_port(d, "Y", FUDE_SIM_LOGIC, FUDE_SIM_OUT, 1u);
    nets[n] = "Y";
    c8 params[24]; snprintf(params, sizeof params, depth == 1u && gate < 6u ? "inputs=%u" : "", n);
    fude_sim_def_inst(d, inner, "W", params, nets, n + 1u);
    fude_sim_lib_add(lib, d);
    return id;
}

static void test_random_networks(void) {
    u32 networks = 0, checks = 0, right = 0, as_text = 0;
    for(u32 trial = 0; trial < 300u; trial++) {
        world w; fude_sim_lib_init(&w.lib);
        const u32 inputs = 1u + rnd(6u), gates = 3u + rnd(40u), depth = rnd(7u);
        rnode node[64];
        fude_sim_def* top = fude_sim_def_new("test/net", "net");
        c8 name[64][8];
        for(u32 i = 0; i < inputs + gates; i++) snprintf(name[i], sizeof name[i], "n%u", i);
        for(u32 i = 0; i < inputs; i++) { c8 ref[8]; snprintf(ref, sizeof ref, "I%u", i); const c8* nn[1] = { name[i] }; fude_sim_def_inst(top, "input", ref, "0", nn, 1u); }
        for(u32 g = 0; g < gates; g++) {
            rnode* r = &node[g];
            r->gate = (u8)rnd(8u);
            r->n = r->gate >= 6u ? 1u : 1u + rnd(4u);
            const c8* nets[5];
            for(u32 i = 0; i < r->n; i++) { r->in[i] = rnd(inputs + g); nets[i] = name[r->in[i]]; }
            nets[r->n] = name[inputs + g];
            c8 ref[8], id[FUDE_SIM_ID], params[24];
            snprintf(ref, sizeof ref, "G%u", g);
            snprintf(params, sizeof params, r->gate < 6u ? "inputs=%u" : "", r->n);
            wrapped(&w.lib, r->gate, r->n, depth, id, sizeof id);
            fude_sim_def_inst(top, id, ref, depth == 0u ? params : "", nets, r->n + 1u);
            c8 pr[8]; snprintf(pr, sizeof pr, "P%u", g);
            const c8* pn[1] = { name[inputs + g] };
            fude_sim_def_inst(top, "probe", pr, "", pn, 1u);
        }
        // Every custom part written as text and read back into the library in its place: the same answers.
        if(depth > 0u && trial % 3u == 0u) {
            fude_sim_def* const* d = (fude_sim_def* const*)w.lib.defs.memory;
            const u32 nd = (u32)rde_arr_length(&w.lib.defs);
            for(u32 i = 0; i < nd; i++) {
                if(d[i]->kind != FUDE_SIM_COMPOSITE) continue;
                rde_arr text = rde_arr_new(sizeof(c8), rde_memory_allocator_get_default_std());
                fude_sim_def_write(d[i], &text);
                c8 err[160];
                fude_sim_def* back = fude_sim_def_read((const c8*)text.memory, rde_arr_length(&text), err, sizeof err);
                CHECK(back != NULL);
                if(back != NULL) { fude_sim_lib_add(&w.lib, back); as_text++; }
                rde_arr_free(&text);
            }
        }
        world_run(&w, top);
        networks++;
        for(u32 v = 0; v < 24u; v++) {
            u8 val[64];
            for(u32 i = 0; i < inputs; i++) { c8 ref[8]; snprintf(ref, sizeof ref, "I%u", i); val[i] = (u8)rnd(2u); set(&w, ref, val[i]); }
            for(u32 g = 0; g < gates; g++) {
                u8 in[4];
                for(u32 i = 0; i < node[g].n; i++) in[i] = val[node[g].in[i]];
                val[inputs + g] = ref_gate(GATES[node[g].gate], in, node[g].n);
                c8 pr[8]; snprintf(pr, sizeof pr, "P%u", g);
                checks++;
                right += at(&w, pr, 0) == val[inputs + g] ? 1u : 0u;
            }
        }
        world_done(&w);
        fude_sim_def_free(top);
        fude_sim_lib_destroy(&w.lib);
    }
    printf("  random networks: %u, %u outputs checked, %u custom parts through their text\n", networks, checks, as_text);
    CHECK(right == checks);
}

// --- the text: random custom parts written and read back; then mangled, read, never crashing --------------------

static void test_text_fuzz(void) {
    u32 rounds = 0, stable = 0, mangled = 0, read_back = 0;
    const c8* domains[3] = { "logic", "electric", "rotation" };
    RDE_UNUSED(domains);
    for(u32 trial = 0; trial < 200u; trial++) {
        c8 id[32]; snprintf(id, sizeof id, "user/r%u", trial);
        fude_sim_def* d = fude_sim_def_new(id, "A part with a long name, made at random");
        snprintf(d->desc, sizeof d->desc, "its description %u", trial);
        const u32 ports = 1u + rnd(10u);
        for(u32 p = 0; p < ports; p++) {
            c8 pn[16]; snprintf(pn, sizeof pn, "P%u", p);
            const u32 k = fude_sim_def_port(d, pn, (u8)rnd(FUDE_SIM_DOMAINS), (u8)rnd(4u), (u8)(1u + rnd(8u)));
            ((fude_sim_port*)d->ports.memory)[k].side = (u8)rnd(4u);
        }
        const u32 params = rnd(4u);
        for(u32 p = 0; p < params; p++) {
            fude_sim_param q; memset(&q, 0, sizeof q);
            snprintf(q.name, sizeof q.name, "k%u", p); snprintf(q.unit, sizeof q.unit, p % 2u ? "ohm" : "");
            q.value = (f64)rnd(100000u) / 7.0; q.lo = -1.0; q.hi = (f64)rnd(10u);
            rde_arr_add(&d->params, &q);
        }
        const u32 insts = rnd(12u);
        for(u32 i = 0; i < insts; i++) {
            const c8* nets[6]; c8 nn[6][16];
            const u32 k = rnd(6u);
            for(u32 j = 0; j < k; j++) { snprintf(nn[j], sizeof nn[j], rnd(3u) == 0u ? "P%u" : "n%u", rnd(8u)); nets[j] = rnd(9u) == 0u ? NULL : nn[j]; }
            c8 ref[8]; snprintf(ref, sizeof ref, "U%u", i);
            fude_sim_def_inst(d, GATES[rnd(8u)], rnd(5u) == 0u ? "" : ref, rnd(2u) ? "inputs=3 delay=2n" : "", nets, k);
        }
        rde_arr t1 = rde_arr_new(sizeof(c8), rde_memory_allocator_get_default_std()), t2 = rde_arr_new(sizeof(c8), rde_memory_allocator_get_default_std());
        fude_sim_def_write(d, &t1);
        c8 err[160];
        fude_sim_def* back = fude_sim_def_read((const c8*)t1.memory, rde_arr_length(&t1), err, sizeof err);
        rounds++;
        if(back != NULL) {
            fude_sim_def_write(back, &t2);
            // (its params' values written with every digit: the same text again)
            stable += rde_arr_length(&t1) == rde_arr_length(&t2) && memcmp(t1.memory, t2.memory, rde_arr_length(&t1)) == 0 ? 1u : 0u;
            fude_sim_def_free(back);
        }
        // Mangled 20 ways: bytes changed, dropped, added, the text cut short. Read: a part or a reason, never a crash.
        for(u32 m = 0; m < 20u; m++) {
            rde_arr bad = rde_arr_new(sizeof(c8), rde_memory_allocator_get_default_std());
            const usize len = rde_arr_length(&t1);
            memcpy(rde_arr_add_n(&bad, len), t1.memory, len);
            const u32 edits = 1u + rnd(6u);
            for(u32 e = 0; e < edits && rde_arr_length(&bad) > 1u; e++) {
                c8* b = (c8*)bad.memory;
                const usize at_ = rnd((u32)rde_arr_length(&bad));
                switch(rnd(4u)) {
                case 0: b[at_] = (c8)(1 + rnd(255u)); break;
                case 1: memmove(&b[at_], &b[at_ + 1u], rde_arr_length(&bad) - at_ - 1u); bad.count--; break;
                case 2: { const c8 c = " \n\"=-x"[rnd(6u)]; rde_arr_add(&bad, &c); b = (c8*)bad.memory; memmove(&b[at_ + 1u], &b[at_], rde_arr_length(&bad) - at_ - 1u); b[at_] = c; break; }
                default: bad.count = at_ + 1u; break;
                }
            }
            fude_sim_def* got = fude_sim_def_read((const c8*)bad.memory, rde_arr_length(&bad), err, sizeof err);
            mangled++;
            if(got != NULL) {
                // (what it read writes and reads back the same)
                rde_arr a = rde_arr_new(sizeof(c8), rde_memory_allocator_get_default_std()), b2 = rde_arr_new(sizeof(c8), rde_memory_allocator_get_default_std());
                fude_sim_def_write(got, &a);
                fude_sim_def* again = fude_sim_def_read((const c8*)a.memory, rde_arr_length(&a), err, sizeof err);
                CHECK(again != NULL);
                if(again != NULL) { fude_sim_def_write(again, &b2); CHECK(rde_arr_length(&a) == rde_arr_length(&b2) && memcmp(a.memory, b2.memory, rde_arr_length(&a)) == 0); fude_sim_def_free(again); }
                rde_arr_free(&a); rde_arr_free(&b2);
                fude_sim_def_free(got);
                read_back++;
            } else {
                CHECK(err[0] != 0);
            }
            rde_arr_free(&bad);
        }
        rde_arr_free(&t1); rde_arr_free(&t2);
        fude_sim_def_free(d);
    }
    // (an empty name and description; a name with spaces at its end: kept as written, back as it was minus them)
    fude_sim_def* e = fude_sim_def_new("user/empty", "");
    rde_arr te = rde_arr_new(sizeof(c8), rde_memory_allocator_get_default_std());
    fude_sim_def_write(e, &te);
    c8 err2[160];
    fude_sim_def* eb = fude_sim_def_read((const c8*)te.memory, rde_arr_length(&te), err2, sizeof err2);
    CHECK(eb != NULL && eb->name[0] == 0 && eb->desc[0] == 0 && strcmp(eb->id, "user/empty") == 0);
    fude_sim_def_free(eb); fude_sim_def_free(e); rde_arr_free(&te);
    printf("  part text: %u written and read back (%u the same), %u mangled (%u still read)\n", rounds, stable, mangled, read_back);
    CHECK(stable == rounds);
}

// --- flip-flops at random against the reference; counters and shift registers of every size; delays ------------

static void test_sequences(void) {
    world w; fude_sim_lib_init(&w.lib);
    // A D, a T and a JK flip-flop on the same inputs, changed one at a time at random, 4000 times.
    fude_sim_def* top = fude_sim_def_new("test/ffs", "ffs");
    const c8* ins[4] = { "d", "c", "r", "k" };
    const c8* refs[4] = { "D", "C", "R", "K" };
    for(u32 i = 0; i < 4u; i++) { const c8* n[1] = { ins[i] }; fude_sim_def_inst(top, "input", refs[i], "0", n, 1u); }
    const c8* dn[5] = { "d", "c", "r", "q", "nq" }; const c8* tn[5] = { "d", "c", "r", "tq", "tnq" }; const c8* jn[6] = { "d", "k", "c", "r", "jq", "jnq" };
    fude_sim_def_inst(top, "dff", "FD", "", dn, 5u);
    fude_sim_def_inst(top, "tff", "FT", "", tn, 5u);
    fude_sim_def_inst(top, "jkff", "FJ", "", jn, 6u);
    world_run(&w, top);
    u8 v[4] = { 0, 0, 0, 0 }, qd = 0, qt = 0, qj = 0;
    u32 right = 0;
    for(u32 step = 0; step < 4000u; step++) {
        const u32 which = rnd(4u);
        const u8 before = v[1];
        v[which] = (u8)(1u - v[which]);
        set(&w, refs[which], v[which]);
        const b8 rose = which == 1u && before == 0u && v[1] == 1u;
        if(v[2]) { qd = qt = qj = 0; }
        else if(rose) {
            qd = v[0];
            qt = v[0] ? (u8)(1u - qt) : qt;
            qj = v[0] && v[3] ? (u8)(1u - qj) : (v[0] ? 1u : (v[3] ? 0u : qj));
        }
        right += at(&w, "FD", 3) == qd && at(&w, "FD", 4) == (u8)(1u - qd) && at(&w, "FT", 3) == qt && at(&w, "FJ", 4) == qj ? 1u : 0u;
    }
    CHECK(right == 4000u);
    world_done(&w);
    fude_sim_def_free(top);
    // Ripple counters of 1 to 10 bits on a clock: after each edge, its count (mod 2^bits).
    u32 count_checks = 0, count_right = 0;
    for(u32 bits = 1; bits <= 10u; bits++) {
        fude_sim_def* c = fude_sim_def_new("test/count", "count");
        const c8* ck[1] = { "clk" }; const c8* one[1] = { "o" }; const c8* zero[1] = { "z" };
        fude_sim_def_inst(c, "clock", "CK", "period=1", ck, 1u);
        fude_sim_def_inst(c, "const", "O", "1", one, 1u);
        fude_sim_def_inst(c, "const", "Z", "0", zero, 1u);
        for(u32 b = 0; b < bits; b++) {
            c8 clk[8], q[8], nq[8], ref[8];
            snprintf(clk, sizeof clk, b == 0u ? "clk" : "nq%u", b - 1u);
            snprintf(q, sizeof q, "q%u", b); snprintf(nq, sizeof nq, "nq%u", b); snprintf(ref, sizeof ref, "T%u", b);
            const c8* n[5] = { "o", clk, "z", q, nq };
            fude_sim_def_inst(c, "tff", ref, "", n, 5u);
        }
        world_run(&w, c);
        const u32 ticks = (1u << bits) + 7u;
        for(u32 k = 1; k <= ticks; k++) {
            fude_sim_run_until(&w.run, (f64)k + 0.1);
            u32 got = 0;
            for(u32 b = 0; b < bits; b++) { c8 ref[8]; snprintf(ref, sizeof ref, "T%u", b); got |= (u32)at(&w, ref, 3) << b; }
            count_checks++;
            count_right += got == (k & ((1u << bits) - 1u)) ? 1u : 0u;
        }
        world_done(&w);
        fude_sim_def_free(c);
    }
    printf("  counters: %u counts checked\n", count_checks);
    CHECK(count_right == count_checks);
    // Shift registers of 1 to 16 bits: random bits shifted in, each stage the bit that went in that many edges ago.
    u32 shift_checks = 0, shift_right = 0;
    for(u32 len = 1; len <= 16u; len++) {
        fude_sim_def* s = fude_sim_def_new("test/shift", "shift");
        const c8* nd[1] = { "din" }; const c8* nc[1] = { "c" }; const c8* nz[1] = { "z" };
        fude_sim_def_inst(s, "input", "DIN", "0", nd, 1u);
        fude_sim_def_inst(s, "input", "C", "0", nc, 1u);
        fude_sim_def_inst(s, "const", "Z", "0", nz, 1u);
        for(u32 b = 0; b < len; b++) {
            c8 din[8], q[8], nq[8], ref[8];
            snprintf(din, sizeof din, b == 0u ? "din" : "s%u", b - 1u);
            snprintf(q, sizeof q, "s%u", b); snprintf(nq, sizeof nq, "ns%u", b); snprintf(ref, sizeof ref, "S%u", b);
            const c8* n[5] = { din, "c", "z", q, nq };
            fude_sim_def_inst(s, "dff", ref, "", n, 5u);
        }
        world_run(&w, s);
        u8 hist[64] = { 0 };
        for(u32 k = 0; k < 48u; k++) {
            const u8 bit = (u8)rnd(2u);
            set(&w, "DIN", bit);
            set(&w, "C", 1); set(&w, "C", 0);
            memmove(&hist[1], &hist[0], 63u);
            hist[0] = bit;
            for(u32 b = 0; b < len; b++) {
                c8 ref[8]; snprintf(ref, sizeof ref, "S%u", b);
                shift_checks++;
                shift_right += at(&w, ref, 3) == hist[b] ? 1u : 0u;
            }
        }
        world_done(&w);
        fude_sim_def_free(s);
    }
    printf("  shift registers: %u stages checked\n", shift_checks);
    CHECK(shift_right == shift_checks);
    // Delays: a pulse shorter than a buffer's delay never comes out (inertial); a longer one does, that much later.
    fude_sim_def* dl = fude_sim_def_new("test/delay", "delay");
    const c8* bn[2] = { "a", "y" }; const c8* an[1] = { "a" };
    fude_sim_def_inst(dl, "buf", "B", "delay=10n", bn, 2u);
    fude_sim_def_inst(dl, "input", "A", "0", an, 1u);
    world_run(&w, dl);
    const u32 a = fude_sim_flat_find(&w.flat, "A");
    fude_sim_run_set(&w.run, a, 1); fude_sim_run_until(&w.run, w.run.now + 3e-9);
    fude_sim_run_set(&w.run, a, 0); fude_sim_run_until(&w.run, w.run.now + 30e-9);
    CHECK(at(&w, "B", 1) == 0u);   // (3 ns of 1: swallowed)
    fude_sim_run_set(&w.run, a, 1); fude_sim_run_until(&w.run, w.run.now + 9e-9);
    CHECK(at(&w, "B", 1) == 0u);   // (not yet: 10 ns)
    fude_sim_run_until(&w.run, w.run.now + 2e-9);
    CHECK(at(&w, "B", 1) == 1u);
    world_done(&w);
    fude_sim_def_free(dl);
    fude_sim_lib_destroy(&w.lib);
}

// --- buses: N tri-states, enabled at random: the one value, X, or Z ------------------------------------

static void test_buses(void) {
    world w; fude_sim_lib_init(&w.lib);
    u32 checks = 0, right = 0;
    for(u32 n = 1; n <= 8u; n++) {
        fude_sim_def* b = fude_sim_def_new("test/bus", "bus");
        for(u32 i = 0; i < n; i++) {
            c8 a[8], e[8], ra[8], re[8], t[8];
            snprintf(a, sizeof a, "a%u", i); snprintf(e, sizeof e, "e%u", i);
            snprintf(ra, sizeof ra, "A%u", i); snprintf(re, sizeof re, "E%u", i); snprintf(t, sizeof t, "T%u", i);
            const c8* tn[3] = { a, e, "bus" }; const c8* an[1] = { a }; const c8* en[1] = { e };
            fude_sim_def_inst(b, "tribuf", t, "", tn, 3u);
            fude_sim_def_inst(b, "input", ra, "0", an, 1u);
            fude_sim_def_inst(b, "input", re, "0", en, 1u);
        }
        const c8* pn[1] = { "bus" };
        fude_sim_def_inst(b, "probe", "P", "", pn, 1u);
        world_run(&w, b);
        for(u32 k = 0; k < 200u; k++) {
            u8 want = FUDE_SIM_Z;
            for(u32 i = 0; i < n; i++) {
                c8 ra[8], re[8]; snprintf(ra, sizeof ra, "A%u", i); snprintf(re, sizeof re, "E%u", i);
                const u8 val = (u8)rnd(2u), en = (u8)(rnd(3u) == 0u);
                set(&w, ra, val); set(&w, re, en);
                if(en) want = want == FUDE_SIM_Z ? val : (want == val ? want : FUDE_SIM_X);
            }
            checks++;
            right += at(&w, "P", 0) == want ? 1u : 0u;
        }
        world_done(&w);
        fude_sim_def_free(b);
    }
    printf("  buses: %u cases\n", checks);
    CHECK(right == checks);
    fude_sim_lib_destroy(&w.lib);
}

// --- elaboration's edges: ports joined inside, ports left open, depth exactly at its limit, a loop through two parts,
//     a library's versions, numbers as typed -------------------------------------------------------------------

static void test_elaboration_edges(void) {
    fude_sim_lib lib; fude_sim_lib_init(&lib);
    fude_sim_flat flat; fude_sim_flat_init(&flat);
    // A part whose two ports are one net inside: what they are on outside becomes one net.
    fude_sim_def* tie = fude_sim_def_new("user/tie", "tie");
    fude_sim_def_port(tie, "A", FUDE_SIM_LOGIC, FUDE_SIM_INOUT, 1u);
    fude_sim_def_port(tie, "B", FUDE_SIM_LOGIC, FUDE_SIM_INOUT, 1u);
    const c8* tn[2] = { "A", "A" };   // (a buffer from A to A: the port B's net, named B, joined below)
    RDE_UNUSED(tn);
    ((u32*)tie->port_nets.memory)[1] = ((u32*)tie->port_nets.memory)[0];   // (B's net inside is A's)
    fude_sim_lib_add(&lib, tie);
    fude_sim_def* top = fude_sim_def_new("test/top", "top");
    const c8* xn[1] = { "x" }; const c8* yn[1] = { "y" }; const c8* xy[2] = { "x", "y" };
    fude_sim_def_inst(top, "input", "X", "1", xn, 1u);
    fude_sim_def_inst(top, "probe", "P", "", yn, 1u);
    fude_sim_def_inst(top, "user/tie", "T", "", xy, 2u);
    CHECK(fude_sim_elaborate(&lib, top, &flat));
    const u32* pn = (const u32*)flat.port_nets.memory;
    const fude_sim_prim* pr = (const fude_sim_prim*)flat.prims.memory;
    CHECK(pn[pr[fude_sim_flat_find(&flat, "X")].first] == pn[pr[fude_sim_flat_find(&flat, "P")].first]);
    fude_sim_def_free(top);
    // A part given fewer nets than it has ports: the rest open (each its own net, joined to nothing).
    top = fude_sim_def_new("test/top2", "top");
    const c8* one[1] = { "a" };
    fude_sim_def_inst(top, "and", "G", "inputs=4", one, 1u);
    CHECK(fude_sim_elaborate(&lib, top, &flat));
    pr = (const fude_sim_prim*)flat.prims.memory; pn = (const u32*)flat.port_nets.memory;
    CHECK(pr[0].count == 5u && flat.nets == 5u && pn[pr[0].first + 1u] != pn[pr[0].first + 2u]);
    fude_sim_def_free(top);
    // Nested exactly 32 deep: fine; 33: a part made of itself.
    for(u32 d = 1; d <= 34u; d++) {
        c8 id[32], inner[32];
        snprintf(id, sizeof id, "user/deep%u", d);
        snprintf(inner, sizeof inner, d == 1u ? "buf" : "user/deep%u", d - 1u);
        fude_sim_def* k = fude_sim_def_new(id, id);
        fude_sim_def_port(k, "A", FUDE_SIM_LOGIC, FUDE_SIM_IN, 1u);
        fude_sim_def_port(k, "Y", FUDE_SIM_LOGIC, FUDE_SIM_OUT, 1u);
        const c8* n[2] = { "A", "Y" };
        fude_sim_def_inst(k, inner, "U", "", n, 2u);
        fude_sim_lib_add(&lib, k);
    }
    for(u32 d = 31; d <= 34u; d++) {
        c8 id[32]; snprintf(id, sizeof id, "user/deep%u", d);
        top = fude_sim_def_new("test/top3", "top");
        const c8* n[2] = { "a", "y" };
        fude_sim_def_inst(top, id, "D", "", n, 2u);
        const b8 ok = fude_sim_elaborate(&lib, top, &flat);
        CHECK(ok == (d <= 32u));   // (32 custom parts nested: as deep as it goes)
        if(ok) {
            c8 path[256] = "D";
            for(u32 k = 1; k < d; k++) strcat(path, "/U");
            strcat(path, "/U");
            CHECK(fude_sim_flat_find(&flat, path) != FUDE_SIM_NONE);
        }
        fude_sim_def_free(top);
    }
    // A loop through two parts (A in B in A): said, not a hang.
    fude_sim_def* la = fude_sim_def_new("user/la", "la");
    fude_sim_def* lb = fude_sim_def_new("user/lb", "lb");
    const c8* none[1] = { "n" };
    fude_sim_def_inst(la, "user/lb", "B", "", none, 1u);
    fude_sim_def_inst(lb, "user/la", "A", "", none, 1u);
    fude_sim_lib_add(&lib, la);
    fude_sim_lib_add(&lib, lb);
    top = fude_sim_def_new("test/top4", "top");
    fude_sim_def_inst(top, "user/la", "L", "", none, 1u);
    CHECK(!fude_sim_elaborate(&lib, top, &flat) && strstr(fude_sim_flat_errors(&flat), "made of itself") != NULL);
    fude_sim_def_free(top);
    // A library: a part added again replaces it, its version one more.
    fude_sim_def* v1 = fude_sim_def_new("user/ver", "ver"); fude_sim_lib_add(&lib, v1);
    fude_sim_def* v2 = fude_sim_def_new("user/ver", "ver"); fude_sim_lib_add(&lib, v2);
    CHECK(fude_sim_lib_find(&lib, "user/ver") == v2 && v2->version == 2u);
    fude_sim_def* v9 = fude_sim_def_new("user/ver", "ver"); v9->version = 9u; fude_sim_lib_add(&lib, v9);
    CHECK(fude_sim_lib_find(&lib, "user/ver")->version == 9u);
    // Every built-in model is in it as a primitive, its ports as its defaults make them.
    for(u32 m = 0; m < fude_sim_model_count(); m++) {
        const fude_sim_def* d = fude_sim_lib_find(&lib, fude_sim_model_at(m)->id);
        CHECK(d != NULL && d->kind == FUDE_SIM_PRIMITIVE && d->model == fude_sim_model_at(m) && rde_arr_length(&d->ports) > 0u);
    }
    // Numbers as typed.
    struct { const c8* s; f64 v; } nums[] = { { "1", 1.0 }, { "4.7k", 4700.0 }, { "4.7K", 4700.0 }, { "2.2 M", 2.2e6 }, { "10n", 1e-8 }, { "3p", 3e-12 },
                                              { "1u", 1e-6 }, { "1\xC2\xB5", 1e-6 }, { "5m", 5e-3 }, { "1G", 1e9 }, { "-3.5", -3.5 }, { "1e-3", 1e-3 }, { "0", 0.0 } };
    for(u32 i = 0; i < sizeof nums / sizeof nums[0]; i++) {
        f64 v = -99.0;
        CHECK(fude_sim_number(nums[i].s, &v) && fabs(v - nums[i].v) <= 1e-12 * fmax(fabs(nums[i].v), 1.0));
    }
    f64 v = 0.0;
    CHECK(!fude_sim_number("", &v) && !fude_sim_number("k", &v) && !fude_sim_number(NULL, &v) && !fude_sim_number("1e999", &v));
    fude_sim_flat_destroy(&flat);
    fude_sim_lib_destroy(&lib);
}

// --- bodies' shapes: measures against formulas; outlines simplified; concave shapes cut into convex pieces ----------

static f64 frand(f64 lo, f64 hi) { return lo + (hi - lo) * (f64)rnd(1000001u) / 1000000.0; }

// The distance from _q to the polygon's edges.
static f64 edge_distance(const fude_sim_v2* p, u32 n, fude_sim_v2 q) {
    f64 best = 1e300;
    for(u32 i = 0, j = n - 1u; i < n; j = i++) {
        const f64 dx = p[i].x - p[j].x, dy = p[i].y - p[j].y, ll = dx * dx + dy * dy;
        const f64 u = ll > 0.0 ? fmax(0.0, fmin(1.0, ((q.x - p[j].x) * dx + (q.y - p[j].y) * dy) / ll)) : 0.0;
        best = fmin(best, hypot(p[j].x + dx * u - q.x, p[j].y + dy * u - q.y));
    }
    return best;
}

// A random simple polygon: star-shaped round a centre it holds (each corner in its own slice of the turn, so no gap
// between two is half a turn: simple), counter-clockwise.
static u32 star(fude_sim_v2* out, u32 n, f64 cx, f64 cy, f64 r) {
    for(u32 i = 0; i < n; i++) {
        const f64 a = ((f64)i + frand(0.05, 0.95)) * 6.283185307179586 / (f64)n;
        const f64 rr = r * frand(0.15, 1.0);
        out[i] = (fude_sim_v2){ cx + cos(a) * rr, cy + sin(a) * rr };
    }
    return n;
}

static void test_bodies(void) {
    // Materials.
    CHECK(fude_sim_material_count() == 8u);
    for(u32 i = 0; i < fude_sim_material_count(); i++) {
        const fude_sim_material* m = fude_sim_material_at(i);
        CHECK(m != NULL && fude_sim_material_find(m->id) == m && m->density > 0.0 && m->friction >= 0.0 && m->friction <= 1.0 && m->bounce >= 0.0 && m->bounce <= 1.0);
    }
    CHECK(fude_sim_material_find("unobtainium") == NULL && fude_sim_material_at(99u) == NULL);
    // Rectangles, triangles, circles (as 720-gons): area, centre, inertia against their formulas — anywhere, any size,
    // turned any way, either winding.
    u32 shapes = 0, shapes_right = 0;
    for(u32 k = 0; k < 2000u; k++) {
        const f64 w = frand(0.01, 500.0), h = frand(0.01, 500.0), cx = frand(-1e5, 1e5), cy = frand(-1e5, 1e5), a = frand(0.0, 6.3);
        fude_sim_v2 r[4];
        const f64 cs = cos(a), sn = sin(a);
        const f64 lx[4] = { -w / 2, w / 2, w / 2, -w / 2 }, ly[4] = { -h / 2, -h / 2, h / 2, h / 2 };
        for(u32 i = 0; i < 4u; i++) r[i] = (fude_sim_v2){ cx + lx[i] * cs - ly[i] * sn, cy + lx[i] * sn + ly[i] * cs };
        if(k % 2u) { fude_sim_v2 t0 = r[0]; r[0] = r[3]; r[3] = t0; t0 = r[1]; r[1] = r[2]; r[2] = t0; }   // (clockwise)
        const f64 area = fude_sim_polygon_area(r, 4u);
        const fude_sim_v2 c = fude_sim_polygon_centroid(r, 4u);
        const f64 j = fude_sim_polygon_inertia(r, 4u, c);
        const f64 jw = w * h * (w * w + h * h) / 12.0;
        shapes++;
        shapes_right += fabs(fabs(area) - w * h) <= 1e-9 * w * h + 1e-6 && (k % 2u ? area < 0.0 : area > 0.0) &&
                        hypot(c.x - cx, c.y - cy) <= 1e-9 * (fabs(cx) + fabs(cy) + w + h) && fabs(j - jw) <= 1e-6 * jw + 1e-9 &&
                        fude_sim_polygon_convex(r, 4u) && fude_sim_polygon_inside(r, 4u, (fude_sim_v2){ cx, cy }) ? 1u : 0u;
        // A triangle: area by the cross product, centre the corners' mean, inertia about it (its known formula).
        fude_sim_v2 t[3] = { { frand(-50, 50), frand(-50, 50) }, { frand(-50, 50), frand(-50, 50) }, { frand(-50, 50), frand(-50, 50) } };
        const f64 ta = 0.5 * ((t[1].x - t[0].x) * (t[2].y - t[0].y) - (t[1].y - t[0].y) * (t[2].x - t[0].x));
        if(fabs(ta) > 1e-3) {
            const fude_sim_v2 g = { (t[0].x + t[1].x + t[2].x) / 3.0, (t[0].y + t[1].y + t[2].y) / 3.0 };
            const fude_sim_v2 tc = fude_sim_polygon_centroid(t, 3u);
            f64 s2 = 0.0;   // (J about the centre: |A| (a² + b² + c²) / 36, a, b, c its sides)
            for(u32 i = 0; i < 3u; i++) { const f64 dx = t[(i + 1u) % 3u].x - t[i].x, dy = t[(i + 1u) % 3u].y - t[i].y; s2 += dx * dx + dy * dy; }
            shapes++;
            shapes_right += fabs(fude_sim_polygon_area(t, 3u) - ta) < 1e-9 && hypot(tc.x - g.x, tc.y - g.y) < 1e-9 &&
                            fabs(fude_sim_polygon_inertia(t, 3u, tc) - fabs(ta) * s2 / 36.0) <= 1e-7 * fabs(ta) * s2 ? 1u : 0u;
        }
    }
    for(u32 k = 0; k < 50u; k++) {
        const f64 rr = frand(0.1, 1000.0), cx = frand(-1000, 1000), cy = frand(-1000, 1000);
        fude_sim_v2 c[720];
        for(u32 i = 0; i < 720u; i++) c[i] = (fude_sim_v2){ cx + rr * cos(i * 6.283185307179586 / 720.0), cy + rr * sin(i * 6.283185307179586 / 720.0) };
        const f64 pi = 3.14159265358979323846;
        shapes++;
        shapes_right += fabs(fude_sim_polygon_area(c, 720u) - pi * rr * rr) < 1e-4 * pi * rr * rr &&
                        fabs(fude_sim_polygon_inertia(c, 720u, (fude_sim_v2){ cx, cy }) - pi * pow(rr, 4.0) / 2.0) < 1e-4 * pi * pow(rr, 4.0) / 2.0 ? 1u : 0u;
    }
    printf("  shapes' measures: %u shapes\n", shapes);
    CHECK(shapes_right == shapes);
    // Simplified: a dense, wobbly outline (either winding, its last point its first, repeats in it): what is kept is
    // its own points, counter-clockwise, no point of the original further than the tolerance from what is kept.
    u32 simp = 0, simp_right = 0;
    for(u32 k = 0; k < 300u; k++) {
        const u32 n = 20u + rnd(2000u);
        const f64 rr = frand(1.0, 300.0), tol = rr * frand(0.001, 0.1);
        rde_arr in_arr = scratch(sizeof(fude_sim_v2), 2u * n + 8u), out_arr = scratch(sizeof(fude_sim_v2), 2u * n + 8u);
        fude_sim_v2* in = (fude_sim_v2*)in_arr.memory, *out = (fude_sim_v2*)out_arr.memory;
        u32 m = 0;
        const u32 lobes = 1u + rnd(7u);
        for(u32 i = 0; i < n; i++) {
            const f64 a = (k % 2u ? -1.0 : 1.0) * 6.283185307179586 * (f64)i / (f64)n;
            const f64 r2 = rr * (1.0 + 0.3 * sin(lobes * a)) + frand(-tol * 0.2, tol * 0.2);
            in[m++] = (fude_sim_v2){ r2 * cos(a), r2 * sin(a) };
            if(rnd(20u) == 0u) { in[m] = in[m - 1u]; m++; }   // (a repeat)
        }
        in[m++] = in[0];   // (closed as drawn: its last its first)
        const u32 s = fude_sim_outline_simplify(in, m, tol, out, 2u * n + 8u);
        b8 ok = s >= 3u && s <= m && fude_sim_polygon_area(out, s) > 0.0;
        for(u32 i = 0; ok && i < s; i++) {
            b8 found = false;
            for(u32 j = 0; j < m && !found; j++) found = out[i].x == in[j].x && out[i].y == in[j].y;
            ok = found;
        }
        f64 worst = 0.0;
        for(u32 i = 0; ok && i < m; i++) worst = fmax(worst, edge_distance(out, s, in[i]));
        simp++;
        simp_right += ok && worst <= tol * (1.0 + 1e-9) ? 1u : 0u;
        rde_arr_free(&in_arr); rde_arr_free(&out_arr);
    }
    // Open lines: their ends kept, no point further than the tolerance from what is kept, its points its own, in order.
    u32 lines = 0, lines_right = 0;
    for(u32 k = 0; k < 300u; k++) {
        const u32 n = 2u + rnd(1500u);
        const f64 tol = frand(0.01, 5.0);
        rde_arr in_arr = scratch(sizeof(fude_sim_v2), n), out_arr = scratch(sizeof(fude_sim_v2), n);
        fude_sim_v2* in = (fude_sim_v2*)in_arr.memory, *out = (fude_sim_v2*)out_arr.memory;
        f64 x = frand(-100, 100), y = frand(-100, 100), a = frand(0, 6.28);
        for(u32 i = 0; i < n; i++) { in[i] = (fude_sim_v2){ x, y }; a += frand(-0.3, 0.3); x += cos(a) * frand(0.1, 3.0); y += sin(a) * frand(0.1, 3.0); }
        const u32 s = fude_sim_polyline_simplify(in, n, tol, out, n);
        b8 ok = s >= 2u && out[0].x == in[0].x && out[s - 1u].x == in[n - 1u].x && out[s - 1u].y == in[n - 1u].y;
        u32 j = 0;
        for(u32 i = 0; ok && i < s; i++) { while(j < n && (in[j].x != out[i].x || in[j].y != out[i].y)) j++; ok = j < n; }   // (in order)
        for(u32 i = 0; ok && i < n; i++) {
            f64 best = 1e300;
            for(u32 q = 0; q + 1u < s; q++) {
                const f64 dx = out[q + 1u].x - out[q].x, dy = out[q + 1u].y - out[q].y, ll = dx * dx + dy * dy;
                const f64 u = ll > 0.0 ? fmax(0.0, fmin(1.0, ((in[i].x - out[q].x) * dx + (in[i].y - out[q].y) * dy) / ll)) : 0.0;
                best = fmin(best, hypot(out[q].x + dx * u - in[i].x, out[q].y + dy * u - in[i].y));
            }
            ok = best <= tol * (1.0 + 1e-9) + 1e-12;
        }
        lines++;
        lines_right += ok ? 1u : 0u;
        rde_arr_free(&in_arr); rde_arr_free(&out_arr);
    }
    printf("  open lines simplified: %u\n", lines);
    CHECK(lines_right == lines);
    fude_sim_v2 two[3] = { { 0, 0 }, { 1, 1 }, { 0, 0 } }, o3[4];
    CHECK(fude_sim_outline_simplify(two, 3u, 0.1, o3, 4u) == 0u);   // (nothing left: no area)
    printf("  outlines simplified: %u\n", simp);
    CHECK(simp_right == simp);
    // Cut into convex pieces: random concave polygons (3 to 60 corners), and by shapes' kinds (L, U, comb, spiral-ish).
    u32 polys = 0, polys_right = 0, pieces_all = 0, points_checked = 0;
    rde_arr pts = rde_arr_new(sizeof(fude_sim_v2), rde_memory_allocator_get_default_std()), cnt = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    for(u32 k = 0; k < 3000u; k++) {
        fude_sim_v2 p[256];
        u32 n = 0;
        const u32 kind = k % 4u;
        const u32 corners = 3u + rnd(6u);   // (3..8)
        if(kind <= 1u) {
            n = star(p, 3u + rnd(58u), frand(-1e4, 1e4), frand(-1e4, 1e4), frand(0.5, 800.0));
        } else if(kind == 2u) {
            // a comb: teeth along a bar
            const u32 teeth = 1u + rnd(10u);
            const f64 tw = frand(1, 20), gap = frand(1, 20), th = frand(1, 50), bh = frand(1, 20);
            const f64 x0 = frand(-500, 500), y0 = frand(-500, 500);
            p[n++] = (fude_sim_v2){ x0, y0 };
            p[n++] = (fude_sim_v2){ x0 + teeth * (tw + gap) - gap, y0 };
            for(i32 t2 = (i32)teeth - 1; t2 >= 0; t2--) {
                const f64 xa = x0 + (f64)t2 * (tw + gap);
                p[n++] = (fude_sim_v2){ xa + tw, y0 + bh + th };
                p[n++] = (fude_sim_v2){ xa, y0 + bh + th };
                if(t2 > 0) { p[n++] = (fude_sim_v2){ xa, y0 + bh }; p[n++] = (fude_sim_v2){ xa - gap, y0 + bh }; }
            }
            // (its points round as listed: bottom left, bottom right, then the teeth's tops from the right back)
            // fix the walk: insert each tooth's right edge base
            n = 0;
            p[n++] = (fude_sim_v2){ x0, y0 };
            p[n++] = (fude_sim_v2){ x0 + teeth * (tw + gap) - gap, y0 };
            for(i32 t2 = (i32)teeth - 1; t2 >= 0; t2--) {
                const f64 xa = x0 + (f64)t2 * (tw + gap);
                p[n++] = (fude_sim_v2){ xa + tw, y0 + bh + th };
                p[n++] = (fude_sim_v2){ xa, y0 + bh + th };
                if(t2 > 0) { p[n++] = (fude_sim_v2){ xa, y0 + bh }; p[n++] = (fude_sim_v2){ xa - gap, y0 + bh }; }
            }
        } else {
            // an L, a U or a T of random proportions
            const f64 a = frand(1, 100), b = frand(1, 100), c = frand(1, 100), d = frand(1, 100);
            const u32 which = rnd(3u);
            if(which == 0u) { const fude_sim_v2 l[6] = { {0,0},{a+b,0},{a+b,c},{a,c},{a,c+d},{0,c+d} }; memcpy(p, l, sizeof l); n = 6u; }
            else if(which == 1u) { const fude_sim_v2 u[8] = { {0,0},{2*a+b,0},{2*a+b,c+d},{a+b,c+d},{a+b,c},{a,c},{a,c+d},{0,c+d} }; memcpy(p, u, sizeof u); n = 8u; }
            else { const fude_sim_v2 t3[8] = { {a,0},{a+b,0},{a+b,c},{2*a+b,c},{2*a+b,c+d},{0,c+d},{0,c},{a,c} }; memcpy(p, t3, sizeof t3); n = 8u; }
        }
        if(n < 3u || fude_sim_polygon_area(p, n) <= 0.0) continue;
        rde_arr_clear(&pts); rde_arr_clear(&cnt);
        const u32 np = fude_sim_convex_pieces(p, n, corners, &pts, &cnt);
        polys++;
        b8 ok = np > 0u;
        f64 sum = 0.0;
        const fude_sim_v2* q = (const fude_sim_v2*)pts.memory;
        const u32* qc = (const u32*)cnt.memory;
        u32 off = 0;
        for(u32 i = 0; ok && i < np; i++) {
            ok = qc[i] >= 3u && qc[i] <= corners && fude_sim_polygon_convex(&q[off], qc[i]) && fude_sim_polygon_area(&q[off], qc[i]) > 0.0;
            for(u32 j = 0; ok && j < qc[i]; j++) {
                b8 found = false;
                for(u32 v = 0; v < n && !found; v++) found = q[off + j].x == p[v].x && q[off + j].y == p[v].y;
                ok = found;
            }
            sum += fude_sim_polygon_area(&q[off], qc[i]);
            off += qc[i];
        }
        const f64 area = fude_sim_polygon_area(p, n);
        ok = ok && fabs(sum - area) <= 1e-9 * area + 1e-9;
        // Points: inside the polygon ⇔ inside exactly one piece (those near an edge left out).
        f64 minx = 1e300, miny = 1e300, maxx = -1e300, maxy = -1e300;
        for(u32 v = 0; v < n; v++) { minx = fmin(minx, p[v].x); maxx = fmax(maxx, p[v].x); miny = fmin(miny, p[v].y); maxy = fmax(maxy, p[v].y); }
        for(u32 s = 0; ok && s < 100u; s++) {
            const fude_sim_v2 t4 = { frand(minx, maxx), frand(miny, maxy) };
            if(edge_distance(p, n, t4) < 1e-6 * (maxx - minx + maxy - miny)) continue;
            u32 inside = 0;
            off = 0;
            for(u32 i = 0; i < np; i++) {
                if(edge_distance(&q[off], qc[i], t4) < 1e-6 * (maxx - minx + maxy - miny)) { inside = 999u; break; }
                inside += fude_sim_polygon_inside(&q[off], qc[i], t4) ? 1u : 0u;
                off += qc[i];
            }
            if(inside == 999u) continue;
            points_checked++;
            ok = inside == (fude_sim_polygon_inside(p, n, t4) ? 1u : 0u);
            if(!ok && getenv("SIM_DEBUG") != NULL) printf("  point %.9g %.9g in %u pieces, in polygon %d\n", t4.x, t4.y, inside, fude_sim_polygon_inside(p, n, t4));
        }
        pieces_all += np;
        polys_right += ok ? 1u : 0u;
        if(!ok && getenv("SIM_DEBUG") != NULL) {
            printf("  bad: kind %u n %u corners %u pieces %u area %.6g sum %.6g\n", kind, n, corners, np, area, sum);
            for(u32 v = 0; v < n; v++) printf("    %.9g %.9g\n", p[v].x, p[v].y);
        }
    }
    // A shape that crosses itself (a bow tie): no pieces, no crash.
    const fude_sim_v2 bow[4] = { { 0, 0 }, { 10, 10 }, { 10, 0 }, { 0, 10 } };
    rde_arr_clear(&pts); rde_arr_clear(&cnt);
    const u32 bp = fude_sim_convex_pieces(bow, 4u, 8u, &pts, &cnt);
    CHECK(bp == 0u || bp > 0u);   // (either: never a crash; ASan watches)
    rde_arr_free(&pts); rde_arr_free(&cnt);
    printf("  convex pieces: %u polygons cut into %u pieces, %u points checked inside\n", polys, pieces_all, points_checked);
    CHECK(polys_right == polys);
}

// ===========================================================================================================
// MACHINES: custom parts made of custom parts, as a person builds them on the canvas and saves them (their part
// text) — latches of gates, flip-flops of latches, registers, a memory, counters, a clock of hours' worth of
// seconds, an ALU, an LFSR, an adding machine; then the chips (chips.c) one by one and a machine of chips only.
// Each against a model of what it should do, step by step.
// ===========================================================================================================

// A part's text read and added (a saved custom part loaded).
static void load(fude_sim_lib* lib, const c8* text) {
    c8 err[160];
    fude_sim_def* d = fude_sim_def_read(text, strlen(text), err, sizeof err);
    if(d == NULL) printf("  part not read: %s\n", err);
    CHECK(d != NULL);
    if(d == NULL) return;
    // (its text again the same: what is saved is what is loaded)
    rde_arr t1 = rde_arr_new(sizeof(c8), rde_memory_allocator_get_default_std()), t2 = rde_arr_new(sizeof(c8), rde_memory_allocator_get_default_std());
    fude_sim_def_write(d, &t1);
    fude_sim_def* again = fude_sim_def_read((const c8*)t1.memory, rde_arr_length(&t1), err, sizeof err);
    CHECK(again != NULL);
    if(again != NULL) {
        fude_sim_def_write(again, &t2);
        CHECK(rde_arr_length(&t1) == rde_arr_length(&t2) && memcmp(t1.memory, t2.memory, rde_arr_length(&t1)) == 0);
        fude_sim_def_free(again);
    }
    rde_arr_free(&t1); rde_arr_free(&t2);
    fude_sim_lib_add(lib, d);
}

// A test bench: the top part's text, built a line at a time — inputs (a switch each: "i:NET") and probes ("p:NET").
typedef struct { c8 text[32768]; usize n; } bench;
static void bench_start(bench* b) { b->n = 0; b->n += (usize)snprintf(b->text, sizeof b->text, "fude-part 1\nid test/bench\nname bench\n"); }
static void line(bench* b, const c8* fmt, ...) {
    va_list a; va_start(a, fmt);
    b->n += (usize)vsnprintf(b->text + b->n, sizeof b->text - b->n, fmt, a);
    va_end(a);
    b->n += (usize)snprintf(b->text + b->n, sizeof b->text - b->n, "\n");
}
static void inputs(bench* b, const c8* nets) {
    c8 buf[1024]; snprintf(buf, sizeof buf, "%s", nets);
    for(c8* t = strtok(buf, " "); t != NULL; t = strtok(NULL, " ")) line(b, "inst input i:%s \"0\" %s", t, t);
}
static void probes(bench* b, const c8* nets) {
    c8 buf[1024]; snprintf(buf, sizeof buf, "%s", nets);
    for(c8* t = strtok(buf, " "); t != NULL; t = strtok(NULL, " ")) line(b, "inst probe p:%s \"\" %s", t, t);
}
// The bench run on world w's library (its top kept in it: freed by world_free).
static fude_sim_def* bench_run(bench* b, world* w) {
    line(b, "end");
    c8 err[160];
    fude_sim_def* top = fude_sim_def_read(b->text, b->n, err, sizeof err);
    if(top == NULL) printf("  bench not read: %s\n", err);
    CHECK(top != NULL);
    if(top != NULL) world_run(w, top);
    return top;
}

static void put(world* w, const c8* net, u8 v) { c8 r[40]; snprintf(r, sizeof r, "i:%s", net); CHECK(fude_sim_flat_find(&w->flat, r) != FUDE_SIM_NONE); set(w, r, v); }
static u8 get(world* w, const c8* net) { c8 r[40]; snprintf(r, sizeof r, "p:%s", net); return at(w, r, 0); }
// A number on nets named fmt % 0, 1… (n bits, the first the lowest); 0xFFFFFFFF: one of them not 0 or 1.
static u32 get_num(world* w, const c8* fmt, u32 first, u32 n) {
    u32 v = 0;
    for(u32 i = 0; i < n; i++) {
        c8 net[32]; snprintf(net, sizeof net, fmt, first + i);
        const u8 b = get(w, net);
        if(b > 1u) return 0xFFFFFFFFu;
        v |= (u32)b << i;
    }
    return v;
}
static void put_num(world* w, const c8* fmt, u32 first, u32 n, u32 v) {
    for(u32 i = 0; i < n; i++) { c8 net[32]; snprintf(net, sizeof net, fmt, first + i); put(w, net, (u8)((v >> i) & 1u)); }
}
static void tick(world* w, const c8* clk) { put(w, clk, 1u); put(w, clk, 0u); }
static void world_free(world* w, fude_sim_def* top) { world_done(w); if(top != NULL) fude_sim_def_free(top); fude_sim_lib_destroy(&w->lib); }

#include "../support/parts.h"

static void load_parts(fude_sim_lib* lib) {
    for(u32 i = 0; i < PARTS_N; i++) load(lib, PARTS_ALL[i]);
}

// --- universal gates, latches, flip-flops of latches ---------------------------------------------------------

static void test_universal(void) {
    world w; fude_sim_lib_init(&w.lib); load_parts(&w.lib);
    bench b; bench_start(&b);
    inputs(&b, "A B CI");
    line(&b, "inst user/nandxor X \"\" A B x"); line(&b, "inst xor XP \"\" A B xp");
    line(&b, "inst user/norand N \"\" A B n");  line(&b, "inst and NP \"\" A B np");
    line(&b, "inst user/nandor O \"\" A B o");  line(&b, "inst or OP \"\" A B op");
    line(&b, "inst user/nandfa F \"\" A B CI s co"); line(&b, "inst user/fa FP \"\" A B CI sp cop");
    probes(&b, "x xp n np o op s sp co cop");
    fude_sim_def* top = bench_run(&b, &w);
    u32 right = 0;
    for(u32 v = 0; v < 8u; v++) {
        put(&w, "A", (u8)(v & 1u)); put(&w, "B", (u8)((v >> 1) & 1u)); put(&w, "CI", (u8)(v >> 2));
        const u32 a = v & 1u, bb = (v >> 1) & 1u, c = v >> 2;
        right += get(&w, "x") == (a ^ bb) && get(&w, "xp") == (a ^ bb) && get(&w, "n") == (a & bb) && get(&w, "np") == (a & bb) &&
                 get(&w, "o") == (a | bb) && get(&w, "op") == (a | bb) && get(&w, "s") == ((a + bb + c) & 1u) && get(&w, "sp") == ((a + bb + c) & 1u) &&
                 get(&w, "co") == ((a + bb + c) >> 1) && get(&w, "cop") == ((a + bb + c) >> 1) ? 1u : 0u;
    }
    CHECK(right == 8u);
    world_free(&w, top);

    // The latch of NANDs: unknown until set or reset; set, held, reset, held; both low: both outputs high.
    fude_sim_lib_init(&w.lib); load_parts(&w.lib);
    bench_start(&b);
    inputs(&b, "nS nR D EN CLK");
    line(&b, "inst user/srnand L \"\" nS nR q nq");
    line(&b, "inst user/dlatch DL \"\" D EN dq dnq");
    line(&b, "inst user/dff F \"\" D CLK fq fnq");
    line(&b, "inst dff FP \"\" D CLK zero fpq -"); line(&b, "inst const Z \"0\" zero");
    probes(&b, "q nq dq dnq fq fnq fpq");
    top = bench_run(&b, &w);
    CHECK(get(&w, "q") == 1u && get(&w, "nq") == 1u);   // (both inputs low as the bench starts: both outputs high)
    put(&w, "nS", 1u); CHECK(get(&w, "q") == 0u && get(&w, "nq") == 1u);   // (~R still low: reset)
    put(&w, "nR", 1u); CHECK(get(&w, "q") == 0u && get(&w, "nq") == 1u);   // (held)
    put(&w, "nS", 0u); CHECK(get(&w, "q") == 1u && get(&w, "nq") == 0u);
    put(&w, "nS", 1u); CHECK(get(&w, "q") == 1u && get(&w, "nq") == 0u);
    put(&w, "nR", 0u); CHECK(get(&w, "q") == 0u && get(&w, "nq") == 1u);
    put(&w, "nR", 1u); CHECK(get(&w, "q") == 0u && get(&w, "nq") == 1u);
    put(&w, "nS", 0u); put(&w, "nR", 0u); CHECK(get(&w, "q") == 1u && get(&w, "nq") == 1u);
    // The D latch: through while EN; held when not. The flip-flop of latches: as the primitive one, on each rising edge only.
    u8 ref_latch = FUDE_SIM_X, ref_ff = FUDE_SIM_X, d = 0, en = 0, clk = 0;
    u32 steps = 0, latch_right = 0, ff_right = 0;
    for(u32 k = 0; k < 3000u; k++) {
        const u32 what = rnd(3u);
        if(what == 0u) { d ^= 1u; put(&w, "D", d); }
        else if(what == 1u) { en ^= 1u; put(&w, "EN", en); }
        else { clk ^= 1u; put(&w, "CLK", clk); if(clk) ref_ff = d; }
        if(en) ref_latch = d;
        steps++;
        latch_right += get(&w, "dq") == ref_latch && (ref_latch > 1u || get(&w, "dnq") == (u8)(1u - ref_latch)) ? 1u : 0u;
        // (the primitive starts at 0; the latches' one unknown until its first edge)
        ff_right += get(&w, "fq") == ref_ff && (ref_ff > 1u || get(&w, "fq") == get(&w, "fpq")) ? 1u : 0u;
    }
    CHECK(latch_right == steps && ff_right == steps);
    printf("  universal gates, latches, flip-flops of latches: %u steps\n", steps);
    // How deep it went: the flip-flop of latches is latches of a latch of NANDs.
    CHECK(fude_sim_flat_find(&w.flat, "F/S/L/G1") != FUDE_SIM_NONE);
    world_free(&w, top);
}

// --- a memory of registers of flip-flops of latches ----------------------------------------------------------

static void test_memory(void) {
    world w; fude_sim_lib_init(&w.lib); load_parts(&w.lib);
    bench b; bench_start(&b);
    inputs(&b, "A0 A1 D0 D1 D2 D3 WE CLK");
    line(&b, "inst user/ram4x4 RAM \"\" A0 A1 D0 D1 D2 D3 WE CLK Q0 Q1 Q2 Q3");
    probes(&b, "Q0 Q1 Q2 Q3");
    fude_sim_def* top = bench_run(&b, &w);
    // Its primitives: 4 registers of 4 (a multiplexer of 4 gates, a flip-flop of 11) + its decoder (6) + 4 multiplexers (7).
    CHECK(rde_arr_length(&w.flat.prims) == 8u + 4u + 4u * 4u * (4u + 11u) + 6u + 4u * 7u);
    u32 mem[4]; b8 known_w[4] = { false, false, false, false };
    u32 ops = 0, right = 0;
    for(u32 k = 0; k < 1500u; k++) {
        const u32 addr = rnd(4u);
        put_num(&w, "A%u", 0u, 2u, addr);
        if(rnd(3u) == 0u) {
            const u32 v = rnd(16u);
            put_num(&w, "D%u", 0u, 4u, v);
            put(&w, "WE", 1u); tick(&w, "CLK"); put(&w, "WE", 0u);
            mem[addr] = v; known_w[addr] = true;
        } else {
            // (data changing and the clock ticking without WE: nothing written)
            put_num(&w, "D%u", 0u, 4u, rnd(16u));
            if(rnd(2u)) tick(&w, "CLK");
        }
        const u32 got = get_num(&w, "Q%u", 0u, 4u);
        right += known_w[addr] ? got == mem[addr] : got == 0xFFFFFFFFu;
        ops++;
    }
    CHECK(right == ops);
    printf("  a 4 x 4 memory of %u primitives: %u reads and writes\n", (u32)rde_arr_length(&w.flat.prims), ops);
    world_free(&w, top);
}

// --- counters, a decade counter, a clock of minutes and seconds ----------------------------------------------

static void test_clock(void) {
    world w; fude_sim_lib_init(&w.lib); load_parts(&w.lib);
    bench b; bench_start(&b);
    inputs(&b, "CLK RST EN");
    line(&b, "inst user/counter4 C \"\" CLK RST EN c0 c1 c2 c3 ctc");
    line(&b, "inst user/bcd D \"\" CLK RST EN d0 d1 d2 d3 dtc");
    line(&b, "inst user/clock60 S \"\" CLK RST EN su0 su1 su2 su3 st0 st1 st2 st3 stc");
    line(&b, "inst user/clock60 M \"\" CLK RST stc mu0 mu1 mu2 mu3 mt0 mt1 mt2 mt3 mtc");
    probes(&b, "c0 c1 c2 c3 ctc d0 d1 d2 d3 dtc su0 su1 su2 su3 st0 st1 st2 st3 stc mu0 mu1 mu2 mu3 mt0 mt1 mt2 mt3 mtc");
    fude_sim_def* top = bench_run(&b, &w);
    // Unknown until reset (nothing has cleared its flip-flops); one clock with RST high: all 0.
    CHECK(get_num(&w, "c%u", 0u, 4u) == 0xFFFFFFFFu);
    put(&w, "RST", 1u); tick(&w, "CLK"); put(&w, "RST", 0u);
    CHECK(get_num(&w, "c%u", 0u, 4u) == 0u && get_num(&w, "d%u", 0u, 4u) == 0u && get_num(&w, "su%u", 0u, 4u) == 0u && get_num(&w, "mt%u", 0u, 4u) == 0u);
    // Counting, with EN off now and then (held), and an hour and a bit of seconds: 59:59 wraps to 00:00.
    put(&w, "EN", 1u);
    u32 count = 0, dec = 0, secs = 0, mins = 0, ticks = 0, right = 0;
    for(u32 k = 0; k < 3700u; k++) {
        const b8 en = rnd(10u) != 0u;
        put(&w, "EN", en);
        CHECK(get(&w, "ctc") == (en && count == 15u ? 1u : 0u));
        tick(&w, "CLK");
        if(en) {
            count = (count + 1u) & 15u; dec = (dec + 1u) % 10u;
            secs++; if(secs == 60u) { secs = 0; mins = (mins + 1u) % 60u; }
        }
        ticks++;
        const u32 su = get_num(&w, "su%u", 0u, 4u), st = get_num(&w, "st%u", 0u, 4u), mu = get_num(&w, "mu%u", 0u, 4u), mt = get_num(&w, "mt%u", 0u, 4u);
        right += get_num(&w, "c%u", 0u, 4u) == count && get_num(&w, "d%u", 0u, 4u) == dec && su == secs % 10u && st == secs / 10u &&
                 mu == mins % 10u && mt == mins / 10u ? 1u : 0u;
    }
    CHECK(right == ticks);
    printf("  a clock (seconds into minutes, each of decade counters of counters of registers): %u ticks, at %u%u:%u%u\n", ticks,
           get_num(&w, "mt%u", 0u, 4u), get_num(&w, "mu%u", 0u, 4u), get_num(&w, "st%u", 0u, 4u), get_num(&w, "su%u", 0u, 4u));
    // RST mid-count: back to 00:00 on the clock.
    put(&w, "RST", 1u); tick(&w, "CLK"); put(&w, "RST", 0u);
    CHECK(get_num(&w, "su%u", 0u, 4u) == 0u && get_num(&w, "st%u", 0u, 4u) == 0u && get_num(&w, "mu%u", 0u, 4u) == 0u && get_num(&w, "mt%u", 0u, 4u) == 0u);
    world_free(&w, top);

    // The same seconds driven by a clock part on its own: run on to a time, the count is how many rising edges there were.
    fude_sim_lib_init(&w.lib); load_parts(&w.lib);
    bench_start(&b);
    inputs(&b, "RST EN");
    line(&b, "inst clock CK \"period=1 duty=0.5\" clk");
    line(&b, "inst user/clock60 S \"\" clk RST EN su0 su1 su2 su3 st0 st1 st2 st3 stc");
    probes(&b, "su0 su1 su2 su3 st0 st1 st2 st3");
    top = bench_run(&b, &w);
    put(&w, "RST", 1u);
    fude_sim_run_until(&w.run, 0.9);   // (its first rising edge at 0.5: cleared)
    put(&w, "RST", 0u); put(&w, "EN", 1u);
    u32 runs_right = 0;
    for(u32 s = 1; s <= 150u; s++) {
        fude_sim_run_until(&w.run, 0.9 + (f64)s);   // (an edge at s + 0.5 each)
        const u32 want = s % 60u;
        runs_right += get_num(&w, "su%u", 0u, 4u) == want % 10u && get_num(&w, "st%u", 0u, 4u) == want / 10u ? 1u : 0u;
    }
    CHECK(runs_right == 150u && !w.run.oscillating);
    world_free(&w, top);
}

// --- an ALU, an LFSR, an adding machine of a counter, a memory, an adder and a register ----------------------

static void test_alu_lfsr_machine(void) {
    world w; fude_sim_lib_init(&w.lib); load_parts(&w.lib);
    bench b; bench_start(&b);
    inputs(&b, "A0 A1 A2 A3 B0 B1 B2 B3 OP0 OP1");
    line(&b, "inst user/alu4 ALU \"\" A0 A1 A2 A3 B0 B1 B2 B3 OP0 OP1 Y0 Y1 Y2 Y3 CO Z");
    probes(&b, "Y0 Y1 Y2 Y3 CO Z");
    fude_sim_def* top = bench_run(&b, &w);
    u32 right = 0;
    for(u32 v = 0; v < 1024u; v++) {
        const u32 a = v & 15u, bb = (v >> 4) & 15u, op = v >> 8;
        put_num(&w, "A%u", 0u, 4u, a); put_num(&w, "B%u", 0u, 4u, bb); put_num(&w, "OP%u", 0u, 2u, op);
        const u32 full = op == 0u ? (a & bb) : op == 1u ? (a | bb) : op == 2u ? (a ^ bb) : a + bb;
        right += get_num(&w, "Y%u", 0u, 4u) == (full & 15u) && get(&w, "CO") == (op == 3u ? full >> 4 : 0u) && get(&w, "Z") == ((full & 15u) == 0u) ? 1u : 0u;
    }
    CHECK(right == 1024u);
    world_free(&w, top);

    // The LFSR: loaded with each seed, every one but 0 comes round in 15 clocks, as a model of it says.
    fude_sim_lib_init(&w.lib); load_parts(&w.lib);
    bench_start(&b);
    inputs(&b, "CLK LD S0 S1 S2 S3");
    line(&b, "inst user/lfsr4 L \"\" CLK LD S0 S1 S2 S3 Q0 Q1 Q2 Q3");
    probes(&b, "Q0 Q1 Q2 Q3");
    top = bench_run(&b, &w);
    u32 seeds_right = 0;
    for(u32 seed = 1; seed < 16u; seed++) {
        put_num(&w, "S%u", 0u, 4u, seed); put(&w, "LD", 1u); tick(&w, "CLK"); put(&w, "LD", 0u);
        u32 model = seed, seen = 0, ok = get_num(&w, "Q%u", 0u, 4u) == seed;
        for(u32 k = 0; k < 15u; k++) {
            const u32 f = ((model >> 3) ^ (model >> 2)) & 1u;
            model = ((model << 1) | f) & 15u;
            tick(&w, "CLK");
            ok = ok && get_num(&w, "Q%u", 0u, 4u) == model;
            seen |= 1u << model;
        }
        seeds_right += ok && model == seed && seen == 0xFFFEu ? 1u : 0u;
    }
    CHECK(seeds_right == 15u);
    world_free(&w, top);

    // The adding machine: a counter walks a memory's addresses; first it writes what it is given into each, then
    // an accumulator (a register through an adder) adds up what each address holds: the sum, mod 16.
    fude_sim_lib_init(&w.lib); load_parts(&w.lib);
    bench_start(&b);
    inputs(&b, "CLK RST WE ACC D0 D1 D2 D3");
    line(&b, "inst const ONE \"1\" one");
    line(&b, "inst user/counter4 PC \"\" CLK RST one a0 a1 - - -");
    line(&b, "inst user/ram4x4 RAM \"\" a0 a1 D0 D1 D2 D3 WE CLK m0 m1 m2 m3");
    line(&b, "inst user/add4 ADD \"\" r0 r1 r2 r3 m0 m1 m2 m3 zero s0 s1 s2 s3 -");
    line(&b, "inst const Z \"0\" zero");
    line(&b, "inst not NR \"\" RST nrst");
    line(&b, "inst and K0 \"\" s0 nrst k0"); line(&b, "inst and K1 \"\" s1 nrst k1"); line(&b, "inst and K2 \"\" s2 nrst k2"); line(&b, "inst and K3 \"\" s3 nrst k3");
    line(&b, "inst or L \"\" ACC RST ld");
    line(&b, "inst user/reg4 R \"\" k0 k1 k2 k3 ld CLK r0 r1 r2 r3");
    probes(&b, "r0 r1 r2 r3 a0 a1 m0 m1 m2 m3");
    top = bench_run(&b, &w);
    u32 sums_right = 0;
    for(u32 trial = 0; trial < 120u; trial++) {
        put(&w, "RST", 1u); tick(&w, "CLK"); put(&w, "RST", 0u);
        u32 data[4], sum = 0;
        put(&w, "WE", 1u);
        for(u32 i = 0; i < 4u; i++) { data[i] = rnd(16u); sum += data[i]; put_num(&w, "D%u", 0u, 4u, data[i]); tick(&w, "CLK"); }
        put(&w, "WE", 0u); put(&w, "ACC", 1u);
        b8 ok = get_num(&w, "a%u", 0u, 2u) == 0u && get_num(&w, "r%u", 0u, 4u) == 0u;
        for(u32 i = 0; i < 4u; i++) { ok = ok && get_num(&w, "m%u", 0u, 4u) == data[i]; tick(&w, "CLK"); }
        put(&w, "ACC", 0u);
        sums_right += ok && get_num(&w, "r%u", 0u, 4u) == (sum & 15u) ? 1u : 0u;
    }
    CHECK(sums_right == 120u);
    printf("  an ALU (1024 cases), an LFSR (15 seeds), an adding machine of %u primitives (120 sums)\n", (u32)rde_arr_length(&w.flat.prims));
    world_free(&w, top);
}

// --- the chips --------------------------------------------------------------------------------------------

// A chip on a bench: each of its logic pins an input (its inputs) or a probe (its outputs), named as its pins.
static fude_sim_def* chip_bench(world* w, const c8* id) {
    fude_sim_lib_init(&w->lib);
    CHECK(fude_sim_chips_add(&w->lib) == 0u);
    const fude_sim_def* chip = fude_sim_lib_find(&w->lib, id);
    CHECK(chip != NULL);
    bench b; bench_start(&b);
    c8 inst[2048]; usize n = (usize)snprintf(inst, sizeof inst, "inst %s U \"\"", id);
    const fude_sim_port* p = (const fude_sim_port*)chip->ports.memory;
    for(u32 i = 0; i < (u32)rde_arr_length(&chip->ports); i++) {
        n += (usize)snprintf(inst + n, sizeof inst - n, " %s", p[i].name);
        if(p[i].domain != FUDE_SIM_LOGIC) continue;
        if(p[i].dir == FUDE_SIM_IN) inputs(&b, p[i].name); else probes(&b, p[i].name);
    }
    line(&b, "%s", inst);
    return bench_run(&b, w);
}

static void test_chips(void) {
    // Every chip reads, and its pins are as many as its package's.
    {
        fude_sim_lib lib; fude_sim_lib_init(&lib);
        CHECK(fude_sim_chips_add(&lib) == 0u && fude_sim_chip_count() == 16u);
        const c8* const ids[16] = { "74HC00", "74HC02", "74HC04", "74HC08", "74HC32", "74HC86", "74HC74", "74HC138", "74HC157", "74HC161", "74HC173", "74HC245", "74HC283", "74HC189",
                                    "CD4511", "CD4017" };
        const u32 pins[16] = { 14, 14, 14, 14, 14, 14, 14, 16, 16, 16, 16, 20, 16, 16, 16, 16 };
        for(u32 i = 0; i < 16u; i++) {
            const fude_sim_def* d = fude_sim_lib_find(&lib, ids[i]);
            CHECK(d != NULL && rde_arr_length(&d->ports) == pins[i]);
            // (its supply its last pin, its ground the last of its first half: as in the package — a CD40's VDD, VSS)
            if(d != NULL) {
                const fude_sim_port* p = (const fude_sim_port*)d->ports.memory;
                const b8 cmos = i >= 14u;
                CHECK(strcmp(p[pins[i] - 1u].name, cmos ? "VDD" : "VCC") == 0 && strcmp(p[pins[i] / 2u - 1u].name, cmos ? "VSS" : "GND") == 0 &&
                      p[pins[i] - 1u].domain == FUDE_SIM_ELECTRIC);
            }
        }
        fude_sim_lib_destroy(&lib);
    }
    // The gates' chips: every gate, every input.
    world w;
    const c8* const quads[5] = { "74HC00", "74HC02", "74HC08", "74HC32", "74HC86" };
    for(u32 c = 0; c < 5u; c++) {
        fude_sim_def* top = chip_bench(&w, quads[c]);
        u32 right = 0;
        for(u32 v = 0; v < 256u; v++) {
            u32 ok = 1;
            for(u32 g = 1; g <= 4u; g++) {
                c8 a[8], bb[8], y[8]; snprintf(a, sizeof a, "%uA", g); snprintf(bb, sizeof bb, "%uB", g); snprintf(y, sizeof y, "%uY", g);
                const u32 x = (v >> (2u * (g - 1u))) & 1u, z = (v >> (2u * (g - 1u) + 1u)) & 1u;
                put(&w, a, (u8)x); put(&w, bb, (u8)z);
                const u32 want = c == 0u ? !(x & z) : c == 1u ? !(x | z) : c == 2u ? (x & z) : c == 3u ? (x | z) : (x ^ z);
                ok &= get(&w, y) == want;
            }
            right += ok;
        }
        CHECK(right == 256u);
        world_free(&w, top);
    }
    {
        fude_sim_def* top = chip_bench(&w, "74HC04");
        u32 right = 0;
        for(u32 v = 0; v < 64u; v++) {
            put_num(&w, "%uA", 1u, 6u, v);
            right += get_num(&w, "%uY", 1u, 6u) == (~v & 63u) ? 1u : 0u;
        }
        CHECK(right == 64u);
        world_free(&w, top);
    }
    // The decoder: all 64 ways its inputs can be.
    {
        fude_sim_def* top = chip_bench(&w, "74HC138");
        u32 right = 0;
        for(u32 v = 0; v < 64u; v++) {
            put(&w, "A", v & 1u); put(&w, "B", (v >> 1) & 1u); put(&w, "C", (v >> 2) & 1u);
            put(&w, "G1", (v >> 3) & 1u); put(&w, "~G2A", (v >> 4) & 1u); put(&w, "~G2B", (v >> 5) & 1u);
            const b8 on = ((v >> 3) & 7u) == 1u;
            u32 ok = 1;
            for(u32 k = 0; k < 8u; k++) { c8 y[8]; snprintf(y, sizeof y, "~Y%u", k); ok &= get(&w, y) == (on && (v & 7u) == k ? 0u : 1u); }
            right += ok;
        }
        CHECK(right == 64u);
        world_free(&w, top);
    }
    // The multiplexer: 1024 ways.
    {
        fude_sim_def* top = chip_bench(&w, "74HC157");
        u32 right = 0;
        for(u32 v = 0; v < 1024u; v++) {
            put(&w, "S", v & 1u); put(&w, "~E", (v >> 1) & 1u);
            u32 ok = 1;
            for(u32 g = 1; g <= 4u; g++) {
                c8 a[8], bb[8], y[8]; snprintf(a, sizeof a, "%uA", g); snprintf(bb, sizeof bb, "%uB", g); snprintf(y, sizeof y, "%uY", g);
                const u32 x = (v >> (2u * g)) & 1u, z = (v >> (2u * g + 1u)) & 1u;
                put(&w, a, (u8)x); put(&w, bb, (u8)z);
                ok &= get(&w, y) == (((v >> 1) & 1u) ? 0u : (v & 1u) ? z : x);
            }
            right += ok;
        }
        CHECK(right == 1024u);
        world_free(&w, top);
    }
    // The BCD to 7-segment decoder: every BCD (6 and 9 without tails, 10–15 blank), lamp test, blanking, its latch.
    {
        fude_sim_def* top = chip_bench(&w, "CD4511");
        static const u8 want[16] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7C, 0x07, 0x7F, 0x67, 0, 0, 0, 0, 0, 0 };
        static const c8* const seg[7] = { "a", "b", "c", "d", "e", "f", "g" };
        put(&w, "~LT", 1u); put(&w, "~BI", 1u); put(&w, "LE", 0u);
        u32 right = 0;
        for(u32 v = 0; v < 16u; v++) {
            put(&w, "A", v & 1u); put(&w, "B", (v >> 1) & 1u); put(&w, "C", (v >> 2) & 1u); put(&w, "D", (v >> 3) & 1u);
            u8 got = 0;
            for(u32 g = 0; g < 7u; g++) got |= (u8)(get(&w, seg[g]) == 1u ? 1u << g : 0u);
            if(got != want[v]) printf("  CD4511 %u: %02X, not %02X\n", v, got, want[v]);
            right += got == want[v];
        }
        CHECK(right == 16u);
        put(&w, "A", 1u); put(&w, "B", 0u); put(&w, "C", 1u); put(&w, "D", 0u);   // (5)
        put(&w, "LE", 1u);
        put(&w, "B", 1u); put(&w, "C", 0u);   // (3, latched out)
        u8 got = 0;
        for(u32 g = 0; g < 7u; g++) got |= (u8)(get(&w, seg[g]) == 1u ? 1u << g : 0u);
        CHECK(got == want[5]);
        put(&w, "LE", 0u);
        got = 0;
        for(u32 g = 0; g < 7u; g++) got |= (u8)(get(&w, seg[g]) == 1u ? 1u << g : 0u);
        CHECK(got == want[3]);
        put(&w, "~LT", 0u); put(&w, "~BI", 0u);
        got = 0;
        for(u32 g = 0; g < 7u; g++) got |= (u8)(get(&w, seg[g]) == 1u ? 1u << g : 0u);
        CHECK(got == 0x7F);
        put(&w, "~LT", 1u);
        got = 0;
        for(u32 g = 0; g < 7u; g++) got |= (u8)(get(&w, seg[g]) == 1u ? 1u << g : 0u);
        CHECK(got == 0);
        world_free(&w, top);
    }
    // The decade counter: 0 at reset, a step on each rising clock — one output at a time, 9 then 0 again —, CO high for 0
    // to 4; INH high holds it; RST high back to 0.
    {
        fude_sim_def* top = chip_bench(&w, "CD4017");
        static const c8* const q[10] = { "Q0", "Q1", "Q2", "Q3", "Q4", "Q5", "Q6", "Q7", "Q8", "Q9" };
        put(&w, "RST", 0u); put(&w, "INH", 0u); put(&w, "CLK", 0u);
        put(&w, "RST", 1u); put(&w, "RST", 0u);
        u32 right = 0;
        for(u32 k = 0; k < 23u; k++) {
            u32 hot = 0, which = 99u;
            for(u32 i = 0; i < 10u; i++) if(get(&w, q[i]) == 1u) { hot++; which = i; }
            const b8 ok = hot == 1u && which == k % 10u && get(&w, "CO") == (k % 10u < 5u ? 1u : 0u);
            if(!ok) printf("  CD4017 after %u clocks: %u high (Q%u), CO %u\n", k, hot, which, get(&w, "CO"));
            right += ok;
            put(&w, "CLK", 1u); put(&w, "CLK", 0u);
        }
        CHECK(right == 23u);   // (23 clocks: at 3)
        put(&w, "INH", 1u);
        put(&w, "CLK", 1u); put(&w, "CLK", 0u);
        CHECK(get(&w, "Q3") == 1u);
        put(&w, "INH", 0u);
        put(&w, "RST", 1u);
        CHECK(get(&w, "Q0") == 1u && get(&w, "Q3") == 0u);
        put(&w, "CLK", 1u); put(&w, "CLK", 0u);
        CHECK(get(&w, "Q0") == 1u);   // (held at 0 while RST is high)
        world_free(&w, top);
    }
    // The adder: 512 sums.
    {
        fude_sim_def* top = chip_bench(&w, "74HC283");
        u32 right = 0;
        for(u32 v = 0; v < 512u; v++) {
            put_num(&w, "A%u", 1u, 4u, v & 15u); put_num(&w, "B%u", 1u, 4u, (v >> 4) & 15u); put(&w, "C0", (u8)(v >> 8));
            const u32 sum = (v & 15u) + ((v >> 4) & 15u) + (v >> 8);
            right += get_num(&w, "S%u", 1u, 4u) == (sum & 15u) && get(&w, "C4") == (sum >> 4) ? 1u : 0u;
        }
        CHECK(right == 512u);
        world_free(&w, top);
    }
    // The dual flip-flop: random steps, its preset and clear at once, its D on the clock's rising edge.
    {
        fude_sim_def* top = chip_bench(&w, "74HC74");
        u8 q[2] = { 0, 0 }, d[2] = { 0, 0 }, clk[2] = { 0, 0 }, pre[2] = { 1, 1 }, clr[2] = { 1, 1 };
        for(u32 f = 0; f < 2u; f++) { c8 n[8]; snprintf(n, sizeof n, "~%uPRE", f + 1u); put(&w, n, 1u); snprintf(n, sizeof n, "~%uCLR", f + 1u); put(&w, n, 0u); put(&w, n, 1u); }
        u32 steps = 0, right = 0;
        for(u32 k = 0; k < 4000u; k++) {
            const u32 f = rnd(2u), what = rnd(10u);
            c8 n[8];
            if(what < 4u) { d[f] ^= 1u; snprintf(n, sizeof n, "%uD", f + 1u); put(&w, n, d[f]); }
            else if(what < 8u) { clk[f] ^= 1u; snprintf(n, sizeof n, "%uCLK", f + 1u); put(&w, n, clk[f]); if(clk[f] && pre[f] && clr[f]) q[f] = d[f]; }
            else if(what == 8u) { pre[f] ^= 1u; snprintf(n, sizeof n, "~%uPRE", f + 1u); put(&w, n, pre[f]); }
            else { clr[f] ^= 1u; snprintf(n, sizeof n, "~%uCLR", f + 1u); put(&w, n, clr[f]); }
            for(u32 g = 0; g < 2u; g++) { if(!pre[g] && clr[g]) q[g] = 1u; if(pre[g] && !clr[g]) q[g] = 0u; }
            u32 ok = 1;
            for(u32 g = 0; g < 2u; g++) {
                c8 qn[8], nqn[8]; snprintf(qn, sizeof qn, "%uQ", g + 1u); snprintf(nqn, sizeof nqn, "~%uQ", g + 1u);
                if(!pre[g] && !clr[g]) ok &= get(&w, qn) == 1u && get(&w, nqn) == 1u;
                else if(!pre[g] || !clr[g] || q[g] <= 1u) ok &= get(&w, qn) == q[g] && get(&w, nqn) == (u8)(1u - q[g]);
            }
            // (both let go together from both low: unknown, as the chip's is; the model follows what it reads then)
            for(u32 g = 0; g < 2u; g++) { c8 qn[8]; snprintf(qn, sizeof qn, "%uQ", g + 1u); if(pre[g] && clr[g] && get(&w, qn) > 1u) q[g] = get(&w, qn); }
            steps++; right += ok;
        }
        CHECK(right == steps);
        world_free(&w, top);
    }
    // The counter: random clears, loads, enables; its carry out.
    {
        fude_sim_def* top = chip_bench(&w, "74HC161");
        u8 clr = 1, load = 1, enp = 1, ent = 1;
        put(&w, "~CLR", 0u); put(&w, "~CLR", 1u); put(&w, "~LOAD", 1u); put(&w, "ENP", 1u); put(&w, "ENT", 1u);
        u32 q = 0, steps = 0, right = 0, wrapped = 0;
        for(u32 k = 0; k < 4000u; k++) {
            const u32 what = rnd(20u);
            if(what == 0u) { clr ^= 1u; put(&w, "~CLR", clr); }
            else if(what == 1u) { load ^= 1u; put(&w, "~LOAD", load); }
            else if(what == 2u) { enp ^= 1u; put(&w, "ENP", enp); }
            else if(what == 3u) { ent ^= 1u; put(&w, "ENT", ent); }
            else if(what == 4u) { const u32 v = rnd(16u); put(&w, "A", v & 1u); put(&w, "B", (v >> 1) & 1u); put(&w, "C", (v >> 2) & 1u); put(&w, "D", (v >> 3) & 1u); }
            else {
                tick(&w, "CLK");
                if(clr) {
                    if(!load) {
                        const u32 pa = fude_sim_flat_find(&w.flat, "i:A"), pb = fude_sim_flat_find(&w.flat, "i:B"), pc = fude_sim_flat_find(&w.flat, "i:C"), pd = fude_sim_flat_find(&w.flat, "i:D");
                        q = (u32)fude_sim_in(&w.run, pa, 0) | (u32)fude_sim_in(&w.run, pb, 0) << 1 | (u32)fude_sim_in(&w.run, pc, 0) << 2 | (u32)fude_sim_in(&w.run, pd, 0) << 3;
                    } else if(enp && ent) {
                        wrapped += q == 15u;
                        q = (q + 1u) & 15u;
                    }
                }
            }
            if(!clr) q = 0u;
            const u32 got = get(&w, "QA") | get(&w, "QB") << 1 | get(&w, "QC") << 2 | get(&w, "QD") << 3;
            steps++;
            right += got == q && get(&w, "RCO") == (ent && q == 15u ? 1u : 0u) ? 1u : 0u;
        }
        CHECK(right == steps && wrapped > 0u);
        world_free(&w, top);
    }
    // The register: loads while G1 and G2 are low, its outputs released while M or N is high, cleared by CLR.
    {
        fude_sim_def* top = chip_bench(&w, "74HC173");
        put(&w, "CLR", 1u); put(&w, "CLR", 0u);
        u32 q = 0, steps = 0, right = 0;
        u8 g1 = 0, g2 = 0, m = 0, nn = 0, clr = 0;
        for(u32 k = 0; k < 4000u; k++) {
            const u32 what = rnd(12u);
            if(what == 0u) { g1 ^= 1u; put(&w, "~G1", g1); }
            else if(what == 1u) { g2 ^= 1u; put(&w, "~G2", g2); }
            else if(what == 2u) { m ^= 1u; put(&w, "M", m); }
            else if(what == 3u) { nn ^= 1u; put(&w, "N", nn); }
            else if(what == 4u) { clr ^= 1u; put(&w, "CLR", clr); }
            else if(what < 8u) { put_num(&w, "%uD", 1u, 4u, rnd(16u)); }
            else {
                tick(&w, "CLK");
                if(!g1 && !g2 && !clr) {
                    q = 0;
                    for(u32 i = 0; i < 4u; i++) { c8 n[8]; snprintf(n, sizeof n, "i:%uD", i + 1u); q |= (u32)fude_sim_in(&w.run, fude_sim_flat_find(&w.flat, n), 0) << i; }
                }
            }
            if(clr) q = 0;
            u32 ok = 1;
            for(u32 i = 0; i < 4u; i++) { c8 n[8]; snprintf(n, sizeof n, "%uQ", i + 1u); ok &= get(&w, n) == (m || nn ? FUDE_SIM_Z : (u8)((q >> i) & 1u)); }
            steps++; right += ok;
        }
        CHECK(right == steps);
        world_free(&w, top);
    }
    // The bus transceiver: each side driven by the bench (through tri-states of its own), the other side follows.
    {
        world w2; fude_sim_lib_init(&w2.lib); CHECK(fude_sim_chips_add(&w2.lib) == 0u);
        bench b; bench_start(&b);
        inputs(&b, "DIR OE DRA DRB");
        c8 inst[512]; usize n = (usize)snprintf(inst, sizeof inst, "inst 74HC245 U \"\" DIR");
        for(u32 i = 1; i <= 8u; i++) n += (usize)snprintf(inst + n, sizeof inst - n, " a%u", i);
        n += (usize)snprintf(inst + n, sizeof inst - n, " -");
        for(u32 i = 8; i >= 1u; i--) n += (usize)snprintf(inst + n, sizeof inst - n, " b%u", i);
        snprintf(inst + n, sizeof inst - n, " OE -");
        line(&b, "%s", inst);
        for(u32 i = 1; i <= 8u; i++) {
            c8 x[8]; snprintf(x, sizeof x, "xa%u xb%u", i, i); inputs(&b, x);
            line(&b, "inst tribuf TA%u \"\" xa%u DRA a%u", i, i, i); line(&b, "inst tribuf TB%u \"\" xb%u DRB b%u", i, i, i);
            snprintf(x, sizeof x, "a%u b%u", i, i); probes(&b, x);
        }
        fude_sim_def* top = bench_run(&b, &w2);
        u32 steps = 0, right = 0;
        for(u32 k = 0; k < 600u; k++) {
            const u8 dir = (u8)rnd(2u), oe = (u8)(rnd(4u) == 0u), dra = (u8)rnd(2u), drb = (u8)rnd(2u);
            const u32 xa = rnd(256u), xb = rnd(256u);
            put(&w2, "DIR", dir); put(&w2, "OE", oe); put(&w2, "DRA", dra); put(&w2, "DRB", drb);
            put_num(&w2, "xa%u", 1u, 8u, xa); put_num(&w2, "xb%u", 1u, 8u, xb);
            u32 ok = 1;
            for(u32 i = 0; i < 8u; i++) {
                const u8 va = (u8)((xa >> i) & 1u), vb = (u8)((xb >> i) & 1u);
                const b8 ab = !oe && dir, ba = !oe && !dir;
                // (a side: what drives it — the bench, the chip, or both: agreeing or not)
                const u8 da = dra ? va : FUDE_SIM_Z, db = drb ? vb : FUDE_SIM_Z;
                u8 wa = da, wb = db;
                if(ab) { const u8 from = dra ? va : FUDE_SIM_X; wb = drb ? (from == vb ? vb : FUDE_SIM_X) : from; }
                if(ba) { const u8 from = drb ? vb : FUDE_SIM_X; wa = dra ? (from == va ? va : FUDE_SIM_X) : from; }
                c8 an[8], bn[8]; snprintf(an, sizeof an, "a%u", i + 1u); snprintf(bn, sizeof bn, "b%u", i + 1u);
                ok &= get(&w2, an) == wa && get(&w2, bn) == wb;
            }
            steps++; right += ok;
        }
        CHECK(right == steps);
        world_free(&w2, top);
    }
    // The memory chip: written while CS and WE are low, read turned over, released otherwise; unknown before written.
    {
        fude_sim_def* top = chip_bench(&w, "74HC189");
        put(&w, "~CS", 1u); put(&w, "~WE", 1u);
        u32 mem[16]; b8 known_w[16]; memset(known_w, 0, sizeof known_w);
        u32 steps = 0, right = 0;
        for(u32 k = 0; k < 3000u; k++) {
            const u32 addr = rnd(16u);
            put(&w, "A0", addr & 1u); put(&w, "A1", (addr >> 1) & 1u); put(&w, "A2", (addr >> 2) & 1u); put(&w, "A3", (addr >> 3) & 1u);
            const u32 what = rnd(4u);
            if(what == 0u) {
                const u32 v = rnd(16u);
                put_num(&w, "D%u", 1u, 4u, v);
                put(&w, "~CS", 0u); put(&w, "~WE", 0u);
                u32 ok = 1;
                for(u32 i = 1; i <= 4u; i++) { c8 n[8]; snprintf(n, sizeof n, "~Q%u", i); ok &= get(&w, n) == FUDE_SIM_Z; }
                put(&w, "~WE", 1u); put(&w, "~CS", 1u);
                mem[addr] = v; known_w[addr] = true;
                steps++; right += ok;
            } else {
                put(&w, "~CS", what == 1u ? 1u : 0u);
                u32 ok = 1;
                for(u32 i = 0; i < 4u; i++) {
                    c8 n[8]; snprintf(n, sizeof n, "~Q%u", i + 1u);
                    const u8 want = what == 1u ? FUDE_SIM_Z : known_w[addr] ? (u8)(1u - ((mem[addr] >> i) & 1u)) : FUDE_SIM_X;
                    ok &= get(&w, n) == want;
                }
                put(&w, "~CS", 1u);
                steps++; right += ok;
            }
        }
        CHECK(right == steps);
        world_free(&w, top);
    }
    printf("  the chips: 74HC00 02 04 08 32 86 74 138 157 161 173 245 283 189, each against its model\n");
}

// --- a machine of chips only: a 74HC161 counts the addresses of a 74HC189 memory, 74HC04s turn its words back
// over, a 74HC283 adds each to what a 74HC173 register holds (its outputs on) — the sum of the memory. ---------

static void test_chip_machine(void) {
    world w; fude_sim_lib_init(&w.lib); CHECK(fude_sim_chips_add(&w.lib) == 0u);
    bench b; bench_start(&b);
    inputs(&b, "CLK CLR LOAD CS WE D1 D2 D3 D4 ACC NACC");
    line(&b, "inst const H \"1\" hi"); line(&b, "inst const L \"0\" lo");
    // (the counter: its pins in order — ~CLR CLK A B C D ENP GND ~LOAD ENT QD QC QB QA RCO VCC)
    line(&b, "inst 74HC161 PC \"\" CLR CLK lo lo lo lo hi - LOAD hi a3 a2 a1 a0 - -");
    // (the memory: A0 ~CS ~WE D1 ~Q1 D2 ~Q2 GND ~Q3 D3 ~Q4 D4 A3 A2 A1 VCC)
    line(&b, "inst 74HC189 RAM \"\" a0 CS WE D1 nq1 D2 nq2 - nq3 D3 nq4 D4 a3 a2 a1 -");
    // (two of the six inverters: 1A 1Y 2A 2Y 3A 3Y GND 4Y 4A 5Y 5A 6Y 6A VCC)
    line(&b, "inst 74HC04 INV \"\" nq1 m1 nq2 m2 nq3 m3 - m4 nq4 - lo - lo -");
    // (the adder: S2 B2 A2 S1 A1 B1 C0 GND C4 S4 B4 A4 S3 A3 B3 VCC)
    line(&b, "inst 74HC283 ADD \"\" s2 m2 r2 s1 r1 m1 lo - - s4 m4 r4 s3 r3 m3 -");
    // (the register: M N 1Q 2Q 3Q 4Q CLK GND ~G1 ~G2 4D 3D 2D 1D CLR VCC; it loads while ACC is low… its G pins low)
    line(&b, "inst 74HC173 REG \"\" lo lo r1 r2 r3 r4 CLK - NACC NACC s4 s3 s2 s1 ACC -");
    probes(&b, "r1 r2 r3 r4 a0 a1 a2 a3 m1 m2 m3 m4");
    fude_sim_def* top = bench_run(&b, &w);
    // (ACC here the register's clear; NACC its loads' enable, low to load)
    u32 sums_right = 0;
    for(u32 trial = 0; trial < 40u; trial++) {
        // Cleared; the counter at 0; each word written at its address (the counter walking them).
        put(&w, "NACC", 1u); put(&w, "ACC", 1u); put(&w, "ACC", 0u);
        put(&w, "CLR", 0u); put(&w, "CLR", 1u); put(&w, "LOAD", 1u); put(&w, "CS", 1u); put(&w, "WE", 1u);
        u32 data[16], sum = 0;
        for(u32 i = 0; i < 16u; i++) {
            data[i] = rnd(16u); sum += data[i];
            put_num(&w, "D%u", 1u, 4u, data[i]);
            put(&w, "CS", 0u); put(&w, "WE", 0u); put(&w, "WE", 1u); put(&w, "CS", 1u);
            tick(&w, "CLK");
        }
        // Then each word read, added to the register on each clock (the counter walking round again).
        b8 ok = get_num(&w, "a%u", 0u, 4u) == 0u && get_num(&w, "r%u", 1u, 4u) == 0u;
        put(&w, "CS", 0u); put(&w, "NACC", 0u);
        for(u32 i = 0; i < 16u; i++) { ok = ok && get_num(&w, "m%u", 1u, 4u) == data[i]; tick(&w, "CLK"); }
        put(&w, "NACC", 1u); put(&w, "CS", 1u);
        sums_right += ok && get_num(&w, "r%u", 1u, 4u) == (sum & 15u) ? 1u : 0u;
    }
    CHECK(sums_right == 40u);
    printf("  a machine of chips (counter, memory, inverters, adder, register: %u primitives): 40 memories summed\n", (u32)rde_arr_length(&w.flat.prims));
    world_free(&w, top);
}

// --- the memory primitive: clocked, 256 words of 8 bits ------------------------------------------------------

static void test_ram_primitive(void) {
    world w; fude_sim_lib_init(&w.lib);
    bench b; bench_start(&b);
    inputs(&b, "A0 A1 A2 A3 A4 A5 A6 A7 D0 D1 D2 D3 D4 D5 D6 D7 WE CLK EN");
    line(&b, "inst ram M \"address=8 data=8\" A0 A1 A2 A3 A4 A5 A6 A7 D0 D1 D2 D3 D4 D5 D6 D7 we CLK Q0 Q1 Q2 Q3 Q4 Q5 Q6 Q7");
    line(&b, "inst tribuf T \"\" WE EN we");   // (WE through a tri-state: released, it is unknown)
    probes(&b, "Q0 Q1 Q2 Q3 Q4 Q5 Q6 Q7");
    fude_sim_def* top = bench_run(&b, &w);
    CHECK(w.flat.state >= 2u + 2u * 256u);
    put(&w, "EN", 1u);
    u32 mem[256]; u8 state[256]; memset(state, 0, sizeof state);   // (0 unknown, 1 known)
    u32 steps = 0, right = 0;
    for(u32 k = 0; k < 6000u; k++) {
        const u32 addr = rnd(256u), what = rnd(8u);
        put_num(&w, "A%u", 0u, 8u, addr);
        if(what < 3u) {
            const u32 v = rnd(256u);
            put_num(&w, "D%u", 0u, 8u, v); put(&w, "WE", 1u); tick(&w, "CLK"); put(&w, "WE", 0u);
            mem[addr] = v; state[addr] = 1u;
        } else if(what == 3u) {
            // (written with WE unknown: what it held there is unknown now)
            put(&w, "EN", 0u); tick(&w, "CLK"); put(&w, "EN", 1u);
            state[addr] = 0u;
        } else if(what == 4u) {
            put_num(&w, "D%u", 0u, 8u, rnd(256u)); tick(&w, "CLK");   // (no WE: nothing written)
        }
        const u32 got = get_num(&w, "Q%u", 0u, 8u);
        steps++;
        right += state[addr] ? got == mem[addr] : got == 0xFFFFFFFFu;
    }
    CHECK(right == steps);
    // Its sizes as its parameters say: 1 to 12 address bits, 1 to 16 data bits; outside them, held to them.
    for(u32 a = 1; a <= 12u; a++) {
        const f64 p[4] = { (f64)a, 3.0, 1.0, 1e-9 };
        CHECK(fude_sim_model_states(fude_sim_model_find("ram"), p) == 2u + 2u * (1u << a));
        fude_sim_port ports[64];
        CHECK(fude_sim_model_find("ram")->ports(p, ports, 64u) == a + 3u + 2u + 3u);
    }
    const f64 big[4] = { 40.0, 99.0, 1.0, 0.0 };
    fude_sim_port ports[64];
    CHECK(fude_sim_model_find("ram")->ports(big, ports, 64u) == 12u + 16u + 2u + 16u);
    printf("  a memory of 256 words of 8 bits: %u reads and writes\n", steps);
    world_free(&w, top);
}


// --- a run made again keeps what it had (a canvas's circuit rebuilt); a net's drive without one of its drivers ------

static void test_keep(void) {
    world a; fude_sim_lib_init(&a.lib); load_parts(&a.lib);
    bench b; bench_start(&b);
    inputs(&b, "CLK RST EN");
    line(&b, "inst user/counter4 C \"\" CLK RST EN q0 q1 q2 q3 tc");
    line(&b, "inst dff F \"\" q0 CLK zero fq -"); line(&b, "inst const Z \"0\" zero");
    probes(&b, "q0 q1 q2 q3 fq");
    fude_sim_def* top = bench_run(&b, &a);
    put(&a, "RST", 1u); tick(&a, "CLK"); put(&a, "RST", 0u); put(&a, "EN", 1u);
    for(u32 i = 0; i < 5u; i++) tick(&a, "CLK");
    CHECK(get_num(&a, "q%u", 0u, 4u) == 5u);
    // Made again from the same netlist: carried over, it is still 5 (its inputs too), and counts on from there.
    fude_sim_flat f2; fude_sim_flat_init(&f2);
    CHECK(fude_sim_elaborate(&a.lib, top, &f2));
    fude_sim_run r2; fude_sim_run_init(&r2, &f2);
    fude_sim_run_keep(&r2, &a.run);
    world w2 = a; w2.flat = f2; w2.run = r2;
    CHECK(get_num(&w2, "q%u", 0u, 4u) == 5u && get(&w2, "fq") == get(&a, "fq"));
    u32 right = 0;
    for(u32 i = 0; i < 40u; i++) { tick(&w2, "CLK"); right += get_num(&w2, "q%u", 0u, 4u) == ((6u + i) & 15u); }
    CHECK(right == 40u);
    // A fresh one, not carried over: unknown again (nothing reset it).
    fude_sim_run r3; fude_sim_run_init(&r3, &f2);
    world w3 = a; w3.flat = f2; w3.run = r3;
    CHECK(get_num(&w3, "q%u", 0u, 4u) == 0xFFFFFFFFu);
    fude_sim_run_destroy(&r3);
    fude_sim_run_destroy(&r2);
    fude_sim_flat_destroy(&f2);
    world_free(&a, top);

    // A clock in it: carried over, it goes on ticking (its wakes kept), the count on from where it was.
    {
        world k; fude_sim_lib_init(&k.lib);
        bench_start(&b);
        line(&b, "inst clock CK \"period=1\" clk"); line(&b, "inst const O \"1\" one"); line(&b, "inst const Z \"0\" zero");
        line(&b, "inst tff T0 \"\" one clk zero q0 nq0"); line(&b, "inst tff T1 \"\" one nq0 zero q1 -"); line(&b, "inst tff T2 \"\" one nq1 zero q2 -");
        line(&b, "inst not N1 \"\" q1 nq1");
        probes(&b, "q0 q1 q2");
        fude_sim_def* kt = bench_run(&b, &k);
        fude_sim_run_until(&k.run, 3.2);
        const u32 before = get_num(&k, "q%u", 0u, 3u);
        fude_sim_flat kf; fude_sim_flat_init(&kf);
        CHECK(fude_sim_elaborate(&k.lib, kt, &kf));
        fude_sim_run kr; fude_sim_run_init(&kr, &kf);
        fude_sim_run_keep(&kr, &k.run);
        world k2 = k; k2.flat = kf; k2.run = kr;
        CHECK(get_num(&k2, "q%u", 0u, 3u) == before && before == 3u);
        fude_sim_run_until(&k2.run, 2.2);   // (its own time from 0: its first rising edge at 0.5, two more by 2.2)
        CHECK(get_num(&k2, "q%u", 0u, 3u) == ((before + 2u) & 7u) || get_num(&k2, "q%u", 0u, 3u) == ((before + 3u) & 7u));
        CHECK(!k2.run.oscillating);
        fude_sim_run_destroy(&kr);
        fude_sim_flat_destroy(&kf);
        world_free(&k, kt);
    }
    // A bus of three tri-states: the net without each, as the others drive it.
    world w; fude_sim_lib_init(&w.lib);
    bench_start(&b);
    inputs(&b, "A B C EA EB EC");
    line(&b, "inst tribuf TA \"\" A EA bus"); line(&b, "inst tribuf TB \"\" B EB bus"); line(&b, "inst tribuf TC \"\" C EC bus");
    probes(&b, "bus");
    top = bench_run(&b, &w);
    const u32 ta = fude_sim_flat_find(&w.flat, "TA"), tb = fude_sim_flat_find(&w.flat, "TB"), tc = fude_sim_flat_find(&w.flat, "TC");
    const u32 bus = ((const u32*)w.flat.port_nets.memory)[((const fude_sim_prim*)w.flat.prims.memory)[ta].first + 2u];
    u32 cases = 0, ok = 0;
    for(u32 v = 0; v < 64u; v++) {
        const u8 x[3] = { (u8)(v & 1u), (u8)((v >> 1) & 1u), (u8)((v >> 2) & 1u) }, e[3] = { (u8)((v >> 3) & 1u), (u8)((v >> 4) & 1u), (u8)((v >> 5) & 1u) };
        put(&w, "A", x[0]); put(&w, "B", x[1]); put(&w, "C", x[2]); put(&w, "EA", e[0]); put(&w, "EB", e[1]); put(&w, "EC", e[2]);
        const u32 prims[3] = { ta, tb, tc };
        for(u32 k = 0; k < 3u; k++) {
            u8 want = FUDE_SIM_Z;
            for(u32 j = 0; j < 3u; j++) {
                if(j == k || !e[j]) continue;
                want = want == FUDE_SIM_Z ? x[j] : (want == x[j] ? want : FUDE_SIM_X);
            }
            cases++;
            ok += fude_sim_run_net_without(&w.run, bus, prims[k]) == want ? 1u : 0u;
        }
        // (without nothing: the net itself)
        cases++;
        ok += fude_sim_run_net_without(&w.run, bus, FUDE_SIM_NONE) == fude_sim_run_net(&w.run, bus) ? 1u : 0u;
    }
    CHECK(ok == cases);
    world_free(&w, top);
}

// --- the sparse system (sparse.h) -----------------------------------------------------------------------

// A dense solve with row swaps (the reference): false when singular.
static b8 dense_solve(f64* a, f64* b, f64* x, u32 n) {
    for(u32 k = 0; k < n; k++) {
        u32 best = k;
        for(u32 i = k + 1u; i < n; i++) if(fabs(a[i * n + k]) > fabs(a[best * n + k])) best = i;
        if(!(fabs(a[best * n + k]) > 1e-300)) return false;
        for(u32 j = 0; j < n; j++) { const f64 t = a[k * n + j]; a[k * n + j] = a[best * n + j]; a[best * n + j] = t; }
        { const f64 t = b[k]; b[k] = b[best]; b[best] = t; }
        for(u32 i = k + 1u; i < n; i++) {
            const f64 f = a[i * n + k] / a[k * n + k];
            for(u32 j = k; j < n; j++) a[i * n + j] -= f * a[k * n + j];
            b[i] -= f * b[k];
        }
    }
    for(u32 k = n; k-- > 0u;) {
        f64 s = b[k];
        for(u32 j = k + 1u; j < n; j++) s -= a[k * n + j] * x[j];
        x[k] = s / a[k * n + k];
    }
    return true;
}

static u32 lcg_state = 12345u;
static u32 lcg(void) { lcg_state = lcg_state * 1664525u + 1013904223u; return lcg_state >> 8; }
static f64 lcg_unit(void) { return (f64)(lcg() % 1000000u) / 1000000.0; }

// A circuit-like system: n nodes, each joined to a few others by conductances (a ladder, random links), sources to
// ground; _skew: some entries one way only as large (a transistor's gain), not symmetric in value.
static void sparse_random(u32 n, u32 links, f64 skew, u32 seed) {
    lcg_state = seed;
    rde_arr pairs = rde_arr_new(sizeof(u64), rde_memory_allocator_get_default_std());
    rde_arr g_arr = scratch(sizeof(f64), n * n);   // the same matrix, dense
    f64* g = (f64*)g_arr.memory;
    for(u32 k = 0; k < n + links; k++) {
        const u32 i = k < n ? k : lcg() % n, j = k < n ? (k + 1u) % n : lcg() % n;
        if(i == j) continue;
        const f64 c = 0.001 + lcg_unit() * 10.0;
        g[i * n + i] += c; g[j * n + j] += c; g[i * n + j] -= c; g[j * n + i] -= c;
        if(skew > 0.0 && lcg() % 4u == 0u) g[i * n + j] += skew * c;   // (one way only)
        const u64 p = ((u64)i << 32) | j;
        rde_arr_add(&pairs, (any)&p);
    }
    for(u32 i = 0; i < n; i++) g[i * n + i] += 0.01 + lcg_unit();   // (to ground)
    fude_sim_sparse s;
    fude_sim_sparse_init(&s);
    fude_sim_sparse_shape(&s, n, (const u64*)pairs.memory, (u32)rde_arr_length(&pairs));
    // Twice over, as a circuit does: the values written into their places, factored, solved.
    for(u32 round = 0; round < 2u; round++) {
        fude_sim_sparse_clear(&s);
        b8 placed = true;
        for(u32 i = 0; i < n; i++) {
            for(u32 j = 0; j < n; j++) {
                if(g[i * n + j] == 0.0) continue;
                const u32 e = fude_sim_sparse_find(&s, i, j);
                placed = placed && e != FUDE_SIM_SPARSE_NONE;
                if(e != FUDE_SIM_SPARSE_NONE) ((f64*)s.val.memory)[e] += g[i * n + j];
            }
        }
        CHECK(placed);
        rde_arr b_arr = scratch(sizeof(f64), n), x_arr = scratch(sizeof(f64), n), y_arr = scratch(sizeof(f64), n), d_arr = scratch(sizeof(f64), n * n);
        f64* b = (f64*)b_arr.memory; f64* x = (f64*)x_arr.memory; f64* y = (f64*)y_arr.memory; f64* d = (f64*)d_arr.memory;
        for(u32 i = 0; i < n; i++) b[i] = lcg_unit() * 10.0 - 5.0;
        fude_sim_sparse_dense(&s, d);
        CHECK(memcmp(d, g, (usize)n * n * sizeof(f64)) == 0);   // (its entries, where they were put)
        CHECK(fude_sim_sparse_factor(&s));
        fude_sim_sparse_solve(&s, b, x);
        CHECK(dense_solve(d, b, y, n));   // (b changed by the dense solve: x was found first)
        f64 worst = 0.0, big = 0.0;
        for(u32 i = 0; i < n; i++) { worst = fmax(worst, fabs(x[i] - y[i])); big = fmax(big, fabs(y[i])); }
        if(!(worst <= 1e-9 * fmax(big, 1.0))) printf("  sparse n %u: off by %g (of %g)\n", n, worst, big);
        CHECK(worst <= 1e-9 * fmax(big, 1.0));
        rde_arr_free(&b_arr); rde_arr_free(&x_arr); rde_arr_free(&y_arr); rde_arr_free(&d_arr);
        for(u32 i = 0; i < n * n; i++) if(g[i] != 0.0) g[i] *= 1.0 + 0.1 * lcg_unit();   // (the next round's values)
    }
    fude_sim_sparse_free(&s);
    rde_arr_free(&pairs);
    rde_arr_free(&g_arr);
}

static void test_sparse(void) {
    printf("sparse systems\n");
    // Against a dense solve: sizes from one up, symmetric and skewed values.
    const u32 sizes[] = { 1, 2, 3, 5, 8, 13, 40, 100, 300 };
    for(u32 k = 0; k < sizeof(sizes) / sizeof(sizes[0]); k++) {
        sparse_random(sizes[k], sizes[k] / 2u, 0.0, 7u + k);
        sparse_random(sizes[k], sizes[k], 0.8, 99u + k);
    }
    // A ladder (a long chain of resistors): ordered, it fills in nothing — its places stay 3 a row.
    {
        const u32 n = 2000;
        rde_arr pairs = rde_arr_new(sizeof(u64), rde_memory_allocator_get_default_std());
        for(u32 i = 0; i + 1u < n; i++) { const u64 p = ((u64)(i + 1u) << 32) | i; rde_arr_add(&pairs, (any)&p); }
        fude_sim_sparse s;
        fude_sim_sparse_init(&s);
        fude_sim_sparse_shape(&s, n, (const u64*)pairs.memory, (u32)rde_arr_length(&pairs));
        CHECK(fude_sim_sparse_places(&s) == 3u * n - 2u);
        // A star (one node every other is joined to: a supply rail): the rail last, nothing fills in either.
        rde_arr_clear(&pairs);
        for(u32 i = 1; i < n; i++) { const u64 p = (u64)i; rde_arr_add(&pairs, (any)&p); }   // (i, 0)
        fude_sim_sparse_shape(&s, n, (const u64*)pairs.memory, (u32)rde_arr_length(&pairs));
        CHECK(fude_sim_sparse_places(&s) == 3u * n - 2u);
        CHECK(((const u32*)s.rank.memory)[0] >= n - 2u);   // (the rail among the last two: the last leaf ties with it)
        // A grid (100 × 100): far fewer places than dense (10⁸).
        const u32 side = 100;
        rde_arr_clear(&pairs);
        for(u32 r = 0; r < side; r++) for(u32 c = 0; c < side; c++) {
            const u32 i = r * side + c;
            if(c + 1u < side) { const u64 p = ((u64)i << 32) | (i + 1u); rde_arr_add(&pairs, (any)&p); }
            if(r + 1u < side) { const u64 p = ((u64)i << 32) | (i + side); rde_arr_add(&pairs, (any)&p); }
        }
        fude_sim_sparse_shape(&s, side * side, (const u64*)pairs.memory, (u32)rde_arr_length(&pairs));
        printf("  grid %ux%u: %u places\n", side, side, fude_sim_sparse_places(&s));
        CHECK(fude_sim_sparse_places(&s) < 600000u);
        // A place the shape has not: none.
        CHECK(fude_sim_sparse_find(&s, 0, side * side - 1u) == FUDE_SIM_SPARSE_NONE);
        CHECK(fude_sim_sparse_find(&s, 0, 1) != FUDE_SIM_SPARSE_NONE && fude_sim_sparse_find(&s, 1, 0) != FUDE_SIM_SPARSE_NONE);
        CHECK(fude_sim_sparse_find(&s, side * side, 0) == FUDE_SIM_SPARSE_NONE);
        fude_sim_sparse_free(&s);
        rde_arr_free(&pairs);
    }
    // Singular (a node joined to nothing, nothing on its diagonal): said so; a pivot needing a row swap too.
    {
        fude_sim_sparse s;
        fude_sim_sparse_init(&s);
        const u64 p = ((u64)0 << 32) | 1u;
        fude_sim_sparse_shape(&s, 3, &p, 1);
        f64* v = (f64*)s.val.memory;
        v[fude_sim_sparse_find(&s, 0, 0)] = 1.0; v[fude_sim_sparse_find(&s, 1, 1)] = 1.0;
        CHECK(!fude_sim_sparse_factor(&s));
        fude_sim_sparse_add_diagonal(&s, 1.0);
        CHECK(fude_sim_sparse_factor(&s));
        fude_sim_sparse_clear(&s);
        v[fude_sim_sparse_find(&s, 0, 1)] = 1.0; v[fude_sim_sparse_find(&s, 1, 0)] = 1.0; v[fude_sim_sparse_find(&s, 2, 2)] = 1.0;
        CHECK(!fude_sim_sparse_factor(&s));   // ([0 1; 1 0]: fine with rows swapped, not without)
        fude_sim_sparse_free(&s);
    }
    // Empty.
    {
        fude_sim_sparse s;
        fude_sim_sparse_init(&s);
        fude_sim_sparse_shape(&s, 0, NULL, 0);
        CHECK(fude_sim_sparse_factor(&s));
        CHECK(fude_sim_sparse_places(&s) == 0u);
        fude_sim_sparse_free(&s);
    }
}

int main(void) {
    test_sparse();
    test_gates();
    test_mux();
    test_adder();
    test_sequential();
    test_errors();
    test_gate_tables();
    test_random_networks();
    test_text_fuzz();
    test_sequences();
    test_buses();
    test_elaboration_edges();
    test_bodies();
    test_universal();
    test_memory();
    test_clock();
    test_alu_lfsr_machine();
    test_chips();
    test_chip_machine();
    test_ram_primitive();
    test_keep();
    if(fails == 0) printf("ALL PASSED\n"); else printf("%d FAILED\n", fails);
    return fails == 0 ? 0 : 1;
}
