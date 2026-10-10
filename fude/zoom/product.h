// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_PRODUCT
#define FUDE_ZOOM_PRODUCT

#include "rde.h"

// ===========================================================================
// PRODUCTS: the apps made from the deep-zoom canvas (docs/product_split.md).
// One engine; each app offers its own topics (page.h's FUDE_ZOOM_TOPIC_), and in
// them only its own tools, Insert entries, instruments and exports — what a
// topic lists, and the app allows (product ∩ topic). Sketching is all of them;
// Notes is for writing, study and maths; Workshop for projects that work
// (circuits, mechanisms, plans, wood, sewing). The same canvas file in each: a
// canvas from another opens, its topic one of this app's if it is not.
// ===========================================================================

// What a canvas is for (page.c's topics): the bar's tools for it.
typedef enum {
    FUDE_ZOOM_TOPIC_GENERAL = 0,   // every tool, as the bar always was
    FUDE_ZOOM_TOPIC_TECHNICAL,     // plans to measure: sheets, instruments, dimensions, DXF, STL
    FUDE_ZOOM_TOPIC_WOOD,          // boards, joints, the saw, the cut list
    FUDE_ZOOM_TOPIC_DIAGRAMS,      // the diagram library, Kanban, areas, arranging
    FUDE_ZOOM_TOPIC_PDF,           // reading a PDF: its pages, notes, highlights
    FUDE_ZOOM_TOPIC_ELECTRONICS,   // circuits: parts, boards, wires, simulated (circuit.h)
    FUDE_ZOOM_TOPIC_MECHANISMS,    // linkages, gears, springs run as rigid bodies (mech.h)
    FUDE_ZOOM_TOPIC_FLOORPLAN,     // walls, doors, windows, furniture at true size, rooms' areas (plan.h)
    FUDE_ZOOM_TOPIC_WIRING,        // a house's electrics on its plan (plan.h)
    FUDE_ZOOM_TOPIC_MATHS,         // graphs of functions, axes, geometry's instruments (plot.h)
    FUDE_ZOOM_TOPIC_SEWING,        // patterns: pieces, seam allowances, marks, printed at true size
    FUDE_ZOOM_TOPIC_COUNT
} FUDE_ZOOM_TOPIC_;

typedef enum {
    FUDE_ZOOM_PRODUCT_SKETCHING = 0,   // every topic and tool (the app as it has been)
    FUDE_ZOOM_PRODUCT_NOTES,
    FUDE_ZOOM_PRODUCT_WORKSHOP,
    FUDE_ZOOM_PRODUCT_COUNT
} FUDE_ZOOM_PRODUCT_;

// The bar's app tools (page.c's FUDE_ZOOM_BASE_TOOLS, in its order).
typedef enum {
    FUDE_ZOOM_TOOL_SHAPES = 0, FUDE_ZOOM_TOOL_INSERT, FUDE_ZOOM_TOOL_SMOOTHING, FUDE_ZOOM_TOOL_FILL, FUDE_ZOOM_TOOL_ARRANGE,
    FUDE_ZOOM_TOOL_INSTRUMENTS, FUDE_ZOOM_TOOL_LAYERS, FUDE_ZOOM_TOOL_EXPORT, FUDE_ZOOM_TOOL_CIRCUIT, FUDE_ZOOM_TOOL_MOTION,
    FUDE_ZOOM_TOOL_PLAN, FUDE_ZOOM_TOOL_PAGES,
    FUDE_ZOOM_TOOL_COUNT
} FUDE_ZOOM_TOOL_;

// Insert's entries (page.c's FUDE_ZOOM_PICTURE_CHOICES, in its order).
typedef enum {
    FUDE_ZOOM_INSERT_PHOTOS = 0, FUDE_ZOOM_INSERT_FILES, FUDE_ZOOM_INSERT_TEXT, FUDE_ZOOM_INSERT_STICKY, FUDE_ZOOM_INSERT_MERMAID,
    FUDE_ZOOM_INSERT_BOARD, FUDE_ZOOM_INSERT_HOLES, FUDE_ZOOM_INSERT_PDF, FUDE_ZOOM_INSERT_FINGERS, FUDE_ZOOM_INSERT_DOVETAIL,
    FUDE_ZOOM_INSERT_DIAGRAM, FUDE_ZOOM_INSERT_KANBAN, FUDE_ZOOM_INSERT_AREA, FUDE_ZOOM_INSERT_PIECES, FUDE_ZOOM_INSERT_SHEET,
    FUDE_ZOOM_INSERT_CIRCUIT_PARTS, FUDE_ZOOM_INSERT_MECH_PARTS, FUDE_ZOOM_INSERT_PLAN_PARTS, FUDE_ZOOM_INSERT_MATHS,
    FUDE_ZOOM_INSERT_SEWING, FUDE_ZOOM_INSERT_EXAMPLES,
    FUDE_ZOOM_INSERT_COUNT
} FUDE_ZOOM_INSERT_;

