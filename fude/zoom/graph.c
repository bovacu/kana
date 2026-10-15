// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/graph_parts.h"

// ===========================================================================
// See graph.h, which has what is read and how the layout works.
//
// graph.c is the coordinator of the graph: its storage (a graph made, freed and cleared; its nodes, edges and subgraphs
// reached), its building (a node found, a node or an edge added), and the "tables" every part of it works with: arrays
// sized once and freed together, so a pointer into one stays put. Reading Mermaid and laying out are in the graph_*.c
// modules; graph_parts.h is what they share and says which module holds what.
//
// Internal names start fude_zoom_graph_ as the public ones do. A graph keeps no state of its own: any thread may use
// one no other thread is using.
// ===========================================================================

// --- the graph -------------------------------------------------------------------------------

// Tables: each an rde_arr of _count items, all zero, sized once (so its memory stays put), kept in a list of them
// (_tables: rde_arr of rde_arr) and freed with it. This makes the empty list.
rde_arr fude_zoom_graph_tables_new(void) {
    return rde_arr_new(sizeof(rde_arr), rde_memory_allocator_get_default_std());
}

// Makes a table of _count zeroed items of _size bytes in the list _tables, and returns its memory (never NULL for
// nothing).
any fude_zoom_graph_table(rde_arr* _tables, usize _count, usize _size) {
    rde_arr _t = rde_arr_new(_size, rde_memory_allocator_get_default_std());
    rde_arr_resize(&_t, _count);
    rde_arr_add(_tables, (any)&_t);
    return _t.memory;
}

// Frees each table; the list is kept, empty.
void fude_zoom_graph_tables_clear(rde_arr* _tables) {
    rde_arr* _t = (rde_arr*)_tables->memory;
    for(u32 _i = 0; _i < (u32)rde_arr_length(_tables); _i++) {
        rde_arr_free(&_t[_i]);
    }
    rde_arr_clear(_tables);
}

// Frees each table and the list itself; safe on a list never made.
void fude_zoom_graph_tables_free(rde_arr* _tables) {
    if(rde_arr_is_inited(_tables)) {
        fude_zoom_graph_tables_clear(_tables);
        rde_arr_free(_tables);
    }
}

// Returns how many items an array holds, as a u32.
u32 fude_zoom_graph_count(const rde_arr* _a) {
    return (u32)rde_arr_length(_a);
}

// See the header.
void fude_zoom_graph_init(fude_zoom_graph* _g) {
    rde_memory_allocator* _heap = rde_memory_allocator_get_default_std();
    rde_memset(_g, 0, sizeof(*_g));
    _g->nodes     = rde_arr_new(sizeof(fude_zoom_graph_node), _heap);
    _g->edges     = rde_arr_new(sizeof(fude_zoom_graph_edge), _heap);
    _g->subgraphs = rde_arr_new(sizeof(fude_zoom_graph_subgraph), _heap);
    _g->bends     = rde_arr_new(sizeof(fude_zoom_v2), _heap);
    _g->direction = FUDE_ZOOM_FLOW_TB;
}

// See the header. Safe on a graph never made (zeroed).
void fude_zoom_graph_destroy(fude_zoom_graph* _g) {
    rde_arr* _all[4] = { &_g->nodes, &_g->edges, &_g->subgraphs, &_g->bends };
    for(u32 _i = 0; _i < 4u; _i++) {
        if(rde_arr_is_inited(_all[_i])) {
            rde_arr_free(_all[_i]);
        }
    }
}

// See the header.
void fude_zoom_graph_clear(fude_zoom_graph* _g) {
    rde_arr_clear(&_g->nodes);
    rde_arr_clear(&_g->edges);
    rde_arr_clear(&_g->subgraphs);
    rde_arr_clear(&_g->bends);
    _g->direction = FUDE_ZOOM_FLOW_TB;
}

