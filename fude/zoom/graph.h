// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_GRAPH
#define FUDE_ZOOM_GRAPH

#include "rde.h"
#include "zoom/zoom.h"

// ===========================================================================
// Diagrams as graphs (research §2.9, items 6 and 9): Mermaid flowcharts read
// into nodes, edges and subgraphs, and a layered layout for them. The canvas
// turns the result into its own shapes and connectors; nothing here draws.
//
// READING. The flowchart part of Mermaid (mermaid.js.org/syntax/flowchart):
//   flowchart TD | TB | BT | LR | RL (or graph; no direction is TB), front
//   matter between --- lines, %% comments and %%{ directives }%%; statements
//   end at a line's end or a ";".
//   Nodes: A  A[a]  A(a)  A([a])  A[[a]]  A[(a)]  A((a))  A(((a)))  A>a]
//   A{a}  A{{a}}  A[/a/]  A[\a\]  A[/a\]  A[\a/], the text quoted when it
//   holds brackets: A["a (b)"]. A node named again without a shape keeps
//   what it had; named again with one, it takes the new shape and text.
//   Edges: --> --- -.-> -.- ==> === ~~~ --o --x, a head at the start too
//   (<--> o--o x--x), longer for more ranks (---> ---->, -..->, ===>), text
//   as -->|a| or -- a --> or -. a .-> or == a ==>. Chains (A --> B --> C)
//   and & (A & B --> C & D: every pair).
//   subgraph id [title] / subgraph title / subgraph "title" ... end, nested.
//   Labels: #quot; #35; #x23; style entities and <br> become their
//   characters; "`markdown`" loses its backticks and is kept as written.
//   Read past, not kept: classDef, class, style, linkStyle, click, accTitle,
//   accDescr, ":::class" after a node, and direction inside a subgraph (a
//   subgraph flows the way the whole graph does). An edge to or from a
//   subgraph's id joins its box (from_subgraph, to_subgraph), one of its
//   nodes standing in for it in the layout. Not read (an error): A@{ ... }
//   shapes, edge ids (e1@-->), lowercase "end" as a node.
//
// LAYOUT. Layered (Sugiyama, kept simple): back edges reversed (a depth-first
// walk in the nodes' order), ranks by longest path then sources pulled down
// towards what they feed, long edges broken by dummies at each rank they
// cross, the order in each rank by barycentre sweeps (the best of a fixed
// number kept), then places: each rank as tall as its tallest node, and along
// it each node drawn towards the median of its neighbours without coming
// nearer the next than node_gap (an exact isotonic fit per rank), and long
// edges straightened where the ranks they cross have room. Worked out top to
// bottom and turned for the other directions.
//   Subgraphs keep together in every rank, in the same order everywhere, so
//   their boxes (padded) hold their members and nothing else.
//   Edge labels given a size (label_w, label_h) get a place of their own, a
//   rank between their ends' ranks, kept clear like a node; the others go
//   halfway along their line.
//   STABLE: a node marked `placed` (every node after a layout) keeps its order
//   in its rank among the placed nodes of the same subgraph, so laying out
//   again after adding a node only fits the new one (and its edges) in: the
//   others may slide along their ranks but never swap. Same input, same
//   output, always. A `pinned` node does not move at all; the ranks line up
//   on the pinned nodes (the middle one, where they disagree) and the rest
//   keeps clear of them (a subgraph's box may then reach over a node, if pins
//   leave it no room).
//
// Positions are Y DOWN (TB's first rank on top, at the smallest y): negate y
// for the canvas. x, y are centres. With nothing pinned the result sits where
// the placed nodes were (on average), or with its corner at 0, 0.
// ===========================================================================

#define FUDE_ZOOM_GRAPH_NONE           0xFFFFFFFFu
#define FUDE_ZOOM_GRAPH_ID             48u     // bytes for an id (with its NUL); a longer one is an error
#define FUDE_ZOOM_GRAPH_LABEL          160u    // a node's or subgraph's text; longer is cut at a whole character
#define FUDE_ZOOM_GRAPH_EDGE_LABEL     96u
#define FUDE_ZOOM_GRAPH_MAX_NODES      4096u
#define FUDE_ZOOM_GRAPH_MAX_EDGES      16384u
#define FUDE_ZOOM_GRAPH_MAX_SUBGRAPHS  512u
#define FUDE_ZOOM_GRAPH_MAX_DEPTH      32u     // subgraphs inside subgraphs
#define FUDE_ZOOM_GRAPH_MAX_LENGTH     16u     // an edge's min_length at most

