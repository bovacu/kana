// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "sim/sim.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

// ===========================================================================
// The logic family's models: what custom parts are built from (a multiplexer
// of gates, an adder of adders). Values 0, 1, Z, X; an input released (Z)
// reads as unknown; each output changes its delay after what makes it
// (inertial: a change undone before then never happens). Flip-flops take
// their input on the clock's rising edge; R (when 1) clears them at once (S,
// where there is one, sets them). A memory keeps 2^address words of `data`
// bits, unknown until written: written on its clock's rising edge while WE
// is 1 (clocked), or all the while WE is 1 (not: a static RAM chip's way),
// its addressed word always out.
// ===========================================================================

#define FSL_DELAY 1e-9   // a gate's delay as it comes (seconds)

RDE_INTERNAL fude_sim_port fsl_port(const c8* _name, u8 _dir) {
    fude_sim_port _p;
    memset(&_p, 0, sizeof(_p));
    snprintf(_p.name, sizeof(_p.name), "%s", _name);
    _p.domain = FUDE_SIM_LOGIC;
    _p.dir    = _dir;
    _p.width  = 1u;
    _p.side   = _dir == FUDE_SIM_OUT ? FUDE_SIM_RIGHT : FUDE_SIM_LEFT;
    return _p;
}

// Ports as named in _names ("A B > Y": those after ">" outputs), into _out.
RDE_INTERNAL u32 fsl_ports(const c8* _names, fude_sim_port* _out, u32 _max) {
    c8 _buf[96];
    snprintf(_buf, sizeof(_buf), "%s", _names);
    u32 _n = 0;
    u8 _dir = FUDE_SIM_IN;
    for(c8* _t = strtok(_buf, " "); _t != NULL && _n < _max; _t = strtok(NULL, " ")) {
        if(strcmp(_t, ">") == 0) {
            _dir = FUDE_SIM_OUT;
            continue;
        }
        _out[_n++] = fsl_port(_t, _dir);
    }
    return _n;
}

RDE_INTERNAL u8 fsl_known(u8 _v) {
    return _v == FUDE_SIM_0 || _v == FUDE_SIM_1 ? _v : FUDE_SIM_X;   // (Z read as unknown)
}

RDE_INTERNAL u8 fsl_not(u8 _v) {
    _v = fsl_known(_v);
    return _v == FUDE_SIM_X ? FUDE_SIM_X : (u8)(1u - _v);
}

// --- input, constant, probe, clock ------------------------------------------------------------------

RDE_INTERNAL const fude_sim_param FSL_VALUE[] = { { "value", "", 0.0, 0.0, 1.0 } };
RDE_INTERNAL u32 fsl_ports_y(const f64* _p, fude_sim_port* _o, u32 _m) { RDE_UNUSED(_p); return fsl_ports("> Y", _o, _m); }
RDE_INTERNAL u32 fsl_ports_a(const f64* _p, fude_sim_port* _o, u32 _m) { RDE_UNUSED(_p); return fsl_ports("A", _o, _m); }

RDE_INTERNAL void fsl_input_start(fude_sim_run* _run, u32 _prim) {
    const u8 _v = fude_sim_arg(_run, _prim, 0) >= 0.5 ? FUDE_SIM_1 : FUDE_SIM_0;
    fude_sim_state(_run, _prim)[0] = (f64)_v;
    fude_sim_out(_run, _prim, 0u, _v, 0.0);
}

RDE_INTERNAL const fude_sim_param FSL_CLOCK[] = { { "period", "s", 1.0, 1e-9, 1e6 }, { "duty", "", 0.5, 0.01, 0.99 } };

RDE_INTERNAL void fsl_clock_start(fude_sim_run* _run, u32 _prim) {
    fude_sim_state(_run, _prim)[0] = 0.0;
    fude_sim_out(_run, _prim, 0u, FUDE_SIM_0, 0.0);
    fude_sim_wake(_run, _prim, fude_sim_arg(_run, _prim, 0) * (1.0 - fude_sim_arg(_run, _prim, 1)));
}

RDE_INTERNAL void fsl_clock_wake(fude_sim_run* _run, u32 _prim) {
    f64* _s = fude_sim_state(_run, _prim);
    _s[0] = _s[0] >= 0.5 ? 0.0 : 1.0;
    fude_sim_out(_run, _prim, 0u, _s[0] >= 0.5 ? FUDE_SIM_1 : FUDE_SIM_0, 0.0);
    const f64 _period = fude_sim_arg(_run, _prim, 0), _duty = fude_sim_arg(_run, _prim, 1);
    fude_sim_wake(_run, _prim, _period * (_s[0] >= 0.5 ? _duty : 1.0 - _duty));
}

