// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/graph_parts.h"

// ===========================================================================
// See graph.h, which has what is read. Graphs, Mermaid read: the text helpers (whole UTF-8 characters into a fixed
// buffer; <br> and entity codes in a label), a flowchart read a statement at a time by its reader
// (fude_zoom_graph_reader: its header, nodes and their shapes, links and their text, chains, subgraphs, and what is
// read past), the edges to a subgraph's id joined to its box, and a mind map's reader (a node a line, its parent the
// nearest one less indented).
//
// The file goes: the text helpers; the flowchart's reader, from its smallest pieces up to a statement; the mind map's;
// then fude_zoom_graph_parse_mermaid, which picks one.
//
// Part of the graph: graph_parts.h holds what the modules share and says which module holds what. Internal names start
// fude_zoom_graph_ as the public ones do. It keeps no state of its own: any thread may use a graph no other thread is
// using.
// ===========================================================================

// --- text ------------------------------------------------------------------------------------

// Returns whether _c is a space within a line: a space, a tab or a carriage return.
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
    u32 cap, n;    // the buffer's size, its NUL included, and the bytes in it
    b8  full;
} fude_zoom_graph_out;

// Appends _n bytes of _s a whole UTF-8 character at a time, keeping the text NUL-ended; a character that does not fit
// ends it.
RDE_INTERNAL void fude_zoom_graph_put(fude_zoom_graph_out* _o, const c8* _s, u32 _n) {
    u32 _i = 0;
    while(_i < _n && !_o->full) {
        u32 _k = fude_zoom_graph_seq(_s[_i]);
        _k = _i + _k <= _n ? _k : _n - _i;
        if(_o->n + _k + 1u > _o->cap) {
            _o->full = true;
            break;
        }
        rde_memcpy(_o->s + _o->n, _s + _i, _k);
        _o->n += _k;
        _i    += _k;
    }
    _o->s[_o->n] = 0;
}

