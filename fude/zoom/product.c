// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/product.h"
#include "drawing/base/text.h"

// ===========================================================================
// See product.h.
//
// The three products are one constant table, FZPD_PRODUCTS, indexed by FUDE_ZOOM_PRODUCT_, and the product set is one
// index into it (fzpd_which); nothing is allocated. The file's own names start with FZPD_ and fzpd_.
// ===========================================================================

#define FZPD_BIT(_k) (1u << (u32)(_k))          // The bit for enum value _k in one of the product's masks.
#define FZPD_ALL(_n) ((1u << (u32)(_n)) - 1u)   // Every bit of an enum with _n values (its _COUNT).

// Each product's description, in FUDE_ZOOM_PRODUCT_ order, with fude_zoom_product's fields in order: topics, first,
// tools, inserts, instruments, exports, hobby, workshop, general_name, maths_name, audio.
RDE_INTERNAL const fude_zoom_product FZPD_PRODUCTS[] = {
    // Sketching: everything, as it has been.
    { FZPD_ALL(FUDE_ZOOM_TOPIC_COUNT), FUDE_ZOOM_TOPIC_GENERAL, FZPD_ALL(FUDE_ZOOM_TOOL_COUNT), FZPD_ALL(FUDE_ZOOM_INSERT_COUNT),
      FZPD_ALL(FUDE_ZOOM_TOOLKIT_COUNT), FZPD_ALL(FUDE_ZOOM_OUT_COUNT), true, true,
      FUDE_TEXT_ZOOM_TOPIC_GENERAL, FUDE_TEXT_ZOOM_TOPIC_MATHS, true },
    // Notes: writing and drawing, PDFs, maths, diagrams — none of a project's parts, joints, saws or cut lists.
    { FZPD_BIT(FUDE_ZOOM_TOPIC_GENERAL) | FZPD_BIT(FUDE_ZOOM_TOPIC_PDF) | FZPD_BIT(FUDE_ZOOM_TOPIC_MATHS) | FZPD_BIT(FUDE_ZOOM_TOPIC_DIAGRAMS),
      FUDE_ZOOM_TOPIC_GENERAL,
      FZPD_BIT(FUDE_ZOOM_TOOL_SHAPES) | FZPD_BIT(FUDE_ZOOM_TOOL_INSERT) | FZPD_BIT(FUDE_ZOOM_TOOL_SMOOTHING) | FZPD_BIT(FUDE_ZOOM_TOOL_FILL) |
          FZPD_BIT(FUDE_ZOOM_TOOL_ARRANGE) | FZPD_BIT(FUDE_ZOOM_TOOL_INSTRUMENTS) | FZPD_BIT(FUDE_ZOOM_TOOL_LAYERS) | FZPD_BIT(FUDE_ZOOM_TOOL_EXPORT) |
          FZPD_BIT(FUDE_ZOOM_TOOL_PAGES),
      FZPD_BIT(FUDE_ZOOM_INSERT_PHOTOS) | FZPD_BIT(FUDE_ZOOM_INSERT_FILES) | FZPD_BIT(FUDE_ZOOM_INSERT_TEXT) | FZPD_BIT(FUDE_ZOOM_INSERT_STICKY) |
          FZPD_BIT(FUDE_ZOOM_INSERT_MERMAID) | FZPD_BIT(FUDE_ZOOM_INSERT_PDF) | FZPD_BIT(FUDE_ZOOM_INSERT_DIAGRAM) | FZPD_BIT(FUDE_ZOOM_INSERT_KANBAN) |
          FZPD_BIT(FUDE_ZOOM_INSERT_AREA) | FZPD_BIT(FUDE_ZOOM_INSERT_PIECES) | FZPD_BIT(FUDE_ZOOM_INSERT_MATHS),
          // (formulas — Insert's, the lasso's Formula, Handwriting and Graph on them — hidden for now: Borja,
          // 2026-10-11, until the handwriting reader is trained on real handwriting, docs/product_split.md §10;
          // Sketching keeps them)
      FZPD_BIT(FUDE_ZOOM_TOOLKIT_RULER) | FZPD_BIT(FUDE_ZOOM_TOOLKIT_SQUARE_45) | FZPD_BIT(FUDE_ZOOM_TOOLKIT_SQUARE_30) |
          FZPD_BIT(FUDE_ZOOM_TOOLKIT_PROTRACTOR) | FZPD_BIT(FUDE_ZOOM_TOOLKIT_COMPASS) | FZPD_BIT(FUDE_ZOOM_TOOLKIT_CIRCLES) |
          FZPD_BIT(FUDE_ZOOM_TOOLKIT_ELLIPSES) | FZPD_BIT(FUDE_ZOOM_TOOLKIT_FRENCH_CURVE) | FZPD_BIT(FUDE_ZOOM_TOOLKIT_STENCIL) |
          FZPD_BIT(FUDE_ZOOM_TOOLKIT_TAPE) | FZPD_BIT(FUDE_ZOOM_TOOLKIT_GUIDE),
      FZPD_BIT(FUDE_ZOOM_OUT_PNG) | FZPD_BIT(FUDE_ZOOM_OUT_SVG) | FZPD_BIT(FUDE_ZOOM_OUT_PDF) | FZPD_BIT(FUDE_ZOOM_OUT_A4) |
          FZPD_BIT(FUDE_ZOOM_OUT_LETTER) | FZPD_BIT(FUDE_ZOOM_OUT_VIDEO) | FZPD_BIT(FUDE_ZOOM_OUT_AREAS),
      false, false,
      FUDE_TEXT_ZOOM_TOPIC_NOTES, FUDE_TEXT_ZOOM_TOPIC_MATHS, true },
    // Workshop: projects that work — circuits, mechanisms, plans, wood, sewing, their PDFs, diagrams and Kanban to plan
    // them (Borja: both apps'), and graphs (Borja, 2026-10-11: its Maths topic, called Graphs there);
    // no formulas (Notes').
    { FZPD_BIT(FUDE_ZOOM_TOPIC_GENERAL) | FZPD_BIT(FUDE_ZOOM_TOPIC_TECHNICAL) | FZPD_BIT(FUDE_ZOOM_TOPIC_WOOD) | FZPD_BIT(FUDE_ZOOM_TOPIC_PDF) |
          FZPD_BIT(FUDE_ZOOM_TOPIC_DIAGRAMS) | FZPD_BIT(FUDE_ZOOM_TOPIC_MATHS) |
          FZPD_BIT(FUDE_ZOOM_TOPIC_ELECTRONICS) | FZPD_BIT(FUDE_ZOOM_TOPIC_MECHANISMS) | FZPD_BIT(FUDE_ZOOM_TOPIC_FLOORPLAN) |
          FZPD_BIT(FUDE_ZOOM_TOPIC_WIRING) | FZPD_BIT(FUDE_ZOOM_TOPIC_SEWING),
      FUDE_ZOOM_TOPIC_GENERAL,
      FZPD_ALL(FUDE_ZOOM_TOOL_COUNT) & ~FZPD_BIT(FUDE_ZOOM_TOOL_PAGES),   // (a notebook's pages: Notes')
      FZPD_ALL(FUDE_ZOOM_INSERT_COUNT) & ~FZPD_BIT(FUDE_ZOOM_INSERT_FORMULA),
      FZPD_ALL(FUDE_ZOOM_TOOLKIT_COUNT),
      FZPD_ALL(FUDE_ZOOM_OUT_COUNT),
      true, true,
      FUDE_TEXT_ZOOM_TOPIC_SKETCH, FUDE_TEXT_ZOOM_INSERT_MATHS, false },   // (no recordings: a notes app's)
};
// A compile-time check that every product has its row (the size is -1, which does not compile, when one has not).
typedef char fzpd_products_counted[(sizeof(FZPD_PRODUCTS) / sizeof(FZPD_PRODUCTS[0]) == FUDE_ZOOM_PRODUCT_COUNT) ? 1 : -1];

