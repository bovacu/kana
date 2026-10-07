// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "sim/sim.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef c8 fsd_name[FUDE_SIM_NAME];

RDE_INTERNAL const c8* const FSD_DOMAINS[FUDE_SIM_DOMAINS] = { "electric", "logic", "rotation", "translation", "signal" };
RDE_INTERNAL const c8* const FSD_DIRS[4]  = { "in", "out", "inout", "passive" };
RDE_INTERNAL const c8* const FSD_SIDES[4] = { "left", "right", "top", "bottom" };

// _src into _dst (_size), its spaces and quotes as underscores (names are single words in a part's text).
RDE_INTERNAL void fsd_name_copy(c8* _dst, usize _size, const c8* _src) {
    usize _i = 0;
    for(; _src != NULL && _src[_i] != 0 && _i + 1u < _size; _i++) {
        const c8 _c = _src[_i];
        _dst[_i] = (_c == ' ' || _c == '\t' || _c == '"' || _c == '\n' || _c == '\r') ? '_' : _c;
    }
    _dst[_i] = 0;
}

RDE_INTERNAL fude_sim_def* fsd_alloc(void) {
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    fude_sim_def* _def = (fude_sim_def*)_heap->calloc(_heap->allocator, 1, sizeof(fude_sim_def));
    _def->version   = 1u;
    _def->ports     = rde_arr_new(sizeof(fude_sim_port), _heap);
    _def->params    = rde_arr_new(sizeof(fude_sim_param), _heap);
    _def->nets      = rde_arr_new(sizeof(fsd_name), _heap);
    _def->insts     = rde_arr_new(sizeof(fude_sim_inst), _heap);
    _def->inst_nets = rde_arr_new(sizeof(u32), _heap);
    _def->port_nets = rde_arr_new(sizeof(u32), _heap);
    return _def;
}

fude_sim_def* fude_sim_def_new(const c8* _id, const c8* _name) {
    fude_sim_def* _def = fsd_alloc();
    fsd_name_copy(_def->id, sizeof(_def->id), _id);
    snprintf(_def->name, sizeof(_def->name), "%s", _name != NULL ? _name : "");
    _def->kind = FUDE_SIM_COMPOSITE;
    return _def;
}

void fude_sim_def_free(fude_sim_def* _def) {
    if(_def == NULL) {
        return;
    }
    rde_arr_free(&_def->ports);
    rde_arr_free(&_def->params);
    rde_arr_free(&_def->nets);
    rde_arr_free(&_def->insts);
    rde_arr_free(&_def->inst_nets);
    rde_arr_free(&_def->port_nets);
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    _heap->free(_heap->allocator, _def);
}

u32 fude_sim_def_net(fude_sim_def* _def, const c8* _name) {
    fsd_name _n;
    fsd_name_copy(_n, sizeof(_n), _name);
    const fsd_name* _nets = (const fsd_name*)_def->nets.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_def->nets); _i++) {
        if(strcmp(_nets[_i], _n) == 0) {
            return _i;
        }
    }
    rde_arr_add(&_def->nets, (any)_n);
    return (u32)rde_arr_length(&_def->nets) - 1u;
}

u32 fude_sim_def_port(fude_sim_def* _def, const c8* _name, u8 _domain, u8 _dir, u8 _width) {
    fude_sim_port _p;
    memset(&_p, 0, sizeof(_p));
    fsd_name_copy(_p.name, sizeof(_p.name), _name);
    _p.domain = _domain < FUDE_SIM_DOMAINS ? _domain : FUDE_SIM_ELECTRIC;
    _p.dir    = _dir <= FUDE_SIM_PASSIVE ? _dir : FUDE_SIM_PASSIVE;
    _p.width  = _width > 0u ? _width : 1u;
    _p.side   = _dir == FUDE_SIM_OUT ? FUDE_SIM_RIGHT : FUDE_SIM_LEFT;   // (inputs on the left, outputs on the right)
    rde_arr_add(&_def->ports, (any)&_p);
    const u32 _net = fude_sim_def_net(_def, _p.name);
    rde_arr_add(&_def->port_nets, (any)&_net);
    return (u32)rde_arr_length(&_def->ports) - 1u;
}