// Copies _n bytes of _s into _dst (_cap bytes), cut between whole characters when they do not fit.
void fude_zoom_graph_copy(c8* _dst, u32 _cap, const c8* _s, u32 _n) {
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
    if(_p + 1 < _b && (*_p == 'x' || *_p == 'X') && rde_char_is_xdigit((u8)_p[1])) {
        for(_p++; _p < _b && rde_char_is_xdigit((u8)*_p) && _digits < 6u; _p++, _digits++) {
            const c8 _c = *_p;
            _v = _v * 16u + (u32)(_c <= '9' ? _c - '0' : (_c | 0x20) - 'a' + 10);
        }
    } else if(_p < _b && rde_char_is_digit((u8)*_p)) {
        for(; _p < _b && rde_char_is_digit((u8)*_p) && _digits < 7u; _p++, _digits++) {
            _v = _v * 10u + (u32)(*_p - '0');
        }
    } else {
        const c8* _name = _p;
        while(_p < _b && rde_char_is_alpha((u8)*_p) && _p - _name < 8) _p++;
        if(_p >= _b || *_p != ';') {
            return 0;
        }
        for(u32 _i = 0; _i < sizeof(FUDE_ZOOM_GRAPH_NAMED) / sizeof(FUDE_ZOOM_GRAPH_NAMED[0]); _i++) {
            const usize _n = rde_strlen(FUDE_ZOOM_GRAPH_NAMED[_i].name);
            if((usize)(_p - _name) == _n && rde_memcmp(_name, FUDE_ZOOM_GRAPH_NAMED[_i].name, _n) == 0) {
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
            fude_zoom_graph_put(&_o, _u, (u32)rde_strlen(_u));
            _a += _len;
            continue;
        }
        u32 _k = fude_zoom_graph_seq(*_a);
        _k = _a + _k <= _b ? _k : (u32)(_b - _a);
        fude_zoom_graph_put(&_o, _a, _k);
        _a += _k;
    }
}

// --- reading Mermaid -------------------------------------------------------------------------

// The reader's state while it goes through one text.
typedef struct {
    fude_zoom_graph* g;
    const c8*        p;                                      // the byte read next
    const c8*        end;
    u32              line;                                   // p's, from 1
    u32              open[FUDE_ZOOM_GRAPH_MAX_DEPTH];        // the subgraphs open (index + 1), innermost last
    u32              open_line[FUDE_ZOOM_GRAPH_MAX_DEPTH];   // the line each was opened on, for an error
    u32              depth;                                  // how many are open
    rde_hash_map     ids;                                    // a node's id (inline, FUDE_ZOOM_GRAPH_ID) → its index
    rde_arr          left, right;                            // a chain's two groups at a time (u32 nodes)
    u8               error;                                  // the first error met (FUDE_ZOOM_MERMAID_)
    u32              error_line;
} fude_zoom_graph_reader;

// A link as read: its line, heads, length in ranks and text, given to every edge it makes.
typedef struct {
    u8  line, head_start, head_end;
    u32 length;
    c8  label[FUDE_ZOOM_GRAPH_EDGE_LABEL];
} fude_zoom_graph_link;

// Records error _why at the reader's line, unless an error was met already, and returns false, so a reader can return
// it straight away.
RDE_INTERNAL b8 fude_zoom_graph_fail(fude_zoom_graph_reader* _r, u8 _why) {
    if(_r->error == FUDE_ZOOM_MERMAID_OK) {
        _r->error      = _why;
        _r->error_line = _r->line;
    }
    return false;
}

// Returns whether byte _c can begin an id: a letter, a digit, '_', or any byte of a non-ASCII character.
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

// Returns whether the text at _p (up to _end) begins with _s.
RDE_INTERNAL b8 fude_zoom_graph_starts(const c8* _p, const c8* _end, const c8* _s) {
    const usize _n = rde_strlen(_s);
    return (usize)(_end - _p) >= _n && rde_memcmp(_p, _s, _n) == 0;
}

// Whether the statement starts with the word _w, not as part of a longer id.
RDE_INTERNAL b8 fude_zoom_graph_word(const fude_zoom_graph_reader* _r, const c8* _w) {
    return fude_zoom_graph_starts(_r->p, _r->end, _w) && !fude_zoom_graph_id_at(_r->p + rde_strlen(_w), _r->end);
}

// The same, with a space after it (classDef, style, a subgraph's direction).
RDE_INTERNAL b8 fude_zoom_graph_keyword(const fude_zoom_graph_reader* _r, const c8* _w) {
    const c8* _after = _r->p + rde_strlen(_w);
    return fude_zoom_graph_word(_r, _w) && _after < _r->end && (*_after == ' ' || *_after == '\t');
}

// Moves past spaces within the line.
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

// Moves to the line's end (its '\n' left to be counted).
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

// Reads the text's start: front matter, then "flowchart", "flowchart-elk" or "graph", and its direction if it gives
// one (TB, TD, BT, LR, RL in any case, or > < ^ v). False, with the error said, when it is not a flowchart's.
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
        _d[_i] = (c8)rde_char_to_upper((u8)_a[_i]);
    }
    if(_n == 2u && (rde_strcmp(_d, "TB") == 0 || rde_strcmp(_d, "TD") == 0)) {
        _r->g->direction = FUDE_ZOOM_FLOW_TB;
    } else if(_n == 2u && rde_strcmp(_d, "BT") == 0) {
        _r->g->direction = FUDE_ZOOM_FLOW_BT;
    } else if(_n == 2u && rde_strcmp(_d, "LR") == 0) {
        _r->g->direction = FUDE_ZOOM_FLOW_LR;
    } else if(_n == 2u && rde_strcmp(_d, "RL") == 0) {
        _r->g->direction = FUDE_ZOOM_FLOW_RL;
    } else if(_n == 1u && (*_a == '>' || *_a == '<' || *_a == '^' || *_a == 'v')) {
        _r->g->direction = *_a == '>' ? FUDE_ZOOM_FLOW_LR : (*_a == '<' ? FUDE_ZOOM_FLOW_RL : (*_a == '^' ? FUDE_ZOOM_FLOW_BT : FUDE_ZOOM_FLOW_TB));
    } else {
        return fude_zoom_graph_fail(_r, FUDE_ZOOM_MERMAID_DIRECTION);
    }
    return fude_zoom_graph_at_end(_r) ? true : fude_zoom_graph_fail(_r, FUDE_ZOOM_MERMAID_DIRECTION);
}

