// Diagrams as graphs (fude/zoom/graph): Mermaid flowcharts read into nodes, edges
// and subgraphs, laid out in layers (no overlaps, edges going down, turned for
// the other directions, the same every time, pinned nodes left alone, the order
// kept when a node is added) and routed from outline to outline.
#include "zoom/graph.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
#define CHECK(c) do { if(!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while(0)

static u32 rng = 12345u;
static u32 rnd(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }

static fude_zoom_graph G;
static u32 err_line;
static u8  err_why;

static b8 parse(const char* text) {
    return fude_zoom_graph_parse_mermaid(&G, text, (u32)strlen(text), &err_line, &err_why);
}
static u32 nodes(const fude_zoom_graph* g) { return (u32)rde_arr_length(&g->nodes); }
static u32 edges(const fude_zoom_graph* g) { return (u32)rde_arr_length(&g->edges); }
static u32 subs(const fude_zoom_graph* g) { return (u32)rde_arr_length(&g->subgraphs); }
static fude_zoom_graph_node* node(const fude_zoom_graph* g, const char* id) {
    const u32 i = fude_zoom_graph_find(g, id);
    return i == FUDE_ZOOM_GRAPH_NONE ? NULL : fude_zoom_graph_node_at(g, i);
}
static fude_zoom_graph_edge* edge(const fude_zoom_graph* g, u32 i) { return fude_zoom_graph_edge_at(g, i); }
static const char* id_of(const fude_zoom_graph* g, u32 i) { return fude_zoom_graph_node_at(g, i)->id; }
static b8 has_edge(const fude_zoom_graph* g, const char* a, const char* b) {
    for(u32 e = 0; e < edges(g); e++) {
        if(strcmp(id_of(g, edge(g, e)->from), a) == 0 && strcmp(id_of(g, edge(g, e)->to), b) == 0) return true;
    }
    return false;
}
static b8 valid_utf8(const char* s) {
    const unsigned char* u = (const unsigned char*)s;
    while(*u) {
        u32 n = *u < 0x80 ? 1 : (*u & 0xE0) == 0xC0 ? 2 : (*u & 0xF0) == 0xE0 ? 3 : (*u & 0xF8) == 0xF0 ? 4 : 0;
        if(n == 0) return false;
        for(u32 k = 1; k < n; k++) if((u[k] & 0xC0) != 0x80) return false;
        u += n;
    }
    return true;
}

static const fude_zoom_graph_spacing SP = { 50.0, 30.0, 16.0, 20.0 };

static void size_all(fude_zoom_graph* g, f64 w, f64 h) {
    for(u32 i = 0; i < nodes(g); i++) {
        fude_zoom_graph_node* n = fude_zoom_graph_node_at(g, i);
        n->w = w + (f64)(strlen(n->label) % 5u) * 8.0;   // a little variety, as measured labels have
        n->h = h;
    }
}

// No two node boxes overlap (touching is fine).
static b8 no_overlaps(const fude_zoom_graph* g) {
    for(u32 i = 0; i < nodes(g); i++) {
        for(u32 j = i + 1; j < nodes(g); j++) {
            const fude_zoom_graph_node *a = fude_zoom_graph_node_at(g, i), *b = fude_zoom_graph_node_at(g, j);
            if(fabs(a->x - b->x) < (a->w + b->w) * 0.5 - 1e-6 && fabs(a->y - b->y) < (a->h + b->h) * 0.5 - 1e-6) {
                printf("  overlap: %s (%.1f %.1f %.1fx%.1f) and %s (%.1f %.1f %.1fx%.1f)\n", a->id, a->x, a->y, a->w, a->h, b->id, b->x, b->y, b->w, b->h);
                return false;
            }
        }
    }
    return true;
}

// Edges going the flow's way (TB: down), and how many go against it.
static u32 against(const fude_zoom_graph* g) {
    u32 bad = 0;
    for(u32 e = 0; e < edges(g); e++) {
        const fude_zoom_graph_node *a = fude_zoom_graph_node_at(g, edge(g, e)->from), *b = fude_zoom_graph_node_at(g, edge(g, e)->to);
        if(a == b) continue;
        const f64 d = g->direction == FUDE_ZOOM_FLOW_TB ? b->y - a->y : g->direction == FUDE_ZOOM_FLOW_BT ? a->y - b->y :
                      g->direction == FUDE_ZOOM_FLOW_LR ? b->x - a->x : a->x - b->x;
        bad += d > 0.0 ? 0u : 1u;
    }
    return bad;
}

// 1 on the node's outline (its box, or a circle's or diamond's own).
static f64 on_outline(const fude_zoom_graph_node* n, fude_zoom_v2 p) {
    const f64 dx = (p.x - n->x) / (n->w * 0.5), dy = (p.y - n->y) / (n->h * 0.5);
    if(n->shape == FUDE_ZOOM_NODE_CIRCLE || n->shape == FUDE_ZOOM_NODE_DOUBLE_CIRCLE) return sqrt(dx * dx + dy * dy);
    if(n->shape == FUDE_ZOOM_NODE_DIAMOND) return fabs(dx) + fabs(dy);
    return fmax(fabs(dx), fabs(dy));
}

// An edge's end as drawn: its node, or the box of the subgraph it joins.
static fude_zoom_graph_node end_of(const fude_zoom_graph* g, u32 n, u32 sub) {
    fude_zoom_graph_node e = *fude_zoom_graph_node_at(g, n);
    if(sub != 0 && fude_zoom_graph_subgraph_at(g, sub - 1)->w > 0) {
        const fude_zoom_graph_subgraph* s = fude_zoom_graph_subgraph_at(g, sub - 1);
        e.x = s->x; e.y = s->y; e.w = s->w; e.h = s->h; e.shape = FUDE_ZOOM_NODE_RECT;
    }
    return e;
}

static b8 routes_ok(const fude_zoom_graph* g) {
    rde_arr pts = rde_arr_new(sizeof(fude_zoom_v2), NULL), starts = rde_arr_new(sizeof(u32), NULL);
    fude_zoom_graph_routes(g, &pts, &starts);
    b8 ok = rde_arr_length(&starts) == edges(g) + 1u;
    const fude_zoom_v2* p = (const fude_zoom_v2*)pts.memory;
    const u32* s = (const u32*)starts.memory;
    for(u32 e = 0; ok && e < edges(g); e++) {
        const fude_zoom_graph_edge* ed = edge(g, e);
        const u32 n = s[e + 1] - s[e];
        ok = ed->from_subgraph || ed->to_subgraph ? n >= 2u && n <= ed->bend_count + 2u : n == ed->bend_count + 2u;
        if(!ok) break;
        const fude_zoom_graph_node from = end_of(g, ed->from, ed->from_subgraph), to = end_of(g, ed->to, ed->to_subgraph);
        const f64 a = on_outline(&from, p[s[e]]);
        const f64 b = on_outline(&to, p[s[e + 1] - 1u]);
        if(fabs(a - 1.0) > 1e-9 || fabs(b - 1.0) > 1e-9) {
            printf("  route %u: ends at %.6f and %.6f of the outline\n", e, a, b);
            ok = false;
        }
    }
    CHECK(rde_arr_length(&pts) == (s ? s[edges(g)] : 0u));
    rde_arr_free(&pts);
    rde_arr_free(&starts);
    return ok;
}

// Every subgraph's box round its nodes and its subgraphs, clear of the rest.
static b8 boxes_ok(const fude_zoom_graph* g) {
    for(u32 s = 0; s < subs(g); s++) {
        const fude_zoom_graph_subgraph* sg = fude_zoom_graph_subgraph_at(g, s);
        if(sg->w == 0.0 && sg->h == 0.0) continue;
        const f64 x0 = sg->x - sg->w * 0.5, x1 = sg->x + sg->w * 0.5, y0 = sg->y - sg->h * 0.5, y1 = sg->y + sg->h * 0.5;
        for(u32 i = 0; i < nodes(g); i++) {
            const fude_zoom_graph_node* n = fude_zoom_graph_node_at(g, i);
            b8 inside = false;
            for(u32 t = n->subgraph; t != 0; t = fude_zoom_graph_subgraph_at(g, t - 1u)->parent) inside = inside || t == s + 1u;
            const f64 a0 = n->x - n->w * 0.5, a1 = n->x + n->w * 0.5, b0 = n->y - n->h * 0.5, b1 = n->y + n->h * 0.5;
            if(inside && !(a0 >= x0 - 1e-6 && a1 <= x1 + 1e-6 && b0 >= y0 - 1e-6 && b1 <= y1 + 1e-6)) {
                printf("  %s sticks out of subgraph %s\n", n->id, sg->id);
                return false;
            }
            if(!inside && a0 < x1 - 1e-6 && a1 > x0 + 1e-6 && b0 < y1 - 1e-6 && b1 > y0 + 1e-6) {
                printf("  %s is in subgraph %s's box but not in it\n", n->id, sg->id);
                return false;
            }
        }
        if(sg->parent != 0) {
            const fude_zoom_graph_subgraph* p = fude_zoom_graph_subgraph_at(g, sg->parent - 1u);
            if(!(x0 >= p->x - p->w * 0.5 - 1e-6 && x1 <= p->x + p->w * 0.5 + 1e-6 && y0 >= p->y - p->h * 0.5 - 1e-6 && y1 <= p->y + p->h * 0.5 + 1e-6)) {
                printf("  subgraph %s sticks out of %s\n", sg->id, p->id);
                return false;
            }
        }
        for(u32 t = s + 1u; t < subs(g); t++) {
            const fude_zoom_graph_subgraph* o = fude_zoom_graph_subgraph_at(g, t);
            if(o->parent != sg->parent || (o->w == 0.0 && o->h == 0.0)) continue;
            if(fabs(o->x - sg->x) < (o->w + sg->w) * 0.5 - 1e-6 && fabs(o->y - sg->y) < (o->h + sg->h) * 0.5 - 1e-6) {
                printf("  sibling subgraphs %s and %s overlap\n", sg->id, o->id);
                return false;
            }
        }
    }
    return true;
}

