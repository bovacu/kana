// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "sim/sim.h"

// ===========================================================================
// See sim.h (its elaboration part).
//
// fude_sim_elaborate walks the top composite's parts depth first (fse_expand), copying each primitive out with its
// path, its parameters' values and its ports, and giving every net it meets a number of its own. A net reached
// through a composite's port is the same net as outside it: the two numbers are joined in a disjoint set (RDE's, each
// set named by its smallest number), and when the walk is done each joined set becomes one net of the flat circuit,
// numbered from 0 in the order of its smallest number, so the order the joins came in changes nothing. Then the
// nets' domains are checked, at every primitive's port and at every composite's port as it is declared. The file's
// own prefix is fse_ (FSE_).
// ===========================================================================

// The domains' names in FUDE_SIM_DOMAIN_ order, for the errors; and each with its article, as an error names what a
// port is joined to ("an electric connection").
static const c8* const FSE_DOMAINS[FUDE_SIM_DOMAINS] = { "electric", "logic", "rotation", "translation", "signal" };
static const c8* const FSE_DOMAINS_A[FUDE_SIM_DOMAINS] = { "an electric", "a logic", "a rotation", "a translation", "a signal" };

// Returns the first domain on a net (_bits: its domains, a bit each) that a port of domain _domain may not be joined
// to, or FUDE_SIM_DOMAINS when there is none. Logic and electric may meet (they are bridged); any other domain meets
// only itself. So a logic port on a net that also holds electric and rotation ports is wrong for the rotation, not
// for the electric it is bridged to.
RDE_INTERNAL u32 fse_wrong_domain(u8 _bits, u8 _domain) {
    const u8 _bridged = (u8)((1u << FUDE_SIM_LOGIC) | (1u << FUDE_SIM_ELECTRIC));
    const u8 _mine    = (u8)(1u << _domain);
    const u8 _wrong   = (u8)(_bits & ~((_mine & _bridged) != 0u ? _bridged : _mine));
    for(u32 _o = 0; _o < FUDE_SIM_DOMAINS; _o++) {
        if(_wrong & (1u << _o)) {
            return _o;
        }
    }
    return FUDE_SIM_DOMAINS;
}

// A custom part's port as it says it is (its domain), on a net out here: checked against what is on that net.
typedef struct {
    u32 net;                                        // the net outside it is on (a disjoint set's number, then a flat net)
    u8  domain;                                     // a FUDE_SIM_DOMAIN_, as the part declares it
    c8  what[FUDE_SIM_PATH + FUDE_SIM_NAME + 4];    // "<its path>'s <port name>", for the error
} fse_declared;

// One elaboration's working state.
typedef struct {
    fude_sim_flat*      flat;     // what is being filled
    const fude_sim_lib* lib;      // where the parts' definitions are found
    rde_disjoint_set    nets;     // the nets' numbers, joined into sets: one set, one net of the flat circuit
    rde_arr TYPE(fse_declared) declared;
    b8                  ok;       // false once anything has gone wrong; the walk stops at the next part
} fse;

// A composite's parameters where its parts are (their names and values), for its parts' values that name them. Only
// the composite directly round a part is looked in.
typedef struct {
    const fude_sim_param* names;
    const f64*            values;
    u32                   count;
} fse_scope;

// See sim.h: every array on the std allocator.
void fude_sim_flat_init(fude_sim_flat* _flat) {
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_memset(_flat, 0, sizeof(*_flat));
    _flat->prims      = rde_arr_new(sizeof(fude_sim_prim), _heap);
    _flat->ports      = rde_arr_new(sizeof(fude_sim_port), _heap);
    _flat->port_nets  = rde_arr_new(sizeof(u32), _heap);
    _flat->net_domain = rde_arr_new(sizeof(u8), _heap);
    _flat->errors     = rde_str_new(NULL, _heap);
    _flat->by_path    = rde_hash_map_new_ex(RDE_HASH_MAP_SLOT_STR_INLINE, FUDE_SIM_PATH, RDE_HASH_MAP_SLOT_BYTES, sizeof(u32), NULL, NULL, NULL, _heap);
}