u32 fude_sim_def_inst(fude_sim_def* _def, const c8* _what, const c8* _ref, const c8* _params, const c8* const* _nets, u32 _count) {
    fude_sim_inst _in;
    memset(&_in, 0, sizeof(_in));
    fsd_name_copy(_in.def, sizeof(_in.def), _what);
    fsd_name_copy(_in.ref, sizeof(_in.ref), _ref);
    snprintf(_in.params, sizeof(_in.params), "%s", _params != NULL ? _params : "");
    for(c8* _c = _in.params; *_c != 0; _c++) {
        *_c = *_c == '"' || *_c == '\n' ? ' ' : *_c;   // (kept between quotes in its text)
    }
    _in.first = (u32)rde_arr_length(&_def->inst_nets);
    _in.count = _count;
    for(u32 _i = 0; _i < _count; _i++) {
        const u32 _net = _nets != NULL && _nets[_i] != NULL && _nets[_i][0] != 0 ? fude_sim_def_net(_def, _nets[_i]) : FUDE_SIM_NONE;
        rde_arr_add(&_def->inst_nets, (any)&_net);
    }
    rde_arr_add(&_def->insts, (any)&_in);
    return (u32)rde_arr_length(&_def->insts) - 1u;
}

// --- the library ---------------------------------------------------------------------------------

void fude_sim_lib_init(fude_sim_lib* _lib) {
    _lib->defs = rde_arr_new(sizeof(fude_sim_def*), rde_memory_allocator_get_default_std());
    for(u32 _m = 0; _m < fude_sim_model_count(); _m++) {
        const fude_sim_model* _model = fude_sim_model_at(_m);
        fude_sim_def* _def = fsd_alloc();
        fsd_name_copy(_def->id, sizeof(_def->id), _model->id);
        snprintf(_def->name, sizeof(_def->name), "%s", _model->name != NULL ? _model->name : _model->id);
        _def->kind  = FUDE_SIM_PRIMITIVE;
        _def->model = _model;
        f64 _defaults[FUDE_SIM_PARAMS] = { 0 };
        for(u32 _k = 0; _k < _model->param_count && _k < FUDE_SIM_PARAMS; _k++) {
            rde_arr_add(&_def->params, (any)&_model->params[_k]);
            _defaults[_k] = _model->params[_k].value;
        }
        fude_sim_port _ports[64];
        const u32 _np = _model->ports != NULL ? _model->ports(_defaults, _ports, 64u) : 0u;
        for(u32 _k = 0; _k < _np; _k++) {
            rde_arr_add(&_def->ports, (any)&_ports[_k]);
        }
        rde_arr_add(&_lib->defs, (any)&_def);
    }
}

void fude_sim_lib_destroy(fude_sim_lib* _lib) {
    fude_sim_def** _d = (fude_sim_def**)_lib->defs.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_lib->defs); _i++) {
        fude_sim_def_free(_d[_i]);
    }
    rde_arr_free(&_lib->defs);
}

const fude_sim_def* fude_sim_lib_find(const fude_sim_lib* _lib, const c8* _id) {
    fude_sim_def* const* _d = (fude_sim_def* const*)_lib->defs.memory;
    for(u32 _i = 0; _id != NULL && _i < (u32)rde_arr_length(&_lib->defs); _i++) {
        if(strcmp(_d[_i]->id, _id) == 0) {
            return _d[_i];
        }
    }
    return NULL;
}

u32 fude_sim_lib_add(fude_sim_lib* _lib, fude_sim_def* _def) {
    fude_sim_def** _d = (fude_sim_def**)_lib->defs.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_lib->defs); _i++) {
        if(strcmp(_d[_i]->id, _def->id) == 0) {
            _def->version = _def->version > _d[_i]->version ? _def->version : _d[_i]->version + 1u;
            fude_sim_def_free(_d[_i]);
            _d[_i] = _def;
            return _i;
        }
    }
    rde_arr_add(&_lib->defs, (any)&_def);
    return (u32)rde_arr_length(&_lib->defs) - 1u;
}

// --- numbers --------------------------------------------------------------------------------------

b8 fude_sim_number(const c8* _text, f64* _out) {
    if(_text == NULL) {
        return false;
    }
    c8* _end = NULL;
    const f64 _v = strtod(_text, &_end);
    if(_end == _text) {
        return false;
    }
    while(*_end == ' ') {
        _end++;
    }
    f64 _k = 1.0;
    switch(*_end) {
    case 'p': _k = 1e-12; break;
    case 'n': _k = 1e-9;  break;
    case 'u': _k = 1e-6;  break;
    case 'm': _k = 1e-3;  break;
    case 'k': case 'K': _k = 1e3; break;
    case 'M': _k = 1e6;   break;
    case 'G': _k = 1e9;   break;
    default:
        if((u8)_end[0] == 0xC2u && (u8)_end[1] == 0xB5u) {
            _k = 1e-6;   // (µ)
        }
        break;
    }
    *_out = _v * _k;
    return isfinite(*_out);
}

// --- as text --------------------------------------------------------------------------------------

RDE_INTERNAL void fsd_put(rde_arr* _out, const c8* _s) {
    const usize _n = strlen(_s);
    if(_n > 0u) {
        memcpy(rde_arr_add_n(_out, _n), _s, _n);
    }
}

