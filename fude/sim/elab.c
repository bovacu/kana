// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "sim/sim.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef c8 fse_name[FUDE_SIM_NAME];

static const c8* const FSE_DOMAINS[FUDE_SIM_DOMAINS] = { "electric", "logic", "rotation", "translation", "signal" };

// A custom part's port as it says it is (its domain), on a net out here: checked against what is on that net.
typedef struct {
    u32 net;
    u8  domain;
    c8  what[FUDE_SIM_PATH + FUDE_SIM_NAME + 4];
} fse_declared;

typedef struct {
    fude_sim_flat*      flat;
    const fude_sim_lib* lib;
    rde_arr TYPE(u32)   parent;   // the nets' union-find
    rde_arr TYPE(fse_declared) declared;
    b8                  ok;
} fse;

// A composite's parameters where its parts are (their names and values), for its parts' values that name them.
typedef struct {
    const fude_sim_param* names;
    const f64*            values;
    u32                   count;
} fse_scope;

void fude_sim_flat_init(fude_sim_flat* _flat) {
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    memset(_flat, 0, sizeof(*_flat));
    _flat->prims      = rde_arr_new(sizeof(fude_sim_prim), _heap);
    _flat->ports      = rde_arr_new(sizeof(fude_sim_port), _heap);
    _flat->port_nets  = rde_arr_new(sizeof(u32), _heap);
    _flat->net_domain = rde_arr_new(sizeof(u8), _heap);
    _flat->errors     = rde_str_new(NULL, _heap);
}

void fude_sim_flat_destroy(fude_sim_flat* _flat) {
    rde_arr_free(&_flat->prims);
    rde_arr_free(&_flat->ports);
    rde_arr_free(&_flat->port_nets);
    rde_arr_free(&_flat->net_domain);
    rde_str_free(&_flat->errors);
}

// A line more of what is wrong.
RDE_INTERNAL void fse_say(fude_sim_flat* _flat, const c8* _line) {
    rde_str_append_str(&_flat->errors, _line);
    rde_str_append_str(&_flat->errors, "\n");
}

const c8* fude_sim_flat_errors(const fude_sim_flat* _flat) {
    return rde_str_to_char_ptr(&_flat->errors);
}

RDE_INTERNAL u32 fse_new_net(fse* _e) {
    const u32 _n = (u32)rde_arr_length(&_e->parent);
    rde_arr_add(&_e->parent, (any)&_n);
    return _n;
}

RDE_INTERNAL u32 fse_root(fse* _e, u32 _x) {
    u32* _p = (u32*)_e->parent.memory;
    while(_p[_x] != _x) {
        _p[_x] = _p[_p[_x]];
        _x = _p[_x];
    }
    return _x;
}

RDE_INTERNAL void fse_join(fse* _e, u32 _a, u32 _b) {
    _a = fse_root(_e, _a);
    _b = fse_root(_e, _b);
    if(_a != _b) {
        ((u32*)_e->parent.memory)[_a < _b ? _b : _a] = _a < _b ? _a : _b;
    }
}

// A part's parameters from its text ("name=value ...", or a bare first value: its first parameter), over its defaults;
// a value that is its composite's parameter's name takes that value.
RDE_INTERNAL void fse_params(const fude_sim_param* _names, u32 _count, const c8* _text, const fse_scope* _scope, f64* _out) {
    for(u32 _k = 0; _k < _count && _k < FUDE_SIM_PARAMS; _k++) {
        _out[_k] = _names[_k].value;
    }
    c8 _buf[128];
    snprintf(_buf, sizeof(_buf), "%s", _text != NULL ? _text : "");
    u32 _bare = 0;
    for(c8* _tok = strtok(_buf, " \t,;"); _tok != NULL; _tok = strtok(NULL, " \t,;")) {
        c8* _eq = strchr(_tok, '=');
        u32 _k = FUDE_SIM_NONE;
        const c8* _value = _tok;
        if(_eq != NULL) {
            *_eq = 0;
            _value = _eq + 1;
            for(u32 _i = 0; _i < _count; _i++) {
                if(strcmp(_names[_i].name, _tok) == 0) {
                    _k = _i;
                }
            }
        } else {
            _k = _bare++;   // (a value alone: the parameters in their order)
        }
        if(_k == FUDE_SIM_NONE || _k >= _count || _k >= FUDE_SIM_PARAMS) {
            continue;
        }
        f64 _v;
        if(fude_sim_number(_value, &_v)) {
            _out[_k] = _v;
            continue;
        }
        for(u32 _i = 0; _scope != NULL && _i < _scope->count; _i++) {
            if(strcmp(_scope->names[_i].name, _value) == 0) {
                _out[_k] = _scope->values[_i];
            }
        }
    }
}