// See the header.
fude_zoom_graph_node* fude_zoom_graph_node_at(const fude_zoom_graph* _g, u32 _i) {
    return &((fude_zoom_graph_node*)_g->nodes.memory)[_i];
}

// See the header.
fude_zoom_graph_edge* fude_zoom_graph_edge_at(const fude_zoom_graph* _g, u32 _i) {
    return &((fude_zoom_graph_edge*)_g->edges.memory)[_i];
}

// See the header.
fude_zoom_graph_subgraph* fude_zoom_graph_subgraph_at(const fude_zoom_graph* _g, u32 _i) {
    return &((fude_zoom_graph_subgraph*)_g->subgraphs.memory)[_i];
}

// --- building --------------------------------------------------------------------------------

// See the header. A linear search by id. The readers never go through it (graph_parts.h's
// fude_zoom_graph_append_node), so only a graph built a node at a time by fude_zoom_graph_add_node pays it.
u32 fude_zoom_graph_find(const fude_zoom_graph* _g, const c8* _id) {
    const u32 _n = fude_zoom_graph_count(&_g->nodes);
    for(u32 _i = 0; _i < _n; _i++) {
        if(rde_strcmp(fude_zoom_graph_node_at(_g, _i)->id, _id) == 0) {
            return _i;
        }
    }
    return FUDE_ZOOM_GRAPH_NONE;
}

// See graph_parts.h.
u32 fude_zoom_graph_append_node(fude_zoom_graph* _g, const c8* _id, u32 _n, const c8* _label, u32 _label_n, u8 _shape) {
    const u32 _count = fude_zoom_graph_count(&_g->nodes);
    if(_n == 0u || _n >= FUDE_ZOOM_GRAPH_ID || _count >= FUDE_ZOOM_GRAPH_MAX_NODES) {
        return FUDE_ZOOM_GRAPH_NONE;
    }
    fude_zoom_graph_node _node;
    rde_memset(&_node, 0, sizeof(_node));
    rde_memcpy(_node.id, _id, _n);
    fude_zoom_graph_copy(_node.label, FUDE_ZOOM_GRAPH_LABEL, _label, _label_n);
    _node.shape = _shape < FUDE_ZOOM_NODE_COUNT ? _shape : FUDE_ZOOM_NODE_RECT;
    rde_arr_add(&_g->nodes, (any)&_node);
    return _count;
}

// See the header. A label too long is cut between whole characters. The id is looked for first (a linear search:
// fude_zoom_graph_find), then the node appended.
u32 fude_zoom_graph_add_node(fude_zoom_graph* _g, const c8* _id, const c8* _label, u8 _shape) {
    const usize _n = rde_strlen(_id);
    if(_n == 0 || _n >= FUDE_ZOOM_GRAPH_ID || fude_zoom_graph_find(_g, _id) != FUDE_ZOOM_GRAPH_NONE) {
        return FUDE_ZOOM_GRAPH_NONE;
    }
    _label = _label != NULL ? _label : _id;
    return fude_zoom_graph_append_node(_g, _id, (u32)_n, _label, (u32)rde_strlen(_label), _shape);
}

// See the header.
u32 fude_zoom_graph_add_edge(fude_zoom_graph* _g, u32 _from, u32 _to) {
    const u32 _n = fude_zoom_graph_count(&_g->nodes), _count = fude_zoom_graph_count(&_g->edges);
    if(_from >= _n || _to >= _n || _count >= FUDE_ZOOM_GRAPH_MAX_EDGES) {
        return FUDE_ZOOM_GRAPH_NONE;
    }
    fude_zoom_graph_edge _edge;
    rde_memset(&_edge, 0, sizeof(_edge));
    _edge.from       = _from;
    _edge.to         = _to;
    _edge.head_end   = FUDE_ZOOM_EDGE_HEAD_ARROW;
    _edge.min_length = 1u;
    rde_arr_add(&_g->edges, (any)&_edge);
    return _count;
}
