// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_SYMBOL
#define FUDE_ZOOM_SYMBOL

#include "rde.h"
#include "zoom/zoom.h"

// ===========================================================================
// Symbol: the diagram library (Insert, Diagram shapes), the shapes and icons diagrams are drawn with:
// flowcharts, UML, sequence and activity diagrams, data and ER, C4 and architecture, BPMN, planning boards, and the
// parts of circuits, mechanisms, floor plans, maths, sewing, science and more.
//
// Where it sits: zoom, the canvas apps' layer. A SYMBOL shape (shape.h) is drawn with it by the renderer, the exports
// and the page, and the simulations' parts (circuit_catalogue.c, logic.c, mech.c, plan.c, chart.c, plot.c, map.c and
// others) are symbols too. symbolart.h can replace a symbol's drawing with the app's own art.
// Its main types and their lifecycle: the symbols are a constant table compiled into symbol.c, so there is nothing to
// create or free. The functions write into arrays the caller creates and frees.
// Threads: called on the main thread. The art hook (fude_zoom_symbol_set_art) is a global, set once by symbolart.c
//   when the page is first set up (fude_zoom_page_init).
// Files: none of its own, but a symbol's kind is its place in the table, and canvases keep that number. The table is
//   therefore append-only: a new symbol goes at its end, and none is ever removed or moved.
// Design:
//   The shapes follow the best-known tools (draw.io, Lucidchart, Visio, Miro, Excalidraw's libraries, Mermaid,
//   PlantUML, the UML and BPMN notations, the C4 model), as generic shapes: never a brand's logo.
//   A SYMBOL shape keeps only which symbol it is, its half sizes and its text. Each symbol is drawn from those as
//   PARTS: polylines in its own units, around its middle, y up. The first part is its outline (what fills, what a
//   connector and the lasso meet, what snaps); the rest are the lines inside it or on it (a class's compartments, a
//   cylinder's rim, an actor's limbs, a gateway's mark). Most are drawn in the box's own proportions, but an icon's
//   round bits are drawn from the smaller half size, so they stay round however the box is stretched.
//   Its text goes in its TEXT BOXES. Most symbols have one; a class has three (name, attributes, operations), its
//   text split into parts at lines that are exactly "---"; a person's text goes under it.
//   See docs/infinite_canvas_design.md §21.
// ===========================================================================

// The library's families, one tab each in the library's panel.
typedef enum {
    FUDE_ZOOM_SYMBOL_FAMILY_FLOW = 0,     // flowchart and basic shapes
    FUDE_ZOOM_SYMBOL_FAMILY_UML,          // UML structure: classes, packages, components, use cases
    FUDE_ZOOM_SYMBOL_FAMILY_BEHAVIOUR,    // UML behaviour: sequence, activity, state
    FUDE_ZOOM_SYMBOL_FAMILY_DATA,         // data, ER, tables
    FUDE_ZOOM_SYMBOL_FAMILY_ARCHITECTURE, // C4, servers, services, cloud, network
    // BPMN's events, tasks, gateways, data and pools.
    FUDE_ZOOM_SYMBOL_FAMILY_BPMN,
    FUDE_ZOOM_SYMBOL_FAMILY_PLANNING,     // Kanban columns and cards, swimlanes, milestones
    FUDE_ZOOM_SYMBOL_FAMILY_ELECTRONICS,  // circuit.h's parts: passive, sources, semiconductors, a breadboard
    FUDE_ZOOM_SYMBOL_FAMILY_LOGIC,        // circuit parts too: logic gates, flip-flops, inputs and probes
    FUDE_ZOOM_SYMBOL_FAMILY_CHIPS,        // circuit parts too: chips and modules of hobby projects
    FUDE_ZOOM_SYMBOL_FAMILY_BOARDS,       // circuit parts too: Arduino, Raspberry Pi, ESP boards
    FUDE_ZOOM_SYMBOL_FAMILY_MECHANISMS,   // mech.h's parts: links, gears, pivots, motors, springs, weights
    FUDE_ZOOM_SYMBOL_FAMILY_FLOORPLAN,    // plan.h's parts: doors, windows, furniture, fittings (at true size)
    FUDE_ZOOM_SYMBOL_FAMILY_WIRING,       // plan.h's too: house wiring's symbols
    FUDE_ZOOM_SYMBOL_FAMILY_MATHS,        // plot.h's: a graph, axes, a number line (drawn from their text)
    FUDE_ZOOM_SYMBOL_FAMILY_SEWING,       // a pattern's marks: grainline, fold, notches, buttons, darts, labels
    FUDE_ZOOM_SYMBOL_FAMILY_SHAPES,       // plain shapes: polygons, stars, arcs, clouds, bubbles, block arrows (0.1.85)
    FUDE_ZOOM_SYMBOL_FAMILY_BRACKETS,     // ( ) [ ] { } ⟨ ⟩ | ‖ ⌊ ⌋ ⌈ ⌉ over what they group, and pairs round a text
    FUDE_ZOOM_SYMBOL_FAMILY_CHARTS,       // bar, line and pie charts drawn from their data (chart.h)
    FUDE_ZOOM_SYMBOL_FAMILY_GEOMETRY,     // geometry's marks: angles, equal lengths, parallels, points, a protractor
    FUDE_ZOOM_SYMBOL_FAMILY_SCIENCE,      // lab glassware, optics, an atom, a cell, DNA
    FUDE_ZOOM_SYMBOL_FAMILY_STAMPS,       // ✓ ✗ ⚠ ? ! ★ ♥ a bulb… in their own colours
    // How many families there are.
    FUDE_ZOOM_SYMBOL_FAMILIES
} FUDE_ZOOM_SYMBOL_FAMILY_;