// _def's parts into the flat circuit: its ports on _port_nets (the flat nets outside: one each; NULL: none), its
// parts' paths under _path, its parameters' values _values.
RDE_INTERNAL void fse_expand(fse* _e, const fude_sim_def* _def, const u32* _port_nets, const c8* _path, u64 _tag, u32 _depth, const f64* _values) {
    if(_depth > FUDE_SIM_DEPTH) {
        c8 _say[256];
        snprintf(_say, sizeof(_say), "error: %s is made of itself (parts in parts deeper than %u)", _def->id, FUDE_SIM_DEPTH);
        fse_say(_e->flat, _say);
        _e->ok = false;
        return;
    }
    // Its nets: its ports' the ones outside; the rest new.
    const u32 _nn = (u32)rde_arr_length(&_def->nets);
    rde_arr _map_arr = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    rde_arr_resize(&_map_arr, _nn);
    u32* _map = (u32*)_map_arr.memory;   // (nothing added to it after: it stays where it is)
    for(u32 _i = 0; _i < _nn; _i++) {
        _map[_i] = FUDE_SIM_NONE;
    }
    const u32* _pn = (const u32*)_def->port_nets.memory;
    for(u32 _p = 0; _p < (u32)rde_arr_length(&_def->port_nets); _p++) {
        if(_pn[_p] >= _nn) {
            continue;
        }
        const u32 _outside = _port_nets != NULL ? _port_nets[_p] : FUDE_SIM_NONE;
        if(_outside == FUDE_SIM_NONE) {
            continue;
        }
        if(_map[_pn[_p]] == FUDE_SIM_NONE) {
            _map[_pn[_p]] = _outside;
        } else {
            fse_join(_e, _map[_pn[_p]], _outside);   // (two ports on one net inside: their nets outside one)
        }
    }
    for(u32 _i = 0; _i < _nn; _i++) {
        if(_map[_i] == FUDE_SIM_NONE) {
            _map[_i] = fse_new_net(_e);
        }
    }
    const fse_scope _scope = { (const fude_sim_param*)_def->params.memory, _values, (u32)rde_arr_length(&_def->params) };
    const fude_sim_inst* _in = (const fude_sim_inst*)_def->insts.memory;
    const u32* _inets = (const u32*)_def->inst_nets.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_def->insts) && _e->ok; _i++) {
        const fude_sim_def* _what = fude_sim_lib_find(_e->lib, _in[_i].def);
        c8 _sub[FUDE_SIM_PATH];
        snprintf(_sub, sizeof(_sub), "%s%s%s", _path, _path[0] != 0 ? "/" : "", _in[_i].ref[0] != 0 ? _in[_i].ref : _in[_i].def);
        if(_what == NULL) {
            c8 _say[256];
            snprintf(_say, sizeof(_say), "error: %s is a %s, and there is no such part", _sub, _in[_i].def);
            fse_say(_e->flat, _say);
            _e->ok = false;
            break;
        }
        const u64 _t = _depth == 0u ? _in[_i].tag : _tag;
        f64 _p[FUDE_SIM_PARAMS] = { 0 };
        fse_params((const fude_sim_param*)_what->params.memory, (u32)rde_arr_length(&_what->params), _in[_i].params, &_scope, _p);
        // Its ports' nets out here (a port it was not given: a net of its own, joined to nothing).
        fude_sim_port _ports[64];
        u32 _np = 0;
        if(_what->kind == FUDE_SIM_PRIMITIVE && _what->model != NULL && _what->model->ports != NULL) {
            _np = _what->model->ports(_p, _ports, 64u);
        } else {
            _np = (u32)rde_arr_length(&_what->ports) < 64u ? (u32)rde_arr_length(&_what->ports) : 64u;
            memcpy(_ports, _what->ports.memory, (usize)_np * sizeof(fude_sim_port));
        }
        u32 _nets[64];
        for(u32 _k = 0; _k < _np; _k++) {
            const u32 _local = _k < _in[_i].count ? _inets[_in[_i].first + _k] : FUDE_SIM_NONE;
            _nets[_k] = _local != FUDE_SIM_NONE && _local < _nn ? _map[_local] : fse_new_net(_e);
        }
        if(_what->kind == FUDE_SIM_PRIMITIVE) {
            fude_sim_prim _prim;
            memset(&_prim, 0, sizeof(_prim));
            _prim.model = _what->model;
            snprintf(_prim.path, sizeof(_prim.path), "%s", _sub);
            _prim.first = (u32)rde_arr_length(&_e->flat->ports);
            _prim.count = _np;
            memcpy(_prim.params, _p, sizeof(_prim.params));
            _prim.state = _e->flat->state;
            _prim.tag   = _t;
            _e->flat->state += fude_sim_model_states(_what->model, _prim.params);
            for(u32 _k = 0; _k < _np; _k++) {
                rde_arr_add(&_e->flat->ports, (any)&_ports[_k]);
                rde_arr_add(&_e->flat->port_nets, (any)&_nets[_k]);
            }
            rde_arr_add(&_e->flat->prims, (any)&_prim);
        } else {
            // (its ports as it says they are: what is joined to them out here, and inside, checked below)
            const fude_sim_port* _dp = (const fude_sim_port*)_what->ports.memory;
            for(u32 _k = 0; _k < _np; _k++) {
                fse_declared _d;
                _d.net    = _nets[_k];
                _d.domain = _dp[_k].domain;
                snprintf(_d.what, sizeof(_d.what), "%s's %s", _sub, _dp[_k].name);
                rde_arr_add(&_e->declared, (any)&_d);
            }
            fse_expand(_e, _what, _nets, _sub, _t, _depth + 1u, _p);
        }
    }
    rde_arr_free(&_map_arr);
}