void fude_sim_def_write(const fude_sim_def* _def, rde_arr* _out) {
    c8 _line[512];
    fsd_put(_out, "fude-part 1\n");
    snprintf(_line, sizeof(_line), "id %s\nname %s\nversion %u\n", _def->id, _def->name, _def->version);
    fsd_put(_out, _line);
    if(_def->desc[0] != 0) {
        snprintf(_line, sizeof(_line), "desc %s\n", _def->desc);
        fsd_put(_out, _line);
    }
    const fude_sim_port* _p = (const fude_sim_port*)_def->ports.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_def->ports); _i++) {
        snprintf(_line, sizeof(_line), "port %s %s %s %u %s\n", _p[_i].name, FSD_DOMAINS[_p[_i].domain % FUDE_SIM_DOMAINS], FSD_DIRS[_p[_i].dir & 3u],
                 (u32)_p[_i].width, FSD_SIDES[_p[_i].side & 3u]);
        fsd_put(_out, _line);
    }
    const fude_sim_param* _q = (const fude_sim_param*)_def->params.memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(&_def->params); _i++) {
        snprintf(_line, sizeof(_line), "param %s %.17g %s %.17g %.17g\n", _q[_i].name, _q[_i].value, _q[_i].unit[0] != 0 ? _q[_i].unit : "-", _q[_i].lo, _q[_i].hi);
        fsd_put(_out, _line);
    }
    if(_def->kind == FUDE_SIM_COMPOSITE) {
        const fsd_name* _n = (const fsd_name*)_def->nets.memory;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_def->nets); _i++) {
            snprintf(_line, sizeof(_line), "net %s\n", _n[_i]);
            fsd_put(_out, _line);
        }
        const fude_sim_inst* _in = (const fude_sim_inst*)_def->insts.memory;
        const u32* _nets = (const u32*)_def->inst_nets.memory;
        for(u32 _i = 0; _i < (u32)rde_arr_length(&_def->insts); _i++) {
            snprintf(_line, sizeof(_line), "inst %s %s \"%s\"", _in[_i].def, _in[_i].ref[0] != 0 ? _in[_i].ref : "-", _in[_i].params);
            fsd_put(_out, _line);
            for(u32 _k = 0; _k < _in[_i].count; _k++) {
                const u32 _net = _nets[_in[_i].first + _k];
                fsd_put(_out, " ");
                fsd_put(_out, _net != FUDE_SIM_NONE ? _n[_net] : "-");
            }
            fsd_put(_out, "\n");
        }
    } else {
        snprintf(_line, sizeof(_line), "model %s\n", _def->model != NULL ? _def->model->id : _def->id);
        fsd_put(_out, _line);
    }
    fsd_put(_out, "end\n");
}

// A line's words (a quoted one whole, its quotes off) into _words (_max at most), in _line (written into). How many.
RDE_INTERNAL u32 fsd_words(c8* _line, c8** _words, u32 _max) {
    u32 _n = 0;
    c8* _c = _line;
    while(*_c != 0 && _n < _max) {
        while(*_c == ' ' || *_c == '\t') {
            _c++;
        }
        if(*_c == 0) {
            break;
        }
        if(*_c == '"') {
            _c++;
            _words[_n++] = _c;
            while(*_c != 0 && *_c != '"') {
                _c++;
            }
        } else {
            _words[_n++] = _c;
            while(*_c != 0 && *_c != ' ' && *_c != '\t') {
                _c++;
            }
        }
        if(*_c != 0) {
            *_c++ = 0;
        }
    }
    return _n;
}

RDE_INTERNAL i32 fsd_index(const c8* const* _names, u32 _n, const c8* _word) {
    for(u32 _i = 0; _i < _n; _i++) {
        if(strcmp(_names[_i], _word) == 0) {
            return (i32)_i;
        }
    }
    return -1;
}

