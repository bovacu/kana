// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_GRAPH_PARTS
#define FUDE_ZOOM_GRAPH_PARTS

#include "zoom/graph.h"
#include "drawing/base/utf8.h"

// ===========================================================================
// Graph parts: the graph's private interface. The graph (graph.h) is written as graph.c and the graph_*.c modules; this
// is what they share with each other: the functions one module calls in another, and the constants and types more than
// one needs. Included by graph.c and graph_*.c only, never anywhere else: everything outside the graph uses graph.h.
//
// Where it sits: fude/zoom, under graph.h (which it includes, with every header the graph uses). The modules include
// this header and nothing else.
// Its main types and their lifecycle: the graph (fude_zoom_graph, graph.h), made with fude_zoom_graph_init and freed
// with fude_zoom_graph_destroy (graph.c); a layout's working state (fude_zoom_graph_work), made on the stack by
// fude_zoom_graph_layout and freed at its end (fude_zoom_graph_work_free); tables (fude_zoom_graph_table), freed with
// their list.
// Threads: any, as graph.h says: nothing here keeps state of its own.
// Files: none.
// Design: graph.c was one file of some 3,200 lines; it was split by what each part does, its code moved unchanged, and
// then fude_zoom_graph_layout, of over 150 lines, was broken into steps by extraction alone (fude_zoom_graph_layout_*).
// A function one module calls in another is declared here (not RDE_INTERNAL); what one module alone uses stays
// RDE_INTERNAL in it. Every internal name keeps graph.c's prefix, fude_zoom_graph_, as the public ones have it.
//
// Which module holds what:
//   graph.c         the coordinator: a graph's storage, its building (a node found, a node or an edge added), tables
//   graph_read.c    Mermaid read: the text helpers, a flowchart a statement at a time, a mind map, subgraphs' ids
//   graph_order.c   the layout's first steps: its vertices, the subgraphs' tree, cycles, ranks, dummies, fillers, order
//   graph_place.c   the places along each rank: the gaps, each row's fit, the boxes made whole, long edges straightened
//   graph_layout.c  routes, the layout's last steps (bends, labels, boxes), a mind map's layout, fude_zoom_graph_layout
// ===========================================================================

// --- the constants and types the modules share (as graph.c had them) -------------------------------------------------

// What a layout vertex is.
enum {
    FUDE_ZOOM_GRAPH_VERTEX_NODE = 0,
    FUDE_ZOOM_GRAPH_VERTEX_DUMMY,
    FUDE_ZOOM_GRAPH_VERTEX_LABEL,      // a dummy holding its edge's label
    FUDE_ZOOM_GRAPH_VERTEX_FILLER
};

// A layout vertex: a node, a dummy, a label's dummy or a filler (FUDE_ZOOM_GRAPH_VERTEX_), its rank, its order in
// it and its place.
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

// A rule between two variables (vertices, or a box's edges) along a row.
typedef struct {
    u32 a, b;
    f64 gap;       // x of b at least x of a + gap
} fude_zoom_graph_rule;

// Something to sort: two keys, then its index (fude_zoom_graph_by_keys).
typedef struct {
    f64 k1, k2;
    u32 i;
} fude_zoom_graph_sorted;

// The layout's working state: the graph, its vertices, what is worked out per edge, per vertex, per rank and per
// subgraph, and the tables they are in.
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

// --- graph.c: tables, counts -----------------------------------------------------------------------------------------

// Tables: each an rde_arr of _count items, all zero, sized once (so its memory stays put), kept in a list of them
// (_tables: rde_arr of rde_arr) and freed with it. This makes the empty list.
rde_arr fude_zoom_graph_tables_new(void);
// Makes a table of _count zeroed items of _size bytes in the list _tables, and returns its memory (never NULL for
// nothing).
any fude_zoom_graph_table(rde_arr* _tables, usize _count, usize _size);
// Frees each table; the list is kept, empty.
void fude_zoom_graph_tables_clear(rde_arr* _tables);
// Frees each table and the list itself; safe on a list never made.
void fude_zoom_graph_tables_free(rde_arr* _tables);
// Returns how many items an array holds, as a u32.
u32 fude_zoom_graph_count(const rde_arr* _a);
// Appends a node whose id is the _n bytes at _id (1 to FUDE_ZOOM_GRAPH_ID - 1 of them, and in no node yet: the caller
// knows it is new), labelled with the _label_n bytes at _label (cut between whole characters), of shape _shape (an
// unknown one a rectangle), and returns its index; FUDE_ZOOM_GRAPH_NONE when the graph holds FUDE_ZOOM_GRAPH_MAX_NODES.
// It searches nothing, so the readers, which know their ids are new (the flowchart's by its map of them, the mind
// map's made new), add each node in constant time.
u32 fude_zoom_graph_append_node(fude_zoom_graph* _g, const c8* _id, u32 _n, const c8* _label, u32 _label_n, u8 _shape);

