// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/logic.h"
#include "zoom/symbol.h"
#include "zoom/placer.h"
#include "drawing/base/utf8.h"

// ===========================================================================
// See logic.h.
//
// Four parts, in order: the library of definitions and the parts made from them (both statics of this file, kept
// for the app's life); a circuit's logic, made (fude_zoom_logic_build writes the circuit's logic parts as one
// netlist of sim/'s and elaborates it) and run; a custom part made of what is drawn (another netlist, from the same
// parts); and a definition's inside drawn back onto the canvas. Every netlist names its instances and nets by
// convention, which the code reads back by name: in a circuit's, "P<object>" a part, "n<node>" a logic node's net,
// "r<node>" its probe, "b<bridge>" a bridge's net with "i<bridge>" its input and "o<bridge>" its probe, and "zero" a
// net tied low; in a custom part's, "U<n>" a part and "J<n>" a bridge between two ports on one node (a buffer of no
// delay). The file's own names start with fzlg_ and FZLG_ (plot.c's are fzl_, listen.c's fzls_).
// ===========================================================================

// --- the library ---------------------------------------------------------------------------------

// The library (fude_zoom_logic_library), made on first use.
RDE_INTERNAL fude_sim_lib fzlg_lib;
RDE_INTERNAL b8           fzlg_lib_made = false;
RDE_INTERNAL u32          fzlg_lib_revision = 0;   // (fude_zoom_logic_revision)

// See the header. It starts with the simulation's models and the chips.
fude_sim_lib* fude_zoom_logic_library(void) {
    if(!fzlg_lib_made) {
        fude_sim_lib_init(&fzlg_lib);
        fude_sim_chips_add(&fzlg_lib);
        fzlg_lib_made = true;
    }
    return &fzlg_lib;
}

// See the header.
const fude_sim_def* fude_zoom_logic_learn(const c8* _text, usize _size, c8* _error, usize _error_size) {
    fude_sim_def* _def = fude_sim_def_read(_text, _size, _error, _error_size);
    if(_def == NULL) {
        return NULL;
    }
    fude_sim_lib* _lib = fude_zoom_logic_library();
    const u32 _at = fude_sim_lib_add(_lib, _def);
    fzlg_lib_revision++;
    return ((fude_sim_def* const*)_lib->defs.memory)[_at];
}

// See the header.
u32 fude_zoom_logic_revision(void) {
    return fzlg_lib_revision;
}

// See the header. Only the text's first line is read, its spaces at both ends off. A forgotten custom part (its id
// emptied) is never found.
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
        if(_d[_i]->kind == FUDE_SIM_COMPOSITE && _d[_i]->id[0] != 0 && rde_strcmp(_d[_i]->name, _name) == 0) {
            return _d[_i];
        }
    }
    // (a name it had: a custom part's id is its first name's — one renamed is found by what it was called)
    c8 _id[FUDE_SIM_ID];
    rde_snprintf(_id, sizeof(_id), "user/");
    fude_zoom_logic_slug(_name, _id + 5, sizeof(_id) - 5u);
    _def = fude_sim_lib_find(_lib, _id);
    return _def != NULL && _def->id[0] != 0 ? _def : NULL;
}

// See the header. Upper-case ASCII letters are lowered; a dash is never first or last.
void fude_zoom_logic_slug(const c8* _name, c8* _out, usize _size) {
    usize _k = 0;
    b8 _dash = false;
    for(const c8* _c = _name; *_c != 0 && _k + 1u < _size; _c++) {
        const c8 _l = (*_c >= 'A' && *_c <= 'Z') ? (c8)(*_c - 'A' + 'a') : *_c;
        if((_l >= 'a' && _l <= 'z') || (_l >= '0' && _l <= '9')) {
            _out[_k++] = _l;
            _dash = false;
        } else if(!_dash && _k > 0u) {
            _out[_k++] = '-';
            _dash = true;
        }
    }
    while(_k > 0u && _out[_k - 1u] == '-') {
        _k--;
    }
    if(_k == 0u) {
        rde_snprintf(_out, _size, "part");
        return;
    }
    _out[_k] = 0;
}

// See the header. Only a definition with an id can be renamed; the name is changed in place, in the library.
b8 fude_zoom_logic_rename(const c8* _id, const c8* _name) {
    fude_sim_def* _def = (fude_sim_def*)fude_sim_lib_find(fude_zoom_logic_library(), _id);
    if(_def == NULL || _def->id[0] == 0 || _name == NULL || _name[0] == 0) {
        return false;
    }
    rde_snprintf(_def->name, sizeof(_def->name), "%s", _name);
    fzlg_lib_revision++;
    return true;
}

// See the header. Its id and name are emptied, so no search finds it, but the definition is not freed.
b8 fude_zoom_logic_forget_part(const c8* _id) {
    fude_sim_def* _def = (fude_sim_def*)fude_sim_lib_find(fude_zoom_logic_library(), _id);
    if(_def == NULL || rde_strncmp(_def->id, "user/", 5u) != 0) {
        return false;
    }
    // (kept, unfindable: what was made of it still holds it)
    _def->id[0] = 0;
    _def->name[0] = 0;
    fzlg_lib_revision++;
    return true;
}

// See the header. Each port is ranked by its old place, a new one after all of them in its own order, and the ports
// and their nets sorted by rank together (an insertion sort, which keeps equal ranks in order).
void fude_zoom_logic_keep_order(fude_sim_def* _new, const fude_sim_def* _old) {
    const u32 _n = (u32)rde_arr_length(&_new->ports);
    if(_old == NULL || _n < 2u) {
        return;
    }
    fude_sim_port* _p = (fude_sim_port*)_new->ports.memory;
    u32* _nets = (u32*)_new->port_nets.memory;
    // Each new port's place: its old one's, by its name; those new to it after them, as they were.
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr _rank_arr = rde_arr_new(sizeof(u32), _heap);
    rde_arr_resize(&_rank_arr, _n);
    u32* _rank = (u32*)_rank_arr.memory;
    const u32 _no = (u32)rde_arr_length(&_old->ports);
    for(u32 _i = 0; _i < _n; _i++) {
        _rank[_i] = _no + _i;
        for(u32 _k = 0; _k < _no; _k++) {
            if(rde_strcmp(((const fude_sim_port*)_old->ports.memory)[_k].name, _p[_i].name) == 0) {
                _rank[_i] = _k;
                break;
            }
        }
    }
    for(u32 _i = 1; _i < _n; _i++) {
        for(u32 _j = _i; _j > 0u && _rank[_j] < _rank[_j - 1u]; _j--) {
            const u32 _r = _rank[_j]; _rank[_j] = _rank[_j - 1u]; _rank[_j - 1u] = _r;
            const fude_sim_port _q = _p[_j]; _p[_j] = _p[_j - 1u]; _p[_j - 1u] = _q;
            const u32 _t = _nets[_j]; _nets[_j] = _nets[_j - 1u]; _nets[_j - 1u] = _t;
        }
    }
    rde_arr_free(&_rank_arr);
}

// --- parts made from definitions -----------------------------------------------------------------

// The most pins a part made from a definition has: as many as a fude_zoom_part's pin_count holds (a u16), far past
// anything a person makes. Every port up to it gets its pin.
#define FZLG_PINS_MOST 0xFFFFu

// A pin's name, as a made part keeps it.
typedef c8 fzlg_pin_name[FUDE_SIM_NAME];

// A part made from a definition (fude_zoom_logic_part): the part itself, and what it was made from (the template, a
// package or a block, the definition's id and version), so it is found again rather than made twice. Its pins and
// their names are arrays of its own on the std allocator, one for each of the definition's ports, sized once and
// never grown, so the part's pointers into them stay good: like the made part itself, they are never freed.
typedef struct {
    fude_zoom_part        part;
    const fude_zoom_part* template_of;
    b8                    package;
    c8                    id[FUDE_SIM_ID];
    u32                   version;
    rde_arr TYPE(fude_zoom_pin)  pins;
    rde_arr TYPE(fzlg_pin_name)  names;
} fzlg_made;

// Every part made so far, as pointers; the array is made on first use.
RDE_INTERNAL rde_arr TYPE(fzlg_made*) fzlg_parts;   // (each kept for good: circuits hold their parts)

// Pin _k of _n round a chip's package: the first half down its left, the rest up its right.
RDE_INTERNAL fude_zoom_pin fzlg_package_pin(u32 _n, u32 _k) {
    const u32 _half = (_n + 1u) / 2u;
    if(_k < _half) {
        return (fude_zoom_pin){ -1.0f, 1.0f - 2.0f * (f32)(_k + 1u) / (f32)(_half + 1u), NULL, FUDE_ZOOM_PIN_LEFT };
    }
    return (fude_zoom_pin){ 1.0f, -1.0f + 2.0f * (f32)(_k - _half + 1u) / (f32)(_half + 1u), NULL, FUDE_ZOOM_PIN_RIGHT };
}