typedef enum {
    FUDE_ZOOM_NODE_RECT = 0,            // A  A[a]
    FUDE_ZOOM_NODE_ROUND,               // A(a): rounded corners
    FUDE_ZOOM_NODE_STADIUM,             // A([a])
    FUDE_ZOOM_NODE_CIRCLE,              // A((a))
    FUDE_ZOOM_NODE_DIAMOND,             // A{a}
    FUDE_ZOOM_NODE_HEXAGON,             // A{{a}}
    FUDE_ZOOM_NODE_PARALLELOGRAM,       // A[/a/]
    FUDE_ZOOM_NODE_CYLINDER,            // A[(a)]
    FUDE_ZOOM_NODE_SUBROUTINE,          // A[[a]]
    FUDE_ZOOM_NODE_DOUBLE_CIRCLE,       // A(((a)))
    FUDE_ZOOM_NODE_FLAG,                // A>a]: Mermaid's asymmetric shape
    FUDE_ZOOM_NODE_PARALLELOGRAM_ALT,   // A[\a\]
    FUDE_ZOOM_NODE_TRAPEZOID,           // A[/a\]: wider at the bottom
    FUDE_ZOOM_NODE_TRAPEZOID_ALT,       // A[\a/]: wider at the top
    FUDE_ZOOM_NODE_COUNT
} FUDE_ZOOM_NODE_;

typedef enum {
    FUDE_ZOOM_EDGE_LINE_SOLID = 0,      // --
    FUDE_ZOOM_EDGE_LINE_DOTTED,         // -.-
    FUDE_ZOOM_EDGE_LINE_THICK,          // ==
    FUDE_ZOOM_EDGE_LINE_INVISIBLE       // ~~~: placed by, not drawn
} FUDE_ZOOM_EDGE_LINE_;

typedef enum {
    FUDE_ZOOM_EDGE_HEAD_NONE = 0,
    FUDE_ZOOM_EDGE_HEAD_ARROW,          // > or <
    FUDE_ZOOM_EDGE_HEAD_CIRCLE,         // o
    FUDE_ZOOM_EDGE_HEAD_CROSS           // x
} FUDE_ZOOM_EDGE_HEAD_;

typedef enum {
    FUDE_ZOOM_FLOW_TB = 0,              // TB and TD
    FUDE_ZOOM_FLOW_BT,
    FUDE_ZOOM_FLOW_LR,
    FUDE_ZOOM_FLOW_RL
} FUDE_ZOOM_FLOW_;

// Why Mermaid text was not read.
typedef enum {
    FUDE_ZOOM_MERMAID_OK = 0,
    FUDE_ZOOM_MERMAID_HEADER,           // no "flowchart" or "graph" first (or front matter left open)
    FUDE_ZOOM_MERMAID_DIRECTION,        // the header's direction is none of TB TD BT LR RL
    FUDE_ZOOM_MERMAID_NODE,             // a node was wanted (after a link or an &)
    FUDE_ZOOM_MERMAID_ID,               // an id too long for FUDE_ZOOM_GRAPH_ID
    FUDE_ZOOM_MERMAID_SHAPE,            // a node's text not closed by its shape's bracket
    FUDE_ZOOM_MERMAID_QUOTE,            // a quoted text not closed
    FUDE_ZOOM_MERMAID_LINK,             // after a node, neither a link, an & nor the statement's end
    FUDE_ZOOM_MERMAID_LABEL,            // a link's text (|a| or -- a -->) not closed
    FUDE_ZOOM_MERMAID_END,              // "end" with no subgraph open
    FUDE_ZOOM_MERMAID_OPEN,             // a subgraph not closed (the line is its own)
    FUDE_ZOOM_MERMAID_DEPTH,            // subgraphs deeper than FUDE_ZOOM_GRAPH_MAX_DEPTH
    FUDE_ZOOM_MERMAID_TOO_BIG           // past FUDE_ZOOM_GRAPH_MAX_NODES, _EDGES or _SUBGRAPHS
} FUDE_ZOOM_MERMAID_;

typedef struct {
    c8  id[FUDE_ZOOM_GRAPH_ID];         // unique in its graph
    c8  label[FUDE_ZOOM_GRAPH_LABEL];   // UTF-8, '\n' a line break; the id when it was given none
    u8  shape;                          // FUDE_ZOOM_NODE_
    b8  pinned;                         // the layout leaves it where it is
    b8  placed;                         // x, y are from an earlier layout (or the caller): its order is kept
    u32 subgraph;                       // index + 1 into subgraphs, 0 none
    f64 x, y;                           // its centre
    f64 w, h;                           // its size, the caller's (its label measured), before the layout
} fude_zoom_graph_node;