// --- gates -------------------------------------------------------------------------------------------

RDE_INTERNAL const fude_sim_param FSL_DELAY_ONLY[] = { { "delay", "s", FSL_DELAY, 0.0, 1.0 } };
RDE_INTERNAL const fude_sim_param FSL_GATE[] = { { "inputs", "", 2.0, 1.0, 16.0 }, { "delay", "s", FSL_DELAY, 0.0, 1.0 } };

RDE_INTERNAL u32 fsl_gate_ports(const f64* _p, fude_sim_port* _out, u32 _max) {
    const u32 _n = (u32)fmin(fmax(_p[0], 1.0), 16.0);
    u32 _k = 0;
    for(u32 _i = 0; _i < _n && _k < _max; _i++) {
        c8 _name[8];
        snprintf(_name, sizeof(_name), "A%u", _i + 1u);
        _out[_k++] = fsl_port(_name, FUDE_SIM_IN);
    }
    if(_k < _max) {
        _out[_k++] = fsl_port("Y", FUDE_SIM_OUT);
    }
    return _k;
}

typedef enum { FSL_AND = 0, FSL_OR, FSL_XOR } FSL_GATE_;

// A gate's output: its inputs (all but its last port) put together, turned over when it is a NAND, NOR, XNOR.
RDE_INTERNAL void fsl_gate(fude_sim_run* _run, u32 _prim, u8 _kind, b8 _invert) {
    const u32 _n = fude_sim_port_count(_run, _prim) - 1u;
    u8 _v;
    if(_kind == FSL_XOR) {
        u32 _ones = 0;
        _v = FUDE_SIM_0;
        for(u32 _i = 0; _i < _n; _i++) {
            const u8 _a = fsl_known(fude_sim_in(_run, _prim, _i));
            if(_a == FUDE_SIM_X) {
                _v = FUDE_SIM_X;
                break;
            }
            _ones += _a;
        }
        _v = _v == FUDE_SIM_X ? FUDE_SIM_X : (u8)(_ones & 1u);
    } else {
        // (AND: a 0 decides it; OR: a 1)
        const u8 _decides = _kind == FSL_AND ? FUDE_SIM_0 : FUDE_SIM_1;
        b8 _unknown = false, _decided = false;
        for(u32 _i = 0; _i < _n; _i++) {
            const u8 _a = fsl_known(fude_sim_in(_run, _prim, _i));
            _decided = _decided || _a == _decides;
            _unknown = _unknown || _a == FUDE_SIM_X;
        }
        _v = _decided ? _decides : (_unknown ? FUDE_SIM_X : (u8)(1u - _decides));
    }
    fude_sim_out(_run, _prim, _n, _invert ? fsl_not(_v) : _v, fude_sim_arg(_run, _prim, 1));
}

RDE_INTERNAL void fsl_and(fude_sim_run* _r, u32 _p)  { fsl_gate(_r, _p, FSL_AND, false); }
RDE_INTERNAL void fsl_nand(fude_sim_run* _r, u32 _p) { fsl_gate(_r, _p, FSL_AND, true); }
RDE_INTERNAL void fsl_or(fude_sim_run* _r, u32 _p)   { fsl_gate(_r, _p, FSL_OR, false); }
RDE_INTERNAL void fsl_nor(fude_sim_run* _r, u32 _p)  { fsl_gate(_r, _p, FSL_OR, true); }
RDE_INTERNAL void fsl_xor(fude_sim_run* _r, u32 _p)  { fsl_gate(_r, _p, FSL_XOR, false); }
RDE_INTERNAL void fsl_xnor(fude_sim_run* _r, u32 _p) { fsl_gate(_r, _p, FSL_XOR, true); }

RDE_INTERNAL u32 fsl_ports_ay(const f64* _p, fude_sim_port* _o, u32 _m) { RDE_UNUSED(_p); return fsl_ports("A > Y", _o, _m); }
RDE_INTERNAL void fsl_buf(fude_sim_run* _r, u32 _p) { fude_sim_out(_r, _p, 1u, fsl_known(fude_sim_in(_r, _p, 0u)), fude_sim_arg(_r, _p, 0)); }
RDE_INTERNAL void fsl_inv(fude_sim_run* _r, u32 _p) { fude_sim_out(_r, _p, 1u, fsl_not(fude_sim_in(_r, _p, 0u)), fude_sim_arg(_r, _p, 0)); }