// Which product this is (FUDE_ZOOM_PRODUCT_): Sketching, the one with everything, until the shell sets another.
RDE_INTERNAL u8 fzpd_which = FUDE_ZOOM_PRODUCT_SKETCHING;

// See the header. An unknown value sets Sketching.
void fude_zoom_product_set(u8 _which) {
    fzpd_which = _which < FUDE_ZOOM_PRODUCT_COUNT ? _which : FUDE_ZOOM_PRODUCT_SKETCHING;
}

// See the header.
const fude_zoom_product* fude_zoom_product_get(void) {
    return &FZPD_PRODUCTS[fzpd_which];
}

// See the header.
const fude_zoom_product* fude_zoom_product_of(u8 _which) {
    return &FZPD_PRODUCTS[_which < FUDE_ZOOM_PRODUCT_COUNT ? _which : FUDE_ZOOM_PRODUCT_SKETCHING];
}

// See the header. A value of FUDE_ZOOM_TOPIC_COUNT or more is no app's topic.
b8 fude_zoom_product_has_topic(u8 _topic) {
    return _topic < FUDE_ZOOM_TOPIC_COUNT && (fude_zoom_product_get()->topics & FZPD_BIT(_topic)) != 0u;
}

// See the header.
u8 fude_zoom_product_topic(u8 _topic) {
    return fude_zoom_product_has_topic(_topic) ? _topic : fude_zoom_product_get()->first;
}

// See the header. An entry of 32 or more is dropped, since a u32 mask has no bit for it.
u32 fude_zoom_product_keep(const u8* _list, u32 _count, u32 _allowed, u8* _out) {
    u32 _n = 0;
    for(u32 _i = 0; _i < _count; _i++) {
        if(_list[_i] < 32u && (_allowed & FZPD_BIT(_list[_i])) != 0u) {
            _out[_n++] = _list[_i];
        }
    }
    return _n;
}
