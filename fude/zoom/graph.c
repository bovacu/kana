// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/graph.h"
#include "drawing/base/utf8.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- the graph -------------------------------------------------------------------------------

// Tables: each an rde_arr of _count items, all zero, sized once (so its memory
// stays put), kept in a list of them (_tables: rde_arr of rde_arr) and freed with it.
RDE_INTERNAL rde_arr fude_zoom_graph_tables_new(void) {
    return rde_arr_new(sizeof(rde_arr), rde_memory_allocator_get_default_std());
}

// Its memory (never NULL for nothing).
RDE_INTERNAL any fude_zoom_graph_table(rde_arr* _tables, usize _count, usize _size) {
    rde_arr _t = rde_arr_new(_size, rde_memory_allocator_get_default_std());
    rde_arr_resize(&_t, _count);
    rde_arr_add(_tables, (any)&_t);
    return _t.memory;
}

// Each table freed; the list kept, empty.
RDE_INTERNAL void fude_zoom_graph_tables_clear(rde_arr* _tables) {
    rde_arr* _t = (rde_arr*)_tables->memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(_tables); _i++) {
        rde_arr_free(&_t[_i]);
    }
    rde_arr_clear(_tables);
}

RDE_INTERNAL void fude_zoom_graph_tables_free(rde_arr* _tables) {
    if(rde_arr_is_inited(_tables)) {
        fude_zoom_graph_tables_clear(_tables);
        rde_arr_free(_tables);
    }
}

RDE_INTERNAL u32 fude_zoom_graph_count(const rde_arr* _a) {
    return (u32)rde_arr_length(_a);
}

void fude_zoom_graph_init(fude_zoom_graph* _g) {
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    memset(_g, 0, sizeof(*_g));
    _g->nodes     = rde_arr_new(sizeof(fude_zoom_graph_node), _heap);
    _g->edges     = rde_arr_new(sizeof(fude_zoom_graph_edge), _heap);
    _g->subgraphs = rde_arr_new(sizeof(fude_zoom_graph_subgraph), _heap);
    _g->bends     = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    _g->direction = FUDE_ZOOM_FLOW_TB;
}

void fude_zoom_graph_destroy(fude_zoom_graph* _g) {
    rde_arr* _all[4] = { &_g->nodes, &_g->edges, &_g->subgraphs, &_g->bends };
    for(u32 _i = 0; _i < 4u; _i++) {
        if(rde_arr_is_inited(_all[_i])) {
            rde_arr_free(_all[_i]);
        }
    }
}

void fude_zoom_graph_clear(fude_zoom_graph* _g) {
    rde_arr_clear(&_g->nodes);
    rde_arr_clear(&_g->edges);
    rde_arr_clear(&_g->subgraphs);
    rde_arr_clear(&_g->bends);
    _g->direction = FUDE_ZOOM_FLOW_TB;
}

fude_zoom_graph_node* fude_zoom_graph_node_at(const fude_zoom_graph* _g, u32 _i) {
    return &((fude_zoom_graph_node*)_g->nodes.memory)[_i];
}

fude_zoom_graph_edge* fude_zoom_graph_edge_at(const fude_zoom_graph* _g, u32 _i) {
    return &((fude_zoom_graph_edge*)_g->edges.memory)[_i];
}

fude_zoom_graph_subgraph* fude_zoom_graph_subgraph_at(const fude_zoom_graph* _g, u32 _i) {
    return &((fude_zoom_graph_subgraph*)_g->subgraphs.memory)[_i];
}

// --- text ------------------------------------------------------------------------------------

RDE_INTERNAL b8 fude_zoom_graph_space(c8 _c) {
    return _c == ' ' || _c == '\t' || _c == '\r';
}

// The length of the UTF-8 sequence _c leads (1 for a stray byte).
RDE_INTERNAL u32 fude_zoom_graph_seq(c8 _c) {
    const u8 _u = (u8)_c;
    if((_u & 0xE0u) == 0xC0u) return 2u;
    if((_u & 0xF0u) == 0xE0u) return 3u;
    if((_u & 0xF8u) == 0xF0u) return 4u;
    return 1u;
}

// Text going into a fixed buffer a whole character at a time. Once one does
// not fit nothing more goes in, so a cut never reads as if it were not there.
typedef struct {
    c8* s;
    u32 cap, n;
    b8  full;
} fude_zoom_graph_out;

RDE_INTERNAL void fude_zoom_graph_put(fude_zoom_graph_out* _o, const c8* _s, u32 _n) {
    u32 _i = 0;
    while(_i < _n && !_o->full) {
        u32 _k = fude_zoom_graph_seq(_s[_i]);
        _k = _i + _k <= _n ? _k : _n - _i;
        if(_o->n + _k + 1u > _o->cap) {
            _o->full = true;
            break;
        }
        memcpy(_o->s + _o->n, _s + _i, _k);
        _o->n += _k;
        _i    += _k;
    }
    _o->s[_o->n] = 0;
}

RDE_INTERNAL void fude_zoom_graph_copy(c8* _dst, u32 _cap, const c8* _s, u32 _n) {
    fude_zoom_graph_out _o = { _dst, _cap, 0, false };
    _dst[0] = 0;
    fude_zoom_graph_put(&_o, _s, _n);
}

// <br>, <br/>, <br /> in any case: its length, else 0.
RDE_INTERNAL u32 fude_zoom_graph_br(const c8* _a, const c8* _b) {
    if(_b - _a < 4 || _a[0] != '<' || (_a[1] | 0x20) != 'b' || (_a[2] | 0x20) != 'r') {
        return 0;
    }
    const c8* _p = _a + 3;
    while(_p < _b && *_p == ' ') _p++;
    if(_p < _b && *_p == '/') _p++;
    while(_p < _b && *_p == ' ') _p++;
    return _p < _b && *_p == '>' ? (u32)(_p + 1 - _a) : 0u;
}

// Mermaid's entity codes (# for HTML's &): #quot; #35; #x23;. Its length and
// character, else 0.
RDE_INTERNAL u32 fude_zoom_graph_entity(const c8* _a, const c8* _b, u32* _cp) {
    static const struct { const c8* name; u32 cp; } FUDE_ZOOM_GRAPH_NAMED[] = {
        { "quot", '"' }, { "amp", '&' }, { "lt", '<' }, { "gt", '>' }, { "apos", '\'' }, { "nbsp", ' ' }, { "num", '#' },
    };
    const c8* _p = _a + 1;
    u32 _v = 0, _digits = 0;
    if(_p + 1 < _b && (*_p == 'x' || *_p == 'X') && isxdigit((u8)_p[1])) {
        for(_p++; _p < _b && isxdigit((u8)*_p) && _digits < 6u; _p++, _digits++) {
            const c8 _c = *_p;
            _v = _v * 16u + (u32)(_c <= '9' ? _c - '0' : (_c | 0x20) - 'a' + 10);
        }
    } else if(_p < _b && isdigit((u8)*_p)) {
        for(; _p < _b && isdigit((u8)*_p) && _digits < 7u; _p++, _digits++) {
            _v = _v * 10u + (u32)(*_p - '0');
        }
    } else {
        const c8* _name = _p;
        while(_p < _b && isalpha((u8)*_p) && _p - _name < 8) _p++;
        if(_p >= _b || *_p != ';') {
            return 0;
        }
        for(u32 _i = 0; _i < sizeof(FUDE_ZOOM_GRAPH_NAMED) / sizeof(FUDE_ZOOM_GRAPH_NAMED[0]); _i++) {
            const usize _n = strlen(FUDE_ZOOM_GRAPH_NAMED[_i].name);
            if((usize)(_p - _name) == _n && memcmp(_name, FUDE_ZOOM_GRAPH_NAMED[_i].name, _n) == 0) {
                *_cp = FUDE_ZOOM_GRAPH_NAMED[_i].cp;
                return (u32)(_p + 1 - _a);
            }
        }
        return 0;
    }
    if(_p >= _b || *_p != ';' || _v == 0 || _v > 0x10FFFFu || (_v >= 0xD800u && _v <= 0xDFFFu)) {
        return 0;
    }
    *_cp = _v;
    return (u32)(_p + 1 - _a);
}

// Trims [*_a, *_b) and takes the quotes off a quoted text.
RDE_INTERNAL void fude_zoom_graph_unquote(const c8** _a, const c8** _b) {
    while(*_a < *_b && (fude_zoom_graph_space(**_a) || **_a == '\n')) (*_a)++;
    while(*_b > *_a && (fude_zoom_graph_space((*_b)[-1]) || (*_b)[-1] == '\n')) (*_b)--;
    if(*_b - *_a >= 2 && **_a == '"' && (*_b)[-1] == '"') {
        (*_a)++;
        (*_b)--;
    }
}

// A label's text, as written between [_a, _b), into _dst: trimmed, a markdown
// string's backticks off, <br> a line break, entity codes their characters.
RDE_INTERNAL void fude_zoom_graph_text(c8* _dst, u32 _cap, const c8* _a, const c8* _b) {
    fude_zoom_graph_unquote(&_a, &_b);
    if(_b - _a >= 2 && *_a == '`' && _b[-1] == '`') {
        _a++;
        _b--;
    }
    fude_zoom_graph_out _o = { _dst, _cap, 0, false };
    _dst[0] = 0;
    while(_a < _b && !_o.full) {
        u32 _len = *_a == '<' ? fude_zoom_graph_br(_a, _b) : 0u;
        if(_len > 0) {
            fude_zoom_graph_put(&_o, "\n", 1u);
            _a += _len;
            continue;
        }
        u32 _cp = 0;
        _len = *_a == '#' ? fude_zoom_graph_entity(_a, _b, &_cp) : 0u;
        if(_len > 0) {
            c8 _u[5];
            fude_utf8_put(_cp, _u);
            fude_zoom_graph_put(&_o, _u, (u32)strlen(_u));
            _a += _len;
            continue;
        }
        u32 _k = fude_zoom_graph_seq(*_a);
        _k = _a + _k <= _b ? _k : (u32)(_b - _a);
        fude_zoom_graph_put(&_o, _a, _k);
        _a += _k;
    }
}

// --- building --------------------------------------------------------------------------------

u32 fude_zoom_graph_find(const fude_zoom_graph* _g, const c8* _id) {
    const u32 _n = fude_zoom_graph_count(&_g->nodes);
    for(u32 _i = 0; _i < _n; _i++) {
        if(strcmp(fude_zoom_graph_node_at(_g, _i)->id, _id) == 0) {
            return _i;
        }
    }
    return FUDE_ZOOM_GRAPH_NONE;
}

u32 fude_zoom_graph_add_node(fude_zoom_graph* _g, const c8* _id, const c8* _label, u8 _shape) {
    const usize _n = strlen(_id);
    const u32   _count = fude_zoom_graph_count(&_g->nodes);
    if(_n == 0 || _n >= FUDE_ZOOM_GRAPH_ID || _count >= FUDE_ZOOM_GRAPH_MAX_NODES || fude_zoom_graph_find(_g, _id) != FUDE_ZOOM_GRAPH_NONE) {
        return FUDE_ZOOM_GRAPH_NONE;
    }
    fude_zoom_graph_node _node;
    memset(&_node, 0, sizeof(_node));
    memcpy(_node.id, _id, _n);
    _label = _label != NULL ? _label : _id;
    fude_zoom_graph_copy(_node.label, FUDE_ZOOM_GRAPH_LABEL, _label, (u32)strlen(_label));
    _node.shape = _shape < FUDE_ZOOM_NODE_COUNT ? _shape : FUDE_ZOOM_NODE_RECT;
    rde_arr_add(&_g->nodes, (any)&_node);
    return _count;
}

u32 fude_zoom_graph_add_edge(fude_zoom_graph* _g, u32 _from, u32 _to) {
    const u32 _n = fude_zoom_graph_count(&_g->nodes), _count = fude_zoom_graph_count(&_g->edges);
    if(_from >= _n || _to >= _n || _count >= FUDE_ZOOM_GRAPH_MAX_EDGES) {
        return FUDE_ZOOM_GRAPH_NONE;
    }
    fude_zoom_graph_edge _edge;
    memset(&_edge, 0, sizeof(_edge));
    _edge.from       = _from;
    _edge.to         = _to;
    _edge.head_end   = FUDE_ZOOM_EDGE_HEAD_ARROW;
    _edge.min_length = 1u;
    rde_arr_add(&_g->edges, (any)&_edge);
    return _count;
}

// --- reading Mermaid -------------------------------------------------------------------------

typedef struct {
    fude_zoom_graph* g;
    const c8*        p;
    const c8*        end;
    u32              line;                                   // p's, from 1
    u32              open[FUDE_ZOOM_GRAPH_MAX_DEPTH];        // the subgraphs open (index + 1), innermost last
    u32              open_line[FUDE_ZOOM_GRAPH_MAX_DEPTH];
    u32              depth;
    rde_arr          table;                                  // u32: ids to node index + 1, open addressing
    u32              mask;
    rde_arr          left, right;                            // a chain's two groups at a time (u32 nodes)
    u8               error;
    u32              error_line;
} fude_zoom_graph_reader;

typedef struct {
    u8  line, head_start, head_end;
    u32 length;
    c8  label[FUDE_ZOOM_GRAPH_EDGE_LABEL];
} fude_zoom_graph_link;

RDE_INTERNAL b8 fude_zoom_graph_fail(fude_zoom_graph_reader* _r, u8 _why) {
    if(_r->error == FUDE_ZOOM_MERMAID_OK) {
        _r->error      = _why;
        _r->error_line = _r->line;
    }
    return false;
}

RDE_INTERNAL b8 fude_zoom_graph_id_start(c8 _c) {
    const u8 _u = (u8)_c;
    return (_u >= '0' && _u <= '9') || (_u >= 'a' && _u <= 'z') || (_u >= 'A' && _u <= 'Z') || _u == '_' || _u >= 0x80u;
}

// Whether the byte at _p goes on an id: - and . too, between letters (so
// A-->B and A-.->B are links, node-1 and a.b ids).
RDE_INTERNAL b8 fude_zoom_graph_id_at(const c8* _p, const c8* _end) {
    if(_p >= _end) {
        return false;
    }
    if(fude_zoom_graph_id_start(*_p)) {
        return true;
    }
    return (*_p == '-' || *_p == '.') && _p + 1 < _end && fude_zoom_graph_id_start(_p[1]);
}

RDE_INTERNAL b8 fude_zoom_graph_starts(const c8* _p, const c8* _end, const c8* _s) {
    const usize _n = strlen(_s);
    return (usize)(_end - _p) >= _n && memcmp(_p, _s, _n) == 0;
}

// Whether the statement starts with the word _w, not as part of a longer id.
RDE_INTERNAL b8 fude_zoom_graph_word(const fude_zoom_graph_reader* _r, const c8* _w) {
    return fude_zoom_graph_starts(_r->p, _r->end, _w) && !fude_zoom_graph_id_at(_r->p + strlen(_w), _r->end);
}

// The same, with a space after it (classDef, style, a subgraph's direction).
RDE_INTERNAL b8 fude_zoom_graph_keyword(const fude_zoom_graph_reader* _r, const c8* _w) {
    const c8* _after = _r->p + strlen(_w);
    return fude_zoom_graph_word(_r, _w) && _after < _r->end && (*_after == ' ' || *_after == '\t');
}

RDE_INTERNAL void fude_zoom_graph_skip_spaces(fude_zoom_graph_reader* _r) {
    while(_r->p < _r->end && fude_zoom_graph_space(*_r->p)) _r->p++;
}

// At the statement's end (spaces past): the text's, a line's, a ; or a comment.
RDE_INTERNAL b8 fude_zoom_graph_at_end(fude_zoom_graph_reader* _r) {
    fude_zoom_graph_skip_spaces(_r);
    return _r->p >= _r->end || *_r->p == '\n' || *_r->p == ';' || fude_zoom_graph_starts(_r->p, _r->end, "%%");
}

// The rest of the statement passed over (a ; in quotes does not end it).
RDE_INTERNAL void fude_zoom_graph_skip_statement(fude_zoom_graph_reader* _r) {
    b8 _quoted = false;
    while(_r->p < _r->end && *_r->p != '\n' && (_quoted || *_r->p != ';')) {
        _quoted = *_r->p == '"' ? !_quoted : _quoted;
        _r->p++;
    }
}

RDE_INTERNAL void fude_zoom_graph_skip_line(fude_zoom_graph_reader* _r) {
    while(_r->p < _r->end && *_r->p != '\n') _r->p++;
}

// A %% comment to the line's end; a %%{ directive }%% to its close, over lines.
RDE_INTERNAL void fude_zoom_graph_comment(fude_zoom_graph_reader* _r) {
    if(fude_zoom_graph_starts(_r->p, _r->end, "%%{")) {
        for(const c8* _q = _r->p + 3; _q < _r->end; _q++) {
            if(fude_zoom_graph_starts(_q, _r->end, "}%%")) {
                for(; _r->p < _q; _r->p++) {
                    _r->line += *_r->p == '\n' ? 1u : 0u;
                }
                _r->p = _q + 3;
                return;
            }
        }
    }
    fude_zoom_graph_skip_line(_r);
}

// Past what lies between statements: spaces, line ends, ; and comments.
RDE_INTERNAL void fude_zoom_graph_between(fude_zoom_graph_reader* _r) {
    while(_r->p < _r->end) {
        const c8 _c = *_r->p;
        if(fude_zoom_graph_space(_c) || _c == ';') {
            _r->p++;
        } else if(_c == '\n') {
            _r->p++;
            _r->line++;
        } else if(fude_zoom_graph_starts(_r->p, _r->end, "%%")) {
            fude_zoom_graph_comment(_r);
        } else {
            break;
        }
    }
}

// Whether the line at _p is "---" (spaces aside); *_next is the next line's start.
RDE_INTERNAL b8 fude_zoom_graph_dashes(const c8* _p, const c8* _end, const c8** _next) {
    while(_p < _end && fude_zoom_graph_space(*_p)) _p++;
    const b8 _three = fude_zoom_graph_starts(_p, _end, "---");
    _p += _three ? 3 : 0;
    while(_p < _end && fude_zoom_graph_space(*_p)) _p++;
    const b8 _alone = _three && (_p >= _end || *_p == '\n');
    while(_p < _end && *_p != '\n') _p++;
    *_next = _p < _end ? _p + 1 : _p;
    return _alone;
}

