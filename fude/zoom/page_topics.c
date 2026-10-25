// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/page_parts.h"

// ===========================================================================
// Page, the bar's tools: the Motion and Pages tools' choices, the base tools (FUDE_ZOOM_BASE_TOOLS), and topics
// (the tools a canvas shows for what it is for).
//
// In order: the Motion tool, the Pages tool, the table of every base tool, the topics' table, then filling the bar
// (fude_zoom_topic_fill) for the canvas's topic as the app has it (product.h), which the app's shell asks for every
// frame (fude_zoom_page_topic_follow). The bar's tables are file-level arrays filled again whenever the topic changes.
//
// Part of the page driver: page.h says what the page is and holds its public functions; page_parts.h holds what the
// modules share (the functions one calls in another, the shared state, constants and types) and says which module
// holds what. Main thread only, as the whole page.
// ===========================================================================

// The Motion tool (the Mechanisms topic): Play runs what the lasso holds; Parts opens the diagram library at the
// mechanisms' tabs. Returns false: the toolbar's panel is not kept open.
b8 fude_zoom_motion_choose(fude_app* _app, u32 _index) {
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL) {
        return false;
    }
    if(_index == 0u) {
        fude_zoom_page_play_selection_now(_page);
    } else if(_index == 1u) {
        fude_zoom_page_library_open_tabs(_page, FUDE_ZOOM_PAGE_LIBRARY_MECHANISMS);
    }
    fude_toolbar_refresh(&_app->ui->bar);
    return false;
}

// Play is lit while a mechanism plays.
RDE_INTERNAL b8 fude_zoom_motion_chosen(const fude_app* _app, u32 _index) {
    RDE_UNUSED(_app);
    const fude_zoom_page* _page = fude_zoom_page_the;
    return _page != NULL && _index == 0u && _page->play_on && _page->play_mech;
}

// The Motion tool is lit while a mechanism plays.
RDE_INTERNAL b8 fude_zoom_motion_selected(const fude_app* _app) {
    RDE_UNUSED(_app);
    return fude_zoom_page_the != NULL && fude_zoom_page_the->play_on && fude_zoom_page_the->play_mech;
}

// The Motion tool's choices, in the order fude_zoom_motion_choose takes them.
RDE_INTERNAL const fude_extension_choice FUDE_ZOOM_MOTION_CHOICES[] = {
    { FUDE_TEXT_ZOOM_PLAY,        FUDE_ICON_PLAY,   fude_zoom_motion_choose },
    { FUDE_TEXT_ZOOM_INSERT_MECH, FUDE_ICON_ROTATE, fude_zoom_motion_choose },
};

// Pages (pages.h): makes the canvas a notebook of A4, A5, Letter or square pages, or changes its page size (its
// drawing staying where it is); adds a page at its end (and flies to it); or makes it the endless canvas again (its
// drawing kept, its pages gone). Pages begin on an empty canvas, or one whose drawing all lies on the first page: one
// drawn on elsewhere keeps what it has where it is, so a notice says so instead. Its choices are the sizes, then Add
// a page, then Endless.
#define FUDE_ZOOM_PAGES_ADD     FUDE_ZOOM_PAGES_SIZES
#define FUDE_ZOOM_PAGES_ENDLESS (FUDE_ZOOM_PAGES_SIZES + 1u)