// --- reading ---------------------------------------------------------------------------------

static void test_header(void) {
    CHECK(parse("flowchart TD\nA-->B") && G.direction == FUDE_ZOOM_FLOW_TB && nodes(&G) == 2 && edges(&G) == 1);
    CHECK(parse("graph LR;A-->B;") && G.direction == FUDE_ZOOM_FLOW_LR && edges(&G) == 1);
    CHECK(parse("flowchart BT\n") && G.direction == FUDE_ZOOM_FLOW_BT && nodes(&G) == 0);
    CHECK(parse("flowchart RL") && G.direction == FUDE_ZOOM_FLOW_RL);
    CHECK(parse("graph TB\nA") && G.direction == FUDE_ZOOM_FLOW_TB && nodes(&G) == 1);
    CHECK(parse("flowchart lr\nA") && G.direction == FUDE_ZOOM_FLOW_LR);
    CHECK(parse("graph\nA-->B") && G.direction == FUDE_ZOOM_FLOW_TB && edges(&G) == 1);
    CHECK(parse("\n  \n%% a note first\n\nflowchart LR\n  A") && G.direction == FUDE_ZOOM_FLOW_LR && nodes(&G) == 1);
    CHECK(parse("---\ntitle: Pasted\nconfig:\n  theme: dark\n---\nflowchart TD\nA-->B") && edges(&G) == 1);
    CHECK(parse("%%{init: {'theme': 'forest'}}%%\nflowchart TD\nA-->B") && edges(&G) == 1);
    CHECK(parse("%%{\n  init: {'theme': 'forest'}\n}%%\nflowchart TD\nA-->B\nB-->C") && edges(&G) == 2);
    CHECK(parse("flowchart TD\r\nA-->B\r\nB-->C\r\n") && edges(&G) == 2 && node(&G, "B") != NULL);
    CHECK(parse("flowchart-elk TD\nA-->B") && edges(&G) == 1);
    // Not flowcharts, and directions that are none.
    CHECK(!parse("sequenceDiagram\nA->>B: hi") && err_why == FUDE_ZOOM_MERMAID_HEADER && err_line == 1);
    CHECK(!parse("\n\nclassDiagram") && err_why == FUDE_ZOOM_MERMAID_HEADER && err_line == 3);
    CHECK(!parse("") && err_why == FUDE_ZOOM_MERMAID_HEADER);
    CHECK(!parse("flowchart XY\nA") && err_why == FUDE_ZOOM_MERMAID_DIRECTION && err_line == 1);
    CHECK(!parse("flowchart TD LR\nA") && err_why == FUDE_ZOOM_MERMAID_DIRECTION);
    CHECK(!parse("---\ntitle: never closed\nflowchart TD\n") && err_why == FUDE_ZOOM_MERMAID_HEADER);
}

static void test_shapes(void) {
    CHECK(parse("flowchart TD\n"
                "  a\n  b[box]\n  c(round)\n  d([stadium])\n  e[[sub]]\n  f[(db)]\n  g((circle))\n  h(((double)))\n"
                "  i>flag]\n  j{diamond}\n  k{{hex}}\n  l[/para/]\n  m[\\alt\\]\n  n[/trap\\]\n  o[\\trapalt/]\n"));
    CHECK(nodes(&G) == 15 && edges(&G) == 0);
    const struct { const char* id; u8 shape; const char* label; } want[] = {
        { "a", FUDE_ZOOM_NODE_RECT, "a" }, { "b", FUDE_ZOOM_NODE_RECT, "box" }, { "c", FUDE_ZOOM_NODE_ROUND, "round" },
        { "d", FUDE_ZOOM_NODE_STADIUM, "stadium" }, { "e", FUDE_ZOOM_NODE_SUBROUTINE, "sub" }, { "f", FUDE_ZOOM_NODE_CYLINDER, "db" },
        { "g", FUDE_ZOOM_NODE_CIRCLE, "circle" }, { "h", FUDE_ZOOM_NODE_DOUBLE_CIRCLE, "double" }, { "i", FUDE_ZOOM_NODE_FLAG, "flag" },
        { "j", FUDE_ZOOM_NODE_DIAMOND, "diamond" }, { "k", FUDE_ZOOM_NODE_HEXAGON, "hex" }, { "l", FUDE_ZOOM_NODE_PARALLELOGRAM, "para" },
        { "m", FUDE_ZOOM_NODE_PARALLELOGRAM_ALT, "alt" }, { "n", FUDE_ZOOM_NODE_TRAPEZOID, "trap" }, { "o", FUDE_ZOOM_NODE_TRAPEZOID_ALT, "trapalt" },
    };
    for(u32 k = 0; k < sizeof(want) / sizeof(want[0]); k++) {
        const fude_zoom_graph_node* n = node(&G, want[k].id);
        CHECK(n != NULL && n->shape == want[k].shape && strcmp(n->label, want[k].label) == 0);
        if(n && (n->shape != want[k].shape || strcmp(n->label, want[k].label) != 0)) printf("  %s: shape %u label '%s'\n", want[k].id, n->shape, n->label);
    }
    // Shapes on nodes in edges, with spaces inside.
    CHECK(parse("flowchart LR\n  A[ Start here ] --> B{ Is it? }\n  B -->|Yes| C(( OK ))") && nodes(&G) == 3 && edges(&G) == 2);
    CHECK(strcmp(node(&G, "A")->label, "Start here") == 0 && strcmp(node(&G, "B")->label, "Is it?") == 0 && node(&G, "C")->shape == FUDE_ZOOM_NODE_CIRCLE);
    // Quoted text holds brackets; entity codes, <br> and markdown strings.
    CHECK(parse("flowchart TD\n"
                "  A[\"text with (parens) and ]\"]\n"
                "  B(\"a [b] {c}\")\n"
                "  C[\"say #quot;hi#quot; #35;1 #x41; #amp; #lt;b#gt;\"]\n"
                "  D[line1<br>line2<br/>line3<BR />end]\n"
                "  E[\"`**bold** text`\"]\n"
                "  F[日本語 ✓ café]\n"
                "  G{\"multi\nline\"}\n"
                "  H[#unknown; #12 stays]\n"));
    CHECK(strcmp(node(&G, "A")->label, "text with (parens) and ]") == 0);
    CHECK(strcmp(node(&G, "B")->label, "a [b] {c}") == 0 && node(&G, "B")->shape == FUDE_ZOOM_NODE_ROUND);
    CHECK(strcmp(node(&G, "C")->label, "say \"hi\" #1 A & <b>") == 0);
    CHECK(strcmp(node(&G, "D")->label, "line1\nline2\nline3\nend") == 0);
    CHECK(strcmp(node(&G, "E")->label, "**bold** text") == 0);
    CHECK(strcmp(node(&G, "F")->label, "日本語 ✓ café") == 0);
    CHECK(strcmp(node(&G, "G")->label, "multi\nline") == 0 && node(&G, "G")->shape == FUDE_ZOOM_NODE_DIAMOND);
    CHECK(strcmp(node(&G, "H")->label, "#unknown; #12 stays") == 0);
    // Named again: without a shape it keeps what it had, with one it changes.
    CHECK(parse("flowchart TD\n  A(round) --> B\n  A --> C\n  B[New text]\n  B --> A") && nodes(&G) == 3);
    CHECK(node(&G, "A")->shape == FUDE_ZOOM_NODE_ROUND && strcmp(node(&G, "A")->label, "round") == 0);
    CHECK(node(&G, "B")->shape == FUDE_ZOOM_NODE_RECT && strcmp(node(&G, "B")->label, "New text") == 0);
    CHECK(parse("flowchart TD\n  A --> B\n  A{Decide}") && node(&G, "A")->shape == FUDE_ZOOM_NODE_DIAMOND && strcmp(node(&G, "A")->label, "Decide") == 0);
    // Ids: digits, dashes and dots between letters, any script.
    CHECK(parse("flowchart TD\n  node-1 --> node.2\n  1 --> 2\n  ノード --> 終わり") && nodes(&G) == 6 && has_edge(&G, "node-1", "node.2") && has_edge(&G, "ノード", "終わり"));
    // Text past its buffer is cut at a whole character.
    char text[1200], label[600];
    u32 at = 0;
    for(u32 k = 0; k < 200; k++) { label[at++] = (char)0xC3; label[at++] = (char)0xA9; }   // é
    label[at] = 0;
    snprintf(text, sizeof(text), "flowchart TD\n  A[%s]", label);
    CHECK(parse(text));
    CHECK(strlen(node(&G, "A")->label) == 158 && valid_utf8(node(&G, "A")->label));
    at = 0;
    for(u32 k = 0; k < 157; k++) label[at++] = 'x';
    memcpy(label + at, "日本", 6);
    label[at + 6] = 0;
    snprintf(text, sizeof(text), "flowchart TD\n  A[%s]", label);
    CHECK(parse(text) && strlen(node(&G, "A")->label) == 157 && valid_utf8(node(&G, "A")->label));
}