// See sim.h.
void fude_sim_flat_destroy(fude_sim_flat* _flat) {
    rde_arr_free(&_flat->prims);
    rde_arr_free(&_flat->ports);
    rde_arr_free(&_flat->port_nets);
    rde_arr_free(&_flat->net_domain);
    rde_str_free(&_flat->errors);
    rde_hash_map_free(&_flat->by_path);
}

// Appends _line, and a line break, to what is wrong.
RDE_INTERNAL void fse_say(fude_sim_flat* _flat, const c8* _line) {
    rde_str_append_str(&_flat->errors, _line);
    rde_str_append_str(&_flat->errors, "\n");
}

// See sim.h.
const c8* fude_sim_flat_errors(const fude_sim_flat* _flat) {
    return rde_str_to_char_ptr(&_flat->errors);
}

// Returns a new net's number: a set of its own.
RDE_INTERNAL u32 fse_new_net(fse* _e) {
    return rde_disjoint_set_add(&_e->nets);
}

// Writes a part's parameters' values into _out (its definition's _names, _count of them; _out holds at least _count,
// since a custom part's parameters are as many as it has): its defaults, then what its text says over them. The text
// is words split by spaces, tabs, commas and semicolons, cut to 127 bytes: "name=value", or a bare value, which takes
// the next parameter in order. A value is a number as fude_sim_number reads it, or the name of a parameter of the
// composite round it (_scope), which gives that one's value. A name or a value that is neither is ignored.
RDE_INTERNAL void fse_params(const fude_sim_param* _names, u32 _count, const c8* _text, const fse_scope* _scope, f64* _out) {
    for(u32 _k = 0; _k < _count; _k++) {
        _out[_k] = _names[_k].value;
    }
    c8 _buf[128];
    rde_snprintf(_buf, sizeof(_buf), "%s", _text != NULL ? _text : "");
    u32 _bare = 0;
    c8* _tok_rest = NULL;   // Where rde_strtok goes on from.
    for(c8* _tok = rde_strtok(_buf, " \t,;", &_tok_rest); _tok != NULL; _tok = rde_strtok(NULL, " \t,;", &_tok_rest)) {
        c8* _eq = rde_strchr(_tok, '=');
        u32 _k = FUDE_SIM_NONE;
        const c8* _value = _tok;
        if(_eq != NULL) {
            *_eq = 0;
            _value = _eq + 1;
            for(u32 _i = 0; _i < _count; _i++) {
                if(rde_strcmp(_names[_i].name, _tok) == 0) {
                    _k = _i;
                }
            }
        } else {
            _k = _bare++;   // A value alone: the parameters in their order.
        }
        if(_k == FUDE_SIM_NONE || _k >= _count) {
            continue;
        }
        f64 _v;
        if(fude_sim_number(_value, &_v)) {
            _out[_k] = _v;
            continue;
        }
        for(u32 _i = 0; _scope != NULL && _i < _scope->count; _i++) {
            if(rde_strcmp(_scope->names[_i].name, _value) == 0) {
                _out[_k] = _scope->values[_i];
            }
        }
    }
}

// The room a primitive's ports are made in (its model's ports(), which stops at the room it is given). Models are
// code, and none makes this many (a memory, the most, 46), so it never cuts one off; a custom part's ports, which are
// as many as a person makes, are never held in a fixed-size array.
#define FSE_MODEL_PORTS 64u

