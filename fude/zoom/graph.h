// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_GRAPH
#define FUDE_ZOOM_GRAPH

#include "rde.h"
#include "zoom/zoom.h"

// ===========================================================================
// Graph: diagrams as graphs (docs/research/infinite_canvas_research.md §2.9, items 6 and 9). Mermaid flowcharts and
// mind maps are read into nodes, edges and subgraphs, and given a layered layout. Nothing here draws: the canvas turns
// the result into its own shapes and connectors.
//
// Where it sits: fude/zoom, pure data and geometry with no scene. The page (page_diagrams.c) uses it to import Mermaid
// text and to lay diagrams out; tests/graph tests it.
// Its main types and their lifecycle: the caller owns a fude_zoom_graph, made with fude_zoom_graph_init and freed with
// fude_zoom_graph_destroy. A pointer from fude_zoom_graph_node_at, _edge_at or _subgraph_at stays valid until that
// array grows (an add, or reading Mermaid).
// Threads: it keeps no state of its own, so any thread may use a graph that no other thread is using.
// Design:
//   READING. The flowchart part of Mermaid (mermaid.js.org/syntax/flowchart):
//     - flowchart TD | TB | BT | LR | RL (or graph; with no direction it is TB), front matter between --- lines,
//       %% comments and %%{ directives }%%. Statements end at a line's end or at a ";".
//     - Nodes: A  A[a]  A(a)  A([a])  A[[a]]  A[(a)]  A((a))  A(((a)))  A>a]  A{a}  A{{a}}  A[/a/]  A[\a\]
//       A[/a\]  A[\a/]. The text is quoted when it holds brackets: A["a (b)"]. A node named again without a shape
//       keeps what it had; named again with one, it takes the new shape and text.
//     - Edges: --> --- -.-> -.- ==> === ~~~ --o --x, with a head at the start too (<--> o--o x--x), longer for more
//       ranks (---> ---->, -..->, ===>), and text written as -->|a| or -- a --> or -. a .-> or == a ==>. Chains
//       (A --> B --> C) and & (A & B --> C & D joins every pair).
//     - subgraph id [title] / subgraph title / subgraph "title" ... end, nested.
//     - In labels, #quot; #35; #x23; style entities and <br> become their characters; "`markdown`" loses its
//       backticks and is kept as written.
//     - Read past but not kept: classDef, class, style, linkStyle, click, accTitle, accDescr, ":::class" after a node,
//       and direction inside a subgraph (a subgraph flows the way the whole graph does).
//     - An edge to or from a subgraph's id joins its box (from_subgraph, to_subgraph), one of its nodes standing in for
//       it in the layout.
//     - Not read (an error): A@{ ... } shapes, edge ids (e1@-->), and lowercase "end" as a node.
//   LAYOUT. Layered (Sugiyama's, kept simple):
//     - Back edges are reversed (found by a depth-first walk in the nodes' order). Ranks come from the longest path,
//       then sources are pulled down towards what they feed. Long edges are broken by dummy nodes at each rank they
//       cross. The order in each rank comes from barycentre sweeps (the best of a fixed number is kept).
//     - Then the places: each rank is as tall as its tallest node, and along it each node is drawn towards the median
//       of its neighbours without coming nearer the next than node_gap (an exact isotonic fit per rank). Long edges
//       are straightened where the ranks they cross have room. It is worked out top to bottom and turned for the
//       other directions.
//     - Subgraphs keep together in every rank, in the same order everywhere, so their boxes (padded) hold their
//       members and nothing else.
//     - Edge labels given a size (label_w, label_h) get a place of their own, a rank between their ends' ranks, kept
//       clear like a node; the others go halfway along their line.
//     - STABLE: a node marked `placed` (as every node is after a layout) keeps its order in its rank among the placed
//       nodes of the same subgraph. So laying out again after adding a node only fits the new one (and its edges) in:
//       the others may slide along their ranks but never swap. The same input always gives the same output.
//     - A `pinned` node does not move at all. The ranks line up on the pinned nodes (on the middle one where they
//       disagree) and the rest keeps clear of them (a subgraph's box may then reach over a node, if pins leave it no
//       room).
//   POSITIONS are Y DOWN (TB's first rank on top, at the smallest y), so the canvas negates y. x and y are centres.
//   With nothing pinned, the result sits where the placed nodes were (on average), or else with its corner at 0, 0.
// ===========================================================================