fude_sim_def* fude_sim_def_read(const c8* _text, usize _size, c8* _error, usize _error_size) {
    if(_error_size > 0u) {
        _error[0] = 0;
    }
    fude_sim_def* _def = fsd_alloc();
    _def->kind = FUDE_SIM_COMPOSITE;
    b8 _header = false, _ended = false;
    u32 _line_no = 0;
    usize _at = 0;
    #define FSD_FAIL(...) do { snprintf(_error, _error_size, __VA_ARGS__); fude_sim_def_free(_def); return NULL; } while(0)
    while(_at < _size && !_ended) {
        usize _end = _at;
        while(_end < _size && _text[_end] != '\n') {
            _end++;
        }
        c8 _line[1024];
        usize _len = _end - _at < sizeof(_line) - 1u ? _end - _at : sizeof(_line) - 1u;
        memcpy(_line, &_text[_at], _len);
        _line[_len] = 0;
        while(_len > 0u && (_line[_len - 1u] == '\r' || _line[_len - 1u] == ' ')) {
            _line[--_len] = 0;
        }
        _at = _end + 1u;
        _line_no++;
        if(_line[0] == 0 || _line[0] == '#') {
            continue;
        }
        // (name and desc: the rest of the line, as it is)
        // (an empty one too: "name" alone, its space trimmed)
        if(strncmp(_line, "name", 4) == 0 && (_line[4] == ' ' || _line[4] == 0)) {
            snprintf(_def->name, sizeof(_def->name), "%s", _line[4] != 0 ? _line + 5 : "");
            continue;
        }
        if(strncmp(_line, "desc", 4) == 0 && (_line[4] == ' ' || _line[4] == 0)) {
            snprintf(_def->desc, sizeof(_def->desc), "%s", _line[4] != 0 ? _line + 5 : "");
            continue;
        }
        c8* _w[72];
        const u32 _n = fsd_words(_line, _w, 72u);
        if(_n == 0u) {
            continue;
        }
        if(strcmp(_w[0], "fude-part") == 0) {
            if(_n < 2u || atoi(_w[1]) != 1) {
                FSD_FAIL("line %u: a part's text of another kind (fude-part %s)", _line_no, _n > 1u ? _w[1] : "?");
            }
            _header = true;
        } else if(!_header) {
            FSD_FAIL("line %u: not a part's text (it starts with fude-part 1)", _line_no);
        } else if(strcmp(_w[0], "id") == 0 && _n >= 2u) {
            fsd_name_copy(_def->id, sizeof(_def->id), _w[1]);
        } else if(strcmp(_w[0], "version") == 0 && _n >= 2u) {
            _def->version = (u32)strtoul(_w[1], NULL, 10);
        } else if(strcmp(_w[0], "port") == 0 && _n >= 4u) {
            const i32 _d = fsd_index(FSD_DOMAINS, FUDE_SIM_DOMAINS, _w[2]), _r = fsd_index(FSD_DIRS, 4u, _w[3]);
            if(_d < 0 || _r < 0) {
                FSD_FAIL("line %u: a port's domain or way (%s %s)", _line_no, _w[2], _w[3]);
            }
            const u32 _p = fude_sim_def_port(_def, _w[1], (u8)_d, (u8)_r, _n >= 5u ? (u8)atoi(_w[4]) : 1u);
            if(_n >= 6u) {
                const i32 _s = fsd_index(FSD_SIDES, 4u, _w[5]);
                ((fude_sim_port*)_def->ports.memory)[_p].side = _s >= 0 ? (u8)_s : 0u;
            }
        } else if(strcmp(_w[0], "param") == 0 && _n >= 3u) {
            fude_sim_param _q;
            memset(&_q, 0, sizeof(_q));
            fsd_name_copy(_q.name, sizeof(_q.name), _w[1]);
            _q.value = strtod(_w[2], NULL);
            if(_n >= 4u && strcmp(_w[3], "-") != 0) {
                fsd_name_copy(_q.unit, sizeof(_q.unit), _w[3]);
            }
            _q.lo = _n >= 5u ? strtod(_w[4], NULL) : 0.0;
            _q.hi = _n >= 6u ? strtod(_w[5], NULL) : 0.0;
            rde_arr_add(&_def->params, (any)&_q);
        } else if(strcmp(_w[0], "net") == 0 && _n >= 2u) {
            fude_sim_def_net(_def, _w[1]);
        } else if(strcmp(_w[0], "inst") == 0 && _n >= 4u) {
            const c8* _nets[64];
            const u32 _k = _n - 4u < 64u ? _n - 4u : 64u;
            for(u32 _i = 0; _i < _k; _i++) {
                _nets[_i] = strcmp(_w[4u + _i], "-") == 0 ? NULL : _w[4u + _i];
            }
            fude_sim_def_inst(_def, _w[1], strcmp(_w[2], "-") == 0 ? "" : _w[2], _w[3], _nets, _k);
        } else if(strcmp(_w[0], "model") == 0 && _n >= 2u) {
            _def->kind  = FUDE_SIM_PRIMITIVE;
            _def->model = fude_sim_model_find(_w[1]);
            if(_def->model == NULL) {
                FSD_FAIL("line %u: no model %s", _line_no, _w[1]);
            }
        } else if(strcmp(_w[0], "end") == 0) {
            _ended = true;
        } else {
            FSD_FAIL("line %u: not understood (%s)", _line_no, _w[0]);
        }
    }
    if(!_header || _def->id[0] == 0) {
        FSD_FAIL("no part in it (fude-part 1, then its id)");
    }
    #undef FSD_FAIL
    return _def;
}