// Puts _def's parts into the flat circuit: its ports on _port_nets (the nets outside, _port_count of them, one for
// each of its ports, or FUDE_SIM_NONE for one unjoined; NULL for the top, which has none; a port past _port_count is
// taken as unjoined), its parts' paths under _path, its parameters' values _values (one for each of _def's
// parameters). A primitive is copied out; a composite has its declared ports noted for the check and is expanded in
// turn, one level deeper. _tag is what the primitives are tagged with: at the top each part's own tag, and below it
// the tag of the top's part they are inside. Stops, with ok false and the error said, at a part whose definition is
// missing or past FUDE_SIM_DEPTH levels. Each part's ports, nets and parameters are kept in arrays that grow (a
// custom part has as many as it has), made once for the call; a part expanded inside reads its caller's, which stay
// as they are until it returns.
RDE_INTERNAL void fse_expand(fse* _e, const fude_sim_def* _def, const u32* _port_nets, u32 _port_count, const c8* _path, u64 _tag, u32 _depth, const f64* _values) {
    if(_depth > FUDE_SIM_DEPTH) {
        c8 _say[256];
        rde_snprintf(_say, sizeof(_say), "error: %s is made of itself (parts in parts deeper than %u)", _def->id, FUDE_SIM_DEPTH);
        fse_say(_e->flat, _say);
        _e->ok = false;
        return;
    }
    // Its nets: those its ports are on are the nets outside; the rest are new.
    const u32 _nn = (u32)rde_arr_length(&_def->nets);
    rde_arr _map_arr = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    rde_arr_resize(&_map_arr, _nn);
    u32* _map = (u32*)_map_arr.memory;   // Nothing is added to it after, so the pointer stays good.
    for(u32 _i = 0; _i < _nn; _i++) {
        _map[_i] = FUDE_SIM_NONE;
    }
    const u32* _pn = (const u32*)_def->port_nets.memory;
    for(u32 _p = 0; _p < (u32)rde_arr_length(&_def->port_nets); _p++) {
        if(_pn[_p] >= _nn) {
            continue;
        }
        const u32 _outside = _port_nets != NULL && _p < _port_count ? _port_nets[_p] : FUDE_SIM_NONE;
        if(_outside == FUDE_SIM_NONE) {
            continue;
        }
        if(_map[_pn[_p]] == FUDE_SIM_NONE) {
            _map[_pn[_p]] = _outside;
        } else {
            rde_disjoint_set_union(&_e->nets, _map[_pn[_p]], _outside);   // Two ports on one net inside: their nets outside are one.
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
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _ports_arr  = rde_arr_new(sizeof(fude_sim_port), _heap);   // each part's ports in turn
    rde_arr _nets_arr   = rde_arr_new(sizeof(u32), _heap);             // the nets they are on out here
    rde_arr _values_arr = rde_arr_new(sizeof(f64), _heap);             // its parameters' values
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_def->insts) && _e->ok; _i++) {
        const fude_sim_def* _what = fude_sim_lib_find(_e->lib, _in[_i].def);
        c8 _sub[FUDE_SIM_PATH];
        rde_snprintf(_sub, sizeof(_sub), "%s%s%s", _path, _path[0] != 0 ? "/" : "", _in[_i].ref[0] != 0 ? _in[_i].ref : _in[_i].def);
        if(_what == NULL) {
            c8 _say[256];
            rde_snprintf(_say, sizeof(_say), "error: %s is a %s, and there is no such part", _sub, _in[_i].def);
            fse_say(_e->flat, _say);
            _e->ok = false;
            break;
        }
        // Its path, "<path>/<its reference>" (its definition's id when it has none), and its tag.
        const u64 _t = _depth == 0u ? _in[_i].tag : _tag;
        // Its parameters' values: room for every one it has, and never less than a primitive keeps (its params).
        const u32 _nparams = (u32)rde_arr_length(&_what->params);
        rde_arr_clear(&_values_arr);
        rde_arr_resize(&_values_arr, _nparams > FUDE_SIM_PARAMS ? _nparams : FUDE_SIM_PARAMS);   // (zeros)
        f64* _p = (f64*)_values_arr.memory;
        fse_params((const fude_sim_param*)_what->params.memory, _nparams, _in[_i].params, &_scope, _p);
        // Its ports: a primitive's as its model makes them for these parameters, a composite's as declared, every one;
        // then their nets out here (a port it was not given: a net of its own, joined to nothing).
        u32 _np = 0;
        rde_arr_clear(&_ports_arr);
        if(_what->kind == FUDE_SIM_PRIMITIVE && _what->model != NULL && _what->model->ports != NULL) {
            rde_arr_resize(&_ports_arr, FSE_MODEL_PORTS);
            _np = _what->model->ports(_p, (fude_sim_port*)_ports_arr.memory, FSE_MODEL_PORTS);
        } else {
            _np = (u32)rde_arr_length(&_what->ports);
            rde_arr_resize(&_ports_arr, _np);
            if(_np > 0u) {
                rde_memcpy(_ports_arr.memory, _what->ports.memory, (usize)_np * sizeof(fude_sim_port));
            }
        }
        const fude_sim_port* _ports = (const fude_sim_port*)_ports_arr.memory;
        rde_arr_clear(&_nets_arr);
        rde_arr_resize(&_nets_arr, _np);
        u32* _nets = (u32*)_nets_arr.memory;
        for(u32 _k = 0; _k < _np; _k++) {
            const u32 _local = _k < _in[_i].count ? _inets[_in[_i].first + _k] : FUDE_SIM_NONE;
            _nets[_k] = _local != FUDE_SIM_NONE && _local < _nn ? _map[_local] : fse_new_net(_e);
        }
        if(_what->kind == FUDE_SIM_PRIMITIVE) {
            fude_sim_prim _prim;
            rde_memset(&_prim, 0, sizeof(_prim));
            _prim.model = _what->model;
            rde_snprintf(_prim.path, sizeof(_prim.path), "%s", _sub);
            _prim.first = (u32)rde_arr_length(&_e->flat->ports);
            _prim.count = _np;
            rde_memcpy(_prim.params, _p, sizeof(_prim.params));
            _prim.state = _e->flat->state;
            _prim.tag   = _t;
            _e->flat->state += fude_sim_model_states(_what->model, _prim.params);
            for(u32 _k = 0; _k < _np; _k++) {
                rde_arr_add(&_e->flat->ports, (any)&_ports[_k]);
                rde_arr_add(&_e->flat->port_nets, (any)&_nets[_k]);
            }
            const u32 _index = (u32)rde_arr_length(&_e->flat->prims);
            rde_arr_add(&_e->flat->prims, (any)&_prim);
            rde_hash_map_add(&_e->flat->by_path, (any)_prim.path, (any)&_index);   // (a path there already: the first kept)
        } else {
            // Its ports as it says they are: what is joined to them out here, and inside, is checked against them in
            // fude_sim_elaborate.
            const fude_sim_port* _dp = (const fude_sim_port*)_what->ports.memory;
            for(u32 _k = 0; _k < _np; _k++) {
                fse_declared _d;
                _d.net    = _nets[_k];
                _d.domain = _dp[_k].domain;
                rde_snprintf(_d.what, sizeof(_d.what), "%s's %s", _sub, _dp[_k].name);
                rde_arr_add(&_e->declared, (any)&_d);
            }
            fse_expand(_e, _what, _nets, _np, _sub, _t, _depth + 1u, _p);
        }
    }
    rde_arr_free(&_values_arr);
    rde_arr_free(&_nets_arr);
    rde_arr_free(&_ports_arr);
    rde_arr_free(&_map_arr);
}