RDE_INTERNAL b8 fude_zoom_graph_header(fude_zoom_graph_reader* _r) {
    fude_zoom_graph_between(_r);
    // Front matter (a title, a config) between --- lines: passed over.
    const c8* _next = NULL;
    if(fude_zoom_graph_dashes(_r->p, _r->end, &_next)) {
        for(;;) {
            _r->p = _next;
            _r->line++;
            if(_r->p >= _r->end) {
                return fude_zoom_graph_fail(_r, FUDE_ZOOM_MERMAID_HEADER);
            }
            if(fude_zoom_graph_dashes(_r->p, _r->end, &_next)) {
                _r->p = _next;
                _r->line += _next[-1] == '\n' ? 1u : 0u;
                break;
            }
        }
        fude_zoom_graph_between(_r);
    }
    if(fude_zoom_graph_word(_r, "flowchart-elk")) {
        _r->p += 13;
    } else if(fude_zoom_graph_word(_r, "flowchart")) {
        _r->p += 9;
    } else if(fude_zoom_graph_word(_r, "graph")) {
        _r->p += 5;
    } else {
        return fude_zoom_graph_fail(_r, FUDE_ZOOM_MERMAID_HEADER);
    }
    if(fude_zoom_graph_at_end(_r)) {
        return true;
    }
    const c8* _a = _r->p;
    while(_r->p < _r->end && !fude_zoom_graph_space(*_r->p) && *_r->p != '\n' && *_r->p != ';') _r->p++;
    const usize _n = (usize)(_r->p - _a);
    c8 _d[3] = { 0, 0, 0 };
    for(usize _i = 0; _i < _n && _i < 2u; _i++) {
        _d[_i] = (c8)toupper((u8)_a[_i]);
    }
    if(_n == 2u && (strcmp(_d, "TB") == 0 || strcmp(_d, "TD") == 0)) {
        _r->g->direction = FUDE_ZOOM_FLOW_TB;
    } else if(_n == 2u && strcmp(_d, "BT") == 0) {
        _r->g->direction = FUDE_ZOOM_FLOW_BT;
    } else if(_n == 2u && strcmp(_d, "LR") == 0) {
        _r->g->direction = FUDE_ZOOM_FLOW_LR;
    } else if(_n == 2u && strcmp(_d, "RL") == 0) {
        _r->g->direction = FUDE_ZOOM_FLOW_RL;
    } else if(_n == 1u && (*_a == '>' || *_a == '<' || *_a == '^' || *_a == 'v')) {
        _r->g->direction = *_a == '>' ? FUDE_ZOOM_FLOW_LR : (*_a == '<' ? FUDE_ZOOM_FLOW_RL : (*_a == '^' ? FUDE_ZOOM_FLOW_BT : FUDE_ZOOM_FLOW_TB));
    } else {
        return fude_zoom_graph_fail(_r, FUDE_ZOOM_MERMAID_DIRECTION);
    }
    return fude_zoom_graph_at_end(_r) ? true : fude_zoom_graph_fail(_r, FUDE_ZOOM_MERMAID_DIRECTION);
}

RDE_INTERNAL u32 fude_zoom_graph_hash(const c8* _s, u32 _n) {
    u32 _h = 2166136261u;
    for(u32 _i = 0; _i < _n; _i++) {
        _h = (_h ^ (u8)_s[_i]) * 16777619u;
    }
    return _h;
}

// The node named _id (_n bytes), made if it is new: a box, its id its text.
RDE_INTERNAL u32 fude_zoom_graph_intern(fude_zoom_graph_reader* _r, const c8* _id, u32 _n) {
    u32* _table = (u32*)_r->table.memory;
    u32  _slot  = fude_zoom_graph_hash(_id, _n) & _r->mask;
    while(_table[_slot] != 0) {
        const fude_zoom_graph_node* _node = fude_zoom_graph_node_at(_r->g, _table[_slot] - 1u);
        if(strlen(_node->id) == _n && memcmp(_node->id, _id, _n) == 0) {
            return _table[_slot] - 1u;
        }
        _slot = (_slot + 1u) & _r->mask;
    }
    const u32 _count = fude_zoom_graph_count(&_r->g->nodes);
    if(_count >= FUDE_ZOOM_GRAPH_MAX_NODES) {
        fude_zoom_graph_fail(_r, FUDE_ZOOM_MERMAID_TOO_BIG);
        return FUDE_ZOOM_GRAPH_NONE;
    }
    fude_zoom_graph_node _node;
    memset(&_node, 0, sizeof(_node));
    memcpy(_node.id, _id, _n);
    fude_zoom_graph_copy(_node.label, FUDE_ZOOM_GRAPH_LABEL, _id, _n);
    rde_arr_add(&_r->g->nodes, (any)&_node);
    _table[_slot] = _count + 1u;
    return _count;
}

// Whether subgraph _inner (index + 1) is _outer or inside it.
RDE_INTERNAL b8 fude_zoom_graph_within(const fude_zoom_graph* _g, u32 _inner, u32 _outer) {
    for(u32 _steps = 0; _inner != 0 && _steps <= FUDE_ZOOM_GRAPH_MAX_SUBGRAPHS; _steps++) {
        if(_inner == _outer) {
            return true;
        }
        _inner = fude_zoom_graph_subgraph_at(_g, _inner - 1u)->parent;
    }
    return _outer == 0;
}

// The text from _r->p to the closer _close (or _close2, if not NULL; which
// into *_which), _r->p left past it. Quoted, it may hold the closer and run
// over lines; else it ends at the first closer on the line.
RDE_INTERNAL b8 fude_zoom_graph_enclosed(fude_zoom_graph_reader* _r, const c8* _close, const c8* _close2, const c8** _a, const c8** _b, u32* _which) {
    const c8* _p = _r->p;
    while(_p < _r->end && fude_zoom_graph_space(*_p)) _p++;
    if(_p < _r->end && *_p == '"') {
        const c8* _q = _p + 1;
        u32 _lines = 0;
        while(_q < _r->end && *_q != '"') {
            _lines += *_q == '\n' ? 1u : 0u;
            _q++;
        }
        if(_q >= _r->end) {
            return fude_zoom_graph_fail(_r, FUDE_ZOOM_MERMAID_QUOTE);
        }
        *_a = _p;
        *_b = _q + 1;
        _r->line += _lines;
        for(_p = _q + 1; _p < _r->end && fude_zoom_graph_space(*_p); _p++) {}
        if(fude_zoom_graph_starts(_p, _r->end, _close)) {
            *_which = 0;
            _r->p   = _p + strlen(_close);
            return true;
        }
        if(_close2 != NULL && fude_zoom_graph_starts(_p, _r->end, _close2)) {
            *_which = 1u;
            _r->p   = _p + strlen(_close2);
            return true;
        }
        return fude_zoom_graph_fail(_r, FUDE_ZOOM_MERMAID_SHAPE);
    }
    for(const c8* _q = _r->p; _q < _r->end && *_q != '\n'; _q++) {
        if(fude_zoom_graph_starts(_q, _r->end, _close) || (_close2 != NULL && fude_zoom_graph_starts(_q, _r->end, _close2))) {
            *_which = fude_zoom_graph_starts(_q, _r->end, _close) ? 0u : 1u;
            *_a     = _r->p;
            *_b     = _q;
            _r->p   = _q + strlen(*_which == 0 ? _close : _close2);
            return true;
        }
    }
    return fude_zoom_graph_fail(_r, FUDE_ZOOM_MERMAID_SHAPE);
}

// A node's brackets, the longer first where one starts another.
typedef struct {
    const c8* open;
    const c8* close;
    const c8* close2;     // the other way to close it (NULL: none)
    u8        shape, shape2;
} fude_zoom_graph_bracket;

static const fude_zoom_graph_bracket FUDE_ZOOM_GRAPH_BRACKETS[] = {
    { "(((", ")))", NULL, FUDE_ZOOM_NODE_DOUBLE_CIRCLE, 0 },
    { "((", "))", NULL, FUDE_ZOOM_NODE_CIRCLE, 0 },
    { "([", "])", NULL, FUDE_ZOOM_NODE_STADIUM, 0 },
    { "(", ")", NULL, FUDE_ZOOM_NODE_ROUND, 0 },
    { "[[", "]]", NULL, FUDE_ZOOM_NODE_SUBROUTINE, 0 },
    { "[(", ")]", NULL, FUDE_ZOOM_NODE_CYLINDER, 0 },
    { "[/", "/]", "\\]", FUDE_ZOOM_NODE_PARALLELOGRAM, FUDE_ZOOM_NODE_TRAPEZOID },
    { "[\\", "\\]", "/]", FUDE_ZOOM_NODE_PARALLELOGRAM_ALT, FUDE_ZOOM_NODE_TRAPEZOID_ALT },
    { "[", "]", NULL, FUDE_ZOOM_NODE_RECT, 0 },
    { "{{", "}}", NULL, FUDE_ZOOM_NODE_HEXAGON, 0 },
    { "{", "}", NULL, FUDE_ZOOM_NODE_DIAMOND, 0 },
    { ">", "]", NULL, FUDE_ZOOM_NODE_FLAG, 0 },
};

// ":::name" after a node: passed over.
RDE_INTERNAL void fude_zoom_graph_class(fude_zoom_graph_reader* _r) {
    if(fude_zoom_graph_starts(_r->p, _r->end, ":::")) {
        _r->p += 3;
        while(fude_zoom_graph_id_at(_r->p, _r->end)) _r->p++;
    }
}

// A node as written (an id, maybe a shape and its text): its index, or NONE.
RDE_INTERNAL u32 fude_zoom_graph_read_node(fude_zoom_graph_reader* _r) {
    const c8* _id = _r->p;
    if(_r->p >= _r->end || !fude_zoom_graph_id_start(*_r->p)) {
        fude_zoom_graph_fail(_r, FUDE_ZOOM_MERMAID_NODE);
        return FUDE_ZOOM_GRAPH_NONE;
    }
    while(fude_zoom_graph_id_at(_r->p, _r->end)) _r->p++;
    const u32 _n = (u32)(_r->p - _id);
    if(_n >= FUDE_ZOOM_GRAPH_ID) {
        fude_zoom_graph_fail(_r, FUDE_ZOOM_MERMAID_ID);
        return FUDE_ZOOM_GRAPH_NONE;
    }
    fude_zoom_graph_class(_r);
    b8 _shaped = false;
    u8 _shape  = FUDE_ZOOM_NODE_RECT;
    const c8 *_a = NULL, *_b = NULL;
    for(u32 _k = 0; _k < sizeof(FUDE_ZOOM_GRAPH_BRACKETS) / sizeof(FUDE_ZOOM_GRAPH_BRACKETS[0]); _k++) {
        const fude_zoom_graph_bracket* _br = &FUDE_ZOOM_GRAPH_BRACKETS[_k];
        if(fude_zoom_graph_starts(_r->p, _r->end, _br->open)) {
            u32 _which = 0;
            _r->p += strlen(_br->open);
            if(!fude_zoom_graph_enclosed(_r, _br->close, _br->close2, &_a, &_b, &_which)) {
                return FUDE_ZOOM_GRAPH_NONE;
            }
            _shape   = _which == 0 ? _br->shape : _br->shape2;
            _shaped  = true;
            break;
        }
    }
    fude_zoom_graph_class(_r);
    const u32 _i = fude_zoom_graph_intern(_r, _id, _n);
    if(_i == FUDE_ZOOM_GRAPH_NONE) {
        return _i;
    }
    fude_zoom_graph_node* _node = fude_zoom_graph_node_at(_r->g, _i);
    if(_shaped) {
        _node->shape = _shape;
        fude_zoom_graph_text(_node->label, FUDE_ZOOM_GRAPH_LABEL, _a, _b);
    }
    // In the innermost subgraph open, unless one it is in already holds that
    // one or lies elsewhere: a node goes to the deepest subgraph naming it, and
    // of two apart, the first (as Mermaid has it).
    if(_r->depth > 0) {
        const u32 _s = _r->open[_r->depth - 1u];
        if(_node->subgraph == 0 || (_node->subgraph != _s && fude_zoom_graph_within(_r->g, _s, _node->subgraph))) {
            _node->subgraph = _s;
        }
    }
    return _i;
}

// After a link's line: > or an o or x that does not begin a word.
RDE_INTERNAL u8 fude_zoom_graph_head(const c8* _p, const c8* _end) {
    if(_p < _end && *_p == '>') {
        return FUDE_ZOOM_EDGE_HEAD_ARROW;
    }
    if(_p < _end && (*_p == 'o' || *_p == 'x') && !fude_zoom_graph_id_at(_p + 1, _end)) {
        return *_p == 'o' ? FUDE_ZOOM_EDGE_HEAD_CIRCLE : FUDE_ZOOM_EDGE_HEAD_CROSS;
    }
    return FUDE_ZOOM_EDGE_HEAD_NONE;
}

// A link's line at *_pp: 1 a whole one (its line, end head and length into
// _l, *_pp past it), 2 the opening of one with text (--, ==, -.), 0 none.
RDE_INTERNAL u32 fude_zoom_graph_token(const c8** _pp, const c8* _end, fude_zoom_graph_link* _l) {
    const c8* _p = *_pp;
    if(_p >= _end) {
        return 0;
    }
    const c8 _c = *_p;
    u32 _n = 0;
    while(_p + _n < _end && _p[_n] == _c) _n++;
    if(_c == '~') {
        if(_n < 3u) {
            return 0;
        }
        _l->line   = FUDE_ZOOM_EDGE_LINE_INVISIBLE;
        _l->length = _n - 2u;
        *_pp       = _p + _n;
        return 1u;
    }
    if(_c == '=' || (_c == '-' && _n >= 2u)) {
        if(_n < 2u) {
            return 0;
        }
        _l->line = _c == '=' ? FUDE_ZOOM_EDGE_LINE_THICK : FUDE_ZOOM_EDGE_LINE_SOLID;
        const u8 _head = fude_zoom_graph_head(_p + _n, _end);
        if(_head != FUDE_ZOOM_EDGE_HEAD_NONE) {
            _l->head_end = _head;
            _l->length   = _n - 1u;
            *_pp         = _p + _n + 1;
            return 1u;
        }
        // The last of three or more is the line's end: --- is as long as -->.
        *_pp = _p + _n;
        if(_n >= 3u) {
            _l->length = _n - 2u;
            return 1u;
        }
        return 2u;
    }
    if(_c == '-' || _c == '.') {
        // Dotted: -.-  -.->  -..->, and .-> (closing a dotted link's text).
        const c8* _q = _p + (_c == '-' ? 1 : 0);
        u32 _dots = 0;
        while(_q < _end && *_q == '.') {
            _dots++;
            _q++;
        }
        if(_dots == 0) {
            return 0;
        }
        _l->line = FUDE_ZOOM_EDGE_LINE_DOTTED;
        if(_q < _end && *_q == '-') {
            _q++;
            _l->length   = _dots;
            _l->head_end = fude_zoom_graph_head(_q, _end);
            *_pp         = _q + (_l->head_end != FUDE_ZOOM_EDGE_HEAD_NONE ? 1 : 0);
            return 1u;
        }
        if(_c == '-') {
            *_pp = _q;
            return 2u;
        }
    }
    return 0;
}

// A link: its heads, line and length, and its text (-- a -->, |a|).
RDE_INTERNAL b8 fude_zoom_graph_read_link(fude_zoom_graph_reader* _r, fude_zoom_graph_link* _l) {
    memset(_l, 0, sizeof(*_l));
    _l->length = 1u;
    const c8* _p = _r->p;
    if(_p + 1 < _r->end && (*_p == '<' || *_p == 'o' || *_p == 'x') && (_p[1] == '-' || _p[1] == '=' || _p[1] == '.')) {
        _l->head_start = *_p == '<' ? FUDE_ZOOM_EDGE_HEAD_ARROW : (*_p == 'o' ? FUDE_ZOOM_EDGE_HEAD_CIRCLE : FUDE_ZOOM_EDGE_HEAD_CROSS);
        _p++;
    }
    const u32 _kind = fude_zoom_graph_token(&_p, _r->end, _l);
    if(_kind == 0) {
        return fude_zoom_graph_fail(_r, FUDE_ZOOM_MERMAID_LINK);
    }
    if(_kind == 2u) {
        // The text runs to the first link of the same line that closes it, on
        // this line (a quoted text may hold anything).
        const c8* _t = _p;
        const c8* _q = _p;
        while(_q < _r->end && fude_zoom_graph_space(*_q)) _q++;
        if(_q < _r->end && *_q == '"') {
            for(_q++; _q < _r->end && *_q != '"' && *_q != '\n'; _q++) {}
            if(_q >= _r->end || *_q != '"') {
                return fude_zoom_graph_fail(_r, FUDE_ZOOM_MERMAID_QUOTE);
            }
        }
        b8 _closed = false;
        for(; _q < _r->end && *_q != '\n' && !_closed; _q++) {
            const c8* _at = _q;
            if(_l->line == FUDE_ZOOM_EDGE_LINE_DOTTED) {
                if(*_q != '.') continue;
                _at = _q > _t && _q[-1] == '-' ? _q - 1 : _q;
            } else if(!(_q + 1 < _r->end && *_q == (_l->line == FUDE_ZOOM_EDGE_LINE_THICK ? '=' : '-') && _q[1] == *_q)) {
                continue;
            }
            fude_zoom_graph_link _close = *_l;
            const c8*            _z     = _at;
            if(fude_zoom_graph_token(&_z, _r->end, &_close) != 1u || _close.line != _l->line) {
                continue;
            }
            fude_zoom_graph_text(_l->label, FUDE_ZOOM_GRAPH_EDGE_LABEL, _t, _at);
            _l->head_end = _close.head_end;
            _l->length   = _close.length;
            _p           = _z;
            _closed      = true;
        }
        if(!_closed) {
            return fude_zoom_graph_fail(_r, FUDE_ZOOM_MERMAID_LABEL);
        }
    }
    _r->p = _p;
    fude_zoom_graph_skip_spaces(_r);
    if(_r->p < _r->end && *_r->p == '|') {
        const c8* _a = _r->p + 1;
        const c8* _q = _a;
        while(_q < _r->end && fude_zoom_graph_space(*_q)) _q++;
        if(_q < _r->end && *_q == '"') {
            for(_q++; _q < _r->end && *_q != '"' && *_q != '\n'; _q++) {}
            if(_q >= _r->end || *_q != '"') {
                return fude_zoom_graph_fail(_r, FUDE_ZOOM_MERMAID_QUOTE);
            }
        }
        while(_q < _r->end && *_q != '|' && *_q != '\n') _q++;
        if(_q >= _r->end || *_q != '|') {
            return fude_zoom_graph_fail(_r, FUDE_ZOOM_MERMAID_LABEL);
        }
        fude_zoom_graph_text(_l->label, FUDE_ZOOM_GRAPH_EDGE_LABEL, _a, _q);
        _r->p = _q + 1;
    }
    return true;
}

// Nodes joined by &, into _into.
RDE_INTERNAL b8 fude_zoom_graph_read_group(fude_zoom_graph_reader* _r, rde_arr* _into) {
    rde_arr_clear(_into);
    for(;;) {
        const u32 _i = fude_zoom_graph_read_node(_r);
        if(_i == FUDE_ZOOM_GRAPH_NONE) {
            return false;
        }
        rde_arr_add(_into, (any)&_i);
        fude_zoom_graph_skip_spaces(_r);
        if(_r->p >= _r->end || *_r->p != '&') {
            return true;
        }
        _r->p++;
        fude_zoom_graph_skip_spaces(_r);
    }
}