// --- graph_read.c: a label's text ------------------------------------------------------------------------------------

// Copies _n bytes of _s into _dst (_cap bytes), cut between whole characters when they do not fit.
void fude_zoom_graph_copy(c8* _dst, u32 _cap, const c8* _s, u32 _n);

// --- graph_order.c: the layout's vertices, the subgraphs' tree, cycles, ranks, order ---------------------------------

// rde_sort's order for fude_zoom_graph_sorted: by k1, then k2, then index, so it never depends on the sort's own order.
int fude_zoom_graph_by_keys(const any _a, const any _b);
// Returns the vertices as an array (good until one is added).
fude_zoom_graph_vertex* fude_zoom_graph_v(const fude_zoom_graph_work* _w);
// Returns a size or a gap made safe: one that is not a finite number above 0 is 0.
f64 fude_zoom_graph_sane(f64 _s);
// Turns a point from flow space to the graph's direction (fude_zoom_graph_unturn, below, turns it back).
fude_zoom_v2 fude_zoom_graph_turn(u8 _dir, f64 _x, f64 _y);
// Turns a point from the graph's direction back into flow space.
fude_zoom_v2 fude_zoom_graph_unturn(u8 _dir, f64 _x, f64 _y);
// Returns whether the graph flows across (LR, RL), so a node's width runs along the flow.
b8 fude_zoom_graph_sideways(u8 _dir);
// Adds a vertex of kind _kind in subgraph _sub at rank _rank, _wide across and _tall along (flow space), and returns
// its index.
u32 fude_zoom_graph_add_vertex(fude_zoom_graph_work* _w, u8 _kind, u32 _sub, i32 _rank, f64 _wide, f64 _tall);
// The parents made safe (a loop or a bad index cut at the top) and depths.
void fude_zoom_graph_tree(fude_zoom_graph_work* _w);
// Returns the innermost subgraph holding both _a and _b (index + 1; 0 the top).
u32 fude_zoom_graph_common(const fude_zoom_graph_work* _w, u32 _a, u32 _b);
// _sub's subgraphs outermost first into _path (most_depth long). How many.
u32 fude_zoom_graph_path(const fude_zoom_graph_work* _w, u32 _sub, u32* _path);
// Back edges turned round: those a depth-first walk (the nodes in order, each one's edges in order) finds going back
// up its own path. Self-loops and edges with no ends are left as they are.
void fude_zoom_graph_break_cycles(fude_zoom_graph_work* _w);
// Ranks by longest path from the sources, then each node that feeds more edges than it is fed pulled down as far as
// its edges let it (shorter edges in all: a source joins what it feeds instead of waiting at the top). The ranks are
// then counted from 0.
void fude_zoom_graph_rank(fude_zoom_graph_work* _w);
// Makes each edge's chain: its tail, a dummy at each rank it crosses (the middle one holding its label when labels
// have ranks of their own), and its head. A dummy is in its ends' innermost common subgraph. Once
// FUDE_ZOOM_GRAPH_DUMMIES are made, a longer edge gets no chain.
void fude_zoom_graph_chains(fude_zoom_graph_work* _w);
// Each subgraph's ranks, and a filler wherever it has none between its first
// and last (the deepest first: one filler stands for its outer ones too).
// Pinned nodes do not count: they are not in the ranks' rows.
void fude_zoom_graph_fillers(fude_zoom_graph_work* _w);
// Each vertex's neighbours above and below, from the chains (edges in order).
void fude_zoom_graph_neighbours(fude_zoom_graph_work* _w);
// The ranks' order: first as a walk down from each node in turn found them,
// then barycentre sweeps, down and up by turns, each rank by where its
// neighbours are in the rank just done, the one with fewest crossings kept.
void fude_zoom_graph_order(fude_zoom_graph_work* _w);

// --- graph_place.c: the places along each rank -----------------------------------------------------------------------

// Along each rank: packed, drawn towards their neighbours, the boxes made
// whole, drawn towards their neighbours again inside them, long edges
// straightened, and the pinned nodes kept clear of.
void fude_zoom_graph_place(fude_zoom_graph_work* _w);

#endif