// See sim.h. In three steps: the walk (fse_expand), the nets numbered from the disjoint set (the ports' and the
// declared ports' nets renumbered to them), then the domains checked. A net may hold logic and electric ports together
// (to be bridged); a port on a net that also holds any other domain is an error, one line for each such port, naming
// the first domain on its net that it may not meet (fse_wrong_domain: never the electric a logic port is bridged to).
b8 fude_sim_elaborate(const fude_sim_lib* _lib, const fude_sim_def* _top, fude_sim_flat* _flat) {
    rde_arr_clear(&_flat->prims);
    rde_arr_clear(&_flat->ports);
    rde_arr_clear(&_flat->port_nets);
    rde_arr_clear(&_flat->net_domain);
    rde_str_clear(&_flat->errors);
    rde_hash_map_clear(&_flat->by_path);
    _flat->nets  = 0;
    _flat->state = 0;
    fse _e = { _flat, _lib, rde_disjoint_set_new(0u, rde_memory_allocator_get_default_std()), rde_arr_new(sizeof(fse_declared), rde_memory_allocator_get_default_std()), true };
    // The top's parameters' values, its defaults: every one of them, as its parts' values may name any.
    rde_arr _values = rde_arr_new(sizeof(f64), rde_memory_allocator_get_default_std());
    rde_arr_resize(&_values, rde_arr_length(&_top->params));
    for(u32 _k = 0; _k < (u32)rde_arr_length(&_top->params); _k++) {
        ((f64*)_values.memory)[_k] = ((const fude_sim_param*)_top->params.memory)[_k].value;
    }
    fse_expand(&_e, _top, NULL, 0u, "", 0u, 0u, (const f64*)_values.memory);
    rde_arr_free(&_values);
    // The nets: each joined set one, numbered from 0 in the order of its lowest number.
    const u32 _raw = rde_disjoint_set_length(&_e.nets);
    rde_arr _numbers = rde_arr_new(sizeof(u32), rde_memory_allocator_get_default_std());
    rde_arr_resize(&_numbers, _raw);
    u32* _number = (u32*)_numbers.memory;
    for(u32 _i = 0; _i < _raw; _i++) {
        _number[_i] = FUDE_SIM_NONE;
    }
    u32 _nets = 0;
    for(u32 _i = 0; _i < _raw; _i++) {
        const u32 _r = rde_disjoint_set_find(&_e.nets, _i);
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
    rde_disjoint_set_free(&_e.nets);
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
                const u32 _o = fse_wrong_domain(_d[_pn[_port]], _ports[_port].domain);
                if(_o < FUDE_SIM_DOMAINS) {
                    c8 _say[256];
                    rde_snprintf(_say, sizeof(_say), "error: %s's %s (%s) is joined to %s connection", _prims[_p].path, _ports[_port].name,
                                 FSE_DOMAINS[_ports[_port].domain], FSE_DOMAINS_A[_o]);
                    fse_say(_flat, _say);
                    _e.ok = false;
                }
            }
        }
    }
    // Custom parts' ports: what they say they are against what their nets hold.
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_e.declared) && _nets > 0u; _i++) {
        const u32 _o = fse_wrong_domain(((const u8*)_flat->net_domain.memory)[_dec[_i].net], _dec[_i].domain);
        if(_o < FUDE_SIM_DOMAINS) {
            c8 _say[320];
            rde_snprintf(_say, sizeof(_say), "error: %s (%s) is joined to %s connection", _dec[_i].what, FSE_DOMAINS[_dec[_i].domain], FSE_DOMAINS_A[_o]);
            fse_say(_flat, _say);
            _e.ok = false;
        }
    }
    rde_arr_free(&_e.declared);
    return _e.ok;
}

// See sim.h: looked up by path (by_path, made as the primitives are put in). A path too long to be one is none.
u32 fude_sim_flat_find(const fude_sim_flat* _flat, const c8* _path) {
    if(_path == NULL || rde_strlen(_path) >= FUDE_SIM_PATH) {
        return FUDE_SIM_NONE;
    }
    const u32* _at = (const u32*)rde_hash_map_find(&_flat->by_path, (any)_path);
    return _at != NULL ? *_at : FUDE_SIM_NONE;
}