// A chain: groups joined by links, an edge for every pair across each link.
RDE_INTERNAL b8 fude_zoom_graph_chain(fude_zoom_graph_reader* _r) {
    if(!fude_zoom_graph_read_group(_r, &_r->left)) {
        return false;
    }
    while(!fude_zoom_graph_at_end(_r)) {
        fude_zoom_graph_link _l;
        if(!fude_zoom_graph_read_link(_r, &_l)) {
            return false;
        }
        fude_zoom_graph_skip_spaces(_r);
        if(!fude_zoom_graph_read_group(_r, &_r->right)) {
            return false;
        }
        const u32 _nl = fude_zoom_graph_count(&_r->left), _nr = fude_zoom_graph_count(&_r->right);
        for(u32 _i = 0; _i < _nl; _i++) {
            for(u32 _j = 0; _j < _nr; _j++) {
                if(fude_zoom_graph_count(&_r->g->edges) >= FUDE_ZOOM_GRAPH_MAX_EDGES) {
                    return fude_zoom_graph_fail(_r, FUDE_ZOOM_MERMAID_TOO_BIG);
                }
                fude_zoom_graph_edge _e;
                memset(&_e, 0, sizeof(_e));
                _e.from       = ((const u32*)_r->left.memory)[_i];
                _e.to         = ((const u32*)_r->right.memory)[_j];
                _e.line       = _l.line;
                _e.head_start = _l.head_start;
                _e.head_end   = _l.head_end;
                _e.min_length = _l.length < FUDE_ZOOM_GRAPH_MAX_LENGTH ? _l.length : FUDE_ZOOM_GRAPH_MAX_LENGTH;
                memcpy(_e.label, _l.label, sizeof(_e.label));
                rde_arr_add(&_r->g->edges, (any)&_e);
            }
        }
        const rde_arr _t = _r->left;
        _r->left  = _r->right;
        _r->right = _t;
    }
    return true;
}

// subgraph id [title] | subgraph id | subgraph "title" | subgraph a title.
RDE_INTERNAL b8 fude_zoom_graph_open(fude_zoom_graph_reader* _r) {
    const u32 _count = fude_zoom_graph_count(&_r->g->subgraphs);
    if(_r->depth >= FUDE_ZOOM_GRAPH_MAX_DEPTH) {
        return fude_zoom_graph_fail(_r, FUDE_ZOOM_MERMAID_DEPTH);
    }
    if(_count >= FUDE_ZOOM_GRAPH_MAX_SUBGRAPHS) {
        return fude_zoom_graph_fail(_r, FUDE_ZOOM_MERMAID_TOO_BIG);
    }
    fude_zoom_graph_subgraph _s;
    memset(&_s, 0, sizeof(_s));
    _s.parent = _r->depth > 0 ? _r->open[_r->depth - 1u] : 0u;
    const u32 _line = _r->line;
    _r->p += 8;
    b8 _named = false;
    if(!fude_zoom_graph_at_end(_r)) {
        const c8* _start = _r->p;
        const c8 *_a = NULL, *_b = NULL;
        u32 _which = 0;
        if(fude_zoom_graph_id_start(*_r->p)) {
            while(fude_zoom_graph_id_at(_r->p, _r->end)) _r->p++;
            const u32 _n = (u32)(_r->p - _start);
            fude_zoom_graph_skip_spaces(_r);
            const b8 _titled = _r->p < _r->end && *_r->p == '[';
            if(_titled || fude_zoom_graph_at_end(_r)) {
                if(_n >= FUDE_ZOOM_GRAPH_ID) {
                    return fude_zoom_graph_fail(_r, FUDE_ZOOM_MERMAID_ID);
                }
                memcpy(_s.id, _start, _n);
                _named = true;
                if(_titled) {
                    _r->p++;
                    if(!fude_zoom_graph_enclosed(_r, "]", NULL, &_a, &_b, &_which)) {
                        return false;
                    }
                    fude_zoom_graph_text(_s.label, FUDE_ZOOM_GRAPH_LABEL, _a, _b);
                } else {
                    fude_zoom_graph_copy(_s.label, FUDE_ZOOM_GRAPH_LABEL, _start, _n);
                }
            }
        }
        if(!_named) {
            // A title of its own: quoted, or the words to the statement's end.
            _r->p = _start;
            if(*_r->p == '"') {
                if(!fude_zoom_graph_enclosed(_r, "", NULL, &_a, &_b, &_which)) {
                    return false;
                }
            } else {
                fude_zoom_graph_skip_statement(_r);
                _a = _start;
                _b = _r->p;
            }
            fude_zoom_graph_text(_s.label, FUDE_ZOOM_GRAPH_LABEL, _a, _b);
        }
    }
    if(!_named) {
        snprintf(_s.id, sizeof(_s.id), "subGraph%u", _count);
    }
    fude_zoom_graph_skip_statement(_r);
    rde_arr_add(&_r->g->subgraphs, (any)&_s);
    _r->open[_r->depth]      = _count + 1u;
    _r->open_line[_r->depth] = _line;
    _r->depth++;
    return true;
}

// Edges to and from a subgraph's id join its box, as in Mermaid: the node
// the id made goes, and the edge keeps one of the subgraph's nodes to stand in
// for the box in the layout (its last for an edge out of it, its first for
// one into it, as they are usually written top to bottom). A subgraph with no
// node of its own to stand in leaves the node be.
RDE_INTERNAL void fude_zoom_graph_join_subgraphs(fude_zoom_graph* _g) {
    const u32 _n = fude_zoom_graph_count(&_g->nodes), _ns = fude_zoom_graph_count(&_g->subgraphs), _ne = fude_zoom_graph_count(&_g->edges);
    if(_ns == 0 || _n == 0) {
        return;
    }
    rde_arr _tables = fude_zoom_graph_tables_new();
    u32* _names = (u32*)fude_zoom_graph_table(&_tables, _n, sizeof(u32));
    u32* _first = (u32*)fude_zoom_graph_table(&_tables, _ns + 1u, sizeof(u32));
    u32* _last  = (u32*)fude_zoom_graph_table(&_tables, _ns + 1u, sizeof(u32));
    u32* _map   = (u32*)fude_zoom_graph_table(&_tables, _n, sizeof(u32));
    for(u32 _i = 0; _i < _n; _i++) {
        for(u32 _s = 0; _s < _ns && _names[_i] == 0; _s++) {
            _names[_i] = strcmp(fude_zoom_graph_node_at(_g, _i)->id, fude_zoom_graph_subgraph_at(_g, _s)->id) == 0 ? _s + 1u : 0u;
        }
    }
    for(u32 _s = 0; _s <= _ns; _s++) {
        _first[_s] = _last[_s] = FUDE_ZOOM_GRAPH_NONE;
    }
    for(u32 _i = 0; _i < _n; _i++) {
        for(u32 _t = _names[_i] == 0 ? fude_zoom_graph_node_at(_g, _i)->subgraph : 0u; _t != 0; _t = fude_zoom_graph_subgraph_at(_g, _t - 1u)->parent) {
            _first[_t] = _first[_t] == FUDE_ZOOM_GRAPH_NONE ? _i : _first[_t];
            _last[_t]  = _i;
        }
    }
    u32 _kept = 0;
    for(u32 _i = 0; _i < _n; _i++) {
        _names[_i] = _names[_i] != 0 && _first[_names[_i]] != FUDE_ZOOM_GRAPH_NONE ? _names[_i] : 0u;
        _map[_i]   = _names[_i] != 0 ? FUDE_ZOOM_GRAPH_NONE : _kept++;
    }
    if(_kept < _n) {
        for(u32 _e = 0; _e < _ne; _e++) {
            fude_zoom_graph_edge* _edge = fude_zoom_graph_edge_at(_g, _e);
            if(_names[_edge->from] != 0) {
                _edge->from_subgraph = _names[_edge->from];
                _edge->from          = _last[_names[_edge->from]];
            }
            if(_names[_edge->to] != 0) {
                _edge->to_subgraph = _names[_edge->to];
                _edge->to          = _first[_names[_edge->to]];
            }
            _edge->from = _map[_edge->from];
            _edge->to   = _map[_edge->to];
        }
        fude_zoom_graph_node* _all = (fude_zoom_graph_node*)_g->nodes.memory;
        for(u32 _i = 0, _k = 0; _i < _n; _i++) {
            if(_map[_i] != FUDE_ZOOM_GRAPH_NONE) {
                _all[_k++] = _all[_i];
            }
        }
        while(fude_zoom_graph_count(&_g->nodes) > _kept) {
            rde_arr_remove(&_g->nodes, fude_zoom_graph_count(&_g->nodes) - 1u);
        }
    }
    fude_zoom_graph_tables_free(&_tables);
}

RDE_INTERNAL b8 fude_zoom_graph_statement(fude_zoom_graph_reader* _r) {
    if(fude_zoom_graph_word(_r, "subgraph")) {
        return fude_zoom_graph_open(_r);
    }
    if(fude_zoom_graph_word(_r, "end")) {
        if(_r->depth == 0) {
            return fude_zoom_graph_fail(_r, FUDE_ZOOM_MERMAID_END);
        }
        _r->depth--;
        _r->p += 3;
        fude_zoom_graph_skip_statement(_r);
        return true;
    }
    // Styling and interaction the canvas has its own ways for, and a
    // subgraph's own direction (it flows as the whole graph does).
    static const c8* FUDE_ZOOM_GRAPH_SKIPPED[] = { "direction", "classDef", "class", "style", "linkStyle", "click" };
    for(u32 _k = 0; _k < sizeof(FUDE_ZOOM_GRAPH_SKIPPED) / sizeof(FUDE_ZOOM_GRAPH_SKIPPED[0]); _k++) {
        if(fude_zoom_graph_keyword(_r, FUDE_ZOOM_GRAPH_SKIPPED[_k])) {
            fude_zoom_graph_skip_statement(_r);
            return true;
        }
    }
    // Accessibility text: accTitle: a, accDescr: a, accDescr { lines }.
    if(fude_zoom_graph_word(_r, "accTitle") || fude_zoom_graph_word(_r, "accDescr")) {
        _r->p += 8;
        fude_zoom_graph_skip_spaces(_r);
        if(_r->p < _r->end && *_r->p == '{') {
            for(; _r->p < _r->end && *_r->p != '}'; _r->p++) {
                _r->line += *_r->p == '\n' ? 1u : 0u;
            }
            _r->p += _r->p < _r->end ? 1 : 0;
        }
        fude_zoom_graph_skip_line(_r);
        return true;
    }
    return fude_zoom_graph_chain(_r);
}

b8 fude_zoom_graph_parse_mermaid(fude_zoom_graph* _g, const c8* _text, u32 _size, u32* _error_line, u8* _error) {
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    fude_zoom_graph_clear(_g);
    fude_zoom_graph_reader _r;
    memset(&_r, 0, sizeof(_r));
    _r.g     = _g;
    _r.p     = _text != NULL ? _text : "";
    _r.end   = _r.p + (_text != NULL ? _size : 0u);
    _r.line  = 1u;
    _r.mask  = FUDE_ZOOM_GRAPH_MAX_NODES * 2u - 1u;
    _r.table = rde_arr_new(sizeof(u32), _heap);
    rde_arr_resize(&_r.table, FUDE_ZOOM_GRAPH_MAX_NODES * 2u);
    _r.left  = rde_arr_new(sizeof(u32), _heap);
    _r.right = rde_arr_new(sizeof(u32), _heap);
    b8 _ok = fude_zoom_graph_header(&_r);
    while(_ok) {
        fude_zoom_graph_between(&_r);
        if(_r.p >= _r.end) {
            break;
        }
        _ok = fude_zoom_graph_statement(&_r);
    }
    if(_ok && _r.depth > 0) {
        _r.line = _r.open_line[_r.depth - 1u];
        _ok     = fude_zoom_graph_fail(&_r, FUDE_ZOOM_MERMAID_OPEN);
    }
    if(_ok) {
        fude_zoom_graph_join_subgraphs(_g);
    }
    rde_arr_free(&_r.table);
    rde_arr_free(&_r.left);
    rde_arr_free(&_r.right);
    if(!_ok) {
        fude_zoom_graph_clear(_g);
    }
    if(_error_line != NULL) {
        *_error_line = _ok ? 0u : _r.error_line;
    }
    if(_error != NULL) {
        *_error = _ok ? (u8)FUDE_ZOOM_MERMAID_OK : _r.error;
    }
    return _ok;
}

// --- laying out ------------------------------------------------------------------------------
//
// Worked out top to bottom ("flow space": x across the ranks, y along them)
// and turned at the end. The layout's own vertices are the nodes (first, at
// their own index), the dummies long edges get at each rank they cross, and
// fillers: a subgraph's stand-in in a rank between its first and last where
// nothing of it is, so its box keeps others out there too.

#define FUDE_ZOOM_GRAPH_SWEEPS  24u          // ordering sweeps, down and up by turns (the best kept)
#define FUDE_ZOOM_GRAPH_NUDGES  8u           // placing passes, each down then up (and half as many inside the boxes)
#define FUDE_ZOOM_GRAPH_DUMMIES (1u << 18)   // dummies at most: a long edge past it is drawn straight

enum {
    FUDE_ZOOM_GRAPH_VERTEX_NODE = 0,
    FUDE_ZOOM_GRAPH_VERTEX_DUMMY,
    FUDE_ZOOM_GRAPH_VERTEX_LABEL,      // a dummy holding its edge's label
    FUDE_ZOOM_GRAPH_VERTEX_FILLER
};

typedef struct {
    u32 node;      // the graph's node; NONE for the others
    u32 sub;       // subgraph index + 1 (0 none); a dummy's: its edge's ends' innermost common one
    i32 rank;
    u32 order;     // its place in its rank
    u32 disc;      // first seen walking down from each node in turn: the first order
    u32 group;     // scratch, arranging a rank
    u8  kind;
    b8  fixed;     // placed: keeps its order among its kind
    b8  pinned;
    f64 w, h;      // across and along
    f64 loop;      // room at its right for a self-loop
    f64 x, y;
    f64 prev;      // across, where it was before (fixed ones)
    f64 key;       // scratch, for sorting
} fude_zoom_graph_vertex;

typedef struct {
    u32 a, b;
    f64 gap;       // x of b at least x of a + gap
} fude_zoom_graph_rule;

typedef struct {
    f64 k1, k2;
    u32 i;
} fude_zoom_graph_sorted;

typedef struct {
    fude_zoom_graph*        g;
    fude_zoom_graph_spacing sp;
    u32                     n, ne, ns;         // nodes, edges, subgraphs
    u32                     nv;                // vertices (once the fillers are in)
    rde_arr                 verts;             // fude_zoom_graph_vertex
    // Per edge.
    u32*                    tail;              // its ends once back edges are turned (NONE: no edge)
    u32*                    head;
    u32*                    minlen;
    b8*                     reversed;
    u32*                    chain_first;       // into chain: its vertices, tail to head (count 0: none)
    u32*                    chain_count;
    u32*                    label_vertex;      // NONE: none
    rde_arr                 chain;             // u32
    // Per vertex, from the chains: its neighbours above and below.
    u32*                    up_first;
    u32*                    up;
    u32*                    down_first;
    u32*                    down;
    u32                     most_degree;
    // Ranks, and the vertices of each in order (and those that are not pinned: its row).
    u32                     ranks;
    u32*                    layer_first;
    u32*                    layer;
    u32*                    row_first;
    u32*                    row;
    u32*                    row_at;            // each vertex's place in row (pinned: NONE)
    // The rules along the rows (fude_zoom_graph_rules), both ways round, and
    // the boxes' edges.
    rde_arr                 rules;             // fude_zoom_graph_rule
    u32                     nvars;
    u32*                    rin_first;
    u32*                    rin;
    u32*                    rout_first;
    u32*                    rout;
    u32*                    topo;
    f64*                    wall;
    f64*                    rank_y;
    f64*                    rank_h;
    // Subgraphs, by index + 1 (0 the top).
    u32*                    parent;
    u32*                    depth;
    u32                     most_depth;
    i32*                    smin;              // its ranks (smin > smax: none)
    i32*                    smax;
    u32*                    sib;               // its place among its siblings
    f64                     pad[4];            // a box's, flow space: left, right, top, bottom
    b8                      labels;            // edge labels have ranks of their own
    b8                      any_pinned;
    f64                     shift_x, shift_y;  // where it all went at the end
    rde_arr                 tables;            // what the pointers above are in (fude_zoom_graph_table)
    rde_arr                 rule_tables;       // ...rin_first, rin, rout_first, rout, topo (made again with the rules)
} fude_zoom_graph_work;

RDE_INTERNAL int fude_zoom_graph_by_keys(const void* _a, const void* _b) {
    const fude_zoom_graph_sorted* _x = (const fude_zoom_graph_sorted*)_a;
    const fude_zoom_graph_sorted* _y = (const fude_zoom_graph_sorted*)_b;
    if(_x->k1 != _y->k1) return _x->k1 < _y->k1 ? -1 : 1;
    if(_x->k2 != _y->k2) return _x->k2 < _y->k2 ? -1 : 1;
    return _x->i < _y->i ? -1 : (_x->i > _y->i ? 1 : 0);
}

RDE_INTERNAL fude_zoom_graph_vertex* fude_zoom_graph_v(const fude_zoom_graph_work* _w) {
    return (fude_zoom_graph_vertex*)_w->verts.memory;
}

RDE_INTERNAL f64 fude_zoom_graph_sane(f64 _s) {
    return isfinite(_s) && _s > 0.0 ? _s : 0.0;
}

// Flow space to the graph's direction, and back.
RDE_INTERNAL fude_zoom_v2 fude_zoom_graph_turn(u8 _dir, f64 _x, f64 _y) {
    switch(_dir) {
        case FUDE_ZOOM_FLOW_BT: return (fude_zoom_v2){ _x, -_y };
        case FUDE_ZOOM_FLOW_LR: return (fude_zoom_v2){ _y, _x };
        case FUDE_ZOOM_FLOW_RL: return (fude_zoom_v2){ -_y, _x };
        default:                return (fude_zoom_v2){ _x, _y };
    }
}

RDE_INTERNAL fude_zoom_v2 fude_zoom_graph_unturn(u8 _dir, f64 _x, f64 _y) {
    switch(_dir) {
        case FUDE_ZOOM_FLOW_BT: return (fude_zoom_v2){ _x, -_y };
        case FUDE_ZOOM_FLOW_LR: return (fude_zoom_v2){ _y, _x };
        case FUDE_ZOOM_FLOW_RL: return (fude_zoom_v2){ _y, -_x };
        default:                return (fude_zoom_v2){ _x, _y };
    }
}

RDE_INTERNAL b8 fude_zoom_graph_sideways(u8 _dir) {
    return _dir == FUDE_ZOOM_FLOW_LR || _dir == FUDE_ZOOM_FLOW_RL;
}

RDE_INTERNAL u32 fude_zoom_graph_add_vertex(fude_zoom_graph_work* _w, u8 _kind, u32 _sub, i32 _rank, f64 _wide, f64 _tall) {
    fude_zoom_graph_vertex _v;
    memset(&_v, 0, sizeof(_v));
    _v.node = FUDE_ZOOM_GRAPH_NONE;
    _v.kind = _kind;
    _v.sub  = _sub;
    _v.rank = _rank;
    _v.w    = _wide;
    _v.h    = _tall;
    rde_arr_add(&_w->verts, (any)&_v);
    return fude_zoom_graph_count(&_w->verts) - 1u;
}

