#ifndef KANA_VIEWER
#define KANA_VIEWER

#include "rde.h"
#include "kanji.h"
#include "glyph.h"
#include "scroll.h"

// ===========================================================================
// The character viewer — milestone 2's thing to hold in the hand: one character
// at a time, writing itself stroke by stroke from the baked data (kanji.h).
//
// The big character and the kana of its readings are drawn from strokes
// (glyph.h). Other text is the UI font (Slug, with Noto Sans JP for Japanese):
// the example words too — words from JMdict (kanji.h), each a row with its
// reading and meaning; tapping one practises its kanji as a set.
//
// It walks a LIST it is given — Browse's results — so Prev/Next stay inside
// whatever was filtered, sorted or searched. The buttons are the toolbar's.
//
// Portrait stacks the character over its details; a wide screen puts the
// details beside it, so the character stays big.
// ===========================================================================

#define KANA_VIEWER_WORDS       6u    // example words shown at most
#define KANA_VIEWER_WORD_KANJI  4u    // kanji a word can bring to practice

// An example word as last drawn: where (screen units), and which.
RDE_STRUCT {
    rde_vec_2F min;
    rde_vec_2F max;
    u32        word;
} kana_viewer_row;

RDE_STRUCT {
    const kana_kanji_db* db;
    kana_glyph           glyph;
    b8                   open;
    rde_arr TYPE(u32)    list;        // record indices being walked
    u32                  position;    // into list
    f64                  started;     // when the current character began writing (engine clock)

    kana_scroller        taps;        // a press, to tell a tap on a word (nothing scrolls)
    kana_viewer_row      rows[KANA_VIEWER_WORDS];
    u32                  row_count;
    i32                  pressed;     // the row under a press still a tap (-1: none)
    f32                  _latin_em;     // the UI font's average Latin advance, per unit of size (0: not measured)
    f32                  _japanese_em;  // and a Japanese character's
} kana_viewer;

void kana_viewer_init(kana_viewer* _viewer, const kana_kanji_db* _db);
void kana_viewer_destroy(kana_viewer* _viewer);

// There is character data to show.
b8   kana_viewer_available(const kana_viewer* _viewer);

// Opens on _records[_position], walking _records (copied).
void kana_viewer_show(kana_viewer* _viewer, const u32* _records, u32 _count, u32 _position);
// Opens on one character by code point, alone. False when the data has none.
b8   kana_viewer_show_codepoint(kana_viewer* _viewer, u32 _codepoint);
void kana_viewer_close(kana_viewer* _viewer);
void kana_viewer_next(kana_viewer* _viewer);
void kana_viewer_prev(kana_viewer* _viewer);
void kana_viewer_replay(kana_viewer* _viewer);

// A pointer on the viewer (screen space, centre origin, Y up): a tap on an
// example word.
void kana_viewer_pointer_down(kana_viewer* _viewer, rde_vec_2F _screen, f64 _time);
void kana_viewer_pointer_moved(kana_viewer* _viewer, rde_vec_2F _screen, f64 _time);
void kana_viewer_pointer_up(kana_viewer* _viewer, f64 _time);
// A word was tapped: its kanji as records, each once, in the order written (at
// most _max); how many — 0 when no word was tapped. Once per tap.
u32  kana_viewer_take_word(kana_viewer* _viewer, u32* _out, u32 _max);

// Draws the whole viewer. Inside a 2D drawing block; screen space is centre
// origin, Y up. _font is drawn at sizes given in screen units, scaled from the
// size it was loaded at, _font_px. _insets: the safe area (left, top, right,
// bottom); _bottom_bar the height kept free for the buttons.
void kana_viewer_render(kana_viewer* _viewer, rde_font* _font, f32 _font_px, rde_vec_2I _window, rde_vec_4I _insets, f32 _bottom_bar);

#endif
