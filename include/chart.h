#ifndef KANA_CHART
#define KANA_CHART

#include "rde.h"
#include "kanji.h"
#include "glyph.h"
#include "scroll.h"

// ===========================================================================
// The kana chart: all of hiragana, then all of katakana, as the classic gojūon
// table — columns あ か さ た な は ま や ら わ ん, each reading down a i u e o —
// followed by the voiced columns (が ざ だ ば ぱ, the ゛ and ゜ marks) and the
// small kana. Every cell is drawn from its strokes, with its romaji under it.
//
// It scrolls like Browse; a tap opens the viewer on that kana, walking the whole
// chart in table order.
// ===========================================================================

typedef enum {
    KANA_CHART_HIRAGANA = 0,
    KANA_CHART_KATAKANA
} KANA_CHART_SECTION_;

RDE_STRUCT {
    f32 x;          // screen x of the cell's left edge
    f32 y;          // content y of its top edge (0 = top of the chart, growing down)
    f32 size;
    u32 position;   // in the chart's list
} kana_chart_cell;

RDE_STRUCT {
    const kana_kanji_db*          db;
    kana_glyph                    glyph;
    kana_scroller                 scroller;
    b8                            open;

    rde_arr TYPE(u32)             list;      // record indices, table order, hiragana then katakana
    rde_arr TYPE(u32)             codepoints;// the same, as code points (for the romaji)
    rde_arr TYPE(const c8*)       romaji;
    rde_arr TYPE(kana_chart_cell) cells;     // laid out for the last frame's width
    f32                           _laid_out_width;
    f32                           content_height;
    f32                           katakana_at;   // content y of the katakana section
    f32                           view_top;      // screen y of the chart's visible top and bottom
    f32                           view_bottom;

    i32                           tapped;    // see kana_chart_take_tap
} kana_chart;

void kana_chart_init(kana_chart* _chart, const kana_kanji_db* _db);
void kana_chart_destroy(kana_chart* _chart);

void kana_chart_open(kana_chart* _chart);
void kana_chart_close(kana_chart* _chart);
// Scrolls to a section's title.
void kana_chart_jump(kana_chart* _chart, KANA_CHART_SECTION_ _section);

void kana_chart_pointer_down(kana_chart* _chart, rde_vec_2F _screen, f64 _time);
void kana_chart_pointer_moved(kana_chart* _chart, rde_vec_2F _screen, f64 _time);
void kana_chart_pointer_up(kana_chart* _chart, f64 _time);

// A kana was tapped: its position in the list. Once per tap.
b8         kana_chart_take_tap(kana_chart* _chart, u32* _position);
const u32* kana_chart_list(const kana_chart* _chart);
u32        kana_chart_count(const kana_chart* _chart);

// The section mostly in view, and where its kana are in the list (the list is
// in chart order: all of hiragana, then all of katakana).
KANA_CHART_SECTION_ kana_chart_section_in_view(const kana_chart* _chart);
void       kana_chart_section_range(const kana_chart* _chart, KANA_CHART_SECTION_ _section, u32* _first, u32* _count);

// A kana's romaji as the chart labels it ("ka", "(tsu)" for small っ), hiragana
// or katakana; a short name for the marks it leaves out (ー "long", 々 "repeat");
// NULL for anything else.
const c8*  kana_chart_romaji(u32 _codepoint);

void kana_chart_update(kana_chart* _chart, f32 _dt);

// Draws the chart between _top and _bottom (screen y). Inside a 2D drawing
// block; clips to that band.
void kana_chart_render(kana_chart* _chart, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom);

#endif
