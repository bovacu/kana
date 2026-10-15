// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_ZOOM_PRODUCT
#define FUDE_ZOOM_PRODUCT

#include "rde.h"

// ===========================================================================
// Products: the apps made from the deep-zoom canvas (docs/product_split.md). There is one engine; each app offers its
// own topics (FUDE_ZOOM_TOPIC_, below), and in them only its own tools, Insert entries, instruments and exports: what a
// topic lists and the app allows (the product's set and the topic's, together). Sketching has all of them (the
// developer's build); Notes (InfiNote) is for writing, study and maths; Workshop (InfiniWorkshop) is for projects that
// work (circuits, mechanisms, plans, wood, sewing) and graphs; diagrams and Kanban are in both. Every app opens the
// same canvas file: a canvas made in another app opens, its topic changed to one of this app's if it is not one.
//
// Where it sits: zoom, the canvas apps' layer. The shell (zoom/shell.c) sets the product once, from the app's
// description (apps/<app>/<app>.c); the page asks it what to offer (page_topics.c, for the bar; page_audio.c and
// page_edit.c, for their own buttons).
// Its main types and their lifecycle: each fude_zoom_product is a constant table entry in product.c; nothing is made
// or freed.
// Threads: main thread. The product is set once at start, before the page is made, and only read after.
// Design: see docs/infinite_canvas_design.md §60 and §62.
// ===========================================================================

// What a canvas is for (page_topics.c's topics): which of the bar's tools it shows.
typedef enum {
    FUDE_ZOOM_TOPIC_GENERAL = 0,   // Every tool, as the bar always was.
    FUDE_ZOOM_TOPIC_TECHNICAL,     // Plans to measure: sheets, instruments, dimensions, DXF, STL.
    FUDE_ZOOM_TOPIC_WOOD,          // Boards, joints, the saw, the cut list.
    FUDE_ZOOM_TOPIC_DIAGRAMS,      // The diagram library, Kanban, areas, arranging.
    FUDE_ZOOM_TOPIC_PDF,           // Reading a PDF: its pages, notes, highlights.
    FUDE_ZOOM_TOPIC_ELECTRONICS,   // Circuits: parts, boards, wires, simulated (circuit.h).
    FUDE_ZOOM_TOPIC_MECHANISMS,    // Linkages, gears and springs run as rigid bodies (mech.h).
    FUDE_ZOOM_TOPIC_FLOORPLAN,     // Walls, doors, windows, furniture at true size, rooms' areas (plan.h).
    FUDE_ZOOM_TOPIC_WIRING,        // A house's electrics on its plan (plan.h).
    FUDE_ZOOM_TOPIC_MATHS,         // Graphs of functions, axes, geometry's instruments (plot.h).
    FUDE_ZOOM_TOPIC_SEWING,        // Patterns: pieces, seam allowances, marks, printed at true size.
    FUDE_ZOOM_TOPIC_COUNT
} FUDE_ZOOM_TOPIC_;

// Which app this is.
typedef enum {
    FUDE_ZOOM_PRODUCT_SKETCHING = 0,   // Every topic and tool (the app as it has always been).
    FUDE_ZOOM_PRODUCT_NOTES,
    FUDE_ZOOM_PRODUCT_WORKSHOP,
    FUDE_ZOOM_PRODUCT_COUNT
} FUDE_ZOOM_PRODUCT_;

// The bar's app tools (page_topics.c's FUDE_ZOOM_BASE_TOOLS, in its order).
typedef enum {
    FUDE_ZOOM_TOOL_SHAPES = 0, FUDE_ZOOM_TOOL_INSERT, FUDE_ZOOM_TOOL_SMOOTHING, FUDE_ZOOM_TOOL_FILL, FUDE_ZOOM_TOOL_ARRANGE,
    FUDE_ZOOM_TOOL_INSTRUMENTS, FUDE_ZOOM_TOOL_LAYERS, FUDE_ZOOM_TOOL_EXPORT, FUDE_ZOOM_TOOL_CIRCUIT, FUDE_ZOOM_TOOL_MOTION,
    FUDE_ZOOM_TOOL_PLAN, FUDE_ZOOM_TOOL_PAGES,
    FUDE_ZOOM_TOOL_COUNT
} FUDE_ZOOM_TOOL_;

// Insert's entries (page_pictures.c's FUDE_ZOOM_PICTURE_CHOICES, in its order).
typedef enum {
    FUDE_ZOOM_INSERT_PHOTOS = 0, FUDE_ZOOM_INSERT_FILES, FUDE_ZOOM_INSERT_TEXT, FUDE_ZOOM_INSERT_STICKY, FUDE_ZOOM_INSERT_MERMAID,
    FUDE_ZOOM_INSERT_BOARD, FUDE_ZOOM_INSERT_HOLES, FUDE_ZOOM_INSERT_PDF, FUDE_ZOOM_INSERT_FINGERS, FUDE_ZOOM_INSERT_DOVETAIL,
    FUDE_ZOOM_INSERT_DIAGRAM, FUDE_ZOOM_INSERT_KANBAN, FUDE_ZOOM_INSERT_AREA, FUDE_ZOOM_INSERT_PIECES, FUDE_ZOOM_INSERT_SHEET,
    FUDE_ZOOM_INSERT_CIRCUIT_PARTS, FUDE_ZOOM_INSERT_MECH_PARTS, FUDE_ZOOM_INSERT_PLAN_PARTS, FUDE_ZOOM_INSERT_MATHS,
    FUDE_ZOOM_INSERT_SEWING, FUDE_ZOOM_INSERT_EXAMPLES, FUDE_ZOOM_INSERT_FORMULA,
    FUDE_ZOOM_INSERT_COUNT
} FUDE_ZOOM_INSERT_;