// See Pages above. Does nothing while a PDF is open. Returns false: the toolbar's panel is not kept open. At
// FUDE_ZOOM_PAGES_MOST pages (pages.h: a bound on a damaged file's count and on the loops over pages, which no
// notebook reaches by hand), Add a page adds none: it flies to the last and says the notebook is full.
b8 fude_zoom_pages_choose(fude_app* _app, u32 _i) {
    RDE_UNUSED(_app);
    fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL || !_page->open || fude_zoom_pdfview_is_open(&_page->pdf)) {
        return false;
    }
    fude_zoom_pages* _p = &_page->pages;
    if(_i == FUDE_ZOOM_PAGES_ADD) {
        if(!_p->on) {
            fude_notice_show(fude_text(FUDE_TEXT_ZOOM_PAGES_SIZE_FIRST));
            return false;
        }
        const b8 _room = _p->count < FUDE_ZOOM_PAGES_MOST;
        if(_room) {
            _p->count++;
            fude_zoom_page_pages_save(_page);
            // A step of its own, with nothing in it but the page: undone, the page goes.
            fude_zoom_history_push_tagged(&_page->scene, _page->scene.home, fude_zoom_pages_box(_p, _p->count - 1u), NULL, 0u, NULL, 0u, NULL, NULL, NULL, 0u,
                                          fude_zoom_page_pages_tag(1));
        }
        // Flies to it: its top at the top of the screen, a page 94% of the screen's width.
        const fude_zoom_v2  _half = fude_zoom_page_half(_page);
        const fude_zoom_box _new  = fude_zoom_pages_box(_p, _p->count - 1u);
        const f64 _z = _half.x * 2.0 * 0.94 / rde_math_max_f64(_new.max_x - _new.min_x, 1e-9);
        fude_zoom_page_fly(_page, (fude_zoom_camera){ _page->scene.home, { 0.0, _new.max_y - (_half.y - 16.0) / _z }, _z }, true);
        c8 _say[200];
        if(_room) {
            FUDE_TEXTF(_say, FUDE_TEXT_ZOOM_PAGES_ADDED, FUDE_TN(_p->count));
        } else {
            FUDE_TEXTF(_say, FUDE_TEXT_ZOOM_PAGES_FULL, FUDE_TN(FUDE_ZOOM_PAGES_MOST));
        }
        fude_notice_show(_say);
        return false;
    }
    if(_i >= FUDE_ZOOM_PAGES_SIZES) {
        if(_p->on) {
            _p->on = false;
            fude_zoom_page_pages_save(_page);
            fude_notice_show(fude_text(FUDE_TEXT_ZOOM_PAGES_OFF));
        }
        return false;
    }
    if(!_p->on) {
        fude_zoom_pages _try = *_p;
        fude_zoom_pages_size(&_try, (u8)_i);
        const fude_zoom_box _drawn = fude_zoom_nav_drawn(&_page->scene, _page->scene.home, FUDE_ZOOM_NONE);
        const fude_zoom_box _first = fude_zoom_pages_box(&_try, 0u);
        if(!fude_zoom_box_is_empty(_drawn) && !(_drawn.min_x >= _first.min_x && _drawn.max_x <= _first.max_x && _drawn.min_y >= _first.min_y && _drawn.max_y <= _first.max_y)) {
            fude_notice_show(fude_text(FUDE_TEXT_ZOOM_PAGES_DRAWN));
            return false;
        }
        *_p = _try;
        _p->count = 1u;
        fude_zoom_page_pages_first_view(_page);
    } else {
        fude_zoom_pages_size(_p, (u8)_i);
    }
    _page->pages_rev = 0xFFFFFFFFu;   // Unlike any revision, so the page count is brought up to date for this size.
    fude_zoom_page_pages_save(_page);
    c8 _say[160];
    FUDE_TEXTF(_say, FUDE_TEXT_ZOOM_PAGES_NOW, FUDE_TS(fude_text(FUDE_TEXT_ZOOM_PAGES_A4 + _i)));
    fude_notice_show(_say);
    return false;
}
// A size is lit when the canvas is a notebook of that size, Endless when it is not a notebook; Add a page never is.
RDE_INTERNAL b8 fude_zoom_pages_chosen(const fude_app* _app, u32 _i) {
    RDE_UNUSED(_app);
    const fude_zoom_page* _page = fude_zoom_page_the;
    if(_page == NULL) {
        return false;
    }
    const fude_zoom_pages* _p = &_page->pages;
    if(_i == FUDE_ZOOM_PAGES_ADD) {
        return false;
    }
    if(_i >= FUDE_ZOOM_PAGES_SIZES) {
        return !_p->on;
    }
    fude_zoom_pages _k;
    fude_zoom_pages_init(&_k);
    fude_zoom_pages_size(&_k, (u8)_i);
    return _p->on && rde_math_abs_f64(_p->w - _k.w) < 1e-9 && rde_math_abs_f64(_p->h - _k.h) < 1e-9;
}
// The Pages tool is lit while the canvas is a notebook.
RDE_INTERNAL b8 fude_zoom_pages_selected(const fude_app* _app) {
    RDE_UNUSED(_app);
    return fude_zoom_page_the != NULL && fude_zoom_page_paged(fude_zoom_page_the);
}
// The Pages tool's choices: the sizes (FUDE_ZOOM_PAGES_SIZES of them, in pages.h's order), Add a page, Endless.
RDE_INTERNAL const fude_extension_choice FUDE_ZOOM_PAGES_CHOICES[FUDE_ZOOM_PAGES_SIZES + 2u] = {
    { FUDE_TEXT_ZOOM_PAGES_A4,      FUDE_ICON_PAGE,          fude_zoom_pages_choose },
    { FUDE_TEXT_ZOOM_PAGES_A5,      FUDE_ICON_PAGE,          fude_zoom_pages_choose },
    { FUDE_TEXT_ZOOM_PAGES_LETTER,  FUDE_ICON_PAGE,          fude_zoom_pages_choose },
    { FUDE_TEXT_ZOOM_PAGES_SQUARE,  FUDE_ICON_PAPER_NONE,    fude_zoom_pages_choose },
    { FUDE_TEXT_ZOOM_PAGES_ADD,     FUDE_ICON_FILE_ADD,      fude_zoom_pages_choose },
    { FUDE_TEXT_ZOOM_PAGES_ENDLESS, FUDE_ICON_PAPER_SQUARES, fude_zoom_pages_choose },
};