// See the header. The parts made are searched in full. A block's pins are spread along each side in their order:
// inputs on the left, outputs on the right, power above and below.
const fude_zoom_part* fude_zoom_logic_part(const fude_zoom_part* _template, const fude_sim_def* _def, b8 _package) {
    if(_template == NULL || _def == NULL) {
        return NULL;
    }
    if(!rde_arr_is_inited(&fzlg_parts)) {
        fzlg_parts = rde_arr_new(sizeof(fzlg_made*), rde_memory_allocator_get_default_std());
    }
    fzlg_made* const* _m = (fzlg_made* const*)fzlg_parts.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&fzlg_parts); _i++) {
        if(_m[_i]->template_of == _template && _m[_i]->package == _package && _m[_i]->version == _def->version && rde_strcmp(_m[_i]->id, _def->id) == 0) {
            return &_m[_i]->part;
        }
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    fzlg_made* _new = (fzlg_made*)_heap->calloc(_heap->allocator, 1, sizeof(fzlg_made));
    _new->template_of = _template;
    _new->package     = _package;
    _new->version     = _def->version;
    rde_snprintf(_new->id, sizeof(_new->id), "%s", _def->id);
    const fude_sim_port* _ports = (const fude_sim_port*)_def->ports.memory;
    const u32 _n = (u32)rde_arr_length(&_def->ports) < FZLG_PINS_MOST ? (u32)rde_arr_length(&_def->ports) : FZLG_PINS_MOST;
    _new->pins  = rde_arr_new(sizeof(fude_zoom_pin), _heap);
    _new->names = rde_arr_new(sizeof(fzlg_pin_name), _heap);
    rde_arr_resize(&_new->pins, _n);
    rde_arr_resize(&_new->names, _n);
    fude_zoom_pin* _pins  = (fude_zoom_pin*)_new->pins.memory;
    fzlg_pin_name* _names = (fzlg_pin_name*)_new->names.memory;
    // A block's: each side's ports spread along it, in their order (inputs left, outputs right, power above and below).
    u32 _on[4] = { 0, 0, 0, 0 }, _seen[4] = { 0, 0, 0, 0 };
    for(u32 _k = 0; _k < _n; _k++) {
        _on[_ports[_k].side & 3u]++;
    }
    for(u32 _k = 0; _k < _n; _k++) {
        rde_snprintf(_names[_k], sizeof(_names[_k]), "%s", _ports[_k].name);
        fude_zoom_pin _p;
        if(_package) {
            _p = fzlg_package_pin(_n, _k);
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
        _p.name = _names[_k];
        _pins[_k] = _p;
    }
    _new->part           = *_template;
    _new->part.id        = _new->id;
    _new->part.pins      = _pins;
    _new->part.pin_count = (u16)_n;
    rde_arr_add(&fzlg_parts, (any)&_new);
    return &_new->part;
}

// --- a circuit's logic: made ---------------------------------------------------------------------

// See the header.
b8 fude_zoom_logic_is(const fude_zoom_part* _part) {
    if(_part == NULL) {
        return false;
    }
    const u8 _m = _part->model;
    return _m == FUDE_ZOOM_MODEL_GATE || _m == FUDE_ZOOM_MODEL_DFF || _m == FUDE_ZOOM_MODEL_TFF || _m == FUDE_ZOOM_MODEL_JKFF ||
           _m == FUDE_ZOOM_MODEL_SRLATCH || _m == FUDE_ZOOM_MODEL_LOGIC_IN ||
           _m == FUDE_ZOOM_MODEL_LOGIC_OUT || _m == FUDE_ZOOM_MODEL_SIM;
}

// See the header. Every array is on the std allocator.
void fude_zoom_logic_init(fude_zoom_logic* _l) {
    rde_memset(_l, 0, sizeof(*_l));
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

// Lets the run and its netlist go, and marks it off. Its flat circuit is kept, to be cleared when it is made again.
RDE_INTERNAL void fzlg_drop_run(fude_zoom_logic* _l) {
    if(_l->on) {
        fude_sim_run_destroy(&_l->run);
    }
    if(_l->netlist != NULL) {
        fude_sim_def_free(_l->netlist);
        _l->netlist = NULL;
    }
    _l->on = false;
}

// See the header.
void fude_zoom_logic_forget(fude_zoom_logic* _l) {
    fzlg_drop_run(_l);
}

// See the header. The struct is zeroed after.
void fude_zoom_logic_destroy(fude_zoom_logic* _l) {
    fzlg_drop_run(_l);
    fude_sim_flat_destroy(&_l->flat);
    rde_arr_free(&_l->bridges);
    rde_arr_free(&_l->part_bridges);
    rde_arr_free(&_l->node_net);
    rde_arr_free(&_l->part_input);
    rde_arr_free(&_l->part_probe);
    rde_arr_free(&_l->part_level);
    rde_str_free(&_l->errors);
    rde_memset(_l, 0, sizeof(*_l));
}

// The simulation's model for each kind of gate.
static const c8* const FZLG_GATES[8] = { "and", "or", "not", "nand", "nor", "xor", "xnor", "buf" };   // (FUDE_ZOOM_GATE_'s order)

// Writes what part _p is in the netlist: its definition's id into _def, its parameters into _params, for each of its
// pins the definition's port it is into _port_of_pin (room for its pins, FZLG_PIN_ROOM at least: a flip-flop's are
// written by number), and how many ports the definition has into *_ports (the ports no pin is are tied low). Returns
// false when it is not a logic part, or its definition is not in the library.
RDE_INTERNAL b8 fzlg_what(const fude_zoom_circuit_part* _p, c8* _def, usize _def_size, c8* _params, usize _params_size, u32* _port_of_pin,
                         u32* _ports) {
    const fude_zoom_part* _part = _p->part;
    _params[0] = 0;
    for(u32 _k = 0; _k < _part->pin_count; _k++) {
        _port_of_pin[_k] = _k;
    }
    switch(_part->model) {
    case FUDE_ZOOM_MODEL_GATE: {
        rde_snprintf(_def, _def_size, "%s", FZLG_GATES[_part->gate & 7u]);
        const u32 _inputs = _part->pin_count > 1u ? _part->pin_count - 1u : 1u;
        if(_part->gate != FUDE_ZOOM_GATE_NOT && _part->gate != FUDE_ZOOM_GATE_BUFFER) {
            rde_snprintf(_params, _params_size, "inputs=%u", _inputs);
        }
        *_ports = _part->pin_count;
        return true;
    }
    case FUDE_ZOOM_MODEL_DFF:
    case FUDE_ZOOM_MODEL_TFF:
        // (D or T, CLK, Q, QN: the model's R between, tied low)
        rde_snprintf(_def, _def_size, "%s", _part->model == FUDE_ZOOM_MODEL_DFF ? "dff" : "tff");
        _port_of_pin[2] = 3u;
        _port_of_pin[3] = 4u;
        *_ports = 5u;
        return true;
    case FUDE_ZOOM_MODEL_JKFF:
        // (J, CLK, K, Q, QN as the model's J, K, CLK, R, Q, QN: its R tied low)
        rde_snprintf(_def, _def_size, "jkff");
        _port_of_pin[0] = 0u;
        _port_of_pin[1] = 2u;
        _port_of_pin[2] = 1u;
        _port_of_pin[3] = 4u;
        _port_of_pin[4] = 5u;
        *_ports = 6u;
        return true;
    case FUDE_ZOOM_MODEL_SRLATCH:
        rde_snprintf(_def, _def_size, "srlatch");   // (S, R, Q, QN: as its pins)
        *_ports = 4u;
        return true;
    case FUDE_ZOOM_MODEL_LOGIC_IN:
        rde_snprintf(_def, _def_size, "input");
        rde_snprintf(_params, _params_size, "%u", (u32)(_p->switch_on & 1u));
        *_ports = 1u;
        return true;
    case FUDE_ZOOM_MODEL_LOGIC_OUT:
        rde_snprintf(_def, _def_size, "probe");
        *_ports = 1u;
        return true;
    case FUDE_ZOOM_MODEL_SIM: {
        const fude_sim_def* _d = fude_zoom_logic_find(_part->id);
        if(_d == NULL) {
            return false;
        }
        rde_snprintf(_def, _def_size, "%s", _d->id);
        *_ports = (u32)rde_arr_length(&_d->ports);
        return true;
    }
    default:
        return false;
    }
}

// Writes pin _pin's domain and way into *_domain and *_dir: a custom part's or a chip's as its definition's port says,
// a built-in part's logic, its way as its pin is (a gate's last pin and a flip-flop's Q and QN are outputs).
RDE_INTERNAL void fzlg_port(const fude_zoom_circuit_part* _p, u32 _pin, u8* _domain, u8* _dir) {
    *_domain = FUDE_SIM_LOGIC;
    *_dir    = FUDE_SIM_IN;
    const fude_zoom_part* _part = _p->part;
    switch(_part->model) {
    case FUDE_ZOOM_MODEL_GATE:      *_dir = _pin + 1u == _part->pin_count ? FUDE_SIM_OUT : FUDE_SIM_IN; return;
    case FUDE_ZOOM_MODEL_DFF:
    case FUDE_ZOOM_MODEL_TFF:       *_dir = _pin >= 2u ? FUDE_SIM_OUT : FUDE_SIM_IN; return;
    case FUDE_ZOOM_MODEL_JKFF:      *_dir = _pin >= 3u ? FUDE_SIM_OUT : FUDE_SIM_IN; return;
    case FUDE_ZOOM_MODEL_SRLATCH:   *_dir = _pin >= 2u ? FUDE_SIM_OUT : FUDE_SIM_IN; return;
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

// Writes a chip's or a custom part's supply and ground pins into *_supply and *_ground: an electric pin named VCC,
// VDD or V+, and one named GND or VSS. FUDE_ZOOM_NONE each where it has none, as every built-in part has.
RDE_INTERNAL void fzlg_power(const fude_zoom_circuit_part* _p, u32* _supply, u32* _ground) {
    *_supply = FUDE_ZOOM_NONE;
    *_ground = FUDE_ZOOM_NONE;
    if(_p->part->model != FUDE_ZOOM_MODEL_SIM) {
        return;
    }
    for(u32 _k = 0; _k < _p->part->pin_count; _k++) {
        u8 _domain, _dir;
        fzlg_port(_p, _k, &_domain, &_dir);
        const c8* _n = _p->part->pins[_k].name;
        if(_domain != FUDE_SIM_ELECTRIC || _n == NULL) {
            continue;
        }
        if(rde_strcmp(_n, "VCC") == 0 || rde_strcmp(_n, "VDD") == 0 || rde_strcmp(_n, "V+") == 0) {
            *_supply = _k;
        } else if(rde_strcmp(_n, "GND") == 0 || rde_strcmp(_n, "VSS") == 0) {
            *_ground = _k;
        }
    }
}

// Returns the net that the primitive named _ref in the flat circuit is on (its first port's: a probe's, what it
// reads); FUDE_SIM_NONE when there is no such primitive or it has no port.
RDE_INTERNAL u32 fzlg_net_of(const fude_sim_flat* _flat, const c8* _ref) {
    const u32 _p = fude_sim_flat_find(_flat, _ref);
    if(_p == FUDE_SIM_NONE) {
        return FUDE_SIM_NONE;
    }
    const fude_sim_prim* _pr = &((const fude_sim_prim*)_flat->prims.memory)[_p];
    return _pr->count > 0u ? ((const u32*)_flat->port_nets.memory)[_pr->first] : FUDE_SIM_NONE;
}

// Marks in _analog (one for each of the circuit's _nn nodes, and one more) the nodes the analog circuit is on: ground's
// (0), and each node a pin of the _np parts _p is on that is not a logic pin. Returns how many of the parts are logic
// parts (fude_zoom_logic_is).
RDE_INTERNAL u32 fzlg_build_analog(const fude_zoom_circuit_part* _p, u32 _np, u32 _nn, u8* _analog) {
    _analog[0] = 1u;
    u32 _logic_parts = 0;
    for(u32 _i = 0; _i < _np; _i++) {
        const b8 _logic = fude_zoom_logic_is(_p[_i].part);
        _logic_parts += _logic ? 1u : 0u;
        for(u32 _k = 0; _k < _p[_i].part->pin_count; _k++) {
            const u32 _node = _p[_i].node[_k];
            if(_node == FUDE_ZOOM_NONE || _node >= _nn) {
                continue;
            }
            u8 _domain = FUDE_SIM_ELECTRIC, _dir;
            if(_logic) {
                fzlg_port(&_p[_i], _k, &_domain, &_dir);
            }
            if(_domain != FUDE_SIM_LOGIC) {
                _analog[_node] = 1u;
            }
        }
    }
    return _logic_parts;
}

// The room fzlg_what writes a part's ports in: its pins, and this many at least (a flip-flop's are written by number).
#define FZLG_PIN_ROOM 8u

// Scratch for writing one logic part into a netlist at a time (fzlg_build_netlist, fzlg_make_parts): for each of its
// pins the port it is and its net's name, and for each port the name of its net. Fitted to each part as it comes
// (fzlg_scratch_fit), so a part is written with every pin it has, however many; on the std allocator, freed with
// fzlg_scratch_free.
typedef struct {
    rde_arr TYPE(u32)           port_of_pin;
    rde_arr TYPE(fzlg_pin_name) names;
    rde_arr TYPE(const c8*)     nets;
} fzlg_scratch;

// Returns empty scratch.
RDE_INTERNAL fzlg_scratch fzlg_scratch_new(void) {
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    fzlg_scratch _sc = { rde_arr_new(sizeof(u32), _heap), rde_arr_new(sizeof(fzlg_pin_name), _heap), rde_arr_new(sizeof(const c8*), _heap) };
    return _sc;
}

// Fits the scratch to a part of _pins pins (FZLG_PIN_ROOM at least) and _ports ports (one at least). What it held is
// not kept; its arrays' memory may move, so their pointers are taken after.
RDE_INTERNAL void fzlg_scratch_fit(fzlg_scratch* _sc, u32 _pins, u32 _ports) {
    const u32 _room = _pins > FZLG_PIN_ROOM ? _pins : FZLG_PIN_ROOM;
    rde_arr_resize(&_sc->port_of_pin, _room);
    rde_arr_resize(&_sc->names, _room);
    rde_arr_resize(&_sc->nets, _ports > 0u ? _ports : 1u);
}

// Lets the scratch go.
RDE_INTERNAL void fzlg_scratch_free(fzlg_scratch* _sc) {
    rde_arr_free(&_sc->port_of_pin);
    rde_arr_free(&_sc->names);
    rde_arr_free(&_sc->nets);
}

// Writes the circuit's logic parts (of the _np parts _p, on _nn nodes) into netlist _list, as fude_zoom_logic_build
// says: each an instance "P" and its object, its pins' nets a logic node's own ("n" and the node, a probe "r" on it,
// its net found once it is made: _node_net) or, on a node the analog circuit is on (_analog), a bridge's ("b" and its
// index, added to _l's bridges, with its input "i" and its probe "o"); each part's bridges (first, how many) in _pb; a
// port no pin is on tied to the net "zero", its constant made once.
RDE_INTERNAL void fzlg_build_netlist(fude_zoom_logic* _l, fude_sim_def* _list, const fude_zoom_circuit_part* _p, u32 _np, u32 _nn, const u8* _analog,
                                    u32* _node_net, u32* _pb) {
    b8 _zero = false;
    fzlg_scratch _sc = fzlg_scratch_new();
    for(u32 _i = 0; _i < _np; _i++) {
        c8 _def[FUDE_SIM_ID], _params[96];
        u32 _ports = 0;
        fzlg_scratch_fit(&_sc, _p[_i].part->pin_count, 0u);
        if(!fude_zoom_logic_is(_p[_i].part) || !fzlg_what(&_p[_i], _def, sizeof(_def), _params, sizeof(_params), (u32*)_sc.port_of_pin.memory, &_ports)) {
            continue;
        }
        u32 _supply, _ground;
        fzlg_power(&_p[_i], &_supply, &_ground);
        rde_arr_resize(&_sc.nets, _ports > 0u ? _ports : 1u);
        const u32* _port_of_pin = (const u32*)_sc.port_of_pin.memory;
        fzlg_pin_name* _names = (fzlg_pin_name*)_sc.names.memory;
        const c8** _nets = (const c8**)_sc.nets.memory;
        for(u32 _k = 0; _k < _ports; _k++) {
            _nets[_k] = "zero";   // (a port no pin is: tied low)
        }
        _pb[2u * _i] = (u32)rde_arr_length(&_l->bridges);
        for(u32 _k = 0; _k < _p[_i].part->pin_count; _k++) {
            const u32 _port = _port_of_pin[_k], _node = _p[_i].node[_k];
            if(_port >= _ports) {
                continue;   // (a pin of a version of its part with more ports than it has now)
            }
            u8 _domain, _dir;
            fzlg_port(&_p[_i], _k, &_domain, &_dir);
            if(_domain != FUDE_SIM_LOGIC || _node == FUDE_ZOOM_NONE || _node >= _nn) {
                _nets[_port] = "-";   // (open; a supply pin: the analog circuit's)
                continue;
            }
            if(!_analog[_node]) {
                rde_snprintf(_names[_k], sizeof(_names[_k]), "n%u", _node);
                _nets[_port] = _names[_k];
                if(_node_net[_node] == FUDE_SIM_NONE) {
                    _node_net[_node] = 0u;   // (its net found once it is made)
                    c8 _probe[24];
                    rde_snprintf(_probe, sizeof(_probe), "r%u", _node);
                    const c8* _pn[1] = { _names[_k] };
                    fude_sim_def_inst(_list, "probe", _probe, "", _pn, 1u);
                }
                continue;
            }
            // A bridge: an input's level from the node (an input primitive on its net), an output's read off its net.
            const u32 _b = (u32)rde_arr_length(&_l->bridges);
            fude_zoom_bridge _br;
            rde_memset(&_br, 0, sizeof(_br));
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
            rde_snprintf(_names[_k], sizeof(_names[_k]), "b%u", _b);
            _nets[_port] = _names[_k];
            const c8* _bn[1] = { _names[_k] };
            c8 _ref[24];
            if(_br.way != FUDE_ZOOM_BRIDGE_OUT) {
                rde_snprintf(_ref, sizeof(_ref), "i%u", _b);
                fude_sim_def_inst(_list, "input", _ref, "0", _bn, 1u);
            }
            rde_snprintf(_ref, sizeof(_ref), "o%u", _b);
            fude_sim_def_inst(_list, "probe", _ref, "", _bn, 1u);
        }
        _pb[2u * _i + 1u] = (u32)rde_arr_length(&_l->bridges) - _pb[2u * _i];
        for(u32 _k = 0; _k < _ports; _k++) {
            _zero = _zero || rde_strcmp(_nets[_k], "zero") == 0;
        }
        c8 _ref[16];
        rde_snprintf(_ref, sizeof(_ref), "P%u", _p[_i].object);
        const u32 _inst = fude_sim_def_inst(_list, _def, _ref, _params, _nets, _ports);
        ((fude_sim_inst*)_list->insts.memory)[_inst].tag = _p[_i].object;
    }
    fzlg_scratch_free(&_sc);
    if(_zero) {
        const c8* _zn[1] = { "zero" };
        fude_sim_def_inst(_list, "const", "Z", "0", _zn, 1u);
    }
}

// See the header. The old flat circuit, run and netlist are kept aside until the new run has taken their state
// (fude_sim_run_keep), then freed. A logic pin on a node only logic pins are on joins the node's own net; one on a
// node the analog circuit is on (ground always is) gets a bridge: a net of its own, read by a probe, and, unless it
// only drives, fed by an input primitive. Once the netlist is elaborated, every net, input and probe is found again
// by its name.
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
    const u32 _logic_parts = fzlg_build_analog(_p, _np, _nn, _analog);
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
    // The netlist: each logic part an instance ("P" and its object: its index on the canvas, not in the circuit's
    // parts), its pins' nets — a logic node's own ("n" and its node), or a bridge's ("b" and its index) where the node
    // is the analog circuit's.
    fude_sim_def* _list = fude_sim_def_new("canvas/logic", "canvas");
    fzlg_build_netlist(_l, _list, _p, _np, _nn, _analog, _node_net, _pb);
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
                rde_snprintf(_probe, sizeof(_probe), "r%u", _n);
                _node_net[_n] = fzlg_net_of(&_l->flat, _probe);
            }
        }
        fude_zoom_bridge* _br = (fude_zoom_bridge*)_l->bridges.memory;
        for(u32 _b = 0; _b < (u32)rde_arr_length(&_l->bridges); _b++) {
            c8 _ref[24];
            rde_snprintf(_ref, sizeof(_ref), "i%u", _b);
            _br[_b].input = fude_sim_flat_find(&_l->flat, _ref);
            rde_snprintf(_ref, sizeof(_ref), "o%u", _b);
            _br[_b].net = fzlg_net_of(&_l->flat, _ref);
        }
        u8* _level = (u8*)_l->part_level.memory;
        for(u32 _i = 0; _i < _np; _i++) {
            c8 _ref[16];
            rde_snprintf(_ref, sizeof(_ref), "P%u", _p[_i].object);
            if(_p[_i].part->model == FUDE_ZOOM_MODEL_LOGIC_IN) {
                _pi[_i] = fude_sim_flat_find(&_l->flat, _ref);
                _level[_i] = 0xFFu;   // (set from its switch on the first step)
            } else if(_p[_i].part->model == FUDE_ZOOM_MODEL_LOGIC_OUT) {
                _pp[_i] = fzlg_net_of(&_l->flat, _ref);
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

// Returns the voltage of the node pin _pin of part _p is on: 0 (ground's) for no pin (FUDE_ZOOM_NONE) or a pin past
// its part's; an open pin's as fude_zoom_circuit_volts says.
RDE_INTERNAL f64 fzlg_volts(const fude_zoom_circuit* _c, const fude_zoom_circuit_part* _p, u32 _pin) {
    if(_pin == FUDE_ZOOM_NONE || _pin >= _p->part->pin_count) {
        return 0.0;
    }
    const u32 _node = _p->node[_pin];
    return fude_zoom_circuit_volts(_c, _node);
}

// See the header. Only a level that changed is set into the run. The settling is given a microsecond.
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
        const f64 _vg = fzlg_volts(_c, _q, _br[_b].ground);
        const f64 _vs = _br[_b].supply != FUDE_ZOOM_NONE ? fzlg_volts(_c, _q, _br[_b].supply) : _vg + FUDE_ZOOM_LOGIC_V;
        const f64 _v  = fude_zoom_circuit_volts(_c, _br[_b].node) - _vg;
        // (unpowered: low — not unknown: a flip-flop reading its reset as unknown as the circuit comes on would hold X
        // for ever (no edge ever clears it), where a real chip comes up in some state; its outputs
        // are not driven anyway)
        const u8  _r  = _vs - _vg < 1.0 ? FUDE_SIM_0 : (_v > 0.5 * (_vs - _vg) ? FUDE_SIM_1 : FUDE_SIM_0);
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

// See the header.
u8 fude_zoom_logic_node(const fude_zoom_logic* _l, u32 _node) {
    if(!_l->on || _node >= (u32)rde_arr_length(&_l->node_net)) {
        return 0xFFu;
    }
    const u32 _net = ((const u32*)_l->node_net.memory)[_node];
    return _net == FUDE_SIM_NONE ? 0xFFu : fude_sim_run_net(&_l->run, _net);
}

// See the header.
u8 fude_zoom_logic_probe(const fude_zoom_logic* _l, u32 _part) {
    if(!_l->on || _part >= (u32)rde_arr_length(&_l->part_probe)) {
        return 0xFFu;
    }
    const u32 _net = ((const u32*)_l->part_probe.memory)[_part];
    return _net == FUDE_SIM_NONE ? 0xFFu : fude_sim_run_net(&_l->run, _net);
}

// See the header. A part past those last built has none.
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

// A port of the custom part being made: the logic input or probe it is, its way, where it is, and its name.
typedef struct {
    u32 part;      // the circuit's
    u8  dir;       // FUDE_SIM_IN, FUDE_SIM_OUT
    f64 x, y;      // where it is (the home frame)
    c8  name[FUDE_SIM_NAME];
} fzlg_port_at;

// rde_sort's order for the ports: inputs before outputs, then from the top down, then from the left.
RDE_INTERNAL int fzlg_port_order(const any _a, const any _b) {
    const fzlg_port_at* _p = (const fzlg_port_at*)_a, *_q = (const fzlg_port_at*)_b;
    if(_p->dir != _q->dir) {
        return _p->dir < _q->dir ? -1 : 1;
    }
    if(rde_math_abs_f64(_p->y - _q->y) > 1e-9) {
        return _p->y > _q->y ? -1 : 1;   // (top first)
    }
    return _p->x < _q->x ? -1 : (_p->x > _q->x ? 1 : 0);
}

// Returns whether _t is a level a logic input's text says rather than a name: nothing, "-", or a word the circuit reads
// as its level (circuit.h's LEVELS: "0", "1", "on", "high", "bajo", "オフ"..., in any of the apps' languages and in any
// case), so that the words the circuit starts it by are never taken for its name.
RDE_INTERNAL b8 fzlg_level_word(const c8* _t) {
    return _t[0] == 0 || rde_strcmp(_t, "-") == 0 || fude_zoom_circuit_level_word(_t);
}

// Returns whether one of the first _i ports in _q is named _name.
RDE_INTERNAL b8 fzlg_port_named(const fzlg_port_at* _q, u32 _i, const c8* _name) {
    for(u32 _j = 0; _j < _i; _j++) {
        if(rde_strcmp(_q[_j].name, _name) == 0) {
            return true;
        }
    }
    return false;
}

// Reads the custom part's ports into _ports (fzlg_port_at): each logic input and probe of the _np parts _p, its way,
// where it is (the home frame _home of _s) and its name, the first word of its text where that is not a level (else
// none yet). Returns how many of the other parts are logic parts (what is inside it). It takes the ports' array's
// handle under the name the function had, and hands it back.
RDE_INTERNAL u32 fzlg_make_ports(const fude_zoom_scene* _s, const fude_zoom_circuit_part* _p, u32 _np, u32 _home, rde_arr* _ports) {
    rde_arr _ports_arr = *_ports;
    u32 _inside = 0;
    for(u32 _i = 0; _i < _np; _i++) {
        const u8 _m = _p[_i].part->model;
        if(_m != FUDE_ZOOM_MODEL_LOGIC_IN && _m != FUDE_ZOOM_MODEL_LOGIC_OUT) {
            _inside += fude_zoom_logic_is(_p[_i].part) ? 1u : 0u;
            continue;
        }
        fzlg_port_at _q;
        rde_memset(&_q, 0, sizeof(_q));
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
        c8 _word[FUDE_ZOOM_SYMBOL_TEXT];
        u32 _k = 0;
        for(const c8* _t = _line; *_t != 0 && *_t != '\n' && _k + 1u < sizeof(_word); _t++) {
            _word[_k++] = (*_t == ' ' || *_t == '"' || *_t == '\t' || *_t == '/') ? '_' : *_t;
        }
        fude_utf8_copy(_q.name, sizeof(_q.name), _word, _k);   // (cut before a whole character when it is longer)
        _k = (u32)rde_strlen(_q.name);
        while(_k > 0u && _q.name[_k - 1u] == '_') {
            _k--;
        }
        _q.name[_k] = 0;
        if(fzlg_level_word(_q.name)) {
            _q.name[0] = 0;
        }
        rde_arr_add(&_ports_arr, (any)&_q);
    }
    *_ports = _ports_arr;
    return _inside;
}

// Names the _nports ports _q (in their order): one with no name "IN" or "OUT" and its number among the inputs or the
// outputs; one named as a port before it made unique, as below.
RDE_INTERNAL void fzlg_make_names(fzlg_port_at* _q, u32 _nports) {
    u32 _ins = 0, _outs = 0;
    for(u32 _i = 0; _i < _nports; _i++) {
        if(_q[_i].name[0] == 0) {
            rde_snprintf(_q[_i].name, sizeof(_q[_i].name), _q[_i].dir == FUDE_SIM_IN ? "IN%u" : "OUT%u", _q[_i].dir == FUDE_SIM_IN ? ++_ins : ++_outs);
        }
        if(!fzlg_port_named(_q, _i, _q[_i].name)) {
            continue;
        }
        // Taken: its name cut at a whole character to leave room, then _<n>, n from its place on until no port before
        // it has that name. Each name before it rules out one n at most (the digits after its last _), so one of the
        // next _i + 1 is free and the search ends; the name is cut each time, so it never grows past its room.
        c8 _base[FUDE_SIM_NAME];
        fude_utf8_set(_base, sizeof(_base), _q[_i].name);
        for(u32 _n = _i + 1u; _n <= 2u * _i + 1u; _n++) {
            c8 _suffix[12];
            const usize _sl = (usize)rde_snprintf(_suffix, sizeof(_suffix), "_%u", _n);
            fude_utf8_copy(_q[_i].name, sizeof(_q[_i].name) - _sl, _base, rde_strlen(_base));
            rde_snprintf(_q[_i].name + rde_strlen(_q[_i].name), _sl + 1u, "%s", _suffix);
            if(!fzlg_port_named(_q, _i, _q[_i].name)) {
                break;
            }
        }
    }
}

// Adds the circuit's logic parts (of the _np parts _p, on _nn nodes) to definition _def as its instances "U<n>", each
// pin's net its node's: a port's name where a port is on it (_node_port, of the ports _q), a constant ("zero", "one")
// on a node of the ground's or a rail's (_const_of), else "n" and the node; the constants used made once each.
RDE_INTERNAL void fzlg_make_parts(fude_sim_def* _def, const fude_zoom_circuit_part* _p, u32 _np, u32 _nn, const fzlg_port_at* _q, const u32* _node_port,
                                 const u8* _const_of) {
    b8 _zero = false, _one = false;
    u32 _u = 0;
    fzlg_scratch _sc = fzlg_scratch_new();
    for(u32 _i = 0; _i < _np; _i++) {
        const u8 _m = _p[_i].part->model;
        c8 _what[FUDE_SIM_ID], _params[96];
        u32 _ports = 0;
        fzlg_scratch_fit(&_sc, _p[_i].part->pin_count, 0u);
        if(_m == FUDE_ZOOM_MODEL_LOGIC_IN || _m == FUDE_ZOOM_MODEL_LOGIC_OUT || !fude_zoom_logic_is(_p[_i].part) ||
           !fzlg_what(&_p[_i], _what, sizeof(_what), _params, sizeof(_params), (u32*)_sc.port_of_pin.memory, &_ports)) {
            continue;
        }
        rde_arr_resize(&_sc.nets, _ports > 0u ? _ports : 1u);
        const u32* _port_of_pin = (const u32*)_sc.port_of_pin.memory;
        fzlg_pin_name* _names = (fzlg_pin_name*)_sc.names.memory;
        const c8** _nets = (const c8**)_sc.nets.memory;
        for(u32 _k = 0; _k < _ports; _k++) {
            _nets[_k] = "zero";
        }
        for(u32 _k = 0; _k < _p[_i].part->pin_count; _k++) {
            const u32 _port = _port_of_pin[_k], _node = _p[_i].node[_k];
            if(_port >= _ports) {
                continue;   // (a pin of a version of its part with more ports than it has now)
            }
            u8 _domain, _dir;
            fzlg_port(&_p[_i], _k, &_domain, &_dir);
            if(_domain != FUDE_SIM_LOGIC || _node == FUDE_ZOOM_NONE || _node > _nn) {
                _nets[_port] = "-";
                continue;
            }
            if(_node_port[_node] != FUDE_ZOOM_NONE) {
                rde_snprintf(_names[_k], sizeof(_names[_k]), "%s", _q[_node_port[_node]].name);
            } else if(_const_of[_node] < 2u) {
                rde_snprintf(_names[_k], sizeof(_names[_k]), "%s", _const_of[_node] == 1u ? "one" : "zero");   // (tied low or high)
                _zero = _zero || _const_of[_node] == 0u;
                _one  = _one || _const_of[_node] == 1u;
            } else {
                rde_snprintf(_names[_k], sizeof(_names[_k]), "n%u", _node);
            }
            _nets[_port] = _names[_k];
        }
        for(u32 _k = 0; _k < _ports; _k++) {
            _zero = _zero || rde_strcmp(_nets[_k], "zero") == 0;
        }
        c8 _ref[16];
        rde_snprintf(_ref, sizeof(_ref), "U%u", ++_u);
        fude_sim_def_inst(_def, _what, _ref, _params, _nets, _ports);
    }
    fzlg_scratch_free(&_sc);
    if(_zero) {
        const c8* _zn[1] = { "zero" };
        fude_sim_def_inst(_def, "const", "Z", "0", _zn, 1u);
    }
    if(_one) {
        const c8* _on[1] = { "one" };
        fude_sim_def_inst(_def, "const", "ONE", "1", _on, 1u);
    }
}

// See the header. Ports are named from the first line of their texts (spaces, quotes, tabs and slashes made '_', cut
// at a whole character to fit); a name used twice gets its place after it. Each port's node is its net, named as the
// port, and a second port on the same node is joined to it by a buffer of no delay ("J<n>", what fude_zoom_logic_draw
// takes for wires). A pin on the ground's node or a supply rail's is tied to a constant.
fude_sim_def* fude_zoom_logic_make(const fude_zoom_circuit* _c, const fude_zoom_scene* _s, const c8* _id, const c8* _name, u32* _problem) {
    *_problem = FUDE_ZOOM_MAKE_OK;
    const fude_zoom_circuit_part* _p = (const fude_zoom_circuit_part*)_c->parts.memory;
    const u32 _np = (u32)rde_arr_length(&_c->parts), _nn = _c->nodes;
    const u32 _home = _s->home != FUDE_ZOOM_NONE ? _s->home : _s->root;
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_arr TYPE(fzlg_port_at) _ports_arr = rde_arr_new(sizeof(fzlg_port_at), _heap);
    const u32 _inside = fzlg_make_ports(_s, _p, _np, _home, &_ports_arr);
    const u32 _nports = (u32)rde_arr_length(&_ports_arr);
    if(_inside == 0u || _nports == 0u) {
        *_problem = _inside == 0u ? FUDE_ZOOM_MAKE_NOTHING : FUDE_ZOOM_MAKE_NO_PORTS;
        rde_arr_free(&_ports_arr);
        return NULL;
    }
    fzlg_port_at* _q = (fzlg_port_at*)_ports_arr.memory;
    rde_sort(_q, _nports, sizeof(fzlg_port_at), fzlg_port_order);
    // (the unnamed named in turn; a name twice made unique)
    fzlg_make_names(_q, _nports);
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
            rde_snprintf(_ref, sizeof(_ref), "J%u", ++_bufs);
            fude_sim_def_inst(_def, "buf", _ref, "delay=0", _bn, 2u);
        }
    }
    // A node of the ground's (0) or a supply rail's: a constant (0, 1) for what of it is on it; 2: none.
    rde_arr _const_arr = rde_arr_new(sizeof(u8), _heap);
    rde_arr_resize(&_const_arr, _nn + 1u);
    u8* _const_of = (u8*)_const_arr.memory;
    rde_memset(_const_of, 2, (usize)_nn + 1u);
    _const_of[0] = 0u;
    for(u32 _i = 0; _i < _np; _i++) {
        if(_p[_i].part->model == FUDE_ZOOM_MODEL_RAIL && _p[_i].node[0] != FUDE_ZOOM_NONE && _p[_i].node[0] <= _nn) {
            _const_of[_p[_i].node[0]] = _p[_i].value[0] >= FUDE_ZOOM_LOGIC_V * 0.5 ? 1u : 0u;
        }
    }
    // Its parts, their pins' nets as their nodes are.
    fzlg_make_parts(_def, _p, _np, _nn, _q, _node_port, _const_of);
    rde_arr_free(&_const_arr);
    rde_arr_free(&_node_port_arr);
    rde_arr_free(&_ports_arr);
    return _def;
}

// --- a custom part's inside, drawn ---------------------------------------------------------------------------------

// The most pins a drawn part maps to its ports one by one (fzlg_drawn's port_of_pin): a flip-flop's four; a gate's,
// a chip's and a custom part's pins are their ports in order (same), however many.
#define FZLG_DRAWN_MAP 4u

// A part of the definition as it is drawn: its symbol (a gate of more than two inputs: a chain of them, each one of
// these), its text, each of its pins' port of the definition's part (FUDE_SIM_NONE: a chain's own link): pin k's
// port is k when `same`, else port_of_pin[k] (fzlg_drawn_port).
typedef struct {
    c8  symbol[FUDE_SIM_ID];
    c8  text[FUDE_SIM_NAME];
    u32 inst;
    b8  same;
    u32 port_of_pin[FZLG_DRAWN_MAP];
    u32 pins;
    u32 out_pins;          // (its pins from this one on are outputs)
    u32 before;            // a chain's: the gate before it (its output into this one's first pin); FUDE_SIM_NONE
    u32 depth;
    f64 w, h;              // (0: the library's)
    f64 x, y;
    u32 object;
} fzlg_drawn;

// The definition's nets in groups (fude_zoom_logic_draw's): the nets a bridge joins (the buffers of no delay that Make
// part puts between a port and its net) are one net. The groups are an rde_disjoint_set, which names a group by its
// smallest net; but the drawing names a group by the net whose group the last bridge joined it INTO (a bridge's first
// net's group goes under its second's), as it always has. That name decides the order the wires are placed in
// (fzlg_draw_wires takes the groups in the order of their names, and each wire is a new object of the canvas: drawn,
// saved and made a part again in that order), so the set's name is mapped to the drawing's (name, by the set's name):
// the objects come out in the order they always did.
typedef struct {
    rde_disjoint_set  set;    // the nets, in groups
    rde_arr TYPE(u32) name;   // by a group's smallest net (the set's name for it): the drawing's name for the group
} fzlg_groups;

// Returns _count nets, each a group of its own named by itself, on _heap; freed with fzlg_groups_free.
RDE_INTERNAL fzlg_groups fzlg_groups_new(u32 _count, rde_memory_allocator* _heap) {
    fzlg_groups _g = { rde_disjoint_set_new(_count, _heap), rde_arr_new(sizeof(u32), _heap) };
    rde_arr_resize(&_g.name, _count);
    u32* _name = (u32*)_g.name.memory;
    for(u32 _n = 0; _n < _count; _n++) {
        _name[_n] = _n;
    }
    return _g;
}

// Returns the name of net _n's group (the drawing's: see fzlg_groups).
RDE_INTERNAL u32 fzlg_group(fzlg_groups* _g, u32 _n) {
    return ((const u32*)_g->name.memory)[rde_disjoint_set_find(&_g->set, _n)];
}

// Joins net _a's group into net _b's: one group, with _b's group's name (nothing changes when they are one already).
RDE_INTERNAL void fzlg_groups_join(fzlg_groups* _g, u32 _a, u32 _b) {
    const u32 _named = fzlg_group(_g, _b);
    ((u32*)_g->name.memory)[rde_disjoint_set_union(&_g->set, _a, _b)] = _named;
}

// Lets the groups go.
RDE_INTERNAL void fzlg_groups_free(fzlg_groups* _g) {
    rde_disjoint_set_free(&_g->set);
    rde_arr_free(&_g->name);
}

// Returns whether port _port of definition _of is an output (false for no definition or no such port).
RDE_INTERNAL b8 fzlg_port_out(const fude_sim_def* _of, u32 _port) {
    return _of != NULL && _port < (u32)rde_arr_length(&_of->ports) && ((const fude_sim_port*)_of->ports.memory)[_port].dir == FUDE_SIM_OUT;
}

// Returns drawn part _d's pin _k's port of its definition's part (FUDE_SIM_NONE: none, a chain's own link).
RDE_INTERNAL u32 fzlg_drawn_port(const fzlg_drawn* _d, u32 _k) {
    return _d->same ? _k : (_k < FZLG_DRAWN_MAP ? _d->port_of_pin[_k] : FUDE_SIM_NONE);
}

// What is drawn of each instance of _def (fude_zoom_logic_draw's), in a new array of fzlg_drawn on _heap (the caller
// frees it): a gate of more than two inputs a chain of two-input ones, a constant a rail or the ground, a gate or a
// flip-flop its symbol, a chip its own symbol, a custom part a block sized to its ports; a bridge (wires) and anything
// else left out.
RDE_INTERNAL rde_arr fzlg_draw_parts(const fude_sim_def* _def, rde_memory_allocator* _heap) {
    static const c8* const _gate_ids[8] = { "and", "or", "nand", "nor", "xor", "xnor", "not", "buf" };
    static const c8* const _gate_symbols[8] = { "AND gate", "OR gate", "NAND gate", "NOR gate", "XOR gate", "XNOR gate", "NOT gate", "buffer" };
    static const c8* const _chain_symbols[8] = { "AND gate", "OR gate", "AND gate", "OR gate", "XOR gate", "XOR gate", "", "" };   // (before the last)
    const fude_sim_inst* _in = (const fude_sim_inst*)_def->insts.memory;
    rde_arr _drawn_arr = rde_arr_new(sizeof(fzlg_drawn), _heap);
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_def->insts); _i++) {
        const fude_sim_inst* _q = &_in[_i];
        if(rde_strcmp(_q->def, "buf") == 0 && _q->ref[0] == 'J' && rde_strstr(_q->params, "delay=0") != NULL) {
            continue;   // (a bridge: wires)
        }
        fzlg_drawn _d;
        rde_memset(&_d, 0, sizeof(_d));
        _d.inst = _i;
        _d.before = FUDE_SIM_NONE;
        for(u32 _k = 0; _k < FZLG_DRAWN_MAP; _k++) {
            _d.port_of_pin[_k] = FUDE_SIM_NONE;
        }
        u32 _gate = 8u;
        for(u32 _g = 0; _g < 8u; _g++) {
            _gate = rde_strcmp(_q->def, _gate_ids[_g]) == 0 ? _g : _gate;
        }
        if(_gate < 6u && _q->count >= 3u) {
            // A gate of n inputs: n − 1 of two, chained (the last of its kind, those before it of its kind without
            // the NOT: AND for NAND, OR for NOR, XOR for XNOR).
            const u32 _inputs = _q->count - 1u;
            for(u32 _k = 0; _k + 1u < _inputs; _k++) {
                const b8 _last = _k + 2u == _inputs;
                rde_snprintf(_d.symbol, sizeof(_d.symbol), "%s", _last ? _gate_symbols[_gate] : _chain_symbols[_gate]);
                _d.pins = 3u;
                _d.out_pins = 2u;
                _d.port_of_pin[0] = _k == 0u ? 0u : FUDE_SIM_NONE;
                _d.port_of_pin[1] = _k + 1u;
                _d.port_of_pin[2] = _last ? _inputs : FUDE_SIM_NONE;
                _d.before = _k == 0u ? FUDE_SIM_NONE : (u32)rde_arr_length(&_drawn_arr) - 1u;
                rde_arr_add(&_drawn_arr, (any)&_d);
            }
            continue;
        }
        if(rde_strcmp(_q->def, "const") == 0) {
            // A constant: a 5 V rail (1) or the ground (0) — Make part's (only where something drawn is on it).
            const b8 _high = rde_strtod(_q->params, NULL) > 0.5;
            rde_snprintf(_d.symbol, sizeof(_d.symbol), "%s", _high ? "supply rail" : "ground");
            rde_snprintf(_d.text, sizeof(_d.text), "%s", _high ? "5V" : "");
            _d.pins = 1u;
            _d.out_pins = 0u;
            _d.port_of_pin[0] = 0u;
        } else if(_gate < 8u) {
            rde_snprintf(_d.symbol, sizeof(_d.symbol), "%s", _gate_symbols[_gate]);
            _d.pins = _q->count;
            _d.out_pins = _q->count - 1u;
            _d.same = true;   // (each pin its port)
        } else if(rde_strcmp(_q->def, "dff") == 0 || rde_strcmp(_q->def, "tff") == 0) {
            // (D or T, CLK, Q, QN: its R tied low, no pin)
            rde_snprintf(_d.symbol, sizeof(_d.symbol), "%s", rde_strcmp(_q->def, "dff") == 0 ? "D flip-flop" : "T flip-flop");
            _d.pins = 4u;
            _d.out_pins = 2u;
            _d.port_of_pin[0] = 0u;
            _d.port_of_pin[1] = 1u;
            _d.port_of_pin[2] = 3u;
            _d.port_of_pin[3] = 4u;
        } else {
            const fude_sim_def* _of = fude_zoom_logic_find(_q->def);
            const b8 _chip   = fude_zoom_part_find(_q->def) != NULL;   // (a chip of the library's: its own symbol)
            const b8 _custom = !_chip && _of != NULL && rde_strncmp(_of->id, "user/", 5u) == 0;
            if(!_chip && !_custom) {
                continue;   // (nothing on the canvas is it)
            }
            rde_snprintf(_d.symbol, sizeof(_d.symbol), "%s", _custom ? "custom part" : _q->def);
            rde_snprintf(_d.text, sizeof(_d.text), "%s", _custom ? _of->name : _q->def);
            _d.pins = _q->count;   // (every one, however many)
            _d.out_pins = _d.pins;
            _d.same = true;
            if(_custom) {
                u32 _ins = 0, _outs = 0;
                for(u32 _k = 0; _k < (u32)rde_arr_length(&_of->ports); _k++) {
                    if(fzlg_port_out(_of, _k)) { _outs++; } else { _ins++; }
                }
                _d.w = 120.0;
                _d.h = rde_math_max_f64(60.0, ((f64)(_ins > _outs ? _ins : _outs) + 1.0) * 24.0);
            }
        }
        rde_arr_add(&_drawn_arr, (any)&_d);
    }
    return _drawn_arr;
}