typedef struct {
    u32 from, to;                       // node indices
    c8  label[FUDE_ZOOM_GRAPH_EDGE_LABEL];
    u8  line;                           // FUDE_ZOOM_EDGE_LINE_
    u8  head_start, head_end;           // FUDE_ZOOM_EDGE_HEAD_ at `from` and at `to`
    u32 min_length;                     // ranks apart at least: 1 for -->, 2 for --->, 3 for ---->
    u32 from_subgraph, to_subgraph;     // index + 1 when that end is a subgraph's box (`from` or `to`
                                        // is then one of its nodes, standing in for it), 0 the node
    f64 label_w, label_h;               // its label's size, the caller's (0, 0: no room kept for it)
    // The layout's:
    u32 bend_first, bend_count;         // its bends in the graph's bends, from `from` to `to`
    f64 label_x, label_y;               // where its label's centre goes
} fude_zoom_graph_edge;

typedef struct {
    c8  id[FUDE_ZOOM_GRAPH_ID];         // "subGraphN" when the text gave none
    c8  label[FUDE_ZOOM_GRAPH_LABEL];   // its title
    u32 parent;                         // index + 1, 0 at the top
    f64 x, y, w, h;                     // the layout's: its box (centre, size) round its members and
                                        // the subgraphs in it, padded; 0 when it holds no node
} fude_zoom_graph_subgraph;

typedef struct {
    rde_arr nodes;                      // fude_zoom_graph_node
    rde_arr edges;                      // fude_zoom_graph_edge
    rde_arr subgraphs;                  // fude_zoom_graph_subgraph
    rde_arr bends;                      // fude_zoom_v2: the edges' bends (the layout's)
    u8      direction;                  // FUDE_ZOOM_FLOW_
} fude_zoom_graph;

typedef struct {
    f64 rank_gap;                       // between ranks, along the flow
    f64 node_gap;                       // between neighbours in a rank (half that beside an edge's bend)
    f64 subgraph_pad;                   // inside a subgraph's box, round what it holds
    f64 subgraph_title;                 // more at a box's top (the page's top, any direction), for its title
} fude_zoom_graph_spacing;

// An empty graph (TB), and its memory given back.
void fude_zoom_graph_init(fude_zoom_graph* _g);
void fude_zoom_graph_destroy(fude_zoom_graph* _g);
// Everything taken out (the direction back to TB).
void fude_zoom_graph_clear(fude_zoom_graph* _g);

fude_zoom_graph_node*     fude_zoom_graph_node_at(const fude_zoom_graph* _g, u32 _i);
fude_zoom_graph_edge*     fude_zoom_graph_edge_at(const fude_zoom_graph* _g, u32 _i);
fude_zoom_graph_subgraph* fude_zoom_graph_subgraph_at(const fude_zoom_graph* _g, u32 _i);
// The node with that id, or FUDE_ZOOM_GRAPH_NONE.
u32  fude_zoom_graph_find(const fude_zoom_graph* _g, const c8* _id);
// A new node (not placed, not pinned; _label NULL: its id) or a solid arrow
// from one to another (min_length 1). Their index, or FUDE_ZOOM_GRAPH_NONE: the
// id taken or too long, an end that is no node, or the graph full.
u32  fude_zoom_graph_add_node(fude_zoom_graph* _g, const c8* _id, const c8* _label, u8 _shape);
u32  fude_zoom_graph_add_edge(fude_zoom_graph* _g, u32 _from, u32 _to);

// Mermaid flowchart text (_size bytes, UTF-8) into _g, which is cleared
// first. False when it cannot be read: _g left empty, and (each may be NULL)
// the line it stopped at (from 1) and why (FUDE_ZOOM_MERMAID_).
b8   fude_zoom_graph_parse_mermaid(fude_zoom_graph* _g, const c8* _text, u32 _size, u32* _error_line, u8* _error);

// Places every node that is not pinned (x, y), each subgraph's box, and each
// edge's bends and label. Give the nodes their size first (and the edges
// their label's, to keep room for it).
void fude_zoom_graph_layout(fude_zoom_graph* _g, fude_zoom_graph_spacing _sp);

// Each edge's line for drawing: from the outline of its `from` node (its box;
// a circle's or a diamond's own outline; a subgraph end's box) through its
// bends to the outline of its `to` node. The points go into _points (rde_arr
// of fude_zoom_v2), and where each edge's begin into _starts (rde_arr of
// u32), one more at the end: edge e is points [starts[e], starts[e + 1]).
// Both cleared first. From the nodes as they are now, so one moved since the
// layout is still met at its edge (the bends stay where the layout put them).
void fude_zoom_graph_routes(const fude_zoom_graph* _g, rde_arr* _points, rde_arr* _starts);

#endif