// The instruments (page_instruments.c's FUDE_ZOOM_INSTRUMENT_CHOICES, in its order).
typedef enum {
    FUDE_ZOOM_TOOLKIT_RULER = 0, FUDE_ZOOM_TOOLKIT_SQUARE_45, FUDE_ZOOM_TOOLKIT_SQUARE_30, FUDE_ZOOM_TOOLKIT_PROTRACTOR,
    FUDE_ZOOM_TOOLKIT_COMPASS, FUDE_ZOOM_TOOLKIT_CIRCLES, FUDE_ZOOM_TOOLKIT_ELLIPSES, FUDE_ZOOM_TOOLKIT_FRENCH_CURVE,
    FUDE_ZOOM_TOOLKIT_CORNER, FUDE_ZOOM_TOOLKIT_STENCIL, FUDE_ZOOM_TOOLKIT_TAPE, FUDE_ZOOM_TOOLKIT_DIMENSION, FUDE_ZOOM_TOOLKIT_SAW,
    FUDE_ZOOM_TOOLKIT_GUIDE, FUDE_ZOOM_TOOLKIT_TRIM,
    FUDE_ZOOM_TOOLKIT_COUNT
} FUDE_ZOOM_TOOLKIT_;

// The exports (page_export.c's FUDE_ZOOM_EXPORT_CHOICES, in its order).
typedef enum {
    FUDE_ZOOM_OUT_PNG = 0, FUDE_ZOOM_OUT_SVG, FUDE_ZOOM_OUT_PDF, FUDE_ZOOM_OUT_DXF, FUDE_ZOOM_OUT_A4, FUDE_ZOOM_OUT_LETTER,
    FUDE_ZOOM_OUT_CUT_LIST, FUDE_ZOOM_OUT_VIDEO, FUDE_ZOOM_OUT_AREAS, FUDE_ZOOM_OUT_SHEETS, FUDE_ZOOM_OUT_STL, FUDE_ZOOM_OUT_PARTS,
    FUDE_ZOOM_OUT_COUNT
} FUDE_ZOOM_OUT_;

// What an app offers. Each set is a bit mask, a bit for each value of its enum (1 << value).
typedef struct {
    u32 topics;        // Its topics (1 << FUDE_ZOOM_TOPIC_), shown in the Topic menu in their order.
    u8  first;         // The topic a canvas opens in when its own is not one of the app's.
    u32 tools;         // Its app tools (1 << FUDE_ZOOM_TOOL_).
    u32 inserts;       // Its Insert entries (1 << FUDE_ZOOM_INSERT_).
    u32 instruments;   // Its instruments (1 << FUDE_ZOOM_TOOLKIT_).
    u32 exports;       // Its exports (1 << FUDE_ZOOM_OUT_).
    b8  hobby;         // Whether it has a project's lasso actions: a part's limits, Make part, Make body.
    b8  workshop;      // Whether it has the workshop's themes (Kraft, Blueprint, Cutting mat).
    // What it calls its General and Maths topics (FUDE_TEXT_ ids): Notes' General is "Notes" and Workshop's "Sketch";
    // Workshop's Maths is "Graphs", since it has the graphs but not the formulas.
    u32 general_name;
    u32 maths_name;
    b8  audio;         // Whether it records sound with the ink (a notes app's: not Workshop's).
} fude_zoom_product;

// fude_zoom_product_set sets which app this is (FUDE_ZOOM_PRODUCT_; an unknown value means Sketching), once, from the
// shell, before the page is made. fude_zoom_product_get returns the app's description: the canvas asks it what the app
// offers, never which app it is. fude_zoom_product_of returns any product's (Sketching's for an unknown value). The
// pointers returned are to constant tables and stay valid for the app's life.
void                     fude_zoom_product_set(u8 _which);
const fude_zoom_product* fude_zoom_product_get(void);
const fude_zoom_product* fude_zoom_product_of(u8 _which);

// fude_zoom_product_has_topic returns whether topic _topic is one of the app's. fude_zoom_product_topic returns the
// topic a canvas saved with topic _topic opens in: its own when the app has it, else the app's first.
b8 fude_zoom_product_has_topic(u8 _topic);
u8 fude_zoom_product_topic(u8 _topic);

// Copies into _out, in order, the entries of a topic's list _list (_count entries, each an index into one of the enums
// above) that the app allows (the bit mask _allowed). Returns how many were copied.
u32 fude_zoom_product_keep(const u8* _list, u32 _count, u32 _allowed, u8* _out);

#endif