static void test_edges(void) {
    const struct { const char* text; u8 line, start, end; u32 length; const char* label; } want[] = {
        { "A-->B", FUDE_ZOOM_EDGE_LINE_SOLID, FUDE_ZOOM_EDGE_HEAD_NONE, FUDE_ZOOM_EDGE_HEAD_ARROW, 1, "" },
        { "A --- B", FUDE_ZOOM_EDGE_LINE_SOLID, FUDE_ZOOM_EDGE_HEAD_NONE, FUDE_ZOOM_EDGE_HEAD_NONE, 1, "" },
        { "A-.->B", FUDE_ZOOM_EDGE_LINE_DOTTED, FUDE_ZOOM_EDGE_HEAD_NONE, FUDE_ZOOM_EDGE_HEAD_ARROW, 1, "" },
        { "A -.- B", FUDE_ZOOM_EDGE_LINE_DOTTED, FUDE_ZOOM_EDGE_HEAD_NONE, FUDE_ZOOM_EDGE_HEAD_NONE, 1, "" },
        { "A==>B", FUDE_ZOOM_EDGE_LINE_THICK, FUDE_ZOOM_EDGE_HEAD_NONE, FUDE_ZOOM_EDGE_HEAD_ARROW, 1, "" },
        { "A === B", FUDE_ZOOM_EDGE_LINE_THICK, FUDE_ZOOM_EDGE_HEAD_NONE, FUDE_ZOOM_EDGE_HEAD_NONE, 1, "" },
        { "A ~~~ B", FUDE_ZOOM_EDGE_LINE_INVISIBLE, FUDE_ZOOM_EDGE_HEAD_NONE, FUDE_ZOOM_EDGE_HEAD_NONE, 1, "" },
        { "A --o B", FUDE_ZOOM_EDGE_LINE_SOLID, FUDE_ZOOM_EDGE_HEAD_NONE, FUDE_ZOOM_EDGE_HEAD_CIRCLE, 1, "" },
        { "A --x B", FUDE_ZOOM_EDGE_LINE_SOLID, FUDE_ZOOM_EDGE_HEAD_NONE, FUDE_ZOOM_EDGE_HEAD_CROSS, 1, "" },
        { "A <--> B", FUDE_ZOOM_EDGE_LINE_SOLID, FUDE_ZOOM_EDGE_HEAD_ARROW, FUDE_ZOOM_EDGE_HEAD_ARROW, 1, "" },
        { "A o--o B", FUDE_ZOOM_EDGE_LINE_SOLID, FUDE_ZOOM_EDGE_HEAD_CIRCLE, FUDE_ZOOM_EDGE_HEAD_CIRCLE, 1, "" },
        { "A x--x B", FUDE_ZOOM_EDGE_LINE_SOLID, FUDE_ZOOM_EDGE_HEAD_CROSS, FUDE_ZOOM_EDGE_HEAD_CROSS, 1, "" },
        { "A <-.-> B", FUDE_ZOOM_EDGE_LINE_DOTTED, FUDE_ZOOM_EDGE_HEAD_ARROW, FUDE_ZOOM_EDGE_HEAD_ARROW, 1, "" },
        { "A <==> B", FUDE_ZOOM_EDGE_LINE_THICK, FUDE_ZOOM_EDGE_HEAD_ARROW, FUDE_ZOOM_EDGE_HEAD_ARROW, 1, "" },
        { "A ---> B", FUDE_ZOOM_EDGE_LINE_SOLID, FUDE_ZOOM_EDGE_HEAD_NONE, FUDE_ZOOM_EDGE_HEAD_ARROW, 2, "" },
        { "A ----> B", FUDE_ZOOM_EDGE_LINE_SOLID, FUDE_ZOOM_EDGE_HEAD_NONE, FUDE_ZOOM_EDGE_HEAD_ARROW, 3, "" },
        { "A ---- B", FUDE_ZOOM_EDGE_LINE_SOLID, FUDE_ZOOM_EDGE_HEAD_NONE, FUDE_ZOOM_EDGE_HEAD_NONE, 2, "" },
        { "A -..-> B", FUDE_ZOOM_EDGE_LINE_DOTTED, FUDE_ZOOM_EDGE_HEAD_NONE, FUDE_ZOOM_EDGE_HEAD_ARROW, 2, "" },
        { "A ===> B", FUDE_ZOOM_EDGE_LINE_THICK, FUDE_ZOOM_EDGE_HEAD_NONE, FUDE_ZOOM_EDGE_HEAD_ARROW, 2, "" },
        { "A ---o B", FUDE_ZOOM_EDGE_LINE_SOLID, FUDE_ZOOM_EDGE_HEAD_NONE, FUDE_ZOOM_EDGE_HEAD_CIRCLE, 2, "" },
        { "A-->|Yes| B", FUDE_ZOOM_EDGE_LINE_SOLID, FUDE_ZOOM_EDGE_HEAD_NONE, FUDE_ZOOM_EDGE_HEAD_ARROW, 1, "Yes" },
        { "A --> | spaced | B", FUDE_ZOOM_EDGE_LINE_SOLID, FUDE_ZOOM_EDGE_HEAD_NONE, FUDE_ZOOM_EDGE_HEAD_ARROW, 1, "spaced" },
        { "A ---|open text| B", FUDE_ZOOM_EDGE_LINE_SOLID, FUDE_ZOOM_EDGE_HEAD_NONE, FUDE_ZOOM_EDGE_HEAD_NONE, 1, "open text" },
        { "A -.->|\"a | b\"| B", FUDE_ZOOM_EDGE_LINE_DOTTED, FUDE_ZOOM_EDGE_HEAD_NONE, FUDE_ZOOM_EDGE_HEAD_ARROW, 1, "a | b" },
        { "A -- some text --> B", FUDE_ZOOM_EDGE_LINE_SOLID, FUDE_ZOOM_EDGE_HEAD_NONE, FUDE_ZOOM_EDGE_HEAD_ARROW, 1, "some text" },
        { "A-- text -->B", FUDE_ZOOM_EDGE_LINE_SOLID, FUDE_ZOOM_EDGE_HEAD_NONE, FUDE_ZOOM_EDGE_HEAD_ARROW, 1, "text" },
        { "A -- a-b --- B", FUDE_ZOOM_EDGE_LINE_SOLID, FUDE_ZOOM_EDGE_HEAD_NONE, FUDE_ZOOM_EDGE_HEAD_NONE, 1, "a-b" },
        { "A -- long --->B", FUDE_ZOOM_EDGE_LINE_SOLID, FUDE_ZOOM_EDGE_HEAD_NONE, FUDE_ZOOM_EDGE_HEAD_ARROW, 2, "long" },
        { "A -. dotted .-> B", FUDE_ZOOM_EDGE_LINE_DOTTED, FUDE_ZOOM_EDGE_HEAD_NONE, FUDE_ZOOM_EDGE_HEAD_ARROW, 1, "dotted" },
        { "A -. e.g. this -.-> B", FUDE_ZOOM_EDGE_LINE_DOTTED, FUDE_ZOOM_EDGE_HEAD_NONE, FUDE_ZOOM_EDGE_HEAD_ARROW, 1, "e.g. this" },
        { "A == thick ==> B", FUDE_ZOOM_EDGE_LINE_THICK, FUDE_ZOOM_EDGE_HEAD_NONE, FUDE_ZOOM_EDGE_HEAD_ARROW, 1, "thick" },
        { "A <-- both --> B", FUDE_ZOOM_EDGE_LINE_SOLID, FUDE_ZOOM_EDGE_HEAD_ARROW, FUDE_ZOOM_EDGE_HEAD_ARROW, 1, "both" },
        { "A -- \"a -- b\" --x B", FUDE_ZOOM_EDGE_LINE_SOLID, FUDE_ZOOM_EDGE_HEAD_NONE, FUDE_ZOOM_EDGE_HEAD_CROSS, 1, "a -- b" },
        { "A-->|日本 #9829;|B", FUDE_ZOOM_EDGE_LINE_SOLID, FUDE_ZOOM_EDGE_HEAD_NONE, FUDE_ZOOM_EDGE_HEAD_ARROW, 1, "日本 ♥" },
    };
    for(u32 k = 0; k < sizeof(want) / sizeof(want[0]); k++) {
        char text[256];
        snprintf(text, sizeof(text), "flowchart TD\n  %s\n", want[k].text);
        const b8 ok = parse(text);
        CHECK(ok && edges(&G) == 1 && nodes(&G) == 2);
        if(!ok || edges(&G) != 1) { printf("  '%s': error %u line %u\n", want[k].text, err_why, err_line); continue; }
        const fude_zoom_graph_edge* e = edge(&G, 0);
        const b8 same = e->line == want[k].line && e->head_start == want[k].start && e->head_end == want[k].end &&
                        e->min_length == want[k].length && strcmp(e->label, want[k].label) == 0 &&
                        strcmp(id_of(&G, e->from), "A") == 0 && strcmp(id_of(&G, e->to), "B") == 0;
        CHECK(same);
        if(!same) printf("  '%s': line %u heads %u %u length %u label '%s'\n", want[k].text, e->line, e->head_start, e->head_end, e->min_length, e->label);
    }
    // A run of dashes past the most is held at the most.
    CHECK(parse("flowchart TD\nA ------------------------------> B") && edge(&G, 0)->min_length == FUDE_ZOOM_GRAPH_MAX_LENGTH);
    // Chains and &.
    CHECK(parse("flowchart TD\n  A --> B --> C") && edges(&G) == 2 && has_edge(&G, "A", "B") && has_edge(&G, "B", "C"));
    CHECK(parse("flowchart TD\n  A & B --> C & D") && edges(&G) == 4 && has_edge(&G, "A", "C") && has_edge(&G, "A", "D") && has_edge(&G, "B", "C") && has_edge(&G, "B", "D"));
    CHECK(parse("flowchart TD\n  A&B-->C-.->D & E[Eee]") && edges(&G) == 4 && has_edge(&G, "C", "E") && edge(&G, 3)->line == FUDE_ZOOM_EDGE_LINE_DOTTED && strcmp(node(&G, "E")->label, "Eee") == 0);
    CHECK(parse("flowchart TD\n  A-->B;B-->C;  C-->A;") && edges(&G) == 3 && has_edge(&G, "C", "A"));
    CHECK(parse("flowchart TD\n  A -->|one| B -- two --> C") && edges(&G) == 2 && strcmp(edge(&G, 0)->label, "one") == 0 && strcmp(edge(&G, 1)->label, "two") == 0);
    CHECK(parse("flowchart TD\n  A[\"x;y\"] --> B") && edges(&G) == 1 && strcmp(node(&G, "A")->label, "x;y") == 0);
}