// Returns the node named _id (_n bytes), made if it is new: a box, its id its text. NONE (an error said) when the id is
// too long or the graph is full.
RDE_INTERNAL u32 fude_zoom_graph_intern(fude_zoom_graph_reader* _r, const c8* _id, u32 _n) {
    if(_n >= FUDE_ZOOM_GRAPH_ID) {
        fude_zoom_graph_fail(_r, FUDE_ZOOM_MERMAID_ID);   // (fude_zoom_graph_read_node refuses these first)
        return FUDE_ZOOM_GRAPH_NONE;
    }
    c8 _key[FUDE_ZOOM_GRAPH_ID];
    rde_memcpy(_key, _id, _n);
    _key[_n] = 0;
    const u32* _known = (const u32*)rde_hash_map_find(&_r->ids, _key);
    if(_known != NULL) {
        return *_known;
    }
    const u32 _count = fude_zoom_graph_count(&_r->g->nodes);
    if(_count >= FUDE_ZOOM_GRAPH_MAX_NODES) {
        fude_zoom_graph_fail(_r, FUDE_ZOOM_MERMAID_TOO_BIG);
        return FUDE_ZOOM_GRAPH_NONE;
    }
    fude_zoom_graph_append_node(_r->g, _id, _n, _id, _n, FUDE_ZOOM_NODE_RECT);   // (new: the map has it not)
    rde_hash_map_add(&_r->ids, _key, (any)&_count);
    return _count;
}

// Returns whether subgraph _inner (index + 1) is _outer or inside it; everything is inside 0, the top.
RDE_INTERNAL b8 fude_zoom_graph_within(const fude_zoom_graph* _g, u32 _inner, u32 _outer) {
    for(u32 _steps = 0; _inner != 0 && _steps <= FUDE_ZOOM_GRAPH_MAX_SUBGRAPHS; _steps++) {
        if(_inner == _outer) {
            return true;
        }
        _inner = fude_zoom_graph_subgraph_at(_g, _inner - 1u)->parent;
    }
    return _outer == 0;
}

// Finds the text from _r->p to the closer _close (or _close2, if not NULL; which into *_which) into [*_a, *_b), _r->p
// left past it. Quoted, it may hold the closer and run over lines; else it ends at the first closer on the line. False
// (an error said) when it does not close.
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
            _r->p   = _p + rde_strlen(_close);
            return true;
        }
        if(_close2 != NULL && fude_zoom_graph_starts(_p, _r->end, _close2)) {
            *_which = 1u;
            _r->p   = _p + rde_strlen(_close2);
            return true;
        }
        return fude_zoom_graph_fail(_r, FUDE_ZOOM_MERMAID_SHAPE);
    }
    for(const c8* _q = _r->p; _q < _r->end && *_q != '\n'; _q++) {
        if(fude_zoom_graph_starts(_q, _r->end, _close) || (_close2 != NULL && fude_zoom_graph_starts(_q, _r->end, _close2))) {
            *_which = fude_zoom_graph_starts(_q, _r->end, _close) ? 0u : 1u;
            *_a     = _r->p;
            *_b     = _q;
            _r->p   = _q + rde_strlen(*_which == 0 ? _close : _close2);
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

// The brackets in the order they are tried: each longer one before a shorter one it starts with.
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

// Reads a node as written (an id, maybe a shape and its text) and returns its index, or NONE (an error said).
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
            _r->p += rde_strlen(_br->open);
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
    // Into the innermost subgraph open, unless the one it is in already lies inside that one, or apart from it: a node
    // goes to the deepest subgraph naming it, and of two apart, the first (as Mermaid has it).
    if(_r->depth > 0) {
        const u32 _s = _r->open[_r->depth - 1u];
        if(_node->subgraph == 0 || (_node->subgraph != _s && fude_zoom_graph_within(_r->g, _s, _node->subgraph))) {
            _node->subgraph = _s;
        }
    }
    return _i;
}

// Returns the head after a link's line at _p: > an arrow, or an o or x that does not begin a word; else none.
RDE_INTERNAL u8 fude_zoom_graph_head(const c8* _p, const c8* _end) {
    if(_p < _end && *_p == '>') {
        return FUDE_ZOOM_EDGE_HEAD_ARROW;
    }
    if(_p < _end && (*_p == 'o' || *_p == 'x') && !fude_zoom_graph_id_at(_p + 1, _end)) {
        return *_p == 'o' ? FUDE_ZOOM_EDGE_HEAD_CIRCLE : FUDE_ZOOM_EDGE_HEAD_CROSS;
    }
    return FUDE_ZOOM_EDGE_HEAD_NONE;
}

// Reads a link's line at *_pp. Returns 1 for a whole one (its line, end head and length into _l, *_pp past it), 2 for
// the opening of one with text (--, ==, -.), 0 for none.
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

// Reads a link into _l: its heads, line and length, and its text (-- a -->, |a|). False (an error said) when it is
// not one, or its text is not closed.
RDE_INTERNAL b8 fude_zoom_graph_read_link(fude_zoom_graph_reader* _r, fude_zoom_graph_link* _l) {
    rde_memset(_l, 0, sizeof(*_l));
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
        // The text runs to the first link of the same line that closes it, on this line (a quoted text may hold
        // anything).
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

// Reads nodes joined by & into _into (cleared first). False (an error said) at a node that cannot be read.
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

// Reads a chain: groups joined by links, an edge for every pair across each link. A node alone is a chain too.
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
                rde_memset(&_e, 0, sizeof(_e));
                _e.from       = ((const u32*)_r->left.memory)[_i];
                _e.to         = ((const u32*)_r->right.memory)[_j];
                _e.line       = _l.line;
                _e.head_start = _l.head_start;
                _e.head_end   = _l.head_end;
                _e.min_length = _l.length < FUDE_ZOOM_GRAPH_MAX_LENGTH ? _l.length : FUDE_ZOOM_GRAPH_MAX_LENGTH;
                rde_memcpy(_e.label, _l.label, sizeof(_e.label));
                rde_arr_add(&_r->g->edges, (any)&_e);
            }
        }
        const rde_arr _t = _r->left;
        _r->left  = _r->right;
        _r->right = _t;
    }
    return true;
}