// How a part is drawn, as bit flags that combine.
typedef enum {
    FUDE_ZOOM_SYMBOL_CLOSED = 1,   // a loop; without it, an open line
    FUDE_ZOOM_SYMBOL_FILLED = 2,   // filled as the shape is (with the paper, or the Fill tool's colour)
    FUDE_ZOOM_SYMBOL_SOLID  = 4,   // filled with its line's colour (a black dot, a fork bar, an arrowhead)
    FUDE_ZOOM_SYMBOL_DASHED = 8,   // dashed (a boundary, a lifeline)
    FUDE_ZOOM_SYMBOL_TINTED = 16,  // filled with its own colour, the part's fill (art: a board's green, a copper pad)
    FUDE_ZOOM_SYMBOL_MOVES  = 32,  // what moves in it while it plays (a solenoid's plunger): drawn as far along as the
                                   // renderer's moved value says (render.h)
    FUDE_ZOOM_SYMBOL_NO_LINE = 64, // no line round it, only its fill
    FUDE_ZOOM_SYMBOL_JOIN   = 128, // another ring of the part before it, filled with it, so it makes a hole (a washer)
} FUDE_ZOOM_SYMBOL_PART_;

// One part of a drawn symbol: a run of the points array.
typedef struct {
    u32       first, count;   // its points: where they start in the points array, and how many
    u8        flags;          // FUDE_ZOOM_SYMBOL_PART_ flags
    rde_color fill;           // with TINTED, its fill's colour, alpha included
    rde_color line;           // its line's colour; alpha 0 means the symbol's own line colour
    f32       width;          // its line's width in the symbol's own units; 0 means the symbol's line width
} fude_zoom_symbol_part;

// Where its text goes.
typedef enum {
    FUDE_ZOOM_SYMBOL_TEXT_INSIDE = 0,   // in its middle (a process, a decision)
    FUDE_ZOOM_SYMBOL_TEXT_PARTS,        // a compartment each, top down (a class: name, attributes, operations)
    FUDE_ZOOM_SYMBOL_TEXT_BELOW,        // under it (a person, an icon)
    FUDE_ZOOM_SYMBOL_TEXT_HEADER,       // in its top band (a Kanban column, a lane, a fragment's label)
    FUDE_ZOOM_SYMBOL_TEXT_LEFT,         // up its left band (a pool, a swimlane)
    FUDE_ZOOM_SYMBOL_TEXT_CORNER,       // from its top left corner, left-aligned (an area's name)
    FUDE_ZOOM_SYMBOL_TEXT_OUT_LEFT,     // outside it on its left, ending next to it (a "{")
    FUDE_ZOOM_SYMBOL_TEXT_OUT_RIGHT,    // outside it on its right, starting next to it (a "}")
    FUDE_ZOOM_SYMBOL_TEXT_ABOVE,        // over it (a brace over a row of things)
    FUDE_ZOOM_SYMBOL_TEXT_REGIONS,      // each part of its text in a region of its own (a Venn diagram's)
} FUDE_ZOOM_SYMBOL_TEXT_;

// What the library knows about one symbol.
typedef struct {
    const c8* id;          // its name in English; the names the app shows, in its language, are page_diagrams.c's
                           // FUDE_ZOOM_PAGE_SYMBOL_NAMES, in the same order
    u8        family;      // FUDE_ZOOM_SYMBOL_FAMILY_
    f32       w, h;        // its size when inserted (screen points)
    u8        place;       // FUDE_ZOOM_SYMBOL_TEXT_
    u8        boxes;       // how many text boxes it has at least (a class: 3)
    b8        box_hull;    // its outline for connecting and snapping is its box (a stick figure, a bar)
} fude_zoom_symbol_info;