static void test_subgraphs(void) {
    CHECK(parse("flowchart TB\n"
                "  c1-->a2\n"
                "  subgraph one\n"
                "    a1-->a2\n"
                "  end\n"
                "  subgraph two [Second one]\n"
                "    b1-->b2\n"
                "    subgraph inner [\"Inner (deep)\"]\n"
                "      direction LR\n"
                "      x\n"
                "    end\n"
                "  end\n"
                "  subgraph Title with words\n"
                "    q\n"
                "  end\n"
                "  subgraph \"Quoted title\"\n"
                "    r\n"
                "  end\n"
                "  one --> two\n"));
    CHECK(subs(&G) == 5);
    const fude_zoom_graph_subgraph* s0 = fude_zoom_graph_subgraph_at(&G, 0);
    const fude_zoom_graph_subgraph* s1 = fude_zoom_graph_subgraph_at(&G, 1);
    const fude_zoom_graph_subgraph* s2 = fude_zoom_graph_subgraph_at(&G, 2);
    const fude_zoom_graph_subgraph* s3 = fude_zoom_graph_subgraph_at(&G, 3);
    const fude_zoom_graph_subgraph* s4 = fude_zoom_graph_subgraph_at(&G, 4);
    CHECK(strcmp(s0->id, "one") == 0 && strcmp(s0->label, "one") == 0 && s0->parent == 0);
    CHECK(strcmp(s1->id, "two") == 0 && strcmp(s1->label, "Second one") == 0 && s1->parent == 0);
    CHECK(strcmp(s2->id, "inner") == 0 && strcmp(s2->label, "Inner (deep)") == 0 && s2->parent == 2);
    CHECK(strcmp(s3->id, "subGraph3") == 0 && strcmp(s3->label, "Title with words") == 0 && s3->parent == 0);
    CHECK(strcmp(s4->id, "subGraph4") == 0 && strcmp(s4->label, "Quoted title") == 0);
    CHECK(node(&G, "c1")->subgraph == 0 && node(&G, "a1")->subgraph == 1 && node(&G, "a2")->subgraph == 1);
    CHECK(node(&G, "b1")->subgraph == 2 && node(&G, "x")->subgraph == 3 && node(&G, "q")->subgraph == 4 && node(&G, "r")->subgraph == 5);
    // An edge between subgraph ids joins their boxes: the last node of one
    // stands in for it going out, the first of two coming in.
    CHECK(node(&G, "one") == NULL && node(&G, "two") == NULL && nodes(&G) == 8 && edges(&G) == 4);
    const fude_zoom_graph_edge* joined = edge(&G, 3);
    CHECK(joined->from_subgraph == 1 && joined->to_subgraph == 2 && strcmp(id_of(&G, joined->from), "a1") == 0 && strcmp(id_of(&G, joined->to), "b1") == 0);
    CHECK(edge(&G, 0)->from_subgraph == 0 && edge(&G, 0)->to_subgraph == 0 && has_edge(&G, "c1", "a2") && has_edge(&G, "b1", "b2"));
    // A node into a subgraph, and an empty subgraph's id is just a node.
    CHECK(parse("flowchart TB\n start --> box\n subgraph box [The box]\n  p --> q\n end\n subgraph hollow\n end\n q --> hollow"));
    CHECK(nodes(&G) == 4 && node(&G, "box") == NULL && node(&G, "hollow") != NULL);
    CHECK(edge(&G, 0)->to_subgraph == 1 && strcmp(id_of(&G, edge(&G, 0)->to), "p") == 0 && strcmp(id_of(&G, edge(&G, 0)->from), "start") == 0);
    CHECK(edge(&G, 2)->to_subgraph == 0 && strcmp(id_of(&G, edge(&G, 2)->to), "hollow") == 0);
    // The deepest subgraph naming a node has it; of two apart, the first.
    CHECK(parse("flowchart TB\n subgraph outer\n  x\n  subgraph inner\n   x --> y\n  end\n end\n subgraph other\n  y --> z\n end"));
    CHECK(node(&G, "x")->subgraph == 2 && node(&G, "y")->subgraph == 2 && node(&G, "z")->subgraph == 3);
    CHECK(parse("flowchart TB\n subgraph s1[\"Title\"]\n  a\n end") && strcmp(fude_zoom_graph_subgraph_at(&G, 0)->label, "Title") == 0);
    CHECK(parse("flowchart TB\n subgraph\n  a\n end") && strcmp(fude_zoom_graph_subgraph_at(&G, 0)->id, "subGraph0") == 0 && fude_zoom_graph_subgraph_at(&G, 0)->label[0] == 0);
    // "End" and "END" are nodes; "end" closes.
    CHECK(parse("flowchart TB\n subgraph s\n  End --> END\n end") && nodes(&G) == 2 && node(&G, "End")->subgraph == 1);
}

static void test_skipped(void) {
    CHECK(parse("flowchart LR\n"
                "  %% a comment\n"
                "  A[Start]:::green --> B:::red\n"
                "  classDef green fill:#9f6,stroke:#333,stroke-width:2px;\n"
                "  classDef red fill:#f66\n"
                "  class A,B green\n"
                "  style A fill:#f9f,stroke:#333,stroke-width:4px\n"
                "  linkStyle 0 stroke:#ff3,stroke-width:4px\n"
                "  click A callback \"Tooltip; with a semicolon\"\n"
                "  click B \"https://example.com\" _blank\n"
                "  accTitle: The title\n"
                "  accDescr: A description; with a semicolon\n"
                "  accDescr {\n"
                "    over\n"
                "    lines\n"
                "  }\n"
                "  subgraph s1\n"
                "    direction TB\n"
                "    C\n"
                "  end\n"
                "  B --> C %% a comment after\n"
                "  C --> D; %% and after a ;\n"));
    CHECK(nodes(&G) == 4 && edges(&G) == 3 && strcmp(node(&G, "A")->label, "Start") == 0);
    CHECK(node(&G, "classDef") == NULL && node(&G, "style") == NULL && node(&G, "click") == NULL && node(&G, "direction") == NULL);
}