// Every app tool the bar can show after Topic, in product.h's FUDE_ZOOM_TOOL_ order (checked below). A topic picks
// some of them, and Insert's, Instruments' and Export's choices are swapped for the topic's own when the bar is filled.
RDE_INTERNAL const fude_extension_tool FUDE_ZOOM_BASE_TOOLS[] = {
    {
        .text = FUDE_TEXT_ZOOM_SHAPES, .icon = FUDE_ICON_SHAPES,
        .choices = FUDE_ZOOM_SHAPES_CHOICES, .choice_count = FUDE_ZOOM_SHAPES_CHOICES_N,
        .chosen = fude_zoom_shapes_chosen, .selected = fude_zoom_shapes_selected,
    },
    {
        .text = FUDE_TEXT_ZOOM_INSERT, .icon = FUDE_ICON_PLUS,
        .choices = FUDE_ZOOM_PICTURE_CHOICES, .choice_count = FUDE_ZOOM_INSERT_COUNT,
        .chosen = fude_zoom_picture_chosen, .selected = fude_zoom_picture_selected,
    },
    {
        .text = FUDE_TEXT_ZOOM_SMOOTHING, .icon = FUDE_ICON_SMOOTHING,
        .choices = FUDE_ZOOM_SMOOTHING_CHOICES, .choice_count = FUDE_ZOOM_SMOOTHING_CHOICES_N,
        .chosen = fude_zoom_smoothing_chosen, .selected = fude_zoom_smoothing_selected,
    },
    {
        .text = FUDE_TEXT_ZOOM_FILL, .icon = FUDE_ICON_FILL,
        .press = fude_zoom_fill_press, .selected = fude_zoom_fill_selected,
    },
    {
        .text = FUDE_TEXT_ZOOM_ARRANGE, .icon = FUDE_ICON_ARRANGE,
        .choices = FUDE_ZOOM_ARRANGE_CHOICES, .choice_count = FUDE_ZOOM_ARRANGE_COUNT,
    },
    {
        .text = FUDE_TEXT_ZOOM_INSTRUMENTS, .icon = FUDE_ICON_ANGLE,
        .choices = FUDE_ZOOM_INSTRUMENT_CHOICES, .choice_count = FUDE_ZOOM_TOOLKIT_COUNT,
        .chosen = fude_zoom_instruments_chosen, .selected = fude_zoom_instruments_selected,
    },
    {
        .text = FUDE_TEXT_ZOOM_LAYERS, .icon = FUDE_ICON_LAYERS,
        .press = fude_zoom_layers_press, .selected = fude_zoom_layers_selected,
    },
    {
        .text = FUDE_TEXT_ZOOM_EXPORT, .icon = FUDE_ICON_EXPORT,
        .choices = FUDE_ZOOM_EXPORT_CHOICES, .choice_count = FUDE_ZOOM_OUT_COUNT,
    },
    {
        .text = FUDE_TEXT_ZOOM_CIRCUIT, .icon = FUDE_ICON_CONNECTOR,
        .choices = FUDE_ZOOM_CIRCUIT_CHOICES, .choice_count = FUDE_ZOOM_CIRCUIT_CHOICES_N,
        .chosen = fude_zoom_circuit_chosen, .selected = fude_zoom_circuit_selected,
    },
    {
        .text = FUDE_TEXT_ZOOM_MOTION, .icon = FUDE_ICON_PLAY,
        .choices = FUDE_ZOOM_MOTION_CHOICES, .choice_count = sizeof(FUDE_ZOOM_MOTION_CHOICES) / sizeof(FUDE_ZOOM_MOTION_CHOICES[0]),
        .chosen = fude_zoom_motion_chosen, .selected = fude_zoom_motion_selected,
    },
    {
        .text = FUDE_TEXT_ZOOM_PLAN, .icon = FUDE_ICON_AREA,
        .choices = FUDE_ZOOM_PLAN_CHOICES, .choice_count = FUDE_ZOOM_PLAN_CHOICES_N,
        .chosen = fude_zoom_plan_chosen, .selected = fude_zoom_plan_selected,
    },
    {
        .text = FUDE_TEXT_ZOOM_PAGES, .icon = FUDE_ICON_BOOK,
        .choices = FUDE_ZOOM_PAGES_CHOICES, .choice_count = FUDE_ZOOM_PAGES_SIZES + 2u,
        .chosen = fude_zoom_pages_chosen, .selected = fude_zoom_pages_selected,
        .by_paper = true,   // Shown by Paper: the page's size beside its paper.
    },
};

