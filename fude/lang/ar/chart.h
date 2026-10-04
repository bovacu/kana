#ifndef FUDE_CHART
#define FUDE_CHART

#include "rde.h"
#include "study/chars/kanji.h"
#include "study/chars/glyph.h"
#include "drawing/widgets/scroll.h"
#include "study/models/select.h"

// ===========================================================================
// The Arabic chart: the 28 letters in alphabetical order, each with the forms
// it takes in a word (final, medial, initial, then alone: read from the right);
// the hamza on its own and on its seats, ة ى and لا; the vowel marks; the
// digits. Every cell is drawn from its strokes, with its Latin name under it.
//
// It scrolls like Browse; a tap opens the viewer on that form, walking the
// whole chart in table order.
// ===========================================================================

typedef enum {
    FUDE_CHART_LETTERS = 0,
    FUDE_CHART_MORE,
    FUDE_CHART_MARKS,
    FUDE_CHART_DIGITS,
    FUDE_CHART_SECTIONS
} FUDE_CHART_SECTION_;

RDE_STRUCT {
    f32 x;          // screen x of the cell's left edge
    f32 y;          // content y of its top edge (0 = top of the chart, growing down)
    f32 size;
    u32 position;   // in the chart's list
} fude_chart_cell;

RDE_STRUCT {
    const fude_kanji_db*          db;
    fude_glyph                    glyph;
    fude_scroller                 scroller;
    b8                            open;

    rde_arr TYPE(u32)             list;      // record indices, in table order
    rde_arr TYPE(u32)             codepoints;// the same, as code points (for the romanization)
    rde_arr TYPE(fude_chart_cell) cells;     // laid out for the last frame's width
    f32                           _laid_out_width;
    f32                           content_height;
    f32                           section_at[FUDE_CHART_SECTIONS];   // content y of each section's title
    u32                           section_first[FUDE_CHART_SECTIONS + 1u];   // where each section starts in the list
    f32                           view_top;      // screen y of the chart's visible top and bottom
    f32                           view_bottom;

    i32                           tapped;    // see fude_chart_take_tap

    fude_selection*               selection; // the app's (select.h), set by its owner: in Select mode the cells show ticks
} fude_chart;

void fude_chart_init(fude_chart* _chart, const fude_kanji_db* _db);
void fude_chart_destroy(fude_chart* _chart);

void fude_chart_open(fude_chart* _chart);
void fude_chart_close(fude_chart* _chart);
// Scrolls to a section's title.
void fude_chart_jump(fude_chart* _chart, FUDE_CHART_SECTION_ _section);

void fude_chart_pointer_down(fude_chart* _chart, rde_vec_2F _screen, f64 _time);
void fude_chart_pointer_moved(fude_chart* _chart, rde_vec_2F _screen, f64 _time);
void fude_chart_pointer_up(fude_chart* _chart, f64 _time);

// A letter was tapped: its position in the list. Once per tap.
b8         fude_chart_take_tap(fude_chart* _chart, u32* _position);
const u32* fude_chart_list(const fude_chart* _chart);
u32        fude_chart_count(const fude_chart* _chart);

// The section mostly in view, and where its letters are in the list (the list
// is in chart order).
FUDE_CHART_SECTION_ fude_chart_section_in_view(const fude_chart* _chart);
void       fude_chart_section_range(const fude_chart* _chart, FUDE_CHART_SECTION_ _section, u32* _first, u32* _count);


void fude_chart_update(fude_chart* _chart, f32 _dt);

// Draws the chart between _top and _bottom (screen y). Inside a 2D drawing
// block; clips to that band.
void fude_chart_render(fude_chart* _chart, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom);

// The screen (screen.h): its rows of buttons, and what they do.
extern const struct fude_screen FUDE_CHART_SCREEN;

#endif
