// Copyright (c) 2026 Borja Vazquez Cuesta. All rights reserved.

#ifndef FUDE_CHART
#define FUDE_CHART

#include "rde.h"
#include "study/chars/kanji.h"
#include "study/chars/glyph.h"
#include "drawing/widgets/scroll.h"
#include "study/models/select.h"
#include "lang/lang.h"

// ===========================================================================
// Chart: a language's letters as a table, the side panel's "Kana", "Hangul" or "Thai letters" screen. What the table
// holds (its sections, its rows, how a narrow screen fits them) belongs to the language (lang.h: fude_lang_chart);
// this module draws any of them, the same way for every language.
//
// It scrolls like Browse. A tap opens the viewer on that letter, walking the whole chart in its order; in Select
// mode the tap ticks the letter instead. Its row of buttons jumps to each section, then has Select, Practice (the
// section in view) and Close.
//
// LAYOUT. Cells are square, at most FUDE_CHART_CELL_MAX across. A row's letters sit in its columns: an empty cell,
// or a letter the character data lacks, keeps its place when the language asks for that (keep_gaps), and closes up
// otherwise. When the screen is too narrow for the widest row at the language's smallest cell (cell_min), rows wrap
// onto more lines (or, with flow, a section's letters run on line after line); a language with no smallest cell has
// its cells shrink to fit instead. Each cell's place in the list (the viewer's walk) is fixed when the chart is made:
// row by row, or down the columns of a block marked so (Japanese reads its gojūon a i u e o down each column).
//
// Where it sits: a study screen (study/screens). The study shell (study/app/shell.c) owns the one chart, places
// FUDE_CHART_SCREEN in the app's screen table, and gives it the app's selection.
// Its main types and their lifecycle: fude_chart, made with fude_chart_init once the character data is loaded (it
// lists what the data has) and freed with fude_chart_destroy at the end. With NULL data, or a language with no
// chart, the chart is empty and never opens.
// Threads: main thread only.
// Files: none.
// ===========================================================================

#define FUDE_CHART_CELL_MAX 88.0f   // The largest a cell can be, in screen points.

// A cell, as laid out.
RDE_STRUCT {
    f32 x;          // The screen x of the cell's left edge.
    f32 y;          // The content y of its top edge (0 is the top of the chart, growing down).
    f32 size;
    u32 position;   // Its place in the chart's list.
} fude_chart_cell;

// A block's subtitle, as laid out: its text and where it is.
RDE_STRUCT {
    u32 text;       // A FUDE_TEXT_ id.
    f32 y;          // The content y of its top.
} fude_chart_subtitle;

RDE_STRUCT {
    const fude_kanji_db*              db;
    const fude_lang_chart_def*            def;        // The language's table (NULL when it has none).
    fude_glyph                        glyph;
    fude_scroller                     scroller;
    b8                                open;

    rde_arr TYPE(u32)                 list;       // Record indices, in the chart's order (the sections in turn).
    rde_arr TYPE(u32)                 codepoints; // The same, as code points (for the romanisation).
    // Each table cell's place in the list, at row * FUDE_LANG_CHART_COLUMNS + column (UINT32_MAX: not listed).
    rde_arr TYPE(u32)                 position_of;// Indexed by row and column (see above).
    rde_arr TYPE(fude_chart_cell)     cells;      // Laid out for the last frame's width and direction.
    rde_arr TYPE(fude_chart_subtitle) subtitles;  // Laid out with them.
    f32                               _laid_out_width;
    b8                                _laid_out_rtl;
    f32                               content_height;
    f32                               section_at[FUDE_LANG_CHART_SECTIONS];         // Each section title's content y.
    u32                               section_first[FUDE_LANG_CHART_SECTIONS + 1u]; // Each section's start in list.
    f32                               view_top;      // The screen y of the chart's visible top and bottom.
    f32                               view_bottom;

    i32                               tapped;     // See fude_chart_take_tap.

    fude_selection*                   selection;  // The app's (select.h), set by its owner; Select mode shows ticks.
} fude_chart;

// Makes the chart over the character data (NULL: none) for the language's chart (fude_lang_chart()); destroy frees
// it.
void fude_chart_init(fude_chart* _chart, const fude_kanji_db* _db);
void fude_chart_destroy(fude_chart* _chart);

// Opens it, when it has any letter; closes it.
void fude_chart_open(fude_chart* _chart);
void fude_chart_close(fude_chart* _chart);
// Scrolls to section _section's title.
void fude_chart_jump(fude_chart* _chart, u32 _section);

// One pointer, in screen space: a drag scrolls, a tap on a cell is kept for fude_chart_take_tap.
void fude_chart_pointer_down(fude_chart* _chart, rde_vec_2F _screen, f64 _time);
void fude_chart_pointer_moved(fude_chart* _chart, rde_vec_2F _screen, f64 _time);
void fude_chart_pointer_up(fude_chart* _chart, f64 _time);

// Whether a letter was tapped; if so its position in the list goes into *_position. Each tap is handed out once.
b8         fude_chart_take_tap(fude_chart* _chart, u32* _position);
// The list (record indices, in the chart's order) and how many: what the viewer walks. The pointer is the chart's
// and stays valid until it is destroyed.
const u32* fude_chart_list(const fude_chart* _chart);
u32        fude_chart_count(const fude_chart* _chart);

// The section mostly in view (the last whose title is in the top third of the view), and where a section's letters
// are in the list: from *_first, *_count of them.
u32        fude_chart_section_in_view(const fude_chart* _chart);
void       fude_chart_section_range(const fude_chart* _chart, u32 _section, u32* _first, u32* _count);

// Once a frame while open: scrolls.
void fude_chart_update(fude_chart* _chart, f32 _dt);

// Draws the chart between _top and _bottom (screen y), clipped to that band. Call it inside a 2D drawing block.
void fude_chart_render(fude_chart* _chart, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom);

// The chart as a screen (screen.h). Its row: a button to jump to each section, then Select, Practice (the section in
// view, as a practice set) and Close; in Select mode, Select's row (study.h). The buttons are made from the
// language's sections when the first chart is made, since every chart of an app is of its one language. What is in
// view is the section in view.
extern const struct fude_screen FUDE_CHART_SCREEN;

#endif