// --- topics: the bar's tools for what a canvas is for ---------------------------------------------------
//
// The bar shows the tools every canvas needs (the core's: undo, the pen, the lasso, colour, paper) and, after its
// Topic, the app's tools for what this canvas is for: General, Technical plans, Woodworking, Diagrams, PDF study,
// Electronics, Mechanisms, Floor plans, Wiring, Maths and Sewing (product.h's FUDE_ZOOM_TOPIC_). Each has its own
// tools from the twelve (FUDE_ZOOM_BASE_TOOLS), and its own choices in Insert, Instruments and Export (their indices
// in the tables: a topic's choice handed on as the table's own). Each canvas keeps its topic (its .tools file); a new
// one opens in the topic that was in use. The bar is built again the frame after it changes
// (fude_zoom_page_topic_follow).

// The base tools' indices in FUDE_ZOOM_BASE_TOOLS.
enum { FUDE_ZOOM_BASE_SHAPES = 0, FUDE_ZOOM_BASE_INSERT, FUDE_ZOOM_BASE_SMOOTHING, FUDE_ZOOM_BASE_FILL, FUDE_ZOOM_BASE_ARRANGE,
       FUDE_ZOOM_BASE_INSTRUMENTS, FUDE_ZOOM_BASE_LAYERS, FUDE_ZOOM_BASE_EXPORT, FUDE_ZOOM_BASE_CIRCUIT, FUDE_ZOOM_BASE_MOTION, FUDE_ZOOM_BASE_PLAN,
       FUDE_ZOOM_BASE_PAGES };

// One topic: its name and icon for the Topic menu, its tools in the bar's order, and its choices for Insert,
// Instruments and Export, each list with its count.
typedef struct {
    u32       text;                                    // Its name (a text id; General's and Maths' are the app's own).
    const c8* icon;
    u8        tools[FUDE_EXTENSION_TOOLS - 1u];        // The base tools after Topic, in order (FUDE_ZOOM_BASE_).
    u8        tool_count;
    u8        insert[FUDE_EXTENSION_CHOICES];          // Insert's choices (FUDE_ZOOM_PICTURE_CHOICES' indices).
    u8        insert_count;
    u8        instruments[FUDE_EXTENSION_CHOICES];     // Instruments' choices (FUDE_ZOOM_INSTRUMENT_CHOICES' indices).
    u8        instruments_count;
    u8        exports[FUDE_EXTENSION_CHOICES];         // Export's choices (FUDE_ZOOM_EXPORT_CHOICES' indices).
    u8        exports_count;
} fude_zoom_topic;

