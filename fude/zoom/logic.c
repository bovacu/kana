// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/logic.h"
#include "zoom/symbol.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// --- the library ---------------------------------------------------------------------------------

RDE_INTERNAL fude_sim_lib fzl_lib;
RDE_INTERNAL b8           fzl_lib_made = false;

fude_sim_lib* fude_zoom_logic_library(void) {
    if(!fzl_lib_made) {
        fude_sim_lib_init(&fzl_lib);
        fude_sim_chips_add(&fzl_lib);
        fzl_lib_made = true;
    }
    return &fzl_lib;
}

const fude_sim_def* fude_zoom_logic_learn(const c8* _text, usize _size, c8* _error, usize _error_size) {
    fude_sim_def* _def = fude_sim_def_read(_text, _size, _error, _error_size);
    if(_def == NULL) {
        return NULL;
    }
    fude_sim_lib* _lib = fude_zoom_logic_library();
    const u32 _at = fude_sim_lib_add(_lib, _def);
    return ((fude_sim_def* const*)_lib->defs.memory)[_at];
}

const fude_sim_def* fude_zoom_logic_find(const c8* _text) {
    if(_text == NULL || _text[0] == 0) {
        return NULL;
    }
    // (its first line, its ends' spaces off)
    c8 _first[FUDE_SIM_NAME + FUDE_SIM_ID];
    usize _n = 0;
    while(_text[_n] != 0 && _text[_n] != '\n' && _n + 1u < sizeof(_first)) {
        _first[_n] = _text[_n];
        _n++;
    }
    while(_n > 0u && (_first[_n - 1u] == ' ' || _first[_n - 1u] == '\r')) {
        _n--;
    }
    _first[_n] = 0;
    const c8* _name = _first;
    while(*_name == ' ') {
        _name++;
    }
    const fude_sim_lib* _lib = fude_zoom_logic_library();
    const fude_sim_def* _def = fude_sim_lib_find(_lib, _name);
    if(_def != NULL) {
        return _def;
    }
    const fude_sim_def* const* _d = (const fude_sim_def* const*)_lib->defs.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_lib->defs); _i++) {
        if(_d[_i]->kind == FUDE_SIM_COMPOSITE && strcmp(_d[_i]->name, _name) == 0) {
            return _d[_i];
        }
    }
    return NULL;
}

// --- parts made from definitions -----------------------------------------------------------------

#define FZL_PINS 64u

typedef struct {
    fude_zoom_part        part;
    const fude_zoom_part* template_of;
    b8                    package;
    c8                    id[FUDE_SIM_ID];
    u32                   version;
    fude_zoom_pin         pins[FZL_PINS];
    c8                    names[FZL_PINS][FUDE_SIM_NAME];
} fzl_made;

RDE_INTERNAL rde_arr TYPE(fzl_made*) fzl_parts;   // (each kept for good: circuits hold their parts)

// Pin _k of _n round a chip's package: the first half down its left, the rest up its right.
RDE_INTERNAL fude_zoom_pin fzl_package_pin(u32 _n, u32 _k) {
    const u32 _half = (_n + 1u) / 2u;
    if(_k < _half) {
        return (fude_zoom_pin){ -1.0f, 1.0f - 2.0f * (f32)(_k + 1u) / (f32)(_half + 1u), NULL, FUDE_ZOOM_PIN_LEFT };
    }
    return (fude_zoom_pin){ 1.0f, -1.0f + 2.0f * (f32)(_k - _half + 1u) / (f32)(_half + 1u), NULL, FUDE_ZOOM_PIN_RIGHT };
}

