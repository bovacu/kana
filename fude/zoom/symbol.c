// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/symbol_parts.h"

// ===========================================================================
// See symbol.h.
//
// Each symbol is drawn by a function of its half sizes (hw, hh), fzs_<what>: its parts, in its own units round its
// middle (Y up). Most are drawn in the box's own proportions (u, v from -1 to 1, as the box is: fude_zoom_sym_uv); an
// icon's round bits are round whatever its box (a radius of the smaller half, FZS_R). The first part a function makes
// is the symbol's outline.
//
// symbol.c is the coordinator of the symbols. The file goes: the drawing helpers every draw function shares (the pen,
// fude_zoom_sym_); the parts the topics draw themselves (the circuit's, mechanisms' and plans' parts, drawn by
// circuit.h, mech.h and plan.h, one draw function a topic); the table FUDE_ZOOM_SYMBOLS that gives each kind its id,
// family, size, text place and draw function; then the public functions: finding a kind, the one kind-to-part map of
// the topics (fude_zoom_symbol_topic, fude_zoom_symbol_topic_part), drawing a kind, its outline, its text boxes, a
// connector's heads, and a SYMBOL shape's numbers. The table is append-only (symbol.h), and page_diagrams.c's
// FUDE_ZOOM_PAGE_SYMBOL_NAMES follows its order. The other draw functions are in symbol_diagrams.c and symbol_shapes.c.
//
// Part of the symbols: symbol_parts.h holds what the modules share and says which module holds what. Internal names:
// the pen's fude_zoom_sym_, the draw functions' fzs_ and FZS_. Main thread only, as symbol.h says.
// ===========================================================================

// Adds a point in the symbol's own units to the part being made.
void fude_zoom_sym_pt(fude_zoom_sym* _s, f64 _x, f64 _y) {
    const fude_zoom_v2 _p = { _x, _y };
    rde_arr_add(_s->points, (any)&_p);
}

// In the box's proportions: u, v from -1 to 1.
void fude_zoom_sym_uv(fude_zoom_sym* _s, f64 _u, f64 _v) {
    fude_zoom_sym_pt(_s, _u * _s->hw, _v * _s->hh);
}

// Starts a part at the next point.
void fude_zoom_sym_begin(fude_zoom_sym* _s) {
    _s->first = (u32)rde_arr_length(_s->points);
}

// Ends the part begun by fude_zoom_sym_begin, flagged _flags: kept with 2 points or more (a solid one with 1: a dot),
// else dropped with its point.
void fude_zoom_sym_end(fude_zoom_sym* _s, u8 _flags) {
    const u32 _n = (u32)rde_arr_length(_s->points) - _s->first;
    if(_n >= 2u || ((_flags & FUDE_ZOOM_SYMBOL_SOLID) && _n >= 1u)) {
        const fude_zoom_symbol_part _p = { .first = _s->first, .count = _n, .flags = _flags };
        rde_arr_add(_s->parts, (any)&_p);
    } else {
        rde_arr_resize(_s->points, _s->first);   // (too few points: none)
    }
}

// A part of _n points in u, v (pairs).
void fude_zoom_sym_poly(fude_zoom_sym* _s, const f64* _uv, u32 _n, u8 _flags) {
    fude_zoom_sym_begin(_s);
    for(u32 _i = 0; _i < _n; _i++) {
        fude_zoom_sym_uv(_s, _uv[2u * _i], _uv[2u * _i + 1u]);
    }
    fude_zoom_sym_end(_s, _flags);
}

// A straight line in u, v.
void fude_zoom_sym_line(fude_zoom_sym* _s, f64 _u0, f64 _v0, f64 _u1, f64 _v1, u8 _flags) {
    const f64 _l[4] = { _u0, _v0, _u1, _v1 };
    fude_zoom_sym_poly(_s, _l, 2u, _flags);
}

// A closed rectangle in u, v.
void fude_zoom_sym_rect(fude_zoom_sym* _s, f64 _u0, f64 _v0, f64 _u1, f64 _v1, u8 _flags) {
    const f64 _r[8] = { _u0, _v0, _u1, _v0, _u1, _v1, _u0, _v1 };
    fude_zoom_sym_poly(_s, _r, 4u, (u8)(_flags | FUDE_ZOOM_SYMBOL_CLOSED));
}

// An arc of the ellipse round (cx, cy) (own units) _rx by _ry, from angle _a0 to _a1 (radians), its points
// added to the part being made (its ends included).
void fude_zoom_sym_arc_pts(fude_zoom_sym* _s, f64 _cx, f64 _cy, f64 _rx, f64 _ry, f64 _a0, f64 _a1) {
    u32 _k = (u32)rde_math_ceil_f64((f64)_s->segments * rde_math_abs_f64(_a1 - _a0) / (2.0 * RDE_PI));
    _k = _k < 2u ? 2u : _k;
    for(u32 _i = 0; _i <= _k; _i++) {
        const f64 _a = _a0 + (_a1 - _a0) * (f64)_i / (f64)_k;
        fude_zoom_sym_pt(_s, _cx + rde_math_cos_f64(_a) * _rx, _cy + rde_math_sin_f64(_a) * _ry);
    }
}

// An ellipse (own units); a whole one closed.
void fude_zoom_sym_ellipse(fude_zoom_sym* _s, f64 _cx, f64 _cy, f64 _rx, f64 _ry, u8 _flags) {
    fude_zoom_sym_begin(_s);
    const u32 _k = _s->segments < 12u ? 12u : _s->segments;
    for(u32 _i = 0; _i < _k; _i++) {
        const f64 _a = 2.0 * RDE_PI * (f64)_i / (f64)_k;
        fude_zoom_sym_pt(_s, _cx + rde_math_cos_f64(_a) * _rx, _cy + rde_math_sin_f64(_a) * _ry);
    }
    fude_zoom_sym_end(_s, (u8)(_flags | FUDE_ZOOM_SYMBOL_CLOSED));
}

// A rounded rectangle (own units), its corners _r round.
void fude_zoom_sym_rounded(fude_zoom_sym* _s, f64 _x0, f64 _y0, f64 _x1, f64 _y1, f64 _r, u8 _flags) {
    _r = rde_math_min_f64(_r, rde_math_min_f64(rde_math_abs_f64(_x1 - _x0), rde_math_abs_f64(_y1 - _y0)) * 0.5);
    fude_zoom_sym_begin(_s);
    const f64 _h = RDE_PI * 0.5;
    fude_zoom_sym_arc_pts(_s, _x1 - _r, _y0 + _r, _r, _r, -_h, 0.0);
    fude_zoom_sym_arc_pts(_s, _x1 - _r, _y1 - _r, _r, _r, 0.0, _h);
    fude_zoom_sym_arc_pts(_s, _x0 + _r, _y1 - _r, _r, _r, _h, 2.0 * _h);
    fude_zoom_sym_arc_pts(_s, _x0 + _r, _y0 + _r, _r, _r, 2.0 * _h, 3.0 * _h);
    fude_zoom_sym_end(_s, (u8)(_flags | FUDE_ZOOM_SYMBOL_CLOSED));
}

// A wave from (x0, y) to (x1, y) (own units), _a high, _n half waves.
void fude_zoom_sym_wave_pts(fude_zoom_sym* _s, f64 _x0, f64 _x1, f64 _y, f64 _a, u32 _n) {
    const u32 _k = 12u * _n;
    for(u32 _i = 0; _i <= _k; _i++) {
        const f64 _t = (f64)_i / (f64)_k;
        fude_zoom_sym_pt(_s, _x0 + (_x1 - _x0) * _t, _y + _a * rde_math_sin_f64(_t * RDE_PI * (f64)_n));
    }
}

// An arrowhead at (x, y) (own units) pointing along (dx, dy), _len long: filled.
void fude_zoom_sym_head(fude_zoom_sym* _s, f64 _x, f64 _y, f64 _dx, f64 _dy, f64 _len) {
    const f64 _l = rde_math_hypot_f64(_dx, _dy);
    if(!(_l > 0.0)) {
        return;
    }
    const f64 _ux = _dx / _l, _uy = _dy / _l;
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, _x, _y);
    fude_zoom_sym_pt(_s, _x - _ux * _len - _uy * _len * 0.5, _y - _uy * _len + _ux * _len * 0.5);
    fude_zoom_sym_pt(_s, _x - _ux * _len + _uy * _len * 0.5, _y - _uy * _len - _ux * _len * 0.5);
    fude_zoom_sym_end(_s, FUDE_ZOOM_SYMBOL_CLOSED | FUDE_ZOOM_SYMBOL_SOLID);
}

// A line with a head at its end (own units).
void fude_zoom_sym_arrow(fude_zoom_sym* _s, f64 _x0, f64 _y0, f64 _x1, f64 _y1, f64 _len) {
    fude_zoom_sym_begin(_s);
    fude_zoom_sym_pt(_s, _x0, _y0);
    fude_zoom_sym_pt(_s, _x1, _y1);
    fude_zoom_sym_end(_s, 0u);
    fude_zoom_sym_head(_s, _x1, _y1, _x1 - _x0, _y1 - _y0, _len);
}

// A regular polygon round the middle, _n sides, the first corner at angle _a.
void fude_zoom_sym_regular(fude_zoom_sym* _s, u32 _n, f64 _a, u8 _flags) {
    fude_zoom_sym_begin(_s);
    for(u32 _i = 0; _i < _n; _i++) {
        const f64 _t = _a + 2.0 * RDE_PI * (f64)_i / (f64)_n;
        fude_zoom_sym_uv(_s, rde_math_cos_f64(_t), rde_math_sin_f64(_t));
    }
    fude_zoom_sym_end(_s, (u8)(_flags | FUDE_ZOOM_SYMBOL_CLOSED));
}

// --- the catalogue ---------------------------------------------------------------------------------

// A symbol's draw function.
typedef void (*fude_zoom_sym_draw)(fude_zoom_sym* _s);

// A row of the catalogue: what symbol.h tells of the symbol, and its draw function.
typedef struct {
    fude_zoom_symbol_info info;
    fude_zoom_sym_draw    draw;
} fude_zoom_sym_entry;

// The topics' parts: a circuit's (circuit.h), a mechanism's (mech.h) and a building plan's (plan.h), each drawn by its
// topic. Every symbol of the Electronics, Logic, Chips and Boards families is a circuit's part, of Mechanisms a
// mechanism's, and of Floor plan and House wiring a plan's (fude_zoom_symbol_topic), its id the part's id in its
// topic's table. The part is found by the kind's id each time it is drawn, as it always was (a chip of the
// simulation's or a sized display is made, and kept, as its part is first found: circuit.h).