// Opens a subgraph: subgraph id [title] | subgraph id | subgraph "title" | subgraph a title. One with no id of its own
// is named subGraphN. False (an error said) when too deep, too many, or its title does not close.
RDE_INTERNAL b8 fude_zoom_graph_open(fude_zoom_graph_reader* _r) {
    const u32 _count = fude_zoom_graph_count(&_r->g->subgraphs);
    if(_r->depth >= FUDE_ZOOM_GRAPH_MAX_DEPTH) {
        return fude_zoom_graph_fail(_r, FUDE_ZOOM_MERMAID_DEPTH);
    }
    if(_count >= FUDE_ZOOM_GRAPH_MAX_SUBGRAPHS) {
        return fude_zoom_graph_fail(_r, FUDE_ZOOM_MERMAID_TOO_BIG);
    }
    fude_zoom_graph_subgraph _s;
    rde_memset(&_s, 0, sizeof(_s));
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
                rde_memcpy(_s.id, _start, _n);
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
        rde_snprintf(_s.id, sizeof(_s.id), "subGraph%u", _count);
    }
    fude_zoom_graph_skip_statement(_r);
    rde_arr_add(&_r->g->subgraphs, (any)&_s);
    _r->open[_r->depth]      = _count + 1u;
    _r->open_line[_r->depth] = _line;
    _r->depth++;
    return true;
}

// Edges to and from a subgraph's id join its box, as in Mermaid: the node the id made goes, and the edge keeps one of
// the subgraph's nodes to stand in for the box in the layout (its last for an edge out of it, its first for one into
// it, as they are usually written top to bottom). A subgraph with no node of its own to stand in leaves the node be.
// The nodes kept keep their order, and the edges are renumbered to them.
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
            _names[_i] = rde_strcmp(fude_zoom_graph_node_at(_g, _i)->id, fude_zoom_graph_subgraph_at(_g, _s)->id) == 0 ? _s + 1u : 0u;
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

// Reads one statement: a subgraph opened or closed, a styling or accessibility statement passed over, or a chain.
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
    // Styling and interaction the canvas has its own ways for, and a subgraph's own direction (it flows as the whole
    // graph does).
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

// --- a mind map (Mermaid's mindmap) ---------------------------------------------------------------