// A tri-state buffer: A through while EN is 1; released (Z) while it is 0.
RDE_INTERNAL u32 fsl_ports_tri(const f64* _p, fude_sim_port* _o, u32 _m) { RDE_UNUSED(_p); return fsl_ports("A EN > Y", _o, _m); }
RDE_INTERNAL void fsl_tri(fude_sim_run* _r, u32 _p) {
    const u8 _en = fsl_known(fude_sim_in(_r, _p, 1u));
    fude_sim_out(_r, _p, 2u, _en == FUDE_SIM_1 ? fsl_known(fude_sim_in(_r, _p, 0u)) : (_en == FUDE_SIM_0 ? FUDE_SIM_Z : FUDE_SIM_X), fude_sim_arg(_r, _p, 0));
}

// --- flip-flops and a latch ---------------------------------------------------------------------------

// Q and QN out (their ports _q, _q + 1) as state[0] says.
RDE_INTERNAL void fsl_q(fude_sim_run* _r, u32 _p, u32 _q) {
    const u8 _v = (u8)fude_sim_state(_r, _p)[0];
    fude_sim_out(_r, _p, _q, _v, fude_sim_arg(_r, _p, 0));
    fude_sim_out(_r, _p, _q + 1u, fsl_not(_v), fude_sim_arg(_r, _p, 0));
}

RDE_INTERNAL void fsl_ff_start(fude_sim_run* _r, u32 _p) {
    f64* _s = fude_sim_state(_r, _p);
    _s[0] = FUDE_SIM_0;
    _s[1] = FUDE_SIM_X;   // (its clock as last seen)
    fsl_q(_r, _p, fude_sim_port_count(_r, _p) - 2u);
}

// A clock's rising edge now? (its port _clk; its last value kept in state[1])
RDE_INTERNAL b8 fsl_rose(fude_sim_run* _r, u32 _p, u32 _clk) {
    f64* _s = fude_sim_state(_r, _p);
    const u8 _now = fsl_known(fude_sim_in(_r, _p, _clk));
    const b8 _rose = (u8)_s[1] == FUDE_SIM_0 && _now == FUDE_SIM_1;
    _s[1] = (f64)_now;
    return _rose;
}

RDE_INTERNAL u32 fsl_ports_dff(const f64* _p, fude_sim_port* _o, u32 _m) { RDE_UNUSED(_p); return fsl_ports("D CLK R > Q QN", _o, _m); }
RDE_INTERNAL void fsl_dff(fude_sim_run* _r, u32 _p) {
    f64* _s = fude_sim_state(_r, _p);
    const b8 _rose = fsl_rose(_r, _p, 1u);
    if(fsl_known(fude_sim_in(_r, _p, 2u)) == FUDE_SIM_1) {
        _s[0] = FUDE_SIM_0;
    } else if(_rose) {
        _s[0] = fsl_known(fude_sim_in(_r, _p, 0u));
    }
    fsl_q(_r, _p, 3u);
}

RDE_INTERNAL u32 fsl_ports_jk(const f64* _p, fude_sim_port* _o, u32 _m) { RDE_UNUSED(_p); return fsl_ports("J K CLK R > Q QN", _o, _m); }
RDE_INTERNAL void fsl_jk(fude_sim_run* _r, u32 _p) {
    f64* _s = fude_sim_state(_r, _p);
    const b8 _rose = fsl_rose(_r, _p, 2u);
    if(fsl_known(fude_sim_in(_r, _p, 3u)) == FUDE_SIM_1) {
        _s[0] = FUDE_SIM_0;
    } else if(_rose) {
        const u8 _j = fsl_known(fude_sim_in(_r, _p, 0u)), _k = fsl_known(fude_sim_in(_r, _p, 1u));
        if(_j == FUDE_SIM_X || _k == FUDE_SIM_X) {
            _s[0] = FUDE_SIM_X;
        } else if(_j == FUDE_SIM_1 && _k == FUDE_SIM_1) {
            _s[0] = fsl_not((u8)_s[0]);
        } else if(_j == FUDE_SIM_1) {
            _s[0] = FUDE_SIM_1;
        } else if(_k == FUDE_SIM_1) {
            _s[0] = FUDE_SIM_0;
        }
    }
    fsl_q(_r, _p, 4u);
}