// A circuit's part, as circuit.h draws it.
RDE_INTERNAL void fzs_circuit(fude_zoom_sym* _s) { fude_zoom_part_draw(fude_zoom_part_find(fude_zoom_symbol_info_of(_s->kind)->id), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
// A mechanism's part, as mech.h draws it.
RDE_INTERNAL void fzs_mech(fude_zoom_sym* _s) { fude_zoom_mech_draw(fude_zoom_mech_find(fude_zoom_symbol_info_of(_s->kind)->id), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }
// A building plan's part, as plan.h draws it.
RDE_INTERNAL void fzs_plan(fude_zoom_sym* _s) { fude_zoom_plan_draw(fude_zoom_plan_find(fude_zoom_symbol_info_of(_s->kind)->id), _s->hw, _s->hh, _s->segments, _s->points, _s->parts); }

// A row of the table: its id; its name's text id, which the row does not keep (page_diagrams.c's
// FUDE_ZOOM_PAGE_SYMBOL_NAMES has the names, in the same order); its family; its size when inserted (w, h, screen
// points); where its text goes; how many text boxes it has at least; whether its box is its hull; its draw function.
#define FZS(_id, _name, _fam, _w, _h, _place, _boxes, _hull, _fn) \
    { { _id, FUDE_ZOOM_SYMBOL_FAMILY_##_fam, _w, _h, FUDE_ZOOM_SYMBOL_TEXT_##_place, _boxes, _hull }, _fn }   // (_name: page_diagrams.c's FUDE_ZOOM_PAGE_SYMBOL_NAMES, in the same order)

// The catalogue, a row a symbol. Append-only: a symbol's kind is its place here, kept in the canvases.
RDE_INTERNAL const fude_zoom_sym_entry FUDE_ZOOM_SYMBOLS[] = {
    // Flowchart and basic.
    FZS("process",          ZOOM_SYM_PROCESS,        FLOW, 120, 70, INSIDE, 1, false, fzs_process),
    FZS("alternate",        ZOOM_SYM_ALTERNATE,      FLOW, 120, 70, INSIDE, 1, false, fzs_rounded),
    FZS("decision",         ZOOM_SYM_DECISION,       FLOW, 120, 84, INSIDE, 1, false, fzs_decision),
    FZS("terminator",       ZOOM_SYM_TERMINATOR,     FLOW, 120, 56, INSIDE, 1, false, fzs_terminator),
    FZS("data",             ZOOM_SYM_DATA,           FLOW, 120, 70, INSIDE, 1, false, fzs_data),
    FZS("predefined",       ZOOM_SYM_PREDEFINED,     FLOW, 130, 70, INSIDE, 1, false, fzs_predefined),
    FZS("document",         ZOOM_SYM_DOCUMENT,       FLOW, 120, 76, INSIDE, 1, false, fzs_document),
    FZS("documents",        ZOOM_SYM_DOCUMENTS,      FLOW, 124, 84, INSIDE, 1, false, fzs_documents),
    FZS("manual input",     ZOOM_SYM_MANUAL_INPUT,   FLOW, 120, 70, INSIDE, 1, false, fzs_manual_input),
    FZS("manual operation", ZOOM_SYM_MANUAL_OP,      FLOW, 120, 70, INSIDE, 1, false, fzs_manual_op),
    FZS("preparation",      ZOOM_SYM_PREPARATION,    FLOW, 130, 70, INSIDE, 1, false, fzs_preparation),
    FZS("delay",            ZOOM_SYM_DELAY,          FLOW, 110, 70, INSIDE, 1, false, fzs_delay),
    FZS("stored data",      ZOOM_SYM_STORED_DATA,    FLOW, 120, 70, INSIDE, 1, false, fzs_stored_data),
    FZS("internal storage", ZOOM_SYM_INTERNAL,       FLOW, 100, 84, INSIDE, 1, false, fzs_internal_storage),
    FZS("display",          ZOOM_SYM_DISPLAY,        FLOW, 130, 70, INSIDE, 1, false, fzs_display),
    FZS("off-page",         ZOOM_SYM_OFFPAGE,        FLOW, 64, 70,  INSIDE, 1, false, fzs_offpage),
    FZS("connector",        ZOOM_SYM_CONNECTOR,      FLOW, 50, 50,  INSIDE, 1, false, fzs_connector),
    FZS("merge",            ZOOM_SYM_MERGE,          FLOW, 84, 70,  INSIDE, 1, false, fzs_merge),
    FZS("extract",          ZOOM_SYM_EXTRACT,        FLOW, 84, 70,  INSIDE, 1, false, fzs_extract),
    FZS("or",               ZOOM_SYM_OR,             FLOW, 50, 50,  BELOW,  1, false, fzs_or),
    FZS("summing junction", ZOOM_SYM_SUMMING,        FLOW, 50, 50,  BELOW,  1, false, fzs_summing),
    FZS("collate",          ZOOM_SYM_COLLATE,        FLOW, 64, 76,  BELOW,  1, false, fzs_collate),
    FZS("sort",             ZOOM_SYM_SORT,           FLOW, 76, 76,  BELOW,  1, false, fzs_sort),
    FZS("annotation",       ZOOM_SYM_ANNOTATION,     FLOW, 130, 60, INSIDE, 1, true,  fzs_annotation),
    FZS("card",             ZOOM_SYM_CARD,           FLOW, 116, 70, INSIDE, 1, false, fzs_card),
    FZS("punched tape",     ZOOM_SYM_TAPE,           FLOW, 120, 70, INSIDE, 1, false, fzs_tape),
    FZS("loop limit",       ZOOM_SYM_LOOP_LIMIT,     FLOW, 120, 60, INSIDE, 1, false, fzs_loop_limit),
    FZS("triangle",         ZOOM_SYM_TRIANGLE,       FLOW, 90, 80,  INSIDE, 1, false, fzs_triangle),
    FZS("hexagon",          ZOOM_SYM_HEXAGON,        FLOW, 110, 90, INSIDE, 1, false, fzs_hexagon),
    FZS("ellipse",          ZOOM_SYM_ELLIPSE,        FLOW, 120, 76, INSIDE, 1, false, fzs_ellipse),
    FZS("callout",          ZOOM_SYM_CALLOUT,        FLOW, 140, 90, INSIDE, 1, false, fzs_callout),
    // UML structure.
    FZS("class",            ZOOM_SYM_CLASS,          UML, 180, 130, PARTS, 3, false, fzs_class),
    FZS("interface",        ZOOM_SYM_INTERFACE,      UML, 40, 60,   BELOW, 1, true,  fzs_interface),
    FZS("required",         ZOOM_SYM_REQUIRED,       UML, 40, 60,   BELOW, 1, true,  fzs_socket),
    FZS("package",          ZOOM_SYM_PACKAGE,        UML, 170, 120, INSIDE, 1, false, fzs_package),
    FZS("note",             ZOOM_SYM_NOTE,           UML, 150, 90,  INSIDE, 1, false, fzs_note),
    FZS("component",        ZOOM_SYM_COMPONENT,      UML, 170, 90,  INSIDE, 1, false, fzs_component),
    FZS("node",             ZOOM_SYM_NODE,           UML, 160, 110, INSIDE, 1, false, fzs_node),
    FZS("artifact",         ZOOM_SYM_ARTIFACT,       UML, 150, 80,  INSIDE, 1, false, fzs_artifact),
    FZS("actor",            ZOOM_SYM_ACTOR,          UML, 46, 90,   BELOW, 1, true,  fzs_actor),
    FZS("use case",         ZOOM_SYM_USE_CASE,       UML, 150, 70,  INSIDE, 1, false, fzs_use_case),
    FZS("system boundary",  ZOOM_SYM_BOUNDARY,       UML, 280, 340, HEADER, 1, false, fzs_boundary),
    FZS("object",           ZOOM_SYM_OBJECT,         UML, 170, 90,  PARTS, 2, false, fzs_class),
    FZS("enumeration",      ZOOM_SYM_ENUM,           UML, 170, 110, PARTS, 2, false, fzs_class),
    // UML behaviour.
    FZS("lifeline",         ZOOM_SYM_LIFELINE,       BEHAVIOUR, 120, 320, HEADER, 1, true, fzs_lifeline),
    FZS("activation",       ZOOM_SYM_ACTIVATION,     BEHAVIOUR, 16, 120,  BELOW, 1, false, fzs_activation),
    FZS("destroy",          ZOOM_SYM_DESTROY,        BEHAVIOUR, 36, 36,   BELOW, 1, true,  fzs_destroy),
    FZS("fragment",         ZOOM_SYM_FRAGMENT,       BEHAVIOUR, 300, 200, HEADER, 1, false, fzs_fragment),
    FZS("initial",          ZOOM_SYM_INITIAL,        BEHAVIOUR, 30, 30,   BELOW, 1, false, fzs_initial),
    FZS("final",            ZOOM_SYM_FINAL,          BEHAVIOUR, 34, 34,   BELOW, 1, false, fzs_final),
    FZS("flow final",       ZOOM_SYM_FLOW_FINAL,     BEHAVIOUR, 34, 34,   BELOW, 1, false, fzs_flow_final),
    FZS("fork / join",      ZOOM_SYM_FORK,           BEHAVIOUR, 170, 10,  BELOW, 1, false, fzs_bar),
    FZS("action",           ZOOM_SYM_ACTION,         BEHAVIOUR, 140, 64,  INSIDE, 1, false, fzs_action),
    FZS("state",            ZOOM_SYM_STATE,          BEHAVIOUR, 150, 90,  PARTS, 2, false, fzs_state),
    FZS("choice",           ZOOM_SYM_CHOICE,         BEHAVIOUR, 50, 50,   BELOW, 1, false, fzs_small_decision),
    FZS("send signal",      ZOOM_SYM_SEND,           BEHAVIOUR, 130, 60,  INSIDE, 1, false, fzs_send),
    FZS("accept signal",    ZOOM_SYM_RECEIVE,        BEHAVIOUR, 130, 60,  INSIDE, 1, false, fzs_receive),
    FZS("time event",       ZOOM_SYM_TIME_EVENT,     BEHAVIOUR, 36, 46,   BELOW, 1, false, fzs_hourglass),
    FZS("history",          ZOOM_SYM_HISTORY,        BEHAVIOUR, 36, 36,   BELOW, 1, false, fzs_history),
    FZS("swimlane",         ZOOM_SYM_SWIMLANE,       BEHAVIOUR, 220, 420, HEADER, 1, false, fzs_vlane),
    // Data.
    FZS("database",         ZOOM_SYM_DATABASE,       DATA, 90, 110,  INSIDE, 1, false, fzs_cylinder),
    FZS("table",            ZOOM_SYM_TABLE,          DATA, 170, 150, PARTS, 2, false, fzs_table),
    FZS("entity",           ZOOM_SYM_ENTITY,         DATA, 130, 70,  INSIDE, 1, false, fzs_entity),
    FZS("weak entity",      ZOOM_SYM_WEAK_ENTITY,    DATA, 140, 74,  INSIDE, 1, false, fzs_weak_entity),
    FZS("relationship",     ZOOM_SYM_RELATIONSHIP,   DATA, 130, 84,  INSIDE, 1, false, fzs_relationship),
    FZS("weak relationship", ZOOM_SYM_WEAK_RELATION, DATA, 140, 90,  INSIDE, 1, false, fzs_weak_relationship),
    FZS("attribute",        ZOOM_SYM_ATTRIBUTE,      DATA, 120, 60,  INSIDE, 1, false, fzs_attribute),
    FZS("multivalued",      ZOOM_SYM_MULTIVALUED,    DATA, 130, 66,  INSIDE, 1, false, fzs_multivalued),
    FZS("derived",          ZOOM_SYM_DERIVED,        DATA, 120, 60,  INSIDE, 1, false, fzs_derived),
    FZS("file",             ZOOM_SYM_FILE,           DATA, 60, 76,   BELOW, 1, false, fzs_file),
    FZS("storage",          ZOOM_SYM_STORAGE,        DATA, 90, 110,  BELOW, 1, false, fzs_disks),
    // Architecture.
    FZS("person",           ZOOM_SYM_PERSON,         ARCHITECTURE, 120, 150, INSIDE, 1, false, fzs_person),
    FZS("software system",  ZOOM_SYM_SYSTEM,         ARCHITECTURE, 180, 110, INSIDE, 1, false, fzs_system),
    FZS("external system",  ZOOM_SYM_EXTERNAL,       ARCHITECTURE, 180, 110, INSIDE, 1, false, fzs_external),
    FZS("boundary",         ZOOM_SYM_C4_BOUNDARY,    ARCHITECTURE, 360, 260, HEADER, 1, false, fzs_c4_boundary),
    FZS("server",           ZOOM_SYM_SERVER,         ARCHITECTURE, 70, 96,   BELOW, 1, false, fzs_server),
    FZS("desktop",          ZOOM_SYM_DESKTOP,        ARCHITECTURE, 96, 80,   BELOW, 1, false, fzs_desktop),
    FZS("laptop",           ZOOM_SYM_LAPTOP,         ARCHITECTURE, 104, 70,  BELOW, 1, false, fzs_laptop),
    FZS("phone",            ZOOM_SYM_PHONE,          ARCHITECTURE, 46, 86,   BELOW, 1, false, fzs_phone),
    FZS("tablet",           ZOOM_SYM_TABLET,         ARCHITECTURE, 70, 92,   BELOW, 1, false, fzs_tablet),
    FZS("browser",          ZOOM_SYM_BROWSER,        ARCHITECTURE, 150, 104, INSIDE, 1, false, fzs_browser),
    FZS("cloud",            ZOOM_SYM_CLOUD,          ARCHITECTURE, 150, 96,  INSIDE, 1, false, fzs_cloud),
    FZS("queue",            ZOOM_SYM_QUEUE,          ARCHITECTURE, 150, 56,  BELOW, 1, false, fzs_queue),
    FZS("cache",            ZOOM_SYM_CACHE,          ARCHITECTURE, 80, 100,  BELOW, 1, false, fzs_cache),
    FZS("load balancer",    ZOOM_SYM_BALANCER,       ARCHITECTURE, 70, 70,   BELOW, 1, false, fzs_balancer),
    FZS("API gateway",      ZOOM_SYM_GATEWAY,        ARCHITECTURE, 84, 70,   BELOW, 1, false, fzs_gateway),
    FZS("microservice",     ZOOM_SYM_MICROSERVICE,   ARCHITECTURE, 110, 96,  INSIDE, 1, false, fzs_microservice),
    FZS("function",         ZOOM_SYM_FUNCTION,       ARCHITECTURE, 66, 66,   BELOW, 1, false, fzs_function),
    FZS("container",        ZOOM_SYM_CONTAINER,      ARCHITECTURE, 110, 64,  BELOW, 1, false, fzs_container),
    FZS("pod",              ZOOM_SYM_POD,            ARCHITECTURE, 76, 76,   BELOW, 1, false, fzs_pod),
    FZS("firewall",         ZOOM_SYM_FIREWALL,       ARCHITECTURE, 80, 70,   BELOW, 1, false, fzs_firewall),
    FZS("bucket",           ZOOM_SYM_BUCKET,         ARCHITECTURE, 70, 80,   BELOW, 1, false, fzs_bucket),
    FZS("globe",            ZOOM_SYM_GLOBE,          ARCHITECTURE, 70, 70,   BELOW, 1, false, fzs_globe),
    FZS("router",           ZOOM_SYM_ROUTER,         ARCHITECTURE, 70, 70,   BELOW, 1, false, fzs_router),
    FZS("switch",           ZOOM_SYM_SWITCH,         ARCHITECTURE, 100, 52,  BELOW, 1, false, fzs_switch),
    FZS("user",             ZOOM_SYM_USER,           ARCHITECTURE, 60, 70,   BELOW, 1, true,  fzs_user),
    FZS("event bus",        ZOOM_SYM_BUS,            ARCHITECTURE, 260, 44,  INSIDE, 1, false, fzs_bus),
    FZS("security",         ZOOM_SYM_LOCK,           ARCHITECTURE, 50, 64,   BELOW, 1, false, fzs_lock),
    FZS("scheduler",        ZOOM_SYM_CLOCK,          ARCHITECTURE, 64, 64,   BELOW, 1, false, fzs_clock),
    FZS("service",          ZOOM_SYM_GEAR,           ARCHITECTURE, 68, 68,   BELOW, 1, false, fzs_gear),
    FZS("folder",           ZOOM_SYM_FOLDER,         ARCHITECTURE, 76, 58,   BELOW, 1, false, fzs_folder),
    FZS("monitoring",       ZOOM_SYM_CHART,          ARCHITECTURE, 96, 70,   BELOW, 1, false, fzs_chart),
    FZS("container database", ZOOM_SYM_CONTAINER_DB, ARCHITECTURE, 120, 130, INSIDE, 1, false, fzs_container_db),
    FZS("internet",         ZOOM_SYM_INTERNET,       ARCHITECTURE, 130, 84,  INSIDE, 1, false, fzs_internet),
    // BPMN.
    FZS("start event",      ZOOM_SYM_START_EVENT,    BPMN, 40, 40,   BELOW, 1, false, fzs_start_event),
    FZS("intermediate event", ZOOM_SYM_INTERMEDIATE, BPMN, 40, 40,   BELOW, 1, false, fzs_intermediate),
    FZS("end event",        ZOOM_SYM_END_EVENT,      BPMN, 40, 40,   BELOW, 1, false, fzs_end_event),
    FZS("timer event",      ZOOM_SYM_TIMER_EVENT,    BPMN, 40, 40,   BELOW, 1, false, fzs_timer_event),
    FZS("message event",    ZOOM_SYM_MESSAGE_EVENT,  BPMN, 40, 40,   BELOW, 1, false, fzs_message_event),
    FZS("task",             ZOOM_SYM_TASK,           BPMN, 130, 80,  INSIDE, 1, false, fzs_task),
    FZS("sub-process",      ZOOM_SYM_SUBPROCESS,     BPMN, 140, 90,  INSIDE, 1, false, fzs_subprocess),
    FZS("exclusive gateway", ZOOM_SYM_XOR_GATEWAY,   BPMN, 56, 56,   BELOW, 1, false, fzs_xor_gateway),
    FZS("parallel gateway", ZOOM_SYM_AND_GATEWAY,    BPMN, 56, 56,   BELOW, 1, false, fzs_and_gateway),
    FZS("inclusive gateway", ZOOM_SYM_OR_GATEWAY,    BPMN, 56, 56,   BELOW, 1, false, fzs_or_gateway),
    FZS("data object",      ZOOM_SYM_DATA_OBJECT,    BPMN, 46, 60,   BELOW, 1, false, fzs_data_object),
    FZS("data store",       ZOOM_SYM_DATA_STORE,     BPMN, 60, 60,   BELOW, 1, false, fzs_data_store),
    FZS("pool / lane",      ZOOM_SYM_POOL,           BPMN, 560, 170, LEFT, 1, false, fzs_pool),
    FZS("group",            ZOOM_SYM_GROUP,          BPMN, 240, 160, HEADER, 1, false, fzs_group),
    // Planning.
    FZS("column",           ZOOM_SYM_COLUMN,         PLANNING, 230, 520, HEADER, 1, false, fzs_column),
    FZS("task card",        ZOOM_SYM_TASK_CARD,      PLANNING, 190, 90,  INSIDE, 1, false, fzs_task_card),
    FZS("lane",             ZOOM_SYM_LANE,           PLANNING, 700, 160, LEFT, 1, false, fzs_hlane),
    FZS("milestone",        ZOOM_SYM_MILESTONE,      PLANNING, 28, 28,   BELOW, 1, false, fzs_milestone),
    FZS("2 × 2 matrix",     ZOOM_SYM_MATRIX,         PLANNING, 340, 340, PARTS, 4, false, fzs_matrix),
    FZS("arrow",            ZOOM_SYM_ARROW_BLOCK,    PLANNING, 150, 70,  INSIDE, 1, false, fzs_arrow_block),
    FZS("timeline",         ZOOM_SYM_TIMELINE,       PLANNING, 420, 30,  BELOW, 1, true,  fzs_timeline),
    // (Appended.)
    FZS("boundary object",  ZOOM_SYM_SEQ_BOUNDARY,   BEHAVIOUR, 70, 50,   BELOW, 1, false, fzs_seq_boundary),
    FZS("control object",   ZOOM_SYM_SEQ_CONTROL,    BEHAVIOUR, 50, 54,   BELOW, 1, false, fzs_seq_control),
    FZS("entity object",    ZOOM_SYM_SEQ_ENTITY,     BEHAVIOUR, 50, 54,   BELOW, 1, false, fzs_seq_entity),
    FZS("terminal",         ZOOM_SYM_TERMINAL,       ARCHITECTURE, 100, 70, BELOW, 1, false, fzs_terminal),
    FZS("key",              ZOOM_SYM_KEY,            ARCHITECTURE, 84, 36,  BELOW, 1, true,  fzs_key),
    FZS("envelope",         ZOOM_SYM_ENVELOPE,       ARCHITECTURE, 80, 54,  BELOW, 1, false, fzs_envelope),
    FZS("code",             ZOOM_SYM_CODE,           ARCHITECTURE, 70, 60,  BELOW, 1, false, fzs_code),
    FZS("users",            ZOOM_SYM_USERS,          ARCHITECTURE, 80, 70,  BELOW, 1, true,  fzs_users),
    FZS("event topic",      ZOOM_SYM_TOPIC,          ARCHITECTURE, 150, 70, BELOW, 1, false, fzs_topic),
    FZS("brace",            ZOOM_SYM_BRACE,          FLOW, 40, 120,  LEFT, 1, true,  fzs_brace),
    FZS("DFD data store",   ZOOM_SYM_DFD_STORE,      DATA, 150, 50,  INSIDE, 1, true,  fzs_dfd_store),
    FZS("area",             ZOOM_SYM_AREA,           PLANNING, 600, 400, CORNER, 1, false, fzs_area),   // (made by Insert's Area, not the library's)
    // Electronics, logic, chips and modules, boards (circuit.h).
    FZS("resistor",            ZOOM_SYM_E_RESISTOR,    ELECTRONICS, 60, 20, BELOW, 1, false, fzs_circuit),
    FZS("potentiometer",       ZOOM_SYM_E_POT,         ELECTRONICS, 60, 40, BELOW, 1, false, fzs_circuit),
    FZS("capacitor",           ZOOM_SYM_E_CAP,         ELECTRONICS, 40, 40, BELOW, 1, false, fzs_circuit),
    FZS("electrolytic",        ZOOM_SYM_E_ECAP,        ELECTRONICS, 40, 40, BELOW, 1, false, fzs_circuit),
    FZS("inductor",            ZOOM_SYM_E_IND,         ELECTRONICS, 60, 20, BELOW, 1, false, fzs_circuit),
    FZS("diode",               ZOOM_SYM_E_DIODE,       ELECTRONICS, 60, 30, BELOW, 1, false, fzs_circuit),
    FZS("LED",                 ZOOM_SYM_E_LED,         ELECTRONICS, 60, 40, BELOW, 1, false, fzs_circuit),
    FZS("zener",               ZOOM_SYM_E_ZENER,       ELECTRONICS, 60, 30, BELOW, 1, false, fzs_circuit),
    FZS("NPN",                 ZOOM_SYM_E_NPN,         ELECTRONICS, 60, 60, BELOW, 1, false, fzs_circuit),
    FZS("PNP",                 ZOOM_SYM_E_PNP,         ELECTRONICS, 60, 60, BELOW, 1, false, fzs_circuit),
    FZS("N-MOSFET",            ZOOM_SYM_E_NMOS,        ELECTRONICS, 60, 60, BELOW, 1, false, fzs_circuit),
    FZS("P-MOSFET",            ZOOM_SYM_E_PMOS,        ELECTRONICS, 60, 60, BELOW, 1, false, fzs_circuit),
    FZS("DC source",           ZOOM_SYM_E_VSRC,        ELECTRONICS, 40, 60, BELOW, 1, false, fzs_circuit),
    FZS("battery",             ZOOM_SYM_E_BATT,        ELECTRONICS, 40, 60, BELOW, 1, false, fzs_circuit),
    FZS("AC source",           ZOOM_SYM_E_AC,          ELECTRONICS, 40, 60, BELOW, 1, false, fzs_circuit),
    FZS("clock",               ZOOM_SYM_E_CLOCK,       ELECTRONICS, 40, 60, BELOW, 1, false, fzs_circuit),
    FZS("current source",      ZOOM_SYM_E_ISRC,        ELECTRONICS, 40, 60, BELOW, 1, false, fzs_circuit),
    FZS("ground",              ZOOM_SYM_E_GND,         ELECTRONICS, 40, 40, BELOW, 1, false, fzs_circuit),
    FZS("supply rail",         ZOOM_SYM_E_RAIL,        ELECTRONICS, 60, 20, INSIDE, 1, false, fzs_circuit),
    FZS("SPST switch",         ZOOM_SYM_E_SWITCH,      ELECTRONICS, 60, 40, BELOW, 1, false, fzs_circuit),
    FZS("push button",         ZOOM_SYM_E_BUTTON,      ELECTRONICS, 60, 40, BELOW, 1, false, fzs_circuit),
    FZS("lamp",                ZOOM_SYM_E_LAMP,        ELECTRONICS, 40, 40, BELOW, 1, false, fzs_circuit),
    FZS("motor",               ZOOM_SYM_E_MOTOR,       ELECTRONICS, 40, 40, BELOW, 1, false, fzs_circuit),
    FZS("buzzer",              ZOOM_SYM_E_BUZZER,      ELECTRONICS, 60, 40, BELOW, 1, false, fzs_circuit),
    FZS("fuse",                ZOOM_SYM_E_FUSE,        ELECTRONICS, 60, 20, BELOW, 1, false, fzs_circuit),
    FZS("op-amp",              ZOOM_SYM_E_OPAMP,       ELECTRONICS, 80, 80, BELOW, 1, false, fzs_circuit),
    FZS("voltmeter",           ZOOM_SYM_E_VOLT,        ELECTRONICS, 40, 40, BELOW, 1, false, fzs_circuit),
    FZS("ammeter",             ZOOM_SYM_E_AMP,         ELECTRONICS, 40, 40, BELOW, 1, false, fzs_circuit),
    FZS("7-segment",           ZOOM_SYM_E_7SEG,        ELECTRONICS, 60, 120, BELOW, 1, false, fzs_circuit),
    FZS("breadboard",          ZOOM_SYM_E_BREADBOARD,  ELECTRONICS, 330, 185, BELOW, 1, true, fzs_circuit),
    FZS("AND gate",            ZOOM_SYM_L_AND,         LOGIC, 80, 60, BELOW, 1, false, fzs_circuit),
    FZS("OR gate",             ZOOM_SYM_L_OR,          LOGIC, 80, 60, BELOW, 1, false, fzs_circuit),
    FZS("NOT gate",            ZOOM_SYM_L_NOT,         LOGIC, 80, 60, BELOW, 1, false, fzs_circuit),
    FZS("NAND gate",           ZOOM_SYM_L_NAND,        LOGIC, 80, 60, BELOW, 1, false, fzs_circuit),
    FZS("NOR gate",            ZOOM_SYM_L_NOR,         LOGIC, 80, 60, BELOW, 1, false, fzs_circuit),
    FZS("XOR gate",            ZOOM_SYM_L_XOR,         LOGIC, 80, 60, BELOW, 1, false, fzs_circuit),
    FZS("XNOR gate",           ZOOM_SYM_L_XNOR,        LOGIC, 80, 60, BELOW, 1, false, fzs_circuit),
    FZS("buffer",              ZOOM_SYM_L_BUF,         LOGIC, 80, 60, BELOW, 1, false, fzs_circuit),
    FZS("D flip-flop",         ZOOM_SYM_L_DFF,         LOGIC, 80, 80, BELOW, 1, false, fzs_circuit),
    FZS("T flip-flop",         ZOOM_SYM_L_TFF,         LOGIC, 80, 80, BELOW, 1, false, fzs_circuit),
    FZS("logic input",         ZOOM_SYM_L_IN,          LOGIC, 60, 40, BELOW, 1, false, fzs_circuit),
    FZS("logic probe",         ZOOM_SYM_L_OUT,         LOGIC, 40, 40, BELOW, 1, false, fzs_circuit),
    FZS("NE555",               ZOOM_SYM_C_555,         CHIPS, 200, 100, INSIDE, 1, true, fzs_circuit),
    FZS("LM358",               ZOOM_SYM_C_LM358,       CHIPS, 200, 100, INSIDE, 1, true, fzs_circuit),
    FZS("ATmega328P",          ZOOM_SYM_C_328P,        CHIPS, 220, 300, INSIDE, 1, true, fzs_circuit),
    FZS("74HC595",             ZOOM_SYM_C_595,         CHIPS, 200, 180, INSIDE, 1, true, fzs_circuit),
    FZS("L293D",               ZOOM_SYM_C_293,         CHIPS, 200, 180, INSIDE, 1, true, fzs_circuit),
    FZS("ULN2003",             ZOOM_SYM_C_2003,        CHIPS, 200, 180, INSIDE, 1, true, fzs_circuit),
    FZS("74HC00",              ZOOM_SYM_C_7400,        CHIPS, 200, 160, INSIDE, 1, true, fzs_circuit),
    FZS("74HC08",              ZOOM_SYM_C_7408,        CHIPS, 200, 160, INSIDE, 1, true, fzs_circuit),
    FZS("74HC32",              ZOOM_SYM_C_7432,        CHIPS, 200, 160, INSIDE, 1, true, fzs_circuit),
    FZS("74HC86",              ZOOM_SYM_C_7486,        CHIPS, 200, 160, INSIDE, 1, true, fzs_circuit),
    FZS("74HC04",              ZOOM_SYM_C_7404,        CHIPS, 200, 160, INSIDE, 1, true, fzs_circuit),
    FZS("MAX7219",             ZOOM_SYM_C_7219,        CHIPS, 220, 260, INSIDE, 1, true, fzs_circuit),
    FZS("PCF8574",             ZOOM_SYM_C_8574,        CHIPS, 200, 180, INSIDE, 1, true, fzs_circuit),
    FZS("7805",                ZOOM_SYM_C_7805,        CHIPS, 80, 40, INSIDE, 1, true, fzs_circuit),
    FZS("AMS1117",             ZOOM_SYM_C_1117,        CHIPS, 100, 40, INSIDE, 1, true, fzs_circuit),
    FZS("DS3231",              ZOOM_SYM_M_DS3231,      CHIPS, 200, 140, INSIDE, 1, true, fzs_circuit),
    FZS("MPU6050",             ZOOM_SYM_M_MPU,         CHIPS, 200, 180, INSIDE, 1, true, fzs_circuit),
    FZS("nRF24L01",            ZOOM_SYM_M_NRF,         CHIPS, 200, 180, INSIDE, 1, true, fzs_circuit),
    FZS("HC-05",               ZOOM_SYM_M_HC05,        CHIPS, 200, 140, INSIDE, 1, true, fzs_circuit),
    FZS("HC-SR04",             ZOOM_SYM_M_SR04,        CHIPS, 200, 100, INSIDE, 1, true, fzs_circuit),
    FZS("DHT11",               ZOOM_SYM_M_DHT,         CHIPS, 120, 80, INSIDE, 1, true, fzs_circuit),
    FZS("servo",               ZOOM_SYM_M_SERVO,       CHIPS, 120, 80, BELOW, 1, true, fzs_circuit),   // (its name under it: its horn turns over its middle)
    FZS("TP4056",              ZOOM_SYM_M_TP4056,      CHIPS, 200, 140, INSIDE, 1, true, fzs_circuit),
    FZS("WS2812B",             ZOOM_SYM_M_WS2812,      CHIPS, 120, 100, INSIDE, 1, true, fzs_circuit),
    FZS("L298N",               ZOOM_SYM_M_L298N,       CHIPS, 200, 280, INSIDE, 1, true, fzs_circuit),
    FZS("relay module",        ZOOM_SYM_M_RELAY,       CHIPS, 200, 140, INSIDE, 1, true, fzs_circuit),
    FZS("I2C LCD",             ZOOM_SYM_M_LCD,         CHIPS, 200, 100, INSIDE, 1, true, fzs_circuit),
    FZS("OLED",                ZOOM_SYM_M_OLED,        CHIPS, 200, 100, INSIDE, 1, true, fzs_circuit),
    FZS("Arduino Uno",         ZOOM_SYM_B_UNO,         BOARDS, 200, 340, INSIDE, 1, true, fzs_circuit),
    FZS("Arduino Nano",        ZOOM_SYM_B_NANO,        BOARDS, 180, 320, INSIDE, 1, true, fzs_circuit),
    FZS("Raspberry Pi Pico",   ZOOM_SYM_B_PICO,        BOARDS, 200, 420, INSIDE, 1, true, fzs_circuit),
    FZS("Raspberry Pi GPIO",   ZOOM_SYM_B_PI,          BOARDS, 220, 420, INSIDE, 1, true, fzs_circuit),
    FZS("ESP32",               ZOOM_SYM_B_ESP32,       BOARDS, 200, 320, INSIDE, 1, true, fzs_circuit),
    FZS("ESP8266",             ZOOM_SYM_B_8266,        BOARDS, 200, 320, INSIDE, 1, true, fzs_circuit),
    // Mechanisms (mech.h).
    FZS("link",                ZOOM_SYM_M_LINK,        MECHANISMS, 120, 24, BELOW, 1, false, fzs_mech),
    FZS("plate",               ZOOM_SYM_M_PLATE,       MECHANISMS, 100, 90, BELOW, 1, false, fzs_mech),
    FZS("gear 10T",            ZOOM_SYM_M_GEAR10,      MECHANISMS, 60, 60, BELOW, 1, false, fzs_mech),
    FZS("gear 15T",            ZOOM_SYM_M_GEAR15,      MECHANISMS, 85, 85, BELOW, 1, false, fzs_mech),
    FZS("gear 20T",            ZOOM_SYM_M_GEAR20,      MECHANISMS, 110, 110, BELOW, 1, false, fzs_mech),
    FZS("gear 30T",            ZOOM_SYM_M_GEAR30,      MECHANISMS, 160, 160, BELOW, 1, false, fzs_mech),
    FZS("gear 40T",            ZOOM_SYM_M_GEAR40,      MECHANISMS, 210, 210, BELOW, 1, false, fzs_mech),
    FZS("gear 60T",            ZOOM_SYM_M_GEAR60,      MECHANISMS, 310, 310, BELOW, 1, false, fzs_mech),
    FZS("fixed pivot",         ZOOM_SYM_M_PIVOT,       MECHANISMS, 40, 40, BELOW, 1, false, fzs_mech),
    FZS("drive motor",         ZOOM_SYM_M_MOTOR,       MECHANISMS, 60, 60, BELOW, 1, false, fzs_mech),
    FZS("spring",              ZOOM_SYM_M_SPRING,      MECHANISMS, 100, 24, BELOW, 1, false, fzs_mech),
    FZS("weight",              ZOOM_SYM_M_WEIGHT,      MECHANISMS, 40, 40, BELOW, 1, false, fzs_mech),
    FZS("wheel",               ZOOM_SYM_M_WHEEL,       MECHANISMS, 80, 80, BELOW, 1, false, fzs_mech),
    FZS("wall",                ZOOM_SYM_M_WALL,        MECHANISMS, 240, 20, BELOW, 1, false, fzs_mech),
    // Floor plans and house wiring (plan.h): a true-size one's w, h its centimetres (page_pieces.c puts it in so).
    FZS("door 70",             ZOOM_SYM_P_DOOR70,      FLOORPLAN, 70, 80, BELOW, 1, false, fzs_plan),
    FZS("door 80",             ZOOM_SYM_P_DOOR80,      FLOORPLAN, 80, 90, BELOW, 1, false, fzs_plan),
    FZS("door 90",             ZOOM_SYM_P_DOOR90,      FLOORPLAN, 90, 100, BELOW, 1, false, fzs_plan),
    FZS("double door",         ZOOM_SYM_P_DOOR2,       FLOORPLAN, 140, 80, BELOW, 1, false, fzs_plan),
    FZS("sliding door",        ZOOM_SYM_P_SLIDING,     FLOORPLAN, 160, 10, BELOW, 1, false, fzs_plan),
    FZS("window 60",           ZOOM_SYM_P_WIN60,       FLOORPLAN, 60, 10, BELOW, 1, false, fzs_plan),
    FZS("window 100",          ZOOM_SYM_P_WIN100,      FLOORPLAN, 100, 10, BELOW, 1, false, fzs_plan),
    FZS("window 120",          ZOOM_SYM_P_WIN120,      FLOORPLAN, 120, 10, BELOW, 1, false, fzs_plan),
    FZS("window 150",          ZOOM_SYM_P_WIN150,      FLOORPLAN, 150, 10, BELOW, 1, false, fzs_plan),
    FZS("window 200",          ZOOM_SYM_P_WIN200,      FLOORPLAN, 200, 10, BELOW, 1, false, fzs_plan),
    FZS("opening 90",          ZOOM_SYM_P_OPENING,     FLOORPLAN, 90, 10, BELOW, 1, false, fzs_plan),
    FZS("single bed",          ZOOM_SYM_P_BED1,        FLOORPLAN, 90, 190, BELOW, 1, false, fzs_plan),
    FZS("double bed",          ZOOM_SYM_P_BED2,        FLOORPLAN, 140, 190, BELOW, 1, false, fzs_plan),
    FZS("queen bed",           ZOOM_SYM_P_BED3,        FLOORPLAN, 160, 200, BELOW, 1, false, fzs_plan),
    FZS("sofa",                ZOOM_SYM_P_SOFA,        FLOORPLAN, 200, 90, BELOW, 1, false, fzs_plan),
    FZS("armchair",            ZOOM_SYM_P_ARMCHAIR,    FLOORPLAN, 80, 80, BELOW, 1, false, fzs_plan),
    FZS("dining table",        ZOOM_SYM_P_TABLE,       FLOORPLAN, 160, 90, BELOW, 1, false, fzs_plan),
    FZS("round table",         ZOOM_SYM_P_RTABLE,      FLOORPLAN, 100, 100, BELOW, 1, false, fzs_plan),
    FZS("chair",               ZOOM_SYM_P_CHAIR,       FLOORPLAN, 45, 50, BELOW, 1, false, fzs_plan),
    FZS("desk",                ZOOM_SYM_P_DESK,        FLOORPLAN, 140, 70, BELOW, 1, false, fzs_plan),
    FZS("wardrobe",            ZOOM_SYM_P_WARDROBE,    FLOORPLAN, 120, 60, BELOW, 1, false, fzs_plan),
    FZS("kitchen counter",     ZOOM_SYM_P_COUNTER,     FLOORPLAN, 240, 60, BELOW, 1, false, fzs_plan),
    FZS("kitchen sink",        ZOOM_SYM_P_SINK,        FLOORPLAN, 80, 50, BELOW, 1, false, fzs_plan),
    FZS("hob",                 ZOOM_SYM_P_HOB,         FLOORPLAN, 60, 60, BELOW, 1, false, fzs_plan),
    FZS("fridge",              ZOOM_SYM_P_FRIDGE,      FLOORPLAN, 60, 65, BELOW, 1, false, fzs_plan),
    FZS("washing machine",     ZOOM_SYM_P_WASHER,      FLOORPLAN, 60, 60, BELOW, 1, false, fzs_plan),
    FZS("toilet",              ZOOM_SYM_P_TOILET,      FLOORPLAN, 40, 65, BELOW, 1, false, fzs_plan),
    FZS("bathtub",             ZOOM_SYM_P_BATH,        FLOORPLAN, 170, 75, BELOW, 1, false, fzs_plan),
    FZS("shower",              ZOOM_SYM_P_SHOWER,      FLOORPLAN, 90, 90, BELOW, 1, false, fzs_plan),
    FZS("washbasin",           ZOOM_SYM_P_BASIN,       FLOORPLAN, 60, 45, BELOW, 1, false, fzs_plan),
    FZS("stairs",              ZOOM_SYM_P_STAIRS,      FLOORPLAN, 100, 280, BELOW, 1, false, fzs_plan),
    FZS("socket",              ZOOM_SYM_W_SOCKET,      WIRING, 40, 30, BELOW, 1, false, fzs_plan),
    FZS("double socket",       ZOOM_SYM_W_SOCKET2,     WIRING, 50, 30, BELOW, 1, false, fzs_plan),
    FZS("light switch",        ZOOM_SYM_W_SWITCH,      WIRING, 36, 36, BELOW, 1, false, fzs_plan),
    FZS("two-way switch",      ZOOM_SYM_W_SWITCH2,     WIRING, 36, 36, BELOW, 1, false, fzs_plan),
    FZS("dimmer",              ZOOM_SYM_W_DIMMER,      WIRING, 36, 36, BELOW, 1, false, fzs_plan),
    FZS("bell push",           ZOOM_SYM_W_BELL,        WIRING, 30, 30, BELOW, 1, false, fzs_plan),
    FZS("ceiling light",       ZOOM_SYM_W_LIGHT,       WIRING, 40, 40, BELOW, 1, false, fzs_plan),
    FZS("wall light",          ZOOM_SYM_W_WLIGHT,      WIRING, 40, 40, BELOW, 1, false, fzs_plan),
    FZS("tube light",          ZOOM_SYM_W_TUBE,        WIRING, 80, 20, BELOW, 1, false, fzs_plan),
    FZS("consumer unit",       ZOOM_SYM_W_BOARD,       WIRING, 60, 30, BELOW, 1, false, fzs_plan),
    FZS("junction box",        ZOOM_SYM_W_JUNCTION,    WIRING, 24, 24, BELOW, 1, false, fzs_plan),
    FZS("TV point",            ZOOM_SYM_W_TV,          WIRING, 36, 30, BELOW, 1, false, fzs_plan),
    FZS("data point",          ZOOM_SYM_W_DATA,        WIRING, 36, 30, BELOW, 1, false, fzs_plan),
    FZS("smoke detector",      ZOOM_SYM_W_SMOKE,       WIRING, 36, 36, BELOW, 1, false, fzs_plan),
    FZS("thermostat",          ZOOM_SYM_W_THERMO,      WIRING, 36, 36, BELOW, 1, false, fzs_plan),
    FZS("ceiling fan",         ZOOM_SYM_W_FAN,         WIRING, 50, 50, BELOW, 1, false, fzs_plan),
    FZS("meter",               ZOOM_SYM_W_METER,       WIRING, 44, 30, BELOW, 1, false, fzs_plan),
    // Maths (plot.h) and sewing.
    FZS("graph",               ZOOM_SYM_X_GRAPH,       MATHS, 320, 220, CORNER, 1, true, fzs_graph),
    FZS("axes",                ZOOM_SYM_X_AXES,        MATHS, 240, 240, CORNER, 1, true, fzs_graph),
    FZS("number line",         ZOOM_SYM_X_LINE,        MATHS, 320, 50, BELOW, 1, true, fzs_number_line),
    FZS("grainline",           ZOOM_SYM_S_GRAIN,       SEWING, 160, 20, BELOW, 1, true, fzs_grainline),
    FZS("place on fold",       ZOOM_SYM_S_FOLD,        SEWING, 160, 40, BELOW, 1, true, fzs_fold),
    FZS("notch",               ZOOM_SYM_S_NOTCH,       SEWING, 16, 20, BELOW, 1, true, fzs_notch),
    FZS("button",              ZOOM_SYM_S_BUTTON,      SEWING, 30, 30, BELOW, 1, false, fzs_button),
    FZS("buttonhole",          ZOOM_SYM_S_HOLE,        SEWING, 50, 12, BELOW, 1, true, fzs_buttonhole),
    FZS("dart",                ZOOM_SYM_S_DART,        SEWING, 40, 100, BELOW, 1, true, fzs_dart),
    FZS("pattern label",       ZOOM_SYM_S_LABEL,       SEWING, 180, 90, INSIDE, 1, true, fzs_pattern_label),
    // (0.1.49: what hangs, and what it hangs from)
    FZS("pulley",              ZOOM_SYM_M_PULLEY,      MECHANISMS, 80, 80, BELOW, 1, false, fzs_mech),
    FZS("rope",                ZOOM_SYM_M_ROPE,        MECHANISMS, 200, 12, BELOW, 1, false, fzs_mech),
    FZS("crate",               ZOOM_SYM_M_CRATE,       MECHANISMS, 60, 50, BELOW, 1, false, fzs_mech),
    // (0.1.50: what slides)
    FZS("rail",                ZOOM_SYM_M_RAIL,        MECHANISMS, 300, 16, BELOW, 1, false, fzs_mech),
    FZS("slider",              ZOOM_SYM_M_SLIDER,      MECHANISMS, 60, 36, BELOW, 1, false, fzs_mech),
    FZS("rack",                ZOOM_SYM_M_RACK,        MECHANISMS, 300, 30, BELOW, 1, false, fzs_mech),
    // (0.1.51: what joins drawn bodies)
    FZS("pin",                 ZOOM_SYM_M_PIN,         MECHANISMS, 16, 16, BELOW, 1, false, fzs_mech),
    // (0.1.51: the simulation's chips (sim/chips.c), and a person's own parts)
    FZS("74HC02",              ZOOM_SYM_C_7402,      CHIPS, 200, 160, INSIDE, 1, true, fzs_circuit),
    FZS("74HC74",              ZOOM_SYM_C_7474,      CHIPS, 200, 160, INSIDE, 1, true, fzs_circuit),
    FZS("74HC138",             ZOOM_SYM_C_74138,     CHIPS, 200, 180, INSIDE, 1, true, fzs_circuit),
    FZS("74HC157",             ZOOM_SYM_C_74157,     CHIPS, 200, 180, INSIDE, 1, true, fzs_circuit),
    FZS("74HC161",             ZOOM_SYM_C_74161,     CHIPS, 200, 180, INSIDE, 1, true, fzs_circuit),
    FZS("74HC173",             ZOOM_SYM_C_74173,     CHIPS, 200, 180, INSIDE, 1, true, fzs_circuit),
    FZS("74HC245",             ZOOM_SYM_C_74245,     CHIPS, 200, 220, INSIDE, 1, true, fzs_circuit),
    FZS("74HC283",             ZOOM_SYM_C_74283,     CHIPS, 200, 180, INSIDE, 1, true, fzs_circuit),
    FZS("74HC189",             ZOOM_SYM_C_74189,     CHIPS, 200, 180, INSIDE, 1, true, fzs_circuit),
    FZS("custom part",         ZOOM_SYM_L_CUSTOM,      LOGIC, 120, 120, INSIDE, 1, true, fzs_circuit),
    // Displays (display.h): the sized ones as their kind comes — 4 digits, 8 × 8, 16 × 2.
    FZS("7-segment panel",     ZOOM_SYM_E_SEGPANEL,    ELECTRONICS, 300, 180, BELOW, 1, true, fzs_circuit),
    FZS("LED matrix",          ZOOM_SYM_E_MATRIX,      ELECTRONICS, 200, 200, BELOW, 1, true, fzs_circuit),
    FZS("LED bar graph",       ZOOM_SYM_E_BARGRAPH,    ELECTRONICS, 140, 220, BELOW, 1, true, fzs_circuit),
    FZS("LM3914",              ZOOM_SYM_C_3914,        CHIPS, 200, 200, INSIDE, 1, true, fzs_circuit),
    FZS("CD4511",              ZOOM_SYM_C_4511,        CHIPS, 200, 180, INSIDE, 1, true, fzs_circuit),
    FZS("panel meter",         ZOOM_SYM_E_METER,       ELECTRONICS, 160, 80, BELOW, 1, true, fzs_circuit),
    FZS("character LCD",       ZOOM_SYM_M_CHARLCD,     CHIPS, 340, 130, BELOW, 1, true, fzs_circuit),
    FZS("oscilloscope",        ZOOM_SYM_E_SCOPE,       ELECTRONICS, 300, 160, BELOW, 1, true, fzs_circuit),
    FZS("tracer",              ZOOM_SYM_M_TRACER,      MECHANISMS, 16, 16, BELOW, 1, false, fzs_mech),
    FZS("hand crank",          ZOOM_SYM_M_CRANK,       MECHANISMS, 60, 60, BELOW, 1, false, fzs_mech),
    FZS("SPDT switch",         ZOOM_SYM_E_SPDT,        ELECTRONICS, 60, 60, BELOW, 1, false, fzs_circuit),
    FZS("DPDT switch",         ZOOM_SYM_E_DPDT,        ELECTRONICS, 60, 120, BELOW, 1, false, fzs_circuit),
    FZS("LDR",                 ZOOM_SYM_E_LDR,         ELECTRONICS, 60, 40, BELOW, 1, false, fzs_circuit),
    FZS("thermistor",          ZOOM_SYM_E_NTC,         ELECTRONICS, 60, 40, BELOW, 1, false, fzs_circuit),
    // (0.1.73: worked by, or working, a mechanism)
    FZS("solenoid",            ZOOM_SYM_E_SOLENOID,    ELECTRONICS, 100, 40, BELOW, 1, true, fzs_circuit),
    FZS("stepper motor",       ZOOM_SYM_E_STEPPER,     ELECTRONICS, 160, 120, BELOW, 1, true, fzs_circuit),
    FZS("rotary encoder",      ZOOM_SYM_E_ENCODER,     ELECTRONICS, 80, 80, BELOW, 1, true, fzs_circuit),
    FZS("slotted sensor",      ZOOM_SYM_E_SLOT,        ELECTRONICS, 80, 60, BELOW, 1, false, fzs_circuit),
    // (0.1.74: belts and chains, cams, ratchets, dampers, worms)
    FZS("belt",                ZOOM_SYM_M_BELT,        MECHANISMS, 240, 40, BELOW, 1, true, fzs_mech),
    FZS("chain",               ZOOM_SYM_M_CHAIN,       MECHANISMS, 240, 40, BELOW, 1, true, fzs_mech),
    FZS("sprocket",            ZOOM_SYM_M_SPROCKET,    MECHANISMS, 60, 60, BELOW, 1, false, fzs_mech),
    FZS("cam",                 ZOOM_SYM_M_CAM,         MECHANISMS, 80, 80, BELOW, 1, false, fzs_mech),
    FZS("follower",            ZOOM_SYM_M_FOLLOWER,    MECHANISMS, 120, 30, BELOW, 1, true, fzs_mech),
    FZS("ratchet",             ZOOM_SYM_M_RATCHET,     MECHANISMS, 80, 80, BELOW, 1, false, fzs_mech),
    FZS("pawl",                ZOOM_SYM_M_PAWL,        MECHANISMS, 70, 16, BELOW, 1, false, fzs_mech),
    FZS("damper",              ZOOM_SYM_M_DAMPER,      MECHANISMS, 120, 24, BELOW, 1, true, fzs_mech),
    FZS("worm",                ZOOM_SYM_M_WORM,        MECHANISMS, 160, 40, BELOW, 1, true, fzs_mech),
    // (0.1.75: more parts)
    FZS("JK flip-flop",        ZOOM_SYM_L_JK,          LOGIC, 80, 80, BELOW, 1, false, fzs_circuit),
    FZS("SR latch",            ZOOM_SYM_L_SR,          LOGIC, 80, 80, BELOW, 1, false, fzs_circuit),
    FZS("LM393",               ZOOM_SYM_C_LM393,       CHIPS, 200, 100, INSIDE, 1, true, fzs_circuit),
    FZS("SCR",                 ZOOM_SYM_E_SCR,         ELECTRONICS, 60, 40, BELOW, 1, false, fzs_circuit),
    FZS("TRIAC",               ZOOM_SYM_E_TRIAC,       ELECTRONICS, 60, 40, BELOW, 1, false, fzs_circuit),
    FZS("transformer",         ZOOM_SYM_E_XFMR,        ELECTRONICS, 80, 80, BELOW, 1, true, fzs_circuit),
    FZS("bridge rectifier",    ZOOM_SYM_E_BRIDGE,      ELECTRONICS, 80, 80, BELOW, 1, true, fzs_circuit),
    FZS("optocoupler",         ZOOM_SYM_E_OPTO,        ELECTRONICS, 100, 60, BELOW, 1, false, fzs_circuit),
    FZS("photodiode",          ZOOM_SYM_E_PHOTODIODE,  ELECTRONICS, 60, 40, BELOW, 1, false, fzs_circuit),
    FZS("phototransistor",     ZOOM_SYM_E_PHOTOTRANS,  ELECTRONICS, 60, 60, BELOW, 1, false, fzs_circuit),
    FZS("speaker",             ZOOM_SYM_E_SPEAKER,     ELECTRONICS, 60, 40, BELOW, 1, true, fzs_circuit),
    FZS("CD4017",              ZOOM_SYM_C_4017,        CHIPS, 200, 180, INSIDE, 1, true, fzs_circuit),
    // (0.1.85: brackets, on their own and in pairs)
    FZS("left parenthesis",    ZOOM_SYM_B_PAREN_L,     BRACKETS, 30, 150, OUT_LEFT, 1, true, fzs_paren_l),
    FZS("right parenthesis",   ZOOM_SYM_B_PAREN_R,     BRACKETS, 30, 150, OUT_RIGHT, 1, true, fzs_paren_r),
    FZS("left square bracket", ZOOM_SYM_B_SQUARE_L,    BRACKETS, 30, 150, OUT_LEFT, 1, true, fzs_square_l),
    FZS("right square bracket", ZOOM_SYM_B_SQUARE_R,   BRACKETS, 30, 150, OUT_RIGHT, 1, true, fzs_square_r),
    FZS("left brace",          ZOOM_SYM_B_CURLY_L,     BRACKETS, 30, 150, OUT_LEFT, 1, true, fzs_curly_l),
    FZS("right brace",         ZOOM_SYM_B_CURLY_R,     BRACKETS, 30, 150, OUT_RIGHT, 1, true, fzs_curly_r),
    FZS("left angle bracket",  ZOOM_SYM_B_ANGLE_L,     BRACKETS, 30, 150, OUT_LEFT, 1, true, fzs_angle_l),
    FZS("right angle bracket", ZOOM_SYM_B_ANGLE_R,     BRACKETS, 30, 150, OUT_RIGHT, 1, true, fzs_angle_r),
    FZS("vertical bar",        ZOOM_SYM_B_BAR,         BRACKETS, 20, 150, OUT_RIGHT, 1, true, fzs_vbar),
    FZS("double bar",          ZOOM_SYM_B_DOUBLE,      BRACKETS, 24, 150, OUT_RIGHT, 1, true, fzs_double_bar),
    FZS("left floor",          ZOOM_SYM_B_FLOOR_L,     BRACKETS, 30, 150, OUT_LEFT, 1, true, fzs_floor_l),
    FZS("right floor",         ZOOM_SYM_B_FLOOR_R,     BRACKETS, 30, 150, OUT_RIGHT, 1, true, fzs_floor_r),
    FZS("left ceiling",        ZOOM_SYM_B_CEILING_L,   BRACKETS, 30, 150, OUT_LEFT, 1, true, fzs_ceiling_l),
    FZS("right ceiling",       ZOOM_SYM_B_CEILING_R,   BRACKETS, 30, 150, OUT_RIGHT, 1, true, fzs_ceiling_r),
    FZS("under brace",         ZOOM_SYM_B_UNDER,       BRACKETS, 220, 30, BELOW, 1, true, fzs_brace_under),
    FZS("over brace",          ZOOM_SYM_B_OVER,        BRACKETS, 220, 30, ABOVE, 1, true, fzs_brace_over),
    FZS("parentheses",         ZOOM_SYM_B_PARENS,      BRACKETS, 180, 80, INSIDE, 1, true, fzs_pair_round),
    FZS("square brackets",     ZOOM_SYM_B_SQUARES,     BRACKETS, 180, 80, INSIDE, 1, true, fzs_pair_square),
    FZS("braces",              ZOOM_SYM_B_CURLIES,     BRACKETS, 180, 80, INSIDE, 1, true, fzs_pair_curly),
    FZS("angle brackets",      ZOOM_SYM_B_ANGLES,      BRACKETS, 180, 80, INSIDE, 1, true, fzs_pair_angle),
    FZS("bars",                ZOOM_SYM_B_BARS,        BRACKETS, 180, 80, INSIDE, 1, true, fzs_pair_bar),
    FZS("double bars",         ZOOM_SYM_B_DOUBLES,     BRACKETS, 180, 80, INSIDE, 1, true, fzs_pair_double),
    // (0.1.85: plain shapes)
    FZS("rectangle",           ZOOM_SYM_P_RECT,        SHAPES, 120, 76, INSIDE, 1, false, fzs_rectangle),
    FZS("rounded rectangle",   ZOOM_SYM_P_ROUNDED,     SHAPES, 120, 76, INSIDE, 1, false, fzs_round_rect),
    FZS("circle",              ZOOM_SYM_P_CIRCLE,      SHAPES, 90, 90, INSIDE, 1, false, fzs_circle),
    FZS("diamond",             ZOOM_SYM_P_DIAMOND,     SHAPES, 110, 90, INSIDE, 1, false, fzs_decision),
    FZS("parallelogram",       ZOOM_SYM_P_PARALLEL,    SHAPES, 130, 70, INSIDE, 1, false, fzs_parallelogram),
    FZS("trapezoid",           ZOOM_SYM_P_TRAPEZOID,   SHAPES, 130, 70, INSIDE, 1, false, fzs_trapezoid),
    FZS("isosceles triangle",  ZOOM_SYM_P_TRIANGLE,    SHAPES, 100, 86, INSIDE, 1, false, fzs_triangle),
    FZS("right triangle",      ZOOM_SYM_P_RIGHT_TRI,   SHAPES, 100, 86, INSIDE, 1, false, fzs_right_triangle),
    FZS("pentagon",            ZOOM_SYM_P_PENTAGON,    SHAPES, 100, 96, INSIDE, 1, false, fzs_pentagon),
    FZS("regular hexagon",     ZOOM_SYM_P_HEXAGON,     SHAPES, 110, 96, INSIDE, 1, false, fzs_hexagon),
    FZS("octagon",             ZOOM_SYM_P_OCTAGON,     SHAPES, 96, 96, INSIDE, 1, false, fzs_octagon),
    FZS("star",                ZOOM_SYM_P_STAR,        SHAPES, 100, 96, INSIDE, 1, false, fzs_star),
    FZS("burst",               ZOOM_SYM_P_BURST,       SHAPES, 110, 110, INSIDE, 1, false, fzs_burst),
    FZS("cross",               ZOOM_SYM_P_CROSS,       SHAPES, 90, 90, INSIDE, 1, false, fzs_cross),
    FZS("heart",               ZOOM_SYM_P_HEART,       SHAPES, 100, 90, INSIDE, 1, false, fzs_heart),
    FZS("cloud shape",         ZOOM_SYM_P_CLOUD,       SHAPES, 150, 96, INSIDE, 1, false, fzs_cloud),
    FZS("speech bubble",       ZOOM_SYM_P_SPEECH,      SHAPES, 150, 100, INSIDE, 1, false, fzs_speech),
    FZS("thought bubble",      ZOOM_SYM_P_THOUGHT,     SHAPES, 150, 110, INSIDE, 1, false, fzs_thought),
    FZS("arc",                 ZOOM_SYM_P_ARC,         SHAPES, 120, 60, BELOW, 1, true, fzs_arc),
    FZS("pie",                 ZOOM_SYM_P_PIE,         SHAPES, 96, 96, BELOW, 1, false, fzs_pie),
    FZS("ring",                ZOOM_SYM_P_RING,        SHAPES, 96, 96, BELOW, 1, false, fzs_ring),
    FZS("cylinder",            ZOOM_SYM_P_CYLINDER,    SHAPES, 90, 110, INSIDE, 1, false, fzs_cylinder),
    FZS("cube",                ZOOM_SYM_P_CUBE,        SHAPES, 110, 110, INSIDE, 1, false, fzs_cube),
    FZS("block arrow",         ZOOM_SYM_P_ARROW,       SHAPES, 150, 70, INSIDE, 1, false, fzs_block_arrow),
    FZS("double block arrow",  ZOOM_SYM_P_DOUBLE_ARROW, SHAPES, 170, 70, INSIDE, 1, false, fzs_double_arrow),
    FZS("curved arrow",        ZOOM_SYM_P_CURVED_ARROW, SHAPES, 120, 90, BELOW, 1, false, fzs_curved_arrow),
    FZS("chevron",             ZOOM_SYM_P_CHEVRON,     SHAPES, 130, 70, INSIDE, 1, false, fzs_chevron),
    // (0.1.86: Venn diagrams, charts, geometry's marks, science, stamps)
    FZS("venn 2",              ZOOM_SYM_P_VENN2,       SHAPES, 240, 150, REGIONS, 3, true, fzs_venn2),
    FZS("venn 3",              ZOOM_SYM_P_VENN3,       SHAPES, 220, 210, REGIONS, 4, true, fzs_venn3),
    FZS("bar chart",           ZOOM_SYM_K_BARS,        CHARTS, 320, 220, CORNER, 1, true, fzs_graph),
    FZS("line chart",          ZOOM_SYM_K_LINES,       CHARTS, 320, 220, CORNER, 1, true, fzs_graph),
    FZS("pie chart",           ZOOM_SYM_K_PIE,         CHARTS, 280, 240, CORNER, 1, true, fzs_graph),
    FZS("angle mark",          ZOOM_SYM_G_ANGLE,       GEOMETRY, 90, 70, BELOW, 1, true, fzs_angle_mark),
    FZS("right angle mark",    ZOOM_SYM_G_RIGHT,       GEOMETRY, 80, 80, BELOW, 1, true, fzs_right_angle_mark),
    FZS("equal mark",          ZOOM_SYM_G_TICK1,       GEOMETRY, 8, 24, BELOW, 1, true, fzs_tick1),
    FZS("double equal mark",   ZOOM_SYM_G_TICK2,       GEOMETRY, 16, 24, BELOW, 1, true, fzs_tick2),
    FZS("triple equal mark",   ZOOM_SYM_G_TICK3,       GEOMETRY, 22, 24, BELOW, 1, true, fzs_tick3),
    FZS("parallel mark",       ZOOM_SYM_G_PAR1,        GEOMETRY, 16, 20, BELOW, 1, true, fzs_parallel1),
    FZS("double parallel mark", ZOOM_SYM_G_PAR2,       GEOMETRY, 26, 20, BELOW, 1, true, fzs_parallel2),
    FZS("point",               ZOOM_SYM_G_POINT,       GEOMETRY, 16, 16, OUT_RIGHT, 1, true, fzs_point),
    FZS("segment",             ZOOM_SYM_G_SEGMENT,     GEOMETRY, 160, 14, BELOW, 1, true, fzs_segment),
    FZS("ray",                 ZOOM_SYM_G_RAY,         GEOMETRY, 160, 14, BELOW, 1, true, fzs_ray),
    FZS("line",                ZOOM_SYM_G_LINE,        GEOMETRY, 180, 14, BELOW, 1, true, fzs_line_both),
    FZS("vector",              ZOOM_SYM_G_VECTOR,      GEOMETRY, 120, 70, ABOVE, 1, true, fzs_vector),
    FZS("circle and radius",   ZOOM_SYM_G_RADIUS,      GEOMETRY, 120, 120, BELOW, 1, false, fzs_circle_radius),
    FZS("protractor",          ZOOM_SYM_G_PROTRACTOR,  GEOMETRY, 220, 110, BELOW, 1, false, fzs_protractor),
    FZS("beaker",              ZOOM_SYM_S_BEAKER,      SCIENCE, 80, 100, BELOW, 1, true, fzs_beaker),
    FZS("flask",               ZOOM_SYM_S_FLASK,       SCIENCE, 90, 110, BELOW, 1, true, fzs_flask),
    FZS("round flask",         ZOOM_SYM_S_ROUND_FLASK, SCIENCE, 90, 120, BELOW, 1, true, fzs_round_flask),
    FZS("test tube",           ZOOM_SYM_S_TUBE,        SCIENCE, 30, 120, BELOW, 1, true, fzs_test_tube),
    FZS("measuring cylinder",  ZOOM_SYM_S_CYLINDER,    SCIENCE, 50, 140, BELOW, 1, true, fzs_cylinder_measure),
    FZS("funnel",              ZOOM_SYM_S_FUNNEL,      SCIENCE, 90, 100, BELOW, 1, true, fzs_funnel),
    FZS("burner",              ZOOM_SYM_S_BURNER,      SCIENCE, 60, 120, BELOW, 1, true, fzs_burner),
    FZS("convex lens",         ZOOM_SYM_S_CONVEX,      SCIENCE, 36, 130, BELOW, 1, false, fzs_lens_convex),
    FZS("concave lens",        ZOOM_SYM_S_CONCAVE,     SCIENCE, 40, 130, BELOW, 1, false, fzs_lens_concave),
    FZS("mirror",              ZOOM_SYM_S_MIRROR,      SCIENCE, 30, 130, BELOW, 1, true, fzs_mirror_flat),
    FZS("curved mirror",       ZOOM_SYM_S_MIRROR2,     SCIENCE, 40, 130, BELOW, 1, true, fzs_mirror_curved),
    FZS("prism",               ZOOM_SYM_S_PRISM,       SCIENCE, 110, 100, BELOW, 1, false, fzs_prism),
    FZS("light source",        ZOOM_SYM_S_LAMP,        SCIENCE, 80, 80, BELOW, 1, true, fzs_lamp),
    FZS("atom",                ZOOM_SYM_S_ATOM,        SCIENCE, 110, 110, BELOW, 1, true, fzs_atom),
    FZS("benzene ring",        ZOOM_SYM_S_BENZENE,     SCIENCE, 90, 100, BELOW, 1, false, fzs_benzene),
    FZS("cell",                ZOOM_SYM_S_CELL,        SCIENCE, 140, 100, BELOW, 1, false, fzs_cell),
    FZS("DNA",                 ZOOM_SYM_S_DNA,         SCIENCE, 180, 60, BELOW, 1, true, fzs_dna),
    FZS("thermometer",         ZOOM_SYM_S_THERMO,      SCIENCE, 36, 130, BELOW, 1, true, fzs_thermometer),
    FZS("check stamp",         ZOOM_SYM_T_CHECK,       STAMPS, 60, 60, BELOW, 1, true, fzs_stamp_check),
    FZS("cross stamp",         ZOOM_SYM_T_CROSS,       STAMPS, 60, 60, BELOW, 1, true, fzs_stamp_cross),
    FZS("warning stamp",       ZOOM_SYM_T_WARNING,     STAMPS, 66, 60, BELOW, 1, false, fzs_stamp_warning),
    FZS("question stamp",      ZOOM_SYM_T_QUESTION,    STAMPS, 46, 66, BELOW, 1, true, fzs_stamp_question),
    FZS("exclamation stamp",   ZOOM_SYM_T_EXCLAIM,     STAMPS, 30, 66, BELOW, 1, true, fzs_stamp_exclamation),
    FZS("info stamp",          ZOOM_SYM_T_INFO,        STAMPS, 60, 60, BELOW, 1, false, fzs_stamp_info),
    FZS("star stamp",          ZOOM_SYM_T_STAR,        STAMPS, 64, 62, BELOW, 1, false, fzs_stamp_star),
    FZS("heart stamp",         ZOOM_SYM_T_HEART,       STAMPS, 62, 56, BELOW, 1, false, fzs_stamp_heart),
    FZS("idea stamp",          ZOOM_SYM_T_BULB,        STAMPS, 50, 70, BELOW, 1, true, fzs_stamp_bulb),
    FZS("flag stamp",          ZOOM_SYM_T_FLAG,        STAMPS, 54, 66, BELOW, 1, true, fzs_stamp_flag),
    FZS("pin stamp",           ZOOM_SYM_T_PIN,         STAMPS, 44, 66, BELOW, 1, false, fzs_stamp_pin),
    FZS("clock stamp",         ZOOM_SYM_T_CLOCK,       STAMPS, 60, 60, BELOW, 1, false, fzs_stamp_clock),
};

// How many symbols there are.
#define FUDE_ZOOM_SYMBOL_N ((u32)(sizeof(FUDE_ZOOM_SYMBOLS) / sizeof(FUDE_ZOOM_SYMBOLS[0])))
// A compile-time check that it is symbol.h's count (the size is -1, which does not compile, when it is not): the
// names' table (page_diagrams.c) is checked against the same count.
typedef char fzs_symbols_as_symbol_h[(FUDE_ZOOM_SYMBOL_N == FUDE_ZOOM_SYMBOL_KINDS) ? 1 : -1];

// The symbols' ids to their kinds (an owned string to a u32), made the first time one is looked for and kept for the
// run, on the std allocator (main thread only, as the header says). The map's labels, the library's counts and a
// container's test ask for kinds by id for every object they look at, every frame: a map finds each at once, where
// going down the table compared every id before it. An id in the table twice would keep its first kind, as that
// search did (rde_hash_map_add keeps the key it has).
RDE_INTERNAL rde_hash_map fzs_kind_of;

// See symbol.h. Found by its id, whole, in fzs_kind_of.
u32 fude_zoom_symbol_find(const c8* _id) {
    if(_id == NULL) {
        return FUDE_ZOOM_NONE;
    }
    if(fzs_kind_of.memory == NULL) {
        fzs_kind_of = rde_hash_map_new_ex(RDE_HASH_MAP_SLOT_STR_OWNED, 0, RDE_HASH_MAP_SLOT_BYTES, sizeof(u32), NULL, NULL, NULL,
                                          rde_memory_allocator_get_default_std());
        rde_hash_map_reserve(&fzs_kind_of, FUDE_ZOOM_SYMBOL_N);
        for(u32 _k = 0; _k < FUDE_ZOOM_SYMBOL_N; _k++) {
            rde_hash_map_add(&fzs_kind_of, (const any)FUDE_ZOOM_SYMBOLS[_k].info.id, (const any)&_k);
        }
    }
    const u32* _kind = (const u32*)rde_hash_map_find(&fzs_kind_of, (const any)_id);
    return _kind != NULL ? *_kind : FUDE_ZOOM_NONE;
}

// See symbol.h.
u32 fude_zoom_symbol_count(void) {
    return FUDE_ZOOM_SYMBOL_N;
}

// See symbol.h.
const fude_zoom_symbol_info* fude_zoom_symbol_info_of(u32 _kind) {
    return _kind < FUDE_ZOOM_SYMBOL_N ? &FUDE_ZOOM_SYMBOLS[_kind].info : NULL;
}

// --- the topics' parts: one kind-to-part map ------------------------------------------------------------------------

// See symbol.h: by the kind's family.
u8 fude_zoom_symbol_topic(u32 _kind) {
    const fude_zoom_symbol_info* _info = fude_zoom_symbol_info_of(_kind);
    switch(_info != NULL ? _info->family : (u8)FUDE_ZOOM_SYMBOL_FAMILIES) {
    case FUDE_ZOOM_SYMBOL_FAMILY_ELECTRONICS:
    case FUDE_ZOOM_SYMBOL_FAMILY_LOGIC:
    case FUDE_ZOOM_SYMBOL_FAMILY_CHIPS:
    case FUDE_ZOOM_SYMBOL_FAMILY_BOARDS:      return FUDE_ZOOM_SYMBOL_TOPIC_CIRCUIT;
    case FUDE_ZOOM_SYMBOL_FAMILY_MECHANISMS:  return FUDE_ZOOM_SYMBOL_TOPIC_MECH;
    case FUDE_ZOOM_SYMBOL_FAMILY_FLOORPLAN:
    case FUDE_ZOOM_SYMBOL_FAMILY_WIRING:      return FUDE_ZOOM_SYMBOL_TOPIC_PLAN;
    default:                                  return FUDE_ZOOM_SYMBOL_TOPIC_NONE;
    }
}

// The one table: each kind's part in its topic's table (fzs_topic_parts), and whether it has been looked for yet
// (fzs_topic_known), a miss kept as NULL. One entry a kind of the library: its bound is the table's (a kind is its
// place in FUDE_ZOOM_SYMBOLS, which only grows with a build).
RDE_INTERNAL const any fzs_topic_parts[FUDE_ZOOM_SYMBOL_N];
RDE_INTERNAL u8        fzs_topic_known[FUDE_ZOOM_SYMBOL_N];

// See symbol.h. A kind's part is looked for by its id in its topic's table the first time it is asked for, then kept.
const any fude_zoom_symbol_topic_part(u32 _kind, u8 _topic) {
    if(_topic == FUDE_ZOOM_SYMBOL_TOPIC_NONE || fude_zoom_symbol_topic(_kind) != _topic) {
        return NULL;   // (no kind, or another topic's: none of this topic's parts has its id)
    }
    if(!fzs_topic_known[_kind]) {
        const c8* _id = FUDE_ZOOM_SYMBOLS[_kind].info.id;
        fzs_topic_parts[_kind] = _topic == FUDE_ZOOM_SYMBOL_TOPIC_CIRCUIT ? (const any)fude_zoom_part_find(_id) :
                                 _topic == FUDE_ZOOM_SYMBOL_TOPIC_MECH    ? (const any)fude_zoom_mech_find(_id) : (const any)fude_zoom_plan_find(_id);
        fzs_topic_known[_kind] = 1u;
    }
    return fzs_topic_parts[_kind];
}

// The app's art hook (fude_zoom_symbol_set_art), or NULL.
RDE_INTERNAL fude_zoom_symbol_art_fn fzs_art = NULL;

// See symbol.h.
void fude_zoom_symbol_set_art(fude_zoom_symbol_art_fn _fn) {
    fzs_art = _fn;
}

// See symbol.h. The art is asked first; when it has nothing for the kind, the table's draw function draws it. Both get
// positive half sizes and the segments kept within 16 to 256.
u32 fude_zoom_symbol_parts(u32 _kind, f64 _hw, f64 _hh, u32 _segments, rde_arr* _points, rde_arr* _parts) {
    rde_arr_clear(_points);
    rde_arr_clear(_parts);
    if(_kind >= FUDE_ZOOM_SYMBOL_N) {
        return 0;
    }
    if(fzs_art != NULL) {
        // (its art, where the app has it)
        const u32 _n = fzs_art(_kind, rde_math_abs_f64(_hw), rde_math_abs_f64(_hh), _segments < 16u ? 16u : (_segments > 256u ? 256u : _segments), _points, _parts);
        if(_n > 0u) {
            return _n;
        }
        rde_arr_clear(_points);
        rde_arr_clear(_parts);
    }
    fude_zoom_sym _s = { _points, _parts, rde_math_abs_f64(_hw), rde_math_abs_f64(_hh), _segments < 16u ? 16u : (_segments > 256u ? 256u : _segments), 0u, _kind };
    FUDE_ZOOM_SYMBOLS[_kind].draw(&_s);
    return (u32)rde_arr_length(_parts);
}

// See symbol.h.
u32 fude_zoom_symbol_outline(u32 _kind, f64 _hw, f64 _hh, u32 _segments, rde_arr* _points) {
    rde_arr_clear(_points);
    if(_kind >= FUDE_ZOOM_SYMBOL_N) {
        return 0;
    }
    const f64 _w = rde_math_abs_f64(_hw), _h = rde_math_abs_f64(_hh);
    if(FUDE_ZOOM_SYMBOLS[_kind].info.box_hull) {
        const fude_zoom_v2 _c[4] = { { -_w, -_h }, { _w, -_h }, { _w, _h }, { -_w, _h } };
        for(u32 _i = 0; _i < 4u; _i++) {
            rde_arr_add(_points, (any)&_c[_i]);
        }
        return 4u;
    }
    rde_arr _all = rde_arr_new(sizeof(fude_zoom_v2), rde_memory_allocator_get_default_std()), _parts = rde_arr_new(sizeof(fude_zoom_symbol_part), rde_memory_allocator_get_default_std());
    if(fude_zoom_symbol_parts(_kind, _w, _h, _segments, &_all, &_parts) > 0u) {
        const fude_zoom_symbol_part* _p = (const fude_zoom_symbol_part*)_parts.memory;
        for(u32 _i = 0; _i < _p[0].count; _i++) {
            rde_arr_add(_points, (any)&((const fude_zoom_v2*)_all.memory)[_p[0].first + _i]);
        }
    }
    rde_arr_free(&_all);
    rde_arr_free(&_parts);
    return (u32)rde_arr_length(_points);
}

// See symbol.h. Each place has its own boxes; a few symbols' inside boxes are their own (a person's body, a browser's
// page, a task card's clear of its tag, a speech bubble's body, a thought bubble's cloud, a cube's front, a pair of
// brackets' inside, a decision's middle).
u32 fude_zoom_symbol_text_boxes(u32 _kind, f64 _hw, f64 _hh, u32 _count, fude_zoom_box* _out, u32 _max) {
    const fude_zoom_symbol_info* _info = fude_zoom_symbol_info_of(_kind);
    if(_info == NULL || _max == 0u) {
        return 0;
    }
    const f64 _w = rde_math_abs_f64(_hw), _h = rde_math_abs_f64(_hh);
    _count = _count < _info->boxes ? _info->boxes : _count;
    _count = _count > _max ? _max : _count;
    const f64 _pad = rde_math_min_f64(_w, _h) * 0.12;
    switch(_info->place) {
        case FUDE_ZOOM_SYMBOL_TEXT_BELOW:
            _out[0] = (fude_zoom_box){ -_w * 2.5, -_h - rde_math_max_f64(_h * 0.9, 26.0), _w * 2.5, -_h - 2.0 };
            return 1u;
        case FUDE_ZOOM_SYMBOL_TEXT_HEADER: {
            const f64 _band = rde_math_min_f64(_h * 0.32, 44.0);
            _out[0] = (fude_zoom_box){ -_w + _pad, _h - _band, _w - _pad, _h };
            return 1u;
        }
        case FUDE_ZOOM_SYMBOL_TEXT_LEFT: {
            const f64 _band = rde_math_min_f64(_w * 0.16, 34.0);
            _out[0] = (fude_zoom_box){ -_w, -_h, -_w + _band, _h };
            return 1u;
        }
        case FUDE_ZOOM_SYMBOL_TEXT_OUT_LEFT:
        case FUDE_ZOOM_SYMBOL_TEXT_OUT_RIGHT: {
            // (beside it, as tall as it is: its text in the middle of its height, against it)
            const f64 _gap = rde_math_max_f64(_w * 0.3, 4.0), _wide = rde_math_max_f64(_h * 1.2, 180.0);
            _out[0] = _info->place == FUDE_ZOOM_SYMBOL_TEXT_OUT_LEFT ? (fude_zoom_box){ -_w - _gap - _wide, -_h, -_w - _gap, _h }
                                                                      : (fude_zoom_box){ _w + _gap, -_h, _w + _gap + _wide, _h };
            return 1u;
        }
        case FUDE_ZOOM_SYMBOL_TEXT_ABOVE:
            _out[0] = (fude_zoom_box){ -_w * 2.5, _h + 2.0, _w * 2.5, _h + rde_math_max_f64(_h * 0.9, 26.0) };
            return 1u;
        case FUDE_ZOOM_SYMBOL_TEXT_REGIONS: {
            // A Venn diagram's regions: each circle's own part, then where they all meet.
            u32 _n = 0;
            if(_kind < FUDE_ZOOM_SYMBOL_N && FUDE_ZOOM_SYMBOLS[_kind].draw == fzs_venn3) {
                const f64 _r = fzs_venn3_r(_w, _h), _d = _r * 0.62;
                for(u32 _i = 0; _i < 3u && _n < _max; _i++) {
                    const f64 _a = RDE_PI * 0.5 + 2.0 * RDE_PI * (f64)_i / 3.0;
                    const f64 _cx = rde_math_cos_f64(_a) * (_d + _r * 0.42), _cy = rde_math_sin_f64(_a) * (_d + _r * 0.42) - _h * 0.05;
                    _out[_n++] = (fude_zoom_box){ _cx - _r * 0.5, _cy - _r * 0.3, _cx + _r * 0.5, _cy + _r * 0.3 };
                }
                if(_n < _max) {
                    _out[_n++] = (fude_zoom_box){ -_r * 0.3, -_r * 0.25 - _h * 0.05, _r * 0.3, _r * 0.25 - _h * 0.05 };
                }
                return _n;
            }
            const f64 _r = fzs_venn2_r(_w, _h), _d = _w - _r, _lens = rde_math_max_f64(_r - _d, _r * 0.2);
            const fude_zoom_box _q[3] = { { -_w + _r * 0.1, -_r * 0.5, -_d - _lens * 0.5 + _r * 0.05, _r * 0.5 }, { -_lens * 0.9, -_r * 0.5, _lens * 0.9, _r * 0.5 },
                                          { _d + _lens * 0.5 - _r * 0.05, -_r * 0.5, _w - _r * 0.1, _r * 0.5 } };
            for(u32 _i = 0; _i < 3u && _n < _max; _i++) {
                _out[_n++] = _q[_i];
            }
            return _n;
        }
        case FUDE_ZOOM_SYMBOL_TEXT_CORNER: {
            const f64 _in = rde_math_min_f64(_w, _h) * 0.035;   // (from its top left, a little in)
            _out[0] = (fude_zoom_box){ -_w + _in, -_h + _in, _w - _in, _h - _in };
            return 1u;
        }
        case FUDE_ZOOM_SYMBOL_TEXT_PARTS: {
            if(_info->boxes == 4u) {
                // A 2 × 2: its quarters, top left first.
                const fude_zoom_box _q[4] = { { -_w, 0, 0, _h }, { 0, 0, _w, _h }, { -_w, -_h, 0, 0 }, { 0, -_h, _w, 0 } };
                const u32 _n = _max < 4u ? _max : 4u;
                for(u32 _i = 0; _i < _n; _i++) {
                    _out[_i] = (fude_zoom_box){ _q[_i].min_x + _pad, _q[_i].min_y + _pad, _q[_i].max_x - _pad, _q[_i].max_y - _pad };
                }
                return _n;
            }
            // Compartments: the first (a name) a band, the rest share what is left.
            const f64 _first = rde_math_min_f64(_h * 2.0 / (f64)(_count + 1u), 40.0);
            _out[0] = (fude_zoom_box){ -_w + _pad, _h - _first, _w - _pad, _h };
            for(u32 _i = 1; _i < _count; _i++) {
                const f64 _each = (2.0 * _h - _first) / (f64)(_count - 1u);
                const f64 _top  = _h - _first - (f64)(_i - 1u) * _each;
                _out[_i] = (fude_zoom_box){ -_w + _pad, _top - _each, _w - _pad, _top };
            }
            return _count;
        }
        default:
            break;
    }
    // Inside: its middle (a person's body, a browser's page, a lifeline's head: their own).
    if(_kind < FUDE_ZOOM_SYMBOL_N && FUDE_ZOOM_SYMBOLS[_kind].draw == fzs_person) {
        _out[0] = (fude_zoom_box){ -_w + _pad, -_h + _pad, _w - _pad, _h * 0.35 };
    } else if(_kind < FUDE_ZOOM_SYMBOL_N && FUDE_ZOOM_SYMBOLS[_kind].draw == fzs_browser) {
        _out[0] = (fude_zoom_box){ -_w + _pad, -_h + _pad, _w - _pad, _h * 0.6 };
    } else if(_kind < FUDE_ZOOM_SYMBOL_N && FUDE_ZOOM_SYMBOLS[_kind].draw == fzs_task_card) {
        const f64 _tag = rde_math_min_f64(_w * 0.05, 6.0) * 1.5;   // (clear of its tag down its left: a task's box beside it)
        _out[0] = (fude_zoom_box){ -_w + _tag + _pad, -_h + _pad, _w - _pad, _h - _pad };
    } else if(_kind < FUDE_ZOOM_SYMBOL_N && FUDE_ZOOM_SYMBOLS[_kind].draw == fzs_speech) {
        _out[0] = (fude_zoom_box){ -_w + _pad, -_h * FZS_BUBBLE_FOOT + _pad, _w - _pad, _h - _pad };   // (its body)
    } else if(_kind < FUDE_ZOOM_SYMBOL_N && FUDE_ZOOM_SYMBOLS[_kind].draw == fzs_thought) {
        _out[0] = (fude_zoom_box){ -_w * 0.7, -_h * 0.3, _w * 0.7, _h * 0.75 };
    } else if(_kind < FUDE_ZOOM_SYMBOL_N && FUDE_ZOOM_SYMBOLS[_kind].draw == fzs_cube) {
        _out[0] = (fude_zoom_box){ -_w + _pad, -_h + _pad, _w * (1.0 - FZS_CUBE_DEPTH) - _pad, _h * (1.0 - FZS_CUBE_DEPTH) - _pad };   // (its front)
    } else if(_info->family == FUDE_ZOOM_SYMBOL_FAMILY_BRACKETS) {
        const f64 _in = fzs_pair_width(_w, _h) + _pad;   // (between its pair)
        _out[0] = (fude_zoom_box){ -_w + _in, -_h + _pad, _w - _in, _h - _pad };
    } else if(_kind < FUDE_ZOOM_SYMBOL_N && FUDE_ZOOM_SYMBOLS[_kind].draw == fzs_decision) {
        _out[0] = (fude_zoom_box){ -_w * 0.6, -_h * 0.6, _w * 0.6, _h * 0.6 };
    } else {
        _out[0] = (fude_zoom_box){ -_w + _pad, -_h + _pad, _w - _pad, _h - _pad };
    }
    return 1u;
}

// --- a connector's ends -------------------------------------------------------------------------

// See symbol.h. The heads are drawn back from the tip along the line, _size their unit.
f64 fude_zoom_symbol_head(u32 _style, b8 _dot, fude_zoom_v2 _tip, fude_zoom_v2 _from, f64 _size, rde_arr* _points, rde_arr* _parts) {
    rde_arr_clear(_points);
    rde_arr_clear(_parts);
    const f64 _len = rde_math_hypot_f64(_tip.x - _from.x, _tip.y - _from.y);
    if(!(_len > 0.0) || !(_size > 0.0)) {
        return 0.0;
    }
    fude_zoom_sym _s = { _points, _parts, 1.0, 1.0, 32u, 0u, 0u };
    const fude_zoom_v2 _u = { (_tip.x - _from.x) / _len, (_tip.y - _from.y) / _len }, _n = { -_u.y, _u.x };
    // A point _back behind the tip along the line, _side across it.
    #define FZH(_back, _side) fude_zoom_sym_pt(&_s, _tip.x - _u.x * (_back) + _n.x * (_side), _tip.y - _u.y * (_back) + _n.y * (_side))
    #define FZH_LINE(_b0, _s0, _b1, _s1) do { fude_zoom_sym_begin(&_s); FZH(_b0, _s0); FZH(_b1, _s1); fude_zoom_sym_end(&_s, 0u); } while(0)
    #define FZH_CIRCLE(_back, _r, _flags) fude_zoom_sym_ellipse(&_s, _tip.x - _u.x * (_back), _tip.y - _u.y * (_back), (_r), (_r), (u8)(_flags))
    #define FZH_CROW(_apex) do { FZH_LINE(_apex, 0.0, 0.0, _size * 0.6); FZH_LINE(_apex, 0.0, 0.0, 0.0); FZH_LINE(_apex, 0.0, 0.0, -_size * 0.6); } while(0)
    const f64 _l = _size;
    switch(_style) {
        case FUDE_ZOOM_HEAD_OPEN:
            fude_zoom_sym_begin(&_s); FZH(_l, _l * 0.5); FZH(0.0, 0.0); FZH(_l, -_l * 0.5); fude_zoom_sym_end(&_s, 0u);
            return 0.0;
        case FUDE_ZOOM_HEAD_HOLLOW:
            fude_zoom_sym_begin(&_s); FZH(0.0, 0.0); FZH(_l * 1.1, _l * 0.55); FZH(_l * 1.1, -_l * 0.55); fude_zoom_sym_end(&_s, FZS_OUT);
            return _l * 1.1;
        case FUDE_ZOOM_HEAD_DIAMOND:
        case FUDE_ZOOM_HEAD_HOLLOW_DIAMOND:
            fude_zoom_sym_begin(&_s); FZH(0.0, 0.0); FZH(_l * 0.9, _l * 0.45); FZH(_l * 1.8, 0.0); FZH(_l * 0.9, -_l * 0.45);
            fude_zoom_sym_end(&_s, _style == FUDE_ZOOM_HEAD_DIAMOND ? FZS_SOLID : FZS_OUT);
            return _l * 1.8;
        case FUDE_ZOOM_HEAD_DOT:
            FZH_CIRCLE(_l * 0.35, _l * 0.35, FZS_SOLID);
            return 0.0;
        case FUDE_ZOOM_HEAD_RING:
            FZH_CIRCLE(_l * 0.4, _l * 0.4, FZS_OUT);
            return _l * 0.8;
        case FUDE_ZOOM_HEAD_BAR:
            FZH_LINE(_l * 0.15, _l * 0.5, _l * 0.15, -_l * 0.5);
            return 0.0;
        case FUDE_ZOOM_HEAD_CROSS:
            FZH_LINE(_l * 0.3, _l * 0.35, _l * 1.0, -_l * 0.35);
            FZH_LINE(_l * 0.3, -_l * 0.35, _l * 1.0, _l * 0.35);
            return 0.0;
        case FUDE_ZOOM_HEAD_ONE:
            FZH_LINE(_l * 0.7, _l * 0.55, _l * 0.7, -_l * 0.55);
            return 0.0;
        case FUDE_ZOOM_HEAD_ONLY_ONE:
            FZH_LINE(_l * 0.5, _l * 0.55, _l * 0.5, -_l * 0.55);
            FZH_LINE(_l * 0.9, _l * 0.55, _l * 0.9, -_l * 0.55);
            return 0.0;
        case FUDE_ZOOM_HEAD_MANY:
            FZH_CROW(_l * 1.2);
            return 0.0;
        case FUDE_ZOOM_HEAD_ONE_MANY:
            FZH_CROW(_l * 1.2);
            FZH_LINE(_l * 1.5, _l * 0.55, _l * 1.5, -_l * 0.55);
            return 0.0;
        case FUDE_ZOOM_HEAD_ZERO_ONE:
            FZH_LINE(_l * 0.6, _l * 0.55, _l * 0.6, -_l * 0.55);
            FZH_CIRCLE(_l * 1.4, _l * 0.4, FZS_OUT);
            return 0.0;
        case FUDE_ZOOM_HEAD_ZERO_MANY:
            FZH_CROW(_l * 1.2);
            FZH_CIRCLE(_l * 1.65, _l * 0.4, FZS_OUT);
            return 0.0;
        default:
            if(_dot) {
                FZH_CIRCLE(_l * 0.35, _l * 0.35, FZS_SOLID);
                return 0.0;
            }
            fude_zoom_sym_begin(&_s); FZH(0.0, 0.0); FZH(_l, _l * 0.45); FZH(_l, -_l * 0.45); fude_zoom_sym_end(&_s, FZS_SOLID);
            return _l * 0.8;
    }
    #undef FZH
    #undef FZH_LINE
    #undef FZH_CIRCLE
    #undef FZH_CROW
}

// --- its numbers --------------------------------------------------------------------------------

// See symbol.h. The text's bytes, its 0 included, are copied into the numbers' memory 8 to a number.
u32 fude_zoom_symbol_numbers(f64* _n, u32 _kind, f64 _hw, f64 _hh, f64 _px, const c8* _text) {
    rde_memset(_n, 0, 4u * sizeof(f64));
    _n[0] = (f64)_kind;
    _n[1] = _hw;
    _n[2] = _hh;
    _n[3] = _px;
    usize _len = _text != NULL ? rde_strlen(_text) : 0u;
    if(_len > FUDE_ZOOM_SYMBOL_TEXT - 1u) {
        _len = FUDE_ZOOM_SYMBOL_TEXT - 1u;
        while(_len > 0 && ((u8)_text[_len] & 0xC0u) == 0x80u) {
            _len--;
        }
    }
    const u32 _words = (u32)((_len + 1u + 7u) / 8u);
    u8 _bytes[FUDE_ZOOM_SYMBOL_TEXT];
    rde_memset(_bytes, 0, (usize)_words * 8u);
    if(_len > 0) {
        rde_memcpy(_bytes, _text, _len);
    }
    rde_memcpy(&_n[4], _bytes, (usize)_words * 8u);
    return 4u + _words;
}

// See symbol.h. The text ends at its 0, or where the numbers end.
void fude_zoom_symbol_text(const f64* _n, u32 _count, c8* _out, usize _size) {
    if(_size == 0) {
        return;
    }
    _out[0] = 0;
    if(_count <= 4u) {
        return;
    }
    const u32 _most  = FUDE_ZOOM_SYMBOL_TEXT / 8u;
    const u32 _words = _count - 4u > _most ? _most : _count - 4u;
    const c8* _bytes = (const c8*)&_n[4];
    usize _len = rde_strnlen(_bytes, (usize)_words * 8u);
    if(_len >= _size) {
        _len = _size - 1u;
        while(_len > 0 && ((u8)_bytes[_len] & 0xC0u) == 0x80u) {
            _len--;
        }
    }
    rde_memcpy(_out, _bytes, _len);
    _out[_len] = 0;
}

// See symbol.h. The line breaks just before a "---" line are left out of the part before it.
u32 fude_zoom_symbol_text_split(const c8* _text, u32* _from, u32* _len, u32 _max) {
    if(_max == 0u) {
        return 0;
    }
    u32 _n = 0, _start = 0, _i = 0;
    const u32 _all = (u32)rde_strlen(_text);
    while(_i <= _all) {
        // A line of its own that is "---" ends a part.
        const u32 _line = _i;
        while(_i < _all && _text[_i] != '\n') {
            _i++;
        }
        const b8 _rule = _i - _line == 3u && rde_strncmp(&_text[_line], "---", 3) == 0;
        if(_rule && _n + 1u < _max) {
            u32 _end = _line;
            while(_end > _start && (_text[_end - 1u] == '\n')) {
                _end--;
            }
            _from[_n] = _start;
            _len[_n]  = _end - _start;
            _n++;
            _start = _i + 1u < _all ? _i + 1u : _all;
        }
        _i++;
    }
    _from[_n] = _start > _all ? _all : _start;
    _len[_n]  = _all - _from[_n];
    return _n + 1u;
}