// --- subgraphs as a tree -----------------------------------------------------------------------

// The parents made safe (a loop or a bad index cut at the top) and depths.
RDE_INTERNAL void fude_zoom_graph_tree(fude_zoom_graph_work* _w) {
    const u32 _ns = _w->ns;
    _w->parent = (u32*)fude_zoom_graph_table(&_w->tables, _ns + 1u, sizeof(u32));
    _w->depth  = (u32*)fude_zoom_graph_table(&_w->tables, _ns + 1u, sizeof(u32));
    for(u32 _s = 1; _s <= _ns; _s++) {
        const u32 _p = fude_zoom_graph_subgraph_at(_w->g, _s - 1u)->parent;
        _w->parent[_s] = _p <= _ns && _p != _s ? _p : 0u;
    }
    for(u32 _s = 1; _s <= _ns; _s++) {
        u32 _t = _s, _steps = 0;
        while(_t != 0 && _steps <= _ns) {
            _t = _w->parent[_t];
            _steps++;
        }
        if(_t != 0) {
            _w->parent[_s] = 0;
        }
    }
    for(u32 _s = 1; _s <= _ns; _s++) {
        u32 _d = 0;
        for(u32 _t = _s; _t != 0; _t = _w->parent[_t]) _d++;
        _w->depth[_s]  = _d;
        _w->most_depth = _d > _w->most_depth ? _d : _w->most_depth;
    }
}

RDE_INTERNAL u32 fude_zoom_graph_common(const fude_zoom_graph_work* _w, u32 _a, u32 _b) {
    while(_w->depth[_a] > _w->depth[_b]) _a = _w->parent[_a];
    while(_w->depth[_b] > _w->depth[_a]) _b = _w->parent[_b];
    while(_a != _b) {
        _a = _w->parent[_a];
        _b = _w->parent[_b];
    }
    return _a;
}

// The subgraph just inside _s on the way down to _sub (0: _sub is _s).
RDE_INTERNAL u32 fude_zoom_graph_child_of(const fude_zoom_graph_work* _w, u32 _sub, u32 _s) {
    if(_sub == _s) {
        return 0;
    }
    while(_sub != 0 && _w->parent[_sub] != _s) _sub = _w->parent[_sub];
    return _sub;
}

// _sub's subgraphs outermost first into _path (most_depth long). How many.
RDE_INTERNAL u32 fude_zoom_graph_path(const fude_zoom_graph_work* _w, u32 _sub, u32* _path) {
    const u32 _n = _w->depth[_sub];
    for(u32 _k = _n; _k > 0; _k--) {
        _path[_k - 1u] = _sub;
        _sub = _w->parent[_sub];
    }
    return _n;
}

// --- cycles and ranks ----------------------------------------------------------------------------

// Back edges turned round: those a depth-first walk (the nodes in order, each
// one's edges in order) finds going back up its own path.
RDE_INTERNAL void fude_zoom_graph_break_cycles(fude_zoom_graph_work* _w) {
    const u32 _n = _w->n, _ne = _w->ne;
    rde_arr _tables = fude_zoom_graph_tables_new();
    u32* _first = (u32*)fude_zoom_graph_table(&_tables, _n + 1u, sizeof(u32));
    u32* _list  = (u32*)fude_zoom_graph_table(&_tables, _ne, sizeof(u32));
    u32* _fill  = (u32*)fude_zoom_graph_table(&_tables, _n + 1u, sizeof(u32));
    u8*  _state = (u8*)fude_zoom_graph_table(&_tables, _n, sizeof(u8));
    u32* _stack = (u32*)fude_zoom_graph_table(&_tables, _n, sizeof(u32));
    u32* _it    = (u32*)fude_zoom_graph_table(&_tables, _n, sizeof(u32));
    for(u32 _e = 0; _e < _ne; _e++) {
        if(_w->tail[_e] != FUDE_ZOOM_GRAPH_NONE && _w->tail[_e] != _w->head[_e]) {
            _first[_w->tail[_e] + 1u]++;
        }
    }
    for(u32 _i = 0; _i < _n; _i++) {
        _first[_i + 1u] += _first[_i];
        _fill[_i]        = _first[_i];
    }
    for(u32 _e = 0; _e < _ne; _e++) {
        if(_w->tail[_e] != FUDE_ZOOM_GRAPH_NONE && _w->tail[_e] != _w->head[_e]) {
            _list[_fill[_w->tail[_e]]++] = _e;
        }
    }
    for(u32 _root = 0; _root < _n; _root++) {
        if(_state[_root] != 0) {
            continue;
        }
        u32 _sp = 0;
        _stack[_sp++] = _root;
        _state[_root] = 1;
        _it[_root]    = _first[_root];
        while(_sp > 0) {
            const u32 _v = _stack[_sp - 1u];
            if(_it[_v] < _first[_v + 1u]) {
                const u32 _e = _list[_it[_v]++];
                const u32 _t = _w->head[_e];
                if(_state[_t] == 1) {
                    _w->reversed[_e] = true;
                } else if(_state[_t] == 0) {
                    _state[_t]    = 1;
                    _it[_t]       = _first[_t];
                    _stack[_sp++] = _t;
                }
            } else {
                _state[_v] = 2;
                _sp--;
            }
        }
    }
    for(u32 _e = 0; _e < _ne; _e++) {
        if(_w->reversed[_e]) {
            const u32 _t = _w->tail[_e];
            _w->tail[_e] = _w->head[_e];
            _w->head[_e] = _t;
        }
    }
    fude_zoom_graph_tables_free(&_tables);
}

// Ranks by longest path from the sources, then each node that feeds more
// edges than it is fed pulled down as far as its edges let it (shorter edges
// in all: a source joins what it feeds instead of waiting at the top).
RDE_INTERNAL void fude_zoom_graph_rank(fude_zoom_graph_work* _w) {
    const u32 _n = _w->n, _ne = _w->ne;
    fude_zoom_graph_vertex* _v = fude_zoom_graph_v(_w);
    rde_arr _tables = fude_zoom_graph_tables_new();
    u32* _first = (u32*)fude_zoom_graph_table(&_tables, _n + 1u, sizeof(u32));
    u32* _list  = (u32*)fude_zoom_graph_table(&_tables, _ne, sizeof(u32));
    u32* _fill  = (u32*)fude_zoom_graph_table(&_tables, _n + 1u, sizeof(u32));
    u32* _in    = (u32*)fude_zoom_graph_table(&_tables, _n, sizeof(u32));
    u32* _ins   = (u32*)fude_zoom_graph_table(&_tables, _n, sizeof(u32));
    u32* _topo  = (u32*)fude_zoom_graph_table(&_tables, _n, sizeof(u32));
    u8*  _done  = (u8*)fude_zoom_graph_table(&_tables, _n, sizeof(u8));
    for(u32 _e = 0; _e < _ne; _e++) {
        if(_w->tail[_e] != FUDE_ZOOM_GRAPH_NONE && _w->tail[_e] != _w->head[_e]) {
            _first[_w->tail[_e] + 1u]++;
            _in[_w->head[_e]]++;
        }
    }
    for(u32 _i = 0; _i < _n; _i++) {
        _first[_i + 1u] += _first[_i];
        _fill[_i]        = _first[_i];
        _ins[_i]         = _in[_i];
    }
    for(u32 _e = 0; _e < _ne; _e++) {
        if(_w->tail[_e] != FUDE_ZOOM_GRAPH_NONE && _w->tail[_e] != _w->head[_e]) {
            _list[_fill[_w->tail[_e]]++] = _e;
        }
    }
    u32 _nt = 0, _at = 0;
    for(u32 _i = 0; _i < _n; _i++) {
        _v[_i].rank = 0;
        if(_in[_i] == 0) {
            _topo[_nt++] = _i;
            _done[_i]    = 1;
        }
    }
    while(_at < _nt) {
        const u32 _t = _topo[_at++];
        for(u32 _k = _first[_t]; _k < _first[_t + 1u]; _k++) {
            const u32 _e = _list[_k], _h = _w->head[_e];
            const i32 _r = _v[_t].rank + (i32)_w->minlen[_e];
            _v[_h].rank = _r > _v[_h].rank ? _r : _v[_h].rank;
            if(--_in[_h] == 0 && !_done[_h]) {
                _topo[_nt++] = _h;
                _done[_h]    = 1;
            }
        }
    }
    for(u32 _k = _nt; _k > 0; _k--) {
        const u32 _t = _topo[_k - 1u];
        const u32 _outs = _first[_t + 1u] - _first[_t];
        if(_outs <= _ins[_t]) {
            continue;
        }
        i32 _hi = 0x7FFFFFFF;
        for(u32 _j = _first[_t]; _j < _first[_t + 1u]; _j++) {
            const i32 _r = _v[_w->head[_list[_j]]].rank - (i32)_w->minlen[_list[_j]];
            _hi = _r < _hi ? _r : _hi;
        }
        _v[_t].rank = _hi > _v[_t].rank ? _hi : _v[_t].rank;
    }
    i32 _low = 0x7FFFFFFF, _high = 0;
    for(u32 _i = 0; _i < _n; _i++) {
        _low = _v[_i].rank < _low ? _v[_i].rank : _low;
    }
    for(u32 _i = 0; _i < _n; _i++) {
        _v[_i].rank -= _low;
        _high = _v[_i].rank > _high ? _v[_i].rank : _high;
    }
    _w->ranks = (u32)_high + 1u;
    fude_zoom_graph_tables_free(&_tables);
}

// --- dummies and fillers -------------------------------------------------------------------------

RDE_INTERNAL void fude_zoom_graph_chains(fude_zoom_graph_work* _w) {
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    _w->chain        = rde_arr_new(sizeof(u32), _heap);
    _w->chain_first  = (u32*)fude_zoom_graph_table(&_w->tables, _w->ne, sizeof(u32));
    _w->chain_count  = (u32*)fude_zoom_graph_table(&_w->tables, _w->ne, sizeof(u32));
    _w->label_vertex = (u32*)fude_zoom_graph_table(&_w->tables, _w->ne, sizeof(u32));
    const b8 _side = fude_zoom_graph_sideways(_w->g->direction);
    u32 _dummies = 0;
    for(u32 _e = 0; _e < _w->ne; _e++) {
        _w->label_vertex[_e] = FUDE_ZOOM_GRAPH_NONE;
        const u32 _t = _w->tail[_e], _h = _w->head[_e];
        if(_t == FUDE_ZOOM_GRAPH_NONE || _t == _h) {
            continue;
        }
        const i32 _rt = fude_zoom_graph_v(_w)[_t].rank, _rh = fude_zoom_graph_v(_w)[_h].rank;
        const u32 _span = (u32)(_rh - _rt);
        if(_span > 1u && _dummies + _span - 1u > FUDE_ZOOM_GRAPH_DUMMIES) {
            continue;
        }
        const u32 _sub = fude_zoom_graph_common(_w, fude_zoom_graph_v(_w)[_t].sub, fude_zoom_graph_v(_w)[_h].sub);
        const fude_zoom_graph_edge* _edge = fude_zoom_graph_edge_at(_w->g, _e);
        const f64 _lw = fude_zoom_graph_sane(_edge->label_w), _lh = fude_zoom_graph_sane(_edge->label_h);
        const b8 _labelled = _w->labels && (_lw > 0.0 || _lh > 0.0);
        _w->chain_first[_e] = fude_zoom_graph_count(&_w->chain);
        _w->chain_count[_e] = _span + 1u;
        rde_arr_add(&_w->chain, (any)&_t);
        for(u32 _k = 1; _k < _span; _k++) {
            const b8  _here = _labelled && _k == _span / 2u;
            const u32 _d    = fude_zoom_graph_add_vertex(_w, _here ? FUDE_ZOOM_GRAPH_VERTEX_LABEL : FUDE_ZOOM_GRAPH_VERTEX_DUMMY, _sub, _rt + (i32)_k,
                                                         _here ? (_side ? _lh : _lw) : 0.0, _here ? (_side ? _lw : _lh) : 0.0);
            if(_here) {
                _w->label_vertex[_e] = _d;
            }
            rde_arr_add(&_w->chain, (any)&_d);
        }
        rde_arr_add(&_w->chain, (any)&_h);
        _dummies += _span - 1u;
    }
}

// A (subgraph, rank) as one key, for a set of them.
RDE_INTERNAL u64 fude_zoom_graph_pair(u32 _s, i32 _r) {
    return (((u64)_s << 32) | (u64)(u32)_r) + 1u;
}

RDE_INTERNAL b8 fude_zoom_graph_set_has(const rde_hash_set* _set, u64 _k) {
    return rde_hash_set_contains(_set, (any)&_k);
}

RDE_INTERNAL void fude_zoom_graph_set_add(rde_hash_set* _set, u64 _k) {
    rde_hash_set_add(_set, (any)&_k);
}

// Each subgraph's ranks, and a filler wherever it has none between its first
// and last (the deepest first: one filler stands for its outer ones too).
// Pinned nodes do not count: they are not in the ranks' rows.
RDE_INTERNAL void fude_zoom_graph_fillers(fude_zoom_graph_work* _w) {
    const u32 _ns = _w->ns;
    _w->smin = (i32*)fude_zoom_graph_table(&_w->tables, _ns + 1u, sizeof(i32));
    _w->smax = (i32*)fude_zoom_graph_table(&_w->tables, _ns + 1u, sizeof(i32));
    for(u32 _s = 0; _s <= _ns; _s++) {
        _w->smin[_s] = 0x7FFFFFFF;
        _w->smax[_s] = -1;
    }
    if(_ns == 0) {
        return;
    }
    rde_hash_set TYPE(u64) _set = rde_hash_set_new(sizeof(u64), rde_hash_map_fn_u64_hash, rde_hash_map_fn_u64_cmp, NULL, rde_memory_allocator_get_default_std());
    const u32 _nv = fude_zoom_graph_count(&_w->verts);
    for(u32 _i = 0; _i < _nv; _i++) {
        const fude_zoom_graph_vertex* _v = &fude_zoom_graph_v(_w)[_i];
        if(_v->pinned) {
            continue;
        }
        for(u32 _s = _v->sub; _s != 0; _s = _w->parent[_s]) {
            fude_zoom_graph_set_add(&_set, fude_zoom_graph_pair(_s, _v->rank));
            _w->smin[_s] = _v->rank < _w->smin[_s] ? _v->rank : _w->smin[_s];
            _w->smax[_s] = _v->rank > _w->smax[_s] ? _v->rank : _w->smax[_s];
        }
    }
    rde_arr _tables = fude_zoom_graph_tables_new();
    fude_zoom_graph_sorted* _deep = (fude_zoom_graph_sorted*)fude_zoom_graph_table(&_tables, _ns, sizeof(fude_zoom_graph_sorted));
    for(u32 _s = 1; _s <= _ns; _s++) {
        _deep[_s - 1u] = (fude_zoom_graph_sorted){ -(f64)_w->depth[_s], 0.0, _s };
    }
    qsort(_deep, _ns, sizeof(fude_zoom_graph_sorted), fude_zoom_graph_by_keys);
    for(u32 _k = 0; _k < _ns; _k++) {
        const u32 _s = _deep[_k].i;
        for(i32 _r = _w->smin[_s]; _r <= _w->smax[_s]; _r++) {
            if(fude_zoom_graph_set_has(&_set, fude_zoom_graph_pair(_s, _r))) {
                continue;
            }
            fude_zoom_graph_add_vertex(_w, FUDE_ZOOM_GRAPH_VERTEX_FILLER, _s, _r, 0.0, 0.0);
            for(u32 _t = _s; _t != 0; _t = _w->parent[_t]) {
                fude_zoom_graph_set_add(&_set, fude_zoom_graph_pair(_t, _r));
            }
        }
    }
    fude_zoom_graph_tables_free(&_tables);
    rde_hash_set_free(&_set);
}

// Each vertex's neighbours above and below, from the chains (edges in order).
RDE_INTERNAL void fude_zoom_graph_neighbours(fude_zoom_graph_work* _w) {
    const u32 _nv = fude_zoom_graph_count(&_w->verts);
    const u32* _chain = (const u32*)_w->chain.memory;
    _w->up_first   = (u32*)fude_zoom_graph_table(&_w->tables, _nv + 1u, sizeof(u32));
    _w->down_first = (u32*)fude_zoom_graph_table(&_w->tables, _nv + 1u, sizeof(u32));
    u32 _segments = 0;
    for(u32 _e = 0; _e < _w->ne; _e++) {
        for(u32 _k = 0; _k + 1u < _w->chain_count[_e]; _k++) {
            _w->down_first[_chain[_w->chain_first[_e] + _k] + 1u]++;
            _w->up_first[_chain[_w->chain_first[_e] + _k + 1u] + 1u]++;
            _segments++;
        }
    }
    rde_arr _tables = fude_zoom_graph_tables_new();
    u32* _fill_up   = (u32*)fude_zoom_graph_table(&_tables, _nv + 1u, sizeof(u32));
    u32* _fill_down = (u32*)fude_zoom_graph_table(&_tables, _nv + 1u, sizeof(u32));
    for(u32 _i = 0; _i < _nv; _i++) {
        const u32 _du = _w->up_first[_i + 1u], _dd = _w->down_first[_i + 1u];
        _w->most_degree      = _du + _dd > _w->most_degree ? _du + _dd : _w->most_degree;
        _w->up_first[_i + 1u]   += _w->up_first[_i];
        _w->down_first[_i + 1u] += _w->down_first[_i];
        _fill_up[_i]   = _w->up_first[_i];
        _fill_down[_i] = _w->down_first[_i];
    }
    _w->up   = (u32*)fude_zoom_graph_table(&_w->tables, _segments, sizeof(u32));
    _w->down = (u32*)fude_zoom_graph_table(&_w->tables, _segments, sizeof(u32));
    for(u32 _e = 0; _e < _w->ne; _e++) {
        for(u32 _k = 0; _k + 1u < _w->chain_count[_e]; _k++) {
            const u32 _a = _chain[_w->chain_first[_e] + _k], _b = _chain[_w->chain_first[_e] + _k + 1u];
            _w->down[_fill_down[_a]++] = _b;
            _w->up[_fill_up[_b]++]     = _a;
        }
    }
    fude_zoom_graph_tables_free(&_tables);
}

// --- order within the ranks ----------------------------------------------------------------------