RDE_INTERNAL u32 fsl_ports_t(const f64* _p, fude_sim_port* _o, u32 _m) { RDE_UNUSED(_p); return fsl_ports("T CLK R > Q QN", _o, _m); }
RDE_INTERNAL void fsl_t(fude_sim_run* _r, u32 _p) {
    f64* _s = fude_sim_state(_r, _p);
    const b8 _rose = fsl_rose(_r, _p, 1u);
    if(fsl_known(fude_sim_in(_r, _p, 2u)) == FUDE_SIM_1) {
        _s[0] = FUDE_SIM_0;
    } else if(_rose) {
        const u8 _t = fsl_known(fude_sim_in(_r, _p, 0u));
        _s[0] = _t == FUDE_SIM_X ? FUDE_SIM_X : (_t == FUDE_SIM_1 ? fsl_not((u8)_s[0]) : _s[0]);
    }
    fsl_q(_r, _p, 3u);
}

// A D flip-flop with set and reset (a 74HC74's half, its pins' sense turned): S or R at 1 now, whatever its clock
// does; both: Q and QN both 1, as the chip does.
RDE_INTERNAL u32 fsl_ports_dffsr(const f64* _p, fude_sim_port* _o, u32 _m) { RDE_UNUSED(_p); return fsl_ports("D CLK S R > Q QN", _o, _m); }
RDE_INTERNAL void fsl_dffsr(fude_sim_run* _r, u32 _p) {
    f64* _s = fude_sim_state(_r, _p);
    const b8 _rose = fsl_rose(_r, _p, 1u);
    const u8 _set = fsl_known(fude_sim_in(_r, _p, 2u)), _reset = fsl_known(fude_sim_in(_r, _p, 3u));
    if(_set == FUDE_SIM_1 && _reset == FUDE_SIM_1) {
        _s[0] = FUDE_SIM_X;
        fude_sim_out(_r, _p, 4u, FUDE_SIM_1, fude_sim_arg(_r, _p, 0));
        fude_sim_out(_r, _p, 5u, FUDE_SIM_1, fude_sim_arg(_r, _p, 0));
        return;
    }
    if(_set == FUDE_SIM_1) {
        _s[0] = FUDE_SIM_1;
    } else if(_reset == FUDE_SIM_1) {
        _s[0] = FUDE_SIM_0;
    } else if(_set == FUDE_SIM_X || _reset == FUDE_SIM_X) {
        _s[0] = FUDE_SIM_X;
    } else if(_rose) {
        _s[0] = fsl_known(fude_sim_in(_r, _p, 0u));
    }
    fsl_q(_r, _p, 4u);
}

RDE_INTERNAL u32 fsl_ports_sr(const f64* _p, fude_sim_port* _o, u32 _m) { RDE_UNUSED(_p); return fsl_ports("S R > Q QN", _o, _m); }
RDE_INTERNAL void fsl_sr_start(fude_sim_run* _r, u32 _p) {
    fude_sim_state(_r, _p)[0] = FUDE_SIM_0;
    fsl_q(_r, _p, 2u);
}
RDE_INTERNAL void fsl_sr(fude_sim_run* _r, u32 _p) {
    f64* _s = fude_sim_state(_r, _p);
    const u8 _set = fsl_known(fude_sim_in(_r, _p, 0u)), _reset = fsl_known(fude_sim_in(_r, _p, 1u));
    if(_set == FUDE_SIM_1 && _reset == FUDE_SIM_1) {
        _s[0] = FUDE_SIM_X;
    } else if(_set == FUDE_SIM_1) {
        _s[0] = FUDE_SIM_1;
    } else if(_reset == FUDE_SIM_1) {
        _s[0] = FUDE_SIM_0;
    }
    fsl_q(_r, _p, 2u);
}

// --- memory ------------------------------------------------------------------------------------------

RDE_INTERNAL const fude_sim_param FSL_RAM[] = { { "address", "", 4.0, 1.0, 12.0 }, { "data", "", 4.0, 1.0, 16.0 }, { "clocked", "", 1.0, 0.0, 1.0 },
                                                { "delay", "s", FSL_DELAY, 0.0, 1.0 } };

RDE_INTERNAL u32 fsl_ram_bits(const f64* _p, u32 _k, u32 _hi) {
    return (u32)fmin(fmax(round(_p[_k]), 1.0), (f64)_hi);
}