static void test_errors(void) {
    const struct { const char* text; u8 why; u32 line; } want[] = {
        { "flowchart TD\nA-->B\nC[unclosed\n", FUDE_ZOOM_MERMAID_SHAPE, 3 },
        { "flowchart TD\nA-->\n", FUDE_ZOOM_MERMAID_NODE, 2 },
        { "flowchart TD\nA & --> B\n", FUDE_ZOOM_MERMAID_NODE, 2 },
        { "flowchart TD\nA B\n", FUDE_ZOOM_MERMAID_LINK, 2 },
        { "flowchart TD\nA -> B\n", FUDE_ZOOM_MERMAID_LINK, 2 },
        { "flowchart TD\nA@{ shape: rect }\n", FUDE_ZOOM_MERMAID_LINK, 2 },
        { "flowchart TD\n\n\nA-->|label B\n", FUDE_ZOOM_MERMAID_LABEL, 4 },
        { "flowchart TD\nA -- text B\n", FUDE_ZOOM_MERMAID_LABEL, 2 },
        { "flowchart TD\nA -- text ==> B\n", FUDE_ZOOM_MERMAID_LABEL, 2 },
        { "flowchart TD\nA-->B\nend\n", FUDE_ZOOM_MERMAID_END, 3 },
        { "flowchart TD\nA\nsubgraph x\nB\nsubgraph y\nC\nend\n", FUDE_ZOOM_MERMAID_OPEN, 3 },
        { "flowchart TD\nA-->B\nA[\"unclosed]\nB-->C\n", FUDE_ZOOM_MERMAID_QUOTE, 3 },
        { "flowchart TD\nA[\"closed\" but not]\n", FUDE_ZOOM_MERMAID_SHAPE, 2 },
        { "flowchart TD\nA(((x))\n", FUDE_ZOOM_MERMAID_SHAPE, 2 },
        { "flowchart TD\nA{x]\n", FUDE_ZOOM_MERMAID_SHAPE, 2 },
        { "flowchart TD\nA[\"two\nlines\" oops]\n", FUDE_ZOOM_MERMAID_SHAPE, 3 },
        { "flowchart TD\nabcdefghijabcdefghijabcdefghijabcdefghijabcdefghij --> B\n", FUDE_ZOOM_MERMAID_ID, 2 },
    };
    for(u32 k = 0; k < sizeof(want) / sizeof(want[0]); k++) {
        const b8 ok = parse(want[k].text);
        CHECK(!ok && err_why == want[k].why && err_line == want[k].line && nodes(&G) == 0 && edges(&G) == 0 && subs(&G) == 0);
        if(ok || err_why != want[k].why || err_line != want[k].line) printf("  case %u: ok %d, error %u at line %u\n", k, ok, err_why, err_line);
    }
    // Too deep, too many.
    char* text = (char*)malloc(1 << 20);
    u32 at = (u32)snprintf(text, 1u << 20, "flowchart TD\n");
    for(u32 k = 0; k <= FUDE_ZOOM_GRAPH_MAX_DEPTH; k++) at += (u32)snprintf(text + at, (1u << 20) - at, "subgraph s%u\n", k);
    CHECK(!parse(text) && err_why == FUDE_ZOOM_MERMAID_DEPTH && err_line == FUDE_ZOOM_GRAPH_MAX_DEPTH + 2u);
    at = (u32)snprintf(text, 1u << 20, "flowchart TD\n");
    for(u32 k = 0; k < 130; k++) at += (u32)snprintf(text + at, (1u << 20) - at, "%sa%u", k ? " & " : "", k);
    at += (u32)snprintf(text + at, (1u << 20) - at, " --> ");
    for(u32 k = 0; k < 130; k++) at += (u32)snprintf(text + at, (1u << 20) - at, "%sb%u", k ? " & " : "", k);
    CHECK(!parse(text) && err_why == FUDE_ZOOM_MERMAID_TOO_BIG && nodes(&G) == 0);
    at = (u32)snprintf(text, 1u << 20, "flowchart TD\n");
    for(u32 k = 0; k <= FUDE_ZOOM_GRAPH_MAX_NODES; k++) at += (u32)snprintf(text + at, (1u << 20) - at, "n%u\n", k);
    CHECK(!parse(text) && err_why == FUDE_ZOOM_MERMAID_TOO_BIG && err_line == FUDE_ZOOM_GRAPH_MAX_NODES + 2u);
    free(text);
    // NULL outs are fine; a good parse after a bad one starts clean.
    CHECK(!fude_zoom_graph_parse_mermaid(&G, "nope", 4, NULL, NULL));
    CHECK(parse("flowchart TD\nA-->B") && err_line == 0 && err_why == FUDE_ZOOM_MERMAID_OK && nodes(&G) == 2);
    // Only _size bytes are read.
    CHECK(fude_zoom_graph_parse_mermaid(&G, "flowchart TD\nA-->B garbage", 18, NULL, NULL) && edges(&G) == 1);
}

static void test_building(void) {
    fude_zoom_graph g;
    fude_zoom_graph_init(&g);
    const u32 a = fude_zoom_graph_add_node(&g, "a", "Alpha", FUDE_ZOOM_NODE_DIAMOND);
    const u32 b = fude_zoom_graph_add_node(&g, "b", NULL, FUDE_ZOOM_NODE_RECT);
    CHECK(a == 0 && b == 1 && fude_zoom_graph_add_node(&g, "a", NULL, 0) == FUDE_ZOOM_GRAPH_NONE);
    CHECK(strcmp(fude_zoom_graph_node_at(&g, b)->label, "b") == 0 && fude_zoom_graph_find(&g, "b") == 1 && fude_zoom_graph_find(&g, "c") == FUDE_ZOOM_GRAPH_NONE);
    CHECK(fude_zoom_graph_add_edge(&g, a, b) == 0 && fude_zoom_graph_add_edge(&g, a, 7) == FUDE_ZOOM_GRAPH_NONE);
    CHECK(edge(&g, 0)->head_end == FUDE_ZOOM_EDGE_HEAD_ARROW && edge(&g, 0)->min_length == 1);
    fude_zoom_graph_clear(&g);
    CHECK(nodes(&g) == 0 && edges(&g) == 0);
    fude_zoom_graph_layout(&g, SP);   // nothing to lay out
    fude_zoom_graph_destroy(&g);
}

// --- laying out ------------------------------------------------------------------------------

static const char* FLOW =
    "flowchart TD\n"
    "  A[Start] --> B{Choose}\n"
    "  B -->|one| C[First way]\n"
    "  B -->|two| D[Second way]\n"
    "  C --> E((Join))\n"
    "  D --> F[More] --> G[Still more] --> E\n"
    "  A --> E\n"
    "  E --> H([End])\n"
    "  B --> I[[Side]]\n"
    "  I --> J[(Store)]\n"
    "  J --> H\n";

static void layout_text(fude_zoom_graph* g, const char* text, u8 direction) {
    CHECK(fude_zoom_graph_parse_mermaid(g, text, (u32)strlen(text), NULL, NULL));
    g->direction = direction;
    size_all(g, 60.0, 36.0);
    for(u32 i = 0; i < nodes(g); i++) {
        fude_zoom_graph_node* n = fude_zoom_graph_node_at(g, i);
        if(n->shape == FUDE_ZOOM_NODE_CIRCLE) n->w = n->h = 48.0;
    }
    fude_zoom_graph_layout(g, SP);
}