// Is pin _k of drawn part _j an output?
// (The depths' and the wires', below: of their _d and _in; undefined after them.)
#define FZLG_PIN_OUT(_j, _k) (_d[_j].out_pins < _d[_j].pins ? (_k) >= _d[_j].out_pins : fzlg_port_out(fude_zoom_logic_find(_in[_d[_j].inst].def), fzlg_drawn_port(&_d[_j], _k)))

// Works out each of the _nd parts _d's depth from the inputs of _def, and each net's group's (_nd_depth, by its name in
// _up): an input's 0, a part's one more than the deepest that comes into it, in passes until none changes (64 at most).
RDE_INTERNAL void fzlg_draw_depths(const fude_sim_def* _def, fzlg_drawn* _d, u32 _nd, fzlg_groups* _up, u32* _nd_depth) {
    const u32 _nn = (u32)rde_arr_length(&_def->nets);
    const fude_sim_inst* _in = (const fude_sim_inst*)_def->insts.memory;
    const u32* _inets = (const u32*)_def->inst_nets.memory;
    for(u32 _round = 0; _round < _nd + 2u && _round < 64u; _round++) {
        b8 _changed = false;
        for(u32 _j = 0; _j < _nd; _j++) {
            u32 _deep = _d[_j].before != FUDE_SIM_NONE ? _d[_d[_j].before].depth : 0u;
            for(u32 _k = 0; _k < _d[_j].pins; _k++) {
                const u32 _port = fzlg_drawn_port(&_d[_j], _k);
                if(_port == FUDE_SIM_NONE || _port >= _in[_d[_j].inst].count || FZLG_PIN_OUT(_j, _k)) {
                    continue;
                }
                const u32 _net = _inets[_in[_d[_j].inst].first + _port];
                if(_net < _nn) {
                    _deep = _nd_depth[fzlg_group(_up, _net)] > _deep ? _nd_depth[fzlg_group(_up, _net)] : _deep;
                }
            }
            const u32 _now = _deep + 1u < _nd + 1u ? _deep + 1u : _nd + 1u;
            if(_now != _d[_j].depth) {
                _d[_j].depth = _now;
                _changed = true;
            }
            for(u32 _k = 0; _k < _d[_j].pins; _k++) {
                const u32 _port = fzlg_drawn_port(&_d[_j], _k);
                if(_port == FUDE_SIM_NONE || _port >= _in[_d[_j].inst].count || !FZLG_PIN_OUT(_j, _k)) {
                    continue;
                }
                const u32 _net = _inets[_in[_d[_j].inst].first + _port];
                if(_net < _nn && _nd_depth[fzlg_group(_up, _net)] < _now) {
                    _nd_depth[fzlg_group(_up, _net)] = _now;
                }
            }
        }
        if(!_changed) {
            break;
        }
    }
}