// Siblings' order, the same in every rank so their boxes never cross: by
// where their members were before when all of them have placed ones, else by
// where their members are now (across their ranks, on average).
RDE_INTERNAL void fude_zoom_graph_siblings(fude_zoom_graph_work* _w) {
    const u32 _ns = _w->ns, _nv = fude_zoom_graph_count(&_w->verts);
    if(_ns == 0) {
        return;
    }
    rde_arr _tables = fude_zoom_graph_tables_new();
    f64* _now   = (f64*)fude_zoom_graph_table(&_tables, _ns + 1u, sizeof(f64));
    f64* _then  = (f64*)fude_zoom_graph_table(&_tables, _ns + 1u, sizeof(f64));
    u32* _count = (u32*)fude_zoom_graph_table(&_tables, _ns + 1u, sizeof(u32));
    u32* _fixed = (u32*)fude_zoom_graph_table(&_tables, _ns + 1u, sizeof(u32));
    fude_zoom_graph_sorted* _kids = (fude_zoom_graph_sorted*)fude_zoom_graph_table(&_tables, _ns, sizeof(fude_zoom_graph_sorted));
    const fude_zoom_graph_vertex* _v = fude_zoom_graph_v(_w);
    for(u32 _i = 0; _i < _nv; _i++) {
        const u32 _size = _w->layer_first[_v[_i].rank + 1] - _w->layer_first[_v[_i].rank];
        const f64 _at   = ((f64)_v[_i].order + 0.5) / (f64)_size;
        for(u32 _s = _v[_i].sub; _s != 0; _s = _w->parent[_s]) {
            _now[_s] += _at;
            _count[_s]++;
            if(_v[_i].kind == FUDE_ZOOM_GRAPH_VERTEX_NODE && _v[_i].fixed) {
                _then[_s] += _v[_i].prev;
                _fixed[_s]++;
            }
        }
    }
    for(u32 _p = 0; _p <= _ns; _p++) {
        u32 _nk = 0;
        b8  _all_fixed = true;
        for(u32 _s = 1; _s <= _ns; _s++) {
            if(_w->parent[_s] == _p) {
                _all_fixed = _all_fixed && _fixed[_s] > 0;
                _kids[_nk++].i = _s;
            }
        }
        for(u32 _k = 0; _k < _nk; _k++) {
            const u32 _s = _kids[_k].i;
            _kids[_k].k1 = _all_fixed ? _then[_s] / (f64)_fixed[_s] : (_count[_s] > 0 ? _now[_s] / (f64)_count[_s] : 0.0);
            _kids[_k].k2 = 0.0;
        }
        qsort(_kids, _nk, sizeof(fude_zoom_graph_sorted), fude_zoom_graph_by_keys);
        for(u32 _k = 0; _k < _nk; _k++) {
            _w->sib[_kids[_k].i] = _k;
        }
    }
    fude_zoom_graph_tables_free(&_tables);
}

typedef struct {
    u32 group;     // the subgraph it stands for (0: a vertex)
    u32 vertex;
    u32 first;     // a subgraph's members in the gathered list
    u32 count;
    f64 key, tie;
    b8  fixed;
    f64 prev;
} fude_zoom_graph_item;

// _list (_m vertices of one rank, all inside subgraph _s) put in order in
// place: by key, each subgraph's members together, the subgraphs in their
// siblings' order and the fixed vertices of this level in theirs (each kept
// to the slots its kind had, so the rest still goes by key).
RDE_INTERNAL void fude_zoom_graph_arrange_level(fude_zoom_graph_work* _w, u32 _s, u32* _list, u32 _m) {
    if(_m == 0) {
        return;
    }
    fude_zoom_graph_vertex* _v = fude_zoom_graph_v(_w);
    rde_arr _tables = fude_zoom_graph_tables_new();
    fude_zoom_graph_sorted* _by    = (fude_zoom_graph_sorted*)fude_zoom_graph_table(&_tables, _m, sizeof(fude_zoom_graph_sorted));
    fude_zoom_graph_sorted* _keyed = (fude_zoom_graph_sorted*)fude_zoom_graph_table(&_tables, _m, sizeof(fude_zoom_graph_sorted));
    fude_zoom_graph_sorted* _kind  = (fude_zoom_graph_sorted*)fude_zoom_graph_table(&_tables, _m, sizeof(fude_zoom_graph_sorted));
    fude_zoom_graph_item*   _items = (fude_zoom_graph_item*)fude_zoom_graph_table(&_tables, _m, sizeof(fude_zoom_graph_item));
    u32*                    _slots = (u32*)fude_zoom_graph_table(&_tables, _m, sizeof(u32));
    u32*                    _seq   = (u32*)fude_zoom_graph_table(&_tables, _m, sizeof(u32));
    for(u32 _i = 0; _i < _m; _i++) {
        _v[_list[_i]].group = fude_zoom_graph_child_of(_w, _v[_list[_i]].sub, _s);
        _by[_i] = (fude_zoom_graph_sorted){ (f64)_v[_list[_i]].group, (f64)_v[_list[_i]].order, _list[_i] };
    }
    qsort(_by, _m, sizeof(fude_zoom_graph_sorted), fude_zoom_graph_by_keys);
    u32 _ni = 0;
    for(u32 _j = 0; _j < _m;) {
        const fude_zoom_graph_vertex* _x = &_v[_by[_j].i];
        fude_zoom_graph_item* _it = &_items[_ni++];
        memset(_it, 0, sizeof(*_it));
        _it->first = _j;
        if(_x->group == 0) {
            _it->vertex = _by[_j].i;
            _it->count  = 1u;
            _it->key    = _x->key;
            _it->tie    = (f64)_x->order;
            _it->fixed  = _x->fixed;
            _it->prev   = _x->prev;
            _j++;
            continue;
        }
        _it->group  = _x->group;
        _it->vertex = FUDE_ZOOM_GRAPH_NONE;
        _it->tie    = (f64)_x->order;
        while(_j < _m && _v[_by[_j].i].group == _it->group) {
            _it->key += _v[_by[_j].i].key;
            _it->tie  = fmin(_it->tie, (f64)_v[_by[_j].i].order);
            _it->count++;
            _j++;
        }
        _it->key /= (f64)_it->count;
    }
    for(u32 _k = 0; _k < _ni; _k++) {
        _keyed[_k] = (fude_zoom_graph_sorted){ _items[_k].key, _items[_k].tie, _k };
    }
    qsort(_keyed, _ni, sizeof(fude_zoom_graph_sorted), fude_zoom_graph_by_keys);
    for(u32 _k = 0; _k < _ni; _k++) {
        _seq[_k] = _keyed[_k].i;
    }
    // The subgraphs into their slots in their siblings' order, then the fixed
    // vertices into theirs in the order they had.
    for(u32 _pass = 0; _pass < 2u; _pass++) {
        u32 _nk = 0;
        for(u32 _k = 0; _k < _ni; _k++) {
            const fude_zoom_graph_item* _it = &_items[_seq[_k]];
            if(_pass == 0 ? _it->group != 0 : (_it->group == 0 && _it->fixed)) {
                _slots[_nk] = _k;
                _kind[_nk]  = (fude_zoom_graph_sorted){ _pass == 0 ? (f64)_w->sib[_it->group] : _it->prev, _pass == 0 ? 0.0 : (f64)_it->vertex, _seq[_k] };
                _nk++;
            }
        }
        qsort(_kind, _nk, sizeof(fude_zoom_graph_sorted), fude_zoom_graph_by_keys);
        for(u32 _k = 0; _k < _nk; _k++) {
            _seq[_slots[_k]] = _kind[_k].i;
        }
    }
    u32 _at = 0;
    for(u32 _k = 0; _k < _ni; _k++) {
        const fude_zoom_graph_item* _it = &_items[_seq[_k]];
        for(u32 _j = 0; _j < _it->count; _j++) {
            _list[_at + _j] = _by[_it->first + _j].i;
        }
        if(_it->group != 0) {
            fude_zoom_graph_arrange_level(_w, _it->group, &_list[_at], _it->count);
        }
        _at += _it->count;
    }
    fude_zoom_graph_tables_free(&_tables);
}

RDE_INTERNAL void fude_zoom_graph_arrange(fude_zoom_graph_work* _w, u32 _r) {
    u32* _list = &_w->layer[_w->layer_first[_r]];
    const u32 _m = _w->layer_first[_r + 1u] - _w->layer_first[_r];
    fude_zoom_graph_arrange_level(_w, 0, _list, _m);
    fude_zoom_graph_vertex* _v = fude_zoom_graph_v(_w);
    for(u32 _i = 0; _i < _m; _i++) {
        _v[_list[_i]].order = _i;
    }
}

// Edge crossings between each rank and the next, all told.
RDE_INTERNAL u64 fude_zoom_graph_crossings(fude_zoom_graph_work* _w) {
    const fude_zoom_graph_vertex* _v = fude_zoom_graph_v(_w);
    u32 _widest = 0;
    for(u32 _r = 0; _r < _w->ranks; _r++) {
        const u32 _m = _w->layer_first[_r + 1u] - _w->layer_first[_r];
        _widest = _m > _widest ? _m : _widest;
    }
    rde_arr _tables = fude_zoom_graph_tables_new();
    u32* _tree = (u32*)fude_zoom_graph_table(&_tables, _widest + 1u, sizeof(u32));
    u32* _at   = (u32*)fude_zoom_graph_table(&_tables, _w->most_degree + 1u, sizeof(u32));
    u64  _total = 0;
    for(u32 _r = 0; _r + 1u < _w->ranks; _r++) {
        const u32 _below = _w->layer_first[_r + 2u] - _w->layer_first[_r + 1u];
        memset(_tree, 0, (_below + 1u) * sizeof(u32));
        u32 _inserted = 0;
        for(u32 _i = _w->layer_first[_r]; _i < _w->layer_first[_r + 1u]; _i++) {
            const u32 _u = _w->layer[_i];
            u32 _n = 0;
            for(u32 _k = _w->down_first[_u]; _k < _w->down_first[_u + 1u]; _k++) {
                // Sorted as they go in (a vertex has few).
                u32 _p = _v[_w->down[_k]].order, _j = _n++;
                while(_j > 0 && _at[_j - 1u] > _p) {
                    _at[_j] = _at[_j - 1u];
                    _j--;
                }
                _at[_j] = _p;
            }
            for(u32 _k = 0; _k < _n; _k++) {
                u32 _not_after = 0;
                for(u32 _q = _at[_k] + 1u; _q > 0; _q -= _q & (0u - _q)) _not_after += _tree[_q];
                _total += _inserted - _not_after;
            }
            for(u32 _k = 0; _k < _n; _k++) {
                for(u32 _q = _at[_k] + 1u; _q <= _below; _q += _q & (0u - _q)) _tree[_q]++;
                _inserted++;
            }
        }
    }
    fude_zoom_graph_tables_free(&_tables);
    return _total;
}

// The ranks' order: first as a walk down from each node in turn found them,
// then barycentre sweeps, down and up by turns, each rank by where its
// neighbours are in the rank just done, the one with fewest crossings kept.
RDE_INTERNAL void fude_zoom_graph_order(fude_zoom_graph_work* _w) {
    const u32 _nv = fude_zoom_graph_count(&_w->verts);
    fude_zoom_graph_vertex* _v = fude_zoom_graph_v(_w);
    // The walk.
    rde_arr _tables = fude_zoom_graph_tables_new();
    u32* _stack = (u32*)fude_zoom_graph_table(&_tables, _nv, sizeof(u32));
    u32* _it    = (u32*)fude_zoom_graph_table(&_tables, _nv, sizeof(u32));
    u8*  _seen  = (u8*)fude_zoom_graph_table(&_tables, _nv, sizeof(u8));
    u32  _disc  = 0;
    for(u32 _root = 0; _root < _nv; _root++) {
        if(_seen[_root]) {
            continue;
        }
        u32 _sp = 0;
        _stack[_sp++] = _root;
        _seen[_root]  = 1;
        _v[_root].disc = _disc++;
        _it[_root]    = _w->down_first[_root];
        while(_sp > 0) {
            const u32 _x = _stack[_sp - 1u];
            if(_it[_x] < _w->down_first[_x + 1u]) {
                const u32 _y = _w->down[_it[_x]++];
                if(!_seen[_y]) {
                    _seen[_y]      = 1;
                    _v[_y].disc    = _disc++;
                    _it[_y]        = _w->down_first[_y];
                    _stack[_sp++]  = _y;
                }
            } else {
                _sp--;
            }
        }
    }
    fude_zoom_graph_tables_clear(&_tables);
    // The ranks, each in the walk's order.
    _w->layer_first = (u32*)fude_zoom_graph_table(&_w->tables, _w->ranks + 1u, sizeof(u32));
    _w->layer       = (u32*)fude_zoom_graph_table(&_w->tables, _nv, sizeof(u32));
    fude_zoom_graph_sorted* _all = (fude_zoom_graph_sorted*)fude_zoom_graph_table(&_tables, _nv, sizeof(fude_zoom_graph_sorted));
    for(u32 _i = 0; _i < _nv; _i++) {
        _all[_i] = (fude_zoom_graph_sorted){ (f64)_v[_i].rank, (f64)_v[_i].disc, _i };
        _w->layer_first[_v[_i].rank + 1]++;
    }
    qsort(_all, _nv, sizeof(fude_zoom_graph_sorted), fude_zoom_graph_by_keys);
    for(u32 _r = 0; _r < _w->ranks; _r++) {
        _w->layer_first[_r + 1u] += _w->layer_first[_r];
    }
    for(u32 _i = 0; _i < _nv; _i++) {
        _w->layer[_i] = _all[_i].i;
        _v[_all[_i].i].order = _i - _w->layer_first[_v[_all[_i].i].rank];
    }
    fude_zoom_graph_tables_clear(&_tables);
    fude_zoom_graph_siblings(_w);
    for(u32 _r = 0; _r < _w->ranks; _r++) {
        for(u32 _i = _w->layer_first[_r]; _i < _w->layer_first[_r + 1u]; _i++) {
            _v[_w->layer[_i]].key = (f64)_v[_w->layer[_i]].order;
        }
        fude_zoom_graph_arrange(_w, _r);
    }
    u32* _best       = (u32*)fude_zoom_graph_table(&_tables, _nv, sizeof(u32));
    u64  _best_cross = fude_zoom_graph_crossings(_w);
    memcpy(_best, _w->layer, (usize)_nv * sizeof(u32));
    for(u32 _sweep = 0; _sweep < FUDE_ZOOM_GRAPH_SWEEPS && _best_cross > 0; _sweep++) {
        const b8 _down = (_sweep & 1u) == 0;
        fude_zoom_graph_siblings(_w);
        for(u32 _k = 0; _k < _w->ranks; _k++) {
            const u32 _r = _down ? _k : _w->ranks - 1u - _k;
            for(u32 _i = _w->layer_first[_r]; _i < _w->layer_first[_r + 1u]; _i++) {
                fude_zoom_graph_vertex* _x = &_v[_w->layer[_i]];
                const u32 _from = _down ? _w->up_first[_w->layer[_i]] : _w->down_first[_w->layer[_i]];
                const u32 _to   = _down ? _w->up_first[_w->layer[_i] + 1u] : _w->down_first[_w->layer[_i] + 1u];
                if(_k == 0 || _from == _to) {
                    _x->key = (f64)_x->order;
                    continue;
                }
                f64 _sum = 0.0;
                for(u32 _j = _from; _j < _to; _j++) {
                    _sum += (f64)_v[_down ? _w->up[_j] : _w->down[_j]].order;
                }
                _x->key = _sum / (f64)(_to - _from);
            }
            fude_zoom_graph_arrange(_w, _r);
        }
        const u64 _cross = fude_zoom_graph_crossings(_w);
        if(_cross < _best_cross) {
            _best_cross = _cross;
            memcpy(_best, _w->layer, (usize)_nv * sizeof(u32));
        }
    }
    memcpy(_w->layer, _best, (usize)_nv * sizeof(u32));
    for(u32 _r = 0; _r < _w->ranks; _r++) {
        for(u32 _i = _w->layer_first[_r]; _i < _w->layer_first[_r + 1u]; _i++) {
            _v[_w->layer[_i]].order = _i - _w->layer_first[_r];
        }
    }
    fude_zoom_graph_tables_free(&_tables);
}

// --- places ------------------------------------------------------------------------------------

// How much of node_gap a vertex keeps from its neighbours: a node all of it,
// an edge's bend half.
RDE_INTERNAL f64 fude_zoom_graph_room(const fude_zoom_graph_vertex* _a) {
    return _a->kind == FUDE_ZOOM_GRAPH_VERTEX_NODE || _a->kind == FUDE_ZOOM_GRAPH_VERTEX_LABEL ? 1.0 : 0.5;
}

RDE_INTERNAL f64 fude_zoom_graph_gap(const fude_zoom_graph_work* _w, const fude_zoom_graph_vertex* _a, const fude_zoom_graph_vertex* _b) {
    return _w->sp.node_gap * fmax(fude_zoom_graph_room(_a), fude_zoom_graph_room(_b));
}

// How far apart the centres of _a and _b (next to it on its right) must be:
// their halves, the gap, and the padding of every box edge between them.
RDE_INTERNAL f64 fude_zoom_graph_sep(const fude_zoom_graph_work* _w, const fude_zoom_graph_vertex* _a, const fude_zoom_graph_vertex* _b) {
    const u32 _c   = fude_zoom_graph_common(_w, _a->sub, _b->sub);
    const f64 _out = (f64)(_w->depth[_a->sub] - _w->depth[_c]), _in = (f64)(_w->depth[_b->sub] - _w->depth[_c]);
    return _a->w * 0.5 + _a->loop + _b->w * 0.5 + fude_zoom_graph_gap(_w, _a, _b) + _out * _w->pad[1] + _in * _w->pad[0];
}

// The x that keep their order, each at least _sep[i] from the next, nearest
// (least squares, by _weight) to _want: pool adjacent violators, on the
// wants less the gaps' running sum (which makes the gaps a plain order).
RDE_INTERNAL void fude_zoom_graph_fit(f64* _x, const f64* _want, const f64* _weight, const f64* _sep, u32 _m, f64* _off, f64* _bv, f64* _bw, u32* _bstart) {
    u32 _nb = 0;
    for(u32 _i = 0; _i < _m; _i++) {
        _off[_i]    = _i == 0 ? 0.0 : _off[_i - 1u] + _sep[_i - 1u];
        _bv[_nb]    = _want[_i] - _off[_i];
        _bw[_nb]    = _weight[_i];
        _bstart[_nb] = _i;
        _nb++;
        while(_nb > 1u && _bv[_nb - 2u] > _bv[_nb - 1u]) {
            const f64 _tw = _bw[_nb - 2u] + _bw[_nb - 1u];
            _bv[_nb - 2u] = (_bv[_nb - 2u] * _bw[_nb - 2u] + _bv[_nb - 1u] * _bw[_nb - 1u]) / _tw;
            _bw[_nb - 2u] = _tw;
            _nb--;
        }
    }
    for(u32 _b = 0; _b < _nb; _b++) {
        const u32 _end = _b + 1u < _nb ? _bstart[_b + 1u] : _m;
        for(u32 _i = _bstart[_b]; _i < _end; _i++) {
            _x[_i] = _bv[_b] + _off[_i];
        }
    }
}

typedef struct {
    f64* want;
    f64* weight;
    f64* sep;
    f64* x;
    f64* off;
    f64* bv;
    f64* bw;
    u32* bstart;
    f64* near;     // a vertex's neighbours' x
} fude_zoom_graph_scratch;

enum { FUDE_ZOOM_GRAPH_ABOVE = 1, FUDE_ZOOM_GRAPH_BELOW = 2 };

// The x of a vertex or of a box's edge (variables nv + 2s and nv + 2s + 1).
RDE_INTERNAL f64 fude_zoom_graph_at(const fude_zoom_graph_work* _w, u32 _var) {
    return _var < _w->nv ? fude_zoom_graph_v(_w)[_var].x : _w->wall[_var - _w->nv];
}