const fude_zoom_part* fude_zoom_logic_part(const fude_zoom_part* _template, const fude_sim_def* _def, b8 _package) {
    if(_template == NULL || _def == NULL) {
        return NULL;
    }
    if(!rde_arr_is_inited(&fzl_parts)) {
        fzl_parts = rde_arr_new(sizeof(fzl_made*), rde_memory_allocator_get_default_std());
    }
    fzl_made* const* _m = (fzl_made* const*)fzl_parts.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&fzl_parts); _i++) {
        if(_m[_i]->template_of == _template && _m[_i]->package == _package && _m[_i]->version == _def->version && strcmp(_m[_i]->id, _def->id) == 0) {
            return &_m[_i]->part;
        }
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    fzl_made* _new = (fzl_made*)_heap->calloc(_heap->allocator, 1, sizeof(fzl_made));
    _new->template_of = _template;
    _new->package     = _package;
    _new->version     = _def->version;
    snprintf(_new->id, sizeof(_new->id), "%s", _def->id);
    const fude_sim_port* _ports = (const fude_sim_port*)_def->ports.memory;
    const u32 _n = (u32)rde_arr_length(&_def->ports) < FZL_PINS ? (u32)rde_arr_length(&_def->ports) : FZL_PINS;
    // A block's: each side's ports spread along it, in their order (inputs left, outputs right, power above and below).
    u32 _on[4] = { 0, 0, 0, 0 }, _seen[4] = { 0, 0, 0, 0 };
    for(u32 _k = 0; _k < _n; _k++) {
        _on[_ports[_k].side & 3u]++;
    }
    for(u32 _k = 0; _k < _n; _k++) {
        snprintf(_new->names[_k], sizeof(_new->names[_k]), "%s", _ports[_k].name);
        fude_zoom_pin _p;
        if(_package) {
            _p = fzl_package_pin(_n, _k);
        } else {
            const u8 _side = _ports[_k].side & 3u;
            const f32 _t = 2.0f * (f32)(++_seen[_side]) / (f32)(_on[_side] + 1u);
            switch(_side) {
                case FUDE_SIM_RIGHT:  _p = (fude_zoom_pin){ 1.0f, 1.0f - _t, NULL, FUDE_ZOOM_PIN_RIGHT }; break;
                case FUDE_SIM_TOP:    _p = (fude_zoom_pin){ -1.0f + _t, 1.0f, NULL, FUDE_ZOOM_PIN_UP }; break;
                case FUDE_SIM_BOTTOM: _p = (fude_zoom_pin){ -1.0f + _t, -1.0f, NULL, FUDE_ZOOM_PIN_DOWN }; break;
                default:              _p = (fude_zoom_pin){ -1.0f, 1.0f - _t, NULL, FUDE_ZOOM_PIN_LEFT }; break;
            }
        }
        _p.name = _new->names[_k];
        _new->pins[_k] = _p;
    }
    _new->part           = *_template;
    _new->part.id        = _new->id;
    _new->part.pins      = _new->pins;
    _new->part.pin_count = (u16)_n;
    rde_arr_add(&fzl_parts, (any)&_new);
    return &_new->part;
}

// --- a circuit's logic: made ---------------------------------------------------------------------

b8 fude_zoom_logic_is(const fude_zoom_part* _part) {
    if(_part == NULL) {
        return false;
    }
    const u8 _m = _part->model;
    return _m == FUDE_ZOOM_MODEL_GATE || _m == FUDE_ZOOM_MODEL_DFF || _m == FUDE_ZOOM_MODEL_TFF || _m == FUDE_ZOOM_MODEL_LOGIC_IN ||
           _m == FUDE_ZOOM_MODEL_LOGIC_OUT || _m == FUDE_ZOOM_MODEL_SIM;
}

void fude_zoom_logic_init(fude_zoom_logic* _l) {
    memset(_l, 0, sizeof(*_l));
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    _l->bridges      = rde_arr_new(sizeof(fude_zoom_bridge), _heap);
    _l->part_bridges = rde_arr_new(sizeof(u32), _heap);
    _l->node_net     = rde_arr_new(sizeof(u32), _heap);
    _l->part_input   = rde_arr_new(sizeof(u32), _heap);
    _l->part_probe   = rde_arr_new(sizeof(u32), _heap);
    _l->part_level   = rde_arr_new(sizeof(u8), _heap);
    _l->errors       = rde_str_new(NULL, _heap);
    fude_sim_flat_init(&_l->flat);
}

// The run and its netlist let go (its flat circuit kept: cleared when made again).
RDE_INTERNAL void fzl_drop_run(fude_zoom_logic* _l) {
    if(_l->on) {
        fude_sim_run_destroy(&_l->run);
    }
    if(_l->netlist != NULL) {
        fude_sim_def_free(_l->netlist);
        _l->netlist = NULL;
    }
    _l->on = false;
}

void fude_zoom_logic_forget(fude_zoom_logic* _l) {
    fzl_drop_run(_l);
}

void fude_zoom_logic_destroy(fude_zoom_logic* _l) {
    fzl_drop_run(_l);
    fude_sim_flat_destroy(&_l->flat);
    rde_arr_free(&_l->bridges);
    rde_arr_free(&_l->part_bridges);
    rde_arr_free(&_l->node_net);
    rde_arr_free(&_l->part_input);
    rde_arr_free(&_l->part_probe);
    rde_arr_free(&_l->part_level);
    rde_str_free(&_l->errors);
    memset(_l, 0, sizeof(*_l));
}

static const c8* const FZL_GATES[8] = { "and", "or", "not", "nand", "nor", "xor", "xnor", "buf" };   // (FUDE_ZOOM_GATE_'s order)