static void test_layout_basic(void) {
    layout_text(&G, FLOW, FUDE_ZOOM_FLOW_TB);
    CHECK(no_overlaps(&G));
    CHECK(against(&G) == 0);
    CHECK(routes_ok(&G));
    // The long edge A --> E crosses ranks: a bend at each; between its ends.
    const fude_zoom_graph_node *a = node(&G, "A"), *e = node(&G, "E"), *h = node(&G, "H");
    u32 ae = FUDE_ZOOM_GRAPH_NONE;
    for(u32 k = 0; k < edges(&G); k++) if(edge(&G, k)->from == fude_zoom_graph_find(&G, "A") && edge(&G, k)->to == fude_zoom_graph_find(&G, "E")) ae = k;
    CHECK(ae != FUDE_ZOOM_GRAPH_NONE && edge(&G, ae)->bend_count >= 2);
    const fude_zoom_v2* bends = (const fude_zoom_v2*)G.bends.memory;
    for(u32 k = 0; ae != FUDE_ZOOM_GRAPH_NONE && k < edge(&G, ae)->bend_count; k++) {
        const fude_zoom_v2 p = bends[edge(&G, ae)->bend_first + k];
        CHECK(p.y > a->y && p.y < e->y);
        if(k > 0) CHECK(p.y > bends[edge(&G, ae)->bend_first + k - 1].y);
    }
    CHECK(h->y > e->y);
    // Ranks: same rank, same centre line; the corner at 0, 0.
    CHECK(fabs(node(&G, "C")->y - node(&G, "D")->y) < 1e-9);
    f64 lx = 1e300, ly = 1e300;
    for(u32 i = 0; i < nodes(&G); i++) {
        const fude_zoom_graph_node* n = fude_zoom_graph_node_at(&G, i);
        lx = fmin(lx, n->x - n->w * 0.5);
        ly = fmin(ly, n->y - n->h * 0.5);
        CHECK(n->placed);
    }
    CHECK(fabs(lx) < 1e-9 && fabs(ly) < 1e-9);
    // Rank gaps: at least rank_gap between a node and the next rank's.
    CHECK(node(&G, "B")->y - node(&G, "A")->y >= (node(&G, "A")->h + node(&G, "B")->h) * 0.5 + SP.rank_gap - 1e-9);
    // Labels without a size: halfway along their line.
    for(u32 k = 0; k < edges(&G); k++) CHECK(isfinite(edge(&G, k)->label_x) && isfinite(edge(&G, k)->label_y));
    // A tree's parent over its children: B between C and D.
    CHECK(node(&G, "B")->x > fmin(node(&G, "C")->x, node(&G, "D")->x) && node(&G, "B")->x < fmax(node(&G, "C")->x, node(&G, "I")->x) + 1e-9);
}

static void test_layout_directions(void) {
    // Square nodes: LR is TB turned exactly, BT and RL its mirrors.
    fude_zoom_graph g[4];
    const u8 dirs[4] = { FUDE_ZOOM_FLOW_TB, FUDE_ZOOM_FLOW_LR, FUDE_ZOOM_FLOW_BT, FUDE_ZOOM_FLOW_RL };
    for(u32 k = 0; k < 4; k++) {
        fude_zoom_graph_init(&g[k]);
        CHECK(fude_zoom_graph_parse_mermaid(&g[k], FLOW, (u32)strlen(FLOW), NULL, NULL));
        g[k].direction = dirs[k];
        for(u32 i = 0; i < nodes(&g[k]); i++) fude_zoom_graph_node_at(&g[k], i)->w = fude_zoom_graph_node_at(&g[k], i)->h = 40.0;
        fude_zoom_graph_layout(&g[k], SP);
        CHECK(no_overlaps(&g[k]));
        CHECK(against(&g[k]) == 0);
        CHECK(routes_ok(&g[k]));
    }
    f64 tall = 0.0, wide = 0.0;
    for(u32 i = 0; i < nodes(&g[0]); i++) {
        tall = fmax(tall, fude_zoom_graph_node_at(&g[0], i)->y + 20.0);
        wide = fmax(wide, fude_zoom_graph_node_at(&g[1], i)->x + 20.0);
    }
    b8 turned = true, flipped = true, mirrored = true;
    for(u32 i = 0; i < nodes(&g[0]); i++) {
        const fude_zoom_graph_node *tb = fude_zoom_graph_node_at(&g[0], i), *lr = fude_zoom_graph_node_at(&g[1], i);
        const fude_zoom_graph_node *bt = fude_zoom_graph_node_at(&g[2], i), *rl = fude_zoom_graph_node_at(&g[3], i);
        turned   = turned && fabs(lr->x - tb->y) < 1e-9 && fabs(lr->y - tb->x) < 1e-9;
        flipped  = flipped && fabs(bt->x - tb->x) < 1e-9 && fabs(bt->y - (tall - tb->y)) < 1e-9;
        mirrored = mirrored && fabs(rl->x - (wide - lr->x)) < 1e-9 && fabs(rl->y - lr->y) < 1e-9;
    }
    CHECK(turned);
    CHECK(flipped);
    CHECK(mirrored);
    // Not square: LR keeps the node's own width along the flow.
    fude_zoom_graph_clear(&g[1]);
    layout_text(&g[1], FLOW, FUDE_ZOOM_FLOW_LR);
    CHECK(no_overlaps(&g[1]) && against(&g[1]) == 0 && routes_ok(&g[1]));
    CHECK(node(&g[1], "B")->x - node(&g[1], "A")->x >= (node(&g[1], "A")->w + node(&g[1], "B")->w) * 0.5 + SP.rank_gap - 1e-9);
    for(u32 k = 0; k < 4; k++) fude_zoom_graph_destroy(&g[k]);
}

static void test_layout_determinism(void) {
    fude_zoom_graph a, b;
    fude_zoom_graph_init(&a);
    fude_zoom_graph_init(&b);
    layout_text(&a, FLOW, FUDE_ZOOM_FLOW_TB);
    layout_text(&b, FLOW, FUDE_ZOOM_FLOW_TB);
    b8 same = nodes(&a) == nodes(&b) && rde_arr_length(&a.bends) == rde_arr_length(&b.bends);
    for(u32 i = 0; same && i < nodes(&a); i++) {
        same = fude_zoom_graph_node_at(&a, i)->x == fude_zoom_graph_node_at(&b, i)->x && fude_zoom_graph_node_at(&a, i)->y == fude_zoom_graph_node_at(&b, i)->y;
    }
    same = same && memcmp(a.bends.memory, b.bends.memory, rde_arr_length(&a.bends) * sizeof(fude_zoom_v2)) == 0;
    CHECK(same);
    // Laid out again as it is: everything stays where it was.
    fude_zoom_graph_layout(&b, SP);
    for(u32 i = 0; i < nodes(&a); i++) {
        CHECK(fabs(fude_zoom_graph_node_at(&a, i)->x - fude_zoom_graph_node_at(&b, i)->x) < 1e-6 && fabs(fude_zoom_graph_node_at(&a, i)->y - fude_zoom_graph_node_at(&b, i)->y) < 1e-6);
    }
    fude_zoom_graph_destroy(&a);
    fude_zoom_graph_destroy(&b);
}

static void test_layout_cycles_and_more(void) {
    // A cycle: one edge turned back, the rest go down.
    layout_text(&G, "flowchart TD\n A --> B --> C --> A\n C --> D", FUDE_ZOOM_FLOW_TB);
    CHECK(against(&G) == 1 && no_overlaps(&G) && routes_ok(&G));
    CHECK(node(&G, "A")->y < node(&G, "B")->y && node(&G, "B")->y < node(&G, "C")->y);
    // A long back edge bends, from its own `from` to its `to`.
    layout_text(&G, "flowchart TD\n A --> B --> C --> D --> A", FUDE_ZOOM_FLOW_TB);
    const fude_zoom_graph_edge* back = edge(&G, 3);
    CHECK(back->bend_count == 2 && routes_ok(&G));
    const fude_zoom_v2* bends = (const fude_zoom_v2*)G.bends.memory;
    CHECK(bends[back->bend_first].y > bends[back->bend_first + 1].y);   // from D (low) up to A
    // A self-loop: two bends out of its side.
    layout_text(&G, "flowchart TD\n A --> A\n A --> B", FUDE_ZOOM_FLOW_TB);
    CHECK(edge(&G, 0)->bend_count == 2 && routes_ok(&G) && against(&G) == 0);
    CHECK(fude_zoom_graph_find(&G, "A") == 0 && ((const fude_zoom_v2*)G.bends.memory)[edge(&G, 0)->bend_first].x > node(&G, "A")->x + node(&G, "A")->w * 0.5);
    // Two edges between the same nodes are bent apart.
    layout_text(&G, "flowchart TD\n A --> B\n A --> B\n B --> A", FUDE_ZOOM_FLOW_TB);
    CHECK(edge(&G, 0)->bend_count == 0 && edge(&G, 1)->bend_count == 1 && edge(&G, 2)->bend_count == 1 && routes_ok(&G));
    // ---> is two ranks: one bend.
    layout_text(&G, "flowchart TD\n A ---> B\n A --> C", FUDE_ZOOM_FLOW_TB);
    CHECK(edge(&G, 0)->bend_count == 1 && node(&G, "B")->y > node(&G, "C")->y + 1.0 && no_overlaps(&G));
    // No edges: one rank. Apart: side by side.
    layout_text(&G, "flowchart LR\n A\n B\n C[A wider one]\n D", FUDE_ZOOM_FLOW_LR);
    CHECK(no_overlaps(&G) && node(&G, "A")->x == node(&G, "D")->x);
    layout_text(&G, "flowchart TD\n A --> B\n C --> D\n E", FUDE_ZOOM_FLOW_TB);
    CHECK(no_overlaps(&G) && against(&G) == 0);
    // A source joins what it feeds instead of waiting at the top.
    layout_text(&G, "flowchart TD\n A --> B --> C --> D\n X --> D", FUDE_ZOOM_FLOW_TB);
    CHECK(fabs(node(&G, "X")->y - node(&G, "C")->y) < 1e-9);
    // Every shape's route meets its outline.
    layout_text(&G, "flowchart LR\n a((c)) --> b{d} --> c[r] --> d(((dc))) --> e{{h}}\n a --> c\n b --> e", FUDE_ZOOM_FLOW_LR);
    CHECK(routes_ok(&G) && no_overlaps(&G));
}