// Returns the kind (table index) of the symbol whose id is _id (compared whole, through a map of the ids made the first
// time one is looked for), or FUDE_ZOOM_NONE when there is none or _id is NULL.
u32  fude_zoom_symbol_find(const c8* _id);
// How many symbols the library has: symbol.c's table and page_diagrams.c's names (FUDE_ZOOM_PAGE_SYMBOL_NAMES) each
// have exactly this many, which each file checks as it compiles, so the two cannot drift apart unnoticed.
#define FUDE_ZOOM_SYMBOL_KINDS 439u
// fude_zoom_symbol_count returns how many symbols there are. fude_zoom_symbol_info_of returns kind _kind's entry, a
// pointer into the constant table valid for the program's life, or NULL past the last.
u32  fude_zoom_symbol_count(void);
const fude_zoom_symbol_info* fude_zoom_symbol_info_of(u32 _kind);

// The topics whose parts are symbols of the library, each with its own table of parts: a circuit's (circuit.h: the
// Electronics, Logic, Chips and Boards families), a mechanism's (mech.h: Mechanisms) and a building plan's (plan.h:
// Floor plan and House wiring). A symbol of any other family is no topic's part.
typedef enum {
    FUDE_ZOOM_SYMBOL_TOPIC_NONE = 0,
    FUDE_ZOOM_SYMBOL_TOPIC_CIRCUIT,
    FUDE_ZOOM_SYMBOL_TOPIC_MECH,
    FUDE_ZOOM_SYMBOL_TOPIC_PLAN,
} FUDE_ZOOM_SYMBOL_TOPIC_;
// Returns the topic (FUDE_ZOOM_SYMBOL_TOPIC_) whose part kind _kind is, by its family: FUDE_ZOOM_SYMBOL_TOPIC_NONE for
// a diagram's symbol, or no kind at all.
u8   fude_zoom_symbol_topic(u32 _kind);
// The one kind-to-part map every topic uses: returns the part kind _kind is in topic _topic's table, or NULL when it is
// none of that topic's parts (or no kind). The pointer is a const fude_zoom_part*, fude_zoom_mech_part* or
// fude_zoom_plan_part*, as each topic's own call types it (fude_zoom_part_of_kind, fude_zoom_mech_of_kind,
// fude_zoom_plan_of_kind); it is valid for the app's life. One table holds every kind: a kind's part is looked for by
// its id in its topic's table (fude_zoom_part_find, fude_zoom_mech_find, fude_zoom_plan_find) the first time it is
// asked for, then kept, a miss too. Main thread only: the table is in statics.
const any fude_zoom_symbol_topic_part(u32 _kind, u8 _topic);

// Draws kind _kind at half sizes _hw × _hh into _points (fude_zoom_v2) and _parts (fude_zoom_symbol_part), both
// cleared first, and returns how many parts. Round parts take _segments points a whole turn (kept between 16 and
// 256). The app's art (fude_zoom_symbol_set_art) is used when it has some for the kind. Returns 0 for an unknown kind.
u32  fude_zoom_symbol_parts(u32 _kind, f64 _hw, f64 _hh, u32 _segments, rde_arr* _points, rde_arr* _parts);
// Art drawn instead of a symbol's own drawing (the app's SVG files, symbolart.h): it draws kind _kind's parts for half
// sizes _hw × _hh as fude_zoom_symbol_parts would (its first part the outline) and returns how many, or 0 when it has
// no art for the kind, which is then drawn as usual.
typedef u32 (*fude_zoom_symbol_art_fn)(u32 _kind, f64 _hw, f64 _hh, u32 _segments, rde_arr* _points, rde_arr* _parts);
// Installs _fn as the art hook for every symbol drawn from now on; NULL removes it. The hook is a global, set by
// symbolart.c when the page is first set up.
void fude_zoom_symbol_set_art(fude_zoom_symbol_art_fn _fn);
// Writes kind _kind's outline into _points (cleared first) and returns how many points: its first part, or its box
// when its info's box_hull is set. Returns 0 for an unknown kind.
u32  fude_zoom_symbol_outline(u32 _kind, f64 _hw, f64 _hh, u32 _segments, rde_arr* _points);
// Writes where each part of kind _kind's text goes, as boxes in its own units, top down, into _out (room for _max) and
// returns how many. _count is how many parts its text has; it is raised to the info's boxes, and most places have a
// single box whatever it is. The lines between the boxes (a compartment's top) are drawn by the parts, not here.
// Returns 0 for an unknown kind or a _max of 0.
u32  fude_zoom_symbol_text_boxes(u32 _kind, f64 _hw, f64 _hh, u32 _count, fude_zoom_box* _out, u32 _max);