RDE_INTERNAL void fude_zoom_graph_set_at(fude_zoom_graph_work* _w, u32 _var, f64 _x) {
    if(_var < _w->nv) {
        fude_zoom_graph_v(_w)[_var].x = _x;
    } else {
        _w->wall[_var - _w->nv] = _x;
    }
}

RDE_INTERNAL void fude_zoom_graph_rule_add(rde_arr* _rules, u32 _a, u32 _b, f64 _gap) {
    const fude_zoom_graph_rule _rule = { _a, _b, _gap };
    rde_arr_add(_rules, (any)&_rule);
}

// The rules, "x of b at least x of a + gap", for each two things side by side
// in a row: the vertices, and the subgraph boxes' left and right edges, each
// one line through all its box's ranks. From a vertex to the next along a
// row: out of each box it is in and the other is not (each right edge its
// padding further), the gap, into each box the other is in (each left edge
// its padding further). _boxes false: the vertices alone, as far apart as
// all that adds up to. False if the rules go round in a circle (they should
// not, the siblings being in one order in every rank).
RDE_INTERNAL b8 fude_zoom_graph_rules(fude_zoom_graph_work* _w, b8 _boxes) {
    const u32 _nv = _w->nv, _ns = _boxes ? _w->ns : 0u;
    fude_zoom_graph_vertex* _v = fude_zoom_graph_v(_w);
    _w->nvars = _nv + 2u * _ns;
    rde_arr_clear(&_w->rules);
    rde_arr _tables = fude_zoom_graph_tables_new();
    u32* _pa = (u32*)fude_zoom_graph_table(&_tables, _w->most_depth + 1u, sizeof(u32));
    u32* _pb = (u32*)fude_zoom_graph_table(&_tables, _w->most_depth + 1u, sizeof(u32));
    for(u32 _r = 0; _r < _w->ranks; _r++) {
        u32 _a = FUDE_ZOOM_GRAPH_NONE, _na = 0;
        for(u32 _i = _w->row_first[_r]; _i <= _w->row_first[_r + 1u]; _i++) {
            const b8  _last = _i == _w->row_first[_r + 1u];
            const u32 _b    = _last ? FUDE_ZOOM_GRAPH_NONE : _w->row[_i];
            if(!_boxes) {
                if(_a != FUDE_ZOOM_GRAPH_NONE && !_last) {
                    fude_zoom_graph_rule_add(&_w->rules, _a, _b, fude_zoom_graph_sep(_w, &_v[_a], &_v[_b]));
                }
                _a = _b;
                continue;
            }
            const u32 _nb = _last ? 0u : fude_zoom_graph_path(_w, _v[_b].sub, _pb);
            u32 _lca = 0;
            while(_a != FUDE_ZOOM_GRAPH_NONE && _lca < _na && _lca < _nb && _pa[_lca] == _pb[_lca]) _lca++;
            u32 _cur = _a;
            f64 _off = _a != FUDE_ZOOM_GRAPH_NONE ? _v[_a].w * 0.5 + _v[_a].loop : 0.0;
            for(u32 _k = _na; _a != FUDE_ZOOM_GRAPH_NONE && _k > _lca; _k--) {
                const u32 _right = _nv + 2u * (_pa[_k - 1u] - 1u) + 1u;
                fude_zoom_graph_rule_add(&_w->rules, _cur, _right, _off + _w->pad[1]);
                _cur = _right;
                _off = 0.0;
            }
            if(_last) {
                break;
            }
            f64 _space = _a != FUDE_ZOOM_GRAPH_NONE ? fude_zoom_graph_gap(_w, &_v[_a], &_v[_b]) : 0.0;
            for(u32 _k = _lca; _k < _nb; _k++) {
                const u32 _left = _nv + 2u * (_pb[_k] - 1u);
                if(_cur != FUDE_ZOOM_GRAPH_NONE) {
                    fude_zoom_graph_rule_add(&_w->rules, _cur, _left, _off + _space);
                }
                _cur   = _left;
                _off   = 0.0;
                _space = _w->pad[0];
            }
            if(_cur != FUDE_ZOOM_GRAPH_NONE) {
                fude_zoom_graph_rule_add(&_w->rules, _cur, _b, _off + _space + _v[_b].w * 0.5);
            }
            _a  = _b;
            _na = _nb;
            memcpy(_pa, _pb, (usize)_nb * sizeof(u32));
        }
    }
    fude_zoom_graph_tables_clear(&_tables);
    // Each variable's rules both ways, and an order every rule goes forward in
    // (Kahn's, by index on ties).
    const u32 _nr = fude_zoom_graph_count(&_w->rules), _nvar = _w->nvars;
    const fude_zoom_graph_rule* _rule = (const fude_zoom_graph_rule*)_w->rules.memory;
    fude_zoom_graph_tables_clear(&_w->rule_tables);   // (the last time's)
    _w->rin_first  = (u32*)fude_zoom_graph_table(&_w->rule_tables, _nvar + 1u, sizeof(u32));
    _w->rout_first = (u32*)fude_zoom_graph_table(&_w->rule_tables, _nvar + 1u, sizeof(u32));
    _w->rin        = (u32*)fude_zoom_graph_table(&_w->rule_tables, _nr, sizeof(u32));
    _w->rout       = (u32*)fude_zoom_graph_table(&_w->rule_tables, _nr, sizeof(u32));
    _w->topo       = (u32*)fude_zoom_graph_table(&_w->rule_tables, _nvar, sizeof(u32));
    u32* _fill_in  = (u32*)fude_zoom_graph_table(&_tables, _nvar + 1u, sizeof(u32));
    u32* _fill_out = (u32*)fude_zoom_graph_table(&_tables, _nvar + 1u, sizeof(u32));
    u32* _in       = (u32*)fude_zoom_graph_table(&_tables, _nvar, sizeof(u32));
    for(u32 _k = 0; _k < _nr; _k++) {
        _w->rout_first[_rule[_k].a + 1u]++;
        _w->rin_first[_rule[_k].b + 1u]++;
    }
    for(u32 _i = 0; _i < _nvar; _i++) {
        _in[_i] = _w->rin_first[_i + 1u];
        _w->rout_first[_i + 1u] += _w->rout_first[_i];
        _w->rin_first[_i + 1u]  += _w->rin_first[_i];
        _fill_out[_i] = _w->rout_first[_i];
        _fill_in[_i]  = _w->rin_first[_i];
    }
    for(u32 _k = 0; _k < _nr; _k++) {
        _w->rout[_fill_out[_rule[_k].a]++] = _k;
        _w->rin[_fill_in[_rule[_k].b]++]   = _k;
    }
    u32 _nt = 0, _at = 0;
    for(u32 _i = 0; _i < _nvar; _i++) {
        if(_in[_i] == 0) _w->topo[_nt++] = _i;
    }
    while(_at < _nt) {
        const u32 _x = _w->topo[_at++];
        for(u32 _k = _w->rout_first[_x]; _k < _w->rout_first[_x + 1u]; _k++) {
            if(--_in[_rule[_w->rout[_k]].b] == 0) _w->topo[_nt++] = _rule[_w->rout[_k]].b;
        }
    }
    fude_zoom_graph_tables_free(&_tables);
    return _nt == _nvar;
}

// The least and most x _var may have, its rules' other ends where they are.
// _walls_only: only the box edges count (the row's vertices fit among
// themselves).
RDE_INTERNAL void fude_zoom_graph_room_of(const fude_zoom_graph_work* _w, u32 _var, b8 _walls_only, f64* _lo, f64* _hi) {
    const fude_zoom_graph_rule* _rule = (const fude_zoom_graph_rule*)_w->rules.memory;
    *_lo = -1e300;
    *_hi = 1e300;
    for(u32 _k = _w->rin_first[_var]; _k < _w->rin_first[_var + 1u]; _k++) {
        const fude_zoom_graph_rule* _q = &_rule[_w->rin[_k]];
        if(!_walls_only || _q->a >= _w->nv) *_lo = fmax(*_lo, fude_zoom_graph_at(_w, _q->a) + _q->gap);
    }
    for(u32 _k = _w->rout_first[_var]; _k < _w->rout_first[_var + 1u]; _k++) {
        const fude_zoom_graph_rule* _q = &_rule[_w->rout[_k]];
        if(!_walls_only || _q->b >= _w->nv) *_hi = fmin(*_hi, fude_zoom_graph_at(_w, _q->b) - _q->gap);
    }
}

// Whether a box edge stands between _a and _b, side by side in a row.
RDE_INTERNAL b8 fude_zoom_graph_walled(const fude_zoom_graph_work* _w, u32 _a, u32 _b) {
    const fude_zoom_graph_rule* _rule = (const fude_zoom_graph_rule*)_w->rules.memory;
    return !(_w->rout_first[_a + 1u] - _w->rout_first[_a] == 1u && _rule[_w->rout[_w->rout_first[_a]]].b == _b);
}

// Rank _r's row drawn towards the median of each one's neighbours (above,
// below or both), keeping order and gaps: an exact fit for each run of it
// between box edges (_walls; else the whole row as one), and kept between the
// edges at its ends, which stay where they are.
RDE_INTERNAL void fude_zoom_graph_nudge(fude_zoom_graph_work* _w, u32 _r, u32 _from, b8 _walls, fude_zoom_graph_scratch* _s) {
    fude_zoom_graph_vertex* _v = fude_zoom_graph_v(_w);
    const u32* _row = &_w->row[_w->row_first[_r]];
    const u32  _m   = _w->row_first[_r + 1u] - _w->row_first[_r];
    for(u32 _i = 0; _i < _m; _i++) {
        const u32 _a = _row[_i];
        u32 _n = 0;
        if(_from & FUDE_ZOOM_GRAPH_ABOVE) {
            for(u32 _k = _w->up_first[_a]; _k < _w->up_first[_a + 1u]; _k++) _s->near[_n++] = _v[_w->up[_k]].x;
        }
        if(_from & FUDE_ZOOM_GRAPH_BELOW) {
            for(u32 _k = _w->down_first[_a]; _k < _w->down_first[_a + 1u]; _k++) _s->near[_n++] = _v[_w->down[_k]].x;
        }
        for(u32 _k = 1; _k < _n; _k++) {
            const f64 _t = _s->near[_k];
            u32 _j = _k;
            for(; _j > 0 && _s->near[_j - 1u] > _t; _j--) _s->near[_j] = _s->near[_j - 1u];
            _s->near[_j] = _t;
        }
        if(_n == 0) {
            _s->want[_i]   = _v[_a].x;
            _s->weight[_i] = 0.05;
        } else {
            _s->want[_i]   = (_n & 1u) ? _s->near[_n / 2u] : 0.5 * (_s->near[_n / 2u - 1u] + _s->near[_n / 2u]);
            _s->weight[_i] = _v[_a].kind == FUDE_ZOOM_GRAPH_VERTEX_NODE ? 1.0 : 2.0;   // straighter long edges
        }
        if(_i + 1u < _m) {
            _s->sep[_i] = fude_zoom_graph_sep(_w, &_v[_a], &_v[_row[_i + 1u]]);
        }
    }
    for(u32 _a = 0; _a < _m;) {
        u32 _b = _a + 1u;
        while(_b < _m && !(_walls && fude_zoom_graph_walled(_w, _row[_b - 1u], _row[_b]))) _b++;
        const u32 _len = _b - _a;
        fude_zoom_graph_fit(_s->x, _s->want + _a, _s->weight + _a, _s->sep + _a, _len, _s->off, _s->bv, _s->bw, _s->bstart);
        f64 _lo = -1e300, _hi = 1e300, _ignore = 0.0;
        if(_walls) {
            fude_zoom_graph_room_of(_w, _row[_a], true, &_lo, &_ignore);
            fude_zoom_graph_room_of(_w, _row[_b - 1u], true, &_ignore, &_hi);
            _hi -= _s->off[_len - 1u];
        }
        if(_lo <= _hi) {
            // A uniform bound on an isotonic fit: clamping it is the fit with the bound.
            for(u32 _i = 0; _i < _len; _i++) {
                _v[_row[_a + _i]].x = fmin(fmax(_s->x[_i] - _s->off[_i], _lo), _hi) + _s->off[_i];
            }
        }
        _a = _b;
    }
}

// The box edges drawn in to what they hold (the deepest boxes first, so an
// outer one follows the inner): only ever inwards, so every rule still holds.
RDE_INTERNAL void fude_zoom_graph_hug(fude_zoom_graph_work* _w, const u32* _deepest) {
    const fude_zoom_graph_rule* _rule = (const fude_zoom_graph_rule*)_w->rules.memory;
    if(_w->nvars == _w->nv) {
        return;
    }
    for(u32 _k = 0; _k < _w->ns; _k++) {
        const u32 _left = _w->nv + 2u * (_deepest[_k] - 1u), _right = _left + 1u;
        if(_w->rout_first[_left + 1u] > _w->rout_first[_left]) {
            f64 _x = 1e300;
            for(u32 _j = _w->rout_first[_left]; _j < _w->rout_first[_left + 1u]; _j++) {
                _x = fmin(_x, fude_zoom_graph_at(_w, _rule[_w->rout[_j]].b) - _rule[_w->rout[_j]].gap);
            }
            _w->wall[_left - _w->nv] = _x;
        }
        if(_w->rin_first[_right + 1u] > _w->rin_first[_right]) {
            f64 _x = -1e300;
            for(u32 _j = _w->rin_first[_right]; _j < _w->rin_first[_right + 1u]; _j++) {
                _x = fmax(_x, fude_zoom_graph_at(_w, _rule[_w->rin[_j]].a) + _rule[_w->rin[_j]].gap);
            }
            _w->wall[_right - _w->nv] = _x;
        }
    }
}

// Subgraph boxes made whole, each edge one line through all its ranks, so a
// box holds its members and no one else: from where everything is (each box
// round what it holds), pushed right as little as the rules need and pushed
// left as little, the two met halfway, which keeps every rule.
RDE_INTERNAL void fude_zoom_graph_legalise(fude_zoom_graph_work* _w) {
    const u32 _nv = _w->nv, _nvar = _w->nvars;
    const fude_zoom_graph_vertex* _v = fude_zoom_graph_v(_w);
    const fude_zoom_graph_rule* _rule = (const fude_zoom_graph_rule*)_w->rules.memory;
    rde_arr _tables = fude_zoom_graph_tables_new();
    f64* _right = (f64*)fude_zoom_graph_table(&_tables, _nvar, sizeof(f64));
    f64* _left  = (f64*)fude_zoom_graph_table(&_tables, _nvar, sizeof(f64));
    for(u32 _i = 0; _i < _nv; _i++) {
        _right[_i] = _v[_i].x;
    }
    for(u32 _i = _nv; _i < _nvar; _i++) {
        _right[_i] = ((_i - _nv) & 1u) ? -1e300 : 1e300;
    }
    for(u32 _r = 0; _r < _w->ranks; _r++) {
        for(u32 _i = _w->row_first[_r]; _i < _w->row_first[_r + 1u]; _i++) {
            const fude_zoom_graph_vertex* _x = &_v[_w->row[_i]];
            for(u32 _s = _x->sub; _s != 0; _s = _w->parent[_s]) {
                const u32 _l = _nv + 2u * (_s - 1u);
                _right[_l]      = fmin(_right[_l], _x->x - _x->w * 0.5 - _w->pad[0]);
                _right[_l + 1u] = fmax(_right[_l + 1u], _x->x + _x->w * 0.5 + _x->loop + _w->pad[1]);
            }
        }
    }
    for(u32 _i = _nv; _i < _nvar; _i += 2u) {
        if(_right[_i] > _right[_i + 1u]) {
            _right[_i] = _right[_i + 1u] = 0.0;
        }
    }
    memcpy(_left, _right, (usize)_nvar * sizeof(f64));
    for(u32 _k = 0; _k < _nvar; _k++) {
        const u32 _x = _w->topo[_k];
        for(u32 _j = _w->rout_first[_x]; _j < _w->rout_first[_x + 1u]; _j++) {
            const fude_zoom_graph_rule* _q = &_rule[_w->rout[_j]];
            _right[_q->b] = fmax(_right[_q->b], _right[_x] + _q->gap);
        }
    }
    for(u32 _k = _nvar; _k > 0; _k--) {
        const u32 _x = _w->topo[_k - 1u];
        for(u32 _j = _w->rout_first[_x]; _j < _w->rout_first[_x + 1u]; _j++) {
            const fude_zoom_graph_rule* _q = &_rule[_w->rout[_j]];
            _left[_x] = fmin(_left[_x], _left[_q->b] - _q->gap);
        }
    }
    for(u32 _i = 0; _i < _nvar; _i++) {
        if(_i >= _nv || _w->row_at[_i] != FUDE_ZOOM_GRAPH_NONE) {
            fude_zoom_graph_set_at(_w, _i, 0.5 * (_right[_i] + _left[_i]));
        }
    }
    fude_zoom_graph_tables_free(&_tables);
}

// Long edges made straight where their rows let them: each edge's dummies in
// runs, as long as the rows they cross have a place in common, each run on one
// line across (where its middle one is, or as near as it can be).
RDE_INTERNAL void fude_zoom_graph_straighten(fude_zoom_graph_work* _w) {
    fude_zoom_graph_vertex* _v     = fude_zoom_graph_v(_w);
    const u32*              _chain = (const u32*)_w->chain.memory;
    rde_arr                 _tables = fude_zoom_graph_tables_new();
    f64*                    _xs    = (f64*)fude_zoom_graph_table(&_tables, _w->ranks + 1u, sizeof(f64));
    for(u32 _e = 0; _e < _w->ne; _e++) {
        const u32  _n = _w->chain_count[_e];
        const u32* _c = &_chain[_w->chain_first[_e]];
        for(u32 _k = 1; _k + 1u < _n;) {
            f64 _lo = -1e300, _hi = 1e300;
            u32 _end = _k;
            for(; _end + 1u < _n; _end++) {
                f64 _a, _b;
                fude_zoom_graph_room_of(_w, _c[_end], false, &_a, &_b);
                if(fmax(_lo, _a) > fmin(_hi, _b)) {
                    break;
                }
                _lo = fmax(_lo, _a);
                _hi = fmin(_hi, _b);
                u32 _j = _end - _k;
                for(; _j > 0 && _xs[_j - 1u] > _v[_c[_end]].x; _j--) _xs[_j] = _xs[_j - 1u];
                _xs[_j] = _v[_c[_end]].x;
            }
            if(_end - _k >= 2u) {
                const f64 _at = fmin(fmax(_xs[(_end - _k - 1u) / 2u], _lo), _hi);
                for(u32 _j = _k; _j < _end; _j++) {
                    _v[_c[_j]].x = _at;
                }
            }
            _k = _end > _k ? _end : _k + 1u;
        }
    }
    fude_zoom_graph_tables_free(&_tables);
}