// No node (or edge).
#define FUDE_ZOOM_GRAPH_NONE           0xFFFFFFFFu
// Sizes in bytes, with the NUL. A longer id is an error; a longer text is cut at a whole character.
#define FUDE_ZOOM_GRAPH_ID             48u     // an id
#define FUDE_ZOOM_GRAPH_LABEL          160u    // a node's or a subgraph's text
// An edge's text.
#define FUDE_ZOOM_GRAPH_EDGE_LABEL     96u
// The most a graph holds.
#define FUDE_ZOOM_GRAPH_MAX_NODES      4096u
#define FUDE_ZOOM_GRAPH_MAX_EDGES      16384u
#define FUDE_ZOOM_GRAPH_MAX_SUBGRAPHS  512u
#define FUDE_ZOOM_GRAPH_MAX_DEPTH      32u     // subgraphs inside subgraphs
#define FUDE_ZOOM_GRAPH_MIND_DEPTH     64u     // a mind map's levels: read and laid out this deep at most
#define FUDE_ZOOM_GRAPH_MAX_LENGTH     16u     // the largest min_length an edge may have

// A node's shape, with the Mermaid that makes it.
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

// An edge's line, with the Mermaid that makes it.
typedef enum {
    FUDE_ZOOM_EDGE_LINE_SOLID = 0,      // --
    FUDE_ZOOM_EDGE_LINE_DOTTED,         // -.-
    FUDE_ZOOM_EDGE_LINE_THICK,          // ==
    FUDE_ZOOM_EDGE_LINE_INVISIBLE       // ~~~: used for placing, not drawn
} FUDE_ZOOM_EDGE_LINE_;

// What an edge's end has, with the Mermaid that makes it.
typedef enum {
    FUDE_ZOOM_EDGE_HEAD_NONE = 0,
    FUDE_ZOOM_EDGE_HEAD_ARROW,          // > or <
    FUDE_ZOOM_EDGE_HEAD_CIRCLE,         // o
    FUDE_ZOOM_EDGE_HEAD_CROSS           // x
} FUDE_ZOOM_EDGE_HEAD_;

// Which way a graph flows.
typedef enum {
    FUDE_ZOOM_FLOW_TB = 0,              // TB and TD
    FUDE_ZOOM_FLOW_BT,
    FUDE_ZOOM_FLOW_LR,
    FUDE_ZOOM_FLOW_RL,
    FUDE_ZOOM_FLOW_MIND                 // a mind map (Mermaid's mindmap): laid out round its root, both ways
} FUDE_ZOOM_FLOW_;

// Why Mermaid text was not read.
typedef enum {
    FUDE_ZOOM_MERMAID_OK = 0,
    FUDE_ZOOM_MERMAID_HEADER,           // no "flowchart" or "graph" first (or front matter left open)
    FUDE_ZOOM_MERMAID_DIRECTION,        // the header's direction is none of TB TD BT LR RL
    FUDE_ZOOM_MERMAID_NODE,             // a node was expected (after a link or an &)
    FUDE_ZOOM_MERMAID_ID,               // an id too long for FUDE_ZOOM_GRAPH_ID
    FUDE_ZOOM_MERMAID_SHAPE,            // a node's text not closed by its shape's bracket
    FUDE_ZOOM_MERMAID_QUOTE,            // a quoted text not closed
    FUDE_ZOOM_MERMAID_LINK,             // after a node, neither a link, an & nor the statement's end
    FUDE_ZOOM_MERMAID_LABEL,            // a link's text (|a| or -- a -->) not closed
    FUDE_ZOOM_MERMAID_END,              // "end" with no subgraph open
    FUDE_ZOOM_MERMAID_OPEN,             // a subgraph not closed (the error line is the subgraph's own)
    // Subgraphs nested deeper than FUDE_ZOOM_GRAPH_MAX_DEPTH, or a mind map more than FUDE_ZOOM_GRAPH_MIND_DEPTH deep.
    FUDE_ZOOM_MERMAID_DEPTH,
    FUDE_ZOOM_MERMAID_TOO_BIG           // more than FUDE_ZOOM_GRAPH_MAX_NODES, _EDGES or _SUBGRAPHS
} FUDE_ZOOM_MERMAID_;

