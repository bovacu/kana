// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_SYMBOL
#define FUDE_ZOOM_SYMBOL

#include "rde.h"
#include "zoom/zoom.h"

// ===========================================================================
// The diagram library (Sketching's Insert → Diagram shapes): the shapes and icons
// software is drawn with — flowcharts, UML, sequence and activity diagrams, data
// and ER, C4 and architecture, BPMN, planning boards — as the best-known tools
// have them (draw.io, Lucidchart, Visio, Miro, Excalidraw's libraries, Mermaid,
// PlantUML, the UML and BPMN notations, the C4 model): generic ones, never a
// brand's logo.
//
// A SYMBOL (shape.h) keeps which one it is, its half sizes and its text; each is
// drawn here from those as PARTS — polylines in its own units (round its middle,
// Y up) — the first its outline (what fills, what a connector and the lasso
// meet, what snaps), the rest the lines inside or on it (a class's compartments,
// a cylinder's rim, an actor's limbs, a gateway's mark). Its text goes in its
// TEXT BOXES (a class's name, attributes and operations: its text's parts split
// at a line of its own that is "---"; most have one; a person's goes under it).
// ===========================================================================

typedef enum {
    FUDE_ZOOM_SYMBOL_FAMILY_FLOW = 0,     // flowchart and basic shapes
    FUDE_ZOOM_SYMBOL_FAMILY_UML,          // UML structure: classes, packages, components, use cases
    FUDE_ZOOM_SYMBOL_FAMILY_BEHAVIOUR,    // UML behaviour: sequence, activity, state
    FUDE_ZOOM_SYMBOL_FAMILY_DATA,         // data, ER, tables
    FUDE_ZOOM_SYMBOL_FAMILY_ARCHITECTURE, // C4, servers, services, cloud, network
    FUDE_ZOOM_SYMBOL_FAMILY_BPMN,
    FUDE_ZOOM_SYMBOL_FAMILY_PLANNING,     // Kanban columns and cards, swimlanes, milestones
    FUDE_ZOOM_SYMBOL_FAMILIES
} FUDE_ZOOM_SYMBOL_FAMILY_;

// A part's way of being drawn.
typedef enum {
    FUDE_ZOOM_SYMBOL_CLOSED = 1,   // a loop (else a line)
    FUDE_ZOOM_SYMBOL_FILLED = 2,   // filled as the shape is (its fill: the paper, or the Fill tool's colour)
    FUDE_ZOOM_SYMBOL_SOLID  = 4,   // filled with its line's colour (a black dot, a fork bar, an arrowhead)
    FUDE_ZOOM_SYMBOL_DASHED = 8,   // dashed (a boundary, a lifeline)
} FUDE_ZOOM_SYMBOL_PART_;

typedef struct {
    u32 first, count;   // its points
    u8  flags;          // FUDE_ZOOM_SYMBOL_PART_
} fude_zoom_symbol_part;

// Where its text goes.
typedef enum {
    FUDE_ZOOM_SYMBOL_TEXT_INSIDE = 0,   // in its middle (a process, a decision)
    FUDE_ZOOM_SYMBOL_TEXT_PARTS,        // a compartment each, top down (a class: name, attributes, operations)
    FUDE_ZOOM_SYMBOL_TEXT_BELOW,        // under it (a person, an icon)
    FUDE_ZOOM_SYMBOL_TEXT_HEADER,       // in its top band (a Kanban column, a lane, a fragment's label)
    FUDE_ZOOM_SYMBOL_TEXT_LEFT,         // up its left band, (a pool, a swimlane)
    FUDE_ZOOM_SYMBOL_TEXT_CORNER,       // from its top left corner, from the left (an area's name)
} FUDE_ZOOM_SYMBOL_TEXT_;

typedef struct {
    const c8* id;          // its name, in English (the app's own names, in its language, in the same order: page.c)
    u8        family;      // FUDE_ZOOM_SYMBOL_FAMILY_
    f32       w, h;        // as it comes (screen points)
    u8        place;       // FUDE_ZOOM_SYMBOL_TEXT_
    u8        boxes;       // its text boxes at least (a class: 3)
    b8        box_hull;    // its outline for connecting and snapping is its box (a stick figure, a bar)
} fude_zoom_symbol_info;

// The one whose id is _id (FUDE_ZOOM_NONE: none).
u32  fude_zoom_symbol_find(const c8* _id);
// How many there are, and each one (NULL past the last).
u32  fude_zoom_symbol_count(void);
const fude_zoom_symbol_info* fude_zoom_symbol_info_of(u32 _kind);

// Its parts (_points, _parts cleared first) for half sizes _hw × _hh, its round
// parts in _segments a whole turn. How many parts.
u32  fude_zoom_symbol_parts(u32 _kind, f64 _hw, f64 _hh, u32 _segments, rde_arr* _points, rde_arr* _parts);
// Its outline (part 0, or its box: info's box_hull) into _points (cleared first).
u32  fude_zoom_symbol_outline(u32 _kind, f64 _hw, f64 _hh, u32 _segments, rde_arr* _points);
// Its text boxes (own units, top down), _count of them (its text's parts, at least info's boxes): where
// each part of its text goes; and the lines between them (a compartment's top) drawn by the parts.
u32  fude_zoom_symbol_text_boxes(u32 _kind, f64 _hw, f64 _hh, u32 _count, fude_zoom_box* _out, u32 _max);

// A CONNECTOR'S END (shape.h' ARROW: each end's style in its heads number, over FUDE_ZOOM_ARROW_'s bits — the
// end's from bit 4, the start's from bit 9; 0 the classic head, a filled arrowhead, or a dot with
// FUDE_ZOOM_ARROW_CIRCLE): UML's and the ER notations' ends.
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
    FUDE_ZOOM_HEAD_COUNT
} FUDE_ZOOM_HEAD_;
#define FUDE_ZOOM_ARROW_END_STYLE(_h)        ((((u32)(_h)) >> 4) & 31u)
#define FUDE_ZOOM_ARROW_START_STYLE(_h)      ((((u32)(_h)) >> 9) & 31u)
#define FUDE_ZOOM_ARROW_STYLES(_end, _start) ((((u32)(_end)) << 4) | (((u32)(_start)) << 9))
// A head of style _style at _tip, the line coming from _from, _size long (any units): its parts into _points and
// _parts (cleared first; SOLID: in the line's colour, FILLED: as the page). How far the line stops short of _tip.
// _dot: the classic head is a dot (FUDE_ZOOM_ARROW_CIRCLE).
f64  fude_zoom_symbol_head(u32 _style, b8 _dot, fude_zoom_v2 _tip, fude_zoom_v2 _from, f64 _size, rde_arr* _points, rde_arr* _parts);

// Its numbers (shape.h' SYMBOL): kind, half sizes (its frame's units), its letters' height (the same), and
// its text's bytes (NUL-ended, cut at a whole character to fit). How many numbers.
#define FUDE_ZOOM_SYMBOL_TEXT 768u   // its text's bytes, its NUL included, at most
u32  fude_zoom_symbol_numbers(f64* _n, u32 _kind, f64 _hw, f64 _hh, f64 _px, const c8* _text);
void fude_zoom_symbol_text(const f64* _n, u32 _count, c8* _out, usize _size);
// Its text's parts (split at lines that are "---"): how many, and each one's start and length in _text.
u32  fude_zoom_symbol_text_split(const c8* _text, u32* _from, u32* _len, u32 _max);

#endif