// Instruments (page.c's FUDE_ZOOM_INSTRUMENT_CHOICES, in its order).
typedef enum {
    FUDE_ZOOM_TOOLKIT_RULER = 0, FUDE_ZOOM_TOOLKIT_SQUARE_45, FUDE_ZOOM_TOOLKIT_SQUARE_30, FUDE_ZOOM_TOOLKIT_PROTRACTOR,
    FUDE_ZOOM_TOOLKIT_COMPASS, FUDE_ZOOM_TOOLKIT_CIRCLES, FUDE_ZOOM_TOOLKIT_ELLIPSES, FUDE_ZOOM_TOOLKIT_FRENCH_CURVE,
    FUDE_ZOOM_TOOLKIT_CORNER, FUDE_ZOOM_TOOLKIT_STENCIL, FUDE_ZOOM_TOOLKIT_TAPE, FUDE_ZOOM_TOOLKIT_DIMENSION, FUDE_ZOOM_TOOLKIT_SAW,
    FUDE_ZOOM_TOOLKIT_GUIDE, FUDE_ZOOM_TOOLKIT_TRIM,
    FUDE_ZOOM_TOOLKIT_COUNT
} FUDE_ZOOM_TOOLKIT_;

// Exports (page.c's FUDE_ZOOM_EXPORT_CHOICES, in its order).
typedef enum {
    FUDE_ZOOM_OUT_PNG = 0, FUDE_ZOOM_OUT_SVG, FUDE_ZOOM_OUT_PDF, FUDE_ZOOM_OUT_DXF, FUDE_ZOOM_OUT_A4, FUDE_ZOOM_OUT_LETTER,
    FUDE_ZOOM_OUT_CUT_LIST, FUDE_ZOOM_OUT_VIDEO, FUDE_ZOOM_OUT_AREAS, FUDE_ZOOM_OUT_SHEETS, FUDE_ZOOM_OUT_STL, FUDE_ZOOM_OUT_PARTS,
    FUDE_ZOOM_OUT_COUNT
} FUDE_ZOOM_OUT_;

typedef struct {
    u32 topics;        // its topics (a bit each: 1 << FUDE_ZOOM_TOPIC_), in the Topic menu in their order
    u8  first;         // a canvas's topic when its own is not one of them
    u32 tools;         // its app tools (1 << FUDE_ZOOM_TOOL_)
    u32 inserts;       // ...Insert's entries (1 << FUDE_ZOOM_INSERT_)
    u32 instruments;   // ...instruments (1 << FUDE_ZOOM_TOOLKIT_)
    u32 exports;       // ...exports (1 << FUDE_ZOOM_OUT_)
    b8  hobby;         // a project's lasso actions: a part's limits, Make part, Make body
    b8  workshop;      // the workshop's themes (Kraft, Blueprint, Cutting mat)
} fude_zoom_product;

// The app's (set once, before the page is made: the shell's), and which it is.
void                     fude_zoom_product_set(u8 _which);
const fude_zoom_product* fude_zoom_product_get(void);
u8                       fude_zoom_product_which(void);
const fude_zoom_product* fude_zoom_product_of(u8 _which);

// Is topic _topic one of the app's? And the topic a canvas saved as _topic opens in.
b8 fude_zoom_product_has_topic(u8 _topic);
u8 fude_zoom_product_topic(u8 _topic);

// Of a topic's list _list (_count entries, a table's indices), those the app allows (bits _allowed) into _out, in
// order; how many.
u32 fude_zoom_product_keep(const u8* _list, u32 _count, u32 _allowed, u8* _out);

#endif