// A mind map's line's node: its shape and its text (between its shape's brackets, or the whole line). False: a bracket
// not closed.
RDE_INTERNAL b8 fude_zoom_graph_mind_node(const c8* _a, const c8* _b, u8* _shape, c8* _label, u32 _cap) {
    static const struct { const c8* open; const c8* close; u8 shape; } _shapes[] = {
        { "((", "))", FUDE_ZOOM_NODE_CIRCLE }, { "))", "((", FUDE_ZOOM_NODE_HEXAGON },   // (a bang: Mermaid's own; a hexagon here)
        { "{{", "}}", FUDE_ZOOM_NODE_HEXAGON }, { ")", "(", FUDE_ZOOM_NODE_ROUND },       // (a cloud)
        { "(", ")", FUDE_ZOOM_NODE_ROUND }, { "[", "]", FUDE_ZOOM_NODE_RECT },
    };
    // (its id: up to the first bracket, no space in it)
    const c8* _o = _a;
    while(_o < _b && *_o != '[' && *_o != '(' && *_o != ')' && *_o != '{' && !fude_zoom_graph_space(*_o)) {
        _o++;
    }
    for(u32 _k = 0; _o < _b && *_o != ' ' && _k < sizeof(_shapes) / sizeof(_shapes[0]); _k++) {
        const u32 _on = (u32)rde_strlen(_shapes[_k].open), _cn = (u32)rde_strlen(_shapes[_k].close);
        if((u32)(_b - _o) < _on + _cn || rde_memcmp(_o, _shapes[_k].open, _on) != 0) {
            continue;
        }
        if(rde_memcmp(_b - _cn, _shapes[_k].close, _cn) != 0) {
            return false;
        }
        *_shape = _shapes[_k].shape;
        fude_zoom_graph_text(_label, _cap, _o + _on, _b - _cn);
        return true;
    }
    *_shape = FUDE_ZOOM_NODE_ROUND;   // (text alone: Mermaid's own default, a rounded box here)
    fude_zoom_graph_text(_label, _cap, _a, _b);
    return true;
}

// Reads a mind map (see fude_zoom_graph_parse_mermaid in the header) a line at a time: blank lines and %% comments
// passed over, "mindmap" first, then a node a line, its parent the nearest open one less indented (a tab counts as 4
// spaces). Its nodes are named mind0, mind1 and so on.
RDE_INTERNAL b8 fude_zoom_graph_parse_mindmap(fude_zoom_graph* _g, const c8* _text, u32 _size, u32* _error_line, u8* _error) {
    fude_zoom_graph_clear(_g);
    u32 _parents[FUDE_ZOOM_GRAPH_MIND_DEPTH], _indents[FUDE_ZOOM_GRAPH_MIND_DEPTH];
    u32 _depth = 0, _line = 0, _count = 0;
    b8  _header = false, _ok = true;
    u8  _why = FUDE_ZOOM_MERMAID_OK;
    const c8* _p = _text != NULL ? _text : "";
    const c8* _end = _p + (_text != NULL ? _size : 0u);
    while(_ok && _p < _end) {
        const c8* _e = _p;
        while(_e < _end && *_e != '\n') {
            _e++;
        }
        _line++;
        u32 _indent = 0;
        const c8* _a = _p;
        while(_a < _e && (*_a == ' ' || *_a == '\t')) {
            _indent += *_a == '\t' ? 4u : 1u;
            _a++;
        }
        const c8* _b = _e;
        while(_b > _a && (fude_zoom_graph_space(_b[-1]) || _b[-1] == '\r')) {
            _b--;
        }
        _p = _e < _end ? _e + 1 : _e;
        if(_b == _a || (_b - _a >= 2 && _a[0] == '%' && _a[1] == '%')) {
            continue;   // (empty; a comment)
        }
        if(!_header) {
            _header = _b - _a == 7 && rde_memcmp(_a, "mindmap", 7u) == 0;
            _ok     = _header;
            _why    = _header ? FUDE_ZOOM_MERMAID_OK : FUDE_ZOOM_MERMAID_HEADER;
            continue;
        }
        if((_b - _a >= 2 && _a[0] == ':' && _a[1] == ':') || (_b - _a >= 3 && rde_memcmp(_a, ":::", 3u) == 0)) {
            continue;   // (an icon, a class: how it looks in Mermaid)
        }
        // Its parent: the nearest above it less indented.
        while(_depth > 0u && _indents[_depth - 1u] >= _indent) {
            _depth--;
        }
        if(_depth == 0u && _count > 0u) {
            _ok  = false;
            _why = FUDE_ZOOM_MERMAID_NODE;   // (a second root)
            break;
        }
        if(_depth >= FUDE_ZOOM_GRAPH_MIND_DEPTH) {
            _ok  = false;
            _why = FUDE_ZOOM_MERMAID_DEPTH;   // (too deep: said before the node is added, as a subgraph too deep is)
            break;
        }
        u8 _shape = FUDE_ZOOM_NODE_ROUND;
        c8 _label[FUDE_ZOOM_GRAPH_LABEL];
        if(!fude_zoom_graph_mind_node(_a, _b, &_shape, _label, sizeof(_label))) {
            _ok  = false;
            _why = FUDE_ZOOM_MERMAID_SHAPE;
            break;
        }
        // (its id new by construction, so appended with no search: a big mind map is read in linear time)
        c8 _id[FUDE_ZOOM_GRAPH_ID];
        const u32 _idn   = (u32)rde_snprintf(_id, sizeof(_id), "mind%u", _count);
        const c8* _shown = _label[0] != 0 ? _label : " ";
        const u32 _n     = fude_zoom_graph_append_node(_g, _id, _idn, _shown, (u32)rde_strlen(_shown), _shape);
        if(_n == FUDE_ZOOM_GRAPH_NONE) {
            _ok  = false;
            _why = FUDE_ZOOM_MERMAID_TOO_BIG;
            break;
        }
        if(_depth > 0u) {
            const u32 _e2 = fude_zoom_graph_add_edge(_g, _parents[_depth - 1u], _n);
            if(_e2 == FUDE_ZOOM_GRAPH_NONE) {
                _ok  = false;
                _why = FUDE_ZOOM_MERMAID_TOO_BIG;
                break;
            }
            fude_zoom_graph_edge_at(_g, _e2)->head_end = FUDE_ZOOM_EDGE_HEAD_NONE;   // (a branch, not an arrow)
        }
        _parents[_depth] = _n;
        _indents[_depth] = _indent;
        _depth++;
        _count++;
    }
    if(_ok && (!_header || _count == 0u)) {
        _ok  = false;
        _why = _header ? FUDE_ZOOM_MERMAID_NODE : FUDE_ZOOM_MERMAID_HEADER;
    }
    if(!_ok) {
        fude_zoom_graph_clear(_g);
    } else {
        _g->direction = FUDE_ZOOM_FLOW_MIND;
    }
    if(_error_line != NULL) {
        *_error_line = _ok ? 0u : _line;
    }
    if(_error != NULL) {
        *_error = _why;
    }
    return _ok;
}

