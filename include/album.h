#ifndef KANA_ALBUM
#define KANA_ALBUM

#include "rde.h"
#include "kanji.h"
#include "glyph.h"
#include "scroll.h"
#include "history.h"

// ===========================================================================
// The album: progress, from the practice history (history.h).
//
// The OVERVIEW is every character practised, as a grid: its last session's
// average, coloured by grade, and how many sessions — sorted weakest first, most
// recent first, or most practised first.
//
// A tap opens that character's PAGE: the character, a summary and a trend line
// of every session's average, then the sessions, newest first — the date, the
// average, and every attempt as it was drawn, over the model, faint. A tap on an
// attempt replays it stroke by stroke, as it was written, and says what its
// score said.
//
// Both re-read the history whenever a session was saved (Practice, opened from a
// page, sits on top of it), so coming back shows the new one.
// ===========================================================================

typedef enum {
    KANA_ALBUM_SORT_WEAKEST = 0,   // lowest last average first
    KANA_ALBUM_SORT_RECENT,        // most recently practised first
    KANA_ALBUM_SORT_MOST,          // most sessions first
    KANA_ALBUM_SORT_COUNT
} KANA_ALBUM_SORT_;

RDE_STRUCT {
    u32                  codepoint;
    u32                  record;     // in the character data
    kana_history_summary summary;
} kana_album_entry;

// Something tappable, as laid out for the last frame (content space: y down
// from the top of the scrolled content).
RDE_STRUCT {
    f32 x, y, w, h;
    u32 index;       // overview: an entry; page: a square of the history
} kana_album_hit;

RDE_STRUCT {
    const kana_kanji_db*            db;
    kana_glyph                      glyph;
    b8                              open;
    KANA_ALBUM_SORT_                sort;

    // The overview.
    rde_arr TYPE(kana_album_entry)  entries;
    u32                             sessions;       // over all entries
    u32                             loaded_revision;
    b8                              loaded;
    kana_scroller                   scroller;

    // A character's page (page_open).
    b8                              page_open;
    u32                             page_codepoint;
    u32                             page_record;
    kana_history                    history;
    kana_scroller                   page_scroller;
    i32                             selected;       // the square being replayed (history.squares), or -1
    f64                             replay_start;

    // Layout of the last frame, for taps.
    rde_arr TYPE(kana_album_hit)    hits;
    f32                             view_top;
    f32                             view_bottom;
    f32                             content_height;

    // Scratch for drawing strokes.
    rde_arr TYPE(rde_vec_2F)        _points;
} kana_album;

void kana_album_init(kana_album* _album, const kana_kanji_db* _db);
void kana_album_destroy(kana_album* _album);

// Opens on the overview (re-reading the history).
void kana_album_open(kana_album* _album);
void kana_album_close(kana_album* _album);
void kana_album_set_sort(kana_album* _album, KANA_ALBUM_SORT_ _sort);

// The weakest characters (lowest last average first), as records: up to _max
// into _out; how many.
u32  kana_album_weakest(const kana_album* _album, u32* _out, u32 _max);

// A character's page; back to the overview.
void kana_album_open_page(kana_album* _album, u32 _codepoint);
void kana_album_close_page(kana_album* _album);

// One pointer (pen, finger or mouse), screen space: drags scroll, taps open a
// character (overview) or replay an attempt (page).
void kana_album_pointer_down(kana_album* _album, rde_vec_2F _screen, f64 _time);
void kana_album_pointer_moved(kana_album* _album, rde_vec_2F _screen, f64 _time);
void kana_album_pointer_up(kana_album* _album, f64 _time);

// Once a frame while it is the top screen: scrolling, and a re-read if a session
// was saved since.
void kana_album_update(kana_album* _album, f32 _dt);

// Draws the album between _top and _bottom (screen y). Inside a 2D drawing block.
void kana_album_render(kana_album* _album, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom);

#endif