// The style of a connector's end, from UML's and the ER notations. A connector (shape.h's ARROW) keeps each end's style
// in its heads number above the FUDE_ZOOM_ARROW_ bits: the end's from bit 4 and the start's from bit 9, five bits
// each, so connectors saved before the styles existed read as style 0. Style 0 is the classic head: a filled
// arrowhead, or a dot with FUDE_ZOOM_ARROW_CIRCLE.
typedef enum {
    FUDE_ZOOM_HEAD_CLASSIC = 0,       // a filled arrowhead (a flow, a call)
    FUDE_ZOOM_HEAD_OPEN,              // two strokes (a dependency, an asynchronous message)
    FUDE_ZOOM_HEAD_HOLLOW,            // a hollow triangle (inheritance, realization)
    FUDE_ZOOM_HEAD_DIAMOND,           // a filled diamond (composition)
    FUDE_ZOOM_HEAD_HOLLOW_DIAMOND,    // a hollow one (aggregation)
    FUDE_ZOOM_HEAD_DOT,               // a filled dot
    FUDE_ZOOM_HEAD_RING,              // a hollow circle (BPMN's message flow's start)
    FUDE_ZOOM_HEAD_BAR,               // a bar across
    FUDE_ZOOM_HEAD_CROSS,             // an × (not navigable)
    FUDE_ZOOM_HEAD_ONE,               // ER: one
    FUDE_ZOOM_HEAD_ONLY_ONE,          // ER: exactly one
    FUDE_ZOOM_HEAD_MANY,              // ER: many (a crow's foot)
    FUDE_ZOOM_HEAD_ONE_MANY,          // ER: one or many
    FUDE_ZOOM_HEAD_ZERO_ONE,          // ER: zero or one
    FUDE_ZOOM_HEAD_ZERO_MANY,         // ER: zero or many
    // How many styles there are.
    FUDE_ZOOM_HEAD_COUNT
} FUDE_ZOOM_HEAD_;
// Read the end's or the start's style from a heads number, and pack both styles into one.
#define FUDE_ZOOM_ARROW_END_STYLE(_h)        ((((u32)(_h)) >> 4) & 31u)
#define FUDE_ZOOM_ARROW_START_STYLE(_h)      ((((u32)(_h)) >> 9) & 31u)
#define FUDE_ZOOM_ARROW_STYLES(_end, _start) ((((u32)(_end)) << 4) | (((u32)(_start)) << 9))
// Draws a head of style _style (FUDE_ZOOM_HEAD_) at _tip, for a line coming from _from, _size long (any units), into
// _points and _parts (cleared first). Parts flagged SOLID take the line's colour, FILLED ones the page's. Returns how
// far the line must stop short of _tip so it does not show through the head (0 for most). _dot makes the classic head
// a dot (FUDE_ZOOM_ARROW_CIRCLE). Draws nothing and returns 0 when _tip and _from meet or _size is not above 0.
f64  fude_zoom_symbol_head(u32 _style, b8 _dot, fude_zoom_v2 _tip, fude_zoom_v2 _from, f64 _size, rde_arr* _points, rde_arr* _parts);

// A SYMBOL shape's numbers (shape.h): its kind, half sizes and letters' height (all in its frame's units), then its
// text's bytes packed 8 to a number, ended by a 0.
#define FUDE_ZOOM_SYMBOL_TEXT 768u   // the most bytes of text a symbol keeps, its ending 0 included
// fude_zoom_symbol_numbers writes a symbol's numbers into _n, which must have room for 4 + FUDE_ZOOM_SYMBOL_TEXT / 8
// of them, cutting _text (NULL: none) at a whole character to fit, and returns how many it wrote.
// fude_zoom_symbol_text reads the text back from _count numbers into _out (_size bytes), cut at a whole character.
u32  fude_zoom_symbol_numbers(f64* _n, u32 _kind, f64 _hw, f64 _hh, f64 _px, const c8* _text);
void fude_zoom_symbol_text(const f64* _n, u32 _count, c8* _out, usize _size);
// Returns a SYMBOL's kind from its numbers (_n, _count of them): its first number, or 0xFFFFFFFF (scene.h's
// FUDE_ZOOM_NONE: no kind, which every symbol call takes as unknown) when it has none or that number is not one a u32
// holds (a damaged file's: NaN, negative or vast, which a cast would not hold). Any thread.
static inline u32 fude_zoom_symbol_kind_of(const f64* _n, u32 _count) {
    return _count >= 1u && _n[0] >= 0.0 && _n[0] < 4294967295.0 ? (u32)_n[0] : 0xFFFFFFFFu;
}
// Splits _text (ended by a 0) into parts at lines that are exactly "---", writes each part's start and length into
// _from and _len (room for _max), and returns how many parts: at least 1 when _max is not 0. Once _max is reached,
// the rest is all in the last part.
u32  fude_zoom_symbol_text_split(const c8* _text, u32* _from, u32* _len, u32 _max);

#endif