// What part _p is in the netlist: its definition, its parameters, and for each of its pins the port it is (the rest of
// the definition's ports tied low). False: not a logic part (or its definition is not there).
RDE_INTERNAL b8 fzl_what(const fude_zoom_circuit_part* _p, c8* _def, usize _def_size, c8* _params, usize _params_size, u32* _port_of_pin,
                         u32* _ports) {
    const fude_zoom_part* _part = _p->part;
    _params[0] = 0;
    for(u32 _k = 0; _k < _part->pin_count && _k < 64u; _k++) {
        _port_of_pin[_k] = _k;
    }
    switch(_part->model) {
    case FUDE_ZOOM_MODEL_GATE: {
        snprintf(_def, _def_size, "%s", FZL_GATES[_part->gate & 7u]);
        const u32 _inputs = _part->pin_count > 1u ? _part->pin_count - 1u : 1u;
        if(_part->gate != FUDE_ZOOM_GATE_NOT && _part->gate != FUDE_ZOOM_GATE_BUFFER) {
            snprintf(_params, _params_size, "inputs=%u", _inputs);
        }
        *_ports = _part->pin_count;
        return true;
    }
    case FUDE_ZOOM_MODEL_DFF:
    case FUDE_ZOOM_MODEL_TFF:
        // (D or T, CLK, Q, QN: the model's R between, tied low)
        snprintf(_def, _def_size, "%s", _part->model == FUDE_ZOOM_MODEL_DFF ? "dff" : "tff");
        _port_of_pin[2] = 3u;
        _port_of_pin[3] = 4u;
        *_ports = 5u;
        return true;
    case FUDE_ZOOM_MODEL_LOGIC_IN:
        snprintf(_def, _def_size, "input");
        snprintf(_params, _params_size, "%u", (u32)(_p->switch_on & 1u));
        *_ports = 1u;
        return true;
    case FUDE_ZOOM_MODEL_LOGIC_OUT:
        snprintf(_def, _def_size, "probe");
        *_ports = 1u;
        return true;
    case FUDE_ZOOM_MODEL_SIM: {
        const fude_sim_def* _d = fude_zoom_logic_find(_part->id);
        if(_d == NULL) {
            return false;
        }
        snprintf(_def, _def_size, "%s", _d->id);
        *_ports = (u32)rde_arr_length(&_d->ports);
        return true;
    }
    default:
        return false;
    }
}

// A SIM part's port _k's (its definition's) domain and way; a built-in's: logic, as its pin is.
RDE_INTERNAL void fzl_port(const fude_zoom_circuit_part* _p, u32 _pin, u8* _domain, u8* _dir) {
    *_domain = FUDE_SIM_LOGIC;
    *_dir    = FUDE_SIM_IN;
    const fude_zoom_part* _part = _p->part;
    switch(_part->model) {
    case FUDE_ZOOM_MODEL_GATE:      *_dir = _pin + 1u == _part->pin_count ? FUDE_SIM_OUT : FUDE_SIM_IN; return;
    case FUDE_ZOOM_MODEL_DFF:
    case FUDE_ZOOM_MODEL_TFF:       *_dir = _pin >= 2u ? FUDE_SIM_OUT : FUDE_SIM_IN; return;
    case FUDE_ZOOM_MODEL_LOGIC_IN:  *_dir = FUDE_SIM_OUT; return;
    case FUDE_ZOOM_MODEL_LOGIC_OUT: *_dir = FUDE_SIM_IN; return;
    case FUDE_ZOOM_MODEL_SIM: {
        const fude_sim_def* _d = fude_zoom_logic_find(_part->id);
        if(_d != NULL && _pin < (u32)rde_arr_length(&_d->ports)) {
            const fude_sim_port* _q = &((const fude_sim_port*)_d->ports.memory)[_pin];
            *_domain = _q->domain;
            *_dir    = _q->dir;
        }
        return;
    }
    default: return;
    }
}

// A part's supply and ground pins (an electric pin named as one), FUDE_ZOOM_NONE each where it has none.
RDE_INTERNAL void fzl_power(const fude_zoom_circuit_part* _p, u32* _supply, u32* _ground) {
    *_supply = FUDE_ZOOM_NONE;
    *_ground = FUDE_ZOOM_NONE;
    if(_p->part->model != FUDE_ZOOM_MODEL_SIM) {
        return;
    }
    for(u32 _k = 0; _k < _p->part->pin_count && _k < 64u; _k++) {
        u8 _domain, _dir;
        fzl_port(_p, _k, &_domain, &_dir);
        const c8* _n = _p->part->pins[_k].name;
        if(_domain != FUDE_SIM_ELECTRIC || _n == NULL) {
            continue;
        }
        if(strcmp(_n, "VCC") == 0 || strcmp(_n, "VDD") == 0 || strcmp(_n, "V+") == 0) {
            *_supply = _k;
        } else if(strcmp(_n, "GND") == 0 || strcmp(_n, "VSS") == 0) {
            *_ground = _k;
        }
    }
}

// The net of probe _ref in the flat circuit (what it reads); FUDE_SIM_NONE: none.
RDE_INTERNAL u32 fzl_net_of(const fude_sim_flat* _flat, const c8* _ref) {
    const u32 _p = fude_sim_flat_find(_flat, _ref);
    if(_p == FUDE_SIM_NONE) {
        return FUDE_SIM_NONE;
    }
    const fude_sim_prim* _pr = &((const fude_sim_prim*)_flat->prims.memory)[_p];
    return _pr->count > 0u ? ((const u32*)_flat->port_nets.memory)[_pr->first] : FUDE_SIM_NONE;
}