// A node.
typedef struct {
    c8  id[FUDE_ZOOM_GRAPH_ID];         // unique in its graph
    c8  label[FUDE_ZOOM_GRAPH_LABEL];   // UTF-8, '\n' for a line break; the id when it was given none
    u8  shape;                          // FUDE_ZOOM_NODE_
    b8  pinned;                         // the layout leaves it where it is
    b8  placed;                         // x, y come from an earlier layout (or the caller), so its order is kept
    u32 subgraph;                       // its subgraph's index + 1 (0 for none)
    f64 x, y;                           // its centre
    f64 w, h;                           // its size, set by the caller (its label measured) before the layout
} fude_zoom_graph_node;

// An edge.
typedef struct {
    u32 from, to;                       // node indices
    c8  label[FUDE_ZOOM_GRAPH_EDGE_LABEL];
    u8  line;                           // FUDE_ZOOM_EDGE_LINE_
    u8  head_start, head_end;           // FUDE_ZOOM_EDGE_HEAD_ at `from` and at `to`
    u32 min_length;                     // the fewest ranks between its ends: 1 for -->, 2 for --->, 3 for ---->
    u32 from_subgraph, to_subgraph;     // a subgraph's index + 1 when that end is its box (`from` or `to` is then
                                        // one of its nodes, standing in for it); 0 when the end is the node
    f64 label_w, label_h;               // its label's size, set by the caller (0, 0: no room is kept for it)
    // Set by the layout:
    u32 bend_first, bend_count;         // its bends in the graph's bends array, in order from `from` to `to`
    f64 label_x, label_y;               // where its label's centre goes
} fude_zoom_graph_edge;

// A subgraph.
typedef struct {
    c8  id[FUDE_ZOOM_GRAPH_ID];         // "subGraphN" when the text gave it none
    c8  label[FUDE_ZOOM_GRAPH_LABEL];   // its title
    u32 parent;                         // the subgraph it is in: index + 1 (0 at the top)
    f64 x, y, w, h;                     // set by the layout: its box (centre and size) round its members and the
                                        // subgraphs in it, padded; all 0 when it holds no node
} fude_zoom_graph_subgraph;

// A graph: its nodes, edges and subgraphs, and the bends the layout gave its edges.
typedef struct {
    rde_arr nodes;                      // fude_zoom_graph_node
    rde_arr edges;                      // fude_zoom_graph_edge
    rde_arr subgraphs;                  // fude_zoom_graph_subgraph
    rde_arr bends;                      // fude_zoom_v2: the edges' bends, set by the layout
    u8      direction;                  // FUDE_ZOOM_FLOW_
} fude_zoom_graph;

// The gaps the layout keeps, in the units of the nodes' sizes.
typedef struct {
    f64 rank_gap;                       // between ranks, along the flow
    f64 node_gap;                       // between neighbours in a rank (half that beside an edge's bend)
    f64 subgraph_pad;                   // inside a subgraph's box, round what it holds
    f64 subgraph_title;                 // more room at a box's top (the page's top, any direction) for its title
} fude_zoom_graph_spacing;

// fude_zoom_graph_init makes an empty graph flowing TB; fude_zoom_graph_destroy frees its memory.
void fude_zoom_graph_init(fude_zoom_graph* _g);
void fude_zoom_graph_destroy(fude_zoom_graph* _g);
// Takes everything out of the graph and sets its direction back to TB, keeping its memory.
void fude_zoom_graph_clear(fude_zoom_graph* _g);