// Each topic's tools and lists. The lists' indices are those of the tables they choose from:
// Insert: 0 Photos, 1 Files, 2 Text, 3 Sticky, 4 Mermaid, 5 Board, 6 Holes, 7 PDF, 8 Fingers, 9 Dovetail, 10 Diagram,
// 11 Kanban, 12 Area, 13 Pieces, 14 Sheet, 15 Circuit parts, 16 Mechanism parts, 17 Plan parts, 18 Maths, 19 Sewing
// marks, 20 Examples, 21 Formula.
// Instruments: 0 Ruler, 1 45°, 2 30°, 3 Protractor, 4 Compass, 5 Circle, 6 Ellipse, 7 French curve, 8 Corner,
// 9 Stencil, 10 Tape, 11 Dimension, 12 Saw, 13 Guide, 14 Trim.
// Export: 0 PNG, 1 SVG, 2 PDF, 3 DXF, 4 Print A4, 5 Print Letter, 6 Cut list, 7 Video, 8 Areas, 9 Sheets, 10 STL,
// 11 Parts list.
// A list with a count of 0 is empty, whatever its entries say.
RDE_INTERNAL const fude_zoom_topic FUDE_ZOOM_TOPICS[] = {
    { FUDE_TEXT_ZOOM_TOPIC_GENERAL, FUDE_ICON_PEN_NIB,
      { 0, 1, 2, 3, 4, 5, 6, 7, FUDE_ZOOM_BASE_PAGES }, 9,
      { 0, 1, 2, 3, 21, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 20 }, 17,
      { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14 }, 15,
      { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 }, 12 },
    { FUDE_TEXT_ZOOM_TOPIC_TECHNICAL, FUDE_ICON_RULER,
      { FUDE_ZOOM_BASE_SHAPES, FUDE_ZOOM_BASE_INSERT, FUDE_ZOOM_BASE_INSTRUMENTS, FUDE_ZOOM_BASE_FILL, FUDE_ZOOM_BASE_LAYERS, FUDE_ZOOM_BASE_EXPORT }, 6,
      { 14, 2, 3, 13, 12, 7, 0, 1 }, 8,
      { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 14 }, 14,
      { 2, 9, 4, 5, 3, 10, 1, 0 }, 8 },
    { FUDE_TEXT_ZOOM_TOPIC_WOOD, FUDE_ICON_COLUMNS,
      { FUDE_ZOOM_BASE_SHAPES, FUDE_ZOOM_BASE_INSERT, FUDE_ZOOM_BASE_INSTRUMENTS, FUDE_ZOOM_BASE_FILL, FUDE_ZOOM_BASE_LAYERS, FUDE_ZOOM_BASE_EXPORT }, 6,
      { 5, 6, 8, 9, 14, 2, 13, 0, 1 }, 9,
      { 0, 1, 2, 3, 4, 10, 11, 12, 13, 14, 9 }, 11,
      { 6, 3, 4, 5, 10, 2, 0 }, 7 },
    { FUDE_TEXT_ZOOM_TOPIC_DIAGRAMS, FUDE_ICON_GRAPH,
      { FUDE_ZOOM_BASE_SHAPES, FUDE_ZOOM_BASE_INSERT, FUDE_ZOOM_BASE_ARRANGE, FUDE_ZOOM_BASE_FILL, FUDE_ZOOM_BASE_SMOOTHING, FUDE_ZOOM_BASE_LAYERS, FUDE_ZOOM_BASE_EXPORT }, 7,
      { 10, 4, 11, 12, 2, 3, 0, 1, 13 }, 9,
      { 0 }, 0,
      { 0, 1, 2, 8, 7 }, 5 },
    { FUDE_TEXT_ZOOM_TOPIC_PDF, FUDE_ICON_BOOK,
      { FUDE_ZOOM_BASE_INSERT, FUDE_ZOOM_BASE_SHAPES, FUDE_ZOOM_BASE_SMOOTHING, FUDE_ZOOM_BASE_LAYERS, FUDE_ZOOM_BASE_EXPORT }, 5,
      { 7, 2, 3, 21, 0, 1, 12 }, 7,
      { 0 }, 0,
      { 2, 0, 8 }, 3 },
    { FUDE_TEXT_ZOOM_TOPIC_ELECTRONICS, FUDE_ICON_SIGNAL_HIGH,
      { FUDE_ZOOM_BASE_CIRCUIT, FUDE_ZOOM_BASE_INSERT, FUDE_ZOOM_BASE_SHAPES, FUDE_ZOOM_BASE_ARRANGE, FUDE_ZOOM_BASE_LAYERS, FUDE_ZOOM_BASE_EXPORT }, 6,
      { 15, 2, 3, 14, 12, 0, 1, 20 }, 8,
      { 0 }, 0,   // (its bar has no Instruments: none, and those lying out are put away as it is chosen)
      { 2, 11, 1, 0, 4, 5, 7 }, 7 },
    { FUDE_TEXT_ZOOM_TOPIC_MECHANISMS, FUDE_ICON_ROTATE,
      { FUDE_ZOOM_BASE_MOTION, FUDE_ZOOM_BASE_INSERT, FUDE_ZOOM_BASE_SHAPES, FUDE_ZOOM_BASE_INSTRUMENTS, FUDE_ZOOM_BASE_LAYERS, FUDE_ZOOM_BASE_EXPORT }, 6,
      { 16, 14, 2, 3, 0, 1, 20 }, 7,
      { 0, 3, 4, 5, 11, 13 }, 6,
      { 2, 11, 1, 0, 9, 10, 7 }, 7 },
    { FUDE_TEXT_ZOOM_TOPIC_FLOORPLAN, FUDE_ICON_AREA,
      { FUDE_ZOOM_BASE_PLAN, FUDE_ZOOM_BASE_INSERT, FUDE_ZOOM_BASE_SHAPES, FUDE_ZOOM_BASE_INSTRUMENTS, FUDE_ZOOM_BASE_FILL, FUDE_ZOOM_BASE_LAYERS, FUDE_ZOOM_BASE_EXPORT }, 7,
      { 17, 14, 2, 3, 0, 1, 7 }, 7,
      { 11, 10, 0, 1, 3, 13 }, 6,
      { 2, 9, 4, 5, 11, 1, 3, 0 }, 8 },
    { FUDE_TEXT_ZOOM_TOPIC_WIRING, FUDE_ICON_SIGNAL_HIGH,
      { FUDE_ZOOM_BASE_PLAN, FUDE_ZOOM_BASE_INSERT, FUDE_ZOOM_BASE_SHAPES, FUDE_ZOOM_BASE_INSTRUMENTS, FUDE_ZOOM_BASE_LAYERS, FUDE_ZOOM_BASE_EXPORT }, 6,
      { 17, 2, 3, 14, 0, 1, 7 }, 7,
      { 11, 10, 0, 13 }, 4,
      { 2, 11, 9, 4, 5, 0 }, 6 },
    { FUDE_TEXT_ZOOM_TOPIC_MATHS, FUDE_ICON_CHART_LINE,
      { FUDE_ZOOM_BASE_INSERT, FUDE_ZOOM_BASE_INSTRUMENTS, FUDE_ZOOM_BASE_SHAPES, FUDE_ZOOM_BASE_FILL, FUDE_ZOOM_BASE_LAYERS, FUDE_ZOOM_BASE_EXPORT,
        FUDE_ZOOM_BASE_PAGES }, 7,
      { 21, 18, 2, 3, 14, 0, 1, 7 }, 8,
      { 0, 1, 2, 3, 4, 5, 6, 10, 11, 13 }, 10,
      { 2, 0, 1, 4, 5 }, 5 },
    { FUDE_TEXT_ZOOM_TOPIC_SEWING, FUDE_ICON_CUT,
      { FUDE_ZOOM_BASE_SHAPES, FUDE_ZOOM_BASE_INSERT, FUDE_ZOOM_BASE_INSTRUMENTS, FUDE_ZOOM_BASE_LAYERS, FUDE_ZOOM_BASE_EXPORT }, 5,
      { 19, 14, 2, 0, 1, 7 }, 6,
      { 0, 7, 10, 11, 4, 5, 3, 13 }, 8,
      { 4, 5, 2, 1, 0 }, 5 },
};