// The pinned nodes kept clear of: along each row, left to right, a vertex
// that would touch one goes past it, to whichever side is nearer (left only
// if there is room), and the rest of the row after it.
RDE_INTERNAL void fude_zoom_graph_avoid(fude_zoom_graph_work* _w) {
    fude_zoom_graph_vertex* _v = fude_zoom_graph_v(_w);
    rde_arr _tables = fude_zoom_graph_tables_new();
    fude_zoom_graph_sorted* _obs = (fude_zoom_graph_sorted*)fude_zoom_graph_table(&_tables, _w->n, sizeof(fude_zoom_graph_sorted));
    const f64 _g = _w->sp.node_gap;
    for(u32 _r = 0; _r < _w->ranks; _r++) {
        const f64 _top = _w->rank_y[_r] - _w->rank_h[_r] * 0.5, _bottom = _w->rank_y[_r] + _w->rank_h[_r] * 0.5;
        u32 _no = 0;
        for(u32 _p = 0; _p < _w->n; _p++) {
            if(_v[_p].pinned && _v[_p].y - _v[_p].h * 0.5 < _bottom && _v[_p].y + _v[_p].h * 0.5 > _top) {
                _obs[_no++] = (fude_zoom_graph_sorted){ _v[_p].x - _v[_p].w * 0.5, _v[_p].x + _v[_p].w * 0.5 + _v[_p].loop, _p };
            }
        }
        if(_no == 0) {
            continue;
        }
        qsort(_obs, _no, sizeof(fude_zoom_graph_sorted), fude_zoom_graph_by_keys);
        u32 _prev = FUDE_ZOOM_GRAPH_NONE;
        for(u32 _i = _w->row_first[_r]; _i < _w->row_first[_r + 1u]; _i++) {
            fude_zoom_graph_vertex* _x = &_v[_w->row[_i]];
            const f64 _lo = _prev != FUDE_ZOOM_GRAPH_NONE ? _v[_prev].x + fude_zoom_graph_sep(_w, &_v[_prev], _x) : -1e300;
            f64 _at = fmax(_x->x, _lo);
            for(u32 _guard = 0; _guard <= _no; _guard++) {
                u32 _hit = FUDE_ZOOM_GRAPH_NONE;
                for(u32 _o = 0; _o < _no && _hit == FUDE_ZOOM_GRAPH_NONE; _o++) {
                    if(_at - _x->w * 0.5 - _g < _obs[_o].k2 && _at + _x->w * 0.5 + _x->loop + _g > _obs[_o].k1) _hit = _o;
                }
                if(_hit == FUDE_ZOOM_GRAPH_NONE) {
                    break;
                }
                const f64 _l = _obs[_hit].k1 - _g - _x->w * 0.5 - _x->loop, _rr = _obs[_hit].k2 + _g + _x->w * 0.5;
                b8 _left_free = _l >= _lo && _at - _l <= _rr - _at;
                for(u32 _o = 0; _o < _no && _left_free; _o++) {
                    _left_free = !(_l - _x->w * 0.5 - _g < _obs[_o].k2 && _l + _x->w * 0.5 + _x->loop + _g > _obs[_o].k1);
                }
                _at = _left_free ? _l : _rr;
            }
            _x->x = _at;
            _prev = _w->row[_i];
        }
    }
    fude_zoom_graph_tables_free(&_tables);
}

// Each rank's line along the flow: as tall as its tallest (pinned ones count,
// as the ranks line up on them), with room between for the padding (and
// title) of the boxes that end above and begin below.
RDE_INTERNAL void fude_zoom_graph_rank_lines(fude_zoom_graph_work* _w) {
    const u32 _ns = _w->ns, _nv = _w->nv;
    fude_zoom_graph_vertex* _v = fude_zoom_graph_v(_w);
    _w->rank_y = (f64*)fude_zoom_graph_table(&_w->tables, _w->ranks, sizeof(f64));
    _w->rank_h = (f64*)fude_zoom_graph_table(&_w->tables, _w->ranks, sizeof(f64));
    rde_arr _tables = fude_zoom_graph_tables_new();
    f64* _below = (f64*)fude_zoom_graph_table(&_tables, _w->ranks, sizeof(f64));
    f64* _above = (f64*)fude_zoom_graph_table(&_tables, _w->ranks, sizeof(f64));
    for(u32 _i = 0; _i < _nv; _i++) {
        _w->rank_h[_v[_i].rank] = fmax(_w->rank_h[_v[_i].rank], _v[_i].h);
    }
    if(_ns > 0) {
        // How far each box reaches past its first and last ranks: its own pad
        // and that of the boxes inside it ending there too.
        f64* _top    = (f64*)fude_zoom_graph_table(&_tables, _ns + 1u, sizeof(f64));
        f64* _bottom = (f64*)fude_zoom_graph_table(&_tables, _ns + 1u, sizeof(f64));
        for(u32 _d = _w->most_depth; _d > 0; _d--) {
            for(u32 _s = 1; _s <= _ns; _s++) {
                if(_w->depth[_s] != _d || _w->smin[_s] > _w->smax[_s]) {
                    continue;
                }
                _top[_s]    += _w->pad[2];
                _bottom[_s] += _w->pad[3];
                _above[_w->smin[_s]] = fmax(_above[_w->smin[_s]], _top[_s]);
                _below[_w->smax[_s]] = fmax(_below[_w->smax[_s]], _bottom[_s]);
                const u32 _p = _w->parent[_s];
                if(_p != 0 && _w->smin[_p] == _w->smin[_s]) _top[_p] = fmax(_top[_p], _top[_s]);
                if(_p != 0 && _w->smax[_p] == _w->smax[_s]) _bottom[_p] = fmax(_bottom[_p], _bottom[_s]);
            }
        }
    }
    const f64 _base = _w->labels ? _w->sp.rank_gap * 0.5 : _w->sp.rank_gap;
    for(u32 _r = 0; _r < _w->ranks; _r++) {
        if(_r == 0) {
            _w->rank_y[0] = _w->rank_h[0] * 0.5;
            continue;
        }
        const f64 _boxes = _below[_r - 1u] + _above[_r];
        const f64 _gap   = _boxes > 0.0 ? fmax(_base, _boxes + _base * 0.5) : _base;
        _w->rank_y[_r] = _w->rank_y[_r - 1u] + _w->rank_h[_r - 1u] * 0.5 + _gap + _w->rank_h[_r] * 0.5;
    }
    fude_zoom_graph_tables_free(&_tables);
}

// Along each rank: packed, drawn towards their neighbours, the boxes made
// whole, drawn towards their neighbours again inside them, long edges
// straightened, and the pinned nodes kept clear of.
RDE_INTERNAL void fude_zoom_graph_place(fude_zoom_graph_work* _w) {
    const u32 _nv = _w->nv;
    fude_zoom_graph_vertex* _v = fude_zoom_graph_v(_w);
    // The rows: each rank without its pinned nodes.
    _w->row_first = (u32*)fude_zoom_graph_table(&_w->tables, _w->ranks + 1u, sizeof(u32));
    _w->row       = (u32*)fude_zoom_graph_table(&_w->tables, _nv, sizeof(u32));
    _w->row_at    = (u32*)fude_zoom_graph_table(&_w->tables, _nv, sizeof(u32));
    u32 _nrow = 0, _widest = 0;
    for(u32 _r = 0; _r < _w->ranks; _r++) {
        _w->row_first[_r] = _nrow;
        for(u32 _i = _w->layer_first[_r]; _i < _w->layer_first[_r + 1u]; _i++) {
            _w->row_at[_w->layer[_i]] = _v[_w->layer[_i]].pinned ? FUDE_ZOOM_GRAPH_NONE : _nrow;
            if(!_v[_w->layer[_i]].pinned) _w->row[_nrow++] = _w->layer[_i];
        }
        _widest = _nrow - _w->row_first[_r] > _widest ? _nrow - _w->row_first[_r] : _widest;
    }
    _w->row_first[_w->ranks] = _nrow;
    // Lines along the flow, moved to where the pinned nodes have their ranks.
    fude_zoom_graph_rank_lines(_w);
    rde_arr _tables = fude_zoom_graph_tables_new();
    f64 _centre = 0.0;
    if(_w->any_pinned) {
        fude_zoom_graph_sorted* _off = (fude_zoom_graph_sorted*)fude_zoom_graph_table(&_tables, _w->n, sizeof(fude_zoom_graph_sorted));
        u32 _np = 0;
        for(u32 _i = 0; _i < _w->n; _i++) {
            if(_v[_i].pinned) {
                _off[_np++] = (fude_zoom_graph_sorted){ _v[_i].y - _w->rank_y[_v[_i].rank], 0.0, _i };
                _centre    += _v[_i].x;
            }
        }
        qsort(_off, _np, sizeof(fude_zoom_graph_sorted), fude_zoom_graph_by_keys);
        const f64 _shift = _off[(_np - 1u) / 2u].k1;
        for(u32 _r = 0; _r < _w->ranks; _r++) {
            _w->rank_y[_r] += _shift;
        }
        _centre /= (f64)_np;
        fude_zoom_graph_tables_clear(&_tables);
    }
    for(u32 _i = 0; _i < _nv; _i++) {
        if(!_v[_i].pinned) _v[_i].y = _w->rank_y[_v[_i].rank];
    }
    // Packed, each row centred.
    for(u32 _r = 0; _r < _w->ranks; _r++) {
        const u32 _a = _w->row_first[_r], _b = _w->row_first[_r + 1u];
        if(_a == _b) {
            continue;
        }
        f64 _x = 0.0;
        for(u32 _i = _a; _i < _b; _i++) {
            _x += _i > _a ? fude_zoom_graph_sep(_w, &_v[_w->row[_i - 1u]], &_v[_w->row[_i]]) : 0.0;
            _v[_w->row[_i]].x = _x;
        }
        const fude_zoom_graph_vertex* _first = &_v[_w->row[_a]];
        const fude_zoom_graph_vertex* _last  = &_v[_w->row[_b - 1u]];
        const f64 _mid = 0.5 * (_first->x - _first->w * 0.5 + _last->x + _last->w * 0.5 + _last->loop);
        for(u32 _i = _a; _i < _b; _i++) {
            _v[_w->row[_i]].x += _centre - _mid;
        }
    }
    fude_zoom_graph_scratch _s;
    _s.want   = (f64*)fude_zoom_graph_table(&_tables, _widest, sizeof(f64));
    _s.weight = (f64*)fude_zoom_graph_table(&_tables, _widest, sizeof(f64));
    _s.sep    = (f64*)fude_zoom_graph_table(&_tables, _widest, sizeof(f64));
    _s.x      = (f64*)fude_zoom_graph_table(&_tables, _widest, sizeof(f64));
    _s.off    = (f64*)fude_zoom_graph_table(&_tables, _widest, sizeof(f64));
    _s.bv     = (f64*)fude_zoom_graph_table(&_tables, _widest, sizeof(f64));
    _s.bw     = (f64*)fude_zoom_graph_table(&_tables, _widest, sizeof(f64));
    _s.bstart = (u32*)fude_zoom_graph_table(&_tables, _widest, sizeof(u32));
    _s.near   = (f64*)fude_zoom_graph_table(&_tables, _w->most_degree + 1u, sizeof(f64));
    for(u32 _pass = 0; _pass < FUDE_ZOOM_GRAPH_NUDGES; _pass++) {
        for(u32 _r = 1; _r < _w->ranks; _r++) fude_zoom_graph_nudge(_w, _r, FUDE_ZOOM_GRAPH_ABOVE, false, &_s);
        for(u32 _r = _w->ranks; _r-- > 0;) fude_zoom_graph_nudge(_w, _r, FUDE_ZOOM_GRAPH_BELOW, false, &_s);
    }
    // The rules, with the boxes if they hold together (else without), the
    // boxes made whole, and a few more passes inside them.
    _w->rules = rde_arr_new(sizeof(fude_zoom_graph_rule), rde_memory_allocator_get_default_std());
    _w->wall  = (f64*)fude_zoom_graph_table(&_w->tables, 2u * _w->ns, sizeof(f64));
    const b8 _boxes = _w->ns > 0 && fude_zoom_graph_rules(_w, true);
    if(!_boxes) {
        fude_zoom_graph_rules(_w, false);
    } else {
        fude_zoom_graph_legalise(_w);
    }
    u32* _deepest = (u32*)fude_zoom_graph_table(&_tables, _w->ns, sizeof(u32));
    for(u32 _d = _w->most_depth, _k = 0; _d > 0; _d--) {
        for(u32 _t = 1; _t <= _w->ns; _t++) {
            if(_w->depth[_t] == _d) _deepest[_k++] = _t;
        }
    }
    for(u32 _pass = 0; _pass <= FUDE_ZOOM_GRAPH_NUDGES / 2u; _pass++) {
        const b8 _both = _pass == FUDE_ZOOM_GRAPH_NUDGES / 2u;
        for(u32 _r = _both ? 0u : 1u; _r < _w->ranks; _r++) {
            fude_zoom_graph_nudge(_w, _r, _both ? FUDE_ZOOM_GRAPH_ABOVE | FUDE_ZOOM_GRAPH_BELOW : FUDE_ZOOM_GRAPH_ABOVE, true, &_s);
        }
        fude_zoom_graph_hug(_w, _deepest);
        for(u32 _r = _both ? 0u : _w->ranks; !_both && _r-- > 0;) fude_zoom_graph_nudge(_w, _r, FUDE_ZOOM_GRAPH_BELOW, true, &_s);
        fude_zoom_graph_hug(_w, _deepest);
    }
    fude_zoom_graph_tables_free(&_tables);
    fude_zoom_graph_straighten(_w);
    if(_w->any_pinned) {
        fude_zoom_graph_avoid(_w);
    }
}

// --- routes ------------------------------------------------------------------------------------

// Where the line from _n's centre towards _to leaves its outline: a circle's
// (an ellipse, if it was not given square), a diamond's, else its box.
RDE_INTERNAL fude_zoom_v2 fude_zoom_graph_outline(const fude_zoom_graph_node* _n, fude_zoom_v2 _to) {
    const f64 _dx = _to.x - _n->x, _dy = _to.y - _n->y;
    const f64 _hw = fude_zoom_graph_sane(_n->w) * 0.5, _hh = fude_zoom_graph_sane(_n->h) * 0.5;
    if((_dx == 0.0 && _dy == 0.0) || (_hw <= 0.0 && _hh <= 0.0)) {
        return (fude_zoom_v2){ _n->x, _n->y };
    }
    f64 _t;
    const b8 _round = _n->shape == FUDE_ZOOM_NODE_CIRCLE || _n->shape == FUDE_ZOOM_NODE_DOUBLE_CIRCLE;
    if(_round && _hw > 0.0 && _hh > 0.0) {
        _t = 1.0 / sqrt((_dx / _hw) * (_dx / _hw) + (_dy / _hh) * (_dy / _hh));
    } else if(_n->shape == FUDE_ZOOM_NODE_DIAMOND && _hw > 0.0 && _hh > 0.0) {
        _t = 1.0 / (fabs(_dx) / _hw + fabs(_dy) / _hh);
    } else {
        _t = fmin(_dx != 0.0 ? _hw / fabs(_dx) : 1e300, _dy != 0.0 ? _hh / fabs(_dy) : 1e300);
    }
    return (fude_zoom_v2){ _n->x + _dx * _t, _n->y + _dy * _t };
}

// An edge's end at a subgraph (index + 1; 0: none) as a box-shaped node, if
// the subgraph has a box.
RDE_INTERNAL void fude_zoom_graph_end_box(const fude_zoom_graph* _g, u32 _sub, fude_zoom_graph_node* _end) {
    if(_sub == 0 || _sub > fude_zoom_graph_count(&_g->subgraphs)) {
        return;
    }
    const fude_zoom_graph_subgraph* _s = fude_zoom_graph_subgraph_at(_g, _sub - 1u);
    if(_s->w > 0.0 && _s->h > 0.0) {
        _end->x     = _s->x;
        _end->y     = _s->y;
        _end->w     = _s->w;
        _end->h     = _s->h;
        _end->shape = FUDE_ZOOM_NODE_RECT;
    }
}

RDE_INTERNAL b8 fude_zoom_graph_inside(const fude_zoom_graph_node* _n, fude_zoom_v2 _p) {
    return fabs(_p.x - _n->x) < _n->w * 0.5 && fabs(_p.y - _n->y) < _n->h * 0.5;
}

// Edge _e's line into _points; how many points.
RDE_INTERNAL u32 fude_zoom_graph_route(const fude_zoom_graph* _g, u32 _e, rde_arr* _points) {
    const fude_zoom_graph_edge* _edge = fude_zoom_graph_edge_at(_g, _e);
    const u32 _n = fude_zoom_graph_count(&_g->nodes);
    if(_edge->from >= _n || _edge->to >= _n) {
        return 0;
    }
    fude_zoom_graph_node _a = *fude_zoom_graph_node_at(_g, _edge->from);
    fude_zoom_graph_node _b = *fude_zoom_graph_node_at(_g, _edge->to);
    fude_zoom_graph_end_box(_g, _edge->from_subgraph, &_a);
    fude_zoom_graph_end_box(_g, _edge->to_subgraph, &_b);
    // The bends, less those inside a box the edge ends at.
    const fude_zoom_v2* _bends = (const fude_zoom_v2*)_g->bends.memory + _edge->bend_first;
    u32 _first = 0, _last = (u64)_edge->bend_first + _edge->bend_count <= fude_zoom_graph_count(&_g->bends) ? _edge->bend_count : 0u;
    while(_edge->from_subgraph != 0 && _first < _last && fude_zoom_graph_inside(&_a, _bends[_first])) _first++;
    while(_edge->to_subgraph != 0 && _last > _first && fude_zoom_graph_inside(&_b, _bends[_last - 1u])) _last--;
    const fude_zoom_v2 _start = fude_zoom_graph_outline(&_a, _last > _first ? _bends[_first] : (fude_zoom_v2){ _b.x, _b.y });
    const fude_zoom_v2 _end   = fude_zoom_graph_outline(&_b, _last > _first ? _bends[_last - 1u] : (fude_zoom_v2){ _a.x, _a.y });
    rde_arr_add(_points, (any)&_start);
    for(u32 _k = _first; _k < _last; _k++) {
        rde_arr_add(_points, (any)&_bends[_k]);
    }
    rde_arr_add(_points, (any)&_end);
    return _last - _first + 2u;
}

void fude_zoom_graph_routes(const fude_zoom_graph* _g, rde_arr* _points, rde_arr* _starts) {
    rde_arr_clear(_points);
    rde_arr_clear(_starts);
    const u32 _ne = fude_zoom_graph_count(&_g->edges);
    for(u32 _e = 0; _e <= _ne; _e++) {
        const u32 _at = fude_zoom_graph_count(_points);
        rde_arr_add(_starts, (any)&_at);
        if(_e < _ne) {
            fude_zoom_graph_route(_g, _e, _points);
        }
    }
}

// --- the layout --------------------------------------------------------------------------------

