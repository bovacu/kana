#ifndef KANA_ALBUM
#define KANA_ALBUM

#include "rde.h"
#include "kanji.h"
#include "glyph.h"
#include "scroll.h"
#include "history.h"
#include "examlog.h"

// ===========================================================================
// The album: progress, from the practice history (history.h).
//
// The OVERVIEW is every character practised, as a grid: its last session's
// average, coloured by grade, and how many sessions — sorted weakest first, most
// recent first, or most practised first. Or, the EXAMS view, every exam taken
// (examlog.h), newest first: what it was of, when, how it went, and each
// character asked, green when right; a tap opens its results again (exam.h).
//
// A tap on a character opens its PAGE: the character, a summary and a trend line
// of every session's average, then the sessions and its exam answers, newest
// first — the date, the average (or right or wrong), and every attempt as it was
// drawn, over the model, faint. A tap on an attempt replays it stroke by stroke,
// as it was written (an exam's at a steady pace: its timing is not kept), and
// says what its score said.
//
// Both re-read the history whenever a session was saved (Practice, opened from a
// page, sits on top of it), and the exams whenever one was taken, so coming back
// shows the new one.
// ===========================================================================

typedef enum {
    KANA_ALBUM_VIEW_CHARACTERS = 0,
    KANA_ALBUM_VIEW_EXAMS
} KANA_ALBUM_VIEW_;

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
    u32 index;       // overview: an entry, or an exam (examlog.h); page: a square of the history, or KANA_ALBUM_EXAM | an exam answer
} kana_album_hit;

#define KANA_ALBUM_EXAM 0x80000000u

RDE_STRUCT {
    const kana_kanji_db*            db;
    kana_glyph                      glyph;
    b8                              open;
    KANA_ALBUM_VIEW_                view;
    KANA_ALBUM_SORT_                sort;
    i32                             tapped_exam;    // the exams view: an exam tapped, to open (-1: none)

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
    i32                             selected_answer;// ...or the exam answer (page_exams), or -1
    f64                             replay_start;
    // ...its exam answers, oldest first, and their writing.
    rde_arr TYPE(kana_examlog_writing) page_exams;
    rde_arr TYPE(kana_history_stroke)  page_exam_strokes;
    rde_arr TYPE(kana_history_point)   page_exam_points;
    u32                             exams_revision;

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
// Sorting shows the characters; the exams view, the exams.
void kana_album_set_sort(kana_album* _album, KANA_ALBUM_SORT_ _sort);
void kana_album_set_view(kana_album* _album, KANA_ALBUM_VIEW_ _view);
// An exam tapped in the exams view: its index (examlog.h), once per tap.
b8   kana_album_take_exam(kana_album* _album, u32* _exam);

// The weakest characters (lowest last average first), as records: up to _max
// into _out; how many.
u32  kana_album_weakest(const kana_album* _album, u32* _out, u32 _max);

// A character's page; back to the overview.
void kana_album_open_page(kana_album* _album, u32 _codepoint);
void kana_album_close_page(kana_album* _album);

// One pointer (pen, finger or mouse), screen space: drags scroll, taps open a
// character or an exam (overview) or replay an attempt (page).
void kana_album_pointer_down(kana_album* _album, rde_vec_2F _screen, f64 _time);
void kana_album_pointer_moved(kana_album* _album, rde_vec_2F _screen, f64 _time);
void kana_album_pointer_up(kana_album* _album, f64 _time);

// Once a frame while it is the top screen: scrolling, and a re-read if a session
// was saved since.
void kana_album_update(kana_album* _album, f32 _dt);

// Draws the album between _top and _bottom (screen y). Inside a 2D drawing block.
void kana_album_render(kana_album* _album, rde_window* _window, rde_font* _font, f32 _font_px, f32 _top, f32 _bottom);

#endif