// The extension's tools for the bar (page.h): Topic, then the topic's own, filled by fude_zoom_topic_fill.
fude_extension_tool FUDE_ZOOM_TOOLS[FUDE_EXTENSION_TOOLS];
// The Topic menu's choices (the app's topics), and the topic's Insert, Instruments and Export choices, each a copy of
// the table's entry with its press handed on through the topic's list.
RDE_INTERNAL fude_extension_choice FUDE_ZOOM_TOPIC_CHOICES[FUDE_ZOOM_TOPIC_COUNT];
RDE_INTERNAL fude_extension_choice fude_zoom_topic_insert_choices[FUDE_EXTENSION_CHOICES];
RDE_INTERNAL fude_extension_choice fude_zoom_topic_instrument_choices[FUDE_EXTENSION_CHOICES];
RDE_INTERNAL fude_extension_choice fude_zoom_topic_export_choices[FUDE_EXTENSION_CHOICES];
u8 fude_zoom_topic_now = FUDE_ZOOM_TOPIC_GENERAL;   // The topic the bar was filled for.
RDE_INTERNAL fude_zoom_topic fude_zoom_topic_bar;                 // That topic as the app has it (product.h).
RDE_INTERNAL u8 fude_zoom_topic_menu[FUDE_ZOOM_TOPIC_COUNT];      // The Topic menu's entries: the app's topics.
RDE_INTERNAL u32 fude_zoom_topic_menu_count = 0;

// A compile-time check that the topics' and the base tools' tables have as many entries as product.h's enums number
// them (the size is -1, which does not compile, when one does not): both are sized by their entries, so one left out
// (or one too many) is found here, rather than read as a topic or a tool of zeros. The choices' tables are checked
// where each is defined (page_parts.h).
typedef char fude_zoom_page_tables_as_product_h[(sizeof(FUDE_ZOOM_TOPICS) / sizeof(FUDE_ZOOM_TOPICS[0]) == FUDE_ZOOM_TOPIC_COUNT &&
                                                 sizeof(FUDE_ZOOM_BASE_TOOLS) / sizeof(FUDE_ZOOM_BASE_TOOLS[0]) == FUDE_ZOOM_TOOL_COUNT) ? 1 : -1];

// Returns the topic the bar was filled for, as the app has it.
RDE_INTERNAL const fude_zoom_topic* fude_zoom_topic_of(void) {
    return &fude_zoom_topic_bar;
}

// A topic's name in this app: General's and Maths' are the app's own (product.h: Notes in the notes product, Sketch and
// Graphs in the workshop one); the others are the same in every app.
RDE_INTERNAL u32 fude_zoom_topic_name(u8 _topic) {
    if(_topic == FUDE_ZOOM_TOPIC_GENERAL) {
        return fude_zoom_product_get()->general_name;
    }
    if(_topic == FUDE_ZOOM_TOPIC_MATHS) {
        return fude_zoom_product_get()->maths_name;
    }
    return FUDE_ZOOM_TOPICS[_topic < FUDE_ZOOM_TOPIC_COUNT ? _topic : 0u].text;
}