// The bends (turned out of flow space), and where each label goes.
RDE_INTERNAL void fude_zoom_graph_bends(fude_zoom_graph_work* _w) {
    fude_zoom_graph* _g = _w->g;
    const u8 _dir = _g->direction;
    const fude_zoom_graph_vertex* _v = fude_zoom_graph_v(_w);
    const u32* _chain = (const u32*)_w->chain.memory;
    rde_arr_clear(&_g->bends);
    // Edges side by side between the same two nodes, with no bend to keep
    // them apart: each after the first bent aside at its middle, by turns.
    rde_arr _tables = fude_zoom_graph_tables_new();
    fude_zoom_graph_sorted* _pairs = (fude_zoom_graph_sorted*)fude_zoom_graph_table(&_tables, _w->ne, sizeof(fude_zoom_graph_sorted));
    u32* _aside = (u32*)fude_zoom_graph_table(&_tables, _w->ne, sizeof(u32));
    u32  _np = 0;
    for(u32 _e = 0; _e < _w->ne; _e++) {
        if(_w->tail[_e] != FUDE_ZOOM_GRAPH_NONE && _w->tail[_e] != _w->head[_e] && _w->chain_count[_e] <= 2u) {
            _pairs[_np++] = (fude_zoom_graph_sorted){ (f64)_w->tail[_e], (f64)_w->head[_e], _e };
        }
    }
    qsort(_pairs, _np, sizeof(fude_zoom_graph_sorted), fude_zoom_graph_by_keys);
    for(u32 _k = 1; _k < _np; _k++) {
        if(_pairs[_k].k1 == _pairs[_k - 1u].k1 && _pairs[_k].k2 == _pairs[_k - 1u].k2) {
            _aside[_pairs[_k].i] = _aside[_pairs[_k - 1u].i] + 1u;
        }
    }
    for(u32 _e = 0; _e < _w->ne; _e++) {
        fude_zoom_graph_edge* _edge = fude_zoom_graph_edge_at(_g, _e);
        _edge->bend_first = fude_zoom_graph_count(&_g->bends);
        _edge->bend_count = 0;
        const u32 _t = _w->tail[_e];
        if(_t == FUDE_ZOOM_GRAPH_NONE) {
            continue;
        }
        if(_t == _w->head[_e]) {
            // A loop out of its right side and back.
            const fude_zoom_graph_vertex* _x = &_v[_t];
            const f64 _out = _x->x + _x->w * 0.5 + _x->loop;
            const fude_zoom_v2 _two[2] = { fude_zoom_graph_turn(_dir, _out, _x->y - _x->h * 0.25), fude_zoom_graph_turn(_dir, _out, _x->y + _x->h * 0.25) };
            rde_arr_add(&_g->bends, (any)&_two[0]);
            rde_arr_add(&_g->bends, (any)&_two[1]);
            _edge->bend_count = 2u;
            continue;
        }
        const u32 _n = _w->chain_count[_e];
        for(u32 _k = 1; _k + 1u < _n; _k++) {
            const u32 _d = _chain[_w->chain_first[_e] + (_w->reversed[_e] ? _n - 1u - _k : _k)];
            const fude_zoom_v2 _p = fude_zoom_graph_turn(_dir, _v[_d].x, _v[_d].y);
            rde_arr_add(&_g->bends, (any)&_p);
            _edge->bend_count++;
        }
        if(_aside[_e] > 0) {
            const fude_zoom_graph_vertex *_a = &_v[_t], *_b = &_v[_w->head[_e]];
            const f64 _by = (f64)((_aside[_e] + 1u) / 2u) * _w->sp.node_gap * 0.5 * ((_aside[_e] & 1u) ? 1.0 : -1.0);
            const fude_zoom_v2 _p = fude_zoom_graph_turn(_dir, 0.5 * (_a->x + _b->x) + _by, 0.5 * (_a->y + _b->y));
            rde_arr_add(&_g->bends, (any)&_p);
            _edge->bend_count = 1u;
        }
    }
    fude_zoom_graph_tables_free(&_tables);
}

// Each label: on its own vertex if it has one; a loop's beside the loop; else
// halfway along the line as drawn.
RDE_INTERNAL void fude_zoom_graph_labels(fude_zoom_graph_work* _w) {
    fude_zoom_graph* _g = _w->g;
    const fude_zoom_graph_vertex* _v = fude_zoom_graph_v(_w);
    rde_arr _points = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std());
    for(u32 _e = 0; _e < _w->ne; _e++) {
        fude_zoom_graph_edge* _edge = fude_zoom_graph_edge_at(_g, _e);
        if(_w->label_vertex[_e] != FUDE_ZOOM_GRAPH_NONE) {
            const fude_zoom_graph_vertex* _x = &_v[_w->label_vertex[_e]];
            const fude_zoom_v2 _p = fude_zoom_graph_turn(_g->direction, _x->x, _x->y);
            _edge->label_x = _p.x + _w->shift_x;
            _edge->label_y = _p.y + _w->shift_y;
            continue;
        }
        if(_w->tail[_e] != FUDE_ZOOM_GRAPH_NONE && _w->tail[_e] == _w->head[_e]) {
            const fude_zoom_graph_vertex* _x = &_v[_w->tail[_e]];
            const fude_zoom_v2 _p = fude_zoom_graph_turn(_g->direction, _x->x + _x->w * 0.5 + _x->loop, _x->y);
            _edge->label_x = _p.x + _w->shift_x;
            _edge->label_y = _p.y + _w->shift_y;
            continue;
        }
        rde_arr_clear(&_points);
        const u32 _n = fude_zoom_graph_route(_g, _e, &_points);
        const fude_zoom_v2* _p = (const fude_zoom_v2*)_points.memory;
        f64 _length = 0.0;
        for(u32 _k = 1; _k < _n; _k++) {
            _length += hypot(_p[_k].x - _p[_k - 1u].x, _p[_k].y - _p[_k - 1u].y);
        }
        f64 _left = _length * 0.5;
        _edge->label_x = _n > 0 ? _p[0].x : 0.0;
        _edge->label_y = _n > 0 ? _p[0].y : 0.0;
        for(u32 _k = 1; _k < _n; _k++) {
            const f64 _d = hypot(_p[_k].x - _p[_k - 1u].x, _p[_k].y - _p[_k - 1u].y);
            if(_d >= _left && _d > 0.0) {
                _edge->label_x = _p[_k - 1u].x + (_p[_k].x - _p[_k - 1u].x) * _left / _d;
                _edge->label_y = _p[_k - 1u].y + (_p[_k].y - _p[_k - 1u].y) * _left / _d;
                break;
            }
            _left -= _d;
        }
    }
    rde_arr_free(&_points);
}

// Each subgraph's box: round its nodes (pinned ones too) and the boxes inside
// it, padded, then turned.
RDE_INTERNAL void fude_zoom_graph_boxes(fude_zoom_graph_work* _w) {
    const u32 _ns = _w->ns;
    if(_ns == 0) {
        return;
    }
    const fude_zoom_graph_vertex* _v = fude_zoom_graph_v(_w);
    rde_arr _tables = fude_zoom_graph_tables_new();
    fude_zoom_box* _box = (fude_zoom_box*)fude_zoom_graph_table(&_tables, _ns + 1u, sizeof(fude_zoom_box));
    for(u32 _s = 0; _s <= _ns; _s++) {
        _box[_s] = (fude_zoom_box){ 1e300, 1e300, -1e300, -1e300 };
    }
    for(u32 _i = 0; _i < _w->n; _i++) {
        const u32 _s = _v[_i].sub;
        if(_s != 0) {
            _box[_s].min_x = fmin(_box[_s].min_x, _v[_i].x - _v[_i].w * 0.5);
            _box[_s].max_x = fmax(_box[_s].max_x, _v[_i].x + _v[_i].w * 0.5 + _v[_i].loop);
            _box[_s].min_y = fmin(_box[_s].min_y, _v[_i].y - _v[_i].h * 0.5);
            _box[_s].max_y = fmax(_box[_s].max_y, _v[_i].y + _v[_i].h * 0.5);
        }
    }
    for(u32 _d = _w->most_depth; _d > 0; _d--) {
        for(u32 _s = 1; _s <= _ns; _s++) {
            fude_zoom_graph_subgraph* _sg = fude_zoom_graph_subgraph_at(_w->g, _s - 1u);
            if(_w->depth[_s] != _d) {
                continue;
            }
            if(_box[_s].min_x > _box[_s].max_x) {
                _sg->x = _sg->y = _sg->w = _sg->h = 0.0;
                continue;
            }
            _box[_s].min_x -= _w->pad[0];
            _box[_s].max_x += _w->pad[1];
            _box[_s].min_y -= _w->pad[2];
            _box[_s].max_y += _w->pad[3];
            const u32 _p = _w->parent[_s];
            if(_p != 0) {
                _box[_p].min_x = fmin(_box[_p].min_x, _box[_s].min_x);
                _box[_p].max_x = fmax(_box[_p].max_x, _box[_s].max_x);
                _box[_p].min_y = fmin(_box[_p].min_y, _box[_s].min_y);
                _box[_p].max_y = fmax(_box[_p].max_y, _box[_s].max_y);
            }
            const fude_zoom_v2 _a = fude_zoom_graph_turn(_w->g->direction, _box[_s].min_x, _box[_s].min_y);
            const fude_zoom_v2 _b = fude_zoom_graph_turn(_w->g->direction, _box[_s].max_x, _box[_s].max_y);
            _sg->x = 0.5 * (_a.x + _b.x);
            _sg->y = 0.5 * (_a.y + _b.y);
            _sg->w = fabs(_b.x - _a.x);
            _sg->h = fabs(_b.y - _a.y);
        }
    }
    fude_zoom_graph_tables_free(&_tables);
}

RDE_INTERNAL void fude_zoom_graph_work_free(fude_zoom_graph_work* _w) {
    fude_zoom_graph_tables_free(&_w->tables);
    fude_zoom_graph_tables_free(&_w->rule_tables);
    if(rde_arr_is_inited(&_w->verts)) rde_arr_free(&_w->verts);
    if(rde_arr_is_inited(&_w->chain)) rde_arr_free(&_w->chain);
    if(rde_arr_is_inited(&_w->rules)) rde_arr_free(&_w->rules);
}

void fude_zoom_graph_layout(fude_zoom_graph* _g, fude_zoom_graph_spacing _sp) {
    fude_zoom_graph_work _w;
    memset(&_w, 0, sizeof(_w));
    _w.g  = _g;
    _w.sp = (fude_zoom_graph_spacing){ fude_zoom_graph_sane(_sp.rank_gap), fude_zoom_graph_sane(_sp.node_gap),
                                       fude_zoom_graph_sane(_sp.subgraph_pad), fude_zoom_graph_sane(_sp.subgraph_title) };
    _w.n  = fude_zoom_graph_count(&_g->nodes);
    _w.ne = fude_zoom_graph_count(&_g->edges);
    _w.ns = fude_zoom_graph_count(&_g->subgraphs);
    const u8 _dir  = _g->direction <= FUDE_ZOOM_FLOW_RL ? _g->direction : FUDE_ZOOM_FLOW_TB;
    _g->direction  = _dir;
    const b8 _side = fude_zoom_graph_sideways(_dir);
    for(u32 _s = 0; _s < _w.ns; _s++) {
        fude_zoom_graph_subgraph* _sg = fude_zoom_graph_subgraph_at(_g, _s);
        _sg->x = _sg->y = _sg->w = _sg->h = 0.0;
    }
    if(_w.n == 0) {
        rde_arr_clear(&_g->bends);
        for(u32 _e = 0; _e < _w.ne; _e++) {
            fude_zoom_graph_edge_at(_g, _e)->bend_count = 0;
        }
        return;
    }
    // The box's padding in flow space: the title goes on the page's top side.
    for(u32 _k = 0; _k < 4u; _k++) {
        _w.pad[_k] = _w.sp.subgraph_pad;
    }
    _w.pad[_dir == FUDE_ZOOM_FLOW_TB ? 2 : (_dir == FUDE_ZOOM_FLOW_BT ? 3 : 0)] += _w.sp.subgraph_title;
    _w.tables      = fude_zoom_graph_tables_new();
    _w.rule_tables = fude_zoom_graph_tables_new();
    fude_zoom_graph_tree(&_w);
    _w.sib = (u32*)fude_zoom_graph_table(&_w.tables, _w.ns + 1u, sizeof(u32));
    // The nodes as vertices (first, at their own index), where they were.
    _w.verts = rde_arr_new(sizeof(fude_zoom_graph_vertex), rde_memory_allocator_get_default_std());
    fude_zoom_v2* _before = (fude_zoom_v2*)fude_zoom_graph_table(&_w.tables, _w.n, sizeof(fude_zoom_v2));   // (freed with the work)
    for(u32 _i = 0; _i < _w.n; _i++) {
        const fude_zoom_graph_node* _node = fude_zoom_graph_node_at(_g, _i);
        const f64 _nw = fude_zoom_graph_sane(_node->w), _nh = fude_zoom_graph_sane(_node->h);
        const u32 _v  = fude_zoom_graph_add_vertex(&_w, FUDE_ZOOM_GRAPH_VERTEX_NODE, _node->subgraph <= _w.ns ? _node->subgraph : 0u, 0, _side ? _nh : _nw, _side ? _nw : _nh);
        fude_zoom_graph_vertex* _x = &fude_zoom_graph_v(&_w)[_v];
        const b8 _where = isfinite(_node->x) && isfinite(_node->y);
        const fude_zoom_v2 _f = fude_zoom_graph_unturn(_dir, _node->x, _node->y);
        _x->node   = _i;
        _x->pinned = _node->pinned && _where;
        _x->fixed  = (_node->placed || _node->pinned) && _where;
        _x->prev   = _f.x;
        _before[_i] = (fude_zoom_v2){ _node->x, _node->y };
        if(_x->pinned) {
            _x->x = _f.x;
            _x->y = _f.y;
            _w.any_pinned = true;
        }
    }
    // The edges: ends, lengths, and whether labels keep ranks of their own.
    _w.tail     = (u32*)fude_zoom_graph_table(&_w.tables, _w.ne, sizeof(u32));
    _w.head     = (u32*)fude_zoom_graph_table(&_w.tables, _w.ne, sizeof(u32));
    _w.minlen   = (u32*)fude_zoom_graph_table(&_w.tables, _w.ne, sizeof(u32));
    _w.reversed = (b8*)fude_zoom_graph_table(&_w.tables, _w.ne, sizeof(b8));
    for(u32 _e = 0; _e < _w.ne; _e++) {
        const fude_zoom_graph_edge* _edge = fude_zoom_graph_edge_at(_g, _e);
        _w.labels = _w.labels || (_edge->from < _w.n && _edge->to < _w.n && _edge->from != _edge->to &&
                                  (fude_zoom_graph_sane(_edge->label_w) > 0.0 || fude_zoom_graph_sane(_edge->label_h) > 0.0));
    }
    for(u32 _e = 0; _e < _w.ne; _e++) {
        const fude_zoom_graph_edge* _edge = fude_zoom_graph_edge_at(_g, _e);
        const b8 _ok  = _edge->from < _w.n && _edge->to < _w.n;
        const u32 _len = _edge->min_length == 0 ? 1u : (_edge->min_length < FUDE_ZOOM_GRAPH_MAX_LENGTH ? _edge->min_length : FUDE_ZOOM_GRAPH_MAX_LENGTH);
        _w.tail[_e]   = _ok ? _edge->from : FUDE_ZOOM_GRAPH_NONE;
        _w.head[_e]   = _ok ? _edge->to : FUDE_ZOOM_GRAPH_NONE;
        _w.minlen[_e] = _len * (_w.labels ? 2u : 1u);
        if(_ok && _edge->from == _edge->to) {
            fude_zoom_graph_vertex* _x = &fude_zoom_graph_v(&_w)[_edge->from];
            _x->loop = fmax(_w.sp.node_gap * 0.5, _x->h * 0.25);
        }
    }
    fude_zoom_graph_break_cycles(&_w);
    fude_zoom_graph_rank(&_w);
    fude_zoom_graph_chains(&_w);
    fude_zoom_graph_fillers(&_w);
    _w.nv = fude_zoom_graph_count(&_w.verts);
    fude_zoom_graph_neighbours(&_w);
    fude_zoom_graph_order(&_w);
    fude_zoom_graph_place(&_w);
    // Out of flow space.
    const fude_zoom_graph_vertex* _v = fude_zoom_graph_v(&_w);
    for(u32 _i = 0; _i < _w.n; _i++) {
        if(!_v[_i].pinned) {
            fude_zoom_graph_node* _node = fude_zoom_graph_node_at(_g, _i);
            const fude_zoom_v2 _p = fude_zoom_graph_turn(_dir, _v[_i].x, _v[_i].y);
            _node->x = _p.x;
            _node->y = _p.y;
        }
    }
    fude_zoom_graph_bends(&_w);
    fude_zoom_graph_boxes(&_w);
    // Where it all goes: pinned, it stays; else where the placed nodes were,
    // on average; else with its corner at 0, 0.
    if(!_w.any_pinned) {
        f64 _dx = 0.0, _dy = 0.0;
        u32 _placed = 0;
        for(u32 _i = 0; _i < _w.n; _i++) {
            if(_v[_i].fixed) {
                const fude_zoom_graph_node* _node = fude_zoom_graph_node_at(_g, _i);
                _dx += _before[_i].x - _node->x;
                _dy += _before[_i].y - _node->y;
                _placed++;
            }
        }
        if(_placed > 0) {
            _dx /= (f64)_placed;
            _dy /= (f64)_placed;
        } else {
            f64 _lx = 1e300, _ly = 1e300;
            for(u32 _i = 0; _i < _w.n; _i++) {
                const fude_zoom_graph_node* _node = fude_zoom_graph_node_at(_g, _i);
                _lx = fmin(_lx, _node->x - fude_zoom_graph_sane(_node->w) * 0.5);
                _ly = fmin(_ly, _node->y - fude_zoom_graph_sane(_node->h) * 0.5);
            }
            for(u32 _s = 0; _s < _w.ns; _s++) {
                const fude_zoom_graph_subgraph* _sg = fude_zoom_graph_subgraph_at(_g, _s);
                if(_sg->w > 0.0 || _sg->h > 0.0) {
                    _lx = fmin(_lx, _sg->x - _sg->w * 0.5);
                    _ly = fmin(_ly, _sg->y - _sg->h * 0.5);
                }
            }
            _dx = -_lx;
            _dy = -_ly;
        }
        _w.shift_x = _dx;
        _w.shift_y = _dy;
        for(u32 _i = 0; _i < _w.n; _i++) {
            fude_zoom_graph_node_at(_g, _i)->x += _dx;
            fude_zoom_graph_node_at(_g, _i)->y += _dy;
        }
        fude_zoom_v2* _bends = (fude_zoom_v2*)_g->bends.memory;
        for(u32 _k = 0; _k < fude_zoom_graph_count(&_g->bends); _k++) {
            _bends[_k].x += _dx;
            _bends[_k].y += _dy;
        }
        for(u32 _s = 0; _s < _w.ns; _s++) {
            fude_zoom_graph_subgraph* _sg = fude_zoom_graph_subgraph_at(_g, _s);
            if(_sg->w > 0.0 || _sg->h > 0.0) {
                _sg->x += _dx;
                _sg->y += _dy;
            }
        }
    }
    fude_zoom_graph_labels(&_w);
    for(u32 _i = 0; _i < _w.n; _i++) {
        fude_zoom_graph_node_at(_g, _i)->placed = true;
    }
    fude_zoom_graph_work_free(&_w);
}