// Places the wires of _def's drawing with _p: each net's group (in the order of their names in _up) from what drives it
// (an input of it, an output of one of the _nd parts _d) to the rest on it, its ports being the objects _port_obj.
// Returns the array of its ends it worked in, on _heap, for the caller to free.
RDE_INTERNAL rde_arr fzlg_draw_wires(const fude_sim_def* _def, struct fude_zoom_placer* _p, const fzlg_drawn* _d, u32 _nd, fzlg_groups* _up, const u32* _port_obj,
                                    rde_memory_allocator* _heap) {
    const u32 _nn = (u32)rde_arr_length(&_def->nets);
    const fude_sim_inst* _in = (const fude_sim_inst*)_def->insts.memory;
    const u32* _inets = (const u32*)_def->inst_nets.memory;
    const u32* _pnets = (const u32*)_def->port_nets.memory;
    const fude_sim_port* _ports = (const fude_sim_port*)_def->ports.memory;
    const u32 _np = (u32)rde_arr_length(&_def->ports);
    typedef struct { u32 object, pin; b8 drives; } fzlg_end;
    rde_arr _ends = rde_arr_new(sizeof(fzlg_end), _heap);
    for(u32 _r = 0; _r < _nn; _r++) {
        if(fzlg_group(_up, _r) != _r) {
            continue;
        }
        rde_arr_clear(&_ends);
        for(u32 _k = 0; _k < _np; _k++) {
            if(_pnets[_k] < _nn && fzlg_group(_up, _pnets[_k]) == _r && _port_obj[_k] != FUDE_ZOOM_NONE) {
                const fzlg_end _e = { _port_obj[_k], 0u, _ports[_k].dir != FUDE_SIM_OUT };
                rde_arr_add(&_ends, (any)&_e);
            }
        }
        for(u32 _j = 0; _j < _nd; _j++) {
            for(u32 _k = 0; _k < _d[_j].pins; _k++) {
                const u32 _port = fzlg_drawn_port(&_d[_j], _k);
                if(_port == FUDE_SIM_NONE || _port >= _in[_d[_j].inst].count || _d[_j].object == FUDE_ZOOM_NONE) {
                    continue;
                }
                const u32 _net = _inets[_in[_d[_j].inst].first + _port];
                if(_net < _nn && fzlg_group(_up, _net) == _r) {
                    const fzlg_end _e = { _d[_j].object, _k, FZLG_PIN_OUT(_j, _k) };
                    rde_arr_add(&_ends, (any)&_e);
                }
            }
        }
        const fzlg_end* _e = (const fzlg_end*)_ends.memory;
        const u32 _ne = (u32)rde_arr_length(&_ends);
        u32 _from = 0;
        for(u32 _k = 0; _k < _ne; _k++) {
            if(_e[_k].drives) {
                _from = _k;
                break;
            }
        }
        for(u32 _k = 0; _k < _ne; _k++) {
            if(_k != _from) {
                fude_zoom_placer_wire(_p, _e[_from].object, _e[_from].pin, _e[_k].object, _e[_k].pin, false);
            }
        }
    }
    return _ends;
}