void fude_zoom_logic_build(fude_zoom_logic* _l, const fude_zoom_circuit* _c) {
    // (what it was, kept until the new one has taken what it held)
    fude_sim_flat _was_flat = _l->flat;
    fude_sim_run  _was_run  = _l->run;
    fude_sim_def* _was_list = _l->netlist;
    const b8      _was_on   = _l->on;
    _l->on      = false;
    _l->netlist = NULL;
    fude_sim_flat_init(&_l->flat);
    rde_arr_clear(&_l->bridges);
    rde_str_clear(&_l->errors);
    const fude_zoom_circuit_part* _p = (const fude_zoom_circuit_part*)_c->parts.memory;
    const u32 _np = (u32)rde_arr_length(&_c->parts), _nn = _c->nodes;
    rde_arr_resize(&_l->part_bridges, 2u * _np);
    rde_arr_resize(&_l->part_input, _np);
    rde_arr_resize(&_l->part_probe, _np);
    rde_arr_resize(&_l->part_level, _np);
    rde_arr_resize(&_l->node_net, _nn);
    u32* _pb = (u32*)_l->part_bridges.memory, *_pi = (u32*)_l->part_input.memory, *_pp = (u32*)_l->part_probe.memory;
    u32* _node_net = (u32*)_l->node_net.memory;
    for(u32 _i = 0; _i < _np; _i++) {
        _pb[2u * _i] = 0u;
        _pb[2u * _i + 1u] = 0u;
        _pi[_i] = FUDE_SIM_NONE;
        _pp[_i] = FUDE_SIM_NONE;
    }
    // Which nodes are the analog circuit's: what is not a logic pin is on them (ground is).
    rde_arr _analog_arr = rde_arr_new(sizeof(u8), rde_memory_allocator_get_default_std());
    rde_arr_resize(&_analog_arr, _nn + 1u);
    u8* _analog = (u8*)_analog_arr.memory;
    _analog[0] = 1u;
    u32 _logic_parts = 0;
    for(u32 _i = 0; _i < _np; _i++) {
        const b8 _logic = fude_zoom_logic_is(_p[_i].part);
        _logic_parts += _logic ? 1u : 0u;
        for(u32 _k = 0; _k < _p[_i].part->pin_count && _k < 64u; _k++) {
            const u32 _node = _p[_i].node[_k];
            if(_node == FUDE_ZOOM_NONE || _node >= _nn) {
                continue;
            }
            u8 _domain = FUDE_SIM_ELECTRIC, _dir;
            if(_logic) {
                fzl_port(&_p[_i], _k, &_domain, &_dir);
            }
            if(_domain != FUDE_SIM_LOGIC) {
                _analog[_node] = 1u;
            }
        }
    }
    for(u32 _n = 0; _n < _nn; _n++) {
        _node_net[_n] = FUDE_SIM_NONE;
    }
    if(_logic_parts == 0u) {
        rde_arr_free(&_analog_arr);
        if(_was_on) {
            fude_sim_run_destroy(&_was_run);
        }
        if(_was_list != NULL) {
            fude_sim_def_free(_was_list);
        }
        fude_sim_flat_destroy(&_was_flat);
        return;
    }
    // The netlist: each logic part an instance ("P" and its index), its pins' nets — a logic node's own ("n" and its
    // index), or a bridge's ("b" and its index) where the node is the analog circuit's.
    fude_sim_def* _list = fude_sim_def_new("canvas/logic", "canvas");
    b8 _zero = false;
    for(u32 _i = 0; _i < _np; _i++) {
        c8 _def[FUDE_SIM_ID], _params[96];
        u32 _port_of_pin[64], _ports = 0;
        if(!fude_zoom_logic_is(_p[_i].part) || !fzl_what(&_p[_i], _def, sizeof(_def), _params, sizeof(_params), _port_of_pin, &_ports)) {
            continue;
        }
        u32 _supply, _ground;
        fzl_power(&_p[_i], &_supply, &_ground);
        c8 _names[64][16];
        const c8* _nets[64];
        for(u32 _k = 0; _k < _ports && _k < 64u; _k++) {
            _nets[_k] = "zero";   // (a port no pin is: tied low)
        }
        _pb[2u * _i] = (u32)rde_arr_length(&_l->bridges);
        for(u32 _k = 0; _k < _p[_i].part->pin_count && _k < 64u; _k++) {
            const u32 _port = _port_of_pin[_k], _node = _p[_i].node[_k];
            if(_port >= 64u) {
                continue;
            }
            u8 _domain, _dir;
            fzl_port(&_p[_i], _k, &_domain, &_dir);
            if(_domain != FUDE_SIM_LOGIC || _node == FUDE_ZOOM_NONE || _node >= _nn) {
                _nets[_port] = "-";   // (open; a supply pin: the analog circuit's)
                continue;
            }
            if(!_analog[_node]) {
                snprintf(_names[_k], sizeof(_names[_k]), "n%u", _node);
                _nets[_port] = _names[_k];
                if(_node_net[_node] == FUDE_SIM_NONE) {
                    _node_net[_node] = 0u;   // (its net found once it is made)
                    c8 _probe[24];
                    snprintf(_probe, sizeof(_probe), "r%u", _node);
                    const c8* _pn[1] = { _names[_k] };
                    fude_sim_def_inst(_list, "probe", _probe, "", _pn, 1u);
                }
                continue;
            }
            // A bridge: an input's level from the node (an input primitive on its net), an output's read off its net.
            const u32 _b = (u32)rde_arr_length(&_l->bridges);
            fude_zoom_bridge _br;
            memset(&_br, 0, sizeof(_br));
            _br.part   = _i;
            _br.pin    = _k;
            _br.node   = _node;
            _br.ground = _ground;
            _br.supply = _supply;
            _br.way    = _dir == FUDE_SIM_OUT ? FUDE_ZOOM_BRIDGE_OUT : (_dir == FUDE_SIM_INOUT ? FUDE_ZOOM_BRIDGE_INOUT : FUDE_ZOOM_BRIDGE_IN);
            _br.read   = FUDE_SIM_X;
            _br.drive  = FUDE_SIM_Z;
            _br.input  = FUDE_SIM_NONE;
            _br.net    = FUDE_SIM_NONE;
            rde_arr_add(&_l->bridges, (any)&_br);
            snprintf(_names[_k], sizeof(_names[_k]), "b%u", _b);
            _nets[_port] = _names[_k];
            const c8* _bn[1] = { _names[_k] };
            c8 _ref[24];
            if(_br.way != FUDE_ZOOM_BRIDGE_OUT) {
                snprintf(_ref, sizeof(_ref), "i%u", _b);
                fude_sim_def_inst(_list, "input", _ref, "0", _bn, 1u);
            }
            snprintf(_ref, sizeof(_ref), "o%u", _b);
            fude_sim_def_inst(_list, "probe", _ref, "", _bn, 1u);
        }
        _pb[2u * _i + 1u] = (u32)rde_arr_length(&_l->bridges) - _pb[2u * _i];
        for(u32 _k = 0; _k < _ports && _k < 64u; _k++) {
            _zero = _zero || strcmp(_nets[_k], "zero") == 0;
        }
        c8 _ref[16];
        snprintf(_ref, sizeof(_ref), "P%u", _p[_i].object);
        const u32 _inst = fude_sim_def_inst(_list, _def, _ref, _params, _nets, _ports < 64u ? _ports : 64u);
        ((fude_sim_inst*)_list->insts.memory)[_inst].tag = _p[_i].object;
    }
    if(_zero) {
        const c8* _zn[1] = { "zero" };
        fude_sim_def_inst(_list, "const", "Z", "0", _zn, 1u);
    }
    rde_arr_free(&_analog_arr);
    _l->netlist = _list;
    if(!fude_sim_elaborate(fude_zoom_logic_library(), _list, &_l->flat)) {
        rde_str_append_str(&_l->errors, fude_sim_flat_errors(&_l->flat));
    } else {
        fude_sim_run_init(&_l->run, &_l->flat);
        _l->on = true;
        if(_was_on) {
            fude_sim_run_keep(&_l->run, &_was_run);
        }
        // Where each node's net is, each bridge's, each logic input's primitive, each probe's net.
        for(u32 _n = 0; _n < _nn; _n++) {
            if(_node_net[_n] != FUDE_SIM_NONE) {
                c8 _probe[24];
                snprintf(_probe, sizeof(_probe), "r%u", _n);
                _node_net[_n] = fzl_net_of(&_l->flat, _probe);
            }
        }
        fude_zoom_bridge* _br = (fude_zoom_bridge*)_l->bridges.memory;
        for(u32 _b = 0; _b < (u32)rde_arr_length(&_l->bridges); _b++) {
            c8 _ref[24];
            snprintf(_ref, sizeof(_ref), "i%u", _b);
            _br[_b].input = fude_sim_flat_find(&_l->flat, _ref);
            snprintf(_ref, sizeof(_ref), "o%u", _b);
            _br[_b].net = fzl_net_of(&_l->flat, _ref);
        }
        u8* _level = (u8*)_l->part_level.memory;
        for(u32 _i = 0; _i < _np; _i++) {
            c8 _ref[16];
            snprintf(_ref, sizeof(_ref), "P%u", _p[_i].object);
            if(_p[_i].part->model == FUDE_ZOOM_MODEL_LOGIC_IN) {
                _pi[_i] = fude_sim_flat_find(&_l->flat, _ref);
                _level[_i] = 0xFFu;   // (set from its switch on the first step)
            } else if(_p[_i].part->model == FUDE_ZOOM_MODEL_LOGIC_OUT) {
                _pp[_i] = fzl_net_of(&_l->flat, _ref);
            }
        }
    }
    if(_was_on) {
        fude_sim_run_destroy(&_was_run);
    }
    if(_was_list != NULL) {
        fude_sim_def_free(_was_list);
    }
    fude_sim_flat_destroy(&_was_flat);
}