// Return the node, edge or subgraph at index _i, which must be in range (it is not checked). The pointer stays valid
// until that array grows.
fude_zoom_graph_node*     fude_zoom_graph_node_at(const fude_zoom_graph* _g, u32 _i);
fude_zoom_graph_edge*     fude_zoom_graph_edge_at(const fude_zoom_graph* _g, u32 _i);
fude_zoom_graph_subgraph* fude_zoom_graph_subgraph_at(const fude_zoom_graph* _g, u32 _i);
// Returns the index of the node with id _id, or FUDE_ZOOM_GRAPH_NONE when there is none.
u32  fude_zoom_graph_find(const fude_zoom_graph* _g, const c8* _id);
// fude_zoom_graph_add_node adds a node, neither placed nor pinned, labelled _label (NULL labels it with its id; an
// unknown shape is a rectangle). fude_zoom_graph_add_edge adds a solid arrow from node _from to node _to (min_length
// 1). Each returns the new index, or FUDE_ZOOM_GRAPH_NONE when the id is empty, taken or too long, an end is not a
// node, or the graph is full.
u32  fude_zoom_graph_add_node(fude_zoom_graph* _g, const c8* _id, const c8* _label, u8 _shape);
u32  fude_zoom_graph_add_edge(fude_zoom_graph* _g, u32 _from, u32 _to);

// Reads Mermaid flowchart text (_size bytes of UTF-8) into _g, which is cleared first. Returns false when it cannot be
// read: _g is left empty, and the line it stopped at (counting from 1) and why (FUDE_ZOOM_MERMAID_) are written to
// _error_line and _error (either may be NULL).
// It reads a MIND MAP too (0.1.86: mermaid.js.org/syntax/mindmap), with "mindmap" first. Each line is a node whose
// parent is the nearest line above it that is less indented; there is one root, and a second is an error
// (FUDE_ZOOM_MERMAID_NODE), as is a node more than FUDE_ZOOM_GRAPH_MIND_DEPTH levels deep (FUDE_ZOOM_MERMAID_DEPTH, its
// line). A node is text alone or id[a] id(a) id((a)) id))a(( id)a( id{{a}}; ::icon(...) and
// :::class lines are read past. Its edges are plain lines from parent to child, and its direction is
// FUDE_ZOOM_FLOW_MIND, so fude_zoom_graph_layout lays it out as a mind map.
b8   fude_zoom_graph_parse_mermaid(fude_zoom_graph* _g, const c8* _text, u32 _size, u32* _error_line, u8* _error);

// Places every node that is not pinned (its x, y), each subgraph's box, and each edge's bends and label, and marks
// every node placed. Give the nodes their sizes first (and the edges their labels' sizes, to keep room for them).
// A mind map (FUDE_ZOOM_FLOW_MIND) is laid out differently: its root stays where it is, and its first branches are
// shared out right and left (each to the side with less so far, in their order). Each branch is a tree growing
// outward, with rank_gap between a node and its children and node_gap between siblings' branches, each node in the
// middle of its own children; each edge curves out of its parent's side into its child's (its bends along the curve).
void fude_zoom_graph_layout(fude_zoom_graph* _g, fude_zoom_graph_spacing _sp);

// Works out each edge's line for drawing: from the outline of its `from` node (its box; a circle's or a diamond's own
// outline; a subgraph's box for a subgraph end) through its bends to the outline of its `to` node. The points go into
// _points (an rde_arr of fude_zoom_v2), and where each edge's points begin into _starts (an rde_arr of u32), with one
// more entry at the end, so edge e is points [starts[e], starts[e + 1]). Both are cleared first. It uses the nodes as
// they are now, so a node moved since the layout is still met at its edge (the bends stay where the layout put them).
void fude_zoom_graph_routes(const fude_zoom_graph* _g, rde_arr* _points, rde_arr* _starts);

#endif