#undef FZLG_PIN_OUT

// See the header. Steps: the nets joined by bridges grouped; what is drawn of each instance (a gate of more than two
// inputs a chain of two-input ones, a chip its own symbol, a custom part a block sized to its ports; anything else
// is left out); constants nothing drawn is on dropped; each part's depth from the inputs, worked out by passes (at
// most 64); the parts placed in a column a depth, 260 apart, the inputs left and the outputs right; then the wires,
// each net's group from what drives it to the rest on it, and the chains' links; and the name over it all.
u32 fude_zoom_logic_draw(const fude_sim_def* _def, struct fude_zoom_placer* _p) {
    if(_def == NULL || _p == NULL || _def->kind == FUDE_SIM_PRIMITIVE) {
        return 0u;
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    const u32 _was = (u32)rde_arr_length(_p->born);
    const u32 _nn = (u32)rde_arr_length(&_def->nets);
    const fude_sim_inst* _in = (const fude_sim_inst*)_def->insts.memory;
    const u32* _inets = (const u32*)_def->inst_nets.memory;
    const fude_sim_port* _ports = (const fude_sim_port*)_def->ports.memory;
    const u32 _np = (u32)rde_arr_length(&_def->ports);
    // Its nets in groups: a bridge (a buffer of no delay, J…) one net with what it joins.
    fzlg_groups _groups = fzlg_groups_new(_nn + 1u, _heap);
    fzlg_groups* _up = &_groups;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_def->insts); _i++) {
        if(rde_strcmp(_in[_i].def, "buf") == 0 && _in[_i].ref[0] == 'J' && rde_strstr(_in[_i].params, "delay=0") != NULL && _in[_i].count == 2u) {
            const u32 _a = _inets[_in[_i].first], _b = _inets[_in[_i].first + 1u];
            if(_a < _nn && _b < _nn) {
                fzlg_groups_join(_up, _a, _b);
            }
        }
    }
    // What is drawn of each of its parts.
    rde_arr _drawn_arr = fzlg_draw_parts(_def, _heap);
    fzlg_drawn* _d = (fzlg_drawn*)_drawn_arr.memory;
    u32 _nd = (u32)rde_arr_length(&_drawn_arr);
    // (a constant nothing drawn is on — what a pin-less port is tied to: left out)
    for(u32 _j = 0; _j < _nd; _j++) {
        if(rde_strcmp(_in[_d[_j].inst].def, "const") != 0) {
            continue;
        }
        const u32 _net = _inets[_in[_d[_j].inst].first];
        b8 _used = false;
        for(u32 _o = 0; _o < _nd && !_used && _net < _nn; _o++) {
            for(u32 _k = 0; _o != _j && _k < _d[_o].pins && !_used; _k++) {
                const u32 _port = fzlg_drawn_port(&_d[_o], _k);
                _used = _port != FUDE_SIM_NONE && _port < _in[_d[_o].inst].count && _inets[_in[_d[_o].inst].first + _port] < _nn &&
                        fzlg_group(_up, _inets[_in[_d[_o].inst].first + _port]) == fzlg_group(_up, _net);
            }
        }
        if(!_used) {
            // (its place taken by the last; the chains' links to the last moved with it)
            for(u32 _o = 0; _o < _nd; _o++) {
                if(_d[_o].before == _nd - 1u) {
                    _d[_o].before = _j;
                }
            }
            _d[_j] = _d[_nd - 1u];
            _nd--;
            _j--;
        }
    }
    // Each net's group's depth from its inputs (theirs 0; a part's one more than what comes into it).
    rde_arr _depth_arr = rde_arr_new(sizeof(u32), _heap);
    rde_arr_resize(&_depth_arr, _nn + 1u);
    u32* _nd_depth = (u32*)_depth_arr.memory;
    fzlg_draw_depths(_def, _d, _nd, _up, _nd_depth);
    // Where each goes: a column a depth, each column down from its top; the inputs left of them, the outputs right.
    u32 _deepest = 0;
    for(u32 _j = 0; _j < _nd; _j++) {
        _deepest = _d[_j].depth > _deepest ? _d[_j].depth : _deepest;
    }
    const f64 _col = 260.0, _left = -_col * ((f64)_deepest + 1.0) * 0.5;
    for(u32 _c = 1; _c <= _deepest; _c++) {
        f64 _total = 0.0;
        for(u32 _j = 0; _j < _nd; _j++) {
            if(_d[_j].depth == _c) {
                const fude_zoom_symbol_info* _info = fude_zoom_symbol_info_of(fude_zoom_symbol_find(_d[_j].symbol));
                _d[_j].h = _d[_j].h > 0.0 ? _d[_j].h : (_info != NULL ? (f64)_info->h : 60.0);
                _total += _d[_j].h + 40.0;
            }
        }
        f64 _y = _total * 0.5;
        for(u32 _j = 0; _j < _nd; _j++) {
            if(_d[_j].depth == _c) {
                _d[_j].x = _left + _col * (f64)_c;
                _d[_j].y = _y - _d[_j].h * 0.5 - 20.0;
                _y -= _d[_j].h + 40.0;
            }
        }
    }
    u32 _ins = 0, _outs = 0;
    for(u32 _k = 0; _k < _np; _k++) {
        if(_ports[_k].dir == FUDE_SIM_OUT) { _outs++; } else { _ins++; }
    }
    rde_arr _port_obj_arr = rde_arr_new(sizeof(u32), _heap);
    rde_arr_resize(&_port_obj_arr, _np + 1u);
    u32* _port_obj = (u32*)_port_obj_arr.memory;
    u32 _ii = 0, _oi = 0;
    for(u32 _k = 0; _k < _np; _k++) {
        const b8 _out = _ports[_k].dir == FUDE_SIM_OUT;
        const f64 _y = ((f64)(_out ? _outs : _ins) - 1.0) * 40.0 - (f64)(_out ? _oi++ : _ii++) * 80.0;
        _port_obj[_k] = fude_zoom_placer_part(_p, _out ? "logic probe" : "logic input", _out ? _left + _col * ((f64)_deepest + 1.0) : _left, _y, 0.0, 0.0, 0.0, _ports[_k].name);
    }
    for(u32 _j = 0; _j < _nd; _j++) {
        _d[_j].object = fude_zoom_placer_part(_p, _d[_j].symbol, _d[_j].x, _d[_j].y, 0.0, _d[_j].w, _d[_j].w > 0.0 ? _d[_j].h : 0.0, _d[_j].text);
    }
    // Its wires: each net's group from what drives it (an input of it, an output of a part) to the rest on it.
    rde_arr _ends = fzlg_draw_wires(_def, _p, _d, _nd, _up, _port_obj, _heap);
    // A chain's links.
    for(u32 _j = 0; _j < _nd; _j++) {
        if(_d[_j].before != FUDE_SIM_NONE) {
            fude_zoom_placer_wire(_p, _d[_d[_j].before].object, 2u, _d[_j].object, 0u, false);
        }
    }
    fude_zoom_placer_text(_p, _left - 40.0, ((f64)(_ins > _outs ? _ins : _outs)) * 40.0 + 110.0, 26.0, 520.0, _def->name);
    rde_arr_free(&_ends);
    rde_arr_free(&_port_obj_arr);
    rde_arr_free(&_depth_arr);
    rde_arr_free(&_drawn_arr);
    fzlg_groups_free(&_groups);
    return (u32)rde_arr_length(_p->born) - _was;
}