// A topic's choices: the table's own, its index handed on through the topic's list. Choose returns what the table's
// own returns; chosen lights the entry when the table's would. Insert's choice handed on to the table's.
RDE_INTERNAL b8 fude_zoom_topic_insert_choose(fude_app* _app, u32 _i) {
    const fude_zoom_topic* _t = fude_zoom_topic_of();
    return _i < _t->insert_count ? fude_zoom_picture_choose(_app, _t->insert[_i]) : false;
}
// Insert's entry lit when the table's is.
RDE_INTERNAL b8 fude_zoom_topic_insert_chosen(const fude_app* _app, u32 _i) {
    const fude_zoom_topic* _t = fude_zoom_topic_of();
    return _i < _t->insert_count && fude_zoom_picture_chosen(_app, _t->insert[_i]);
}
// Instruments' choice handed on to the table's.
RDE_INTERNAL b8 fude_zoom_topic_instrument_choose(fude_app* _app, u32 _i) {
    const fude_zoom_topic* _t = fude_zoom_topic_of();
    return _i < _t->instruments_count ? fude_zoom_instruments_choose(_app, _t->instruments[_i]) : false;
}
// Instruments' entry lit when the table's is.
RDE_INTERNAL b8 fude_zoom_topic_instrument_chosen(const fude_app* _app, u32 _i) {
    const fude_zoom_topic* _t = fude_zoom_topic_of();
    return _i < _t->instruments_count && fude_zoom_instruments_chosen(_app, _t->instruments[_i]);
}
// Export's choice handed on to the table's.
RDE_INTERNAL b8 fude_zoom_topic_export_choose(fude_app* _app, u32 _i) {
    const fude_zoom_topic* _t = fude_zoom_topic_of();
    return _i < _t->exports_count ? fude_zoom_export_choose(_app, _t->exports[_i]) : false;
}

// Topic: one chosen from the menu (_at, an index into the app's topics). The bar is filled for it the frame after, where
// every change of topic is picked up (fude_zoom_page_topic_follow). Everything the last topic's tools left in
// hand is put down: what the pen was set to do (wires, walls, rooms, a shape, the curve, fill, trim, a measure, text)
// and the Saw, so the pen draws again; the parts' and My pieces' panels; and the instruments lying out that the new
// topic does not have (Undo brings them back). The canvas keeps its topic. Returns false: the panel is not kept open.
b8 fude_zoom_topic_choose(fude_app* _app, u32 _at) {
    RDE_UNUSED(_app);
    fude_zoom_page* _page = fude_zoom_page_the;
    const u32 _i = _at < fude_zoom_topic_menu_count ? fude_zoom_topic_menu[_at] : FUDE_ZOOM_TOPIC_COUNT;   // The app's.
    if(_page != NULL && _i < FUDE_ZOOM_TOPIC_COUNT && _i != _page->topic) {
        fude_zoom_page_pen_modes_off(_page);
        _page->saw            = false;
        _page->library_open   = false;
        _page->pieces_open    = false;
        _page->pieces_stencil = false;
        const fude_zoom_topic* _to = &FUDE_ZOOM_TOPICS[_i];
        const u32 _allowed = fude_zoom_product_get()->instruments;
        for(u32 _k = 0; _k < FUDE_ZOOM_INSTRUMENT_MOST; _k++) {
            const fude_zoom_instrument* _t = &_page->instruments.tools[_k];
            b8 _has = false;
            for(u32 _j = 0; _j < _to->instruments_count && !_has; _j++) {
                _has = _to->instruments[_j] == _t->kind && (_allowed >> _to->instruments[_j] & 1u);
            }
            if(_t->shown && !_has) {
                fude_zoom_instrument_remove(&_page->instruments, _k);
            }
        }
        if(_page->anchor_tool >= 0 && !_page->instruments.tools[_page->anchor_tool].shown) {
            _page->anchor_tool = -1;
        }
        _page->topic = (u8)_i;
        fude_zoom_page_tools_save(_page);   // The canvas keeps it.
        c8 _say[200];
        FUDE_TEXTF(_say, FUDE_TEXT_ZOOM_TOPIC_NOW, FUDE_TS(fude_text(fude_zoom_topic_name((u8)_i))));
        fude_notice_show(_say);
    }
    return false;
}
// The Topic menu's entry for the canvas's topic is lit.
RDE_INTERNAL b8 fude_zoom_topic_chosen(const fude_app* _app, u32 _at) {
    RDE_UNUSED(_app);
    return fude_zoom_page_the != NULL && _at < fude_zoom_topic_menu_count && fude_zoom_page_the->topic == fude_zoom_topic_menu[_at];
}
// The availability of a bar slot no topic fills: never, so the bar leaves it out.
RDE_INTERNAL b8 fude_zoom_topic_absent(void) {
    return false;
}