// --- a circuit's logic: run ----------------------------------------------------------------------

// A pin's node's voltage (an open one, or none: ground's).
RDE_INTERNAL f64 fzl_volts(const fude_zoom_circuit* _c, const fude_zoom_circuit_part* _p, u32 _pin) {
    if(_pin == FUDE_ZOOM_NONE || _pin >= 64u) {
        return 0.0;
    }
    const u32 _node = _p->node[_pin];
    return _node != FUDE_ZOOM_NONE && _node < _c->nodes && _node < FUDE_ZOOM_CIRCUIT_MAX_NODES ? _c->v[_node] : 0.0;
}

b8 fude_zoom_logic_step(fude_zoom_logic* _l, const fude_zoom_circuit* _c, f64 _t) {
    if(!_l->on) {
        return false;
    }
    if(_t > _l->run.now) {
        fude_sim_run_until(&_l->run, _t);
    }
    const fude_zoom_circuit_part* _p = (const fude_zoom_circuit_part*)_c->parts.memory;
    const u32 _np = (u32)rde_arr_length(&_c->parts);
    const u32* _pi = (const u32*)_l->part_input.memory;
    u8* _level = (u8*)_l->part_level.memory;
    // The logic inputs as their switches are; each input bridge as its node's voltage is, against its supply's half.
    for(u32 _i = 0; _i < _np && _i < (u32)rde_arr_length(&_l->part_input); _i++) {
        if(_pi[_i] == FUDE_SIM_NONE) {
            continue;
        }
        const u8 _v = (u8)(_p[_i].switch_on & 1u);
        if(_level[_i] != _v) {
            _level[_i] = _v;
            fude_sim_run_set(&_l->run, _pi[_i], _v);
        }
    }
    fude_zoom_bridge* _br = (fude_zoom_bridge*)_l->bridges.memory;
    const u32 _nb = (u32)rde_arr_length(&_l->bridges);
    for(u32 _b = 0; _b < _nb; _b++) {
        if(_br[_b].input == FUDE_SIM_NONE || _br[_b].part >= _np) {
            continue;
        }
        const fude_zoom_circuit_part* _q = &_p[_br[_b].part];
        const f64 _vg = fzl_volts(_c, _q, _br[_b].ground);
        const f64 _vs = _br[_b].supply != FUDE_ZOOM_NONE ? fzl_volts(_c, _q, _br[_b].supply) : _vg + FUDE_ZOOM_LOGIC_V;
        const f64 _v  = _c->v[_br[_b].node] - _vg;
        const u8  _r  = _vs - _vg < 1.0 ? FUDE_SIM_X : (_v > 0.5 * (_vs - _vg) ? FUDE_SIM_1 : FUDE_SIM_0);   // (unpowered: unknown)
        if(_r != _br[_b].read) {
            _br[_b].read = _r;
            fude_sim_run_set(&_l->run, _br[_b].input, _r);
        }
    }
    fude_sim_run_settle(&_l->run, 1e-6);
    // What each output bridge drives now: its pin's level (a two-way pin's without its own input's).
    b8 _changed = false;
    for(u32 _b = 0; _b < _nb; _b++) {
        if(_br[_b].way == FUDE_ZOOM_BRIDGE_IN || _br[_b].net == FUDE_SIM_NONE) {
            continue;
        }
        const u8 _d = _br[_b].way == FUDE_ZOOM_BRIDGE_INOUT ? fude_sim_run_net_without(&_l->run, _br[_b].net, _br[_b].input)
                                                           : fude_sim_run_net(&_l->run, _br[_b].net);
        if(_d != _br[_b].drive) {
            _br[_b].drive = _d;
            _changed = true;
        }
    }
    return _changed;
}