static void test_layout_labels(void) {
    layout_text(&G, FLOW, FUDE_ZOOM_FLOW_TB);
    for(u32 k = 0; k < edges(&G); k++) {
        if(edge(&G, k)->label[0]) { edge(&G, k)->label_w = 40.0; edge(&G, k)->label_h = 18.0; }
    }
    fude_zoom_graph_layout(&G, SP);
    CHECK(no_overlaps(&G) && against(&G) == 0 && routes_ok(&G));
    // A label with room kept sits on its edge's line, clear of every node.
    for(u32 k = 0; k < edges(&G); k++) {
        const fude_zoom_graph_edge* e = edge(&G, k);
        if(!e->label[0]) continue;
        CHECK(e->bend_count >= 1);
        const fude_zoom_graph_node *a = fude_zoom_graph_node_at(&G, e->from), *b = fude_zoom_graph_node_at(&G, e->to);
        CHECK(e->label_y > a->y && e->label_y < b->y);
        for(u32 i = 0; i < nodes(&G); i++) {
            const fude_zoom_graph_node* n = fude_zoom_graph_node_at(&G, i);
            CHECK(!(fabs(n->x - e->label_x) < (n->w + e->label_w) * 0.5 && fabs(n->y - e->label_y) < (n->h + e->label_h) * 0.5));
        }
        b8 on = false;
        const fude_zoom_v2* bends = (const fude_zoom_v2*)G.bends.memory;
        for(u32 j = 0; j < e->bend_count; j++) on = on || (bends[e->bend_first + j].x == e->label_x && bends[e->bend_first + j].y == e->label_y);
        CHECK(on);
    }
}

static void test_layout_subgraphs(void) {
    static const char* text =
        "flowchart TB\n"
        "  c1-->a2\n"
        "  subgraph one [The first]\n"
        "    a1-->a2\n"
        "  end\n"
        "  subgraph two\n"
        "    b1-->b2\n"
        "    subgraph three\n"
        "      x1 --> x2\n"
        "    end\n"
        "    b2 --> x2\n"
        "  end\n"
        "  subgraph four\n"
        "    c2\n"
        "  end\n"
        "  c1 --> b1\n"
        "  a2 --> c2\n"
        "  x2 --> c3\n"
        "  c1 --> c3\n"
        "  a1 --> x1\n";
    const u8 dirs[4] = { FUDE_ZOOM_FLOW_TB, FUDE_ZOOM_FLOW_LR, FUDE_ZOOM_FLOW_BT, FUDE_ZOOM_FLOW_RL };
    for(u32 k = 0; k < 4; k++) {
        layout_text(&G, text, dirs[k]);
        CHECK(no_overlaps(&G) && against(&G) == 0 && routes_ok(&G));
        CHECK(boxes_ok(&G));
        for(u32 s = 0; s < subs(&G); s++) CHECK(fude_zoom_graph_subgraph_at(&G, s)->w > 0.0 && fude_zoom_graph_subgraph_at(&G, s)->h > 0.0);
        // The title's room at the top of the page: more above the first members than below the last.
        const fude_zoom_graph_subgraph* four = fude_zoom_graph_subgraph_at(&G, 3);
        const fude_zoom_graph_node* c2 = node(&G, "c2");
        CHECK(fabs((c2->y - c2->h * 0.5) - (four->y - four->h * 0.5) - (SP.subgraph_pad + SP.subgraph_title)) < 1e-6);
        CHECK(fabs((four->y + four->h * 0.5) - (c2->y + c2->h * 0.5) - SP.subgraph_pad) < 1e-6);
    }
    // Edges joining boxes: drawn from box to box.
    layout_text(&G, "flowchart TB\n start --> one\n subgraph one\n a1 --> a2\n end\n subgraph two\n b1 --> b2 --> b3\n end\n one --> two\n two --> finish\n a1 --> b3", FUDE_ZOOM_FLOW_TB);
    CHECK(nodes(&G) == 7 && no_overlaps(&G) && boxes_ok(&G) && routes_ok(&G) && against(&G) == 0);
    CHECK(fude_zoom_graph_subgraph_at(&G, 1)->y > fude_zoom_graph_subgraph_at(&G, 0)->y);
    // An empty subgraph has no box.
    layout_text(&G, "flowchart TB\n subgraph empty\n end\n A --> B", FUDE_ZOOM_FLOW_TB);
    CHECK(fude_zoom_graph_subgraph_at(&G, 0)->w == 0.0 && no_overlaps(&G));
}

static void test_layout_pinned(void) {
    layout_text(&G, FLOW, FUDE_ZOOM_FLOW_TB);
    // Pin D somewhere new, right where C is: it stays, the rest keeps clear.
    fude_zoom_graph_node* d = node(&G, "D");
    d->pinned = true;
    d->x = node(&G, "C")->x;
    d->y = node(&G, "C")->y + 5.0;
    const f64 dx = d->x, dy = d->y;
    fude_zoom_graph_layout(&G, SP);
    CHECK(node(&G, "D")->x == dx && node(&G, "D")->y == dy);
    CHECK(no_overlaps(&G));
    CHECK(routes_ok(&G));
    // The ranks line up on a pinned node: A's rank is where A is, the next a
    // rank on.
    node(&G, "D")->pinned = false;
    fude_zoom_graph_node* a = node(&G, "A");
    a->pinned = true;
    a->x = 100.0;
    a->y = 100.0;
    fude_zoom_graph_layout(&G, SP);
    CHECK(node(&G, "A")->x == 100.0 && node(&G, "A")->y == 100.0);
    CHECK(fabs(node(&G, "B")->y - (100.0 + (node(&G, "A")->h + node(&G, "B")->h) * 0.5 + SP.rank_gap)) < 1e-9);
    CHECK(fabs(node(&G, "B")->x - 100.0) < 60.0 && no_overlaps(&G));
    // Two far apart (they disagree: the ranks follow one of them).
    node(&G, "D")->pinned = true;
    node(&G, "D")->x = 900.0;
    node(&G, "D")->y = -400.0;
    fude_zoom_graph_layout(&G, SP);
    CHECK(node(&G, "D")->x == 900.0 && node(&G, "D")->y == -400.0 && node(&G, "A")->x == 100.0 && node(&G, "A")->y == 100.0);
    CHECK(no_overlaps(&G) && routes_ok(&G));
    // Pinned in a crowded rank: every other node goes round it.
    layout_text(&G, "flowchart TD\n R --> A & B & C & D & E & F\n", FUDE_ZOOM_FLOW_TB);
    fude_zoom_graph_node* p = node(&G, "R");
    p->pinned = true;
    p->x = node(&G, "C")->x;
    p->y = node(&G, "C")->y;
    p->w = 200.0;
    fude_zoom_graph_layout(&G, SP);
    CHECK(no_overlaps(&G));
    // Pinned in subgraphs: still no overlaps.
    layout_text(&G, "flowchart TB\n subgraph s\n a --> b\n end\n b --> c --> d\n", FUDE_ZOOM_FLOW_TB);
    node(&G, "c")->pinned = true;
    node(&G, "c")->x = node(&G, "b")->x;
    node(&G, "c")->y = node(&G, "b")->y;
    fude_zoom_graph_layout(&G, SP);
    CHECK(no_overlaps(&G) && routes_ok(&G));
}

// The relative order across each rank of every node there, before and after.
static b8 order_kept(const fude_zoom_graph* before, const fude_zoom_graph* after) {
    for(u32 i = 0; i < nodes(before); i++) {
        for(u32 j = 0; j < nodes(before); j++) {
            const fude_zoom_graph_node *a0 = fude_zoom_graph_node_at(before, i), *b0 = fude_zoom_graph_node_at(before, j);
            const fude_zoom_graph_node *a1 = fude_zoom_graph_node_at(after, i), *b1 = fude_zoom_graph_node_at(after, j);
            if(i == j || a0->subgraph != b0->subgraph) continue;
            if(fabs(a0->y - b0->y) < 1e-9 && fabs(a1->y - b1->y) < 1e-9 && a0->x < b0->x && !(a1->x < b1->x)) {
                printf("  %s and %s swapped\n", a0->id, b0->id);
                return false;
            }
        }
    }
    return true;
}

