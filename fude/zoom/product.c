// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#include "zoom/product.h"

// ===========================================================================
// See product.h.
// ===========================================================================

#define FZP_BIT(_k) (1u << (u32)(_k))
#define FZP_ALL(_n) ((1u << (u32)(_n)) - 1u)

RDE_INTERNAL const fude_zoom_product FZP_PRODUCTS[FUDE_ZOOM_PRODUCT_COUNT] = {
    // Sketching: everything, as it has been.
    { FZP_ALL(FUDE_ZOOM_TOPIC_COUNT), FUDE_ZOOM_TOPIC_GENERAL, FZP_ALL(FUDE_ZOOM_TOOL_COUNT), FZP_ALL(FUDE_ZOOM_INSERT_COUNT),
      FZP_ALL(FUDE_ZOOM_TOOLKIT_COUNT), FZP_ALL(FUDE_ZOOM_OUT_COUNT), true, true },
    // Notes: writing and drawing, PDFs, maths, diagrams — none of a project's parts, joints, saws or cut lists.
    { FZP_BIT(FUDE_ZOOM_TOPIC_GENERAL) | FZP_BIT(FUDE_ZOOM_TOPIC_PDF) | FZP_BIT(FUDE_ZOOM_TOPIC_MATHS) | FZP_BIT(FUDE_ZOOM_TOPIC_DIAGRAMS),
      FUDE_ZOOM_TOPIC_GENERAL,
      FZP_BIT(FUDE_ZOOM_TOOL_SHAPES) | FZP_BIT(FUDE_ZOOM_TOOL_INSERT) | FZP_BIT(FUDE_ZOOM_TOOL_SMOOTHING) | FZP_BIT(FUDE_ZOOM_TOOL_FILL) |
          FZP_BIT(FUDE_ZOOM_TOOL_ARRANGE) | FZP_BIT(FUDE_ZOOM_TOOL_INSTRUMENTS) | FZP_BIT(FUDE_ZOOM_TOOL_LAYERS) | FZP_BIT(FUDE_ZOOM_TOOL_EXPORT) |
          FZP_BIT(FUDE_ZOOM_TOOL_PAGES),
      FZP_BIT(FUDE_ZOOM_INSERT_PHOTOS) | FZP_BIT(FUDE_ZOOM_INSERT_FILES) | FZP_BIT(FUDE_ZOOM_INSERT_TEXT) | FZP_BIT(FUDE_ZOOM_INSERT_STICKY) |
          FZP_BIT(FUDE_ZOOM_INSERT_MERMAID) | FZP_BIT(FUDE_ZOOM_INSERT_PDF) | FZP_BIT(FUDE_ZOOM_INSERT_DIAGRAM) | FZP_BIT(FUDE_ZOOM_INSERT_KANBAN) |
          FZP_BIT(FUDE_ZOOM_INSERT_AREA) | FZP_BIT(FUDE_ZOOM_INSERT_PIECES) | FZP_BIT(FUDE_ZOOM_INSERT_MATHS),
      FZP_BIT(FUDE_ZOOM_TOOLKIT_RULER) | FZP_BIT(FUDE_ZOOM_TOOLKIT_SQUARE_45) | FZP_BIT(FUDE_ZOOM_TOOLKIT_SQUARE_30) |
          FZP_BIT(FUDE_ZOOM_TOOLKIT_PROTRACTOR) | FZP_BIT(FUDE_ZOOM_TOOLKIT_COMPASS) | FZP_BIT(FUDE_ZOOM_TOOLKIT_CIRCLES) |
          FZP_BIT(FUDE_ZOOM_TOOLKIT_ELLIPSES) | FZP_BIT(FUDE_ZOOM_TOOLKIT_FRENCH_CURVE) | FZP_BIT(FUDE_ZOOM_TOOLKIT_STENCIL) |
          FZP_BIT(FUDE_ZOOM_TOOLKIT_TAPE) | FZP_BIT(FUDE_ZOOM_TOOLKIT_GUIDE),
      FZP_BIT(FUDE_ZOOM_OUT_PNG) | FZP_BIT(FUDE_ZOOM_OUT_SVG) | FZP_BIT(FUDE_ZOOM_OUT_PDF) | FZP_BIT(FUDE_ZOOM_OUT_A4) |
          FZP_BIT(FUDE_ZOOM_OUT_LETTER) | FZP_BIT(FUDE_ZOOM_OUT_VIDEO) | FZP_BIT(FUDE_ZOOM_OUT_AREAS),
      false, false },
    // Workshop: projects that work — circuits, mechanisms, plans, wood, sewing, their PDFs; no diagrams, Kanban or
    // maths' graphs (Notes').
    { FZP_BIT(FUDE_ZOOM_TOPIC_GENERAL) | FZP_BIT(FUDE_ZOOM_TOPIC_TECHNICAL) | FZP_BIT(FUDE_ZOOM_TOPIC_WOOD) | FZP_BIT(FUDE_ZOOM_TOPIC_PDF) |
          FZP_BIT(FUDE_ZOOM_TOPIC_ELECTRONICS) | FZP_BIT(FUDE_ZOOM_TOPIC_MECHANISMS) | FZP_BIT(FUDE_ZOOM_TOPIC_FLOORPLAN) |
          FZP_BIT(FUDE_ZOOM_TOPIC_WIRING) | FZP_BIT(FUDE_ZOOM_TOPIC_SEWING),
      FUDE_ZOOM_TOPIC_GENERAL,
      FZP_ALL(FUDE_ZOOM_TOOL_COUNT) & ~FZP_BIT(FUDE_ZOOM_TOOL_PAGES),   // (a notebook's pages: Notes')
      FZP_ALL(FUDE_ZOOM_INSERT_COUNT) & ~(FZP_BIT(FUDE_ZOOM_INSERT_MERMAID) | FZP_BIT(FUDE_ZOOM_INSERT_DIAGRAM) | FZP_BIT(FUDE_ZOOM_INSERT_KANBAN) |
                                          FZP_BIT(FUDE_ZOOM_INSERT_MATHS)),
      FZP_ALL(FUDE_ZOOM_TOOLKIT_COUNT),
      FZP_ALL(FUDE_ZOOM_OUT_COUNT),
      true, true },
};