// Its ports: A0… (its address), D0… (what is written), WE, CLK, then Q0… (the word addressed).
RDE_INTERNAL u32 fsl_ports_ram(const f64* _p, fude_sim_port* _out, u32 _max) {
    const u32 _a = fsl_ram_bits(_p, 0u, 12u), _d = fsl_ram_bits(_p, 1u, 16u);
    u32 _k = 0;
    c8 _name[8];
    for(u32 _i = 0; _i < _a && _k < _max; _i++) { snprintf(_name, sizeof(_name), "A%u", _i); _out[_k++] = fsl_port(_name, FUDE_SIM_IN); }
    for(u32 _i = 0; _i < _d && _k < _max; _i++) { snprintf(_name, sizeof(_name), "D%u", _i); _out[_k++] = fsl_port(_name, FUDE_SIM_IN); }
    if(_k < _max) { _out[_k++] = fsl_port("WE", FUDE_SIM_IN); }
    if(_k < _max) { _out[_k++] = fsl_port("CLK", FUDE_SIM_IN); }
    for(u32 _i = 0; _i < _d && _k < _max; _i++) { snprintf(_name, sizeof(_name), "Q%u", _i); _out[_k++] = fsl_port(_name, FUDE_SIM_OUT); }
    return _k;
}

// Kept: [0] unused, [1] its clock as last seen, then each word's bits and which of them are known (all unknown: 0s).
RDE_INTERNAL u32 fsl_ram_states(const f64* _p) {
    return 2u + 2u * (1u << fsl_ram_bits(_p, 0u, 12u));
}

RDE_INTERNAL void fsl_ram_start(fude_sim_run* _r, u32 _p) {
    fude_sim_state(_r, _p)[1] = FUDE_SIM_X;
}

// _n bits from port _first on, as a number (false: one of them unknown); unknown ones' bits into _unknown.
RDE_INTERNAL b8 fsl_bits(const fude_sim_run* _r, u32 _p, u32 _first, u32 _n, u32* _value, u32* _unknown) {
    *_value = 0u;
    *_unknown = 0u;
    for(u32 _i = 0; _i < _n; _i++) {
        const u8 _v = fsl_known(fude_sim_in(_r, _p, _first + _i));
        if(_v == FUDE_SIM_X) {
            *_unknown |= 1u << _i;
        } else {
            *_value |= (u32)_v << _i;
        }
    }
    return *_unknown == 0u;
}

RDE_INTERNAL void fsl_ram(fude_sim_run* _r, u32 _p) {
    f64* _s = fude_sim_state(_r, _p);
    f64 _par[4];
    for(u32 _k = 0; _k < 4u; _k++) {
        _par[_k] = fude_sim_arg(_r, _p, _k);
    }
    const u32 _a = fsl_ram_bits(_par, 0u, 12u), _d = fsl_ram_bits(_par, 1u, 16u), _words = 1u << _a;
    const u32 _we = _a + _d, _clk = _we + 1u, _q = _clk + 1u;
    const u32 _mask = _d >= 32u ? 0xFFFFFFFFu : (1u << _d) - 1u;
    u32 _addr, _addr_x, _data, _data_x;
    const b8 _addr_known = fsl_bits(_r, _p, 0u, _a, &_addr, &_addr_x);
    fsl_bits(_r, _p, _a, _d, &_data, &_data_x);
    const u8 _write = fsl_known(fude_sim_in(_r, _p, _we));
    const b8 _clocked = _par[2] >= 0.5;
    const b8 _rose = _clocked && fsl_rose(_r, _p, _clk);
    const b8 _now = _clocked ? _rose : true;   // (a moment it may write)
    if(_now && _write != FUDE_SIM_0) {
        // (written: what is known of it — or, not knowing whether or where, whatever it may have touched unknown)
        const b8 _sure = _write == FUDE_SIM_1;
        for(u32 _w = 0; _w < _words; _w++) {
            if(_addr_known && _w != _addr) {
                continue;
            }
            f64* _word = &_s[2u + 2u * _w];
            if(_sure && _addr_known) {
                _word[0] = (f64)(_data & _mask);
                _word[1] = (f64)(~_data_x & _mask);
            } else {
                _word[1] = 0.0;
            }
        }
    }
    const f64 _delay = _par[3];
    for(u32 _i = 0; _i < _d; _i++) {
        u8 _v = FUDE_SIM_X;
        if(_addr_known) {
            const f64* _word = &_s[2u + 2u * _addr];
            const u32 _bits = (u32)_word[0], _known = (u32)_word[1];
            _v = (_known >> _i) & 1u ? (u8)((_bits >> _i) & 1u) : FUDE_SIM_X;
        }
        fude_sim_out(_r, _p, _q + _i, _v, _delay);
    }
}