// Returns whether the text's first line (past blank lines and %% comments) is "mindmap".
RDE_INTERNAL b8 fude_zoom_graph_is_mindmap(const c8* _text, u32 _size) {
    const c8* _p = _text != NULL ? _text : "";
    const c8* _end = _p + (_text != NULL ? _size : 0u);
    while(_p < _end) {
        while(_p < _end && (fude_zoom_graph_space(*_p) || *_p == '\n' || *_p == '\r')) {
            _p++;
        }
        if(_end - _p >= 2 && _p[0] == '%' && _p[1] == '%') {
            while(_p < _end && *_p != '\n') {
                _p++;
            }
            continue;
        }
        return _end - _p >= 7 && rde_memcmp(_p, "mindmap", 7u) == 0 && (_end - _p == 7 || fude_zoom_graph_space(_p[7]) || _p[7] == '\n' || _p[7] == '\r');
    }
    return false;
}

// See the header. A mind map goes to its own reader; a flowchart is read statement by statement, a subgraph left open
// at the end being an error, and its subgraphs' ids joined to their boxes at the end.
b8 fude_zoom_graph_parse_mermaid(fude_zoom_graph* _g, const c8* _text, u32 _size, u32* _error_line, u8* _error) {
    if(fude_zoom_graph_is_mindmap(_text, _size)) {
        return fude_zoom_graph_parse_mindmap(_g, _text, _size, _error_line, _error);
    }
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    fude_zoom_graph_clear(_g);
    fude_zoom_graph_reader _r;
    rde_memset(&_r, 0, sizeof(_r));
    _r.g     = _g;
    _r.p     = _text != NULL ? _text : "";
    _r.end   = _r.p + (_text != NULL ? _size : 0u);
    _r.line  = 1u;
    _r.ids   = rde_hash_map_new_ex(RDE_HASH_MAP_SLOT_STR_INLINE, FUDE_ZOOM_GRAPH_ID, RDE_HASH_MAP_SLOT_BYTES, sizeof(u32), NULL, NULL, NULL, _heap);
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
    rde_hash_map_free(&_r.ids);
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