// Fills the bar's tools for topic _topic (one of the app's, else the app's first): Topic first, then the topic's own,
// and the rest left out. Of the topic's tools and choices only the app's are kept (product.h), and the Topic menu
// lists the app's topics.
void fude_zoom_topic_fill(u8 _topic) {
    const fude_zoom_product* _app = fude_zoom_product_get();
    fude_zoom_topic_now = fude_zoom_product_topic(_topic < FUDE_ZOOM_TOPIC_COUNT ? _topic : FUDE_ZOOM_TOPIC_GENERAL);
    const fude_zoom_topic* _whole = &FUDE_ZOOM_TOPICS[fude_zoom_topic_now];
    fude_zoom_topic_bar = *_whole;
    fude_zoom_topic_bar.text              = fude_zoom_topic_name(fude_zoom_topic_now);
    fude_zoom_topic_bar.tool_count        = (u8)fude_zoom_product_keep(_whole->tools, _whole->tool_count, _app->tools, fude_zoom_topic_bar.tools);
    fude_zoom_topic_bar.insert_count      = (u8)fude_zoom_product_keep(_whole->insert, _whole->insert_count, _app->inserts, fude_zoom_topic_bar.insert);
    fude_zoom_topic_bar.instruments_count = (u8)fude_zoom_product_keep(_whole->instruments, _whole->instruments_count, _app->instruments,
                                                                       fude_zoom_topic_bar.instruments);
    fude_zoom_topic_bar.exports_count     = (u8)fude_zoom_product_keep(_whole->exports, _whole->exports_count, _app->exports, fude_zoom_topic_bar.exports);
    const fude_zoom_topic* _t = fude_zoom_topic_of();
    fude_zoom_topic_menu_count = 0;
    for(u32 _i = 0; _i < FUDE_ZOOM_TOPIC_COUNT; _i++) {
        if(fude_zoom_product_has_topic((u8)_i)) {
            FUDE_ZOOM_TOPIC_CHOICES[fude_zoom_topic_menu_count] = (fude_extension_choice){ fude_zoom_topic_name((u8)_i), FUDE_ZOOM_TOPICS[_i].icon, fude_zoom_topic_choose };
            fude_zoom_topic_menu[fude_zoom_topic_menu_count++] = (u8)_i;
        }
    }
    for(u32 _i = 0; _i < _t->insert_count; _i++) {
        fude_zoom_topic_insert_choices[_i] = FUDE_ZOOM_PICTURE_CHOICES[_t->insert[_i]];
        fude_zoom_topic_insert_choices[_i].press = fude_zoom_topic_insert_choose;
    }
    for(u32 _i = 0; _i < _t->instruments_count; _i++) {
        fude_zoom_topic_instrument_choices[_i] = FUDE_ZOOM_INSTRUMENT_CHOICES[_t->instruments[_i]];
        fude_zoom_topic_instrument_choices[_i].press = fude_zoom_topic_instrument_choose;
    }
    for(u32 _i = 0; _i < _t->exports_count; _i++) {
        fude_zoom_topic_export_choices[_i] = FUDE_ZOOM_EXPORT_CHOICES[_t->exports[_i]];
        fude_zoom_topic_export_choices[_i].press = fude_zoom_topic_export_choose;
    }
    FUDE_ZOOM_TOOLS[0] = (fude_extension_tool){ .text = FUDE_TEXT_ZOOM_TOPIC, .icon = _t->icon, .choices = FUDE_ZOOM_TOPIC_CHOICES,
                                                .choice_count = fude_zoom_topic_menu_count, .chosen = fude_zoom_topic_chosen };
    for(u32 _i = 1; _i < FUDE_EXTENSION_TOOLS; _i++) {
        // A slot no topic fills: a button of Topic's look the bar leaves out.
        FUDE_ZOOM_TOOLS[_i] = (fude_extension_tool){ .text = FUDE_TEXT_ZOOM_TOPIC, .icon = _t->icon, .available = fude_zoom_topic_absent };
    }
    for(u32 _i = 0; _i < _t->tool_count && _i + 1u < FUDE_EXTENSION_TOOLS; _i++) {
        fude_extension_tool _tool = FUDE_ZOOM_BASE_TOOLS[_t->tools[_i]];
        if(_t->tools[_i] == FUDE_ZOOM_BASE_INSERT) {
            _tool.choices = fude_zoom_topic_insert_choices;
            _tool.choice_count = _t->insert_count;
            _tool.chosen = fude_zoom_topic_insert_chosen;
        } else if(_t->tools[_i] == FUDE_ZOOM_BASE_INSTRUMENTS) {
            _tool.choices = fude_zoom_topic_instrument_choices;
            _tool.choice_count = _t->instruments_count;
            _tool.chosen = fude_zoom_topic_instrument_chosen;
        } else if(_t->tools[_i] == FUDE_ZOOM_BASE_EXPORT) {
            _tool.choices = fude_zoom_topic_export_choices;
            _tool.choice_count = _t->exports_count;
        }
        FUDE_ZOOM_TOOLS[1u + _i] = _tool;
    }
}

// Fills the bar again when the canvas's topic is not the one it was filled for, and rebuilds the UI at once. Called
// every frame.
void fude_zoom_page_topic_follow(fude_zoom_page* _page, fude_ui* _ui) {
    _page->topic = fude_zoom_product_topic(_page->topic);   // A canvas from another app: one of this one's.
    if(_page->topic != fude_zoom_topic_now) {
        fude_zoom_topic_fill(_page->topic);
        fude_ui_rebuild_now(_ui);
    }
}