u8 fude_zoom_logic_node(const fude_zoom_logic* _l, u32 _node) {
    if(!_l->on || _node >= (u32)rde_arr_length(&_l->node_net)) {
        return 0xFFu;
    }
    const u32 _net = ((const u32*)_l->node_net.memory)[_node];
    return _net == FUDE_SIM_NONE ? 0xFFu : fude_sim_run_net(&_l->run, _net);
}

u8 fude_zoom_logic_probe(const fude_zoom_logic* _l, u32 _part) {
    if(!_l->on || _part >= (u32)rde_arr_length(&_l->part_probe)) {
        return 0xFFu;
    }
    const u32 _net = ((const u32*)_l->part_probe.memory)[_part];
    return _net == FUDE_SIM_NONE ? 0xFFu : fude_sim_run_net(&_l->run, _net);
}

u32 fude_zoom_logic_bridges_of(const fude_zoom_logic* _l, u32 _part, u32* _first) {
    if(2u * _part + 1u >= (u32)rde_arr_length(&_l->part_bridges)) {
        *_first = 0u;
        return 0u;
    }
    const u32* _pb = (const u32*)_l->part_bridges.memory;
    *_first = _pb[2u * _part];
    return _pb[2u * _part + 1u];
}

// --- custom parts made of what is drawn ---------------------------------------------------------

