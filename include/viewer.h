#ifndef KANA_VIEWER
#define KANA_VIEWER

#include "rde.h"
#include "kanji.h"
#include "glyph.h"

// ===========================================================================
// The character viewer — milestone 2's thing to hold in the hand: one character
// at a time, writing itself stroke by stroke from the baked data (kanji.h).
//
// Everything is drawn from strokes (glyph.h): the big character, and the kana
// of its readings. Latin text is the UI font (Slug).
//
// It walks a LIST it is given — Browse's results — so Prev/Next stay inside
// whatever was filtered, sorted or searched. The buttons are the toolbar's.
// ===========================================================================

RDE_STRUCT {
    const kana_kanji_db* db;
    kana_glyph           glyph;
    b8                   open;
    rde_arr TYPE(u32)    list;        // record indices being walked
    u32                  position;    // into list
    f64                  started;     // when the current character began writing (engine clock)
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

// Draws the whole viewer. Inside a 2D drawing block; screen space is centre
// origin, Y up. _font is drawn at sizes given in screen units, scaled from the
// size it was loaded at, _font_px. _insets: the safe area (left, top, right,
// bottom); _bottom_bar the height kept free for the buttons.
void kana_viewer_render(kana_viewer* _viewer, rde_font* _font, f32 _font_px, rde_vec_2I _window, rde_vec_4I _insets, f32 _bottom_bar);

#endif