// --- the registry -----------------------------------------------------------------------------------

#define FSL_N(_a) ((u32)(sizeof(_a) / sizeof((_a)[0])))
RDE_INTERNAL const fude_sim_model FSL_MODELS[] = {
    { "input",  "Logic input",  FSL_VALUE, 1u, 1u, fsl_ports_y, fsl_input_start, NULL, NULL, NULL },
    { "const",  "Constant",     FSL_VALUE, 1u, 1u, fsl_ports_y, fsl_input_start, NULL, NULL, NULL },
    { "probe",  "Logic probe",  NULL, 0u, 0u, fsl_ports_a, NULL, NULL, NULL, NULL },
    { "clock",  "Clock",        FSL_CLOCK, FSL_N(FSL_CLOCK), 1u, fsl_ports_y, fsl_clock_start, NULL, fsl_clock_wake, NULL },
    { "buf",    "Buffer",       FSL_DELAY_ONLY, 1u, 0u, fsl_ports_ay, NULL, fsl_buf, NULL, NULL },
    { "not",    "NOT gate",     FSL_DELAY_ONLY, 1u, 0u, fsl_ports_ay, NULL, fsl_inv, NULL, NULL },
    { "and",    "AND gate",     FSL_GATE, FSL_N(FSL_GATE), 0u, fsl_gate_ports, NULL, fsl_and, NULL, NULL },
    { "or",     "OR gate",      FSL_GATE, FSL_N(FSL_GATE), 0u, fsl_gate_ports, NULL, fsl_or, NULL, NULL },
    { "nand",   "NAND gate",    FSL_GATE, FSL_N(FSL_GATE), 0u, fsl_gate_ports, NULL, fsl_nand, NULL, NULL },
    { "nor",    "NOR gate",     FSL_GATE, FSL_N(FSL_GATE), 0u, fsl_gate_ports, NULL, fsl_nor, NULL, NULL },
    { "xor",    "XOR gate",     FSL_GATE, FSL_N(FSL_GATE), 0u, fsl_gate_ports, NULL, fsl_xor, NULL, NULL },
    { "xnor",   "XNOR gate",    FSL_GATE, FSL_N(FSL_GATE), 0u, fsl_gate_ports, NULL, fsl_xnor, NULL, NULL },
    { "tribuf", "Tri-state buffer", FSL_DELAY_ONLY, 1u, 0u, fsl_ports_tri, NULL, fsl_tri, NULL, NULL },
    { "dff",    "D flip-flop",  FSL_DELAY_ONLY, 1u, 2u, fsl_ports_dff, fsl_ff_start, fsl_dff, NULL, NULL },
    { "jkff",   "JK flip-flop", FSL_DELAY_ONLY, 1u, 2u, fsl_ports_jk, fsl_ff_start, fsl_jk, NULL, NULL },
    { "tff",    "T flip-flop",  FSL_DELAY_ONLY, 1u, 2u, fsl_ports_t, fsl_ff_start, fsl_t, NULL, NULL },
    { "srlatch", "SR latch",    FSL_DELAY_ONLY, 1u, 1u, fsl_ports_sr, fsl_sr_start, fsl_sr, NULL, NULL },
    { "dffsr",  "D flip-flop with set and reset", FSL_DELAY_ONLY, 1u, 2u, fsl_ports_dffsr, fsl_ff_start, fsl_dffsr, NULL, NULL },
    { "ram",    "Memory",       FSL_RAM, FSL_N(FSL_RAM), 0u, fsl_ports_ram, fsl_ram_start, fsl_ram, NULL, fsl_ram_states },
};

u32 fude_sim_model_count(void) {
    return FSL_N(FSL_MODELS);
}

const fude_sim_model* fude_sim_model_at(u32 _i) {
    return _i < fude_sim_model_count() ? &FSL_MODELS[_i] : NULL;
}

u32 fude_sim_model_states(const fude_sim_model* _model, const f64* _params) {
    if(_model == NULL) {
        return 0u;
    }
    return _model->states != NULL ? _model->states(_params) : _model->state;
}

const fude_sim_model* fude_sim_model_find(const c8* _id) {
    for(u32 _i = 0; _id != NULL && _i < fude_sim_model_count(); _i++) {
        if(strcmp(FSL_MODELS[_i].id, _id) == 0) {
            return &FSL_MODELS[_i];
        }
    }
    return NULL;
}