typedef struct {
    u32 part;      // the circuit's
    u8  dir;       // FUDE_SIM_IN, FUDE_SIM_OUT
    f64 x, y;      // where it is (the home frame)
    c8  name[FUDE_SIM_NAME];
} fzl_port_at;

RDE_INTERNAL int fzl_port_order(const void* _a, const void* _b) {
    const fzl_port_at* _p = (const fzl_port_at*)_a, *_q = (const fzl_port_at*)_b;
    if(_p->dir != _q->dir) {
        return _p->dir < _q->dir ? -1 : 1;
    }
    if(fabs(_p->y - _q->y) > 1e-9) {
        return _p->y > _q->y ? -1 : 1;   // (top first)
    }
    return _p->x < _q->x ? -1 : (_p->x > _q->x ? 1 : 0);
}

// Is _text a level a logic input says (its value, not a name)?
RDE_INTERNAL b8 fzl_level_word(const c8* _t) {
    static const c8* const _words[8] = { "", "0", "1", "on", "off", "high", "low", "-" };
    for(u32 _i = 0; _i < 8u; _i++) {
        if(strcmp(_t, _words[_i]) == 0) {
            return true;
        }
    }
    return false;
}

fude_sim_def* fude_zoom_logic_make(const fude_zoom_circuit* _c, const fude_zoom_scene* _s, const c8* _id, const c8* _name, u32* _problem) {
    *_problem = FUDE_ZOOM_MAKE_OK;
    const fude_zoom_circuit_part* _p = (const fude_zoom_circuit_part*)_c->parts.memory;
    const u32 _np = (u32)rde_arr_length(&_c->parts), _nn = _c->nodes;
    const u32 _home = _s->home != FUDE_ZOOM_NONE ? _s->home : _s->root;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr TYPE(fzl_port_at) _ports_arr = rde_arr_new(sizeof(fzl_port_at), _heap);
    u32 _inside = 0;
    for(u32 _i = 0; _i < _np; _i++) {
        const u8 _m = _p[_i].part->model;
        if(_m != FUDE_ZOOM_MODEL_LOGIC_IN && _m != FUDE_ZOOM_MODEL_LOGIC_OUT) {
            _inside += fude_zoom_logic_is(_p[_i].part) ? 1u : 0u;
            continue;
        }
        fzl_port_at _q;
        memset(&_q, 0, sizeof(_q));
        _q.part = _i;
        _q.dir  = _m == FUDE_ZOOM_MODEL_LOGIC_IN ? FUDE_SIM_IN : FUDE_SIM_OUT;
        const fude_zoom_object* _o = fude_zoom_scene_object(_s, _p[_i].object);
        const fude_zoom_v2 _at = fude_zoom_sim_apply(fude_zoom_scene_sim(_s, _o->frame, _home), fude_zoom_scene_place_of(_s, _p[_i].object).t);
        _q.x = _at.x;
        _q.y = _at.y;
        // (its text its name, as one word, where it is not a level)
        f64 _n[FUDE_ZOOM_SHAPE_NUMBERS + 200];
        const u32 _count = fude_zoom_scene_shape_numbers(_s, _p[_i].object, _n, FUDE_ZOOM_SHAPE_NUMBERS + 200u);
        c8 _text[FUDE_ZOOM_SYMBOL_TEXT];
        fude_zoom_symbol_text(_n, _count, _text, sizeof(_text));
        c8* _line = _text;
        while(*_line == ' ') {
            _line++;
        }
        u32 _k = 0;
        for(const c8* _t = _line; *_t != 0 && *_t != '\n' && _k + 1u < sizeof(_q.name); _t++) {
            _q.name[_k++] = (*_t == ' ' || *_t == '"' || *_t == '\t' || *_t == '/') ? '_' : *_t;
        }
        while(_k > 0u && _q.name[_k - 1u] == '_') {
            _k--;
        }
        _q.name[_k] = 0;
        if(fzl_level_word(_q.name)) {
            _q.name[0] = 0;
        }
        rde_arr_add(&_ports_arr, (any)&_q);
    }
    const u32 _nports = (u32)rde_arr_length(&_ports_arr);
    if(_inside == 0u || _nports == 0u) {
        *_problem = _inside == 0u ? FUDE_ZOOM_MAKE_NOTHING : FUDE_ZOOM_MAKE_NO_PORTS;
        rde_arr_free(&_ports_arr);
        return NULL;
    }
    fzl_port_at* _q = (fzl_port_at*)_ports_arr.memory;
    qsort(_q, _nports, sizeof(fzl_port_at), fzl_port_order);
    // (the unnamed named in turn; a name twice made unique)
    u32 _ins = 0, _outs = 0;
    for(u32 _i = 0; _i < _nports; _i++) {
        if(_q[_i].name[0] == 0) {
            snprintf(_q[_i].name, sizeof(_q[_i].name), _q[_i].dir == FUDE_SIM_IN ? "IN%u" : "OUT%u", _q[_i].dir == FUDE_SIM_IN ? ++_ins : ++_outs);
        }
        for(u32 _j = 0; _j < _i; _j++) {
            if(strcmp(_q[_j].name, _q[_i].name) == 0) {
                c8 _was[FUDE_SIM_NAME];
                snprintf(_was, sizeof(_was), "%s", _q[_i].name);
                snprintf(_q[_i].name, sizeof(_q[_i].name), "%.24s_%u", _was, _i + 1u);
                _j = (u32)-1;   // (looked through again)
            }
        }
    }
    fude_sim_def* _def = fude_sim_def_new(_id, _name);
    // Each port's node its net (named as the port); a second port on a node joined to it through a buffer.
    rde_arr _node_port_arr = rde_arr_new(sizeof(u32), _heap);
    rde_arr_resize(&_node_port_arr, _nn + 1u);
    u32* _node_port = (u32*)_node_port_arr.memory;
    for(u32 _n = 0; _n <= _nn; _n++) {
        _node_port[_n] = FUDE_ZOOM_NONE;
    }
    u32 _bufs = 0;
    for(u32 _i = 0; _i < _nports; _i++) {
        const u32 _port = fude_sim_def_port(_def, _q[_i].name, FUDE_SIM_LOGIC, _q[_i].dir, 1u);
        ((fude_sim_port*)_def->ports.memory)[_port].side = _q[_i].dir == FUDE_SIM_IN ? FUDE_SIM_LEFT : FUDE_SIM_RIGHT;
        const u32 _node = _p[_q[_i].part].node[0];
        if(_node == FUDE_ZOOM_NONE || _node > _nn) {
            continue;
        }
        if(_node_port[_node] == FUDE_ZOOM_NONE) {
            _node_port[_node] = _i;
        } else {
            const c8* _first = _q[_node_port[_node]].name;
            const c8* _bn[2] = { _q[_i].dir == FUDE_SIM_OUT ? _first : _q[_i].name, _q[_i].dir == FUDE_SIM_OUT ? _q[_i].name : _first };
            c8 _ref[16];
            snprintf(_ref, sizeof(_ref), "J%u", ++_bufs);
            fude_sim_def_inst(_def, "buf", _ref, "delay=0", _bn, 2u);
        }
    }
    // Its parts, their pins' nets as their nodes are.
    b8 _zero = false;
    u32 _u = 0;
    for(u32 _i = 0; _i < _np; _i++) {
        const u8 _m = _p[_i].part->model;
        c8 _what[FUDE_SIM_ID], _params[96];
        u32 _port_of_pin[64], _ports = 0;
        if(_m == FUDE_ZOOM_MODEL_LOGIC_IN || _m == FUDE_ZOOM_MODEL_LOGIC_OUT || !fude_zoom_logic_is(_p[_i].part) ||
           !fzl_what(&_p[_i], _what, sizeof(_what), _params, sizeof(_params), _port_of_pin, &_ports)) {
            continue;
        }
        c8 _names[64][FUDE_SIM_NAME];
        const c8* _nets[64];
        for(u32 _k = 0; _k < _ports && _k < 64u; _k++) {
            _nets[_k] = "zero";
        }
        for(u32 _k = 0; _k < _p[_i].part->pin_count && _k < 64u; _k++) {
            const u32 _port = _port_of_pin[_k], _node = _p[_i].node[_k];
            if(_port >= 64u) {
                continue;
            }
            u8 _domain, _dir;
            fzl_port(&_p[_i], _k, &_domain, &_dir);
            if(_domain != FUDE_SIM_LOGIC || _node == FUDE_ZOOM_NONE || _node > _nn) {
                _nets[_port] = "-";
                continue;
            }
            if(_node_port[_node] != FUDE_ZOOM_NONE) {
                snprintf(_names[_k], sizeof(_names[_k]), "%s", _q[_node_port[_node]].name);
            } else {
                snprintf(_names[_k], sizeof(_names[_k]), "n%u", _node);
            }
            _nets[_port] = _names[_k];
        }
        for(u32 _k = 0; _k < _ports && _k < 64u; _k++) {
            _zero = _zero || strcmp(_nets[_k], "zero") == 0;
        }
        c8 _ref[16];
        snprintf(_ref, sizeof(_ref), "U%u", ++_u);
        fude_sim_def_inst(_def, _what, _ref, _params, _nets, _ports < 64u ? _ports : 64u);
    }
    if(_zero) {
        const c8* _zn[1] = { "zero" };
        fude_sim_def_inst(_def, "const", "Z", "0", _zn, 1u);
    }
    rde_arr_free(&_node_port_arr);
    rde_arr_free(&_ports_arr);
    return _def;
}