b8 fude_sim_elaborate(const fude_sim_lib* _lib, const fude_sim_def* _top, fude_sim_flat* _flat) {
    rde_arr_clear(&_flat->prims);
    rde_arr_clear(&_flat->ports);
    rde_arr_clear(&_flat->port_nets);
    rde_arr_clear(&_flat->net_domain);
    rde_str_clear(&_flat->errors);
    _flat->nets  = 0;
    _flat->state = 0;
    fse _e = { _flat, _lib, rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std()), rde_arr_new(sizeof(fse_declared), rde_memory_allocator_get_default_std()), true };
    f64 _values[FUDE_SIM_PARAMS] = { 0 };
    for(u32 _k = 0; _k < (u32)rde_arr_length(&_top->params) && _k < FUDE_SIM_PARAMS; _k++) {
        _values[_k] = ((const fude_sim_param*)_top->params.memory)[_k].value;
    }
    fse_expand(&_e, _top, NULL, "", 0u, 0u, _values);
    // The nets: each joined set one, numbered from 0.
    const u32 _raw = (u32)rde_arr_length(&_e.parent);
    rde_arr _numbers = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    rde_arr_resize(&_numbers, _raw);
    u32* _number = (u32*)_numbers.memory;
    for(u32 _i = 0; _i < _raw; _i++) {
        _number[_i] = FUDE_SIM_NONE;
    }
    u32 _nets = 0;
    for(u32 _i = 0; _i < _raw; _i++) {
        const u32 _r = fse_root(&_e, _i);
        if(_number[_r] == FUDE_SIM_NONE) {
            _number[_r] = _nets++;
        }
        _number[_i] = _number[_r];
    }
    u32* _pn = (u32*)_flat->port_nets.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_flat->port_nets); _i++) {
        _pn[_i] = _number[_pn[_i]];
    }
    fse_declared* _dec = (fse_declared*)_e.declared.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_e.declared); _i++) {
        _dec[_i].net = _number[_dec[_i].net];
    }
    _flat->nets = _nets;
    rde_arr_free(&_numbers);
    rde_arr_free(&_e.parent);
    // Each net's domains (a bit each): logic and electric together are bridged; any other mix is a mistake.
    if(_nets > 0u) {
        rde_arr_resize(&_flat->net_domain, _nets);
        u8* _d = (u8*)_flat->net_domain.memory;
        const fude_sim_port* _ports = (const fude_sim_port*)_flat->ports.memory;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_flat->ports); _i++) {
            _d[_pn[_i]] |= (u8)(1u << _ports[_i].domain);
        }
        const fude_sim_prim* _prims = (const fude_sim_prim*)_flat->prims.memory;
        for(u32 _p = 0; _p < (u32)rde_arr_length(&_flat->prims); _p++) {
            for(u32 _k = 0; _k < _prims[_p].count; _k++) {
                const u32 _port = _prims[_p].first + _k;
                const u8 _bits = _d[_pn[_port]], _mine = (u8)(1u << _ports[_port].domain);
                const u8 _bridged = (u8)((1u << FUDE_SIM_LOGIC) | (1u << FUDE_SIM_ELECTRIC));
                if((_bits & ~_mine) != 0u && (_bits | _bridged) != _bridged) {
                    for(u32 _o = 0; _o < FUDE_SIM_DOMAINS; _o++) {
                        if((_bits & ~_mine) & (1u << _o)) {
                            c8 _say[256];
                            snprintf(_say, sizeof(_say), "error: %s's %s (%s) is joined to a %s connection", _prims[_p].path, _ports[_port].name,
                                     FSE_DOMAINS[_ports[_port].domain], FSE_DOMAINS[_o]);
                            fse_say(_flat, _say);
                            _e.ok = false;
                            break;
                        }
                    }
                }
            }
        }
    }
    // Custom parts' ports: what they say they are against what their nets hold.
    const u8 _bridged = (u8)((1u << FUDE_SIM_LOGIC) | (1u << FUDE_SIM_ELECTRIC));
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_e.declared) && _nets > 0u; _i++) {
        const u8 _bits = ((const u8*)_flat->net_domain.memory)[_dec[_i].net], _mine = (u8)(1u << _dec[_i].domain);
        if((_bits & ~_mine) == 0u || (_bits | _mine | _bridged) == _bridged) {
            continue;
        }
        for(u32 _o = 0; _o < FUDE_SIM_DOMAINS; _o++) {
            if((_bits & ~_mine) & (1u << _o)) {
                c8 _say[320];
                snprintf(_say, sizeof(_say), "error: %s (%s) is joined to a %s connection", _dec[_i].what, FSE_DOMAINS[_dec[_i].domain], FSE_DOMAINS[_o]);
                fse_say(_flat, _say);
                _e.ok = false;
                break;
            }
        }
    }
    rde_arr_free(&_e.declared);
    return _e.ok;
}

u32 fude_sim_flat_find(const fude_sim_flat* _flat, const c8* _path) {
    const fude_sim_prim* _p = (const fude_sim_prim*)_flat->prims.memory;
    for(u32 _i = 0; _path != NULL && _i < (u32)rde_arr_length(&_flat->prims); _i++) {
        if(strcmp(_p[_i].path, _path) == 0) {
            return _i;
        }
    }
    return FUDE_SIM_NONE;
}