static void test_layout_stable(void) {
    static const char* ten =
        "flowchart TD\n"
        "  A --> B & C & D\n"
        "  B --> E & F\n"
        "  C --> G\n"
        "  D --> H & I\n"
        "  E --> J\n"
        "  G --> J\n"
        "  I --> F\n";
    fude_zoom_graph before, after;
    fude_zoom_graph_init(&before);
    fude_zoom_graph_init(&after);
    layout_text(&before, ten, FUDE_ZOOM_FLOW_TB);
    CHECK(nodes(&before) == 10);
    const char* parents[] = { "A", "B", "C", "D", "E", "F", "G", "H", "I", "J" };
    for(u32 k = 0; k < 10; k++) {
        layout_text(&after, ten, FUDE_ZOOM_FLOW_TB);
        // A leaf on each node in turn: no existing node swaps in any rank.
        const u32 leaf = fude_zoom_graph_add_node(&after, "leaf", "New leaf", FUDE_ZOOM_NODE_RECT);
        CHECK(leaf == 10 && fude_zoom_graph_add_edge(&after, fude_zoom_graph_find(&after, parents[k]), leaf) != FUDE_ZOOM_GRAPH_NONE);
        fude_zoom_graph_node_at(&after, leaf)->w = 80.0;
        fude_zoom_graph_node_at(&after, leaf)->h = 36.0;
        fude_zoom_graph_layout(&after, SP);
        CHECK(order_kept(&before, &after));
        CHECK(no_overlaps(&after) && against(&after) == 0);
        // And the ranks stay: every old node on its old rank's line.
        for(u32 i = 0; i < 10; i++) {
            for(u32 j = 0; j < 10; j++) {
                const b8 was = fabs(fude_zoom_graph_node_at(&before, i)->y - fude_zoom_graph_node_at(&before, j)->y) < 1e-9;
                const b8 is  = fabs(fude_zoom_graph_node_at(&after, i)->y - fude_zoom_graph_node_at(&after, j)->y) < 1e-9;
                CHECK(was == is);
            }
        }
    }
    // Random graphs, a random leaf (or a node and two edges) added.
    for(u32 seed = 1; seed <= 60; seed++) {
        rng = seed * 2654435761u;
        fude_zoom_graph_clear(&before);
        const u32 n = 10u + rnd() % 15u;
        char id[16];
        for(u32 i = 0; i < n; i++) { snprintf(id, sizeof(id), "n%u", i); fude_zoom_graph_add_node(&before, id, NULL, (u8)(rnd() % FUDE_ZOOM_NODE_COUNT)); }
        for(u32 k = 0; k < n + n / 2u; k++) {
            const u32 a = rnd() % n, b = rnd() % n;
            if(a != b) fude_zoom_graph_add_edge(&before, a < b ? a : b, a < b ? b : a);
        }
        size_all(&before, 50.0, 30.0);
        fude_zoom_graph_layout(&before, SP);
        CHECK(no_overlaps(&before) && against(&before) == 0 && routes_ok(&before));
        // Copy it, add, lay out again.
        fude_zoom_graph_clear(&after);
        for(u32 i = 0; i < n; i++) rde_arr_add(&after.nodes, (any)fude_zoom_graph_node_at(&before, i));
        for(u32 e = 0; e < edges(&before); e++) rde_arr_add(&after.edges, (any)edge(&before, e));
        const u32 leaf = fude_zoom_graph_add_node(&after, "new", NULL, FUDE_ZOOM_NODE_RECT);
        fude_zoom_graph_node_at(&after, leaf)->w = 70.0;
        fude_zoom_graph_node_at(&after, leaf)->h = 30.0;
        fude_zoom_graph_add_edge(&after, rnd() % n, leaf);
        fude_zoom_graph_layout(&after, SP);
        CHECK(order_kept(&before, &after));
        CHECK(no_overlaps(&after) && against(&after) == 0 && routes_ok(&after));
    }
    fude_zoom_graph_destroy(&before);
    fude_zoom_graph_destroy(&after);
}

// Random graphs with subgraphs, labels, cycles and long edges: the layout's
// promises hold in every one.
static void test_layout_random(void) {
    for(u32 seed = 1; seed <= 80; seed++) {
        rng = seed * 40503u + 7u;
        fude_zoom_graph_clear(&G);
        G.direction = (u8)(seed % 4u);
        const u32 n = 5u + rnd() % 45u, ns = rnd() % 6u;
        char id[16];
        for(u32 s = 0; s < ns; s++) {
            fude_zoom_graph_subgraph sg;
            memset(&sg, 0, sizeof(sg));
            snprintf(sg.id, sizeof(sg.id), "s%u", s);
            sg.parent = s > 0 && (rnd() & 1u) ? 1u + rnd() % s : 0u;
            rde_arr_add(&G.subgraphs, (any)&sg);
        }
        for(u32 i = 0; i < n; i++) {
            snprintf(id, sizeof(id), "n%u", i);
            const u32 v = fude_zoom_graph_add_node(&G, id, NULL, (u8)(rnd() % FUDE_ZOOM_NODE_COUNT));
            fude_zoom_graph_node_at(&G, v)->subgraph = ns > 0 && rnd() % 3u ? 1u + rnd() % ns : 0u;
        }
        const u32 ne = rnd() % (n * 2u);
        for(u32 k = 0; k < ne; k++) {
            const u32 e = fude_zoom_graph_add_edge(&G, rnd() % n, rnd() % n);
            edge(&G, e)->min_length = 1u + (rnd() % 7u == 0 ? rnd() % 3u : 0u);
            if(rnd() % 4u == 0) { edge(&G, e)->label_w = 30.0; edge(&G, e)->label_h = 14.0; }
        }
        size_all(&G, 40.0 + (f64)(rnd() % 40u), 24.0 + (f64)(rnd() % 20u));
        fude_zoom_graph_layout(&G, SP);
        const b8 ok = no_overlaps(&G) && routes_ok(&G) && boxes_ok(&G);
        CHECK(ok);
        if(!ok) printf("  seed %u (%u nodes, %u edges, %u subgraphs, direction %u)\n", seed, n, ne, ns, G.direction);
        // Determinism, once more, on these.
        fude_zoom_graph h;
        fude_zoom_graph_init(&h);
        for(u32 s = 0; s < ns; s++) rde_arr_add(&h.subgraphs, (any)fude_zoom_graph_subgraph_at(&G, s));
        for(u32 i = 0; i < n; i++) { fude_zoom_graph_node c = *fude_zoom_graph_node_at(&G, i); c.placed = false; c.x = c.y = 0; rde_arr_add(&h.nodes, (any)&c); }
        for(u32 e = 0; e < ne; e++) rde_arr_add(&h.edges, (any)edge(&G, e));
        h.direction = G.direction;
        fude_zoom_graph_layout(&h, SP);
        fude_zoom_graph_clear(&G);
        b8 same = true;
        fude_zoom_graph h2;
        fude_zoom_graph_init(&h2);
        for(u32 s = 0; s < ns; s++) rde_arr_add(&h2.subgraphs, (any)fude_zoom_graph_subgraph_at(&h, s));
        for(u32 i = 0; i < n; i++) { fude_zoom_graph_node c = *fude_zoom_graph_node_at(&h, i); c.placed = false; c.x = c.y = 0; rde_arr_add(&h2.nodes, (any)&c); }
        for(u32 e = 0; e < ne; e++) rde_arr_add(&h2.edges, (any)edge(&h, e));
        h2.direction = h.direction;
        fude_zoom_graph_layout(&h2, SP);
        for(u32 i = 0; i < n; i++) same = same && fude_zoom_graph_node_at(&h, i)->x == fude_zoom_graph_node_at(&h2, i)->x && fude_zoom_graph_node_at(&h, i)->y == fude_zoom_graph_node_at(&h2, i)->y;
        CHECK(same);
        fude_zoom_graph_destroy(&h);
        fude_zoom_graph_destroy(&h2);
    }
    // A big one, to see it is quick enough and still sound.
    fude_zoom_graph_clear(&G);
    char id[16];
    for(u32 i = 0; i < 600; i++) { snprintf(id, sizeof(id), "n%u", i); fude_zoom_graph_add_node(&G, id, NULL, FUDE_ZOOM_NODE_RECT); }
    rng = 99u;
    for(u32 k = 0; k < 900; k++) { const u32 a = rnd() % 600u, b = rnd() % 600u; fude_zoom_graph_add_edge(&G, a, b); }
    size_all(&G, 50.0, 30.0);
    fude_zoom_graph_layout(&G, SP);
    CHECK(no_overlaps(&G) && routes_ok(&G));
}

int main(void) {
    fude_zoom_graph_init(&G);
    test_header();
    test_shapes();
    test_edges();
    test_subgraphs();
    test_skipped();
    test_errors();
    test_building();
    test_layout_basic();
    test_layout_directions();
    test_layout_determinism();
    test_layout_cycles_and_more();
    test_layout_labels();
    test_layout_subgraphs();
    test_layout_pinned();
    test_layout_stable();
    test_layout_random();
    fude_zoom_graph_destroy(&G);
    printf(fails ? "%d FAILURES\n" : "ALL PASSED\n", fails);
    return fails != 0;
}