RDE_INTERNAL u8 fzp_which = FUDE_ZOOM_PRODUCT_SKETCHING;

void fude_zoom_product_set(u8 _which) {
    fzp_which = _which < FUDE_ZOOM_PRODUCT_COUNT ? _which : FUDE_ZOOM_PRODUCT_SKETCHING;
}

const fude_zoom_product* fude_zoom_product_get(void) {
    return &FZP_PRODUCTS[fzp_which];
}

u8 fude_zoom_product_which(void) {
    return fzp_which;
}

const fude_zoom_product* fude_zoom_product_of(u8 _which) {
    return &FZP_PRODUCTS[_which < FUDE_ZOOM_PRODUCT_COUNT ? _which : FUDE_ZOOM_PRODUCT_SKETCHING];
}

b8 fude_zoom_product_has_topic(u8 _topic) {
    return _topic < FUDE_ZOOM_TOPIC_COUNT && (fude_zoom_product_get()->topics & FZP_BIT(_topic)) != 0u;
}

u8 fude_zoom_product_topic(u8 _topic) {
    return fude_zoom_product_has_topic(_topic) ? _topic : fude_zoom_product_get()->first;
}

u32 fude_zoom_product_keep(const u8* _list, u32 _count, u32 _allowed, u8* _out) {
    u32 _n = 0;
    for(u32 _i = 0; _i < _count; _i++) {
        if(_list[_i] < 32u && (_allowed & FZP_BIT(_list[_i])) != 0u) {
            _out[_n++] = _list[_i];
        }
    }
    return _n;
}
